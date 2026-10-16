#include "diagnose.hpp"

#include "positions.hpp"

#include <baker/config.hpp>
#include <baker/parse.hpp>
#include <baker/project.hpp>
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

// Macros and script functions, found in the text of the files the load read. A session
// does not list its macros, and a {func} is only known once its #autorun runs.
void find_definitions(const std::vector<std::string>& files, const Buffers& buffers,
                      Loaded& loaded) {
    for (const std::string& file : files) {
        std::string text;
        if (!buffers.read(file, text)) continue;
        int line = 0;
        for (size_t start = 0; start <= text.size();) {
            size_t nl = text.find('\n', start);
            if (nl == std::string::npos) nl = text.size();
            const std::string_view l = std::string_view(text).substr(start, nl - start);
            ++line;
            start = nl + 1;
            size_t i = l.find_first_not_of(" \t");
            if (i == std::string_view::npos) continue;
            const auto word_at = [&l](size_t at) {
                const size_t b = l.find_first_not_of(" \t", at);
                if (b == std::string_view::npos) return std::string();
                size_t e = b;
                while (e < l.size() && l[e] != ' ' && l[e] != '\t' && l[e] != '(' && l[e] != ')' &&
                       l[e] != '[' && l[e] != ']' && l[e] != '{' && l[e] != '}')
                    ++e;
                return std::string(l.substr(b, e - b));
            };
            if (l.compare(i, 7, "#define") == 0) {
                if (std::string n = word_at(i + 7); !n.empty())
                    loaded.macros.push_back({n, file, line});
            } else if (i == 0 && l[0] == '[') {
                if (std::string n = word_at(1); !n.empty())
                    loaded.macros.push_back({n, file, line});
            }
            for (size_t f = l.find("{func"); f != std::string_view::npos;
                 f = l.find("{func", f + 5)) {
                if (f + 5 < l.size() && l[f + 5] != ' ' && l[f + 5] != '\t') continue;
                if (std::string n = word_at(f + 5); !n.empty())
                    loaded.functions.push_back({n, file, line});
            }
        }
    }
}

Diagnosis finish(Diagnosis& out, Collector& found, const std::set<std::string>& files,
                 std::chrono::steady_clock::time_point start) {
    out.problems = std::move(found.problems);
    std::sort(out.problems.begin(), out.problems.end());
    out.problems.erase(std::unique(out.problems.begin(), out.problems.end()), out.problems.end());
    out.files.assign(files.begin(), files.end());
    out.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return std::move(out);
}

template <class T>
baker::List<T> list_of(const std::vector<T>& v) {
    return baker::List<T>(std::span<const T>(v.data(), v.size()));
}

// The command line's setup as a project, as if a project file had said it.
baker::Project project_of(const Setup& setup) {
    baker::Project p;
    std::vector<baker::ProjectLayer> layers;
    for (const Layer& l : setup.layers)
        layers.push_back({baker::Symbol(l.name), baker::Symbol(l.path), l.optional, l.enabled});
    std::vector<baker::ProjectEntry> levels, dirs, defines;
    for (const std::string& s : setup.pack_levels) levels.push_back({baker::Symbol(s)});
    for (const std::string& s : setup.include_dirs) dirs.push_back({baker::Symbol(s)});
    for (const std::string& s : setup.defines) defines.push_back({baker::Symbol(s)});
    p.layers = list_of(layers);
    p.pack_levels = list_of(levels);
    p.include_dirs = list_of(dirs);
    p.defines = list_of(defines);
    return p;
}

}  // namespace

Diagnosis diagnose(const Setup& setup, const baker::Context& ctx, const Buffers& buffers,
                   bool undefined_fails) {
    const auto start = std::chrono::steady_clock::now();
    Diagnosis out;
    Collector found;
    // Shared with the reader, which the kept Config holds after this returns.
    const auto read_files = std::make_shared<std::set<std::string>>();
    std::set<std::string>& files = *read_files;

    // Every file is read through the editor's buffers, the project file too.
    const baker::LoadSession::Reader reader = [&buffers, read_files](std::string_view path,
                                                                     baker::TextSink sink) {
        const std::string file = canonical_path(std::string(path));
        std::string text;
        if (!buffers.read(file, text)) return false;
        read_files->insert(file);
        sink(text);
        return true;
    };

    baker::Project project;
    if (setup.project.empty()) {
        project = project_of(setup);
    } else if (auto read = baker::read_project(setup.project, reader)) {
        project = std::move(*read);
    } else {
        found.add(read.error(), Severity::Error);
        if (found.problems.back().file.empty()) found.problems.back().file = setup.project;
        return finish(out, found, files, start);
    }
    // The skipped layers: ProjectOptions::skip takes a list written in the code.
    std::vector<baker::ProjectLayer> kept;
    for (const baker::ProjectLayer& l : project.layers)
        if (std::find(setup.skip.begin(), setup.skip.end(), l.name.str()) == setup.skip.end())
            kept.push_back(l);
    project.layers = list_of(kept);

    auto loaded = std::make_shared<Loaded>();
    loaded->config = std::make_unique<baker::Config>();
    baker::Config& cfg = *loaded->config;
    cfg.set_warning_handler([&found](const baker::Error& e) { found.add(e, Severity::Warning); });
    cfg.session().set_reader(reader);
    if (auto opened = baker::open_project(cfg, project, {.go_on = true}); !opened) {
        found.add(opened.error(), Severity::Error);
    } else {
        out.every_layer_read = opened->empty();
        for (const baker::ProjectProblem& p : *opened) {
            found.add(p.error, Severity::Error);
            // A layer whose file is not there: on the project file, at the entry naming it.
            Problem& last = found.problems.back();
            if (!last.file.empty() || setup.project.empty()) continue;
            last.file = setup.project;
            for (const baker::ProjectLayer& l : project.layers)
                if (l.name == p.layer) {
                    last.line = l.path_at.line;
                    last.column = l.path_at.column;
                }
        }
    }

    // An open .dta file the layers do not read, read on its own with the project's setup.
    baker::LoadSession extra_session;
    extra_session.set_reader(reader);
    std::vector<baker::Symbol> levels;
    for (const baker::ProjectEntry& e : project.pack_levels) levels.push_back(e.value);
    if (!levels.empty())
        extra_session.set_pack_levels(std::span<const baker::Symbol>(levels.data(), levels.size()));
    for (const baker::ProjectEntry& e : project.include_dirs)
        extra_session.add_include_dir(e.value.str());
    for (const baker::ProjectEntry& e : project.defines) extra_session.define(e.value);

    std::vector<baker::Ref<baker::Array>> extra;
    const std::set<std::string> layered = files;
    for (const auto& [path, text] : buffers.all()) {
        if (layered.count(path) || path == setup.project || path.size() < 4 ||
            path.compare(path.size() - 4, 4, ".dta") || !under(path, setup))
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
                // A method's class is the game's, which the server cannot declare: only
                // functions are reported. A call on an object the game names is no typo.
                if (d.stub || !d.on.str().empty()) continue;
                if (std::find(setup.objects.begin(), setup.objects.end(), d.name.str()) !=
                    setup.objects.end())
                    continue;
                for (const baker::CheckReport::Site& site : d.sites) {
                    Problem p;
                    p.file = canonical_path(std::string(site.file.str()));
                    p.line = site.line;
                    p.code = "undefined";
                    p.key = std::string(d.name.str());
                    p.message = "'" + std::string(d.name.str()) +
                                "' is not defined: not in the game's reference, and no {func} "
                                "defines it";
                    found.problems.push_back(std::move(p));
                }
            }

        baker::shape::Set& shapes = loaded->shapes;
        baker::shape::Report shaped;
        shapes.add(*cfg.root(), shaped, &cfg.read_session());
        baker::shape::check(*cfg.root(), shapes, shaped);
        found.add(shaped.errors, Severity::Error);
        // A field no shape declares: reported, never a failure.
        found.add(shaped.warnings, Severity::Information);
    }
    cfg.set_warning_handler(nullptr);  // `found` goes with this call
    for (const baker::Pack& p : cfg.read_session().packs())
        loaded->packs.emplace_back(p.name.str());
    find_definitions(std::vector<std::string>(files.begin(), files.end()), buffers, *loaded);
    out.loaded = std::move(loaded);
    return finish(out, found, files, start);
}

bool under(const std::string& path, const Setup& setup) {
    if (setup.project.empty()) return true;
    const size_t slash = setup.project.rfind('/');
    const std::string_view root = std::string_view(setup.project).substr(0, slash + 1);
    return std::string_view(path).substr(0, root.size()) == root;
}

}  // namespace baker_lsp
