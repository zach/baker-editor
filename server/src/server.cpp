#include "server.hpp"

#include "positions.hpp"
#include "stubs.hpp"

#include <cstdio>
#include <utility>

namespace baker_lsp {

using nlohmann::json;

namespace {

constexpr int kLogError = 1, kLogWarning = 2, kLogInfo = 3;

json to_json(const Range& r) {
    return {{"start", {{"line", r.start.line}, {"character", r.start.character}}},
            {"end", {{"line", r.end.line}, {"character", r.end.character}}}};
}

}  // namespace

Server::Server(Setup setup, Send send) : setup_(std::move(setup)), send_(std::move(send)) {
    if (setup_.reference.empty()) {
        startup_log_.push_back(
            "no --reference: calls are not counted against the game's functions, and calls to "
            "undefined functions are not reported");
        return;
    }
    StubCount count;
    std::string error;
    if (!register_stubs(ctx_, setup_.reference, count, error)) {
        startup_log_.push_back(error + "; checking without the game's functions");
        return;
    }
    have_reference_ = true;
    startup_log_.push_back("the game's reference: " + std::to_string(count.functions) +
                           " functions, " + std::to_string(count.overloads) + " overloads, from " +
                           setup_.reference);
    if (count.approximate > 0)
        startup_log_.push_back(std::to_string(count.approximate) +
                               " overloads take more arguments than the server counts; any "
                               "number is accepted for them");
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
        find_project(message.value("params", json::object()));
        reply["result"] = {{"capabilities",
                            {{"textDocumentSync",
                              {{"openClose", true},
                               {"change", 1 /* full */},
                               {"save", {{"includeText", false}}}}}}},
                           {"serverInfo", {{"name", "baker-lsp"}, {"version", "0.1.0"}}}};
    } else if (method == "shutdown") {
        shutdown_ = true;
        reply["result"] = nullptr;
    } else {
        reply["error"] = {{"code", -32601}, {"message", "not supported: " + method}};
    }
    send_(reply);
}

void Server::find_project(const json& params) {
    if (!setup_.project.empty() || setup_.by_hand()) return;
    std::vector<std::string> roots;
    if (const json folders = params.value("workspaceFolders", json()); folders.is_array())
        for (const json& f : folders) roots.push_back(path_from_uri(f.value("uri", "")));
    if (params.contains("rootUri") && params["rootUri"].is_string())
        roots.push_back(path_from_uri(params["rootUri"].get<std::string>()));
    for (const std::string& root : roots) {
        if (root.empty()) continue;
        const std::string file = canonical_path(root + "/" + kProjectFile);
        std::string text;
        if (buffers_.read(file, text)) {
            setup_.project = file;
            startup_log_.push_back("the project: " + file);
            return;
        }
    }
    startup_log_.push_back(std::string("no ") + kProjectFile +
                           " at the workspace's root, and no --project or --layer: nothing to "
                           "check");
}

void Server::notification(const json& message, Clock::time_point now) {
    const std::string method = message.value("method", "");
    const json params = message.value("params", json::object());
    const auto document_path = [&params]() {
        return path_from_uri(params.value("textDocument", json::object()).value("uri", ""));
    };
    if (method == "initialized") {
        for (const std::string& line : startup_log_)
            log(have_reference_ ? kLogInfo : kLogWarning, line);
        schedule(now);
    } else if (method == "exit") {
        exit_code_ = shutdown_ ? 0 : 1;
    } else if (method == "textDocument/didOpen") {
        const json doc = params.value("textDocument", json::object());
        const std::string path = canonical_path(document_path());
        uris_[path] = doc.value("uri", "");
        buffers_.set(path, doc.value("text", ""));
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

void Server::check_now() {
    due_.reset();
    if (setup_.project.empty() && !setup_.by_hand()) return;
    const Diagnosis d = diagnose(setup_, ctx_, buffers_, have_reference_);
    std::map<std::string, json> by_uri;
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
    char took[96];
    std::snprintf(took, sizeof took, "checked %zu files in %.1f ms: %zu problems", d.files.size(),
                  d.milliseconds, d.problems.size());
    log(kLogInfo, took);
}

void Server::log(int type, const std::string& text) {
    send_({{"jsonrpc", "2.0"},
           {"method", "window/logMessage"},
           {"params", {{"type", type}, {"message", text}}}});
}

std::string Server::uri_for(const std::string& path) const {
    auto it = uris_.find(path);
    return it != uris_.end() ? it->second : uri_from_path(path);
}

}  // namespace baker_lsp
