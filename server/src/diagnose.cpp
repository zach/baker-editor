#include "diagnose.hpp"

#include "positions.hpp"

#include <baker/config.hpp>
#include <baker/parse.hpp>
#include <baker/shape.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>
#include <sstream>
#include <tuple>

namespace baker_lsp {

bool operator<(const Problem& a, const Problem& b) {
    return std::tie(a.file, a.line, a.column, a.severity, a.message, a.code) <
           std::tie(b.file, b.line, b.column, b.severity, b.message, b.code);
}

const std::string* Buffers::find(const std::string& path) const {
    auto it = text_.find(path);
    return it == text_.end() ? nullptr : &it->second;
}

bool Buffers::read(const std::string& path, std::string& out) const {
    if (const std::string* t = find(path)) {
        out = *t;
        return true;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream s;
    s << in.rdbuf();
    out = std::move(s).str();
    return true;
}

namespace {

class Collector {
public:
    void add(const baker::Error& e, Severity severity) {
        Problem p;
        if (!e.file().str().empty()) p.file = canonical_path(std::string(e.file().str()));
        p.line = e.line();
        p.column = e.column();
        p.severity = severity;
        p.code = std::string(baker::error_code_name(e.code()));
        p.message = std::string(e.message());
        if (!e.path().empty()) {
            p.message += " (at " + std::string(e.path()) + ")";
            p.key = last_path_element(e.path());
        } else if (const size_t q = p.message.find('\''); q != std::string::npos) {
            // A call's error names the function: "no overload of '-' takes 3 arguments".
            const size_t end = p.message.find('\'', q + 1);
            if (end != std::string::npos) p.key = p.message.substr(q + 1, end - q - 1);
        }
        problems.push_back(std::move(p));
    }
    void add(const baker::List<baker::Error>& errors, Severity severity) {
        for (const baker::Error& e : errors) add(e, severity);
    }
    std::vector<Problem> problems;
};

}  // namespace

Diagnosis diagnose(const Setup& setup, const baker::Context& ctx, const Buffers& buffers,
                   bool undefined_fails) {
    const auto start = std::chrono::steady_clock::now();
    Diagnosis out;
    Collector found;
    std::set<std::string> files;

    baker::Config cfg;
    cfg.set_warning_handler([&found](const baker::Error& e) { found.add(e, Severity::Warning); });
    baker::LoadSession& session = cfg.session();
    if (!setup.pack_levels.empty()) {
        std::vector<baker::Symbol> levels(setup.pack_levels.begin(), setup.pack_levels.end());
        session.set_pack_levels(std::span<const baker::Symbol>(levels.data(), levels.size()));
    }
    for (const std::string& dir : setup.include_dirs) session.add_include_dir(dir);
    for (const std::string& name : setup.defines) session.define(baker::Symbol(name));
    session.set_reader([&buffers, &files](std::string_view path, baker::TextSink sink) {
        const std::string file = canonical_path(std::string(path));
        std::string text;
        if (!buffers.read(file, text)) return false;
        files.insert(file);
        sink(text);
        return true;
    });

    // Extra files read with the same setup, before the layers add their macros and packs.
    baker::LoadSession extra_session = session;

    for (const Layer& layer : setup.layers)
        if (auto r = cfg.push_layer(layer.name, layer.path, {.optional = layer.optional}); !r)
            found.add(r.error(), Severity::Error);

    std::vector<baker::Ref<baker::Array>> extra;
    const std::set<std::string> layered = files;
    for (const auto& [path, text] : buffers.all()) {
        if (layered.count(path) || path.size() < 4 || path.compare(path.size() - 4, 4, ".dta"))
            continue;
        if (auto tree = baker::read_file(path, extra_session))
            extra.push_back(*tree);
        else
            found.add(tree.error(), Severity::Error);
    }
    std::vector<const baker::Array*> extra_trees;
    for (const auto& t : extra) extra_trees.push_back(t.get());

    if (cfg.root()) {
        baker::CheckReport report;
        baker::check(cfg, ctx, report, {.unresolved_fails = undefined_fails},
                     std::span<const baker::Array* const>(extra_trees.data(), extra_trees.size()));
        found.add(report.errors, Severity::Error);
        found.add(report.warnings, Severity::Warning);  // the one-way rule
        if (undefined_fails)
            for (const baker::CheckReport::Demand& d : report.demand) {
                // A method's class is the game's, which the reference does not give the
                // server yet: only functions are reported.
                if (d.stub || !d.on.str().empty()) continue;
                if (std::find(setup.objects.begin(), setup.objects.end(), d.name.str()) !=
                    setup.objects.end())
                    continue;
                for (const baker::CheckReport::Site& s : d.sites) {
                    Problem p;
                    p.file = canonical_path(std::string(s.file.str()));
                    p.line = s.line;
                    p.code = "undefined";
                    p.key = std::string(d.name.str());
                    p.message = "'" + std::string(d.name.str()) +
                                "' is not defined: not in the game's reference, and no {func} "
                                "defines it";
                    found.problems.push_back(std::move(p));
                }
            }

        baker::shape::Set shapes;
        baker::shape::Report shaped;
        shapes.add(*cfg.root(), shaped, &cfg.read_session());
        baker::shape::check(*cfg.root(), shapes, shaped);
        found.add(shaped.errors, Severity::Error);
        // A field no shape declares: reported, never a failure.
        found.add(shaped.warnings, Severity::Information);
    }

    out.problems = std::move(found.problems);
    std::sort(out.problems.begin(), out.problems.end());
    out.problems.erase(std::unique(out.problems.begin(), out.problems.end()), out.problems.end());
    out.files.assign(files.begin(), files.end());
    out.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return out;
}

}  // namespace baker_lsp
