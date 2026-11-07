#include "infix.hpp"

#include <doctest.h>

#include <string>

using namespace baker_lsp;

namespace {

// The fixes at the '|' (taken out), each as the text it leaves.
std::vector<std::string> fixed(std::string text) {
    const size_t bar = text.find('|');
    text.erase(bar, 1);
    std::vector<std::string> out;
    for (const BraceEdit& e : infix_fixes(read_structure(text), text, bar)) {
        std::string t = text;
        t.replace(e.edit.begin, e.edit.end - e.edit.begin, e.edit.text);
        out.push_back(e.title + ": " + t);
    }
    return out;
}

}  // namespace

TEST_CASE("a brace missing its operator: put between every two operands") {
    CHECK(fixed("(pass {0.3 <= $va|lue 0.7})") ==
          std::vector<std::string>{"Insert the missing '<=': (pass {0.3 <= $value <= 0.7})"});
    CHECK(fixed("(pass {|$v <= 1 2 3})") ==
          std::vector<std::string>{"Insert the missing '<=': (pass {$v <= 1 <= 2 <= 3})"});
    CHECK(fixed("(x {{$a + $b} {$c|}})").empty());  // no operator at all: no infix to mend
}

TEST_CASE("mixed comparisons: each pair its own brace, under and") {
    CHECK(fixed("(pass {0 < $va|lue <= 10})") ==
          std::vector<std::string>{"Nest under and: (pass {and {0 < $value} {$value <= 10}})"});
    CHECK(fixed("(x {1 + 2 |* 3})").empty());  // arithmetic: no precedence to assume
    CHECK(fixed("(pass {0 < $va|lue <= 10 ; why\n})").empty());  // a comment would be lost
}

TEST_CASE("a brace that reads, as infix or as a call, has nothing to fix") {
    CHECK(fixed("(pass {2 <= $va|lue <= 10})").empty());
    CHECK(fixed("(pass {<= 0.3 $va|lue 0.7})").empty());
    CHECK(fixed("(r {wheat -|> flour})").empty());
    CHECK(fixed("(pass {$value <|=})").empty());  // ends with its operator: the person's to finish
}

namespace {
std::vector<std::string> respelled(std::string text) {
    const size_t bar = text.find('|');
    text.erase(bar, 1);
    std::vector<std::string> out;
    for (const BraceEdit& e : infix_spellings(read_structure(text), text, bar)) {
        std::string t = text;
        t.replace(e.edit.begin, e.edit.end - e.edit.begin, e.edit.text);
        out.push_back(e.title + ": " + t);
    }
    return out;
}
}  // namespace

TEST_CASE("a call written both ways: infix to prefix and back, only where Baker reads it so") {
    CHECK(respelled("(pass {2 <= $va|lue <= 10})") ==
          std::vector<std::string>{"Write prefix: (pass {<= 2 $value 10})"});
    CHECK(respelled("(pass {<= 2 $va|lue 10})") ==
          std::vector<std::string>{"Write infix: (pass {2 <= $value <= 10})"});
    CHECK(respelled("(x {{$a + $b} |/ 2})") ==
          std::vector<std::string>{"Write prefix: (x {/ {$a + $b} 2})"});
    CHECK(respelled("(x {- |5})").empty());             // one operand: no infix spelling
    CHECK(respelled("(x {+ a| b})").empty());           // bare symbols would read as a call of a
    CHECK(respelled("(x {game.wait |1})").empty());     // no operator
    CHECK(respelled("(x {<= 1 |2 ; why\n})").empty());  // a comment would be lost
}
