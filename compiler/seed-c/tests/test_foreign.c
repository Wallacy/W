#include "w_seed_foreign.h"

#include <stdio.h>
#include <string.h>

static unsigned failures = 0;

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "foreign check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                       \
      failures += 1;                                                            \
    }                                                                           \
  } while (0)

static const w_seed_foreign_limits DEFAULT_LIMITS = {
    64u * 1024u,
    256u,
};

static bool scan_bytes(const uint8_t *bytes, size_t length,
                       w_seed_foreign_limits limits,
                       w_seed_foreign_source_validation *result,
                       w_seed_foreign_error *error) {
  return w_seed_foreign_scan_c_inline_1((w_seed_byte_view){bytes, length},
                                        limits, result, error);
}

static void expect_digest(const uint8_t actual[32], const uint8_t expected[32]) {
  CHECK(memcmp(actual, expected, 32) == 0);
}

static void test_empty_body(void) {
  static const uint8_t source[] = {'{', '}'};
  static const uint8_t digest[] = {
      0xe3u, 0xb0u, 0xc4u, 0x42u, 0x98u, 0xfc, 0x1cu, 0x14u,
      0x9au, 0xfbu, 0xf4u, 0xc8u, 0x99u, 0x6fu, 0xb9u, 0x24u,
      0x27u, 0xaeu, 0x41u, 0xe4u, 0x64u, 0x9bu, 0x93u, 0x4cu,
      0xa4u, 0x95u, 0x99u, 0x1bu, 0x78u, 0x52u, 0xb8u, 0x55u,
  };
  w_seed_foreign_source_validation result;
  w_seed_foreign_error error;
  CHECK(scan_bytes(source, sizeof(source), DEFAULT_LIMITS, &result, &error));
  CHECK(error.kind == W_SEED_FOREIGN_ERROR_NONE);
  CHECK(result.profile == W_SEED_FOREIGN_PROFILE_C_INLINE_1);
  CHECK(result.body_start_byte == 1 && result.body_end_byte == 1);
  CHECK(result.close_byte == 1 && result.next_byte == 2);
  CHECK(result.maximum_nesting_observed == 0);
  CHECK(result.digest_valid);
  expect_digest(result.body_digest, digest);
}

static void test_c_lexical_states(void) {
  static const uint8_t source[] =
      "{\"x{y}\" /* { */ { <% %> } // }\r\n 'a' }";
  static const uint8_t escaped_crlf[] = {
      '{', '"', 'a', '\\', '\r', '\n', 'b', '"', '}',
  };
  static const uint8_t comment_splice[] = {
      '{', '/', '/', 'x', '\\', '\r', '\n', 'y', '\r', '\n', '}',
  };
  w_seed_foreign_source_validation result;
  w_seed_foreign_error error;
  CHECK(scan_bytes(source, sizeof(source) - 1u, DEFAULT_LIMITS, &result,
                   &error));
  CHECK(result.body_end_byte == sizeof(source) - 2u);
  CHECK(result.close_byte == result.body_end_byte);
  CHECK(result.next_byte == sizeof(source) - 1u);
  CHECK(result.maximum_nesting_observed == 2);
  CHECK(result.digest_valid);
  CHECK(scan_bytes(escaped_crlf, sizeof(escaped_crlf), DEFAULT_LIMITS, &result,
                   &error));
  CHECK(scan_bytes(comment_splice, sizeof(comment_splice), DEFAULT_LIMITS,
                   &result, &error));
}

static void test_limits(void) {
  static const uint8_t empty[] = "{}";
  static const uint8_t one_byte[] = "{a}";
  static const uint8_t exact[] = "{abc}";
  static const uint8_t too_long[] = "{abcd}";
  static const uint8_t nested[] = "{{}}";
  w_seed_foreign_source_validation result;
  w_seed_foreign_error error;
  CHECK(scan_bytes(empty, sizeof(empty) - 1u,
                   (w_seed_foreign_limits){0u, 0u}, &result, &error));
  CHECK(!scan_bytes(one_byte, sizeof(one_byte) - 1u,
                    (w_seed_foreign_limits){0u, 0u}, &result, &error));
  CHECK(error.kind == W_SEED_FOREIGN_ERROR_BODY_LIMIT);
  CHECK(scan_bytes(exact, sizeof(exact) - 1u, (w_seed_foreign_limits){3u, 4u},
                   &result, &error));
  CHECK(!scan_bytes(too_long, sizeof(too_long) - 1u,
                    (w_seed_foreign_limits){3u, 4u}, &result, &error));
  CHECK(error.kind == W_SEED_FOREIGN_ERROR_BODY_LIMIT);
  CHECK(error.terminal_state == W_SEED_FOREIGN_TERMINAL_BODY_LIMIT);
  CHECK(!scan_bytes(nested, sizeof(nested) - 1u,
                    (w_seed_foreign_limits){64u, 0u}, &result, &error));
  CHECK(error.kind == W_SEED_FOREIGN_ERROR_NESTING_LIMIT);
  CHECK(scan_bytes(nested, sizeof(nested) - 1u,
                   (w_seed_foreign_limits){2u, 1u}, &result, &error));
  CHECK(result.maximum_nesting_observed == 1);
}

static void test_rejections(void) {
  static const uint8_t invalid_utf8[] = {'{', 0xc0u, 0x80u, '}'};
  static const uint8_t nul[] = {'{', 'x', 0, '}'};
  static const uint8_t missing_open[] = "x}";
  static const uint8_t missing_close[] = "{x";
  static const uint8_t literal[] = "{\"x}";
  static const uint8_t comment[] = "{/* x}";
  static const uint8_t directive[] = "{\n #define X 1\n}";
  static const uint8_t splice[] = "{x\\\ny}";
  const uint8_t *sources[] = {invalid_utf8, nul, missing_open, missing_close,
                              literal, comment, directive, splice};
  const size_t lengths[] = {sizeof(invalid_utf8), sizeof(nul),
                            sizeof(missing_open) - 1u,
                            sizeof(missing_close) - 1u,
                            sizeof(literal) - 1u, sizeof(comment) - 1u,
                            sizeof(directive) - 1u, sizeof(splice) - 1u};
  const w_seed_foreign_error_kind kinds[] = {
      W_SEED_FOREIGN_ERROR_INVALID_UTF8,
      W_SEED_FOREIGN_ERROR_NUL,
      W_SEED_FOREIGN_ERROR_MISSING_OPEN,
      W_SEED_FOREIGN_ERROR_MISSING_CLOSE,
      W_SEED_FOREIGN_ERROR_UNTERMINATED_LITERAL,
      W_SEED_FOREIGN_ERROR_UNTERMINATED_COMMENT,
      W_SEED_FOREIGN_ERROR_PREPROCESSOR_DIRECTIVE,
      W_SEED_FOREIGN_ERROR_LINE_SPLICE,
  };
  w_seed_foreign_source_validation result;
  w_seed_foreign_error error;
  for (size_t index = 0; index < sizeof(sources) / sizeof(sources[0]);
       index += 1) {
    CHECK(!scan_bytes(sources[index], lengths[index], DEFAULT_LIMITS, &result,
                      &error));
    CHECK(error.kind == kinds[index]);
    CHECK(error.primary.start_byte <= lengths[index]);
    CHECK(!result.digest_valid);
  }
}

static void test_suffix_boundaries(void) {
  static const uint8_t invalid_utf8_suffix[] = {'{', 'x', '}', 0xffu};
  static const uint8_t nul_suffix[] = {'{', 'x', '}', 0};
  static const uint8_t invalid_utf8_body[] = {'{', 0xc0u, 0x80u, '}'};
  static const uint8_t nul_body[] = {'{', 'x', 0, '}'};
  w_seed_foreign_source_validation result;
  w_seed_foreign_error error;
  const uint8_t *suffix_sources[] = {invalid_utf8_suffix, nul_suffix};
  const size_t suffix_lengths[] = {sizeof(invalid_utf8_suffix),
                                   sizeof(nul_suffix)};
  for (size_t index = 0; index < sizeof(suffix_sources) / sizeof(suffix_sources[0]);
       index += 1) {
    CHECK(scan_bytes(suffix_sources[index], suffix_lengths[index], DEFAULT_LIMITS,
                     &result, &error));
    CHECK(error.kind == W_SEED_FOREIGN_ERROR_NONE);
    CHECK(result.body_start_byte == 1 && result.body_end_byte == 2);
    CHECK(result.close_byte == 2 && result.next_byte == 3);
    CHECK(result.digest_valid);
  }
  CHECK(!scan_bytes(invalid_utf8_body, sizeof(invalid_utf8_body), DEFAULT_LIMITS,
                    &result, &error));
  CHECK(error.kind == W_SEED_FOREIGN_ERROR_INVALID_UTF8);
  CHECK(!scan_bytes(nul_body, sizeof(nul_body), DEFAULT_LIMITS, &result, &error));
  CHECK(error.kind == W_SEED_FOREIGN_ERROR_NUL);
}

static void test_arguments(void) {
  w_seed_foreign_source_validation result;
  w_seed_foreign_error error;
  CHECK(!w_seed_foreign_scan_c_inline_1((w_seed_byte_view){NULL, 0},
                                        DEFAULT_LIMITS, &result, &error));
  CHECK(error.kind == W_SEED_FOREIGN_ERROR_MISSING_OPEN);
  CHECK(!w_seed_foreign_scan_c_inline_1((w_seed_byte_view){NULL, 1},
                                        DEFAULT_LIMITS, &result, &error));
  CHECK(error.kind == W_SEED_FOREIGN_ERROR_NULL_ARGUMENT);
  CHECK(!scan_bytes((const uint8_t *)"{}", 2u,
                    (w_seed_foreign_limits){SIZE_MAX, SIZE_MAX}, NULL,
                    &error));
  CHECK(error.kind == W_SEED_FOREIGN_ERROR_NULL_ARGUMENT);
}

int main(void) {
  test_empty_body();
  test_c_lexical_states();
  test_limits();
  test_rejections();
  test_suffix_boundaries();
  test_arguments();
  if (failures != 0) {
    (void)fprintf(stderr, "foreign tests: %u failure(s)\n", failures);
    return 1;
  }
  (void)puts("Seed C foreign scanner: source-validation unit cases passed");
  return 0;
}
