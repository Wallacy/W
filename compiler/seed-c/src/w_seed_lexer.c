#include "w_seed_lexer.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "w_seed_unicode.h"

static void clear_error(w_seed_lex_error *error) {
  if (error == NULL) return;
  error->kind = W_SEED_LEX_ERROR_NONE;
  error->primary.start_byte = 0;
  error->primary.end_byte = 0;
  error->opening.start_byte = 0;
  error->opening.end_byte = 0;
  error->literal = W_SEED_LITERAL_NONE;
  error->code_point = 0;
  error->reached_eof = false;
}

static bool fail_error(w_seed_lexer *lexer, w_seed_lex_error *error,
                       w_seed_lex_error_kind kind, w_seed_span primary,
                       w_seed_span opening, w_seed_literal_kind literal,
                       bool reached_eof) {
  if (error != NULL) {
    error->kind = kind;
    error->primary = primary;
    error->opening = opening;
    error->literal = literal;
    error->reached_eof = reached_eof;
  }
  if (lexer != NULL) lexer->terminal = true;
  return false;
}

static bool fail_simple(w_seed_lexer *lexer, w_seed_lex_error *error,
                        w_seed_lex_error_kind kind, size_t offset) {
  const w_seed_span span = {offset, offset};
  return fail_error(lexer, error, kind, span, span, W_SEED_LITERAL_NONE,
                    false);
}

static bool source_is_ready(const w_seed_source *source) {
  return source != NULL &&
         (source->bytes.length == 0 || source->bytes.data != NULL);
}

static bool span_within(const w_seed_lexer *lexer, w_seed_span span) {
  if (lexer == NULL) return false;
  return span.start_byte <= span.end_byte &&
         span.start_byte >= lexer->bounds.start_byte &&
         span.end_byte <= lexer->bounds.end_byte;
}

static uint8_t byte_at(const w_seed_lexer *lexer, size_t offset) {
  return lexer->source->bytes.data[offset];
}

static bool is_ascii_letter(uint8_t byte) {
  return (byte >= (uint8_t)'a' && byte <= (uint8_t)'z') ||
         (byte >= (uint8_t)'A' && byte <= (uint8_t)'Z');
}

static bool is_ascii_digit(uint8_t byte) {
  return byte >= (uint8_t)'0' && byte <= (uint8_t)'9';
}

static bool is_ascii_word_start(uint8_t byte) {
  return is_ascii_letter(byte) || byte == (uint8_t)'_';
}

static bool is_ascii_word_continue(uint8_t byte) {
  return is_ascii_word_start(byte) || is_ascii_digit(byte);
}

static bool is_hex_digit(uint8_t byte) {
  return is_ascii_digit(byte) ||
         (byte >= (uint8_t)'a' && byte <= (uint8_t)'f') ||
         (byte >= (uint8_t)'A' && byte <= (uint8_t)'F');
}

static bool is_oct_digit(uint8_t byte) {
  return byte >= (uint8_t)'0' && byte <= (uint8_t)'7';
}

static bool is_bin_digit(uint8_t byte) {
  return byte == (uint8_t)'0' || byte == (uint8_t)'1';
}

static w_seed_span make_span(size_t start, size_t end) {
  const w_seed_span span = {start, end};
  return span;
}

static void clear_item(w_seed_lex_item *item) {
  (void)memset(item, 0, sizeof(*item));
  item->kind = W_SEED_LEX_ITEM_EOF;
}

static bool push_frame(w_seed_lexer *lexer, const w_seed_lexer_frame *frame,
                       w_seed_lex_error *error, size_t offset) {
  if (lexer->frame_count >= lexer->frame_capacity) {
    return fail_simple(lexer, error, W_SEED_LEX_ERROR_FRAME_LIMIT, offset);
  }
  lexer->frames[lexer->frame_count] = *frame;
  lexer->frame_count += 1;
  return true;
}

static void pop_frame(w_seed_lexer *lexer) {
  if (lexer->frame_count != 0) lexer->frame_count -= 1;
}

static w_seed_lexer_frame *top_frame(w_seed_lexer *lexer) {
  if (lexer->frame_count == 0) return NULL;
  return &lexer->frames[lexer->frame_count - 1];
}

static bool emit_simple(w_seed_lex_item *item, w_seed_lex_item_kind kind,
                        w_seed_span span) {
  item->kind = kind;
  item->span = span;
  return true;
}

static bool emit_trivia(w_seed_lex_item *item, w_seed_trivia_kind trivia,
                        w_seed_span span) {
  item->kind = W_SEED_LEX_ITEM_TRIVIA;
  item->span = span;
  item->payload.trivia = trivia;
  return true;
}

static bool emit_token(w_seed_lex_item *item, w_seed_lex_item_kind kind,
                       uint32_t flags, w_seed_span span) {
  item->kind = kind;
  item->span = span;
  item->payload.token.flags = flags;
  return true;
}

static bool emit_literal(w_seed_lex_item *item,
                         w_seed_literal_event_kind event,
                         w_seed_literal_kind literal, w_seed_span span) {
  item->kind = W_SEED_LEX_ITEM_LITERAL_EVENT;
  item->span = span;
  item->payload.literal.event = event;
  item->payload.literal.literal = literal;
  return true;
}

static bool match_bytes(const w_seed_lexer *lexer, size_t offset,
                        const char *text, size_t length) {
  if (length > lexer->bounds.end_byte - offset) return false;
  for (size_t index = 0; index < length; index += 1) {
    if (byte_at(lexer, offset + index) != (uint8_t)text[index]) return false;
  }
  return true;
}

static size_t utf8_width(const w_seed_lexer *lexer, size_t offset) {
  const uint8_t first = byte_at(lexer, offset);
  if (first < 0x80u) return 1;
  if (first < 0xE0u) return 2;
  if (first < 0xF0u) return 3;
  return 4;
}

static uint32_t decode_utf8_code_point(const w_seed_lexer *lexer, size_t offset,
                                       size_t *width) {
  const uint8_t first = byte_at(lexer, offset);
  if (first < 0x80u) {
    *width = 1;
    return (uint32_t)first;
  }
  if (first < 0xE0u) {
    *width = 2;
    return (((uint32_t)first & UINT32_C(0x1F)) << 6) |
           ((uint32_t)byte_at(lexer, offset + 1) & UINT32_C(0x3F));
  }
  if (first < 0xF0u) {
    *width = 3;
    return (((uint32_t)first & UINT32_C(0x0F)) << 12) |
           (((uint32_t)byte_at(lexer, offset + 1) & UINT32_C(0x3F)) << 6) |
           ((uint32_t)byte_at(lexer, offset + 2) & UINT32_C(0x3F));
  }
  *width = 4;
  return (((uint32_t)first & UINT32_C(0x07)) << 18) |
         (((uint32_t)byte_at(lexer, offset + 1) & UINT32_C(0x3F)) << 12) |
         (((uint32_t)byte_at(lexer, offset + 2) & UINT32_C(0x3F)) << 6) |
         ((uint32_t)byte_at(lexer, offset + 3) & UINT32_C(0x3F));
}

static bool emit_disallowed_identifier_code_point(w_seed_lexer *lexer,
                                                  w_seed_lex_error *error,
                                                  uint32_t code_point,
                                                  size_t width) {
  const size_t available = lexer->bounds.end_byte - lexer->cursor;
  const size_t used = width < available ? width : available;
  const w_seed_span span = make_span(lexer->cursor, lexer->cursor + used);
  const bool result = fail_error(
      lexer, error, W_SEED_LEX_ERROR_DISALLOWED_IDENTIFIER_CODE_POINT, span,
      span, W_SEED_LITERAL_NONE, false);
  if (error != NULL) error->code_point = code_point;
  return result;
}

static bool emit_control_error(w_seed_lexer *lexer, w_seed_lex_error *error) {
  return fail_simple(lexer, error, W_SEED_LEX_ERROR_UNSUPPORTED_CONTROL,
                     lexer->cursor);
}

static bool scan_space_or_newline(w_seed_lexer *lexer, w_seed_lex_item *item,
                                  w_seed_lex_error *error) {
  const size_t start = lexer->cursor;
  const uint8_t first = byte_at(lexer, start);
  if (first == (uint8_t)'\r') {
    if (start + 1 < lexer->bounds.end_byte &&
        byte_at(lexer, start + 1) == (uint8_t)'\n') {
      lexer->cursor += 2;
      return emit_trivia(item, W_SEED_TRIVIA_NEWLINE,
                         make_span(start, lexer->cursor));
    }
    return emit_control_error(lexer, error);
  }
  if (first == (uint8_t)'\n') {
    lexer->cursor += 1;
    return emit_trivia(item, W_SEED_TRIVIA_NEWLINE,
                       make_span(start, lexer->cursor));
  }
  while (lexer->cursor < lexer->bounds.end_byte) {
    const uint8_t byte = byte_at(lexer, lexer->cursor);
    if (byte != (uint8_t)' ' && byte != (uint8_t)'\t') {
      break;
    }
    lexer->cursor += 1;
  }
  return emit_trivia(item, W_SEED_TRIVIA_SPACE,
                     make_span(start, lexer->cursor));
}

static bool scan_line_comment(w_seed_lexer *lexer, w_seed_lex_item *item) {
  const size_t start = lexer->cursor;
  lexer->cursor += 2;
  while (lexer->cursor < lexer->bounds.end_byte) {
    const uint8_t byte = byte_at(lexer, lexer->cursor);
    if (byte == (uint8_t)'\n' || byte == (uint8_t)'\r') break;
    lexer->cursor += 1;
  }
  return emit_trivia(item, W_SEED_TRIVIA_LINE_COMMENT,
                     make_span(start, lexer->cursor));
}

static bool scan_block_comment(w_seed_lexer *lexer, w_seed_lex_item *item,
                               w_seed_lex_error *error) {
  const size_t start = lexer->cursor;
  size_t depth = 1;
  lexer->cursor += 2;
  while (lexer->cursor < lexer->bounds.end_byte) {
    if (match_bytes(lexer, lexer->cursor, "/*", 2)) {
      if (depth == SIZE_MAX) {
        return fail_simple(lexer, error, W_SEED_LEX_ERROR_FRAME_LIMIT,
                           lexer->cursor);
      }
      depth += 1;
      lexer->cursor += 2;
      continue;
    }
    if (match_bytes(lexer, lexer->cursor, "*/", 2)) {
      depth -= 1;
      lexer->cursor += 2;
      if (depth == 0) {
        return emit_trivia(item, W_SEED_TRIVIA_BLOCK_COMMENT,
                           make_span(start, lexer->cursor));
      }
      continue;
    }
    lexer->cursor += 1;
  }
  return fail_error(lexer, error, W_SEED_LEX_ERROR_UNTERMINATED_COMMENT,
                    make_span(start, lexer->cursor), make_span(start, start + 2),
                    W_SEED_LITERAL_NONE,
                    lexer->bounds.end_byte == lexer->source->bytes.length);
}

static size_t consume_digits(const w_seed_lexer *lexer, size_t offset,
                             bool (*predicate)(uint8_t)) {
  size_t cursor = offset;
  bool previous_digit = false;
  while (cursor < lexer->bounds.end_byte) {
    const uint8_t byte = byte_at(lexer, cursor);
    if (predicate(byte)) {
      previous_digit = true;
      cursor += 1;
      continue;
    }
    if (byte == (uint8_t)'_' && previous_digit &&
        cursor + 1 < lexer->bounds.end_byte &&
        predicate(byte_at(lexer, cursor + 1))) {
      previous_digit = false;
      cursor += 1;
      continue;
    }
    break;
  }
  return cursor;
}

static bool unit_name(const w_seed_lexer *lexer, size_t *cursor,
                      bool *saw_name) {
  size_t at = *cursor;
  if (!is_ascii_word_start(byte_at(lexer, at))) return false;
  while (true) {
    at += 1;
    while (at < lexer->bounds.end_byte &&
           is_ascii_word_continue(byte_at(lexer, at))) {
      at += 1;
    }
    if (at >= lexer->bounds.end_byte || byte_at(lexer, at) != (uint8_t)'.') {
      break;
    }
    if (at + 1 >= lexer->bounds.end_byte ||
        !is_ascii_word_start(byte_at(lexer, at + 1))) {
      return false;
    }
    at += 1;
  }
  *cursor = at;
  *saw_name = true;
  return true;
}

static bool try_quantity(const w_seed_lexer *lexer, size_t start,
                         size_t *end) {
  if (start >= lexer->bounds.end_byte || byte_at(lexer, start) != (uint8_t)'<') {
    return false;
  }
  size_t cursor = start + 1;
  size_t depth = 0;
  bool expect_operand = true;
  bool exponent = false;
  bool exponent_sign = false;
  bool saw_name = false;
  bool saw_numerator = false;
  while (cursor < lexer->bounds.end_byte) {
    const uint8_t byte = byte_at(lexer, cursor);
    if (byte == (uint8_t)' ' || byte == (uint8_t)'\t') {
      cursor += 1;
      continue;
    }
    if (byte == (uint8_t)'>') {
      if (depth == 0 && !expect_operand && saw_name) {
        *end = cursor + 1;
        return true;
      }
      return false;
    }
    if (byte == (uint8_t)'(') {
      if (!expect_operand || depth == SIZE_MAX) return false;
      depth += 1;
      cursor += 1;
      exponent = false;
      exponent_sign = false;
      continue;
    }
    if (byte == (uint8_t)')') {
      if (depth == 0 || expect_operand) return false;
      depth -= 1;
      cursor += 1;
      exponent = false;
      exponent_sign = false;
      continue;
    }
    if (byte == (uint8_t)'*' || byte == (uint8_t)'/') {
      if (expect_operand) return false;
      expect_operand = true;
      exponent = false;
      exponent_sign = false;
      cursor += 1;
      continue;
    }
    if (byte == (uint8_t)'^') {
      if (expect_operand) return false;
      expect_operand = true;
      exponent = true;
      exponent_sign = false;
      cursor += 1;
      continue;
    }
    if (exponent && !exponent_sign &&
        (byte == (uint8_t)'+' || byte == (uint8_t)'-')) {
      exponent_sign = true;
      cursor += 1;
      continue;
    }
    if (exponent && is_ascii_digit(byte)) {
      cursor = consume_digits(lexer, cursor, is_ascii_digit);
      expect_operand = false;
      exponent = false;
      exponent_sign = false;
      continue;
    }
    if (!exponent && is_ascii_word_start(byte)) {
      if (!unit_name(lexer, &cursor, &saw_name)) return false;
      expect_operand = false;
      continue;
    }
    if (!exponent && is_ascii_digit(byte) && expect_operand && !saw_name &&
        !saw_numerator && byte == (uint8_t)'1' &&
        (cursor + 1 >= lexer->bounds.end_byte ||
         !is_ascii_word_continue(byte_at(lexer, cursor + 1)))) {
      cursor += 1;
      saw_numerator = true;
      expect_operand = false;
      continue;
    }
    return false;
  }
  return false;
}

static bool scan_number(w_seed_lexer *lexer, w_seed_lex_item *item) {
  const size_t start = lexer->cursor;
  size_t cursor = start;
  uint32_t flags = 0;
  bool radix = false;
  if (byte_at(lexer, cursor) == (uint8_t)'0' && cursor + 1 < lexer->bounds.end_byte) {
    const uint8_t marker = byte_at(lexer, cursor + 1);
    if (marker == (uint8_t)'b' || marker == (uint8_t)'B') {
      radix = true;
      flags |= W_SEED_NUMBER_RADIX;
      cursor = consume_digits(lexer, cursor + 2, is_bin_digit);
    } else if (marker == (uint8_t)'o' || marker == (uint8_t)'O') {
      radix = true;
      flags |= W_SEED_NUMBER_RADIX;
      cursor = consume_digits(lexer, cursor + 2, is_oct_digit);
    } else if (marker == (uint8_t)'x' || marker == (uint8_t)'X') {
      radix = true;
      flags |= W_SEED_NUMBER_RADIX;
      cursor = consume_digits(lexer, cursor + 2, is_hex_digit);
    }
  }
  if (!radix) {
    cursor = consume_digits(lexer, cursor, is_ascii_digit);
    if (cursor + 1 < lexer->bounds.end_byte &&
        byte_at(lexer, cursor) == (uint8_t)'.' &&
        is_ascii_digit(byte_at(lexer, cursor + 1))) {
      flags |= W_SEED_NUMBER_FRACTION;
      cursor = consume_digits(lexer, cursor + 2, is_ascii_digit);
    }
    if (cursor < lexer->bounds.end_byte &&
        (byte_at(lexer, cursor) == (uint8_t)'e' ||
         byte_at(lexer, cursor) == (uint8_t)'E')) {
      size_t exponent_start = cursor + 1;
      if (exponent_start < lexer->bounds.end_byte &&
          (byte_at(lexer, exponent_start) == (uint8_t)'+' ||
           byte_at(lexer, exponent_start) == (uint8_t)'-')) {
        exponent_start += 1;
      }
      if (exponent_start < lexer->bounds.end_byte &&
          is_ascii_digit(byte_at(lexer, exponent_start))) {
        flags |= W_SEED_NUMBER_EXPONENT;
        cursor = consume_digits(lexer, exponent_start, is_ascii_digit);
      }
    }
  }
  if (cursor + 1 < lexer->bounds.end_byte &&
      byte_at(lexer, cursor) == (uint8_t)'_' &&
      is_ascii_letter(byte_at(lexer, cursor + 1))) {
    flags |= W_SEED_NUMBER_SUFFIX;
    cursor += 2;
    while (cursor < lexer->bounds.end_byte &&
           is_ascii_word_continue(byte_at(lexer, cursor))) {
      cursor += 1;
    }
  }
  if (cursor < lexer->bounds.end_byte && byte_at(lexer, cursor) == (uint8_t)'<') {
    size_t quantity_end = 0;
    if (try_quantity(lexer, cursor, &quantity_end)) {
      flags |= W_SEED_NUMBER_QUANTITY;
      cursor = quantity_end;
    }
  }
  lexer->cursor = cursor;
  return emit_token(item, W_SEED_LEX_ITEM_NUMBER, flags,
                    make_span(start, lexer->cursor));
}

static bool scan_word(w_seed_lexer *lexer, w_seed_lex_item *item) {
  const size_t start = lexer->cursor;
  if (byte_at(lexer, lexer->cursor) < 0x80u) {
    lexer->cursor += 1;
  } else {
    size_t width = 0;
    (void)decode_utf8_code_point(lexer, lexer->cursor, &width);
    lexer->cursor += width;
  }
  while (lexer->cursor < lexer->bounds.end_byte) {
    const uint8_t byte = byte_at(lexer, lexer->cursor);
    if (byte < 0x80u) {
      if (!is_ascii_word_continue(byte)) break;
      lexer->cursor += 1;
      continue;
    }
    size_t width = 0;
    const uint32_t code_point =
        decode_utf8_code_point(lexer, lexer->cursor, &width);
    if (!w_seed_unicode_is_identifier_continue(code_point)) break;
    lexer->cursor += width;
  }
  return emit_token(item, W_SEED_LEX_ITEM_WORD, 0,
                    make_span(start, lexer->cursor));
}

static bool punctuation_byte(uint8_t byte) {
  switch (byte) {
    case (uint8_t)'(':
    case (uint8_t)')':
    case (uint8_t)'[':
    case (uint8_t)']':
    case (uint8_t)'{':
    case (uint8_t)'}':
    case (uint8_t)',':
    case (uint8_t)';':
    case (uint8_t)':':
    case (uint8_t)'.':
    case (uint8_t)'?':
    case (uint8_t)'!':
    case (uint8_t)'~':
    case (uint8_t)'@':
    case (uint8_t)'#':
    case (uint8_t)'$':
    case (uint8_t)'%':
    case (uint8_t)'&':
    case (uint8_t)'*':
    case (uint8_t)'+':
    case (uint8_t)'-':
    case (uint8_t)'/':
    case (uint8_t)'<':
    case (uint8_t)'=':
    case (uint8_t)'>':
    case (uint8_t)'^':
    case (uint8_t)'|':
      return true;
    default:
      return false;
  }
}

static bool scan_punctuation(w_seed_lexer *lexer, w_seed_lex_item *item) {
  static const char *const operators[] = {
      ">..<", "...", "..<", ">..", "**=", "<<=", ">>=", "?.", "??",
      "=>",   "==",  "!=",  "<=",  ">=",  "&&",  "||",  "|>", "+=", "-=",
      "*=",   "/=",  "%=",  "&=",  "^=",  "|=",  "**",  "<<", ">>",
  };
  const size_t start = lexer->cursor;
  for (size_t index = 0; index < sizeof(operators) / sizeof(operators[0]);
       index += 1) {
    const size_t length = strlen(operators[index]);
    if (match_bytes(lexer, start, operators[index], length)) {
      lexer->cursor += length;
      return emit_token(item, W_SEED_LEX_ITEM_PUNCTUATION, 0,
                        make_span(start, lexer->cursor));
    }
  }
  lexer->cursor += 1;
  return emit_token(item, W_SEED_LEX_ITEM_PUNCTUATION, 0,
                    make_span(start, lexer->cursor));
}

static bool scan_literal_start(w_seed_lexer *lexer, w_seed_lex_item *item,
                               w_seed_lex_error *error) {
  const size_t start = lexer->cursor;
  w_seed_literal_kind literal = W_SEED_LITERAL_NONE;
  size_t opener_length = 0;
  size_t close_length = 0;
  bool allows_interpolation = false;
  bool raw = false;
  if (match_bytes(lexer, start, "#\"\"\"", 4)) {
    literal = W_SEED_LITERAL_RAW_MULTILINE_STRING;
    opener_length = 4;
    close_length = 4;
    raw = true;
  } else if (match_bytes(lexer, start, "#\"", 2)) {
    literal = W_SEED_LITERAL_RAW_STRING;
    opener_length = 2;
    close_length = 2;
    raw = true;
  } else if (match_bytes(lexer, start, "\"\"\"", 3)) {
    literal = W_SEED_LITERAL_MULTILINE_STRING;
    opener_length = 3;
    close_length = 3;
    allows_interpolation = true;
  } else if (match_bytes(lexer, start, "b\"", 2)) {
    literal = W_SEED_LITERAL_BYTE_STRING;
    opener_length = 2;
    close_length = 1;
  } else if (match_bytes(lexer, start, "b'", 2)) {
    literal = W_SEED_LITERAL_BYTE_SCALAR;
    opener_length = 2;
    close_length = 1;
  } else if (match_bytes(lexer, start, "\"", 1)) {
    literal = W_SEED_LITERAL_STRING;
    opener_length = 1;
    close_length = 1;
    allows_interpolation = true;
  } else if (match_bytes(lexer, start, "'", 1)) {
    literal = W_SEED_LITERAL_SCALAR;
    opener_length = 1;
    close_length = 1;
  } else {
    return false;
  }

  if (lexer->frame_count >= lexer->frame_capacity) {
    return fail_simple(lexer, error, W_SEED_LEX_ERROR_FRAME_LIMIT, start);
  }
  const w_seed_lexer_frame frame = {
      W_SEED_LEXER_FRAME_LITERAL,
      literal,
      make_span(start, start + opener_length),
      close_length,
      0,
      allows_interpolation,
      raw,
  };
  lexer->cursor += opener_length;
  if (!push_frame(lexer, &frame, error, start)) return false;
  return emit_literal(item, W_SEED_LITERAL_START, literal,
                      make_span(start, lexer->cursor));
}

static bool scan_root_item(w_seed_lexer *lexer, w_seed_lex_item *item,
                           w_seed_lex_error *error) {
  const uint8_t byte = byte_at(lexer, lexer->cursor);
  if (byte == (uint8_t)'\r' || byte == (uint8_t)'\n' || byte == (uint8_t)' ' ||
      byte == (uint8_t)'\t') {
    return scan_space_or_newline(lexer, item, error);
  }
  if (match_bytes(lexer, lexer->cursor, "//", 2)) {
    return scan_line_comment(lexer, item);
  }
  if (match_bytes(lexer, lexer->cursor, "/*", 2)) {
    return scan_block_comment(lexer, item, error);
  }
  if (scan_literal_start(lexer, item, error)) return true;
  if (lexer->terminal) return false;
  if (is_ascii_digit(byte)) return scan_number(lexer, item);
  if (is_ascii_word_start(byte)) return scan_word(lexer, item);
  if (byte >= 0x80u) {
    size_t width = 0;
    const uint32_t code_point =
        decode_utf8_code_point(lexer, lexer->cursor, &width);
    if (w_seed_unicode_is_identifier_start(code_point)) {
      return scan_word(lexer, item);
    }
    return emit_disallowed_identifier_code_point(lexer, error, code_point,
                                                 width);
  }
  if (byte < 0x20u || byte == 0x7Fu) {
    return emit_control_error(lexer, error);
  }
  if (punctuation_byte(byte)) return scan_punctuation(lexer, item);
  lexer->cursor += 1;
  return emit_simple(item, W_SEED_LEX_ITEM_UNKNOWN,
                     make_span(lexer->cursor - 1, lexer->cursor));
}

static size_t single_line_boundary_length(const w_seed_lexer *lexer,
                                          size_t offset) {
  if (offset >= lexer->bounds.end_byte) return 0;
  const uint8_t byte = byte_at(lexer, offset);
  size_t newline_length = 0;
  if (byte == (uint8_t)'\r') {
    newline_length = offset + 1 < lexer->bounds.end_byte &&
                             byte_at(lexer, offset + 1) == (uint8_t)'\n'
                         ? 2
                         : 1;
  } else if (byte == (uint8_t)'\n') {
    newline_length = 1;
  }
  if (newline_length != 0) return newline_length;
  if (byte == (uint8_t)'\\' && offset + 1 < lexer->bounds.end_byte) {
    const uint8_t next = byte_at(lexer, offset + 1);
    if (next == (uint8_t)'\r') {
      return 1 + (offset + 2 < lexer->bounds.end_byte &&
                          byte_at(lexer, offset + 2) == (uint8_t)'\n'
                      ? 2
                      : 1);
    }
    if (next == (uint8_t)'\n') return 2;
  }
  return 0;
}

static bool scan_literal_frame(w_seed_lexer *lexer, w_seed_lex_item *item,
                               w_seed_lex_error *error) {
  w_seed_lexer_frame *frame = top_frame(lexer);
  if (frame == NULL) return false;
  if (lexer->cursor >= lexer->bounds.end_byte) {
    return fail_error(lexer, error, W_SEED_LEX_ERROR_UNTERMINATED_LITERAL,
                      make_span(lexer->cursor, lexer->cursor), frame->opening,
                      frame->literal,
                      lexer->bounds.end_byte == lexer->source->bytes.length);
  }
  if (frame->allows_interpolation &&
      match_bytes(lexer, lexer->cursor, "${", 2)) {
    if (lexer->frame_count >= lexer->frame_capacity) {
      return fail_simple(lexer, error, W_SEED_LEX_ERROR_FRAME_LIMIT,
                         lexer->cursor);
    }
    const w_seed_span span = make_span(lexer->cursor, lexer->cursor + 2);
    const w_seed_literal_kind literal = frame->literal;
    lexer->cursor += 2;
    const w_seed_lexer_frame interpolation = {
        W_SEED_LEXER_FRAME_INTERPOLATION,
        literal,
        span,
        0,
        1,
        false,
        false,
    };
    if (!push_frame(lexer, &interpolation, error, span.start_byte)) return false;
    return emit_literal(item, W_SEED_INTERPOLATION_START, literal, span);
  }
  const char *close = NULL;
  if (frame->literal == W_SEED_LITERAL_RAW_STRING) close = "\"#";
  if (frame->literal == W_SEED_LITERAL_RAW_MULTILINE_STRING) close = "\"\"\"#";
  if (close == NULL) {
    if (frame->literal == W_SEED_LITERAL_MULTILINE_STRING) close = "\"\"\"";
    else close = "\"";
    if (frame->literal == W_SEED_LITERAL_SCALAR ||
        frame->literal == W_SEED_LITERAL_BYTE_SCALAR) close = "'";
  }
  const size_t close_length = strlen(close);
  const bool multiline = frame->literal == W_SEED_LITERAL_MULTILINE_STRING ||
                         frame->literal == W_SEED_LITERAL_RAW_MULTILINE_STRING;
  const size_t boundary_length =
      multiline ? 0 : single_line_boundary_length(lexer, lexer->cursor);
  if (boundary_length != 0) {
    return fail_error(lexer, error, W_SEED_LEX_ERROR_UNTERMINATED_LITERAL,
                      make_span(lexer->cursor,
                                lexer->cursor + boundary_length),
                      frame->opening, frame->literal, false);
  }
  if (match_bytes(lexer, lexer->cursor, close, close_length)) {
    const size_t start = lexer->cursor;
    const w_seed_literal_kind literal = frame->literal;
    lexer->cursor += close_length;
    pop_frame(lexer);
    return emit_literal(item, W_SEED_LITERAL_END, literal,
                        make_span(start, lexer->cursor));
  }

  const size_t start = lexer->cursor;
  while (lexer->cursor < lexer->bounds.end_byte) {
    if (!multiline &&
        single_line_boundary_length(lexer, lexer->cursor) != 0) {
      break;
    }
    if (frame->allows_interpolation &&
        match_bytes(lexer, lexer->cursor, "${", 2)) break;
    if (match_bytes(lexer, lexer->cursor, close, close_length)) break;
    if (!frame->raw && byte_at(lexer, lexer->cursor) == (uint8_t)'\\') {
      lexer->cursor += 1;
      if (lexer->cursor < lexer->bounds.end_byte) {
        const size_t width = utf8_width(lexer, lexer->cursor);
        const size_t remaining = lexer->bounds.end_byte - lexer->cursor;
        lexer->cursor += width < remaining ? width : remaining;
      }
      continue;
    }
    if (byte_at(lexer, lexer->cursor) >= 0x80u) {
      const size_t width = utf8_width(lexer, lexer->cursor);
      const size_t remaining = lexer->bounds.end_byte - lexer->cursor;
      lexer->cursor += width < remaining ? width : remaining;
    } else {
      lexer->cursor += 1;
    }
  }
  if (lexer->cursor != start) {
    return emit_literal(item, W_SEED_LITERAL_TEXT, frame->literal,
                        make_span(start, lexer->cursor));
  }
  return fail_error(lexer, error, W_SEED_LEX_ERROR_UNTERMINATED_LITERAL,
                    make_span(lexer->cursor, lexer->cursor), frame->opening,
                    frame->literal,
                    lexer->bounds.end_byte == lexer->source->bytes.length);
}

static bool scan_interpolation_frame(w_seed_lexer *lexer, w_seed_lex_item *item,
                                     w_seed_lex_error *error) {
  w_seed_lexer_frame *frame = top_frame(lexer);
  if (frame == NULL) return false;
  if (lexer->cursor >= lexer->bounds.end_byte) {
    w_seed_span opening = frame->opening;
    w_seed_literal_kind literal = frame->literal;
    for (size_t index = lexer->frame_count; index > 0; index -= 1) {
      const w_seed_lexer_frame *candidate = &lexer->frames[index - 1];
      if (candidate->kind == W_SEED_LEXER_FRAME_LITERAL) {
        opening = candidate->opening;
        literal = candidate->literal;
        break;
      }
    }
    return fail_error(lexer, error, W_SEED_LEX_ERROR_UNTERMINATED_LITERAL,
                      make_span(lexer->cursor, lexer->cursor), opening, literal,
                      lexer->bounds.end_byte == lexer->source->bytes.length);
  }
  const uint8_t byte = byte_at(lexer, lexer->cursor);
  if (byte == (uint8_t)'}') {
    if (frame->brace_depth == 1) {
      const size_t start = lexer->cursor;
      const w_seed_literal_kind literal = frame->literal;
      lexer->cursor += 1;
      pop_frame(lexer);
      return emit_literal(item, W_SEED_INTERPOLATION_END, literal,
                          make_span(start, lexer->cursor));
    }
    frame->brace_depth -= 1;
    return scan_punctuation(lexer, item);
  }
  if (byte == (uint8_t)'{') {
    if (frame->brace_depth == SIZE_MAX) {
      return fail_simple(lexer, error, W_SEED_LEX_ERROR_FRAME_LIMIT,
                         lexer->cursor);
    }
    frame->brace_depth += 1;
    return scan_punctuation(lexer, item);
  }
  return scan_root_item(lexer, item, error);
}

bool w_seed_lexer_init(const w_seed_source *source, w_seed_span bounds,
                       w_seed_lexer_frame *frames, size_t frame_capacity,
                       w_seed_lexer *lexer, w_seed_lex_error *error) {
  clear_error(error);
  if (lexer == NULL || source == NULL ||
      (frame_capacity != 0 && frames == NULL)) {
    return fail_simple(NULL, error, W_SEED_LEX_ERROR_NULL_ARGUMENT, 0);
  }
  (void)memset(lexer, 0, sizeof(*lexer));
  if (!source_is_ready(source)) {
    return fail_simple(NULL, error, W_SEED_LEX_ERROR_NULL_ARGUMENT, 0);
  }
  w_seed_source_error source_error;
  if (!w_seed_source_validate_span(source, bounds, &source_error)) {
    const w_seed_span span = make_span(source_error.byte_offset,
                                       source_error.byte_offset);
    return fail_error(NULL, error, W_SEED_LEX_ERROR_BAD_SPAN, span, span,
                      W_SEED_LITERAL_NONE, false);
  }
  lexer->source = source;
  lexer->bounds = bounds;
  lexer->cursor = bounds.start_byte;
  lexer->frames = frames;
  lexer->frame_capacity = frame_capacity;
  lexer->source_prefix_pending = bounds.start_byte == 0 &&
                                 source->bom_length == 3 &&
                                 bounds.end_byte >= 3;
  return true;
}

bool w_seed_lexer_require_opaque(w_seed_lexer *lexer,
                                 w_seed_lex_error *error) {
  clear_error(error);
  if (lexer == NULL) {
    return fail_simple(NULL, error, W_SEED_LEX_ERROR_NULL_ARGUMENT, 0);
  }
  if (lexer->terminal || lexer->opaque_required || lexer->frame_count != 0 ||
      lexer->source_prefix_pending || lexer->cursor >= lexer->bounds.end_byte) {
    return fail_simple(lexer, error, W_SEED_LEX_ERROR_OPAQUE_RANGE,
                       lexer->cursor);
  }
  lexer->opaque_required = true;
  lexer->opaque_claimed = false;
  lexer->opaque_span = make_span(lexer->cursor, lexer->cursor);
  return true;
}

bool w_seed_lexer_claim_opaque(w_seed_lexer *lexer, w_seed_span body,
                               w_seed_lex_error *error) {
  clear_error(error);
  if (lexer == NULL) {
    return fail_simple(NULL, error, W_SEED_LEX_ERROR_NULL_ARGUMENT, 0);
  }
  if (lexer->terminal || !lexer->opaque_required || lexer->opaque_claimed ||
      body.start_byte != lexer->cursor || !span_within(lexer, body)) {
    return fail_simple(lexer, error, W_SEED_LEX_ERROR_OPAQUE_RANGE,
                       lexer->cursor);
  }
  w_seed_source_error source_error;
  if (!w_seed_source_validate_span(lexer->source, body, &source_error)) {
    return fail_simple(lexer, error, W_SEED_LEX_ERROR_OPAQUE_RANGE,
                       source_error.byte_offset);
  }
  lexer->opaque_span = body;
  lexer->opaque_claimed = true;
  return true;
}

bool w_seed_lexer_next(w_seed_lexer *lexer, w_seed_lex_item *item,
                       w_seed_lex_error *error) {
  clear_error(error);
  if (lexer == NULL || item == NULL || lexer->source == NULL) {
    return fail_simple(NULL, error, W_SEED_LEX_ERROR_NULL_ARGUMENT, 0);
  }
  clear_item(item);
  if (lexer->terminal) return false;
  if (lexer->opaque_required && !lexer->opaque_claimed) {
    return fail_simple(lexer, error, W_SEED_LEX_ERROR_OPAQUE_UNCLAIMED,
                       lexer->cursor);
  }
  if (lexer->opaque_claimed) {
    item->kind = W_SEED_LEX_ITEM_FOREIGN_BODY;
    item->span = lexer->opaque_span;
    lexer->cursor = lexer->opaque_span.end_byte;
    lexer->opaque_required = false;
    lexer->opaque_claimed = false;
    return true;
  }
  if (lexer->source_prefix_pending) {
    const w_seed_span span = make_span(lexer->cursor, lexer->cursor + 3);
    lexer->cursor += 3;
    lexer->source_prefix_pending = false;
    item->kind = W_SEED_LEX_ITEM_SOURCE_PREFIX;
    item->span = span;
    item->payload.source_prefix = W_SEED_SOURCE_PREFIX_UTF8_BOM;
    return true;
  }
  if (lexer->frame_count != 0) {
    w_seed_lexer_frame *frame = top_frame(lexer);
    if (frame->kind == W_SEED_LEXER_FRAME_LITERAL) {
      return scan_literal_frame(lexer, item, error);
    }
    return scan_interpolation_frame(lexer, item, error);
  }
  if (lexer->cursor >= lexer->bounds.end_byte) {
    item->span = make_span(lexer->cursor, lexer->cursor);
    return true;
  }
  return scan_root_item(lexer, item, error);
}

size_t w_seed_lexer_offset(const w_seed_lexer *lexer) {
  return lexer == NULL ? 0 : lexer->cursor;
}
