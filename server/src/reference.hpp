#pragma once
// The game's vocabulary, from its reference JSON (baker.reference/1.1): the server has no
// game, so it declares each native function from its description (Context::declare), for
// check to count and type a call's arguments against, and learns the objects the game names.
#include <baker/script.hpp>

#include <string>
#include <vector>

namespace baker_lsp {

// A function of the game's, for hover and completion.
struct FunctionInfo {
    std::string name;
    std::vector<std::string> signatures;  // one per overload: {name symbol [int] values...}
    std::string doc;
    std::string status;  // normal, stub, provisional or deprecated
    std::string tier;    // native or script
};

struct Reference {
    int functions = 0;  // declared
    int overloads = 0;
    // Functions with a parameter of a type Context::declare does not know, one of the game's
    // own: declared with it taking any value.
    int widened = 0;
    std::vector<std::string> failed;   // functions that could not be declared at all
    std::vector<std::string> objects;  // the objects the game names, such as clock
    std::vector<FunctionInfo> all;     // every function the reference lists, Baker's too
};

// Reads the reference at `path` and declares its native functions in `ctx`, skipping names
// `ctx` already has (Baker's own). A function keeps its status: a stub stays a stub. False
// with `error` if the file is missing or not a reference.
bool load_reference(baker::Context& ctx, const std::string& path, Reference& out,
                    std::string& error);

}  // namespace baker_lsp
