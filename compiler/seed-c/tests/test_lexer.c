#include "w_seed_lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "lexer check failed: %s (%s:%d)\n", #condition,  \
                    __FILE__, __LINE__);                                      \
      return false;                                                            \
    }                                                                           \
  } while (0)

typedef struct {
  w_seed_source source;
  w_seed_lexer lexer;
  w_seed_lexer_frame frames[64];
} fixture;

static bool fixture_init(fixture *fixture_value, const char *text) {
  w_seed_source_error source_error;
  const w_seed_byte_view bytes = {
      (const uint8_t *)text,
      strlen(text),
  };
  CHECK(w_seed_source_init(bytes, &fixture_value->source, &source_error));
  w_seed_lex_error lex_error;
  const w_seed_span bounds = {0, bytes.length};
  CHECK(w_seed_lexer_init(&fixture_value->source, bounds,
                         fixture_value->frames,
                         sizeof(fixture_value->frames) /
                             sizeof(fixture_value->frames[0]),
                         &fixture_value->lexer, &lex_error));
  return true;
}

static bool next_item(fixture *fixture_value, w_seed_lex_item *item) {
  w_seed_lex_error error;
  if (!w_seed_lexer_next(&fixture_value->lexer, item, &error)) {
    (void)fprintf(stderr, "unexpected lexer error %d at %llu\n", error.kind,
                  (unsigned long long)error.primary.start_byte);
    return false;
  }
  return true;
}

static bool test_prefix_and_crlf(void) {
  fixture fixture_value;
  CHECK(fixture_init(&fixture_value, "\xEF\xBB\xBFlet\r\nx"));
  w_seed_lex_item item;
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_SOURCE_PREFIX);
  CHECK(item.span.start_byte == 0 && item.span.end_byte == 3);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD);
  CHECK(item.span.start_byte == 3 && item.span.end_byte == 6);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(item.payload.trivia == W_SEED_TRIVIA_NEWLINE);
  CHECK(item.span.start_byte == 6 && item.span.end_byte == 8);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD);
  CHECK(item.span.start_byte == 8 && item.span.end_byte == 9);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_EOF);
  CHECK(item.span.start_byte == 9 && item.span.end_byte == 9);
  CHECK(w_seed_lexer_offset(&fixture_value.lexer) == 9);
  return true;
}

static bool test_numbers_and_relation(void) {
  fixture fixture_value;
  CHECK(fixture_init(&fixture_value,
                     "-0b1111_0000_u8 1.0e-9_f32 "
                     "9.80665<si.m/si.s^2> 1 < order 12km 2<"));
  w_seed_lex_item item;
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_PUNCTUATION);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_NUMBER);
  CHECK((item.payload.token.flags & W_SEED_NUMBER_RADIX) != 0);
  CHECK((item.payload.token.flags & W_SEED_NUMBER_SUFFIX) != 0);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_NUMBER);
  CHECK((item.payload.token.flags & W_SEED_NUMBER_FRACTION) != 0);
  CHECK((item.payload.token.flags & W_SEED_NUMBER_EXPONENT) != 0);
  CHECK((item.payload.token.flags & W_SEED_NUMBER_SUFFIX) != 0);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_NUMBER);
  CHECK((item.payload.token.flags & W_SEED_NUMBER_QUANTITY) != 0);
  CHECK(item.span.end_byte > item.span.start_byte + 10);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_NUMBER);
  CHECK((item.payload.token.flags & W_SEED_NUMBER_QUANTITY) == 0);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_PUNCTUATION);
  CHECK(item.span.end_byte == item.span.start_byte + 1);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_NUMBER);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_NUMBER);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_PUNCTUATION);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_EOF);

  CHECK(fixture_init(&fixture_value, "3<foo*2>"));
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_NUMBER &&
        (item.payload.token.flags & W_SEED_NUMBER_QUANTITY) == 0);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_PUNCTUATION);

  CHECK(fixture_init(&fixture_value, "7_weird 8_"));
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_NUMBER && item.span.start_byte == 0 &&
        item.span.end_byte == 7 &&
        (item.payload.token.flags & W_SEED_NUMBER_SUFFIX) != 0);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_NUMBER && item.span.start_byte == 8 &&
        item.span.end_byte == 9 &&
        (item.payload.token.flags & W_SEED_NUMBER_SUFFIX) == 0);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD && item.span.start_byte == 9 &&
        item.span.end_byte == 10);

  CHECK(fixture_init(&fixture_value, "++ -- -> :: .."));
  for (size_t index = 0; index < 14; index += 1) {
    CHECK(next_item(&fixture_value, &item));
    if ((index % 3u) != 2u) {
      CHECK(item.kind == W_SEED_LEX_ITEM_PUNCTUATION);
      CHECK(item.span.end_byte == item.span.start_byte + 1);
    }
  }
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_EOF);
  return true;
}

static bool test_longest_punctuation(void) {
  static const size_t lengths[] = {
      4, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2,
  };
  fixture fixture_value;
  CHECK(fixture_init(
      &fixture_value,
      ">..< ... ..< >.. **= >>= <<= ?. ?? => += -= *= /= %= &= ^= |= "
      "<< >> ** == != <= >= && ||"));
  w_seed_lex_item item;
  for (size_t index = 0; index < sizeof(lengths) / sizeof(lengths[0]);
       index += 1) {
    CHECK(next_item(&fixture_value, &item));
    CHECK(item.kind == W_SEED_LEX_ITEM_PUNCTUATION);
    CHECK(item.span.end_byte == item.span.start_byte + lengths[index]);
    if (index + 1 < sizeof(lengths) / sizeof(lengths[0])) {
      CHECK(next_item(&fixture_value, &item));
      CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA &&
            item.payload.trivia == W_SEED_TRIVIA_SPACE);
    }
  }
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_EOF);
  return true;
}

static bool test_literals_and_interpolation(void) {
  fixture fixture_value;
  CHECK(fixture_init(&fixture_value,
                     "\"hi ${name {x}} end\" #\"raw ${no}\"# "
                     "\"\"\"multi ${value}\"\"\" b\"bytes\" 'λ' b'A' "
                     "#\"raw\"# #\"\"\"multi\"\"\"#"));
  w_seed_lex_item item;
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_LITERAL_EVENT &&
        item.payload.literal.event == W_SEED_LITERAL_START &&
        item.payload.literal.literal == W_SEED_LITERAL_STRING);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.payload.literal.event == W_SEED_LITERAL_TEXT);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.payload.literal.event == W_SEED_INTERPOLATION_START);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD);
  CHECK(next_item(&fixture_value, &item));
  CHECK((item.kind == W_SEED_LEX_ITEM_TRIVIA &&
         item.payload.trivia == W_SEED_TRIVIA_SPACE) ||
        item.kind == W_SEED_LEX_ITEM_PUNCTUATION);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_PUNCTUATION);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_PUNCTUATION);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.payload.literal.event == W_SEED_INTERPOLATION_END);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.payload.literal.event == W_SEED_LITERAL_TEXT);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.payload.literal.event == W_SEED_LITERAL_END);

  size_t starts = 0;
  size_t ends = 0;
  bool saw_raw_interpolation = false;
  for (;;) {
    CHECK(next_item(&fixture_value, &item));
    if (item.kind == W_SEED_LEX_ITEM_EOF) break;
    if (item.kind == W_SEED_LEX_ITEM_LITERAL_EVENT) {
      if (item.payload.literal.event == W_SEED_LITERAL_START) starts += 1;
      if (item.payload.literal.event == W_SEED_LITERAL_END) ends += 1;
      if (item.payload.literal.event == W_SEED_INTERPOLATION_START &&
          (item.payload.literal.literal == W_SEED_LITERAL_RAW_STRING ||
           item.payload.literal.literal == W_SEED_LITERAL_BYTE_STRING)) {
        saw_raw_interpolation = true;
      }
    }
  }
  CHECK(starts == 7);
  CHECK(ends == 7);
  CHECK(!saw_raw_interpolation);
  return true;
}

static bool test_interpolation_brace_ownership(void) {
  fixture fixture_value;
  CHECK(fixture_init(
      &fixture_value,
      "\"start ${ \"}\" // } { line\n /* { nested /* } */ tail */ {root} } end\""));
  w_seed_lex_item item;
  bool saw_interpolation_start = false;
  bool saw_nested_string = false;
  bool saw_line_comment = false;
  bool saw_block_comment = false;
  bool saw_root_open = false;
  bool saw_root_close = false;
  bool saw_interpolation_end = false;
  for (;;) {
    CHECK(next_item(&fixture_value, &item));
    if (item.kind == W_SEED_LEX_ITEM_EOF) break;
    if (item.kind == W_SEED_LEX_ITEM_TRIVIA &&
        item.payload.trivia == W_SEED_TRIVIA_LINE_COMMENT) {
      saw_line_comment = true;
    }
    if (item.kind == W_SEED_LEX_ITEM_TRIVIA &&
        item.payload.trivia == W_SEED_TRIVIA_BLOCK_COMMENT) {
      saw_block_comment = true;
    }
    if (item.kind == W_SEED_LEX_ITEM_LITERAL_EVENT) {
      if (item.payload.literal.event == W_SEED_INTERPOLATION_START) {
        saw_interpolation_start = true;
      }
      if (item.payload.literal.event == W_SEED_LITERAL_START &&
          item.payload.literal.literal == W_SEED_LITERAL_STRING &&
          saw_interpolation_start) {
        saw_nested_string = true;
      }
      if (item.payload.literal.event == W_SEED_INTERPOLATION_END) {
        saw_interpolation_end = true;
        CHECK(saw_root_open && saw_root_close && saw_line_comment &&
              saw_block_comment && saw_nested_string);
      }
    }
    if (saw_interpolation_start && !saw_interpolation_end &&
        item.kind == W_SEED_LEX_ITEM_PUNCTUATION &&
        item.span.end_byte == item.span.start_byte + 1) {
      const uint8_t byte = fixture_value.source.bytes.data[item.span.start_byte];
      if (byte == (uint8_t)'{' && !saw_root_open) saw_root_open = true;
      if (byte == (uint8_t)'}' && saw_root_open) saw_root_close = true;
    }
  }
  CHECK(saw_interpolation_start && saw_nested_string && saw_block_comment &&
        saw_line_comment && saw_root_open && saw_root_close &&
        saw_interpolation_end);
  return true;
}

static bool test_comments_and_errors(void) {
  fixture fixture_value;
  CHECK(fixture_init(&fixture_value, "/* outer /* inner */ tail */ x // line\n"));
  w_seed_lex_item item;
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA &&
        item.payload.trivia == W_SEED_TRIVIA_BLOCK_COMMENT);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA &&
        item.payload.trivia == W_SEED_TRIVIA_SPACE);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA &&
        item.payload.trivia == W_SEED_TRIVIA_LINE_COMMENT);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_EOF);

  CHECK(fixture_init(&fixture_value, "\"unterminated"));
  CHECK(next_item(&fixture_value, &item));
  w_seed_lex_error error;
  CHECK(next_item(&fixture_value, &item));
  CHECK(!w_seed_lexer_next(&fixture_value.lexer, &item, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_UNTERMINATED_LITERAL);
  CHECK(error.opening.start_byte == 0 && error.opening.end_byte == 1);
  CHECK(error.reached_eof);

  CHECK(fixture_init(&fixture_value, "/* unterminated"));
  CHECK(!w_seed_lexer_next(&fixture_value.lexer, &item, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_UNTERMINATED_COMMENT);
  CHECK(error.opening.start_byte == 0 && error.opening.end_byte == 2);
  CHECK(error.reached_eof);

  CHECK(fixture_init(&fixture_value, "\"line\n\""));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(!w_seed_lexer_next(&fixture_value.lexer, &item, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_UNTERMINATED_LITERAL);
  CHECK(error.primary.start_byte == 5 && error.primary.end_byte == 6);
  CHECK(!error.reached_eof);

  CHECK(fixture_init(&fixture_value, "\"foo\r\n"));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(!w_seed_lexer_next(&fixture_value.lexer, &item, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_UNTERMINATED_LITERAL);
  CHECK(error.primary.start_byte == 4 && error.primary.end_byte == 6);
  CHECK(!error.reached_eof);

  CHECK(fixture_init(&fixture_value, "\"foo\\\n"));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(!w_seed_lexer_next(&fixture_value.lexer, &item, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_UNTERMINATED_LITERAL);
  CHECK(error.primary.start_byte == 4 && error.primary.end_byte == 6);
  CHECK(!error.reached_eof);
  return true;
}

static bool test_item_progress(void) {
  fixture fixture_value;
  CHECK(fixture_init(&fixture_value, "a + b\n"));
  w_seed_lex_item item;
  size_t previous = 0;
  while (true) {
    CHECK(next_item(&fixture_value, &item));
    if (item.kind == W_SEED_LEX_ITEM_EOF) {
      CHECK(item.span.start_byte == previous && item.span.end_byte == previous);
      break;
    }
    if (item.kind != W_SEED_LEX_ITEM_FOREIGN_BODY) {
      CHECK(item.span.end_byte > item.span.start_byte);
    }
    CHECK(item.span.start_byte == previous);
    previous = item.span.end_byte;
  }
  return true;
}

static bool test_stops_and_opaque(void) {
  fixture fixture_value;
  CHECK(fixture_init(&fixture_value, "let café = 1"));
  w_seed_lex_item item;
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  w_seed_lex_error error;
  CHECK(next_item(&fixture_value, &item));
  CHECK(!w_seed_lexer_next(&fixture_value.lexer, &item, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_UNSUPPORTED_UNICODE_IDENTIFIER);

  CHECK(fixture_init(&fixture_value, "x\ry"));
  CHECK(next_item(&fixture_value, &item));
  CHECK(!w_seed_lexer_next(&fixture_value.lexer, &item, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_UNSUPPORTED_CONTROL);

  CHECK(fixture_init(&fixture_value, "x\ay"));
  CHECK(next_item(&fixture_value, &item));
  CHECK(!w_seed_lexer_next(&fixture_value.lexer, &item, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_UNSUPPORTED_CONTROL);

  CHECK(fixture_init(&fixture_value, "fn<C>() { foreign }"));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA && item.span.end_byte == 10);
  CHECK(w_seed_lexer_require_opaque(&fixture_value.lexer, &error));
  CHECK(!w_seed_lexer_next(&fixture_value.lexer, &item, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_OPAQUE_UNCLAIMED);

  CHECK(fixture_init(&fixture_value, "fn<C>() { foreign }"));
  for (size_t index = 0; index < 9; index += 1) {
    CHECK(next_item(&fixture_value, &item));
  }
  const w_seed_span body = {10, 17};
  CHECK(w_seed_lexer_require_opaque(&fixture_value.lexer, &error));
  CHECK(w_seed_lexer_claim_opaque(&fixture_value.lexer, body, &error));
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_FOREIGN_BODY);
  CHECK(item.span.start_byte == 10 && item.span.end_byte == 17);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_PUNCTUATION);
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_EOF);
  return true;
}

static bool test_opaque_validation(void) {
  fixture fixture_value;
  w_seed_lex_item item;
  w_seed_lex_error error;

  CHECK(fixture_init(&fixture_value, "abc"));
  CHECK(!w_seed_lexer_claim_opaque(&fixture_value.lexer, (w_seed_span){0, 1},
                                   &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_OPAQUE_RANGE);
  CHECK(!w_seed_lexer_next(&fixture_value.lexer, &item, &error));

  CHECK(fixture_init(&fixture_value, "abc"));
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD);
  CHECK(!w_seed_lexer_require_opaque(&fixture_value.lexer, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_OPAQUE_RANGE);

  CHECK(fixture_init(&fixture_value, "a b"));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(w_seed_lexer_require_opaque(&fixture_value.lexer, &error));
  CHECK(!w_seed_lexer_claim_opaque(&fixture_value.lexer,
                                   (w_seed_span){1, 2}, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_OPAQUE_RANGE);
  CHECK(!w_seed_lexer_next(&fixture_value.lexer, &item, &error));

  CHECK(fixture_init(&fixture_value, "a b"));
  CHECK(next_item(&fixture_value, &item));
  CHECK(next_item(&fixture_value, &item));
  CHECK(w_seed_lexer_require_opaque(&fixture_value.lexer, &error));
  CHECK(!w_seed_lexer_claim_opaque(&fixture_value.lexer,
                                   (w_seed_span){2, 99}, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_OPAQUE_RANGE);

  CHECK(fixture_init(&fixture_value, "{}"));
  CHECK(next_item(&fixture_value, &item));
  CHECK(w_seed_lexer_require_opaque(&fixture_value.lexer, &error));
  CHECK(w_seed_lexer_claim_opaque(&fixture_value.lexer,
                                  (w_seed_span){1, 1}, &error));
  CHECK(next_item(&fixture_value, &item));
  CHECK(item.kind == W_SEED_LEX_ITEM_FOREIGN_BODY && item.span.start_byte == 1 &&
        item.span.end_byte == 1);
  CHECK(!w_seed_lexer_claim_opaque(&fixture_value.lexer,
                                   (w_seed_span){1, 1}, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_OPAQUE_RANGE);
  return true;
}

int main(void) {
  const bool passed = test_prefix_and_crlf() && test_numbers_and_relation() &&
                      test_literals_and_interpolation() &&
                      test_longest_punctuation() &&
                      test_interpolation_brace_ownership() &&
                      test_comments_and_errors() && test_item_progress() &&
                      test_stops_and_opaque() && test_opaque_validation();
  if (!passed) return EXIT_FAILURE;
  (void)puts("Seed C lexer: adversarial lossless probes passed.");
  return EXIT_SUCCESS;
}
