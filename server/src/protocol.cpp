#include "protocol.hpp"

#include <cstdlib>

namespace baker_lsp {

bool read_message(std::FILE* in, std::string& body) {
    size_t length = 0;
    bool have_length = false;
    std::string line;
    for (;;) {
        line.clear();
        int c;
        while ((c = std::fgetc(in)) != EOF && c != '\n') line += static_cast<char>(c);
        if (c == EOF) return false;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;  // the end of the header
        constexpr std::string_view kLength = "Content-Length:";
        if (line.size() > kLength.size() && line.compare(0, kLength.size(), kLength) == 0) {
            length = std::strtoull(line.c_str() + kLength.size(), nullptr, 10);
            have_length = true;
        }
    }
    if (!have_length) return false;
    body.resize(length);
    return std::fread(body.data(), 1, length, in) == length;
}

void write_message(std::FILE* out, std::string_view body) {
    std::fprintf(out, "Content-Length: %zu\r\n\r\n", body.size());
    std::fwrite(body.data(), 1, body.size(), out);
    std::fflush(out);
}

}  // namespace baker_lsp
