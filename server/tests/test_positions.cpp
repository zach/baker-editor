#include "positions.hpp"

#include <doctest.h>

using namespace baker_lsp;

TEST_CASE("line_text finds lines and drops their breaks") {
    const std::string_view text = "one\r\ntwo\nthree";
    CHECK(line_text(text, 1) == "one");
    CHECK(line_text(text, 2) == "two");
    CHECK(line_text(text, 3) == "three");
    CHECK(line_text(text, 4).empty());
    CHECK(line_text(text, 0).empty());
}

TEST_CASE("columns in code points become UTF-16 units") {
    CHECK(utf16_units("abc", 2) == 2);
    CHECK(utf16_units("\xC3\xA9t\xC3\xA9", 2) == 2);  // été: two-byte characters, one unit
    CHECK(utf16_units("\xF0\x9F\x8E\xB2x", 1) == 2);  // a die: four bytes, a surrogate pair
    CHECK(utf16_units("ab", 10) == 2);
}

TEST_CASE("a known column underlines the token there") {
    const Range r = problem_range("x\n  (speed fast)\n", 2, 10);
    CHECK(r.start.line == 1);
    CHECK(r.start.character == 9);
    CHECK(r.end.character == 13);
}

TEST_CASE("a column after a character outside the BMP counts its two units") {
    // ( n a m e ␠ " 🎲 " ␠ o: `oops` is the 11th code point and starts at unit 11.
    const Range r = problem_range("(name \"\xF0\x9F\x8E\xB2\" oops)", 1, 11);
    CHECK(r.start.character == 11);
    CHECK(r.end.character == 15);
}

TEST_CASE("an unknown column underlines the named array, else the first token") {
    const std::string_view text = "   (health 5) (armor 3)";
    Range r = problem_range(text, 1, 0, "armor");
    CHECK(r.start.character == 14);
    CHECK(r.end.character == 20);  // (armor
    r = problem_range(text, 1, 0, "missing");
    CHECK(r.start.character == 3);
    CHECK(r.end.character == 10);  // (health
    r = problem_range("{- 1 2 3}", 1, 0, "-");
    CHECK(r.start.character == 0);
    CHECK(r.end.character == 2);
}

TEST_CASE("no line underlines the start of the file") {
    const Range r = problem_range("abc", 0, 0);
    CHECK(r.start.line == 0);
    CHECK(r.end.character == 1);
}

TEST_CASE("the last element of an error's path") {
    CHECK(last_path_element("fighters/kestrel/armor") == "armor");
    CHECK(last_path_element("menu/'bacon and eggs'") == "bacon and eggs");
    CHECK(last_path_element("a/'x/y'") == "x/y");
    CHECK(last_path_element("") == "");
}

TEST_CASE("file URIs and paths") {
    CHECK(path_from_uri("file:///a/b%20c.dta") == "/a/b c.dta");
    CHECK(path_from_uri("file:///C:/data/x.dta") == "C:/data/x.dta");
    CHECK(path_from_uri("untitled:1").empty());
    CHECK(uri_from_path("/a/b c.dta") == "file:///a/b%20c.dta");
    CHECK(uri_from_path("C:/data/x.dta") == "file:///C:/data/x.dta");
    CHECK(path_from_uri(uri_from_path("/s/\xC3\xA9t\xC3\xA9.dta")) == "/s/\xC3\xA9t\xC3\xA9.dta");
}
