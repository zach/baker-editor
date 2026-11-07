#include "diagnose.hpp"

#include "scenarios.hpp"

#include "positions.hpp"

#include <baker/config.hpp>
#include <baker/parse.hpp>
#include <baker/project.hpp>
#include <baker/shape.hpp>
#include <baker/std_interop.hpp>
#include <baker/tools/layers.hpp>
#include <baker/tools/migrate.hpp>
#include <baker/tools/reference.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

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
        problems.push_back(problem_of(e, severity));
    }
    void add(const baker::List<baker::Error>& errors, Severity severity) {
        for (const baker::Error& e : errors) add(e, severity);
    }
    std::vector<Problem> problems;
};

// The file name a hint shows: its last part.
std::string short_name(std::string_view file) {
    const size_t slash = file.find_last_of("/\\");
    return std::string(slash == std::string_view::npos ? file : file.substr(slash + 1));
}

// The last element of a dotted path, unquoted: fighters.'big one'.health -> health.
std::string last_dotted(std::string_view path) {
    size_t start = 0;
    bool quoted = false;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '\'')
            quoted = !quoted;
        else if ((path[i] == '.' || path[i] == '/') && !quoted)
            start = i + 1;
    }
    std::string_view last = path.substr(start);
    if (last.size() >= 2 && last.front() == '\'' && last.back() == '\'')
        last = last.substr(1, last.size() - 2);
    return std::string(last);
}

// The array at dotted `path` of `root`, or null.
const baker::Array* array_in(const baker::Array* root, std::string_view path) {
    const baker::Path p(path);
    const baker::Array* a = root;
    for (size_t i = 0; a && p.valid() && i < p.size(); ++i)
        a = p[i].is_index ? nullptr : a->try_find(p[i].name);
    return p.empty() ? nullptr : a;
}

// A value as a hint shows it: a record, a tagged array holding arrays, by its key.
std::string brief(const baker::Node& v) {
    const baker::Array* a = v.try_array();
    if (a && !a->tag().empty())
        for (const baker::Node& n : a->values())
            if (n.try_array()) return std::string(a->tag().str());
    return baker::to_string(v);
}

// `s`, cut to about `n` bytes with an ellipsis, for a hint's line.
std::string capped(std::string s, size_t n = 60) {
    if (s.size() <= n) return s;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
    return s.substr(0, n) + "\u2026";
}

// Values as Baker text: one is itself, several a list in parentheses.
std::string packed(const std::vector<std::string>& values) {
    std::string out;
    for (const std::string& v : values) out += (out.empty() ? "" : " ") + v;
    return values.size() == 1 ? out : "(" + out + ")";
}

// A row's values, each as Baker text (or as `show` shows it).
std::vector<std::string> unpacked(const baker::RowValues& values,
                                  std::string (*show)(const baker::Node&) = nullptr) {
    std::vector<std::string> out;
    for (const baker::Node& n : values) out.push_back(show ? show(n) : baker::to_string(n));
    return out;
}

// Whether `layer` is the top contributor to `path`: no layer above it (nor scratch) changes
// the value again.
bool top_contributor(const baker::Config& cfg, std::string_view path, baker::Symbol layer) {
    const auto chain = baker::tools::explain(cfg, baker::Path(path));
    if (!chain) return false;
    bool after = false;
    for (const baker::tools::ExplainEntry& e : *chain) {
        if (after && e.standing != baker::tools::Standing::Disabled) return false;
        after = after || e.layer == layer;
    }
    return after;
}

// Each layer's state in the merge, and the defines its conditions may name.
void find_conditions(const baker::Config& cfg, const baker::Project& project, Loaded& loaded) {
    std::vector<std::string> named;  // each name in order, once
    const auto add = [&](std::string_view name, std::string from) {
        if (name.empty() || std::find(named.begin(), named.end(), name) != named.end()) return;
        named.emplace_back(name);
        loaded.defines.emplace_back(std::string(name), std::move(from));
    };
    const baker::LoadSession& session = cfg.read_session();
    for (const baker::ActiveDefine& d : session.active_defines())
        add(d.name.str(), std::string(baker::define_from_name(d.from)));
    for (const baker::ConditionalSection& c : session.conditional_sections())
        add(c.name.str(), "not defined");
    for (const baker::ProjectLayer& l : project.layers) {
        loaded.layers.push_back({std::string(l.name.str()), std::string(l.when.str()),
                                 cfg.layer_enabled(l.name.str())});
        // The names its condition tests, (and PC (not DEMO)): its words but the three.
        const std::string_view when = l.when.str();
        for (size_t i = 0; i < when.size();) {
            const size_t end = when.find_first_of(" ()\t\n", i);
            const std::string_view word =
                when.substr(i, end == std::string_view::npos ? end : end - i);
            if (!word.empty() && word != "not" && word != "and" && word != "or")
                add(word, "not defined");
            if (end == std::string_view::npos) break;
            i = end + 1;
        }
    }
}

// The hints of every layer but the bottom one, and of what they override.
void find_hints(const baker::Config& cfg, const baker::Project& project, Loaded& loaded,
                Collector& found) {
    bool bottom = true;
    for (const baker::ProjectLayer& layer : project.layers) {
        if (!layer.enabled) continue;
        if (bottom) {  // its values are found as what the layers above override
            bottom = false;
            continue;
        }
        const auto effect = baker::tools::layer_effect(cfg, layer.name.str());
        if (!effect) continue;
        for (const baker::LayerEffectEntry& e : *effect) {
            if (e.source.file.str().empty()) continue;
            // A declaration combines with the ones below it: it overrides nothing.
            if (!e.declares.empty()) continue;
            const std::string file = canonical_path(std::string(e.source.file.str()));
            const std::string value = shown(e.values);
            const std::string key = last_dotted(e.path.str());
            LayerRow row;
            row.path = baker::to_string(baker::Path(e.path.str()));
            row.layer = std::string(layer.name.str());
            row.op = std::string(e.op.str());
            row.values = unpacked(e.values);
            row.operands = packed(unpacked(e.values, brief));
            row.file = file;
            row.line = e.source.line;
            row.stale = e.stale;
            if (e.below) {
                row.below = packed(unpacked(*e.below, brief));
                row.below_values = unpacked(*e.below);
                if (e.below_source && !e.below_source->file.str().empty()) {
                    row.below_file = canonical_path(std::string(e.below_source->file.str()));
                    row.below_line = e.below_source->line;
                }
            }
            if (row.composes()) {
                // It changes the list below, which stands: not an override, nothing faded.
                const baker::Ref<baker::Array> root = cfg.root();
                const baker::Array* now = top_contributor(cfg, e.path.str(), layer.name)
                                              ? array_in(root.get(), e.path.str())
                                              : nullptr;
                if (now && !now->values().empty()) {
                    std::vector<std::string> shown;
                    for (const baker::Node& v : now->values()) {
                        row.result_values.push_back(baker::to_string(v));
                        shown.push_back(brief(v));
                    }
                    row.result = packed(shown);
                }
                std::string label;
                if (!row.below.empty()) {
                    if (!row.below_file.empty())
                        label = "\u2190 " + short_name(row.below_file) + ":" +
                                std::to_string(row.below_line) + " ";
                    label += capped(row.below) + " ";
                }
                label += row.op + " " + capped(row.operands) +
                         (row.result.empty() ? "" : " \u2192 " + capped(row.result));
                loaded.hints.push_back(
                    {file, row.line, std::string(e.path.str()), key, label, false, value});
                if (!row.below_file.empty())
                    loaded.hints.push_back(
                        {row.below_file, row.below_line, std::string(e.path.str()), key,
                         "composed by " + short_name(file) + ":" + std::to_string(row.line) + " (" +
                             row.op + " " + row.operands + ")",
                         false, row.below});
                loaded.rows.push_back(std::move(row));
                continue;
            }
            loaded.rows.push_back(std::move(row));
            if (e.below && e.below_source && !e.below_source->file.str().empty()) {
                const std::string below_file =
                    canonical_path(std::string(e.below_source->file.str()));
                const std::string below = shown(*e.below);
                loaded.hints.push_back({file, e.source.line, std::string(e.path.str()), key,
                                        "over " + short_name(below_file) + ":" +
                                            std::to_string(e.below_source->line) + " (" + below +
                                            ")",
                                        e.shadowed, value});
                // The value below is shadowed by this one.
                loaded.hints.push_back({below_file, e.below_source->line, std::string(e.path.str()),
                                        key,
                                        "\u2190 " + short_name(file) + ":" +
                                            std::to_string(e.source.line) + " (" + value + ")",
                                        true, below});
                Problem p;
                p.file = below_file;
                p.line = e.below_source->line;
                p.severity = Severity::Hint;
                p.code = "shadowed";
                p.key = key;
                p.unnecessary = true;
                p.message = "shadowed: " + short_name(file) + ":" + std::to_string(e.source.line) +
                            " sets it to " + value;
                loaded.shadowed.push_back(p);
                found.problems.push_back(std::move(p));
            }
        }
    }
}

}  // namespace

std::string shown(const baker::RowValues& values) {
    return packed(unpacked(values));
}

namespace {

// A warning at each old form of the .dta files of `files` that a migration of Baker's rewrites
// (tools::migrate_text), found again in a file only when its text changed since `kept` kept it.
void find_old_forms(const std::set<std::string>& files, const Buffers& buffers, Kept* kept,
                    Collector& found) {
    for (const std::string& file : files) {
        if (file.size() < 4 || file.compare(file.size() - 4, 4, ".dta") != 0) continue;
        std::string text;
        if (!buffers.read(file, text)) continue;
        const size_t hash = std::hash<std::string>{}(text);
        if (kept)
            if (const auto it = kept->old_forms.find(file);
                it != kept->old_forms.end() && it->second.hash == hash) {
                found.problems.insert(found.problems.end(), it->second.problems.begin(),
                                      it->second.problems.end());
                continue;
            }
        baker::tools::MigrateOptions options;
        options.file = baker::Symbol(file);
        const baker::tools::Migrated m =
            baker::tools::migrate_text(text, baker::TextSink{}, options);
        std::vector<Problem> problems;
        size_t line = 1, line_start = 0, at = 0;  // a finding's place, counted on from the last
        for (const baker::tools::MigrationFinding& f : m.findings) {
            for (; at < f.begin && at < text.size(); ++at)
                if (text[at] == '\n') {
                    ++line;
                    line_start = at + 1;
                }
            int column = 1;  // code points
            for (size_t i = line_start; i < f.begin && i < text.size(); ++i)
                if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) ++column;
            Problem p;
            p.file = file;
            p.line = static_cast<int>(line);
            p.column = column;
            p.severity = Severity::Warning;
            p.code = "migration";
            p.message = std::string(f.warning.message()) + " (baker_migrate --only " +
                        baker::tools::migration_name(f.migration) + " rewrites it)";
            problems.push_back(std::move(p));
        }
        found.problems.insert(found.problems.end(), problems.begin(), problems.end());
        if (kept) kept->old_forms[file] = {hash, std::move(problems)};
    }
}

}  // namespace

Problem problem_of(const baker::Error& e, Severity severity) {
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
    return p;
}

namespace {

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
        layers.push_back({.name = baker::Symbol(l.name),
                          .path = baker::Symbol(l.path),
                          .optional = l.optional,
                          .enabled = l.enabled});
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

// What a check's stack reads through, kept with it: the buffers of the check that reads it,
// the files read, and the warnings the last merge reported, which a reload that changes
// nothing does not report again.
struct KeptRead {
    const Buffers* buffers = nullptr;
    std::set<std::string> files;
    std::vector<Problem> warnings;
};

namespace {

// Every file is read through the editor's buffers.
baker::LoadSession::Reader reader_of(const std::shared_ptr<KeptRead>& read) {
    return [read](std::string_view path, baker::TextSink sink) {
        const std::string file = canonical_path(std::string(path));
        std::string text;
        if (!read->buffers->read(file, text)) return false;
        read->files.insert(file);
        sink(text);
        return true;
    };
}

// What the stack is opened from: two checks with the same key open the same layers the
// same way, so the second may reload the first's.
std::string key_of(const baker::Project& project, const Setup& setup) {
    std::string key;
    const auto add = [&key](std::string_view s) {
        key += std::to_string(s.size());
        key += ':';
        key += s;
    };
    for (const baker::ProjectLayer& l : project.layers) {
        add(l.name.str());
        add(l.path.str());
        key += l.optional ? 'o' : '-';
        key += l.enabled ? 'e' : '-';
        key += l.dev_only ? 'd' : '-';
        key += l.dir ? 'D' : '-';  // a directory of layers
        add(l.when.str());         // (when COND): which defines the layer waits on
    }
    for (const auto* entries : {&project.pack_levels, &project.include_dirs, &project.defines}) {
        key += '|';
        for (const baker::ProjectEntry& e : *entries) add(e.value.str());
    }
    key += '|';
    for (const std::string& name : setup.skip) add(name);
    return key;
}

}  // namespace

Diagnosis diagnose(const Setup& setup, const baker::Context& ctx, const Buffers& buffers,
                   bool undefined_fails, Kept* kept) {
    const auto start = std::chrono::steady_clock::now();
    Diagnosis out;
    Collector found;
    // The files this check reads besides the stack's: the project file, scenarios.
    const auto read_here = std::make_shared<KeptRead>();
    read_here->buffers = &buffers;
    const baker::LoadSession::Reader reader = reader_of(read_here);
    std::set<std::string>& files = read_here->files;

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
    auto loaded = std::make_shared<Loaded>();
    const std::string key = key_of(project, setup);

    // The kept stack, reloaded. A reload that does not read changes nothing, and the check
    // then loads afresh, so that the layers above the one that does not read are checked.
    std::shared_ptr<KeptRead> stack_read;
    if (kept && kept->config && kept->key == key) {
        KeptRead& k = *kept->read;
        k.buffers = &buffers;
        k.files.clear();
        std::vector<Problem> before = std::move(k.warnings);
        k.warnings.clear();
        const baker::Expected<bool> reloaded = kept->config->reload();
        if (reloaded && *reloaded) {
            loaded->config = kept->config;
        } else {
            k.warnings = std::move(before);
            if (reloaded) loaded->config = kept->config;
        }
        if (loaded->config) {
            stack_read = kept->read;
            out.every_layer_read = true;
        }
    }
    if (!loaded->config) {
        stack_read = std::make_shared<KeptRead>();
        stack_read->buffers = &buffers;
        loaded->config = std::make_shared<baker::Config>();
        baker::Config& cfg = *loaded->config;
        cfg.set_warning_handler([read = stack_read](const baker::Error& e) {
            read->warnings.push_back(problem_of(e, Severity::Warning));
        });
        cfg.session().set_reader(reader_of(stack_read));
        baker::ProjectOptions options{.go_on = true};
        for (const std::string& name : setup.skip) options.skip.add(name);  // too many: refused
        if (auto opened = baker::open_project(cfg, project, options); !opened) {
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
        if (kept && out.every_layer_read) {
            kept->key = key;
            kept->config = loaded->config;
            kept->read = stack_read;
        }
    }
    baker::Config& cfg = *loaded->config;
    files.insert(stack_read->files.begin(), stack_read->files.end());
    // What the merge warned of, and after it what the check adds through the same handler.
    const size_t merge_warnings = stack_read->warnings.size();

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

    // And every file of the project's scenarios and objectives, open or not, as the game's
    // own check reads them with the data.
    std::vector<baker::Ref<baker::Array>> extra;
    const std::set<std::string> layered = files;
    std::set<std::string> beside;
    for (const auto& [path, text] : buffers.all()) beside.insert(path);
    for (const auto* entries : {&project.scenarios, &project.objectives})
        for (const baker::ProjectEntry& e : *entries)
            for (const std::string& f : suite_files(std::string(e.value.str()), buffers))
                beside.insert(f);
    for (const std::string& path : beside) {
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
                if (d.stub) continue;
                // A method a declared type of the game's lacks; one on a type the reference
                // does not describe cannot be told from a typo, and is left alone.
                if (!d.on.str().empty()) {
                    if (std::find(setup.types.begin(), setup.types.end(), d.on.str()) ==
                        setup.types.end())
                        continue;
                    for (const baker::CheckReport::Site& site : d.sites) {
                        Problem p;
                        p.file = canonical_path(std::string(site.file.str()));
                        p.line = site.line;
                        p.code = "undefined";
                        p.key = std::string(d.name.str());
                        p.message = "'" + std::string(d.on.str()) + "' has no method '" +
                                    std::string(d.name.str()) + "' in the game's reference";
                        found.problems.push_back(std::move(p));
                    }
                    continue;
                }
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
        shapes.add(cfg.read_session(), shaped);  // every layer's #shape declarations
        baker::shape::check(*cfg.root(), shapes, shaped);
        found.add(shaped.errors, Severity::Error);
        // A field no shape declares: reported, never a failure.
        found.add(shaped.warnings, Severity::Information);
    }
    for (const Problem& p : stack_read->warnings) found.problems.push_back(p);
    stack_read->warnings.resize(merge_warnings);
    for (const baker::Pack& p : cfg.read_session().packs()) {
        // A pack is declared by its file's #pack (NAME ...): the line of the directive.
        Definition d{std::string(p.name.str()), canonical_path(std::string(p.file.str())), 1};
        std::string text;
        if (buffers.read(d.file, text))
            if (const size_t at = text.find("#pack"); at != std::string::npos)
                d.line =
                    1 + static_cast<int>(std::count(
                            text.begin(), text.begin() + static_cast<std::ptrdiff_t>(at), '\n'));
        loaded->packs.push_back(std::move(d));
    }
    for (const baker::ProjectEntry& e : project.include_dirs)
        loaded->include_dirs.push_back(canonical_path(std::string(e.value.str())));
    // The data's own vocabulary: the macros that stand (not a flag for #ifdef, not one
    // #undef took back), and the {func}s its #autorun commands define.
    const baker::LoadSession& read = cfg.read_session();
    for (const baker::Definition& d : read.macro_definitions())
        if (!d.flag && read.is_defined(d.name))
            loaded->macros.push_back(
                {std::string(d.name.str()), canonical_path(std::string(d.file.str())), d.line});
    const baker::List<baker::Node> autorun = cfg.autorun();
    for (const baker::Definition& d : baker::tools::script_functions(autorun.items()))
        loaded->functions.push_back(
            {std::string(d.name.str()), canonical_path(std::string(d.file.str())), d.line});
    find_hints(cfg, project, *loaded, found);
    find_conditions(cfg, project, *loaded);
    for (const LayerHint& h : loaded->hints)
        if (!loaded->texts.count(h.file)) buffers.read(h.file, loaded->texts[h.file]);
    loaded->files.assign(files.begin(), files.end());
    out.loaded = std::move(loaded);
    find_old_forms(files, buffers, kept, found);
    return finish(out, found, files, start);
}

bool under(const std::string& path, const Setup& setup) {
    if (setup.project.empty()) return true;
    const size_t slash = setup.project.rfind('/');
    const std::string_view root = std::string_view(setup.project).substr(0, slash + 1);
    return std::string_view(path).substr(0, root.size()) == root;
}

}  // namespace baker_lsp
