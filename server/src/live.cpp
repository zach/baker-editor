#include "live.hpp"

namespace baker_lsp {

std::optional<nlohmann::json> Live::ask(const std::string& command) {
    if (port_ == 0) return std::nullopt;
    const auto now = std::chrono::steady_clock::now();
    if (!connection_.open()) {
        if (now < next_try_) return std::nullopt;
        next_try_ = now + kRetry;
        write_back_.reset();
        if (!connection_.connect(port_, static_cast<int>(kTimeout.count()))) return std::nullopt;
    }
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

std::optional<std::vector<LiveEntry>> Live::explain(const std::string& path) {
    const auto reply = ask("{config.explain " + path + "}");
    if (!reply || !reply->value("ok", false)) return std::nullopt;
    const nlohmann::json value = reply->value("value", nlohmann::json());
    if (!value.is_array()) return std::nullopt;
    std::vector<LiveEntry> out;
    for (const nlohmann::json& e : value) {
        if (!e.is_object()) continue;
        const nlohmann::json v = e.value("value", nlohmann::json());
        out.push_back({e.value("layer", ""), v.is_string() ? v.get<std::string>() : v.dump(),
                       e.value("standing", ""), e.value("file", ""), e.value("line", 0)});
    }
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
