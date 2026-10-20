#include "live.hpp"

namespace baker_lsp {

namespace {

std::vector<LiveEntry> chain_of(const nlohmann::json& reply) {
    std::vector<LiveEntry> out;
    if (!reply.value("ok", false)) return out;
    const nlohmann::json value = reply.value("value", nlohmann::json());
    if (!value.is_array()) return out;
    for (const nlohmann::json& e : value) {
        if (!e.is_object()) continue;
        const nlohmann::json v = e.value("value", nlohmann::json());
        out.push_back({e.value("layer", ""), v.is_string() ? v.get<std::string>() : v.dump(),
                       e.value("standing", ""), e.value("file", ""), e.value("line", 0)});
    }
    return out;
}

}  // namespace

bool Live::ready() {
    if (port_ == 0) return false;
    if (connection_.open()) return true;
    const auto now = std::chrono::steady_clock::now();
    if (now < next_try_) return false;
    next_try_ = now + kRetry;
    write_back_.reset();
    return connection_.connect(port_, static_cast<int>(kTimeout.count()));
}

std::optional<nlohmann::json> Live::ask(const std::string& command) {
    if (!ready()) return std::nullopt;
    const int id = next_id_++;
    std::string reply;
    if (!connection_.send("(" + std::to_string(id) + " " + command + ")\n") ||
        !connection_.read_until(reply, "\n\n", static_cast<int>(kTimeout.count()))) {
        next_try_ = std::chrono::steady_clock::now() + kRetry;
        return std::nullopt;
    }
    nlohmann::json j = nlohmann::json::parse(reply, nullptr, false);
    if (j.is_discarded() || !j.is_object() || j.value("id", -1) != id) {
        connection_.close();  // out of step with the game: start again
        return std::nullopt;
    }
    return j;
}

std::optional<std::vector<nlohmann::json>> Live::ask_many(
    const std::vector<std::string>& commands) {
    if (commands.empty() || !ready()) return std::nullopt;
    const int first = next_id_;
    std::string requests;
    for (const std::string& command : commands)
        requests += "(" + std::to_string(next_id_++) + " " + command + ")\n";
    std::string replies;
    if (!connection_.send(requests) ||
        !connection_.read_until(replies, "\n\n", static_cast<int>(kTimeout.count()),
                                commands.size())) {
        next_try_ = std::chrono::steady_clock::now() + kRetry;
        return std::nullopt;
    }
    std::vector<nlohmann::json> out(commands.size());
    for (size_t start = 0, end; (end = replies.find("\n\n", start)) != std::string::npos;
         start = end + 2) {
        nlohmann::json j =
            nlohmann::json::parse(replies.substr(start, end - start), nullptr, false);
        const int at = j.is_object() ? j.value("id", -1) - first : -1;
        if (at < 0 || at >= static_cast<int>(out.size()) || !out[at].is_null()) {
            connection_.close();  // out of step with the game: start again
            return std::nullopt;
        }
        out[at] = std::move(j);
    }
    return out;
}

std::optional<std::vector<LiveEntry>> Live::explain(const std::string& path) {
    const auto reply = ask("{config.explain " + path + "}");
    if (!reply || !reply->value("ok", false)) return std::nullopt;
    if (!reply->value("value", nlohmann::json()).is_array()) return std::nullopt;
    return chain_of(*reply);
}

std::optional<std::vector<std::vector<LiveEntry>>> Live::explain_many(
    const std::vector<std::string>& paths) {
    std::vector<std::string> commands;
    commands.reserve(paths.size());
    for (const std::string& path : paths) commands.push_back("{config.explain " + path + "}");
    const auto replies = ask_many(commands);
    if (!replies) return std::nullopt;
    std::vector<std::vector<LiveEntry>> out;
    out.reserve(replies->size());
    for (const nlohmann::json& reply : *replies) out.push_back(chain_of(reply));
    return out;
}

bool Live::can_write_back() {
    if (!write_back_) {
        // With no arguments, a bound config.write_back fails for its arguments; an unbound
        // one is "not a function".
        const auto reply = ask("{config.write_back}");
        if (!reply) return false;
        const std::string message =
            reply->value("error", nlohmann::json::object()).value("message", "");
        write_back_ = message.find("is not a function") == std::string::npos;
    }
    return *write_back_;
}

}  // namespace baker_lsp
