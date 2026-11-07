#include "scan.hpp"

#include <doctest.h>

#include <string>

using namespace baker_lsp;

namespace {
// The cursor is at the '|', which is taken out.
Cursor at(std::string text) {
    const size_t bar = text.find('|');
    text.erase(bar, 1);
    return scan(read_structure(text), text, bar);
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

TEST_CASE("the cursor is found as a read of the whole text finds it") {
    // A string or comment running on past the cursor's line.
    Cursor c = at("(name \"a|\nb\")");
    CHECK(c.in_string);
    CHECK(c.token == "a\nb");
    CHECK(at("/* a|\n b */ (x 1)").quiet);
    CHECK(!at("(x 1) /* a */ (y |\n 2)").quiet);
    // At the line's end, inside a bracket closed on a later line.
    c = at("(objective puzzle.brisk|\n   (below 3))");
    CHECK(c.previous == "objective");
    CHECK(c.index == 1);
    // Baker's own tokens: a hex number, a variable, a directive.
    c = at("(a 0x1F $v |)");
    CHECK(c.index == 3);
    CHECK(c.previous == "$v");
    c = at("#define |X\n");
    CHECK(c.token == "X");
}

TEST_CASE("a row's key is the path it names, segment by segment") {
    Cursor c = at("(/units.knight.health 1|00)");
    CHECK(c.data_path() == std::vector<std::string>{"units", "knight", "health"});
    c = at("(/units.kni|ght.health 100)");
    CHECK(c.token == "knight");
    CHECK(c.index == 0);
    CHECK(c.key_before == std::vector<std::string>{"units"});
    CHECK(c.named_path() == std::vector<std::string>{"units", "knight"});
    c = at("(/menu/'bacon_eggs'.pr|ice 3)");
    CHECK(c.named_path() == std::vector<std::string>{"menu", "bacon_eggs", "price"});
    c = at("(/units.knight (health 1|00))");  // a row of a whole record
    CHECK(c.data_path() == std::vector<std::string>{"units", "knight", "health"});
    c = at("(/units..health 1|00)");  // a key that does not read names nothing
    CHECK(c.data_path().empty());
    c = at("(/units.0.health 1|00)");
    CHECK(c.data_path().empty());
}

TEST_CASE("an infix brace: its operator is the head, each operand after it") {
    Cursor c = at("(pass {2 <|= $value <= 10})");
    CHECK(c.token == "<=");
    CHECK(c.index == 0);
    CHECK(c.stack.back().tag == "<=");
    c = at("(pass {2 <= $value <|= 10})");  // the second operator too
    CHECK(c.index == 0);
    c = at("(pass {|2 <= $value <= 10})");  // the first operand is no head
    CHECK(c.token == "2");
    CHECK(c.index >= 1);
    CHECK(c.stack.back().tag == "<=");
    c = at("(pass {<= 2 $va|lue 10})");  // a prefix command, as before
    CHECK(c.stack.back().tag == "<=");
    CHECK(c.index == 2);
    c = at("(r {wheat -|> flour})");  // bare symbols: a call of wheat
    CHECK(c.stack.back().tag == "wheat");
    CHECK(c.index == 1);
}
