#include "w_seed_lexer.h"
#include "w_seed_unicode.h"

#include <stdio.h>
#include <stdint.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "unicode check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                      \
      return false;                                                            \
    }                                                                           \
  } while (0)

typedef struct {
  w_seed_source source;
  w_seed_lexer lexer;
  w_seed_lexer_frame frames[32];
} fixture;

static bool fixture_init(fixture *fixture_value, const uint8_t *bytes,
                         size_t length) {
  w_seed_source_error source_error;
  const w_seed_byte_view view = {bytes, length};
  CHECK(w_seed_source_init(view, &fixture_value->source, &source_error));
  w_seed_lex_error lex_error;
  const w_seed_span bounds = {0, length};
  CHECK(w_seed_lexer_init(&fixture_value->source, bounds,
                         fixture_value->frames,
                         sizeof(fixture_value->frames) /
                             sizeof(fixture_value->frames[0]),
                         &fixture_value->lexer, &lex_error));
  return true;
}

static bool next_item(fixture *fixture_value, w_seed_lex_item *item,
                      w_seed_lex_error *error) {
  if (!w_seed_lexer_next(&fixture_value->lexer, item, error)) {
    (void)fprintf(stderr, "unexpected unicode lexer error %d at %llu\n",
                  error->kind, (unsigned long long)error->primary.start_byte);
    return false;
  }
  return true;
}

static bool test_classifier_ranges(void) {
  CHECK(w_seed_unicode_xid_start_range_count == 691u);
  CHECK(w_seed_unicode_xid_continue_range_count == 806u);
  CHECK(w_seed_unicode_default_ignorable_range_count == 17u);
  CHECK(w_seed_unicode_xid_start_ranges[0].start == UINT32_C(0x41));
  CHECK(w_seed_unicode_xid_start_ranges[0].end == UINT32_C(0x5A));
  CHECK(w_seed_unicode_xid_continue_ranges[0].start == UINT32_C(0x30));
  CHECK(w_seed_unicode_default_ignorable_ranges[0].start ==
        UINT32_C(0x00AD));

  for (size_t index = 0; index < w_seed_unicode_xid_start_range_count;
       index += 1) {
    const w_seed_unicode_range range = w_seed_unicode_xid_start_ranges[index];
    CHECK(range.start <= range.end);
    CHECK(w_seed_unicode_is_identifier_start(range.start) ||
          w_seed_unicode_is_default_ignorable(range.start));
    CHECK(w_seed_unicode_is_identifier_start(range.end) ||
          w_seed_unicode_is_default_ignorable(range.end));
    if (range.start != 0) {
      if (!w_seed_unicode_is_default_ignorable(range.start - 1)) {
        CHECK(!w_seed_unicode_is_identifier_start(range.start - 1));
      }
    }
    if (range.end != UINT32_C(0x10FFFF)) {
      if (!w_seed_unicode_is_default_ignorable(range.end + 1)) {
        CHECK(!w_seed_unicode_is_identifier_start(range.end + 1));
      }
    }
  }
  for (size_t index = 0; index < w_seed_unicode_xid_continue_range_count;
       index += 1) {
    const w_seed_unicode_range range =
        w_seed_unicode_xid_continue_ranges[index];
    CHECK(range.start <= range.end);
    CHECK(w_seed_unicode_is_identifier_continue(range.start) ||
          w_seed_unicode_is_default_ignorable(range.start));
    CHECK(w_seed_unicode_is_identifier_continue(range.end) ||
          w_seed_unicode_is_default_ignorable(range.end));
    if (range.start != 0) {
      if (!w_seed_unicode_is_default_ignorable(range.start - 1)) {
        CHECK(!w_seed_unicode_is_identifier_continue(range.start - 1));
      }
    }
    if (range.end != UINT32_C(0x10FFFF)) {
      if (!w_seed_unicode_is_default_ignorable(range.end + 1)) {
        CHECK(!w_seed_unicode_is_identifier_continue(range.end + 1));
      }
    }
  }

  CHECK(w_seed_unicode_is_identifier_start(UINT32_C(0x5F)));
  CHECK(w_seed_unicode_is_identifier_continue(UINT32_C(0x5F)));
  CHECK(!w_seed_unicode_is_identifier_start(UINT32_C(0x30)));
  CHECK(w_seed_unicode_is_identifier_continue(UINT32_C(0x30)));
  CHECK(!w_seed_unicode_is_identifier_start(UINT32_C(0x301)));
  CHECK(w_seed_unicode_is_identifier_continue(UINT32_C(0x301)));
  CHECK(!w_seed_unicode_is_identifier_start(UINT32_C(0x20DD)));
  CHECK(!w_seed_unicode_is_identifier_continue(UINT32_C(0x20DD)));

  CHECK(w_seed_unicode_is_identifier_start(UINT32_C(0x03B1)));
  CHECK(w_seed_unicode_is_identifier_start(UINT32_C(0x0430)));
  CHECK(w_seed_unicode_is_identifier_start(UINT32_C(0x0627)));
  CHECK(w_seed_unicode_is_identifier_start(UINT32_C(0x4E2D)));
  CHECK(w_seed_unicode_is_identifier_start(UINT32_C(0x0905)));
  CHECK(w_seed_unicode_is_identifier_start(UINT32_C(0x10400)));

  CHECK(w_seed_unicode_is_default_ignorable(UINT32_C(0x200C)));
  CHECK(w_seed_unicode_is_default_ignorable(UINT32_C(0x200D)));
  CHECK(w_seed_unicode_is_default_ignorable(UINT32_C(0xFE0F)));
  CHECK(w_seed_unicode_is_default_ignorable(UINT32_C(0x202E)));
  CHECK(w_seed_unicode_is_default_ignorable(UINT32_C(0xFEFF)));
  CHECK(!w_seed_unicode_is_identifier_continue(UINT32_C(0x200C)));
  CHECK(!w_seed_unicode_is_identifier_continue(UINT32_C(0x200D)));
  CHECK(!w_seed_unicode_is_identifier_continue(UINT32_C(0xFE0F)));
  CHECK(!w_seed_unicode_is_identifier_continue(UINT32_C(0x202E)));
  CHECK(!w_seed_unicode_is_identifier_continue(UINT32_C(0xFEFF)));
  CHECK(!w_seed_unicode_is_default_ignorable(UINT32_C(0x1F600)));
  CHECK(!w_seed_unicode_is_identifier_start(UINT32_C(0x1F600)));
  CHECK(!w_seed_unicode_is_identifier_continue(UINT32_C(0x1F600)));
  CHECK(!w_seed_unicode_is_identifier_start(UINT32_C(0x10FFFF)));
  CHECK(!w_seed_unicode_is_identifier_continue(UINT32_C(0x110000)));
  return true;
}

static bool test_raw_composed_and_decomposed(void) {
  static const uint8_t composed[] = {
      'l', 'e', 't', ' ', 'c', 'a', 'f', 0xC3, 0xA9, ' ', '=', ' ', '1',
  };
  fixture fixture_value;
  CHECK(fixture_init(&fixture_value, composed, sizeof(composed)));
  w_seed_lex_item item;
  w_seed_lex_error error;
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD && item.span.start_byte == 0 &&
        item.span.end_byte == 3);
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD && item.span.start_byte == 4 &&
        item.span.end_byte == 9);
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_PUNCTUATION);
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_TRIVIA);
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_NUMBER);
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_EOF);

  static const uint8_t decomposed[] = {
      'c', 'a', 'f', 'e', 0xCC, 0x81,
  };
  CHECK(fixture_init(&fixture_value, decomposed, sizeof(decomposed)));
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD && item.span.start_byte == 0 &&
        item.span.end_byte == sizeof(decomposed));
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_EOF);
  return true;
}

static bool test_scripts_and_boundaries(void) {
  static const uint8_t scripts[] = {
      0xCE, 0xB1, /* Greek alpha */
      0xD0, 0xB0, /* Cyrillic a */
      0xD8, 0xA7, /* Arabic alef */
      0xE4, 0xB8, 0xAD, /* Han */
      0xE0, 0xA4, 0x85, /* Devanagari a */
      0xF0, 0x90, 0x90, 0x80, /* Deseret */
      '2',
  };
  fixture fixture_value;
  CHECK(fixture_init(&fixture_value, scripts, sizeof(scripts)));
  w_seed_lex_item item;
  w_seed_lex_error error;
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD && item.span.start_byte == 0 &&
        item.span.end_byte == sizeof(scripts));
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_EOF);

  static const uint8_t mixed_confusable[] = {
      'p', 'a', 'y', 'p', 'a', 'l', 0xD1, 0x80, /* Cyrillic er */
  };
  CHECK(fixture_init(&fixture_value, mixed_confusable,
                     sizeof(mixed_confusable)));
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD &&
        item.span.end_byte == sizeof(mixed_confusable));
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_EOF);

  static const uint8_t digit_start[] = {'2', 'f', 'o', 'o'};
  CHECK(fixture_init(&fixture_value, digit_start, sizeof(digit_start)));
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_NUMBER);
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD);
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_EOF);

  static const uint8_t underscore[] = {'_', 'f', 'o', 'o', '_', '2'};
  CHECK(fixture_init(&fixture_value, underscore, sizeof(underscore)));
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_WORD &&
        item.span.end_byte == sizeof(underscore));
  CHECK(next_item(&fixture_value, &item, &error));
  CHECK(item.kind == W_SEED_LEX_ITEM_EOF);
  return true;
}

static bool expect_unicode_stop(const uint8_t *bytes, size_t length,
                                size_t start, size_t end,
                                uint32_t code_point) {
  fixture fixture_value;
  CHECK(fixture_init(&fixture_value, bytes, length));
  w_seed_lex_item item;
  w_seed_lex_error error;
  if (start != 0) CHECK(next_item(&fixture_value, &item, &error));
  CHECK(!w_seed_lexer_next(&fixture_value.lexer, &item, &error));
  CHECK(error.kind == W_SEED_LEX_ERROR_DISALLOWED_IDENTIFIER_CODE_POINT);
  CHECK(error.primary.start_byte == start && error.primary.end_byte == end);
  CHECK(error.code_point == code_point);
  return true;
}

static bool test_rejected_ignorables_and_marks(void) {
  static const uint8_t combining_start[] = {0xCC, 0x81};
  CHECK(expect_unicode_stop(combining_start, sizeof(combining_start), 0, 2,
                            UINT32_C(0x301)));

  static const uint8_t zwnj[] = {'a', 0xE2, 0x80, 0x8C, 'b'};
  CHECK(expect_unicode_stop(zwnj, sizeof(zwnj), 1, 4, UINT32_C(0x200C)));
  static const uint8_t zwj[] = {'a', 0xE2, 0x80, 0x8D, 'b'};
  CHECK(expect_unicode_stop(zwj, sizeof(zwj), 1, 4, UINT32_C(0x200D)));
  static const uint8_t variation_selector[] = {'a', 0xEF, 0xB8, 0x8F};
  CHECK(expect_unicode_stop(variation_selector, sizeof(variation_selector), 1,
                            4, UINT32_C(0xFE0F)));
  static const uint8_t bidi_control[] = {'a', 0xE2, 0x80, 0xAE, 'b'};
  CHECK(expect_unicode_stop(bidi_control, sizeof(bidi_control), 1, 4,
                            UINT32_C(0x202E)));
  static const uint8_t internal_bom[] = {'a', 0xEF, 0xBB, 0xBF};
  CHECK(expect_unicode_stop(internal_bom, sizeof(internal_bom), 1, 4,
                            UINT32_C(0xFEFF)));
  static const uint8_t emoji[] = {'a', 0xF0, 0x9F, 0x98, 0x80, 'b'};
  CHECK(expect_unicode_stop(emoji, sizeof(emoji), 1, 5,
                            UINT32_C(0x1F600)));
  return true;
}

static bool test_comments_strings_and_invalid_source(void) {
  static const uint8_t source_bytes[] = {
      '/', '/', ' ', 'c', 'a', 'f', 0xC3, 0xA9, '\n',
      '"', 'c', 'a', 'f', 0xC3, 0xA9, '"',
  };
  fixture fixture_value;
  CHECK(fixture_init(&fixture_value, source_bytes, sizeof(source_bytes)));
  w_seed_lex_item item;
  w_seed_lex_error error;
  bool saw_comment = false;
  bool saw_literal_text = false;
  for (;;) {
    CHECK(next_item(&fixture_value, &item, &error));
    if (item.kind == W_SEED_LEX_ITEM_EOF) break;
    if (item.kind == W_SEED_LEX_ITEM_TRIVIA &&
        item.payload.trivia == W_SEED_TRIVIA_LINE_COMMENT) {
      saw_comment = true;
    }
    if (item.kind == W_SEED_LEX_ITEM_LITERAL_EVENT &&
        item.payload.literal.event == W_SEED_LITERAL_TEXT) {
      saw_literal_text = true;
    }
  }
  CHECK(saw_comment && saw_literal_text);

  static const uint8_t invalid_utf8[] = {0xC3};
  w_seed_source source;
  w_seed_source_error source_error;
  CHECK(!w_seed_source_init((w_seed_byte_view){invalid_utf8,
                                               sizeof(invalid_utf8)},
                            &source, &source_error));
  CHECK(source_error.kind == W_SEED_SOURCE_ERROR_UTF8_TRUNCATED);
  return true;
}

int main(void) {
  const bool passed = test_classifier_ranges() &&
                      test_raw_composed_and_decomposed() &&
                      test_scripts_and_boundaries() &&
                      test_rejected_ignorables_and_marks() &&
                      test_comments_strings_and_invalid_source();
  if (!passed) return 1;
  (void)puts("Seed C Unicode classifier: adversarial ranges and raw lexer words passed.");
  return 0;
}
