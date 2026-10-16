#pragma once
// One check of the whole project: read the project file, open it as the game would (Baker's
// open_project), with open editors' text in place of the files on disk, then run Baker's
// check and the shapes. A layer that does not read does not stop the load (go_on): the
// layers above it are read too, so the editor shows every file's errors. An open .dta
// file the layers do not read (a scenario, an objective) is read on its own and checked
// beside them, as the game's `check` command checks its scenarios.
#include "setup.hpp"

#include <baker/script.hpp>

#include <map>
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
    std::string key;  // what to underline on the line when the column is unknown

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

struct Diagnosis {
    std::vector<Problem> problems;   // sorted, without duplicates
    std::vector<std::string> files;  // every file the load read, canonical
    double milliseconds = 0;
};

// `ctx` holds the game's functions (reference.hpp); `undefined_fails`: a call to a function
// nothing defines is an error (with a reference), else not reported.
Diagnosis diagnose(const Setup& setup, const baker::Context& ctx, const Buffers& buffers,
                   bool undefined_fails);

// Whether an open file belongs to the project: under the project file's directory. Every
// file does without a project file.
bool under(const std::string& path, const Setup& setup);

}  // namespace baker_lsp
