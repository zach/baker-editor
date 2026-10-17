#include "tree_sitter/parser.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define LANGUAGE_VERSION 14
#define STATE_COUNT 17
#define LARGE_STATE_COUNT 16
#define SYMBOL_COUNT 21
#define ALIAS_COUNT 0
#define TOKEN_COUNT 15
#define EXTERNAL_TOKEN_COUNT 0
#define FIELD_COUNT 0
#define MAX_ALIAS_SEQUENCE_LENGTH 3
#define PRODUCTION_ID_COUNT 1

enum ts_symbol_identifiers {
  sym_symbol = 1,
  anon_sym_LPAREN = 2,
  anon_sym_RPAREN = 3,
  anon_sym_LBRACE = 4,
  anon_sym_RBRACE = 5,
  anon_sym_LBRACK = 6,
  anon_sym_RBRACK = 7,
  sym_number = 8,
  sym_string = 9,
  sym_quoted_symbol = 10,
  sym_variable = 11,
  sym_directive = 12,
  sym_comment = 13,
  sym_block_comment = 14,
  sym_source_file = 15,
  sym__node = 16,
  sym_array = 17,
  sym_command = 18,
  sym_bracket = 19,
  aux_sym_source_file_repeat1 = 20,
};

static const char * const ts_symbol_names[] = {
  [ts_builtin_sym_end] = "end",
  [sym_symbol] = "symbol",
  [anon_sym_LPAREN] = "(",
  [anon_sym_RPAREN] = ")",
  [anon_sym_LBRACE] = "{",
  [anon_sym_RBRACE] = "}",
  [anon_sym_LBRACK] = "[",
  [anon_sym_RBRACK] = "]",
  [sym_number] = "number",
  [sym_string] = "string",
  [sym_quoted_symbol] = "quoted_symbol",
  [sym_variable] = "variable",
  [sym_directive] = "directive",
  [sym_comment] = "comment",
  [sym_block_comment] = "block_comment",
  [sym_source_file] = "source_file",
  [sym__node] = "_node",
  [sym_array] = "array",
  [sym_command] = "command",
  [sym_bracket] = "bracket",
  [aux_sym_source_file_repeat1] = "source_file_repeat1",
};

static const TSSymbol ts_symbol_map[] = {
  [ts_builtin_sym_end] = ts_builtin_sym_end,
  [sym_symbol] = sym_symbol,
  [anon_sym_LPAREN] = anon_sym_LPAREN,
  [anon_sym_RPAREN] = anon_sym_RPAREN,
  [anon_sym_LBRACE] = anon_sym_LBRACE,
  [anon_sym_RBRACE] = anon_sym_RBRACE,
  [anon_sym_LBRACK] = anon_sym_LBRACK,
  [anon_sym_RBRACK] = anon_sym_RBRACK,
  [sym_number] = sym_number,
  [sym_string] = sym_string,
  [sym_quoted_symbol] = sym_quoted_symbol,
  [sym_variable] = sym_variable,
  [sym_directive] = sym_directive,
  [sym_comment] = sym_comment,
  [sym_block_comment] = sym_block_comment,
  [sym_source_file] = sym_source_file,
  [sym__node] = sym__node,
  [sym_array] = sym_array,
  [sym_command] = sym_command,
  [sym_bracket] = sym_bracket,
  [aux_sym_source_file_repeat1] = aux_sym_source_file_repeat1,
};

static const TSSymbolMetadata ts_symbol_metadata[] = {
  [ts_builtin_sym_end] = {
    .visible = false,
    .named = true,
  },
  [sym_symbol] = {
    .visible = true,
    .named = true,
  },
  [anon_sym_LPAREN] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_RPAREN] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_LBRACE] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_RBRACE] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_LBRACK] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_RBRACK] = {
    .visible = true,
    .named = false,
  },
  [sym_number] = {
    .visible = true,
    .named = true,
  },
  [sym_string] = {
    .visible = true,
    .named = true,
  },
  [sym_quoted_symbol] = {
    .visible = true,
    .named = true,
  },
  [sym_variable] = {
    .visible = true,
    .named = true,
  },
  [sym_directive] = {
    .visible = true,
    .named = true,
  },
  [sym_comment] = {
    .visible = true,
    .named = true,
  },
  [sym_block_comment] = {
    .visible = true,
    .named = true,
  },
  [sym_source_file] = {
    .visible = true,
    .named = true,
  },
  [sym__node] = {
    .visible = false,
    .named = true,
  },
  [sym_array] = {
    .visible = true,
    .named = true,
  },
  [sym_command] = {
    .visible = true,
    .named = true,
  },
  [sym_bracket] = {
    .visible = true,
    .named = true,
  },
  [aux_sym_source_file_repeat1] = {
    .visible = false,
    .named = false,
  },
};

static const TSSymbol ts_alias_sequences[PRODUCTION_ID_COUNT][MAX_ALIAS_SEQUENCE_LENGTH] = {
  [0] = {0},
};

static const uint16_t ts_non_terminal_alias_map[] = {
  0,
};

static const TSStateId ts_primary_state_ids[STATE_COUNT] = {
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
  [4] = 4,
  [5] = 5,
  [6] = 6,
  [7] = 7,
  [8] = 8,
  [9] = 9,
  [10] = 10,
  [11] = 11,
  [12] = 12,
  [13] = 13,
  [14] = 14,
  [15] = 15,
  [16] = 16,
};

static TSCharacterRange sym_variable_character_set_1[] = {
  {0, 0x08}, {0x0e, 0x1f}, {'!', '!'}, {'#', '&'}, {'*', ':'}, {'<', 'Z'}, {'\\', '\\'}, {'^', 'z'},
  {'|', '|'}, {'~', 0x10ffff},
};

static TSCharacterRange sym_symbol_character_set_1[] = {
  {0, 0x08}, {0x0e, 0x1f}, {'!', '!'}, {'#', '#'}, {'%', '&'}, {'*', ':'}, {'<', 'Z'}, {'\\', '\\'},
  {'^', 'z'}, {'|', '|'}, {'~', 0x10ffff},
};

static bool ts_lex(TSLexer *lexer, TSStateId state) {
  START_LEXER();
  eof = lexer->eof(lexer);
  switch (state) {
    case 0:
      if (eof) ADVANCE(8);
      ADVANCE_MAP(
        '"', 1,
        '#', 32,
        '$', 6,
        '\'', 2,
        '(', 9,
        ')', 10,
        '-', 27,
        '.', 29,
        '/', 24,
        '0', 15,
        ';', 34,
        '[', 13,
        ']', 14,
        '{', 11,
        '}', 12,
      );
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ') SKIP(0);
      if (('1' <= lookahead && lookahead <= '9')) ADVANCE(16);
      if (lookahead != 0) ADVANCE(33);
      END_STATE();
    case 1:
      if (lookahead == '"') ADVANCE(20);
      if (lookahead != 0) ADVANCE(1);
      END_STATE();
    case 2:
      if (lookahead == '\'') ADVANCE(21);
      if (lookahead != 0) ADVANCE(2);
      END_STATE();
    case 3:
      if (lookahead == '*') ADVANCE(5);
      END_STATE();
    case 4:
      if (lookahead == '*') ADVANCE(4);
      if (lookahead == '/') ADVANCE(35);
      if (lookahead != 0) ADVANCE(5);
      END_STATE();
    case 5:
      if (lookahead == '*') ADVANCE(4);
      if (lookahead != 0) ADVANCE(5);
      END_STATE();
    case 6:
      if ((!eof && set_contains(sym_variable_character_set_1, 10, lookahead))) ADVANCE(22);
      END_STATE();
    case 7:
      if (eof) ADVANCE(8);
      if (lookahead == '/') ADVANCE(3);
      if (lookahead == ';') ADVANCE(34);
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ') SKIP(7);
      END_STATE();
    case 8:
      ACCEPT_TOKEN(ts_builtin_sym_end);
      END_STATE();
    case 9:
      ACCEPT_TOKEN(anon_sym_LPAREN);
      END_STATE();
    case 10:
      ACCEPT_TOKEN(anon_sym_RPAREN);
      END_STATE();
    case 11:
      ACCEPT_TOKEN(anon_sym_LBRACE);
      END_STATE();
    case 12:
      ACCEPT_TOKEN(anon_sym_RBRACE);
      END_STATE();
    case 13:
      ACCEPT_TOKEN(anon_sym_LBRACK);
      END_STATE();
    case 14:
      ACCEPT_TOKEN(anon_sym_RBRACK);
      END_STATE();
    case 15:
      ACCEPT_TOKEN(sym_number);
      if (lookahead == '.') ADVANCE(17);
      if (lookahead == 'E' ||
          lookahead == 'e') ADVANCE(28);
      if (lookahead == 'X' ||
          lookahead == 'x') ADVANCE(31);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(16);
      END_STATE();
    case 16:
      ACCEPT_TOKEN(sym_number);
      if (lookahead == '.') ADVANCE(17);
      if (lookahead == 'E' ||
          lookahead == 'e') ADVANCE(28);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(16);
      END_STATE();
    case 17:
      ACCEPT_TOKEN(sym_number);
      if (lookahead == 'E' ||
          lookahead == 'e') ADVANCE(28);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(17);
      END_STATE();
    case 18:
      ACCEPT_TOKEN(sym_number);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(18);
      END_STATE();
    case 19:
      ACCEPT_TOKEN(sym_number);
      if (('0' <= lookahead && lookahead <= '9') ||
          ('A' <= lookahead && lookahead <= 'F') ||
          ('a' <= lookahead && lookahead <= 'f')) ADVANCE(19);
      END_STATE();
    case 20:
      ACCEPT_TOKEN(sym_string);
      END_STATE();
    case 21:
      ACCEPT_TOKEN(sym_quoted_symbol);
      END_STATE();
    case 22:
      ACCEPT_TOKEN(sym_variable);
      if ((!eof && set_contains(sym_variable_character_set_1, 10, lookahead))) ADVANCE(22);
      END_STATE();
    case 23:
      ACCEPT_TOKEN(sym_directive);
      if (('0' <= lookahead && lookahead <= '9') ||
          ('A' <= lookahead && lookahead <= 'Z') ||
          lookahead == '_' ||
          ('a' <= lookahead && lookahead <= 'z')) ADVANCE(23);
      END_STATE();
    case 24:
      ACCEPT_TOKEN(sym_symbol);
      if (lookahead == '*') ADVANCE(26);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(33);
      END_STATE();
    case 25:
      ACCEPT_TOKEN(sym_symbol);
      if (lookahead == '*') ADVANCE(25);
      if (lookahead == '/') ADVANCE(33);
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ' ||
          lookahead == '"' ||
          lookahead == '$' ||
          ('\'' <= lookahead && lookahead <= ')') ||
          lookahead == ';' ||
          lookahead == '[' ||
          lookahead == ']' ||
          lookahead == '{' ||
          lookahead == '}') ADVANCE(5);
      if (lookahead != 0) ADVANCE(26);
      END_STATE();
    case 26:
      ACCEPT_TOKEN(sym_symbol);
      if (lookahead == '*') ADVANCE(25);
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ' ||
          lookahead == '"' ||
          lookahead == '$' ||
          ('\'' <= lookahead && lookahead <= ')') ||
          lookahead == ';' ||
          lookahead == '[' ||
          lookahead == ']' ||
          lookahead == '{' ||
          lookahead == '}') ADVANCE(5);
      if (lookahead != 0) ADVANCE(26);
      END_STATE();
    case 27:
      ACCEPT_TOKEN(sym_symbol);
      if (lookahead == '.') ADVANCE(29);
      if (lookahead == '0') ADVANCE(15);
      if (('1' <= lookahead && lookahead <= '9')) ADVANCE(16);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(33);
      END_STATE();
    case 28:
      ACCEPT_TOKEN(sym_symbol);
      if (lookahead == '+' ||
          lookahead == '-') ADVANCE(30);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(18);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(33);
      END_STATE();
    case 29:
      ACCEPT_TOKEN(sym_symbol);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(17);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(33);
      END_STATE();
    case 30:
      ACCEPT_TOKEN(sym_symbol);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(18);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(33);
      END_STATE();
    case 31:
      ACCEPT_TOKEN(sym_symbol);
      if (('0' <= lookahead && lookahead <= '9') ||
          ('A' <= lookahead && lookahead <= 'F') ||
          ('a' <= lookahead && lookahead <= 'f')) ADVANCE(19);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(33);
      END_STATE();
    case 32:
      ACCEPT_TOKEN(sym_symbol);
      if (('A' <= lookahead && lookahead <= 'Z') ||
          lookahead == '_' ||
          ('a' <= lookahead && lookahead <= 'z')) ADVANCE(23);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(33);
      END_STATE();
    case 33:
      ACCEPT_TOKEN(sym_symbol);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(33);
      END_STATE();
    case 34:
      ACCEPT_TOKEN(sym_comment);
      if (lookahead != 0 &&
          lookahead != '\n') ADVANCE(34);
      END_STATE();
    case 35:
      ACCEPT_TOKEN(sym_block_comment);
      END_STATE();
    default:
      return false;
  }
}

static bool ts_lex_keywords(TSLexer *lexer, TSStateId state) {
  START_LEXER();
  eof = lexer->eof(lexer);
  switch (state) {
    case 0:
      ACCEPT_TOKEN(ts_builtin_sym_end);
      END_STATE();
    default:
      return false;
  }
}

static const TSLexMode ts_lex_modes[STATE_COUNT] = {
  [0] = {.lex_state = 0},
  [1] = {.lex_state = 0},
  [2] = {.lex_state = 0},
  [3] = {.lex_state = 0},
  [4] = {.lex_state = 0},
  [5] = {.lex_state = 0},
  [6] = {.lex_state = 0},
  [7] = {.lex_state = 0},
  [8] = {.lex_state = 0},
  [9] = {.lex_state = 0},
  [10] = {.lex_state = 0},
  [11] = {.lex_state = 0},
  [12] = {.lex_state = 0},
  [13] = {.lex_state = 0},
  [14] = {.lex_state = 0},
  [15] = {.lex_state = 0},
  [16] = {.lex_state = 7},
};

static const uint16_t ts_parse_table[LARGE_STATE_COUNT][SYMBOL_COUNT] = {
  [0] = {
    [ts_builtin_sym_end] = ACTIONS(1),
    [sym_symbol] = ACTIONS(1),
    [anon_sym_LPAREN] = ACTIONS(1),
    [anon_sym_RPAREN] = ACTIONS(1),
    [anon_sym_LBRACE] = ACTIONS(1),
    [anon_sym_RBRACE] = ACTIONS(1),
    [anon_sym_LBRACK] = ACTIONS(1),
    [anon_sym_RBRACK] = ACTIONS(1),
    [sym_number] = ACTIONS(1),
    [sym_string] = ACTIONS(1),
    [sym_quoted_symbol] = ACTIONS(1),
    [sym_variable] = ACTIONS(1),
    [sym_directive] = ACTIONS(1),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [1] = {
    [sym_source_file] = STATE(16),
    [sym__node] = STATE(6),
    [sym_array] = STATE(6),
    [sym_command] = STATE(6),
    [sym_bracket] = STATE(6),
    [aux_sym_source_file_repeat1] = STATE(6),
    [ts_builtin_sym_end] = ACTIONS(7),
    [sym_symbol] = ACTIONS(9),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(17),
    [sym_string] = ACTIONS(17),
    [sym_quoted_symbol] = ACTIONS(17),
    [sym_variable] = ACTIONS(17),
    [sym_directive] = ACTIONS(17),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [2] = {
    [sym__node] = STATE(2),
    [sym_array] = STATE(2),
    [sym_command] = STATE(2),
    [sym_bracket] = STATE(2),
    [aux_sym_source_file_repeat1] = STATE(2),
    [ts_builtin_sym_end] = ACTIONS(19),
    [sym_symbol] = ACTIONS(21),
    [anon_sym_LPAREN] = ACTIONS(24),
    [anon_sym_RPAREN] = ACTIONS(19),
    [anon_sym_LBRACE] = ACTIONS(27),
    [anon_sym_RBRACE] = ACTIONS(19),
    [anon_sym_LBRACK] = ACTIONS(30),
    [anon_sym_RBRACK] = ACTIONS(19),
    [sym_number] = ACTIONS(33),
    [sym_string] = ACTIONS(33),
    [sym_quoted_symbol] = ACTIONS(33),
    [sym_variable] = ACTIONS(33),
    [sym_directive] = ACTIONS(33),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [3] = {
    [sym__node] = STATE(7),
    [sym_array] = STATE(7),
    [sym_command] = STATE(7),
    [sym_bracket] = STATE(7),
    [aux_sym_source_file_repeat1] = STATE(7),
    [sym_symbol] = ACTIONS(36),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_RPAREN] = ACTIONS(38),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(40),
    [sym_string] = ACTIONS(40),
    [sym_quoted_symbol] = ACTIONS(40),
    [sym_variable] = ACTIONS(40),
    [sym_directive] = ACTIONS(40),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [4] = {
    [sym__node] = STATE(8),
    [sym_array] = STATE(8),
    [sym_command] = STATE(8),
    [sym_bracket] = STATE(8),
    [aux_sym_source_file_repeat1] = STATE(8),
    [sym_symbol] = ACTIONS(42),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_RBRACE] = ACTIONS(44),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(46),
    [sym_string] = ACTIONS(46),
    [sym_quoted_symbol] = ACTIONS(46),
    [sym_variable] = ACTIONS(46),
    [sym_directive] = ACTIONS(46),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [5] = {
    [sym__node] = STATE(9),
    [sym_array] = STATE(9),
    [sym_command] = STATE(9),
    [sym_bracket] = STATE(9),
    [aux_sym_source_file_repeat1] = STATE(9),
    [sym_symbol] = ACTIONS(48),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [anon_sym_RBRACK] = ACTIONS(50),
    [sym_number] = ACTIONS(52),
    [sym_string] = ACTIONS(52),
    [sym_quoted_symbol] = ACTIONS(52),
    [sym_variable] = ACTIONS(52),
    [sym_directive] = ACTIONS(52),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [6] = {
    [sym__node] = STATE(2),
    [sym_array] = STATE(2),
    [sym_command] = STATE(2),
    [sym_bracket] = STATE(2),
    [aux_sym_source_file_repeat1] = STATE(2),
    [ts_builtin_sym_end] = ACTIONS(54),
    [sym_symbol] = ACTIONS(56),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(58),
    [sym_string] = ACTIONS(58),
    [sym_quoted_symbol] = ACTIONS(58),
    [sym_variable] = ACTIONS(58),
    [sym_directive] = ACTIONS(58),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [7] = {
    [sym__node] = STATE(2),
    [sym_array] = STATE(2),
    [sym_command] = STATE(2),
    [sym_bracket] = STATE(2),
    [aux_sym_source_file_repeat1] = STATE(2),
    [sym_symbol] = ACTIONS(56),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_RPAREN] = ACTIONS(60),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(58),
    [sym_string] = ACTIONS(58),
    [sym_quoted_symbol] = ACTIONS(58),
    [sym_variable] = ACTIONS(58),
    [sym_directive] = ACTIONS(58),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [8] = {
    [sym__node] = STATE(2),
    [sym_array] = STATE(2),
    [sym_command] = STATE(2),
    [sym_bracket] = STATE(2),
    [aux_sym_source_file_repeat1] = STATE(2),
    [sym_symbol] = ACTIONS(56),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_RBRACE] = ACTIONS(62),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(58),
    [sym_string] = ACTIONS(58),
    [sym_quoted_symbol] = ACTIONS(58),
    [sym_variable] = ACTIONS(58),
    [sym_directive] = ACTIONS(58),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [9] = {
    [sym__node] = STATE(2),
    [sym_array] = STATE(2),
    [sym_command] = STATE(2),
    [sym_bracket] = STATE(2),
    [aux_sym_source_file_repeat1] = STATE(2),
    [sym_symbol] = ACTIONS(56),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [anon_sym_RBRACK] = ACTIONS(64),
    [sym_number] = ACTIONS(58),
    [sym_string] = ACTIONS(58),
    [sym_quoted_symbol] = ACTIONS(58),
    [sym_variable] = ACTIONS(58),
    [sym_directive] = ACTIONS(58),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [10] = {
    [ts_builtin_sym_end] = ACTIONS(66),
    [sym_symbol] = ACTIONS(68),
    [anon_sym_LPAREN] = ACTIONS(66),
    [anon_sym_RPAREN] = ACTIONS(66),
    [anon_sym_LBRACE] = ACTIONS(66),
    [anon_sym_RBRACE] = ACTIONS(66),
    [anon_sym_LBRACK] = ACTIONS(66),
    [anon_sym_RBRACK] = ACTIONS(66),
    [sym_number] = ACTIONS(66),
    [sym_string] = ACTIONS(66),
    [sym_quoted_symbol] = ACTIONS(66),
    [sym_variable] = ACTIONS(66),
    [sym_directive] = ACTIONS(66),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [11] = {
    [ts_builtin_sym_end] = ACTIONS(70),
    [sym_symbol] = ACTIONS(72),
    [anon_sym_LPAREN] = ACTIONS(70),
    [anon_sym_RPAREN] = ACTIONS(70),
    [anon_sym_LBRACE] = ACTIONS(70),
    [anon_sym_RBRACE] = ACTIONS(70),
    [anon_sym_LBRACK] = ACTIONS(70),
    [anon_sym_RBRACK] = ACTIONS(70),
    [sym_number] = ACTIONS(70),
    [sym_string] = ACTIONS(70),
    [sym_quoted_symbol] = ACTIONS(70),
    [sym_variable] = ACTIONS(70),
    [sym_directive] = ACTIONS(70),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [12] = {
    [ts_builtin_sym_end] = ACTIONS(74),
    [sym_symbol] = ACTIONS(76),
    [anon_sym_LPAREN] = ACTIONS(74),
    [anon_sym_RPAREN] = ACTIONS(74),
    [anon_sym_LBRACE] = ACTIONS(74),
    [anon_sym_RBRACE] = ACTIONS(74),
    [anon_sym_LBRACK] = ACTIONS(74),
    [anon_sym_RBRACK] = ACTIONS(74),
    [sym_number] = ACTIONS(74),
    [sym_string] = ACTIONS(74),
    [sym_quoted_symbol] = ACTIONS(74),
    [sym_variable] = ACTIONS(74),
    [sym_directive] = ACTIONS(74),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [13] = {
    [ts_builtin_sym_end] = ACTIONS(78),
    [sym_symbol] = ACTIONS(80),
    [anon_sym_LPAREN] = ACTIONS(78),
    [anon_sym_RPAREN] = ACTIONS(78),
    [anon_sym_LBRACE] = ACTIONS(78),
    [anon_sym_RBRACE] = ACTIONS(78),
    [anon_sym_LBRACK] = ACTIONS(78),
    [anon_sym_RBRACK] = ACTIONS(78),
    [sym_number] = ACTIONS(78),
    [sym_string] = ACTIONS(78),
    [sym_quoted_symbol] = ACTIONS(78),
    [sym_variable] = ACTIONS(78),
    [sym_directive] = ACTIONS(78),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [14] = {
    [ts_builtin_sym_end] = ACTIONS(82),
    [sym_symbol] = ACTIONS(84),
    [anon_sym_LPAREN] = ACTIONS(82),
    [anon_sym_RPAREN] = ACTIONS(82),
    [anon_sym_LBRACE] = ACTIONS(82),
    [anon_sym_RBRACE] = ACTIONS(82),
    [anon_sym_LBRACK] = ACTIONS(82),
    [anon_sym_RBRACK] = ACTIONS(82),
    [sym_number] = ACTIONS(82),
    [sym_string] = ACTIONS(82),
    [sym_quoted_symbol] = ACTIONS(82),
    [sym_variable] = ACTIONS(82),
    [sym_directive] = ACTIONS(82),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [15] = {
    [ts_builtin_sym_end] = ACTIONS(86),
    [sym_symbol] = ACTIONS(88),
    [anon_sym_LPAREN] = ACTIONS(86),
    [anon_sym_RPAREN] = ACTIONS(86),
    [anon_sym_LBRACE] = ACTIONS(86),
    [anon_sym_RBRACE] = ACTIONS(86),
    [anon_sym_LBRACK] = ACTIONS(86),
    [anon_sym_RBRACK] = ACTIONS(86),
    [sym_number] = ACTIONS(86),
    [sym_string] = ACTIONS(86),
    [sym_quoted_symbol] = ACTIONS(86),
    [sym_variable] = ACTIONS(86),
    [sym_directive] = ACTIONS(86),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
};

static const uint16_t ts_small_parse_table[] = {
  [0] = 2,
    ACTIONS(90), 1,
      ts_builtin_sym_end,
    ACTIONS(3), 2,
      sym_comment,
      sym_block_comment,
};

static const uint32_t ts_small_parse_table_map[] = {
  [SMALL_STATE(16)] = 0,
};

static const TSParseActionEntry ts_parse_actions[] = {
  [0] = {.entry = {.count = 0, .reusable = false}},
  [1] = {.entry = {.count = 1, .reusable = false}}, RECOVER(),
  [3] = {.entry = {.count = 1, .reusable = true}}, SHIFT_EXTRA(),
  [5] = {.entry = {.count = 1, .reusable = false}}, SHIFT_EXTRA(),
  [7] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_source_file, 0, 0, 0),
  [9] = {.entry = {.count = 1, .reusable = false}}, SHIFT(6),
  [11] = {.entry = {.count = 1, .reusable = true}}, SHIFT(3),
  [13] = {.entry = {.count = 1, .reusable = true}}, SHIFT(4),
  [15] = {.entry = {.count = 1, .reusable = true}}, SHIFT(5),
  [17] = {.entry = {.count = 1, .reusable = true}}, SHIFT(6),
  [19] = {.entry = {.count = 1, .reusable = true}}, REDUCE(aux_sym_source_file_repeat1, 2, 0, 0),
  [21] = {.entry = {.count = 2, .reusable = false}}, REDUCE(aux_sym_source_file_repeat1, 2, 0, 0), SHIFT_REPEAT(2),
  [24] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_source_file_repeat1, 2, 0, 0), SHIFT_REPEAT(3),
  [27] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_source_file_repeat1, 2, 0, 0), SHIFT_REPEAT(4),
  [30] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_source_file_repeat1, 2, 0, 0), SHIFT_REPEAT(5),
  [33] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_source_file_repeat1, 2, 0, 0), SHIFT_REPEAT(2),
  [36] = {.entry = {.count = 1, .reusable = false}}, SHIFT(7),
  [38] = {.entry = {.count = 1, .reusable = true}}, SHIFT(10),
  [40] = {.entry = {.count = 1, .reusable = true}}, SHIFT(7),
  [42] = {.entry = {.count = 1, .reusable = false}}, SHIFT(8),
  [44] = {.entry = {.count = 1, .reusable = true}}, SHIFT(11),
  [46] = {.entry = {.count = 1, .reusable = true}}, SHIFT(8),
  [48] = {.entry = {.count = 1, .reusable = false}}, SHIFT(9),
  [50] = {.entry = {.count = 1, .reusable = true}}, SHIFT(12),
  [52] = {.entry = {.count = 1, .reusable = true}}, SHIFT(9),
  [54] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_source_file, 1, 0, 0),
  [56] = {.entry = {.count = 1, .reusable = false}}, SHIFT(2),
  [58] = {.entry = {.count = 1, .reusable = true}}, SHIFT(2),
  [60] = {.entry = {.count = 1, .reusable = true}}, SHIFT(13),
  [62] = {.entry = {.count = 1, .reusable = true}}, SHIFT(14),
  [64] = {.entry = {.count = 1, .reusable = true}}, SHIFT(15),
  [66] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_array, 2, 0, 0),
  [68] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_array, 2, 0, 0),
  [70] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_command, 2, 0, 0),
  [72] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_command, 2, 0, 0),
  [74] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_bracket, 2, 0, 0),
  [76] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_bracket, 2, 0, 0),
  [78] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_array, 3, 0, 0),
  [80] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_array, 3, 0, 0),
  [82] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_command, 3, 0, 0),
  [84] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_command, 3, 0, 0),
  [86] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_bracket, 3, 0, 0),
  [88] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_bracket, 3, 0, 0),
  [90] = {.entry = {.count = 1, .reusable = true}},  ACCEPT_INPUT(),
};

#ifdef __cplusplus
extern "C" {
#endif
#ifdef TREE_SITTER_HIDE_SYMBOLS
#define TS_PUBLIC
#elif defined(_WIN32)
#define TS_PUBLIC __declspec(dllexport)
#else
#define TS_PUBLIC __attribute__((visibility("default")))
#endif

TS_PUBLIC const TSLanguage *tree_sitter_baker(void) {
  static const TSLanguage language = {
    .version = LANGUAGE_VERSION,
    .symbol_count = SYMBOL_COUNT,
    .alias_count = ALIAS_COUNT,
    .token_count = TOKEN_COUNT,
    .external_token_count = EXTERNAL_TOKEN_COUNT,
    .state_count = STATE_COUNT,
    .large_state_count = LARGE_STATE_COUNT,
    .production_id_count = PRODUCTION_ID_COUNT,
    .field_count = FIELD_COUNT,
    .max_alias_sequence_length = MAX_ALIAS_SEQUENCE_LENGTH,
    .parse_table = &ts_parse_table[0][0],
    .small_parse_table = ts_small_parse_table,
    .small_parse_table_map = ts_small_parse_table_map,
    .parse_actions = ts_parse_actions,
    .symbol_names = ts_symbol_names,
    .symbol_metadata = ts_symbol_metadata,
    .public_symbol_map = ts_symbol_map,
    .alias_map = ts_non_terminal_alias_map,
    .alias_sequences = &ts_alias_sequences[0][0],
    .lex_modes = ts_lex_modes,
    .lex_fn = ts_lex,
    .keyword_lex_fn = ts_lex_keywords,
    .keyword_capture_token = sym_symbol,
    .primary_state_ids = ts_primary_state_ids,
  };
  return &language;
}
#ifdef __cplusplus
}
#endif
