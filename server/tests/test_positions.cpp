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

TEST_CASE("a line map finds each kept line in the later text") {
    const std::string before = "a\nb\nc\nd\ne\n";
    // Two lines put in after the first, and the fourth edited.
    const LineMap map(before, "a\nx\ny\nb\nc\nD\ne\n");
    CHECK(map.line(1) == 1);
    CHECK(map.line(2) == 4);
    CHECK(map.line(3) == 5);
    CHECK(!map.line(4));  // edited: no place
    CHECK(map.line(5) == 7);
    CHECK(!map.line(9));
    // A line removed, and a repeated line matched in order.
    const LineMap removed("x\n)\ny\n)\n", "x\ny\n)\n");
    CHECK(map.line(1) == 1);
    CHECK(removed.line(1) == 1);
    CHECK(!removed.line(2));
    CHECK(removed.line(3) == 2);
    CHECK(removed.line(4) == 3);
    CHECK(LineMap(before, before).line(3) == 3);
}

TEST_CASE("a line map keeps the lines between two edits far apart") {
    std::string before;
    for (int i = 0; i < 20000; ++i) before += "(line " + std::to_string(i) + ")\n";
    std::string after = before;
    after.insert(after.find("(line 5000)"), "; one\n; two\n");  // two lines in, at line 5001
    after += "(open\n";                                         // and one left open at the end
    const LineMap map(before, after);
    CHECK(map.line(5000) == 5000);
    CHECK(map.line(5001) == 5003);
    CHECK(map.line(15000) == 15002);
    CHECK(map.line(20000) == 20002);
    // Past kMaxEdits lines of edits between them, the lines between have no place.
    std::string rewritten = before;
    for (size_t i = 0; i < 1200; ++i) rewritten.insert(rewritten.find("(line 3000)"), "x\n");
    rewritten += "(open\n";
    const LineMap far(before, rewritten);
    CHECK(far.line(10) == 10);
    CHECK(!far.line(15000));
}
