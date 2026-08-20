#include "w_seed_source.h"

#include <stdio.h>
#include <string.h>

static unsigned failures = 0;

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
      failures += 1;                                                            \
    }                                                                           \
  } while (0)

static void expect_error(const uint8_t *bytes, size_t length,
                         w_seed_source_error_kind kind, size_t offset) {
  w_seed_source source = {0};
  w_seed_source_error error = {0};
  const w_seed_byte_view view = {bytes, length};
  CHECK(!w_seed_source_init(view, &source, &error));
  CHECK(error.kind == kind);
  CHECK(error.byte_offset == offset);
}

static void test_valid_source_and_slices(void) {
  static const uint8_t bytes[] = {'A', '\n', 'B', '\r', '\n'};
  const w_seed_byte_view view = {bytes, sizeof(bytes)};
  w_seed_source source = {0};
  w_seed_source_error error = {0};
  CHECK(w_seed_source_init(view, &source, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NONE);
  CHECK(w_seed_source_bytes(&source).data == bytes);
  CHECK(w_seed_source_bytes(&source).length == sizeof(bytes));
  CHECK(!w_seed_source_has_bom(&source));
  CHECK(w_seed_source_line_count(&source) == 3);

  const w_seed_span span = {2, 3};
  w_seed_byte_view slice = {0};
  CHECK(w_seed_source_slice(&source, span, &slice, &error));
  CHECK(slice.data == bytes + 2);
  CHECK(slice.length == 1);
  CHECK(slice.data[0] == 'B');

  CHECK(w_seed_source_slice(&source, (w_seed_span){2, 2}, &slice, &error));
  CHECK(slice.data == NULL);
  CHECK(slice.length == 0);
}

static void test_empty_source_and_slice(void) {
  const w_seed_byte_view view = {NULL, 0};
  w_seed_source source = {0};
  w_seed_source_error error = {0};
  CHECK(w_seed_source_init(view, &source, &error));
  CHECK(w_seed_source_line_count(&source) == 1);
  w_seed_byte_view slice = {NULL, 0};
  CHECK(w_seed_source_slice(&source, (w_seed_span){0, 0}, &slice, &error));
  CHECK(slice.data == NULL);
  CHECK(slice.length == 0);
  w_seed_source_point point = {0};
  CHECK(w_seed_source_offset_to_point(&source, 0, &point, &error));
  CHECK(point.line == 0 && point.byte_column == 0);
}

static void test_bom_is_preserved(void) {
  static const uint8_t bytes[] = {0xEFu, 0xBBu, 0xBFu, 'A'};
  const w_seed_byte_view view = {bytes, sizeof(bytes)};
  w_seed_source source = {0};
  w_seed_source_error error = {0};
  CHECK(w_seed_source_init(view, &source, &error));
  CHECK(w_seed_source_has_bom(&source));
  CHECK(source.bom_length == 3);
  w_seed_byte_view slice = {0};
  CHECK(w_seed_source_slice(&source, (w_seed_span){0, 3}, &slice, &error));
  CHECK(slice.data == bytes);
  CHECK(slice.length == 3);
  CHECK(memcmp(slice.data, bytes, slice.length) == 0);
}

static void test_spans_and_boundaries(void) {
  static const uint8_t bytes[] = {
      'A',
      0xC2u, 0xA9u,                         /* U+00A9: two bytes. */
      0xE2u, 0x82u, 0xACu,                  /* U+20AC: three bytes. */
      0xF0u, 0x9Fu, 0x98u, 0x80u,          /* U+1F600: four bytes. */
      'Z',
  };
  const w_seed_byte_view view = {bytes, sizeof(bytes)};
  w_seed_source source = {0};
  w_seed_source_error error = {0};
  CHECK(w_seed_source_init(view, &source, &error));
  CHECK(w_seed_source_validate_span(&source, (w_seed_span){1, 3}, &error));
  CHECK(w_seed_source_validate_span(&source, (w_seed_span){3, 6}, &error));
  CHECK(w_seed_source_validate_span(&source, (w_seed_span){6, 10}, &error));
  CHECK(!w_seed_source_validate_span(&source, (w_seed_span){10, 1}, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_SPAN_ORDER);
  CHECK(!w_seed_source_validate_span(&source, (w_seed_span){0, 12}, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_SPAN_OUT_OF_RANGE);
  CHECK(!w_seed_source_validate_span(&source, (w_seed_span){2, 3}, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NOT_CODE_POINT_BOUNDARY);
  CHECK(error.byte_offset == 2);
  CHECK(!w_seed_source_validate_span(&source, (w_seed_span){4, 6}, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NOT_CODE_POINT_BOUNDARY);
  CHECK(error.byte_offset == 4);
  CHECK(!w_seed_source_validate_span(&source, (w_seed_span){7, 10}, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NOT_CODE_POINT_BOUNDARY);
  CHECK(error.byte_offset == 7);
}

static void test_points(void) {
  static const uint8_t bytes[] = {'A', '\n', 0xC3u, 0xA9u, '\r', '\n', 'Z'};
  const w_seed_byte_view view = {bytes, sizeof(bytes)};
  w_seed_source source = {0};
  w_seed_source_error error = {0};
  CHECK(w_seed_source_init(view, &source, &error));
  CHECK(w_seed_source_line_count(&source) == 3);

  w_seed_source_point point = {0};
  CHECK(w_seed_source_offset_to_point(&source, 0, &point, &error));
  CHECK(point.line == 0 && point.byte_column == 0);
  CHECK(w_seed_source_offset_to_point(&source, 2, &point, &error));
  CHECK(point.line == 1 && point.byte_column == 0);
  CHECK(w_seed_source_offset_to_point(&source, 5, &point, &error));
  CHECK(point.line == 1 && point.byte_column == 3);
  CHECK(w_seed_source_offset_to_point(&source, sizeof(bytes), &point, &error));
  CHECK(point.line == 2 && point.byte_column == 1);

  size_t offset = 0;
  CHECK(w_seed_source_point_to_offset(&source, (w_seed_source_point){1, 2},
                                      &offset, &error));
  CHECK(offset == 4);
  CHECK(w_seed_source_point_to_offset(&source, (w_seed_source_point){1, 3},
                                      &offset, &error));
  CHECK(offset == 5);
  CHECK(w_seed_source_point_to_offset(&source, (w_seed_source_point){2, 1},
                                      &offset, &error));
  CHECK(offset == sizeof(bytes));

  CHECK(!w_seed_source_offset_to_point(&source, 3, &point, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NOT_CODE_POINT_BOUNDARY);
  CHECK(!w_seed_source_point_to_offset(&source, (w_seed_source_point){1, 1},
                                       &offset, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NOT_CODE_POINT_BOUNDARY);
  CHECK(!w_seed_source_point_to_offset(&source, (w_seed_source_point){3, 0},
                                       &offset, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_LINE_OUT_OF_RANGE);
  CHECK(error.byte_offset == sizeof(bytes));
  CHECK(!w_seed_source_point_to_offset(
      &source, (w_seed_source_point){0, SIZE_MAX}, &offset, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_COLUMN_OUT_OF_RANGE);
  CHECK(error.byte_offset == 1);
  CHECK(!w_seed_source_offset_to_point(&source, sizeof(bytes) + 1, &point,
                                       &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_OFFSET_OUT_OF_RANGE);
}

static void test_utf8_errors(void) {
  static const uint8_t stray[] = {0x80u};
  static const uint8_t overlong_lead[] = {0xC0u, 0x80u};
  static const uint8_t overlong_three[] = {0xE0u, 0x80u, 0x80u};
  static const uint8_t surrogate[] = {0xEDu, 0xA0u, 0x80u};
  static const uint8_t out_of_range[] = {0xF4u, 0x90u, 0x80u, 0x80u};
  static const uint8_t truncated[] = {0xE2u, 0x82u};
  static const uint8_t bad_continuation[] = {0xE2u, 0x28u, 0xA1u};
  static const uint8_t invalid_lead[] = {0xF8u};
  expect_error(stray, sizeof(stray),
               W_SEED_SOURCE_ERROR_UTF8_STRAY_CONTINUATION, 0);
  expect_error(overlong_lead, sizeof(overlong_lead),
               W_SEED_SOURCE_ERROR_UTF8_OVERLONG, 0);
  expect_error(overlong_three, sizeof(overlong_three),
               W_SEED_SOURCE_ERROR_UTF8_OVERLONG, 1);
  expect_error(surrogate, sizeof(surrogate),
               W_SEED_SOURCE_ERROR_UTF8_SURROGATE, 1);
  expect_error(out_of_range, sizeof(out_of_range),
               W_SEED_SOURCE_ERROR_UTF8_OUT_OF_RANGE, 1);
  expect_error(truncated, sizeof(truncated),
               W_SEED_SOURCE_ERROR_UTF8_TRUNCATED, 0);
  expect_error(bad_continuation, sizeof(bad_continuation),
               W_SEED_SOURCE_ERROR_UTF8_BAD_CONTINUATION, 1);
  expect_error(invalid_lead, sizeof(invalid_lead),
               W_SEED_SOURCE_ERROR_INVALID_UTF8_LEAD, 0);
}

static void test_null_arguments(void) {
  w_seed_source source = {0};
  w_seed_source_error error = {0};
  const w_seed_span span = {0, 0};
  w_seed_byte_view slice = {NULL, 0};
  w_seed_source_point point = {0, 0};
  size_t offset = 0;
  CHECK(!w_seed_source_init((w_seed_byte_view){NULL, 1}, &source, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NULL_ARGUMENT);
  CHECK(!w_seed_source_init((w_seed_byte_view){NULL, 0}, NULL, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NULL_ARGUMENT);
  CHECK(w_seed_source_bytes(NULL).data == NULL);
  CHECK(w_seed_source_bytes(NULL).length == 0);
  CHECK(!w_seed_source_has_bom(NULL));
  CHECK(w_seed_source_line_count(NULL) == 0);

  CHECK(!w_seed_source_validate_span(NULL, span, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NULL_ARGUMENT);
  CHECK(!w_seed_source_slice(NULL, span, &slice, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NULL_ARGUMENT);
  CHECK(!w_seed_source_slice(NULL, span, NULL, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NULL_ARGUMENT);
  CHECK(!w_seed_source_offset_to_point(NULL, 0, &point, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NULL_ARGUMENT);
  CHECK(!w_seed_source_offset_to_point(NULL, 0, NULL, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NULL_ARGUMENT);
  CHECK(!w_seed_source_point_to_offset(NULL, point, &offset, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NULL_ARGUMENT);
  CHECK(!w_seed_source_point_to_offset(NULL, point, NULL, &error));
  CHECK(error.kind == W_SEED_SOURCE_ERROR_NULL_ARGUMENT);
}

int main(void) {
  test_valid_source_and_slices();
  test_empty_source_and_slice();
  test_bom_is_preserved();
  test_spans_and_boundaries();
  test_points();
  test_utf8_errors();
  test_null_arguments();
  if (failures != 0) {
    (void)fprintf(stderr, "%u source-reader test(s) failed\n", failures);
    return 1;
  }
  (void)puts("w_seed_source_tests: ok");
  return 0;
}
