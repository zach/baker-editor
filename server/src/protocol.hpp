#pragma once
// LSP's base protocol: messages framed by a Content-Length header.
#include <cstdio>
#include <string>
#include <string_view>

namespace baker_lsp {

// Reads one message's body from `in`; false at the end of input or on a malformed header.
bool read_message(std::FILE* in, std::string& body);
void write_message(std::FILE* out, std::string_view body);

}  // namespace baker_lsp
