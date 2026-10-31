#include "rewrite.hpp"

#include <doctest.h>

#include <string>

using namespace baker_lsp;

namespace {

// The text after `make` at the '|' (taken out), checked by Baker against the text before it;
// empty if `make` offers nothing.
template <typename Make>
std::string rewritten(std::string text, Make make, bool same = true) {
    const size_t bar = text.find('|');
    text.erase(bar, 1);
    auto r = make(read_structure(text), text, bar);
    if (!r) return {};
    const std::string out = apply(text, *r, baker::WriteOptions{});
    std::string why;
    CHECK_MESSAGE(same_tree(text, out, "test.dta", {}, why) == same, why);
    return out;
}

const auto row = [](const Structure& s, std::string_view t, size_t at) { return as_row(s, t, at); };
const auto nested = [](const Structure& s, std::string_view t, size_t at) {
    return as_nested(s, t, at);
};

}  // namespace

TEST_CASE("write as a row, and nested again: the same tree both ways") {
    const std::string start =
        "(units\n   (knight (health 1|00) (speed 2.5))\n   (archer (health 60)))\n";
    const std::string as_rows = rewritten(start, row);
    CHECK(
        as_rows ==
        "(units\n   (knight (speed 2.5))\n   (archer (health 60)))\n(/units.knight.health 100)\n");
    // Back into the form that writes units/knight, after its last child, the row's line gone.
    std::string back = as_rows;
    back.insert(back.find("health 100"), "|");
    CHECK(rewritten(back, nested) ==
          "(units\n   (knight (speed 2.5) (health 100))\n   (archer (health 60)))\n");
}

TEST_CASE("write as a row: metadata goes with the value; what cannot be a row is not offered") {
    CHECK(rewritten("(x (y 1 (@range 0 9|)))", row) == "(x)\n(/x.y 1 (@range 0 9))\n");
    CHECK(rewritten("(x (y '1.5' (z 1|)))", row).empty() == false);  // a quoted value is fine
    CHECK(rewritten("(x 1|)", row).empty());                         // at the top level
    CHECK(rewritten("(menu ('bacon and eggs' (price 3|)))", row).empty());  // a space in a key
    CHECK(rewritten("(x (y 1 ; why\n 2|))", row).empty());  // a comment in the value
    CHECK(rewritten("(x (y (z 1|) (w 2)))", row) == "(x (y (w 2)))\n(/x.y.z 1)\n");
    // A sibling after it on its line keeps the line where it is.
    CHECK(rewritten("(x\n   (y\n      (z 1|) (w 2)\n      (v 3)))", row) ==
          "(x\n   (y\n      (w 2)\n      (v 3)))\n(/x.y.z 1)\n");
    CHECK(rewritten("(x (y (z 1) (|w 2)))", row) == "(x (y (z 1)))\n(/x.y.w 2)\n");
    // A segment that must be quoted in a key.
    CHECK(rewritten("(x ('a.b' (z 1|)))", row) == "(x ('a.b'))\n(/x.'a.b'.z 1)\n");
}

TEST_CASE("write nested: into the form for the key's longest prefix, else in place") {
    CHECK(rewritten("(/a.b 1|)\n", nested) == "(a (b 1))\n");
    CHECK(rewritten("(/a.b.c 1|)\n(/a.d 2)\n", nested) == "(a (b (c 1)))\n(/a.d 2)\n");
    // A child on its own line: the value goes on a line of its own, at that child's column.
    CHECK(rewritten("(units\n   (knight\n      (speed 2.5)))\n(/units.knight.health 1|00)\n",
                    nested) == "(units\n   (knight\n      (speed 2.5)\n      (health 100)))\n");
    // After a comment that ends the last child's line, not into it.
    CHECK(rewritten("(a\n   (b 1) ; why\n)\n(/a.c 2|)\n", nested) ==
          "(a\n   (b 1) ; why\n   (c 2)\n)\n");
    CHECK(rewritten("(a)\n(/a.b.c 2|)\n", nested) == "(a (b (c 2)))\n");
    CHECK(rewritten("(a (b 1))\n(/a.b 2|)\n", nested).empty());  // the whole key is written
}

TEST_CASE("the same tree: values at their paths, keys in any order among their siblings") {
    std::string why;
    CHECK(same_tree("(a (b 1) (c 2))", "(a (c 2) (b 1))", "t.dta", {}, why));
    CHECK(!same_tree("(a (b 1))", "(a (b 2))", "t.dta", {}, why));
    CHECK(!same_tree("(a (b 1 2))", "(a (b 2 1))", "t.dta", {}, why));  // values keep their order
    CHECK(!same_tree("(a (b 1))", "(a (b 1)", "t.dta", {}, why));
    CHECK(why.find("would not read") != std::string::npos);
}
