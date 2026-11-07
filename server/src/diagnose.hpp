#pragma once
// One check of the whole project: read the project file, open it as the game would (Baker's
// open_project), with open editors' text in place of the files on disk, then run Baker's
// check and the shapes. A layer that does not read does not stop the load (go_on): the
// layers above it are read too, so the editor shows every file's errors. An open .dta
// file the layers do not read (a scenario, an objective) is read on its own and checked
// beside them, as the game's `check` command checks its scenarios. Given a Kept, a check
// keeps the stack's Config for the next, which reloads it (Config::reload: only the layers
// whose text changed are read again) while the project's layers and setup stay the same.
#include "setup.hpp"

#include <baker/config.hpp>
#include <baker/script.hpp>
#include <baker/shape.hpp>

#include <algorithm>
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
    // Replaces `count` bytes at `at` of an open file's text; nothing if it is not open.
    void replace(const std::string& path, size_t at, size_t count, std::string_view with) {
        if (const auto it = text_.find(path); it != text_.end())
            it->second.replace(std::min(at, it->second.size()), count, with);
    }
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

// A value a layer above the bottom one sets, or a row that composes the list below it, (items
// #append rope) or (things #drop lamp), from Config::layer_effect: what it changes, and what
// it makes.
struct LayerRow {
    std::string path;   // as a Path's text: households/cottage/things
    std::string layer;  // the layer's name
    std::string op;     // #append, #drop or #remove; empty for a value
    // The value, or a verb's operands, as shown: as Baker text, a record by its key, (jab2 ...)
    // as jab2, so that a list of records reads as its keys.
    std::string operands;
    // The same and the list below, a value each, as Baker text: (3 4) is 3 and 4.
    std::vector<std::string> values, below_values;
    std::string file;   // canonical: where the row is written
    int line = 0;       // 1-based
    std::string below;  // the list below, as shown; empty if the layers below lack it
    std::string below_file;
    int below_line = 0;
    // The list a verb makes, if no layer above changes it again (the stack's value); else
    // empty. Empty for a value too. As shown, and a value each as Baker text.
    std::string result;
    std::vector<std::string> result_values;
    bool stale = false;  // its (@was ...) pin is not the value below

    [[nodiscard]] bool composes() const { return op == "#append" || op == "#drop"; }
    friend bool operator==(const LayerRow&, const LayerRow&) = default;
};

// A row's values as the editor shows them, each as Baker text: one is itself, several a list
// in parentheses, (3 4); none is ().
std::string shown(const baker::RowValues& values);

// What a check loaded, kept for hover and completion until the next check.
struct Loaded {
    std::shared_ptr<baker::Config> config;  // the stack as the game merges it
    baker::shape::Set shapes;
    std::vector<Definition> packs;          // the packs the files declare
    std::vector<std::string> include_dirs;  // the project's, for files #include names
    std::vector<Definition> macros;         // LoadSession::macro_definitions that stand
    std::vector<Definition> functions;      // the {func}s the #autorun commands define
    std::vector<std::string> files;         // every file the load read, canonical
    // Each overridden or overriding value, from the layers above the bottom one
    // (Config::layer_effect): their entries name the value below and where it is.
    std::vector<LayerHint> hints;
    std::vector<Problem> shadowed;  // the hints' faded values, as problems (Hint, Unnecessary)
    std::vector<LayerRow> rows;     // the values and verbs of the layers above the bottom one
    // Each layer of the project, bottom to top: whether the merge has it now
    // (Config::layer_enabled), and the condition it waits on, (when HANDHELD), as written.
    struct LayerState {
        std::string name;
        std::string when;  // empty: no condition
        bool in = true;    // in the merge: enabled, and its condition holding
    };
    std::vector<LayerState> layers;
    // The defines a condition may name: each that stands, with who made it ("host",
    // "project" or "file"), and each an #ifdef tests or a layer's condition names that does
    // not ("not defined").
    std::vector<std::pair<std::string, std::string>> defines;
    // Each hinted file's text as this check read it, so that a later check that does not read
    // can carry the hints to where their lines are now (LineMap).
    std::map<std::string, std::string> texts;
};

struct Diagnosis {
    std::vector<Problem> problems;   // sorted, without duplicates
    std::vector<std::string> files;  // every file the load read, canonical
    double milliseconds = 0;
    std::shared_ptr<Loaded> loaded;  // null if the project file did not read
    bool every_layer_read = false;   // no layer failed: `loaded` is the whole stack
};

struct KeptRead;

// One project's stack, kept from a check to the next. It is the last stack every layer of
// read: while a layer does not, each check loads afresh, and the kept stack is reloaded
// again once it does.
struct Kept {
    std::string key;                        // the project's layers and setup, as text
    std::shared_ptr<baker::Config> config;  // null until a check keeps one
    std::shared_ptr<KeptRead> read;         // its reader's state, and its merge's warnings
    // Each file's old forms (tools::migrate_text), by a hash of the text they were found in:
    // a file found again only once its text changes.
    struct OldForms {
        size_t hash = 0;
        std::vector<Problem> problems;
    };
    std::map<std::string, OldForms> old_forms;
};

// `ctx` holds the game's functions (reference.hpp); `undefined_fails`: a call to a function
// nothing defines is an error (with a reference), else not reported. With `kept`, the
// project's stack is reloaded from it, and kept there; the result is a fresh load's.
Diagnosis diagnose(const Setup& setup, const baker::Context& ctx, const Buffers& buffers,
                   bool undefined_fails, Kept* kept = nullptr);

// Whether an open file belongs to the project: under the project file's directory. Every
// file does without a project file.
bool under(const std::string& path, const Setup& setup);

// A Baker error as a problem: located as Baker locates it, keyed by its path's last element.
Problem problem_of(const baker::Error& e, Severity severity);

}  // namespace baker_lsp
