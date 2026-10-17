#include "scan.hpp"

#include <doctest.h>

#include <string>

using namespace baker_lsp;

namespace {
// The cursor is at the '|', which is taken out.
Cursor at(std::string text) {
    const size_t bar = text.find('|');
    text.erase(bar, 1);
    return scan(text, bar);
}
}  // namespace

TEST_CASE("the cursor's enclosing arrays and token") {
    Cursor c = at("(units (knight (health 1|00)))");
    CHECK(c.token == "100");
    CHECK(c.index == 1);
    CHECK(c.data_path() == std::vector<std::string>{"units", "knight", "health"});

    c = at("(units (knight (|health 100)))");
    CHECK(c.token == "health");
    CHECK(c.index == 0);
    CHECK(c.parent_path() == std::vector<std::string>{"units", "knight"});
}

TEST_CASE("text being typed, which does not parse") {
    const Cursor c = at("(units (knight (he|");
    CHECK(c.token == "he");
    CHECK(c.index == 0);
    CHECK(c.parent_path() == std::vector<std::string>{"units", "knight"});
}

TEST_CASE("between tokens, and before a bracket") {
    Cursor c = at("(units (knight |(speed 2)))");
    CHECK(c.token.empty());
    CHECK(c.index == 1);
    CHECK(c.data_path() == std::vector<std::string>{"units", "knight"});
    c = at("(units (|");
    CHECK(c.index == 0);
    CHECK(c.parent_path() == std::vector<std::string>{"units"});
}

TEST_CASE("arrays already closed are skipped; commands have no data path") {
    Cursor c = at("(a (b 1) (c 2)) (d |");
    CHECK(c.data_path() == std::vector<std::string>{"d"});
    c = at("(start {game.sp|");
    CHECK(c.stack.back().open == '{');
    CHECK(c.token == "game.sp");
    CHECK(c.data_path().empty());
}

TEST_CASE("comments and strings are quiet") {
    CHECK(at("; a (comment|\n(x 1)").quiet);
    CHECK(at("(name \"a (str|ing\")").quiet);
    CHECK(!at("; done\n(x |1)").quiet);
    CHECK(at("(x \"a ) b\" (y |").data_path() == std::vector<std::string>{"x", "y"});
    CHECK(at("/* (a (b |) */").quiet);
    CHECK(at("/* (a\n (b */ (c |").data_path() == std::vector<std::string>{"c"});
    CHECK(at("(x \"a\\\" (y |").data_path() == std::vector<std::string>{"x", "y"});  // "a\" ends
}

TEST_CASE("a quoted tag is unquoted, and quoted again in a path") {
    const Cursor c = at("(menu ('bacon and eggs' (price |3)))");
    CHECK(c.data_path() == std::vector<std::string>{"menu", "bacon and eggs", "price"});
    CHECK(path_text(c.data_path()) == "menu/'bacon and eggs'/price");
    CHECK(path_text({"a", "0"}) == "a/'0'");
}
