#pragma once
// The game's native functions, from its reference JSON (baker.reference/1): the server has
// no game, so each function is registered as a stand-in with the same number of arguments,
// for check to count a call's arguments against.
#include <baker/script.hpp>

#include <string>

namespace baker_lsp {

struct StubCount {
    int functions = 0;  // registered
    int overloads = 0;
    int approximate = 0;  // overloads with more parameters than a stand-in takes: any count
};

// Reads the reference at `path` and registers its native functions in `ctx`, skipping names
// `ctx` already has (Baker's own). A function with status stub stays a stub. False with
// `error` if the file is missing or not a reference.
bool register_stubs(baker::Context& ctx, const std::string& path, StubCount& count,
                    std::string& error);

}  // namespace baker_lsp
