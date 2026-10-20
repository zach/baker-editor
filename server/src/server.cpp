#include "server.hpp"

#include "features.hpp"
#include "outline.hpp"
#include "positions.hpp"
#include "reference.hpp"
#include "structure.hpp"

#include <baker/project.hpp>

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

Server::Server(Setup setup, Send send)
    : given_(std::move(setup)), live_(given_.console_port), send_(std::move(send)) {
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
    // --reference wins; else the project file's (reference ...).
    if (setup.reference.empty() && !key.empty()) {
        const baker::LoadSession::Reader reader = [this](std::string_view path,
                                                         baker::TextSink sink) {
            std::string text;
            if (!buffers_.read(canonical_path(std::string(path)), text)) return false;
            sink(text);
            return true;
        };
        if (auto p = baker::read_project(key, reader); p && !p->reference.str().empty())
            setup.reference = canonical_path(std::string(p->reference.str()));
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
            c.reference = r;
            log(kLogInfo, "the game's reference: " + std::to_string(r.functions) + " functions, " +
                              std::to_string(r.overloads) + " overloads, " +
                              std::to_string(r.objects.size()) + " named objects, from " +
                              setup.reference);
            if (r.widened > 0)
                log(kLogInfo, std::to_string(r.widened) +
                                  " functions take a type this server cannot declare; any "
                                  "value is accepted for it");
            for (const std::string& f : r.failed) log(kLogWarning, "not declared: " + f);
        }
    }
    c.setup = std::move(setup);
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

void Server::request(const json& message) {
    const std::string method = message.value("method", "");
    json reply = {{"jsonrpc", "2.0"}, {"id", message["id"]}};
    if (method == "initialize") {
        const json params = message.value("params", json::object());
        if (const json folders = params.value("workspaceFolders", json()); folders.is_array())
            for (const json& f : folders) find_project(path_from_uri(f.value("uri", "")));
        if (params.contains("rootUri") && params["rootUri"].is_string())
            find_project(path_from_uri(params["rootUri"].get<std::string>()));
        reply["result"] = {
            {"capabilities",
             {{"textDocumentSync",
               {{"openClose", true}, {"change", 1 /* full */}, {"save", {{"includeText", false}}}}},
              {"hoverProvider", true},
              {"definitionProvider", true},
              {"documentSymbolProvider", true},
              {"foldingRangeProvider", true},
              {"referencesProvider", true},
              {"renameProvider", {{"prepareProvider", true}}},
              {"codeActionProvider", true},
              {"executeCommandProvider", {{"commands", json::array({"baker.writeBack"})}}},
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
        const json& changes = params.value("contentChanges", json::array());
        if (changes.empty()) return;
        // Full sync: the last change holds the whole text.
        buffers_.set(canonical_path(document_path()), changes.back().value("text", ""));
        schedule(now + kDebounce, true);
    } else if (method == "textDocument/didClose") {
        buffers_.erase(canonical_path(document_path()));
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

json Server::hover(const json& params) {
    if (due_) check_now();  // answer from the text as it is now
    std::string path, text;
    size_t offset = 0;
    if (!place(params, buffers_, path, text, offset)) return nullptr;
    Checked* c = project_of(path);
    if (!c || !c->loaded) return nullptr;
    auto h = baker_lsp::hover(text, offset, *c->loaded, c->reference);
    if (!h) return nullptr;
    if (!h->path.empty())
        if (const auto chain = live_chain(*c, h->path)) {
            h->markdown += "\n\n**Live**, the game on 127.0.0.1:" + std::to_string(live_.port()) +
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
    if (due_) check_now();
    std::string path, text;
    size_t offset = 0;
    if (!place(params, buffers_, path, text, offset)) return json::array();
    Checked* c = project_of(path);
    if (!c || !c->loaded) return json::array();
    const Completions found = baker_lsp::complete(text, offset, *c->loaded, c->reference);
    json items = json::array();
    const json range = range_json(text, found.begin, found.end);
    for (const Completion& item : found.items)
        items.push_back({{"label", item.label},
                         {"kind", static_cast<int>(item.kind)},
                         {"detail", item.detail},
                         {"documentation", {{"kind", "markdown"}, {"value", item.documentation}}},
                         {"textEdit", {{"range", range}, {"newText", item.label}}}});
    return {{"isIncomplete", false}, {"items", std::move(items)}};
}

json Server::definition(const json& params) {
    if (due_) check_now();
    std::string path, text;
    size_t offset = 0;
    if (!place(params, buffers_, path, text, offset)) return nullptr;
    Checked* c = project_of(path);
    if (!c || !c->loaded) return nullptr;
    const auto exists = [this](const std::string& f) {
        std::string ignored;
        return buffers_.read(f, ignored);
    };
    json out = json::array();
    for (const Location& l :
         baker_lsp::definition(text, offset, path, *c->loaded, c->reference, exists)) {
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
    auto chain = live_.explain(path);
    if (!chain) return std::nullopt;
    // The game on the port may be another project's: live only if its files are this one's.
    for (const LiveEntry& e : *chain)
        if (!e.file.empty() && under(canonical_path(e.file), c.setup)) return chain;
    return std::nullopt;
}

json Server::code_actions(const json& params) {
    json out = json::array();
    std::string path, text;
    size_t offset = 0;
    json at = params;
    at["position"] = params.value("range", json::object()).value("start", json::object());
    if (!place(at, buffers_, path, text, offset)) return out;
    Checked* c = project_of(path);
    const std::string data = data_path_at(text, offset);
    if (!c || data.empty()) return out;
    const auto chain = live_chain(*c, data);
    if (!chain) return out;
    // A value tuned in the running game and not yet written: the game's own write-back
    // writes it to the line it came from.
    bool live_edit = false;
    for (const LiveEntry& e : *chain)
        live_edit = live_edit || (e.layer == "scratch" && e.standing == "wins");
    if (!live_edit || !live_.can_write_back()) return out;
    out.push_back({{"title", "Write the live value here"},
                   {"kind", "quickfix"},
                   {"command",
                    {{"title", "Write the live value here"},
                     {"command", "baker.writeBack"},
                     {"arguments", json::array({data})}}}});
    return out;
}

json Server::execute(const json& params) {
    if (params.value("command", "") != "baker.writeBack") return nullptr;
    const json args = params.value("arguments", json::array());
    if (args.empty() || !args[0].is_string()) return nullptr;
    const std::string path = args[0].get<std::string>();
    const auto reply = live_.ask("{config.write_back " + path + "}");
    std::string message;
    int type = kLogError;
    if (!reply) {
        message = "no game answers on 127.0.0.1:" + std::to_string(live_.port());
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

json Server::document_symbols(const json& params) {
    std::string text;
    if (!buffers_.read(document_of(params), text)) return json::array();
    const LineIndex lines(text);
    std::vector<OutlineSymbol> symbols = outline(text, kMaxSymbols);
    if (const size_t cut = cap_outline(symbols, kMaxSymbols))
        log(kLogInfo, "outline: " + std::to_string(cut) +
                          " symbols left out of a large file; "
                          "leaf fields go first");
    json out = json::array();
    for (const OutlineSymbol& s : symbols) out.push_back(symbol_json(lines, s));
    return out;
}

json Server::folding_ranges(const json& params) {
    std::string text;
    if (!buffers_.read(document_of(params), text)) return json::array();
    const LineIndex lines(text);
    json out = json::array();
    for (const Fold& f : folds(text)) {
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
    const auto target = target_at(text, offset, *c->loaded, c->reference);
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
    const auto target = target_at(text, offset, *c->loaded, c->reference);
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
    const auto target = target_at(text, offset, *c->loaded, c->reference);
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

void Server::check_now() {
    due_.reset();
    std::map<std::string, json> by_uri;
    for (auto& [key, c] : projects_) {
        const Diagnosis d = diagnose(c.setup, *c.ctx, buffers_, c.have_reference);
        // While a file is mid-edit it often does not read, and its layer, with the shapes
        // and packs it brings, drops out; hover and completion keep the last whole stack.
        if (d.loaded && (d.every_layer_read || !c.loaded)) c.loaded = d.loaded;
        for (const Problem& p : d.problems) {
            if (p.file.empty()) {
                log(p.severity == Severity::Error ? kLogError : kLogWarning, p.message);
                continue;
            }
            std::string text;
            buffers_.read(p.file, text);
            json diagnostic = {{"range", to_json(problem_range(text, p.line, p.column, p.key))},
                               {"severity", static_cast<int>(p.severity)},
                               {"source", "baker"},
                               {"code", p.code},
                               {"message", p.message}};
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
