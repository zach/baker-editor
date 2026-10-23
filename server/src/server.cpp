#include "server.hpp"

#include "features.hpp"
#include "highlight.hpp"
#include "hints.hpp"
#include "merged.hpp"
#include "outline.hpp"
#include "positions.hpp"
#include "reference.hpp"
#include "scan.hpp"
#include "scenarios.hpp"
#include "structure.hpp"

#include <baker/project.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <utility>

namespace baker_lsp {

using nlohmann::json;

namespace {

constexpr int kLogError = 1, kLogWarning = 2, kLogInfo = 3;

json to_json(const Range& r) {
    return {{"start", {{"line", r.start.line}, {"character", r.start.character}}},
            {"end", {{"line", r.end.line}, {"character", r.end.character}}}};
}

std::string parent_of(const std::string& path) {
    return std::filesystem::path(path).parent_path().generic_string();
}

}  // namespace

Server::Server(Setup setup, Send send) : given_(std::move(setup)), send_(std::move(send)) {
    fixed_ = !given_.project.empty() || given_.by_hand();
    if (fixed_) add(given_);
}

std::vector<std::string> Server::projects() const {
    std::vector<std::string> out;
    for (const auto& [file, checked] : projects_) out.push_back(file);
    return out;
}

void Server::add(Setup setup) {
    const std::string key = setup.project;
    if (projects_.count(key)) return;
    Checked& c = projects_[key];
    const std::string name = key.empty() ? std::string("the command line's setup") : key;
    log(kLogInfo, "checking " + name);
    uint16_t project_port = 0;  // the project file's (console PORT), if it names one
    // --reference wins; else the project file's (reference ...).
    if (!key.empty()) {
        const baker::LoadSession::Reader reader = [this](std::string_view path,
                                                         baker::TextSink sink) {
            std::string text;
            if (!buffers_.read(canonical_path(std::string(path)), text)) return false;
            sink(text);
            return true;
        };
        if (auto p = baker::read_project(key, reader)) {
            if (setup.reference.empty() && !p->reference.str().empty())
                setup.reference = canonical_path(std::string(p->reference.str()));
            project_port = p->console_port;
        }
    }
    if (setup.reference.empty()) {
        log(kLogWarning,
            name +
                ": no reference (--reference, or (reference ...) in the project file): calls "
                "are not checked against the game's functions");
    } else {
        Reference r;
        std::string error;
        if (!load_reference(*c.ctx, setup.reference, r, error)) {
            log(kLogWarning, error + "; checking without the game's functions");
        } else {
            c.have_reference = true;
            setup.objects = r.objects;
            setup.types = r.declared_types;
            c.reference = r;
            c.tweaks = std::make_unique<GameTweaks>(c.reference.tweaks);
            for (const std::string& t : c.tweaks->left_out())
                log(kLogInfo, "tweak " + t + ": a type of the game's own, not checked by Baker");
            log(kLogInfo, "the game's reference: " + std::to_string(r.functions) + " functions, " +
                              std::to_string(r.overloads) + " overloads, " +
                              std::to_string(r.declared_types.size()) + " types, " +
                              std::to_string(r.declared_objects) + " named objects, from " +
                              setup.reference);
            if (r.widened > 0)
                log(kLogInfo, std::to_string(r.widened) +
                                  " functions take a type this server cannot declare; any "
                                  "value is accepted for it");
            for (const std::string& f : r.failed) log(kLogWarning, "not declared: " + f);
        }
    }
    // Its console: the nearest --console directory above the project, else the project file's
    // (console PORT), else the default.
    c.port = project_port ? project_port : given_.console_port;
    size_t nearest = 0;
    const std::string dir = key.empty() ? std::string() : parent_of(key);
    for (const auto& [under_dir, port] : given_.console_ports)
        if ((dir == under_dir || dir.starts_with(under_dir + "/")) && under_dir.size() >= nearest) {
            c.port = port;
            nearest = under_dir.size();
        }
    c.setup = std::move(setup);
}

Live& Server::live_of(const Checked& c) {
    std::unique_ptr<Live>& live = lives_[c.port];
    if (!live) live = std::make_unique<Live>(c.port);
    return *live;
}

Live& Server::live_at(const std::string& file) {
    if (const Checked* c = project_of(file)) return live_of(*c);
    std::unique_ptr<Live>& live = lives_[given_.console_port];
    if (!live) live = std::make_unique<Live>(given_.console_port);
    return *live;
}

std::optional<Server::Clock::time_point> Server::due() const {
    if (!given_.live_inlays || !initialized_ || !refresh_inlays_) return due_;
    if (!due_ || next_poll_ < *due_) return next_poll_;
    return due_;
}

void Server::tick(Clock::time_point now) {
    if (due_ && *due_ <= now) check_now();
    if (given_.live_inlays && initialized_ && refresh_inlays_ && next_poll_ <= now) poll(now);
}

void Server::poll(Clock::time_point now) {
    next_poll_ = now + kPoll;
    bool changed = false;
    std::set<uint16_t> asked;
    for (auto& [key, c] : projects_) {
        if (c.port == 0 || !asked.insert(c.port).second) continue;
        const auto stamp = live_of(c).stamp();
        if (!stamp) {
            stamps_.erase(c.port);  // no game: nothing live to refresh until one answers
            continue;
        }
        auto [it, added] = stamps_.try_emplace(c.port, *stamp);
        if (!added && it->second != *stamp) {
            it->second = *stamp;
            changed = true;
        } else if (added) {
            changed = true;  // a game came up
        }
    }
    if (changed) refresh_inlays();
}

void Server::find_project(const std::string& path) {
    if (fixed_ || path.empty()) return;
    const baker::LoadSession::Reader reader = [this](std::string_view candidate, baker::TextSink) {
        std::string text;
        return buffers_.read(canonical_path(std::string(candidate)), text);
    };
    const baker::Symbol found = baker::find_project(path, reader);
    if (found.str().empty()) return;
    Setup s = given_;
    s.project = canonical_path(std::string(found.str()));
    add(std::move(s));
}

void Server::handle(const json& message, Clock::time_point now) {
    if (!message.is_object() || !message.contains("method")) return;  // a response to us
    if (message.contains("id"))
        request(message);
    else
        notification(message, now);
}

namespace {

// A request for one of a file's views: it can wait behind what a person waits on, and is
// stale once its file changes.
bool is_view(const json& message) {
    static const std::set<std::string> kViews = {"textDocument/documentSymbol",
                                                 "textDocument/foldingRange",
                                                 "textDocument/semanticTokens/full",
                                                 "textDocument/semanticTokens/range",
                                                 "textDocument/documentColor",
                                                 "textDocument/inlayHint",
                                                 "textDocument/codeLens"};
    return message.contains("id") && kViews.count(message.value("method", ""));
}

std::string uri_of(const json& message) {  // without copying the params, an edit's text
    const auto params = message.find("params");
    if (params == message.end() || !params->is_object()) return {};
    const auto doc = params->find("textDocument");
    if (doc == params->end() || !doc->is_object()) return {};
    const auto uri = doc->find("uri");
    return uri != doc->end() && uri->is_string() ? uri->get<std::string>() : std::string();
}

}  // namespace

void Server::receive(json message) {
    if (message.is_object() && message.value("method", "") == "$/cancelRequest") {
        const json id = message.value("params", json::object()).value("id", json());
        for (auto it = queue_.begin(); it != queue_.end(); ++it)
            if (it->contains("method") && it->contains("id") && (*it)["id"] == id) {
                send_({{"jsonrpc", "2.0"},
                       {"id", id},
                       {"error", {{"code", -32800}, {"message", "cancelled"}}}});
                queue_.erase(it);
                break;
            }
        return;
    }
    queue_.push_back(std::move(message));
}

bool Server::work(Clock::time_point now) {
    if (queue_.empty()) return false;
    auto next =
        std::find_if(queue_.begin(), queue_.end(), [](const json& m) { return !is_view(m); });
    if (next == queue_.end()) {
        // Only views wait: a due check first, then the oldest view.
        if (due_ && *due_ <= now) {
            check_now();
            return true;
        }
        next = queue_.begin();
    } else if (const std::string method = next->value("method", "");
               method == "textDocument/didChange" || method == "textDocument/didClose") {
        // The views of this file asked before this edit are of a text that is going. (An
        // erase moves the deque's elements, so by index, and `next` found again after.)
        const std::string uri = uri_of(*next);
        for (size_t i = 0, end = static_cast<size_t>(next - queue_.begin()); i < end;) {
            if (is_view(queue_[i]) && uri_of(queue_[i]) == uri) {
                send_({{"jsonrpc", "2.0"},
                       {"id", queue_[i]["id"]},
                       {"error", {{"code", -32801}, {"message", "the text changed"}}}});
                queue_.erase(queue_.begin() + static_cast<std::ptrdiff_t>(i));
                --end;
            } else {
                ++i;
            }
        }
        next =
            std::find_if(queue_.begin(), queue_.end(), [](const json& m) { return !is_view(m); });
    }
    const json message = std::move(*next);
    queue_.erase(next);
    handle(message, now);
    return true;
}

void Server::request(const json& message) {
    const std::string method = message.value("method", "");
    json reply = {{"jsonrpc", "2.0"}, {"id", message["id"]}};
    if (method == "initialize") {
        const json params = message.value("params", json::object());
        if (const json folders = params.value("workspaceFolders", json()); folders.is_array())
            for (const json& f : folders) find_project(path_from_uri(f.value("uri", "")));
        refresh_inlays_ = params.value("capabilities", json::object())
                              .value("workspace", json::object())
                              .value("inlayHint", json::object())
                              .value("refreshSupport", false);
        if (params.contains("rootUri") && params["rootUri"].is_string())
            find_project(path_from_uri(params["rootUri"].get<std::string>()));
        reply["result"] = {
            {"capabilities",
             {{"textDocumentSync",
               {{"openClose", true},
                {"change", 2 /* incremental */},
                {"save", {{"includeText", false}}}}},
              {"hoverProvider", true},
              {"definitionProvider", true},
              {"documentSymbolProvider", true},
              {"foldingRangeProvider", true},
              {"referencesProvider", true},
              {"renameProvider", {{"prepareProvider", true}}},
              {"codeActionProvider", true},
              {"inlayHintProvider", true},
              {"codeLensProvider", {{"resolveProvider", false}}},
              {"colorProvider", true},
              {"semanticTokensProvider",
               {{"legend",
                 {{"tokenTypes",
                   json(std::vector<std::string>(std::begin(kTokenTypes), std::end(kTokenTypes)))},
                  {"tokenModifiers", json(std::vector<std::string>(std::begin(kTokenModifiers),
                                                                   std::end(kTokenModifiers)))}}},
                {"full", true},
                {"range", true}}},
              {"executeCommandProvider",
               {{"commands",
                 json::array({"baker.writeBack", "baker.evaluate", "baker.try", "baker.revert"})}}},
              {"completionProvider", {{"triggerCharacters", json::array({"(", "{", " "})}}}}},
            {"serverInfo", {{"name", "baker-lsp"}, {"version", "0.1.0"}}}};
    } else if (method == "textDocument/hover") {
        reply["result"] = hover(message.value("params", json::object()));
    } else if (method == "textDocument/definition") {
        reply["result"] = definition(message.value("params", json::object()));
    } else if (method == "textDocument/documentSymbol") {
        reply["result"] = document_symbols(message.value("params", json::object()));
    } else if (method == "textDocument/foldingRange") {
        reply["result"] = folding_ranges(message.value("params", json::object()));
    } else if (method == "textDocument/references") {
        reply["result"] = references(message.value("params", json::object()));
    } else if (method == "textDocument/prepareRename") {
        reply["result"] = prepare_rename(message.value("params", json::object()));
    } else if (method == "textDocument/rename") {
        std::string error;
        json edit = rename(message.value("params", json::object()), error);
        if (error.empty())
            reply["result"] = std::move(edit);
        else
            reply["error"] = {{"code", -32602}, {"message", error}};
    } else if (method == "baker/merged" || method == "baker/layerEffect" ||
               method == "baker/layers") {
        std::string error;
        json result = view(method, message.value("params", json::object()), error);
        if (error.empty())
            reply["result"] = std::move(result);
        else
            reply["error"] = {{"code", -32602}, {"message", error}};
    } else if (method == "textDocument/documentColor") {
        reply["result"] = document_colors(message.value("params", json::object()));
    } else if (method == "textDocument/colorPresentation") {
        reply["result"] = color_presentations(message.value("params", json::object()));
    } else if (method == "baker/scenarios") {
        std::string error;
        json result = scenarios(message.value("params", json::object()), error);
        if (error.empty())
            reply["result"] = std::move(result);
        else
            reply["error"] = {{"code", -32602}, {"message", error}};
    } else if (method == "textDocument/semanticTokens/full" ||
               method == "textDocument/semanticTokens/range") {
        reply["result"] = semantic_tokens(message.value("params", json::object()),
                                          method == "textDocument/semanticTokens/range");
    } else if (method == "textDocument/codeLens") {
        reply["result"] = code_lenses(message.value("params", json::object()));
    } else if (method == "textDocument/inlayHint") {
        reply["result"] = inlay_hints(message.value("params", json::object()));
    } else if (method == "textDocument/codeAction") {
        reply["result"] = code_actions(message.value("params", json::object()));
    } else if (method == "workspace/executeCommand") {
        reply["result"] = execute(message.value("params", json::object()));
    } else if (method == "textDocument/completion") {
        reply["result"] = complete(message.value("params", json::object()));
    } else if (method == "shutdown") {
        shutdown_ = true;
        reply["result"] = nullptr;
    } else {
        reply["error"] = {{"code", -32601}, {"message", "not supported: " + method}};
    }
    send_(reply);
}

void Server::notification(const json& message, Clock::time_point now) {
    const std::string method = message.value("method", "");
    const json params = message.value("params", json::object());
    const auto document_path = [&params]() {
        return path_from_uri(params.value("textDocument", json::object()).value("uri", ""));
    };
    if (method == "initialized") {
        initialized_ = true;
        for (const auto& [type, text] : pending_log_) log(type, text);
        pending_log_.clear();
        if (projects_.empty())
            log(kLogWarning, std::string("no ") + std::string(baker::kProjectFileName) +
                                 " in the workspace yet: a file is checked once one is found at "
                                 "or above it");
        schedule(now);
    } else if (method == "exit") {
        exit_code_ = shutdown_ ? 0 : 1;
    } else if (method == "textDocument/didOpen") {
        const json doc = params.value("textDocument", json::object());
        const std::string path = canonical_path(document_path());
        uris_[path] = doc.value("uri", "");
        buffers_.set(path, doc.value("text", ""));
        find_project(parent_of(path));
        schedule(now);
    } else if (method == "textDocument/didChange") {
        const auto changes = params.find("contentChanges");
        if (changes == params.end() || !changes->is_array() || changes->empty()) return;
        // Incremental sync: each change replaces a range of the text as the ones before it
        // left it; one with no range holds the whole text.
        const std::string path = canonical_path(document_path());
        for (const json& change : *changes) {
            const auto text = change.find("text");
            if (text == change.end() || !text->is_string()) continue;
            const auto range = change.find("range");
            if (range == change.end()) {
                buffers_.set(path, text->get<std::string>());
                continue;
            }
            const std::string* now_text = buffers_.find(path);
            if (!now_text) continue;  // not open: no text to change
            const auto at = [&](const char* end) {
                const json p = range->value(end, json::object());
                return offset_of(*now_text, {p.value("line", 0), p.value("character", 0)});
            };
            const size_t begin = at("start"), end = std::max(begin, at("end"));
            buffers_.replace(path, begin, end - begin, text->get_ref<const std::string&>());
        }
        evaluated_.erase(path);  // its results sit at offsets of the old text
        // A scenario run's failures in it would sit on lines that moved: they go until the
        // next run.
        for (auto& [project, scenarios] : sim_)
            for (auto& [scenario, problems] : scenarios)
                std::erase_if(problems, [&path](const Problem& p) { return p.file == path; });
        schedule(now + kDebounce, true);
    } else if (method == "textDocument/didClose") {
        buffers_.erase(canonical_path(document_path()));
        reads_.erase(canonical_path(document_path()));
        evaluated_.erase(canonical_path(document_path()));
        schedule(now);
    } else if (method == "workspace/didChangeWatchedFiles") {
        // A project file or a reference rebuilt on disk: read that project again.
        std::set<std::string> again;
        for (const json& change : params.value("changes", json::array())) {
            const std::string path = canonical_path(path_from_uri(change.value("uri", "")));
            for (const auto& [key, c] : projects_)
                if (path == c.setup.reference || (!key.empty() && path == key)) again.insert(key);
        }
        for (const std::string& key : again) {
            Setup s = given_;
            s.project = key;
            projects_.erase(key);
            add(std::move(s));
        }
        schedule(now);
    } else if (method == "baker/simResults") {
        sim_results(params);
        schedule(now);  // publish them
    } else if (method == "textDocument/didSave") {
        schedule(now);
    }
}

void Server::schedule(Clock::time_point when, bool push_back) {
    // An edit pushes the check back; an open or save brings it forward.
    if (!due_ || push_back || when < *due_) due_ = when;
}

Server::Checked* Server::project_of(const std::string& path) {
    Checked* best = nullptr;
    size_t depth = 0;
    for (auto& [key, c] : projects_)
        if (under(path, c.setup) && (!best || key.size() > depth)) {
            best = &c;
            depth = key.size();
        }
    return best;
}

namespace {

json range_json(std::string_view text, size_t begin, size_t end) {
    const Position a = position_of(text, begin), b = position_of(text, end);
    return {{"start", {{"line", a.line}, {"character", a.character}}},
            {"end", {{"line", b.line}, {"character", b.character}}}};
}

// The document and the byte the request is at, or false.
bool place(const json& params, const Buffers& buffers, std::string& path, std::string& text,
           size_t& offset) {
    path = canonical_path(
        path_from_uri(params.value("textDocument", json::object()).value("uri", "")));
    if (!buffers.read(path, text)) return false;
    const json p = params.value("position", json::object());
    offset = offset_of(text, {p.value("line", 0), p.value("character", 0)});
    return true;
}

}  // namespace

Server::Checked* Server::loaded_for(const std::string& path) {
    Checked* c = project_of(path);
    if (c && !c->loaded) check_now();
    return c && c->loaded ? c : nullptr;
}

json Server::hover(const json& params) {
    std::string path, text;
    size_t offset = 0;
    if (!place(params, buffers_, path, text, offset)) return nullptr;
    Checked* c = loaded_for(path);
    if (!c) return nullptr;
    auto h = baker_lsp::hover(*read_of(path), offset, *c->loaded, c->reference);
    if (!h) return nullptr;
    if (!h->path.empty())
        if (const auto chain = live_chain(*c, h->path)) {
            h->markdown +=
                "\n\n**Live**, the game on 127.0.0.1:" + std::to_string(live_of(*c).port()) +
                ":\n\n| layer | value | where | |\n|---|---|---|---|";
            for (const LiveEntry& e : *chain) {
                std::string where = e.file.substr(e.file.rfind('/') + 1);
                if (e.line > 0) where += ":" + std::to_string(e.line);
                h->markdown += "\n| " + e.layer + " | `" + e.value + "` | " + where + " | " +
                               e.standing + " |";
            }
        }
    return {{"contents", {{"kind", "markdown"}, {"value", h->markdown}}},
            {"range", range_json(text, h->begin, h->end)}};
}

json Server::complete(const json& params) {
    std::string path, text;
    size_t offset = 0;
    if (!place(params, buffers_, path, text, offset)) return json::array();
    Checked* c = loaded_for(path);
    if (!c) return json::array();
    const Completions found = baker_lsp::complete(
        *read_of(path), offset, *c->loaded, c->reference, path,
        [this](const std::string& f, std::string& out) { return buffers_.read(f, out); });
    json items = json::array();
    const json range = range_json(text, found.begin, found.end);
    for (const Completion& item : found.items) {
        json entry = {
            {"label", item.label},
            {"kind", static_cast<int>(item.kind)},
            {"detail", item.detail},
            {"documentation", {{"kind", "markdown"}, {"value", item.documentation}}},
            {"textEdit",
             {{"range", range}, {"newText", item.snippet.empty() ? item.label : item.snippet}}}};
        if (!item.snippet.empty()) entry["insertTextFormat"] = 2;  // a snippet
        if (!item.sort.empty()) entry["sortText"] = item.sort;
        items.push_back(std::move(entry));
    }
    return {{"isIncomplete", false}, {"items", std::move(items)}};
}

json Server::definition(const json& params) {
    std::string path, text;
    size_t offset = 0;
    if (!place(params, buffers_, path, text, offset)) return nullptr;
    Checked* c = loaded_for(path);
    if (!c) return nullptr;
    const auto exists = [this](const std::string& f) {
        std::string ignored;
        return buffers_.read(f, ignored);
    };
    json out = json::array();
    for (const Location& l :
         baker_lsp::definition(*read_of(path), offset, path, *c->loaded, c->reference, exists)) {
        const int row = l.line > 0 ? l.line - 1 : 0;
        out.push_back({{"uri", uri_for(l.file)},
                       {"range",
                        {{"start", {{"line", row}, {"character", 0}}},
                         {"end", {{"line", row}, {"character", 0}}}}}});
    }
    return out;
}

std::optional<std::vector<LiveEntry>> Server::live_chain(const Checked& c,
                                                         const std::string& path) {
    auto chain = live_of(c).explain(path);
    if (!chain) return std::nullopt;
    // The game on the port may be another project's: live only if its files are this one's.
    for (const LiveEntry& e : *chain)
        if (!e.file.empty() && under(canonical_path(e.file), c.setup)) return chain;
    return std::nullopt;
}

namespace {

// The innermost closed {command} around `offset`: its first and one-past-last bytes.
std::optional<std::pair<size_t, size_t>> command_at(std::string_view text, size_t offset) {
    const Structure s = read_structure(text);
    std::optional<std::pair<size_t, size_t>> out;
    for (const Bracket& b : s.brackets) {
        if (b.begin > offset) break;
        if (b.open == '{' && offset < b.end && b.end <= text.size() && text[b.end - 1] == '}')
            out = {b.begin, b.end};  // later ones are inner
    }
    return out;
}

// A value written in the data: a tagged array of values alone, such as (health 1000).
struct Leaf {
    std::string path;   // its data path, as a Path's text
    std::string value;  // the text after its tag
    size_t end = 0;     // one past its closing bracket
};

// The innermost leaf around `offset`.
std::optional<Leaf> leaf_at(std::string_view text, size_t offset) {
    const Structure s = read_structure(text);
    int found = -1;
    for (size_t b = 0; b < s.brackets.size() && s.brackets[b].begin <= offset; ++b)
        if (offset < s.brackets[b].end) found = static_cast<int>(b);
    if (found < 0) return std::nullopt;
    const Bracket& b = s.brackets[found];
    if (b.open != '(' || b.has_child || b.tag < 0 || b.end > text.size() || text[b.end - 1] != ')')
        return std::nullopt;
    const std::vector<std::string> tags = s.data_path(found);
    if (tags.empty()) return std::nullopt;
    const size_t from = s.tokens[b.tag].end + (s.tokens[b.tag].quote ? 1 : 0);
    std::string_view value = text.substr(from, b.end - 1 - from);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);
    if (value.empty() || value.find_first_of(";\n") != std::string_view::npos)
        return std::nullopt;  // nothing to set, or a comment or several lines in the way
    return Leaf{path_text(tags), std::string(value), b.end};
}

json command_json(const std::string& title, const std::string& command, json arguments) {
    return {
        {"title", title},
        {"kind", "quickfix"},
        {"command", {{"title", title}, {"command", command}, {"arguments", std::move(arguments)}}}};
}

// The result's first line, shortened for an inlay.
std::string first_line(const std::string& text) {
    constexpr size_t kMax = 80;
    std::string line = text.substr(0, text.find('\n'));
    if (line.size() > kMax) {
        size_t cut = kMax;
        while (cut > 0 && (static_cast<unsigned char>(line[cut]) & 0xC0) == 0x80) --cut;
        line = line.substr(0, cut) + "\u2026";
    } else if (line.size() < text.size()) {
        line += " \u2026";
    }
    return line;
}

}  // namespace

bool Server::game_is(Checked& c) {
    Live& live = live_of(c);
    if (!live.reachable()) return false;
    if (c.game_generation == live.generation()) return c.game_ours;
    c.game_generation = live.generation();
    c.game_ours = false;
    // A value from the project's data: the game is the project's if the files it says the
    // value comes from are in the project's directory.
    if (!c.loaded) return false;
    int tries = 8;
    for (const std::string& file : c.loaded->files) {
        std::string text;
        if (file == c.setup.project || !buffers_.read(file, text)) continue;
        const Structure s = read_structure(text);
        for (size_t b = 0; b < s.brackets.size(); ++b) {
            if (s.brackets[b].open != '(' || s.brackets[b].has_child) continue;
            const std::vector<std::string> tags = s.data_path(static_cast<int>(b));
            if (tags.empty()) continue;
            const auto chain = live.explain(path_text(tags));
            if (!chain) return false;  // no game answers, or its packs differ: not this one
            for (const LiveEntry& e : *chain)
                if (!e.file.empty()) {
                    c.game_ours = under(canonical_path(e.file), c.setup);
                    return c.game_ours;
                }
            if (--tries == 0) return false;
            break;  // a value from no file: try the next file
        }
    }
    return false;
}

json Server::code_actions(const json& params) {
    json out = json::array();
    std::string path, text;
    size_t offset = 0;
    json at = params;
    at["position"] = params.value("range", json::object()).value("start", json::object());
    if (!place(at, buffers_, path, text, offset)) return out;
    Checked* c = project_of(path);
    if (!c || !game_is(*c)) return out;  // no game, or another project's: nothing to offer
    const json where = json::array({uri_for(path), at["position"]});

    if (command_at(text, offset))
        out.push_back(command_json("Evaluate in the game", "baker.evaluate", where));
    const auto leaf = leaf_at(text, offset);
    if (leaf) out.push_back(command_json("Try this value in the game", "baker.try", where));

    const std::string data = data_path_at(*read_of(path), offset);
    if (data.empty()) return out;
    const auto chain = live_chain(*c, data);
    if (!chain) return out;
    // A value tuned in the running game and not yet written: the game's own write-back
    // writes it to the line it came from, or the edit is dropped.
    bool live_edit = false;
    for (const LiveEntry& e : *chain)
        live_edit = live_edit || (e.layer == "scratch" && e.standing == "wins");
    if (!live_edit) return out;
    if (live_of(*c).can_write_back())
        out.push_back(command_json("Write the live value here", "baker.writeBack",
                                   json::array({data, uri_for(path)})));
    if (live_of(*c).can_clear())
        out.push_back(command_json("Revert the live edit", "baker.revert",
                                   json::array({data, uri_for(path)})));
    return out;
}

void Server::refresh_inlays() {
    if (refresh_inlays_ && initialized_)
        send_({{"jsonrpc", "2.0"},
               {"id", "refresh-" + std::to_string(next_request_++)},
               {"method", "workspace/inlayHint/refresh"}});
}

void Server::evaluate(const std::string& path, size_t at, const std::string& command) {
    Live& live = live_at(path);
    const auto reply = live.ask(command, kEvaluateTimeout);
    Evaluated result{at, {}, command};
    int type = kLogInfo;
    if (!reply) {
        result.label = "\u2717 no game answered";
        result.tooltip = "no game answers on 127.0.0.1:" + std::to_string(live.port());
        type = kLogError;
    } else if (reply->value("ok", false)) {
        std::string text = reply->value("text", "");
        // A string reads better as its content than as Baker text with its escapes.
        if (const json v = reply->value("value", json()); v.is_string() && text.starts_with('"'))
            text = v.get<std::string>();
        result.label = "\u21d2 " + first_line(text);
        result.tooltip = command + "\n\n" + text;
    } else {
        const json error = reply->value("error", json::object());
        const std::string message = error.value("message", "");
        result.label = "\u2717 " + first_line(message);
        result.tooltip = message;
        for (const json& f : error.value("stack", json::array()))
            result.tooltip += "\n  in " + f.value("function", std::string("?")) + " (" +
                              f.value("file", std::string()) + ":" +
                              std::to_string(f.value("line", 0)) + ")";
        type = kLogError;
    }
    // An editor without inlay hints still sees an error, and every result in the log.
    if (type == kLogError)
        send_({{"jsonrpc", "2.0"},
               {"method", "window/showMessage"},
               {"params", {{"type", type}, {"message", result.tooltip}}}});
    log(kLogInfo,
        command + " => " + (reply ? reply->value("text", result.tooltip) : result.tooltip));
    auto& list = evaluated_[path];
    std::erase_if(list, [at](const Evaluated& e) { return e.at == at; });
    list.push_back(std::move(result));
    refresh_inlays();
}

json Server::execute(const json& params) {
    const std::string command = params.value("command", "");
    const json args = params.value("arguments", json::array());
    if (command == "baker.evaluate" || command == "baker.try") {
        if (args.size() < 2 || !args[0].is_string() || !args[1].is_object()) return nullptr;
        std::string path, text;
        size_t offset = 0;
        if (!place({{"textDocument", {{"uri", args[0]}}}, {"position", args[1]}}, buffers_, path,
                   text, offset))
            return nullptr;
        if (command == "baker.evaluate") {
            const auto found = command_at(text, offset);
            if (!found) return nullptr;
            evaluate(path, found->second, text.substr(found->first, found->second - found->first));
        } else {
            const auto leaf = leaf_at(text, offset);
            if (!leaf) return nullptr;
            evaluate(path, leaf->end, "{config.set " + leaf->path + " " + leaf->value + "}");
        }
        return nullptr;
    }
    // The data path, and the document it was asked from, whose project's game is meant.
    const auto game = [&]() -> Live& {
        return live_at(args.size() > 1 && args[1].is_string()
                           ? canonical_path(path_from_uri(args[1].get<std::string>()))
                           : std::string());
    };
    if (command == "baker.revert") {
        if (args.empty() || !args[0].is_string()) return nullptr;
        const std::string path = args[0].get<std::string>();
        const auto reply = game().ask("{" + std::string(Live::kClear) + " " + path + "}");
        if (!reply || !reply->value("ok", false))
            send_({{"jsonrpc", "2.0"},
                   {"method", "window/showMessage"},
                   {"params",
                    {{"type", kLogError},
                     {"message",
                      "the game did not revert " + path + ": " +
                          (reply ? reply->value("error", json::object()).value("message", "")
                                 : "no game answers")}}}});
        refresh_inlays();
        return nullptr;
    }
    if (command != "baker.writeBack") return nullptr;
    if (args.empty() || !args[0].is_string()) return nullptr;
    const std::string path = args[0].get<std::string>();
    Live& live = game();
    const auto reply = live.ask("{config.write_back " + path + "}");
    std::string message;
    int type = kLogError;
    if (!reply) {
        message = "no game answers on 127.0.0.1:" + std::to_string(live.port());
    } else if (!reply->value("ok", false)) {
        message = "the game did not write " + path + ": " +
                  reply->value("error", json::object()).value("message", "");
    } else {
        message = "the game wrote " + path + ": " + reply->value("text", "");
        type = kLogInfo;
    }
    send_({{"jsonrpc", "2.0"},
           {"method", "window/showMessage"},
           {"params", {{"type", type}, {"message", message}}}});
    return reply ? reply->value("text", json()) : json();
}

namespace {

json lsp_range(const LineIndex& lines, size_t begin, size_t end) {
    const auto [l0, c0] = lines.position(begin);
    const auto [l1, c1] = lines.position(end);
    return {{"start", {{"line", l0}, {"character", c0}}},
            {"end", {{"line", l1}, {"character", c1}}}};
}

json symbol_json(const LineIndex& lines, const OutlineSymbol& s) {
    json children = json::array();
    for (const OutlineSymbol& c : s.children) children.push_back(symbol_json(lines, c));
    json out = {{"name", s.name},
                {"kind", static_cast<int>(s.kind)},
                {"range", lsp_range(lines, s.begin, s.end)},
                {"selectionRange", lsp_range(lines, s.name_begin, s.name_end)},
                {"children", std::move(children)}};
    if (!s.detail.empty()) out["detail"] = s.detail;
    return out;
}

std::string document_of(const json& params) {
    return canonical_path(
        path_from_uri(params.value("textDocument", json::object()).value("uri", "")));
}

}  // namespace

std::shared_ptr<const FileRead> Server::read_of(const std::string& path) {
    std::string text;
    if (!buffers_.read(path, text)) return nullptr;
    if (!buffers_.find(path)) return std::make_shared<const FileRead>(std::move(text));
    std::shared_ptr<const FileRead>& kept = reads_[path];
    if (!kept || kept->text != text) kept = std::make_shared<const FileRead>(std::move(text));
    return kept;
}

json Server::document_symbols(const json& params) {
    const auto read = read_of(document_of(params));
    if (!read) return json::array();
    const LineIndex& lines = read->lines;
    std::vector<OutlineSymbol> symbols = outline(read->structure, read->text, kMaxSymbols);
    if (const size_t cut = cap_outline(symbols, kMaxSymbols))
        log(kLogInfo, "outline: " + std::to_string(cut) +
                          " symbols left out of a large file; "
                          "leaf fields go first");
    json out = json::array();
    for (const OutlineSymbol& s : symbols) out.push_back(symbol_json(lines, s));
    return out;
}

json Server::folding_ranges(const json& params) {
    const auto read = read_of(document_of(params));
    if (!read) return json::array();
    const LineIndex& lines = read->lines;
    json out = json::array();
    for (const Fold& f : folds(read->structure, read->text)) {
        if (out.size() >= kMaxSymbols) break;
        const int a = lines.position(f.begin).first, b = lines.position(f.end).first;
        if (b <= a) continue;
        json range = {{"startLine", a}, {"endLine", b}};
        if (f.comment) range["kind"] = "comment";
        out.push_back(std::move(range));
    }
    return out;
}

namespace {

// Each file's text and line index, read once for many ranges.
class Ranges {
public:
    explicit Ranges(const Buffers& buffers) : buffers_(buffers) {}
    json operator()(const std::string& file, size_t begin, size_t end) {
        auto it = lines_.find(file);
        if (it == lines_.end()) {
            std::string& text = texts_[file];
            buffers_.read(file, text);
            it = lines_.emplace(file, LineIndex(text)).first;
        }
        return lsp_range(it->second, begin, end);
    }

private:
    const Buffers& buffers_;
    std::map<std::string, std::string> texts_;  // nodes stay put, so the indexes' views hold
    std::map<std::string, LineIndex> lines_;
};

}  // namespace

json Server::references(const json& params) {
    if (due_) check_now();
    std::string path, text;
    size_t offset = 0;
    if (!place(params, buffers_, path, text, offset)) return json::array();
    Checked* c = project_of(path);
    if (!c || !c->loaded) return json::array();
    const auto target = target_at(read_of(path)->structure, offset, *c->loaded, c->reference);
    if (!target) return json::array();
    std::vector<std::string> files = c->loaded->files;
    for (const auto& [open, ignored] : buffers_.all())
        if (under(open, c->setup) && std::find(files.begin(), files.end(), open) == files.end())
            files.push_back(open);
    const auto read = [this](const std::string& f, std::string& out) {
        return buffers_.read(f, out);
    };
    json out = json::array();
    Ranges ranges(buffers_);
    for (const Occurrence& o : occurrences(*target, files, *c->loaded, read))
        out.push_back({{"uri", uri_for(o.file)}, {"range", ranges(o.file, o.begin, o.end)}});
    return out;
}

json Server::prepare_rename(const json& params) {
    if (due_) check_now();
    std::string path, text;
    size_t offset = 0;
    if (!place(params, buffers_, path, text, offset)) return nullptr;
    Checked* c = project_of(path);
    if (!c || !c->loaded) return nullptr;
    const auto target = target_at(read_of(path)->structure, offset, *c->loaded, c->reference);
    if (!target || !target->renamable) return nullptr;
    return {{"range", lsp_range(LineIndex(text), target->begin, target->end)},
            {"placeholder", target->name}};
}

json Server::rename(const json& params, std::string& error) {
    if (due_) check_now();
    std::string path, text;
    size_t offset = 0;
    if (!place(params, buffers_, path, text, offset)) return nullptr;
    Checked* c = project_of(path);
    if (!c || !c->loaded) return nullptr;
    const auto target = target_at(read_of(path)->structure, offset, *c->loaded, c->reference);
    if (!target || !target->renamable) {
        error = "nothing here can be renamed: the game's own functions are its code's";
        return nullptr;
    }
    const std::string name = params.value("newName", "");
    if (!valid_name(name, *target)) {
        error = "'" + name + "' cannot be written as a name here";
        return nullptr;
    }
    std::vector<std::string> files = c->loaded->files;
    for (const auto& [open, ignored] : buffers_.all())
        if (under(open, c->setup) && std::find(files.begin(), files.end(), open) == files.end())
            files.push_back(open);
    const auto read = [this](const std::string& f, std::string& out) {
        return buffers_.read(f, out);
    };
    json changes = json::object();
    Ranges ranges(buffers_);
    const bool spaced = name.find_first_of(" \t") != std::string::npos;
    for (const Occurrence& o : occurrences(*target, files, *c->loaded, read)) {
        const std::string written = o.quoted || spaced ? "'" + name + "'" : name;
        changes[uri_for(o.file)].push_back(
            {{"range", ranges(o.file, o.begin, o.end)}, {"newText", written}});
    }
    return {{"changes", std::move(changes)}};
}

namespace {

// The start of each line of `text`, and one past its end.
std::vector<size_t> line_starts(std::string_view text) {
    std::vector<size_t> out{0};
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] == '\n') out.push_back(i + 1);
    out.push_back(text.size() + 1);
    return out;
}

// Where a hint for the array tagged `key` that opens on 1-based `line` goes: after the array,
// or at the line's end when no such array opens and closes there. Only the line is read.
std::optional<size_t> hint_place(std::string_view text, const std::vector<size_t>& starts, int line,
                                 std::string_view key) {
    if (line < 1 || static_cast<size_t>(line) + 1 >= starts.size()) return std::nullopt;
    const size_t begin = starts[line - 1];
    size_t end = std::min(starts[line] - 1, text.size());
    const std::string_view here = text.substr(begin, end - begin);
    const Structure s = read_structure(here);
    for (size_t b = 0; b < s.brackets.size(); ++b)
        if (s.brackets[b].open == '(' && s.tag(static_cast<int>(b)) == key &&
            s.brackets[b].end <= here.size() && here[s.brackets[b].end - 1] == ')')
            return begin + s.brackets[b].end;
    while (end > begin && (text[end - 1] == '\r' || text[end - 1] == ' ' || text[end - 1] == '\t'))
        --end;
    return end;
}

// Where the lines of a check's texts are now: a hint or a shadowed value found in a file as
// the check read it is shown on its line in the file now, or not at all if that line changed.
class Moved {
public:
    Moved(const Loaded& loaded, const Buffers& buffers) : loaded_(loaded), buffers_(buffers) {}
    std::optional<int> line(const std::string& file, int line) {
        const auto then = loaded_.texts.find(file);
        if (then == loaded_.texts.end()) return line;
        auto it = maps_.find(file);
        if (it == maps_.end()) {
            std::string now;
            buffers_.read(file, now);
            std::optional<LineMap> map;
            if (now != then->second) map.emplace(then->second, now);
            it = maps_.emplace(file, std::move(map)).first;
        }
        return it->second ? it->second->line(line) : line;
    }

private:
    const Loaded& loaded_;
    const Buffers& buffers_;
    std::map<std::string, std::optional<LineMap>> maps_;  // empty: the text is the same
};

json hint_json(const LineIndex& lines, size_t at, const std::string& label,
               const std::string& tooltip) {
    const auto [line, character] = lines.position(at);
    return {{"position", {{"line", line}, {"character", character}}},
            {"label", label},
            {"tooltip", tooltip},
            {"paddingLeft", true}};
}

}  // namespace

json Server::inlay_hints(const json& params) {
    const std::string path = document_of(params);
    loaded_for(path);
    std::string text;
    json out = json::array();
    if (!buffers_.read(path, text)) return out;
    const json range = params.value("range", json::object());
    const int first = range.value("start", json::object()).value("line", 0) + 1;
    const int last = range.value("end", json::object()).value("line", 0) + 1;
    const LineIndex lines(text);
    if (const auto e = evaluated_.find(path); e != evaluated_.end())
        for (const Evaluated& r : e->second)
            if (r.at <= text.size()) {
                const int line = lines.position(r.at).first + 1;
                if (line >= first && line <= last)
                    out.push_back(hint_json(lines, r.at, r.label, r.tooltip));
            }
    Checked* c = project_of(path);
    if (!c || !c->loaded) return out;
    const std::vector<size_t> starts = line_starts(text);

    Moved moved(*c->loaded, buffers_);
    for (const LayerHint& h : c->loaded->hints) {
        if (h.file != path) continue;
        const std::optional<int> line = moved.line(path, h.line);
        if (!line || *line < first || *line > last) continue;
        if (const auto at = hint_place(text, starts, *line, h.key))
            out.push_back(hint_json(lines, *at, h.label,
                                    h.shadowed ? "`" + h.path + "`: a higher layer overrides it"
                                               : "`" + h.path + "`: overrides a lower layer"));
    }
    Live& live = live_of(*c);
    if (!given_.live_inlays || live.port() == 0) return out;

    // The running game's value where a live edit differs from the file value it overrides.
    const auto read = read_of(path);  // the whole file: a value's path is its arrays'
    const Structure& s = read->structure;
    std::vector<int> leaves;
    std::vector<std::string> paths;
    for (size_t b = 0; b < s.brackets.size() && leaves.size() < kMaxLiveInlays; ++b) {
        const Bracket& br = s.brackets[b];
        if (br.open != '(' || br.has_child || br.tag < 0) continue;
        const int line = lines.position(br.begin).first + 1;
        if (line < first) continue;
        if (line > last) break;
        const std::vector<std::string> tags = s.data_path(static_cast<int>(b));
        if (tags.empty()) continue;
        leaves.push_back(static_cast<int>(b));
        paths.push_back(path_text(tags));
    }
    const auto chains = live.explain_many(paths);
    if (!chains) return out;
    for (size_t i = 0; i < chains->size(); ++i) {
        const std::vector<LiveEntry>& chain = (*chains)[i];
        if (chain.size() < 2 || chain.back().layer != "scratch") continue;
        const LiveEntry& edit = chain.back();
        const LiveEntry& filed = chain[chain.size() - 2];  // the files' winner, below it
        const Bracket& br = s.brackets[leaves[i]];
        if (filed.file.empty() || canonical_path(filed.file) != path ||
            filed.line != lines.position(br.begin).first + 1 || filed.value == edit.value)
            continue;
        out.push_back(hint_json(lines, br.end, "live " + edit.value,
                                "the running game's value, a live edit not yet written"));
    }
    return out;
}

namespace {

json declared_json(const Declared& d, const std::function<std::string(const std::string&)>& uri) {
    return {{"name", d.name}, {"uri", uri(d.file)},         {"line", d.line - 1},
            {"desc", d.desc}, {"objectives", d.objectives}, {"median", d.median}};
}

}  // namespace

json Server::scenarios(const json& params, std::string& error) {
    const std::string path = canonical_path(path_from_uri(params.value("uri", std::string())));
    Checked* c = project_of(path);
    if (!c || c->setup.project.empty()) {
        error = "no project file governs " + path;
        return nullptr;
    }
    Suite suite;
    if (!read_suite(c->setup.project, buffers_, suite, error)) return nullptr;
    const auto uri = [this](const std::string& f) { return uri_for(f); };
    json out = {{"project", uri_for(suite.project)},
                {"directory", parent_of(suite.project)},
                {"tool", suite.tool},
                {"scenarios", json::array()},
                {"objectives", json::array()}};
    for (const Declared& d : suite.scenarios) out["scenarios"].push_back(declared_json(d, uri));
    for (const Declared& d : suite.objectives) out["objectives"].push_back(declared_json(d, uri));
    return out;
}

void Server::sim_results(const json& params) {
    const std::string path = canonical_path(path_from_uri(params.value("uri", std::string())));
    Checked* c = project_of(path);
    const json results = params.value("results", json::object());
    if (!c || c->setup.project.empty() || results.value("schema", "").rfind("baker.sim/1", 0) != 0)
        return;
    Suite suite;
    std::string error;
    if (!read_suite(c->setup.project, buffers_, suite, error)) return;
    // Each scenario the results cover replaces what its last run found.
    std::map<std::string, json> by_scenario;
    for (const json& run : results.value("runs", json::array())) {
        json& r = by_scenario[run.value("scenario", std::string())];
        r["runs"].push_back(run);
    }
    for (const json& m : results.value("medians", json::array()))
        by_scenario[m.value("scenario", std::string())]["medians"].push_back(m);
    auto& found = sim_[c->setup.project];
    for (const auto& [scenario, part] : by_scenario) found[scenario] = sim_problems(part, suite);
}

json Server::view(const std::string& method, const json& params, std::string& error) {
    const std::string path = canonical_path(path_from_uri(params.value("uri", std::string())));
    Checked* c = project_of(path);
    if (due_ || (c && !c->loaded)) check_now();  // a view may be asked before any edit
    if (!c || !c->loaded || !c->loaded->config) {
        error = "no project is checked for " + path;
        return nullptr;
    }
    const baker::Config& cfg = *c->loaded->config;
    if (method == "baker/layers") {
        json names = json::array();
        for (const baker::Symbol l : cfg.layers())
            if (l.str() != "scratch") names.push_back(std::string(l.str()));
        return names;
    }
    std::optional<View> v;
    if (method == "baker/merged") {
        v = merged_view(cfg,
                        c->setup.project.empty() ? std::string() : parent_of(c->setup.project));
    } else {
        v = layer_view(cfg, params.value("layer", std::string()), error);
        if (!v) return nullptr;
    }
    json sources = json::array();
    for (const ViewSource& s : v->sources)
        sources.push_back({{"line", s.line}, {"uri", uri_for(s.file)}, {"fileLine", s.file_line}});
    return {{"text", std::move(v->text)}, {"sources", std::move(sources)}};
}

namespace {

// A color component as written back: at most three decimals, no trailing zeros.
std::string component(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.3f", std::clamp(v, 0.0, 1.0));
    std::string s = b;
    while (s.size() > 1 && s.back() == '0') s.pop_back();
    if (s.back() == '.') s.pop_back();
    return s;
}

}  // namespace

json Server::document_colors(const json& params) {
    json out = json::array();
    const std::string path = document_of(params);
    std::string text;
    Checked* c = loaded_for(path);
    if (!c || !buffers_.read(path, text)) return out;
    const auto read = read_of(path);
    const Structure& s = read->structure;
    const LineIndex& lines = read->lines;
    // Each array's own values, after its tag; -1 once it holds an array that is not metadata.
    std::vector<std::vector<int>> values(s.brackets.size());
    for (size_t t = 0; t < s.tokens.size(); ++t) {
        const Token& tok = s.tokens[t];
        if (tok.bracket >= 0 && tok.index > 0) values[tok.bracket].push_back(static_cast<int>(t));
    }
    std::vector<bool> plain(s.brackets.size(), true);  // holds no data arrays
    for (const Bracket& b : s.brackets)
        if (b.parent >= 0 && !(b.open == '(' && b.tag >= 0 && is_meta_tag(s.tokens[b.tag].text)))
            plain[b.parent] = false;
    for (size_t b = 0; b < s.brackets.size(); ++b) {
        const Bracket& br = s.brackets[b];
        const std::vector<int>& v = values[b];
        if (br.open != '(' || br.tag < 0 || !plain[b] || (v.size() != 3 && v.size() != 4)) continue;
        std::vector<double> rgba;
        for (const int t : v) {
            const Token& tok = s.tokens[t];
            if (tok.quote) break;
            char* end = nullptr;
            const std::string n(tok.text);
            const double d = std::strtod(n.c_str(), &end);
            if (!end || *end) break;
            rgba.push_back(d);
        }
        if (rgba.size() != v.size()) continue;
        const std::vector<std::string> tags = s.data_path(static_cast<int>(b));
        if (tags.empty()) continue;
        const Hints h = hints_at(*c->loaded, c->reference, tags);
        if (!is_color(*c->loaded, h, tags)) continue;
        const auto [l0, c0] = lines.position(s.tokens[v.front()].begin);
        const auto [l1, c1] = lines.position(s.tokens[v.back()].end);
        out.push_back({{"range",
                        {{"start", {{"line", l0}, {"character", c0}}},
                         {"end", {{"line", l1}, {"character", c1}}}}},
                       {"color",
                        {{"red", std::clamp(rgba[0], 0.0, 1.0)},  // components from 0 to 1
                         {"green", std::clamp(rgba[1], 0.0, 1.0)},
                         {"blue", std::clamp(rgba[2], 0.0, 1.0)},
                         {"alpha", rgba.size() == 4 ? std::clamp(rgba[3], 0.0, 1.0) : 1.0}}}});
    }
    return out;
}

json Server::color_presentations(const json& params) {
    const std::string path = document_of(params);
    std::string text;
    if (!buffers_.read(path, text)) return json::array();
    const json range = params.value("range", json::object());
    const json start = range.value("start", json::object()),
               end = range.value("end", json::object());
    const size_t a = offset_of(text, {start.value("line", 0), start.value("character", 0)});
    const size_t b = offset_of(text, {end.value("line", 0), end.value("character", 0)});
    if (b < a) return json::array();
    // As many components as the value has now: three, or four with alpha.
    const Structure s = read_structure(std::string_view(text).substr(a, b - a));
    const json color = params.value("color", json::object());
    std::string label = component(color.value("red", 0.0)) + " " +
                        component(color.value("green", 0.0)) + " " +
                        component(color.value("blue", 0.0));
    if (s.tokens.size() == 4) label += " " + component(color.value("alpha", 1.0));
    return json::array({{{"label", label}, {"textEdit", {{"range", range}, {"newText", label}}}}});
}

json Server::semantic_tokens(const json& params, bool ranged) {
    const std::string path = document_of(params);
    Checked* c = project_of(path);
    if (c && !c->loaded) check_now();
    const auto read = read_of(path);
    if (!read) return {{"data", json::array()}};
    int first = 0, last = INT32_MAX;  // 0-based lines
    if (ranged) {
        const json range = params.value("range", json::object());
        first = range.value("start", json::object()).value("line", 0);
        last = range.value("end", json::object()).value("line", 0);
    }
    // The values a higher layer overrides here, on their lines now.
    std::vector<std::pair<int, std::string>> shadowed;
    if (c && c->loaded) {
        Moved moved(*c->loaded, buffers_);
        for (const LayerHint& h : c->loaded->hints)
            if (h.shadowed && h.file == path)
                if (const std::optional<int> line = moved.line(path, h.line))
                    shadowed.emplace_back(*line, h.key);
    }
    static const Reference kNone;
    std::vector<int> data;
    int line = 0, column = 0;  // the last token's, for LSP's relative encoding
    for (const SemanticToken& t : baker_lsp::semantic_tokens(*read, c ? c->loaded.get() : nullptr,
                                                             c ? c->reference : kNone, shadowed)) {
        const auto [l, col] = read->lines.position(t.begin);
        if (l < first || l > last) continue;
        const auto [end_line, end_col] = read->lines.position(t.end);
        if (end_line != l) continue;  // one across lines: the grammar colors it
        data.insert(data.end(), {l - line, l == line ? col - column : col, end_col - col,
                                 static_cast<int>(t.type), static_cast<int>(t.modifiers)});
        line = l;
        column = col;
    }
    return {{"data", std::move(data)}};
}

json Server::code_lenses(const json& params) {
    json out = json::array();
    const std::string path = document_of(params);
    std::string text;
    Checked* c = project_of(path);
    if (!c || c->setup.project != path || !buffers_.read(path, text)) return out;
    // (project ... (layers (name "file" ...) ...)): a lens on each layer, and one for the stack.
    const auto read = read_of(path);
    const Structure& s = read->structure;
    const LineIndex& lines = read->lines;
    const json uri = uri_for(path);
    const auto lens = [&](const Token& at, const std::string& title, const std::string& command,
                          json args) {
        const auto [l0, c0] = lines.position(at.begin);
        const auto [l1, c1] = lines.position(at.end);
        out.push_back({{"range",
                        {{"start", {{"line", l0}, {"character", c0}}},
                         {"end", {{"line", l1}, {"character", c1}}}}},
                       {"command",
                        {{"title", title}, {"command", command}, {"arguments", std::move(args)}}}});
    };
    for (size_t b = 0; b < s.brackets.size(); ++b) {
        const Bracket& layers = s.brackets[b];
        if (layers.open != '(' || s.tag(static_cast<int>(b)) != "layers" || layers.parent < 0 ||
            s.tag(layers.parent) != "project")
            continue;
        lens(s.tokens[layers.tag], "Show the merged stack", "baker.showMerged", json::array({uri}));
        for (size_t l = b + 1; l < s.brackets.size() && s.brackets[l].begin < layers.end; ++l)
            if (s.brackets[l].parent == static_cast<int>(b) && s.brackets[l].tag >= 0)
                lens(s.tokens[s.brackets[l].tag], "Show what this layer changes",
                     "baker.showLayerEffect",
                     json::array({uri, std::string(s.tag(static_cast<int>(l)))}));
    }
    return out;
}

void Server::check_now() {
    due_.reset();
    std::map<std::string, json> by_uri;
    bool hints_changed = false;
    for (auto& [key, c] : projects_) {
        Diagnosis d = diagnose(c.setup, *c.ctx, buffers_, c.have_reference, &c.kept);
        // The game's tweaks, held to the data by Baker (check_tweaks) for its own types, and
        // ranges only the running game enforces: metadata in the data, a game type's tweaks.
        if (d.loaded) {
            const GameTweaks* tweaks = c.tweaks.get();
            for (Problem& p : range_problems(
                     *d.loaded, c.reference,
                     [tweaks](const std::string& path) { return tweaks && tweaks->checks(path); }))
                d.problems.push_back(std::move(p));
            if (tweaks)
                for (Problem& p : tweaks->check(*d.loaded)) d.problems.push_back(std::move(p));
        }
        // While a file is mid-edit it often does not read, and its layer, with the shapes
        // and packs it brings, drops out; hover and completion keep the last whole stack.
        if (d.loaded && (d.every_layer_read || !c.loaded)) {
            hints_changed = hints_changed || !c.loaded || c.loaded->hints != d.loaded->hints;
            c.loaded = d.loaded;
        }
        // And its shadowed values, on their lines now: a check whose layers did not all read
        // has lost the hints of the layer that did not.
        if (!d.every_layer_read && c.loaded && c.loaded != d.loaded) {
            std::erase_if(d.problems, [](const Problem& p) { return p.code == "shadowed"; });
            Moved moved(*c.loaded, buffers_);
            for (Problem p : c.loaded->shadowed)
                if (const std::optional<int> line = moved.line(p.file, p.line)) {
                    p.line = *line;
                    d.problems.push_back(std::move(p));
                }
        }
        // What the last runs of the project's scenarios found, until each runs again.
        std::vector<Problem> problems = d.problems;
        if (const auto sim = sim_.find(key); sim != sim_.end())
            for (const auto& [scenario, found] : sim->second)
                problems.insert(problems.end(), found.begin(), found.end());
        for (const Problem& p : problems) {
            if (p.file.empty()) {
                log(p.severity == Severity::Error ? kLogError : kLogWarning, p.message);
                continue;
            }
            std::string text;
            buffers_.read(p.file, text);
            json diagnostic = {{"range", to_json(problem_range(text, p.line, p.column, p.key))},
                               {"severity", static_cast<int>(p.severity)},
                               {"source", p.code == "sim" ? "baker sim" : "baker"},
                               {"code", p.code},
                               {"message", p.message}};
            if (p.unnecessary) diagnostic["tags"] = json::array({1 /* Unnecessary */});
            by_uri[uri_for(p.file)].push_back(std::move(diagnostic));
        }
        char took[160];
        std::snprintf(took, sizeof took, "%s: checked %zu files in %.1f ms: %zu problems",
                      key.empty() ? "the command line's setup" : key.c_str(), d.files.size(),
                      d.milliseconds, d.problems.size());
        log(kLogInfo, took);
    }
    std::set<std::string> now_published;
    for (auto& [uri, list] : by_uri) {
        send_({{"jsonrpc", "2.0"},
               {"method", "textDocument/publishDiagnostics"},
               {"params", {{"uri", uri}, {"diagnostics", std::move(list)}}}});
        now_published.insert(uri);
    }
    for (const std::string& uri : published_)
        if (!now_published.count(uri))
            send_({{"jsonrpc", "2.0"},
                   {"method", "textDocument/publishDiagnostics"},
                   {"params", {{"uri", uri}, {"diagnostics", json::array()}}}});
    published_ = std::move(now_published);
    // An edit to one layer moves the hints of another layer's open editor.
    if (hints_changed && refresh_inlays_ && initialized_)
        send_({{"jsonrpc", "2.0"},
               {"id", "refresh-" + std::to_string(next_request_++)},
               {"method", "workspace/inlayHint/refresh"}});
}

void Server::log(int type, const std::string& text) {
    if (!initialized_) {  // the client takes window/logMessage only after initialize
        pending_log_.emplace_back(type, text);
        return;
    }
    send_({{"jsonrpc", "2.0"},
           {"method", "window/logMessage"},
           {"params", {{"type", type}, {"message", text}}}});
}

std::string Server::uri_for(const std::string& path) const {
    auto it = uris_.find(path);
    return it != uris_.end() ? it->second : uri_from_path(path);
}

}  // namespace baker_lsp
