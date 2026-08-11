#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>

enum TokenType {
  FOREIGN_BODY_CONTENT,
  FOREIGN_BODY_ERROR_SENTINEL,
};

enum ScanState {
  SCAN_NORMAL,
  SCAN_LINE_COMMENT,
  SCAN_BLOCK_COMMENT,
  SCAN_SINGLE_QUOTE,
  SCAN_DOUBLE_QUOTE,
  SCAN_DIRECTIVE,
};

void *tree_sitter_w_external_scanner_create(void) {
  return NULL;
}

void tree_sitter_w_external_scanner_destroy(void *payload) {
  (void)payload;
}

unsigned tree_sitter_w_external_scanner_serialize(
    void *payload,
    char *buffer
) {
  (void)payload;
  (void)buffer;
  return 0;
}

void tree_sitter_w_external_scanner_deserialize(
    void *payload,
    const char *buffer,
    unsigned length
) {
  (void)payload;
  (void)buffer;
  (void)length;
}

static void advance(TSLexer *lexer) {
  lexer->advance(lexer, false);
}

static bool is_horizontal_space(int32_t codepoint) {
  return codepoint == ' ' || codepoint == '\t' || codepoint == '\v' ||
         codepoint == '\f';
}

static void advance_escape(TSLexer *lexer) {
  advance(lexer);
  if (lexer->eof(lexer)) {
    return;
  }
  if (lexer->lookahead == '\r') {
    advance(lexer);
    if (lexer->lookahead == '\n') {
      advance(lexer);
    }
    return;
  }
  advance(lexer);
}

bool tree_sitter_w_external_scanner_scan(
    void *payload,
    TSLexer *lexer,
    const bool *valid_symbols
) {
  (void)payload;

  if (!valid_symbols[FOREIGN_BODY_CONTENT] ||
      valid_symbols[FOREIGN_BODY_ERROR_SENTINEL]) {
    return false;
  }

  if (lexer->eof(lexer) || lexer->lookahead == '}') {
    return false;
  }

  enum ScanState state = SCAN_NORMAL;
  uint32_t brace_depth = 0;
  bool at_line_start = true;
  bool consumed = false;

  while (!lexer->eof(lexer)) {
    const int32_t codepoint = lexer->lookahead;

    if (state == SCAN_LINE_COMMENT || state == SCAN_DIRECTIVE) {
      if (codepoint == '\\') {
        consumed = true;
        advance_escape(lexer);
        continue;
      }
      if (codepoint == '\r' || codepoint == '\n') {
        state = SCAN_NORMAL;
        at_line_start = true;
      }
      consumed = true;
      advance(lexer);
      continue;
    }

    if (state == SCAN_BLOCK_COMMENT) {
      consumed = true;
      if (codepoint == '*') {
        advance(lexer);
        if (lexer->lookahead == '/') {
          advance(lexer);
          state = SCAN_NORMAL;
        }
      } else {
        if (codepoint == '\r' || codepoint == '\n') {
          at_line_start = true;
        }
        advance(lexer);
      }
      continue;
    }

    if (state == SCAN_SINGLE_QUOTE || state == SCAN_DOUBLE_QUOTE) {
      const int32_t delimiter =
          state == SCAN_SINGLE_QUOTE ? '\'' : '"';
      consumed = true;
      if (codepoint == '\\') {
        advance_escape(lexer);
        continue;
      }
      advance(lexer);
      if (codepoint == delimiter) {
        state = SCAN_NORMAL;
      }
      continue;
    }

    if (codepoint == '}' && brace_depth == 0) {
      lexer->mark_end(lexer);
      lexer->result_symbol = FOREIGN_BODY_CONTENT;
      return consumed;
    }

    consumed = true;

    if (codepoint == '<') {
      at_line_start = false;
      advance(lexer);
      if (lexer->lookahead == '%') {
        brace_depth += 1;
        advance(lexer);
      }
      continue;
    }

    if (codepoint == '%') {
      const bool directive_start = at_line_start;
      at_line_start = false;
      advance(lexer);
      if (lexer->lookahead == '>') {
        if (brace_depth > 0) {
          brace_depth -= 1;
        }
        advance(lexer);
      } else if (lexer->lookahead == ':' && directive_start) {
        advance(lexer);
        state = SCAN_DIRECTIVE;
      }
      continue;
    }

    if (codepoint == '/') {
      at_line_start = false;
      advance(lexer);
      if (lexer->lookahead == '/') {
        advance(lexer);
        state = SCAN_LINE_COMMENT;
      } else if (lexer->lookahead == '*') {
        advance(lexer);
        state = SCAN_BLOCK_COMMENT;
      }
      continue;
    }

    if (codepoint == '"') {
      at_line_start = false;
      state = SCAN_DOUBLE_QUOTE;
      advance(lexer);
      continue;
    }

    if (codepoint == '\'') {
      at_line_start = false;
      state = SCAN_SINGLE_QUOTE;
      advance(lexer);
      continue;
    }

    if (codepoint == '#' && at_line_start) {
      state = SCAN_DIRECTIVE;
      advance(lexer);
      continue;
    }

    if (codepoint == '{') {
      brace_depth += 1;
    } else if (codepoint == '}') {
      brace_depth -= 1;
    }

    if (codepoint == '\r' || codepoint == '\n') {
      at_line_start = true;
    } else if (!is_horizontal_space(codepoint)) {
      at_line_start = false;
    }

    advance(lexer);
  }

  if (consumed) {
    lexer->mark_end(lexer);
    lexer->result_symbol = FOREIGN_BODY_CONTENT;
  }
  return consumed;
}
