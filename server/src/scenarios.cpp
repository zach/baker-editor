#include "scenarios.hpp"

#include "setup.hpp"
#include "structure.hpp"

#include <baker/project.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace baker_lsp {
namespace {}  // namespace

std::vector<std::string> suite_files(const std::string& path, const Buffers& buffers) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(path, ec)) return {canonical_path(path)};
    std::vector<std::string> out;
    for (const auto& e : fs::directory_iterator(path, ec))
        if (e.path().extension() == ".dta") out.push_back(canonical_path(e.path().string()));
    // An open file not yet saved there.
    const std::string dir = canonical_path(path) + "/";
    for (const auto& [open, text] : buffers.all())
        if (open.starts_with(dir) && open.find('/', dir.size()) == std::string::npos &&
            open.ends_with(".dta") && std::find(out.begin(), out.end(), open) == out.end())
            out.push_back(open);
    std::sort(out.begin(), out.end());
    return out;
}

namespace {

std::string first_value(const Structure& s, int bracket) {
    const Bracket& b = s.brackets[bracket];
    const size_t at = static_cast<size_t>(b.tag) + 1;
    if (b.tag < 0 || at >= s.tokens.size() || s.tokens[at].bracket != bracket) return {};
    return std::string(s.tokens[at].text);
}

// The top-level `(tag NAME ...)` forms of `file`.
void read_forms(const std::string& file, std::string_view tag, const Buffers& buffers,
                std::vector<Declared>& out) {
    std::string text;
    if (!buffers.read(file, text)) return;
    const Structure s = read_structure(text);
    const LineIndex lines(text);
    for (size_t b = 0; b < s.brackets.size(); ++b) {
        const Bracket& br = s.brackets[b];
        if (br.parent >= 0 || br.open != '(' || s.tag(static_cast<int>(b)) != tag) continue;
        Declared d;
        d.name = first_value(s, static_cast<int>(b));
        if (d.name.empty()) continue;
        d.file = file;
        d.line = lines.position(br.begin).first + 1;
        const size_t name = s.tokens[static_cast<size_t>(br.tag) + 1].begin;
        size_t start = text.rfind('\n', name);
        start = start == std::string::npos ? 0 : start + 1;
        for (size_t i = start; i < name; ++i)  // code points: every byte but a continuation
            d.column += (static_cast<unsigned char>(text[i]) & 0xC0) != 0x80;
        ++d.column;
        for (size_t c = b + 1; c < s.brackets.size() && s.brackets[c].begin < br.end; ++c) {
            if (s.brackets[c].parent != static_cast<int>(b)) continue;
            const std::string_view child = s.tag(static_cast<int>(c));
            if (child == "desc") {
                d.desc = first_value(s, static_cast<int>(c));
            } else if (child == "over") {
                d.median = first_value(s, static_cast<int>(c)) == "median";
            } else if (child == "objectives") {
                for (const Token& t : s.tokens)
                    if (t.bracket == static_cast<int>(c) && t.index > 0)
                        d.objectives.emplace_back(t.text);
            }
        }
        out.push_back(std::move(d));
    }
}

const Declared* named(const std::vector<Declared>& list, const std::string& name) {
    for (const Declared& d : list)
        if (d.name == name) return &d;
    return nullptr;
}

std::string number(const nlohmann::json& v) {
    if (!v.is_number()) return "no number";
    char b[32];
    std::snprintf(b, sizeof b, "%g", v.get<double>());
    return b;
}

// A problem at `file` and `line`; on the name of `declared`, if that is where it is.
Problem at(const std::string& file, int line, const std::string& key, std::string message,
           const Declared* declared = nullptr) {
    Problem p;
    p.file = canonical_path(file);
    p.line = line;
    if (declared && declared->file == p.file && declared->line == line) p.column = declared->column;
    p.severity = Severity::Error;
    p.code = "sim";
    p.key = key;
    p.message = std::move(message);
    return p;
}

}  // namespace

bool read_suite(const std::string& project, const Buffers& buffers, Suite& out,
                std::string& error) {
    const baker::LoadSession::Reader reader = [&buffers](std::string_view path,
                                                         baker::TextSink sink) {
        std::string text;
        if (!buffers.read(canonical_path(std::string(path)), text)) return false;
        sink(text);
        return true;
    };
    auto read = baker::read_project(project, reader);
    if (!read) {
        error = std::string(read.error().message());
        return false;
    }
    out.project = canonical_path(project);
    // `$build` resolved: the project's (build ...), else "build" beside the project file.
    const baker::Symbol tool = read->tool_path();
    out.tool = tool.str().empty() ? std::string() : canonical_path(std::string(tool.str()));
    for (const baker::ProjectEntry& e : read->scenarios)
        for (const std::string& f : suite_files(std::string(e.value.str()), buffers))
            read_forms(f, "scenario", buffers, out.scenarios);
    for (const baker::ProjectEntry& e : read->objectives)
        for (const std::string& f : suite_files(std::string(e.value.str()), buffers))
            read_forms(f, "objective", buffers, out.objectives);
    return true;
}

std::vector<Problem> sim_problems(const nlohmann::json& results, const Suite& suite) {
    std::vector<Problem> out;
    const auto failed = [&](const std::string& scenario, const nlohmann::json& o,
                            const std::string& how) {
        const std::string name = o.value("name", o.value("objective", std::string()));
        const nlohmann::json error = o.value("error", nlohmann::json());
        std::string why = error.is_object() ? error.value("message", std::string())
                                            : "value " + number(o.value("value", nlohmann::json()));
        const Declared* d = named(suite.objectives, name);
        std::string message = name + " failed in scenario " + scenario + how + ": " + why;
        if (d && !d->desc.empty()) message += " (" + d->desc + ")";
        if (!o.value("file", std::string()).empty())
            out.push_back(at(o.value("file", std::string()), o.value("line", 0), name, message, d));
        if (const Declared* s = named(suite.scenarios, scenario))
            out.push_back(at(s->file, s->line, scenario, message, s));
    };
    for (const nlohmann::json& run : results.value("runs", nlohmann::json::array())) {
        const std::string scenario = run.value("scenario", std::string());
        const std::string seed = " (seed " + std::to_string(run.value("seed", 0)) + ")";
        for (const nlohmann::json& e : run.value("errors", nlohmann::json::array()))
            if (!e.value("file", std::string()).empty())
                out.push_back(at(
                    e.value("file", std::string()), e.value("line", 0), std::string(),
                    "in scenario " + scenario + seed + ": " + e.value("message", std::string())));
        for (const nlohmann::json& o : run.value("objectives", nlohmann::json::array()))
            if (o.value("over", "seed") == "seed" && !o.value("pass", false))
                failed(scenario, o, seed);
    }
    for (const nlohmann::json& m : results.value("medians", nlohmann::json::array()))
        if (!m.value("pass", false))
            failed(m.value("scenario", std::string()), m,
                   " (median over " + std::to_string(m.value("seeds", 0)) + " seeds)");
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

}  // namespace baker_lsp
