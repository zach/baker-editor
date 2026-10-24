#include "format.hpp"

#include <doctest.h>

#include <string>

using namespace baker_lsp;

namespace {

// `text` with the edits made, last first so that each one's offsets still hold.
std::string edited(std::string text, const std::vector<TextEdit>& edits) {
    for (auto e = edits.rbegin(); e != edits.rend(); ++e)
        text.replace(e->begin, e->end - e->begin, e->text);
    return text;
}

std::string laid_out(const std::string& text, int indent = 2) {
    std::string error;
    const std::string out = edited(text, format_edits(text, {.indent = indent}, error));
    CHECK(error.empty());
    return out;
}

}  // namespace

TEST_CASE("formatting lays text out as Baker writes it, keeping its comments") {
    const std::string text =
        "; units\n(units   (knight (health 100)  ; tough\n)\n\n\n(archer (health 60)))\n";
    const std::string once = laid_out(text);
    CHECK(once.find("; units") == 0);
    CHECK(once.find("; tough") != std::string::npos);
    CHECK(once.find("\n\n\n") == std::string::npos);  // one blank line where any was written
    CHECK(once.find("(units   ") == std::string::npos);
    // Formatting its own output changes nothing.
    std::string error;
    CHECK(format_edits(once, {}, error).empty());
    // The project's indent.
    CHECK(laid_out("(a\n(b 1) ; x\n)\n", 3).find("\n   (b 1)") != std::string::npos);
}

TEST_CASE("text that does not read is left as it is") {
    std::string error;
    CHECK(format_edits("(units (knight (health 100)\n", {}, error).empty());
    CHECK(!error.empty());
}

TEST_CASE("a range lays out only the forms it touches") {
    const std::string text = "(a   1)\n(b   2)\n";
    std::string error;
    const std::string out = edited(text, format_edits(text, {}, error, 9, 10));  // in (b
    CHECK(out == "(a   1)\n(b 2)\n");
}

TEST_CASE("a CRLF text keeps its line ends") {
    const std::string out = laid_out("(a\r\n(b 1) ; c\r\n)\r\n");
    CHECK(out.find("\r\n") != std::string::npos);
    CHECK(out.find("\n") == out.find("\r\n") + 1);
}
