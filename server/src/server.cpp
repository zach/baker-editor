#include "server.hpp"

#include "features.hpp"
#include "positions.hpp"
#include "reference.hpp"

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
              {"completionProvider", {{"triggerCharacters", json::array({"(", "{", " "})}}}}},
            {"serverInfo", {{"name", "baker-lsp"}, {"version", "0.1.0"}}}};
    } else if (method == "textDocument/hover") {
        reply["result"] = hover(message.value("params", json::object()));
    } else if (method == "textDocument/definition") {
        reply["result"] = definition(message.value("params", json::object()));
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
    } else if (method == "textDocument/didSave" || method == "workspace/didChangeWatchedFiles") {
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
    const auto h = baker_lsp::hover(text, offset, *c->loaded, c->reference);
    if (!h) return nullptr;
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
