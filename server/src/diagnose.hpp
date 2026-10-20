#pragma once
// One check of the whole project: read the project file, open it as the game would (Baker's
// open_project), with open editors' text in place of the files on disk, then run Baker's
// check and the shapes. A layer that does not read does not stop the load (go_on): the
// layers above it are read too, so the editor shows every file's errors. An open .dta
// file the layers do not read (a scenario, an objective) is read on its own and checked
// beside them, as the game's `check` command checks its scenarios.
#include "setup.hpp"

#include <baker/config.hpp>
#include <baker/script.hpp>
#include <baker/shape.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace baker_lsp {

enum class Severity { Error = 1, Warning = 2, Information = 3, Hint = 4 };  // LSP's values

struct Problem {
    std::string file;  // canonical; empty when Baker did not say
    int line = 0;      // Baker's: 1-based, 0 when unknown
    int column = 0;    // Baker's: 1-based code points, 0 when unknown
    Severity severity = Severity::Error;
    std::string code;  // the ErrorCode's name, or "undefined" for a call nothing defines
    std::string message;
    std::string key;           // what to underline on the line when the column is unknown
    bool unnecessary = false;  // LSP's Unnecessary tag: shown faded (a shadowed value)

    friend bool operator<(const Problem& a, const Problem& b);
    friend bool operator==(const Problem& a, const Problem& b) = default;
};

// The text of files open in the editor, by canonical path.
class Buffers {
public:
    void set(const std::string& path, std::string text) { text_[path] = std::move(text); }
    void erase(const std::string& path) { text_.erase(path); }
    // The editor's text, or null if the file is not open.
    [[nodiscard]] const std::string* find(const std::string& path) const;
    // The editor's text, else the file's; false if neither.
    bool read(const std::string& path, std::string& out) const;
    [[nodiscard]] const std::map<std::string, std::string>& all() const { return text_; }

private:
    std::map<std::string, std::string> text_;
};

// A name defined in the data, and where: a macro, or a script function ({func name ...}).
struct Definition {
    std::string name;
    std::string file;  // canonical
    int line = 0;      // 1-based
};

// A value one layer sets that another layer's value meets: shown at the end of its line.
struct LayerHint {
    std::string file;       // canonical: where the value is written
    int line = 0;           // 1-based
    std::string path;       // dotted, as Config::layer_effect gives it
    std::string key;        // the path's last element, unquoted: the array's tag
    std::string label;      // "← local.dta:3 (1200)" or "over default.dta:18 (1000)"
    bool shadowed = false;  // a higher layer overrides it
    std::string value;      // as Baker text, the value written here

    friend bool operator==(const LayerHint&, const LayerHint&) = default;
};

// What a check loaded, kept for hover and completion until the next check.
struct Loaded {
    std::unique_ptr<baker::Config> config;  // the stack as the game merges it
    baker::shape::Set shapes;
    std::vector<Definition> packs;          // the packs the files declare
    std::vector<std::string> include_dirs;  // the project's, for files #include names
    std::vector<Definition> macros;         // LoadSession::macro_definitions that stand
    std::vector<Definition> functions;      // the {func}s the #autorun commands define
    std::vector<std::string> files;         // every file the load read, canonical
    // Each overridden or overriding value, from the layers above the bottom one
    // (Config::layer_effect): their entries name the value below and where it is.
    std::vector<LayerHint> hints;
};

struct Diagnosis {
    std::vector<Problem> problems;   // sorted, without duplicates
    std::vector<std::string> files;  // every file the load read, canonical
    double milliseconds = 0;
    std::shared_ptr<Loaded> loaded;  // null if the project file did not read
    bool every_layer_read = false;   // no layer failed: `loaded` is the whole stack
};

// `ctx` holds the game's functions (reference.hpp); `undefined_fails`: a call to a function
// nothing defines is an error (with a reference), else not reported.
Diagnosis diagnose(const Setup& setup, const baker::Context& ctx, const Buffers& buffers,
                   bool undefined_fails);

// Whether an open file belongs to the project: under the project file's directory. Every
// file does without a project file.
bool under(const std::string& path, const Setup& setup);

}  // namespace baker_lsp
