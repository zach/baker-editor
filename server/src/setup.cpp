#include "setup.hpp"

#include <cstdlib>
#include <filesystem>

namespace baker_lsp {

std::string canonical_path(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path p = fs::weakly_canonical(fs::absolute(fs::path(path), ec), ec);
    if (ec) p = fs::absolute(fs::path(path), ec).lexically_normal();
    return p.generic_string();
}

bool parse_args(std::span<const std::string> args, Setup& setup, std::string& error) {
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--stdio") continue;
        if (a == "--live-inlays") {
            setup.live_inlays = true;
            continue;
        }
        if (i + 1 >= args.size()) {
            error = a.rfind("--", 0) == 0 ? a + " needs a value" : "unexpected argument " + a;
            return false;
        }
        const std::string& v = args[++i];
        if (a == "--layer" || a == "--optional-layer") {
            const size_t eq = v.find('=');
            if (eq == 0 || eq == std::string::npos || eq + 1 == v.size()) {
                error = a + " takes NAME=FILE, not " + v;
                return false;
            }
            setup.layers.push_back(
                {v.substr(0, eq), canonical_path(v.substr(eq + 1)), a == "--optional-layer"});
        } else if (a == "--project") {
            setup.project = canonical_path(v);
        } else if (a == "--skip") {
            setup.skip.push_back(v);
        } else if (a == "--pack-levels") {
            setup.pack_levels.clear();
            for (size_t start = 0; start <= v.size();) {
                size_t comma = v.find(',', start);
                if (comma == std::string::npos) comma = v.size();
                if (comma > start) setup.pack_levels.push_back(v.substr(start, comma - start));
                start = comma + 1;
            }
        } else if (a == "--include") {
            setup.include_dirs.push_back(canonical_path(v));
        } else if (a == "--define") {
            setup.defines.push_back(v);
        } else if (a == "--console-port") {
            char* end = nullptr;
            const unsigned long port = std::strtoul(v.c_str(), &end, 10);
            if (!end || *end || port > 65535) {
                error = "--console-port takes a port, 0 to 65535, not " + v;
                return false;
            }
            setup.console_port = static_cast<uint16_t>(port);
        } else if (a == "--reference") {
            setup.reference = canonical_path(v);
        } else {
            error = "unknown option " + a;
            return false;
        }
    }
    if (setup.by_hand()) {
        if (!setup.project.empty()) {
            error =
                "--project with --layer, --pack-levels, --include or --define: the project "
                "file says those";
            return false;
        }
        if (!setup.skip.empty()) {
            error = "--skip leaves out a project's layer; without a project, omit the --layer";
            return false;
        }
        if (setup.layers.empty()) {
            error = "no layers: give at least one --layer NAME=FILE";
            return false;
        }
    }
    return true;
}

}  // namespace baker_lsp
