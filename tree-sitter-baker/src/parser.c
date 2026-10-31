#include "tree_sitter/parser.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define LANGUAGE_VERSION 14
#define STATE_COUNT 25
#define LARGE_STATE_COUNT 22
#define SYMBOL_COUNT 26
#define ALIAS_COUNT 0
#define TOKEN_COUNT 18
#define EXTERNAL_TOKEN_COUNT 0
#define FIELD_COUNT 0
#define MAX_ALIAS_SEQUENCE_LENGTH 4
#define PRODUCTION_ID_COUNT 1

enum ts_symbol_identifiers {
  sym_symbol = 1,
  anon_sym_LPAREN = 2,
  anon_sym_RPAREN = 3,
  aux_sym_row_key_token1 = 4,
  aux_sym_row_key_token2 = 5,
  aux_sym_row_key_token3 = 6,
  anon_sym_LBRACE = 7,
  anon_sym_RBRACE = 8,
  anon_sym_LBRACK = 9,
  anon_sym_RBRACK = 10,
  sym_number = 11,
  sym_string = 12,
  sym_quoted_symbol = 13,
  sym_variable = 14,
  sym_directive = 15,
  sym_comment = 16,
  sym_block_comment = 17,
  sym_source_file = 18,
  sym__node = 19,
  sym_array = 20,
  sym_row_key = 21,
  sym_command = 22,
  sym_bracket = 23,
  aux_sym_source_file_repeat1 = 24,
  aux_sym_row_key_repeat1 = 25,
};

static const char * const ts_symbol_names[] = {
  [ts_builtin_sym_end] = "end",
  [sym_symbol] = "symbol",
  [anon_sym_LPAREN] = "(",
  [anon_sym_RPAREN] = ")",
  [aux_sym_row_key_token1] = "key_segment",
  [aux_sym_row_key_token2] = "key_separator",
  [aux_sym_row_key_token3] = "key_segment",
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
  [sym_row_key] = "row_key",
  [sym_command] = "command",
  [sym_bracket] = "bracket",
  [aux_sym_source_file_repeat1] = "source_file_repeat1",
  [aux_sym_row_key_repeat1] = "row_key_repeat1",
};

static const TSSymbol ts_symbol_map[] = {
  [ts_builtin_sym_end] = ts_builtin_sym_end,
  [sym_symbol] = sym_symbol,
  [anon_sym_LPAREN] = anon_sym_LPAREN,
  [anon_sym_RPAREN] = anon_sym_RPAREN,
  [aux_sym_row_key_token1] = aux_sym_row_key_token1,
  [aux_sym_row_key_token2] = aux_sym_row_key_token2,
  [aux_sym_row_key_token3] = aux_sym_row_key_token1,
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
  [sym_row_key] = sym_row_key,
  [sym_command] = sym_command,
  [sym_bracket] = sym_bracket,
  [aux_sym_source_file_repeat1] = aux_sym_source_file_repeat1,
  [aux_sym_row_key_repeat1] = aux_sym_row_key_repeat1,
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
  [aux_sym_row_key_token1] = {
    .visible = true,
    .named = true,
  },
  [aux_sym_row_key_token2] = {
    .visible = true,
    .named = true,
  },
  [aux_sym_row_key_token3] = {
    .visible = true,
    .named = true,
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
  [sym_row_key] = {
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
  [aux_sym_row_key_repeat1] = {
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
  [17] = 17,
  [18] = 18,
  [19] = 19,
  [20] = 20,
  [21] = 21,
  [22] = 22,
  [23] = 23,
  [24] = 24,
};

static TSCharacterRange aux_sym_row_key_token1_character_set_2[] = {
  {0, 0x08}, {0x0e, 0x1f}, {'!', '!'}, {'#', '#'}, {'%', '&'}, {'*', '-'}, {'0', ':'}, {'<', 'Z'},
  {'\\', '\\'}, {'^', 'z'}, {'|', '|'}, {'~', 0x10ffff},
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
      if (eof) ADVANCE(15);
      ADVANCE_MAP(
        '"', 4,
        '#', 29,
        '$', 11,
        '\'', 5,
        '(', 16,
        ')', 17,
        '-', 23,
        '.', 21,
        '/', 21,
        '0', 25,
        ';', 56,
        '[', 34,
        ']', 35,
        '{', 32,
        '}', 33,
      );
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ') SKIP(12);
      if (('1' <= lookahead && lookahead <= '9')) ADVANCE(26);
      if (lookahead != 0) ADVANCE(31);
      END_STATE();
    case 1:
      ADVANCE_MAP(
        '"', 4,
        '#', 54,
        '$', 11,
        '\'', 6,
        '(', 16,
        ')', 17,
        '-', 49,
        '.', 21,
        '/', 21,
        '0', 36,
        ';', 56,
        '[', 34,
        '{', 32,
      );
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ') SKIP(2);
      if (('1' <= lookahead && lookahead <= '9')) ADVANCE(37);
      if (lookahead != 0 &&
          lookahead != ']' &&
          lookahead != '}') ADVANCE(55);
      END_STATE();
    case 2:
      ADVANCE_MAP(
        '"', 4,
        '#', 54,
        '$', 11,
        '\'', 6,
        '(', 16,
        ')', 17,
        '-', 49,
        '.', 51,
        '/', 46,
        '0', 36,
        ';', 56,
        '[', 34,
        '{', 32,
      );
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ') SKIP(2);
      if (('1' <= lookahead && lookahead <= '9')) ADVANCE(37);
      if (lookahead != 0 &&
          lookahead != ']' &&
          lookahead != '}') ADVANCE(55);
      END_STATE();
    case 3:
      ADVANCE_MAP(
        '"', 4,
        '#', 54,
        '$', 11,
        '\'', 6,
        '(', 16,
        ')', 17,
        '-', 49,
        '.', 51,
        '/', 45,
        '0', 36,
        ';', 56,
        '[', 34,
        '{', 32,
      );
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ') SKIP(3);
      if (('1' <= lookahead && lookahead <= '9')) ADVANCE(37);
      if (lookahead != 0 &&
          lookahead != ']' &&
          lookahead != '}') ADVANCE(55);
      END_STATE();
    case 4:
      if (lookahead == '"') ADVANCE(41);
      if (lookahead != 0) ADVANCE(4);
      END_STATE();
    case 5:
      if (lookahead == '\'') ADVANCE(22);
      if (lookahead != 0) ADVANCE(5);
      END_STATE();
    case 6:
      if (lookahead == '\'') ADVANCE(42);
      if (lookahead != 0) ADVANCE(6);
      END_STATE();
    case 7:
      if (lookahead == '\'') ADVANCE(18);
      if (lookahead != 0) ADVANCE(7);
      END_STATE();
    case 8:
      if (lookahead == '*') ADVANCE(10);
      END_STATE();
    case 9:
      if (lookahead == '*') ADVANCE(9);
      if (lookahead == '/') ADVANCE(57);
      if (lookahead != 0) ADVANCE(10);
      END_STATE();
    case 10:
      if (lookahead == '*') ADVANCE(9);
      if (lookahead != 0) ADVANCE(10);
      END_STATE();
    case 11:
      if ((!eof && set_contains(sym_variable_character_set_1, 10, lookahead))) ADVANCE(43);
      END_STATE();
    case 12:
      if (eof) ADVANCE(15);
      ADVANCE_MAP(
        '"', 4,
        '#', 54,
        '$', 11,
        '\'', 6,
        '(', 16,
        ')', 17,
        '-', 49,
        '.', 51,
        '/', 46,
        '0', 36,
        ';', 56,
        '[', 34,
        ']', 35,
        '{', 32,
        '}', 33,
      );
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ') SKIP(12);
      if (('1' <= lookahead && lookahead <= '9')) ADVANCE(37);
      if (lookahead != 0) ADVANCE(55);
      END_STATE();
    case 13:
      if (eof) ADVANCE(15);
      if (lookahead == '\'') ADVANCE(5);
      if (lookahead == '/') ADVANCE(8);
      if (lookahead == ';') ADVANCE(56);
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ') SKIP(14);
      if ((!eof && set_contains(aux_sym_row_key_token1_character_set_2, 12, lookahead))) ADVANCE(31);
      END_STATE();
    case 14:
      if (eof) ADVANCE(15);
      if (lookahead == '/') ADVANCE(8);
      if (lookahead == ';') ADVANCE(56);
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ') SKIP(14);
      END_STATE();
    case 15:
      ACCEPT_TOKEN(ts_builtin_sym_end);
      END_STATE();
    case 16:
      ACCEPT_TOKEN(anon_sym_LPAREN);
      END_STATE();
    case 17:
      ACCEPT_TOKEN(anon_sym_RPAREN);
      END_STATE();
    case 18:
      ACCEPT_TOKEN(aux_sym_row_key_token1);
      END_STATE();
    case 19:
      ACCEPT_TOKEN(aux_sym_row_key_token1);
      if (lookahead == '*') ADVANCE(19);
      if ((!eof && set_contains(aux_sym_row_key_token1_character_set_2, 12, lookahead))) ADVANCE(19);
      END_STATE();
    case 20:
      ACCEPT_TOKEN(aux_sym_row_key_token1);
      if ((!eof && set_contains(aux_sym_row_key_token1_character_set_2, 12, lookahead))) ADVANCE(20);
      END_STATE();
    case 21:
      ACCEPT_TOKEN(aux_sym_row_key_token2);
      END_STATE();
    case 22:
      ACCEPT_TOKEN(aux_sym_row_key_token3);
      END_STATE();
    case 23:
      ACCEPT_TOKEN(aux_sym_row_key_token3);
      if (lookahead == '0') ADVANCE(25);
      if (('1' <= lookahead && lookahead <= '9')) ADVANCE(26);
      if ((!eof && set_contains(aux_sym_row_key_token1_character_set_2, 12, lookahead))) ADVANCE(31);
      END_STATE();
    case 24:
      ACCEPT_TOKEN(aux_sym_row_key_token3);
      if (lookahead == '+' ||
          lookahead == '-') ADVANCE(27);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(27);
      if ((!eof && set_contains(aux_sym_row_key_token1_character_set_2, 12, lookahead))) ADVANCE(31);
      END_STATE();
    case 25:
      ACCEPT_TOKEN(aux_sym_row_key_token3);
      if (lookahead == 'E' ||
          lookahead == 'e') ADVANCE(24);
      if (lookahead == 'X' ||
          lookahead == 'x') ADVANCE(28);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(26);
      if ((!eof && set_contains(aux_sym_row_key_token1_character_set_2, 12, lookahead))) ADVANCE(31);
      END_STATE();
    case 26:
      ACCEPT_TOKEN(aux_sym_row_key_token3);
      if (lookahead == 'E' ||
          lookahead == 'e') ADVANCE(24);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(26);
      if ((!eof && set_contains(aux_sym_row_key_token1_character_set_2, 12, lookahead))) ADVANCE(31);
      END_STATE();
    case 27:
      ACCEPT_TOKEN(aux_sym_row_key_token3);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(27);
      if ((!eof && set_contains(aux_sym_row_key_token1_character_set_2, 12, lookahead))) ADVANCE(31);
      END_STATE();
    case 28:
      ACCEPT_TOKEN(aux_sym_row_key_token3);
      if (('0' <= lookahead && lookahead <= '9') ||
          ('A' <= lookahead && lookahead <= 'F') ||
          ('a' <= lookahead && lookahead <= 'f')) ADVANCE(28);
      if ((!eof && set_contains(aux_sym_row_key_token1_character_set_2, 12, lookahead))) ADVANCE(31);
      END_STATE();
    case 29:
      ACCEPT_TOKEN(aux_sym_row_key_token3);
      if (('A' <= lookahead && lookahead <= 'Z') ||
          ('a' <= lookahead && lookahead <= 'z')) ADVANCE(30);
      if ((!eof && set_contains(aux_sym_row_key_token1_character_set_2, 12, lookahead))) ADVANCE(31);
      END_STATE();
    case 30:
      ACCEPT_TOKEN(aux_sym_row_key_token3);
      if (('0' <= lookahead && lookahead <= '9') ||
          ('A' <= lookahead && lookahead <= 'Z') ||
          lookahead == '_' ||
          ('a' <= lookahead && lookahead <= 'z')) ADVANCE(30);
      if ((!eof && set_contains(aux_sym_row_key_token1_character_set_2, 12, lookahead))) ADVANCE(31);
      END_STATE();
    case 31:
      ACCEPT_TOKEN(aux_sym_row_key_token3);
      if ((!eof && set_contains(aux_sym_row_key_token1_character_set_2, 12, lookahead))) ADVANCE(31);
      END_STATE();
    case 32:
      ACCEPT_TOKEN(anon_sym_LBRACE);
      END_STATE();
    case 33:
      ACCEPT_TOKEN(anon_sym_RBRACE);
      END_STATE();
    case 34:
      ACCEPT_TOKEN(anon_sym_LBRACK);
      END_STATE();
    case 35:
      ACCEPT_TOKEN(anon_sym_RBRACK);
      END_STATE();
    case 36:
      ACCEPT_TOKEN(sym_number);
      if (lookahead == '.') ADVANCE(38);
      if (lookahead == 'E' ||
          lookahead == 'e') ADVANCE(50);
      if (lookahead == 'X' ||
          lookahead == 'x') ADVANCE(53);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(37);
      END_STATE();
    case 37:
      ACCEPT_TOKEN(sym_number);
      if (lookahead == '.') ADVANCE(38);
      if (lookahead == 'E' ||
          lookahead == 'e') ADVANCE(50);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(37);
      END_STATE();
    case 38:
      ACCEPT_TOKEN(sym_number);
      if (lookahead == 'E' ||
          lookahead == 'e') ADVANCE(50);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(38);
      END_STATE();
    case 39:
      ACCEPT_TOKEN(sym_number);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(39);
      END_STATE();
    case 40:
      ACCEPT_TOKEN(sym_number);
      if (('0' <= lookahead && lookahead <= '9') ||
          ('A' <= lookahead && lookahead <= 'F') ||
          ('a' <= lookahead && lookahead <= 'f')) ADVANCE(40);
      END_STATE();
    case 41:
      ACCEPT_TOKEN(sym_string);
      END_STATE();
    case 42:
      ACCEPT_TOKEN(sym_quoted_symbol);
      END_STATE();
    case 43:
      ACCEPT_TOKEN(sym_variable);
      if ((!eof && set_contains(sym_variable_character_set_1, 10, lookahead))) ADVANCE(43);
      END_STATE();
    case 44:
      ACCEPT_TOKEN(sym_directive);
      if (('0' <= lookahead && lookahead <= '9') ||
          ('A' <= lookahead && lookahead <= 'Z') ||
          lookahead == '_' ||
          ('a' <= lookahead && lookahead <= 'z')) ADVANCE(44);
      END_STATE();
    case 45:
      ACCEPT_TOKEN(sym_symbol);
      if (lookahead == '\'') ADVANCE(7);
      if (lookahead == '*') ADVANCE(19);
      if (lookahead == '.' ||
          lookahead == '/') ADVANCE(55);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(20);
      END_STATE();
    case 46:
      ACCEPT_TOKEN(sym_symbol);
      if (lookahead == '*') ADVANCE(48);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(55);
      END_STATE();
    case 47:
      ACCEPT_TOKEN(sym_symbol);
      if (lookahead == '*') ADVANCE(47);
      if (lookahead == '/') ADVANCE(55);
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ' ||
          lookahead == '"' ||
          lookahead == '$' ||
          ('\'' <= lookahead && lookahead <= ')') ||
          lookahead == ';' ||
          lookahead == '[' ||
          lookahead == ']' ||
          lookahead == '{' ||
          lookahead == '}') ADVANCE(10);
      if (lookahead != 0) ADVANCE(48);
      END_STATE();
    case 48:
      ACCEPT_TOKEN(sym_symbol);
      if (lookahead == '*') ADVANCE(47);
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ' ||
          lookahead == '"' ||
          lookahead == '$' ||
          ('\'' <= lookahead && lookahead <= ')') ||
          lookahead == ';' ||
          lookahead == '[' ||
          lookahead == ']' ||
          lookahead == '{' ||
          lookahead == '}') ADVANCE(10);
      if (lookahead != 0) ADVANCE(48);
      END_STATE();
    case 49:
      ACCEPT_TOKEN(sym_symbol);
      if (lookahead == '.') ADVANCE(51);
      if (lookahead == '0') ADVANCE(36);
      if (('1' <= lookahead && lookahead <= '9')) ADVANCE(37);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(55);
      END_STATE();
    case 50:
      ACCEPT_TOKEN(sym_symbol);
      if (lookahead == '+' ||
          lookahead == '-') ADVANCE(52);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(39);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(55);
      END_STATE();
    case 51:
      ACCEPT_TOKEN(sym_symbol);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(38);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(55);
      END_STATE();
    case 52:
      ACCEPT_TOKEN(sym_symbol);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(39);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(55);
      END_STATE();
    case 53:
      ACCEPT_TOKEN(sym_symbol);
      if (('0' <= lookahead && lookahead <= '9') ||
          ('A' <= lookahead && lookahead <= 'F') ||
          ('a' <= lookahead && lookahead <= 'f')) ADVANCE(40);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(55);
      END_STATE();
    case 54:
      ACCEPT_TOKEN(sym_symbol);
      if (('A' <= lookahead && lookahead <= 'Z') ||
          ('a' <= lookahead && lookahead <= 'z')) ADVANCE(44);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(55);
      END_STATE();
    case 55:
      ACCEPT_TOKEN(sym_symbol);
      if ((!eof && set_contains(sym_symbol_character_set_1, 11, lookahead))) ADVANCE(55);
      END_STATE();
    case 56:
      ACCEPT_TOKEN(sym_comment);
      if (lookahead != 0 &&
          lookahead != '\n') ADVANCE(56);
      END_STATE();
    case 57:
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
  [1] = {.lex_state = 12},
  [2] = {.lex_state = 12},
  [3] = {.lex_state = 3},
  [4] = {.lex_state = 12},
  [5] = {.lex_state = 12},
  [6] = {.lex_state = 12},
  [7] = {.lex_state = 12},
  [8] = {.lex_state = 12},
  [9] = {.lex_state = 12},
  [10] = {.lex_state = 12},
  [11] = {.lex_state = 12},
  [12] = {.lex_state = 12},
  [13] = {.lex_state = 12},
  [14] = {.lex_state = 12},
  [15] = {.lex_state = 12},
  [16] = {.lex_state = 12},
  [17] = {.lex_state = 12},
  [18] = {.lex_state = 12},
  [19] = {.lex_state = 1},
  [20] = {.lex_state = 1},
  [21] = {.lex_state = 1},
  [22] = {.lex_state = 1},
  [23] = {.lex_state = 13},
  [24] = {.lex_state = 13},
};

static const uint16_t ts_parse_table[LARGE_STATE_COUNT][SYMBOL_COUNT] = {
  [0] = {
    [ts_builtin_sym_end] = ACTIONS(1),
    [sym_symbol] = ACTIONS(1),
    [anon_sym_LPAREN] = ACTIONS(1),
    [anon_sym_RPAREN] = ACTIONS(1),
    [aux_sym_row_key_token2] = ACTIONS(1),
    [aux_sym_row_key_token3] = ACTIONS(1),
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
    [sym_source_file] = STATE(23),
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
    [sym__node] = STATE(8),
    [sym_array] = STATE(8),
    [sym_row_key] = STATE(7),
    [sym_command] = STATE(8),
    [sym_bracket] = STATE(8),
    [aux_sym_source_file_repeat1] = STATE(8),
    [sym_symbol] = ACTIONS(36),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_RPAREN] = ACTIONS(38),
    [aux_sym_row_key_token1] = ACTIONS(40),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(42),
    [sym_string] = ACTIONS(42),
    [sym_quoted_symbol] = ACTIONS(42),
    [sym_variable] = ACTIONS(42),
    [sym_directive] = ACTIONS(42),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [4] = {
    [sym__node] = STATE(9),
    [sym_array] = STATE(9),
    [sym_command] = STATE(9),
    [sym_bracket] = STATE(9),
    [aux_sym_source_file_repeat1] = STATE(9),
    [sym_symbol] = ACTIONS(44),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_RBRACE] = ACTIONS(46),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(48),
    [sym_string] = ACTIONS(48),
    [sym_quoted_symbol] = ACTIONS(48),
    [sym_variable] = ACTIONS(48),
    [sym_directive] = ACTIONS(48),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [5] = {
    [sym__node] = STATE(11),
    [sym_array] = STATE(11),
    [sym_command] = STATE(11),
    [sym_bracket] = STATE(11),
    [aux_sym_source_file_repeat1] = STATE(11),
    [sym_symbol] = ACTIONS(50),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [anon_sym_RBRACK] = ACTIONS(52),
    [sym_number] = ACTIONS(54),
    [sym_string] = ACTIONS(54),
    [sym_quoted_symbol] = ACTIONS(54),
    [sym_variable] = ACTIONS(54),
    [sym_directive] = ACTIONS(54),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [6] = {
    [sym__node] = STATE(2),
    [sym_array] = STATE(2),
    [sym_command] = STATE(2),
    [sym_bracket] = STATE(2),
    [aux_sym_source_file_repeat1] = STATE(2),
    [ts_builtin_sym_end] = ACTIONS(56),
    [sym_symbol] = ACTIONS(58),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(60),
    [sym_string] = ACTIONS(60),
    [sym_quoted_symbol] = ACTIONS(60),
    [sym_variable] = ACTIONS(60),
    [sym_directive] = ACTIONS(60),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [7] = {
    [sym__node] = STATE(10),
    [sym_array] = STATE(10),
    [sym_command] = STATE(10),
    [sym_bracket] = STATE(10),
    [aux_sym_source_file_repeat1] = STATE(10),
    [sym_symbol] = ACTIONS(62),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_RPAREN] = ACTIONS(64),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(66),
    [sym_string] = ACTIONS(66),
    [sym_quoted_symbol] = ACTIONS(66),
    [sym_variable] = ACTIONS(66),
    [sym_directive] = ACTIONS(66),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [8] = {
    [sym__node] = STATE(2),
    [sym_array] = STATE(2),
    [sym_command] = STATE(2),
    [sym_bracket] = STATE(2),
    [aux_sym_source_file_repeat1] = STATE(2),
    [sym_symbol] = ACTIONS(58),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_RPAREN] = ACTIONS(64),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(60),
    [sym_string] = ACTIONS(60),
    [sym_quoted_symbol] = ACTIONS(60),
    [sym_variable] = ACTIONS(60),
    [sym_directive] = ACTIONS(60),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [9] = {
    [sym__node] = STATE(2),
    [sym_array] = STATE(2),
    [sym_command] = STATE(2),
    [sym_bracket] = STATE(2),
    [aux_sym_source_file_repeat1] = STATE(2),
    [sym_symbol] = ACTIONS(58),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_RBRACE] = ACTIONS(68),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(60),
    [sym_string] = ACTIONS(60),
    [sym_quoted_symbol] = ACTIONS(60),
    [sym_variable] = ACTIONS(60),
    [sym_directive] = ACTIONS(60),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [10] = {
    [sym__node] = STATE(2),
    [sym_array] = STATE(2),
    [sym_command] = STATE(2),
    [sym_bracket] = STATE(2),
    [aux_sym_source_file_repeat1] = STATE(2),
    [sym_symbol] = ACTIONS(58),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_RPAREN] = ACTIONS(70),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [sym_number] = ACTIONS(60),
    [sym_string] = ACTIONS(60),
    [sym_quoted_symbol] = ACTIONS(60),
    [sym_variable] = ACTIONS(60),
    [sym_directive] = ACTIONS(60),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [11] = {
    [sym__node] = STATE(2),
    [sym_array] = STATE(2),
    [sym_command] = STATE(2),
    [sym_bracket] = STATE(2),
    [aux_sym_source_file_repeat1] = STATE(2),
    [sym_symbol] = ACTIONS(58),
    [anon_sym_LPAREN] = ACTIONS(11),
    [anon_sym_LBRACE] = ACTIONS(13),
    [anon_sym_LBRACK] = ACTIONS(15),
    [anon_sym_RBRACK] = ACTIONS(72),
    [sym_number] = ACTIONS(60),
    [sym_string] = ACTIONS(60),
    [sym_quoted_symbol] = ACTIONS(60),
    [sym_variable] = ACTIONS(60),
    [sym_directive] = ACTIONS(60),
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
  [16] = {
    [ts_builtin_sym_end] = ACTIONS(90),
    [sym_symbol] = ACTIONS(92),
    [anon_sym_LPAREN] = ACTIONS(90),
    [anon_sym_RPAREN] = ACTIONS(90),
    [anon_sym_LBRACE] = ACTIONS(90),
    [anon_sym_RBRACE] = ACTIONS(90),
    [anon_sym_LBRACK] = ACTIONS(90),
    [anon_sym_RBRACK] = ACTIONS(90),
    [sym_number] = ACTIONS(90),
    [sym_string] = ACTIONS(90),
    [sym_quoted_symbol] = ACTIONS(90),
    [sym_variable] = ACTIONS(90),
    [sym_directive] = ACTIONS(90),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [17] = {
    [ts_builtin_sym_end] = ACTIONS(94),
    [sym_symbol] = ACTIONS(96),
    [anon_sym_LPAREN] = ACTIONS(94),
    [anon_sym_RPAREN] = ACTIONS(94),
    [anon_sym_LBRACE] = ACTIONS(94),
    [anon_sym_RBRACE] = ACTIONS(94),
    [anon_sym_LBRACK] = ACTIONS(94),
    [anon_sym_RBRACK] = ACTIONS(94),
    [sym_number] = ACTIONS(94),
    [sym_string] = ACTIONS(94),
    [sym_quoted_symbol] = ACTIONS(94),
    [sym_variable] = ACTIONS(94),
    [sym_directive] = ACTIONS(94),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [18] = {
    [ts_builtin_sym_end] = ACTIONS(98),
    [sym_symbol] = ACTIONS(100),
    [anon_sym_LPAREN] = ACTIONS(98),
    [anon_sym_RPAREN] = ACTIONS(98),
    [anon_sym_LBRACE] = ACTIONS(98),
    [anon_sym_RBRACE] = ACTIONS(98),
    [anon_sym_LBRACK] = ACTIONS(98),
    [anon_sym_RBRACK] = ACTIONS(98),
    [sym_number] = ACTIONS(98),
    [sym_string] = ACTIONS(98),
    [sym_quoted_symbol] = ACTIONS(98),
    [sym_variable] = ACTIONS(98),
    [sym_directive] = ACTIONS(98),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [19] = {
    [aux_sym_row_key_repeat1] = STATE(20),
    [sym_symbol] = ACTIONS(102),
    [anon_sym_LPAREN] = ACTIONS(104),
    [anon_sym_RPAREN] = ACTIONS(104),
    [aux_sym_row_key_token2] = ACTIONS(106),
    [anon_sym_LBRACE] = ACTIONS(104),
    [anon_sym_LBRACK] = ACTIONS(104),
    [sym_number] = ACTIONS(102),
    [sym_string] = ACTIONS(104),
    [sym_quoted_symbol] = ACTIONS(104),
    [sym_variable] = ACTIONS(104),
    [sym_directive] = ACTIONS(104),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [20] = {
    [aux_sym_row_key_repeat1] = STATE(21),
    [sym_symbol] = ACTIONS(108),
    [anon_sym_LPAREN] = ACTIONS(110),
    [anon_sym_RPAREN] = ACTIONS(110),
    [aux_sym_row_key_token2] = ACTIONS(106),
    [anon_sym_LBRACE] = ACTIONS(110),
    [anon_sym_LBRACK] = ACTIONS(110),
    [sym_number] = ACTIONS(108),
    [sym_string] = ACTIONS(110),
    [sym_quoted_symbol] = ACTIONS(110),
    [sym_variable] = ACTIONS(110),
    [sym_directive] = ACTIONS(110),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
  [21] = {
    [aux_sym_row_key_repeat1] = STATE(21),
    [sym_symbol] = ACTIONS(112),
    [anon_sym_LPAREN] = ACTIONS(114),
    [anon_sym_RPAREN] = ACTIONS(114),
    [aux_sym_row_key_token2] = ACTIONS(116),
    [anon_sym_LBRACE] = ACTIONS(114),
    [anon_sym_LBRACK] = ACTIONS(114),
    [sym_number] = ACTIONS(112),
    [sym_string] = ACTIONS(114),
    [sym_quoted_symbol] = ACTIONS(114),
    [sym_variable] = ACTIONS(114),
    [sym_directive] = ACTIONS(114),
    [sym_comment] = ACTIONS(3),
    [sym_block_comment] = ACTIONS(5),
  },
};

static const uint16_t ts_small_parse_table[] = {
  [0] = 4,
    ACTIONS(3), 1,
      sym_comment,
    ACTIONS(5), 1,
      sym_block_comment,
    ACTIONS(112), 2,
      sym_number,
      sym_symbol,
    ACTIONS(114), 9,
      anon_sym_LPAREN,
      anon_sym_RPAREN,
      aux_sym_row_key_token2,
      anon_sym_LBRACE,
      anon_sym_LBRACK,
      sym_string,
      sym_quoted_symbol,
      sym_variable,
      sym_directive,
  [22] = 2,
    ACTIONS(119), 1,
      ts_builtin_sym_end,
    ACTIONS(3), 2,
      sym_comment,
      sym_block_comment,
  [30] = 2,
    ACTIONS(121), 1,
      aux_sym_row_key_token3,
    ACTIONS(3), 2,
      sym_comment,
      sym_block_comment,
};

static const uint32_t ts_small_parse_table_map[] = {
  [SMALL_STATE(22)] = 0,
  [SMALL_STATE(23)] = 22,
  [SMALL_STATE(24)] = 30,
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
  [36] = {.entry = {.count = 1, .reusable = false}}, SHIFT(8),
  [38] = {.entry = {.count = 1, .reusable = true}}, SHIFT(12),
  [40] = {.entry = {.count = 1, .reusable = true}}, SHIFT(19),
  [42] = {.entry = {.count = 1, .reusable = true}}, SHIFT(8),
  [44] = {.entry = {.count = 1, .reusable = false}}, SHIFT(9),
  [46] = {.entry = {.count = 1, .reusable = true}}, SHIFT(13),
  [48] = {.entry = {.count = 1, .reusable = true}}, SHIFT(9),
  [50] = {.entry = {.count = 1, .reusable = false}}, SHIFT(11),
  [52] = {.entry = {.count = 1, .reusable = true}}, SHIFT(14),
  [54] = {.entry = {.count = 1, .reusable = true}}, SHIFT(11),
  [56] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_source_file, 1, 0, 0),
  [58] = {.entry = {.count = 1, .reusable = false}}, SHIFT(2),
  [60] = {.entry = {.count = 1, .reusable = true}}, SHIFT(2),
  [62] = {.entry = {.count = 1, .reusable = false}}, SHIFT(10),
  [64] = {.entry = {.count = 1, .reusable = true}}, SHIFT(17),
  [66] = {.entry = {.count = 1, .reusable = true}}, SHIFT(10),
  [68] = {.entry = {.count = 1, .reusable = true}}, SHIFT(18),
  [70] = {.entry = {.count = 1, .reusable = true}}, SHIFT(16),
  [72] = {.entry = {.count = 1, .reusable = true}}, SHIFT(15),
  [74] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_array, 2, 0, 0),
  [76] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_array, 2, 0, 0),
  [78] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_command, 2, 0, 0),
  [80] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_command, 2, 0, 0),
  [82] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_bracket, 2, 0, 0),
  [84] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_bracket, 2, 0, 0),
  [86] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_bracket, 3, 0, 0),
  [88] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_bracket, 3, 0, 0),
  [90] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_array, 4, 0, 0),
  [92] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_array, 4, 0, 0),
  [94] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_array, 3, 0, 0),
  [96] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_array, 3, 0, 0),
  [98] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_command, 3, 0, 0),
  [100] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_command, 3, 0, 0),
  [102] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_row_key, 1, 0, 0),
  [104] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_row_key, 1, 0, 0),
  [106] = {.entry = {.count = 1, .reusable = true}}, SHIFT(24),
  [108] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_row_key, 2, 0, 0),
  [110] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_row_key, 2, 0, 0),
  [112] = {.entry = {.count = 1, .reusable = false}}, REDUCE(aux_sym_row_key_repeat1, 2, 0, 0),
  [114] = {.entry = {.count = 1, .reusable = true}}, REDUCE(aux_sym_row_key_repeat1, 2, 0, 0),
  [116] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_row_key_repeat1, 2, 0, 0), SHIFT_REPEAT(24),
  [119] = {.entry = {.count = 1, .reusable = true}},  ACCEPT_INPUT(),
  [121] = {.entry = {.count = 1, .reusable = true}}, SHIFT(22),
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
