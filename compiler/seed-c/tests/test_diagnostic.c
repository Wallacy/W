#include "w_seed_diagnostic.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static bool all_bytes_equal(const uint8_t *bytes, size_t length,
                            uint8_t value) {
  for (size_t index = 0; index < length; index += 1) {
    if (bytes[index] != value) return false;
  }
  return true;
}

static bool contains_text(const uint8_t *bytes, size_t length,
                          const char *text) {
  const size_t text_length = strlen(text);
  if (text_length > length) return false;
  for (size_t offset = 0; offset <= length - text_length; offset += 1) {
    if (memcmp(bytes + offset, text, text_length) == 0) return true;
  }
  return false;
}

int main(void) {
  static const uint8_t source[] = {'a', '\n', '"'};
  static const uint8_t canonical[] = {'a', '\n', '\\', '"', 0xc3, 0xa9};
  static const char instance[] = "D123456";
  static const uint8_t source_id_bytes[] = {
      'f', 'o', 'r', 'm', 'a', 't', '/', 'q', 'u', 'o', 't', 'e', 0xc3,
      0xa7, '"', '\\', '.', 'w', 0};
  const char *source_id = (const char *)source_id_bytes;
  static uint8_t output[4096];
  w_seed_diagnostic_result measured;
  if (w_seed_diagnostic_format_record(
          instance, sizeof(instance) - 1, source_id, sizeof(source_id_bytes) - 1,
          source, sizeof(source), canonical, sizeof(canonical), NULL, 0,
          &measured) != W_SEED_DIAGNOSTIC_CAPACITY ||
          measured.required_bytes == 0 || measured.primary_byte != 2) {
    return 1;
  }
  if (measured.required_bytes >= sizeof(output)) return 2;
  (void)memset(output, 0xA5, sizeof(output));
  w_seed_diagnostic_result short_result;
  if (w_seed_diagnostic_format_record(
          instance, sizeof(instance) - 1, source_id, sizeof(source_id_bytes) - 1,
          source, sizeof(source), canonical, sizeof(canonical), output,
          measured.required_bytes - 1, &short_result) !=
          W_SEED_DIAGNOSTIC_CAPACITY ||
      short_result.written_bytes != 0 ||
      !all_bytes_equal(output, sizeof(output), 0xA5)) {
    return 3;
  }
  w_seed_diagnostic_result written;
  if (w_seed_diagnostic_format_record(
          instance, sizeof(instance) - 1, source_id, sizeof(source_id_bytes) - 1,
          source, sizeof(source), canonical, sizeof(canonical), output,
          sizeof(output), &written) != W_SEED_DIAGNOSTIC_OK ||
      written.written_bytes != measured.required_bytes) {
    return 4;
  }
  if (!contains_text(output, written.written_bytes, "D123456") ||
      !contains_text(output, written.written_bytes, "format/quote") ||
      !contains_text(output, written.written_bytes, "a\\n\\\\\\\"") ||
      !contains_text(output, written.written_bytes, "\xc3\xa9") ||
      !contains_text(output, written.written_bytes,
                     "sha256:447e3784786fae550bfb3e266b082dbc23b2493dfc16d98ee15c8b285a4881da")) {
    return 5;
  }
  w_seed_diagnostic_result no_record;
  if (w_seed_diagnostic_format_record(
          instance, sizeof(instance) - 1, source_id, sizeof(source_id_bytes) - 1,
          source, sizeof(source), source, sizeof(source), NULL, 0,
          &no_record) != W_SEED_DIAGNOSTIC_NO_RECORD) {
    return 6;
  }
  static const char bad_instance[] = "D\"1";
  if (w_seed_diagnostic_format_record(
          bad_instance, sizeof(bad_instance) - 1, source_id,
          sizeof(source_id_bytes) - 1, source, sizeof(source), canonical,
          sizeof(canonical), NULL, 0, &no_record) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 7;
  }
  static const uint8_t invalid_source_id[] = {'x', 0xc3};
  if (w_seed_diagnostic_format_record(
          instance, sizeof(instance) - 1, (const char *)invalid_source_id,
          sizeof(invalid_source_id), source, sizeof(source), canonical,
          sizeof(canonical), NULL, 0, &no_record) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 8;
  }
  static const uint8_t nul_source_id[] = {'x', 0, 'y'};
  if (w_seed_diagnostic_format_record(
          instance, sizeof(instance) - 1, (const char *)nul_source_id,
          sizeof(nul_source_id), source, sizeof(source), canonical,
          sizeof(canonical), NULL, 0, &no_record) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 21;
  }
  static const uint8_t invalid_source[] = {'x', 0xc3};
  if (w_seed_diagnostic_format_record(
          instance, sizeof(instance) - 1, source_id,
          sizeof(source_id_bytes) - 1, invalid_source, sizeof(invalid_source),
          canonical, sizeof(canonical), NULL, 0, &no_record) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 22;
  }
  static const uint8_t nul_canonical[] = {'x', 0};
  if (w_seed_diagnostic_format_record(
          instance, sizeof(instance) - 1, source_id,
          sizeof(source_id_bytes) - 1, source, sizeof(source), nul_canonical,
          sizeof(nul_canonical), NULL, 0, &no_record) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 23;
  }
  static const w_seed_lex_error unterminated = {
      W_SEED_LEX_ERROR_UNTERMINATED_LITERAL, {2, 3}, {0, 1},
      W_SEED_LITERAL_STRING, 0, true};
  static const char lex_source_id[] = "lex";
  static const char expected_lex[] =
      "{\"schemaVersion\":1,\"instance\":\"D123456\",\"code\":\"W-LEX-0001\","
      "\"phase\":\"source.lex\",\"severity\":\"error\",\"primary\":{\"source\":\"lex\","
      "\"startByte\":2,\"endByte\":3},\"labels\":[{\"role\":\"opening-delimiter\","
      "\"span\":{\"source\":\"lex\",\"startByte\":0,\"endByte\":1}}],\"facts\":{"
      "\"construct\":\"string-literal\",\"delimiter\":\"quote\",\"reachedEof\":true},"
      "\"notes\":[],\"fixes\":[],\"root\":null}";
  w_seed_diagnostic_result lex_result;
  if (w_seed_diagnostic_lex_record(
          instance, sizeof(instance) - 1, lex_source_id, sizeof(lex_source_id) - 1,
          &unterminated, sizeof(source), output, sizeof(output), &lex_result) !=
          W_SEED_DIAGNOSTIC_OK ||
      lex_result.written_bytes != sizeof(expected_lex) - 1 ||
      memcmp(output, expected_lex, sizeof(expected_lex) - 1) != 0) {
    return 9;
  }
  w_seed_diagnostic_result lex_measured;
  if (w_seed_diagnostic_lex_record(
          instance, sizeof(instance) - 1, lex_source_id,
          sizeof(lex_source_id) - 1, &unterminated, sizeof(source), NULL, 0,
          &lex_measured) != W_SEED_DIAGNOSTIC_CAPACITY ||
      lex_measured.required_bytes != lex_result.written_bytes) {
    return 24;
  }
  (void)memset(output, 0xA5, sizeof(output));
  if (w_seed_diagnostic_lex_record(
          instance, sizeof(instance) - 1, lex_source_id,
          sizeof(lex_source_id) - 1, &unterminated, sizeof(source), output,
          lex_measured.required_bytes - 1, &lex_result) !=
          W_SEED_DIAGNOSTIC_CAPACITY ||
      lex_result.written_bytes != 0 || !all_bytes_equal(output, sizeof(output),
                                                        0xA5)) {
    return 25;
  }
  static const w_seed_lex_error unterminated_comment = {
      W_SEED_LEX_ERROR_UNTERMINATED_COMMENT, {2, 3}, {0, 2},
      W_SEED_LITERAL_NONE, 0, true};
  static const char expected_comment[] =
      "{\"schemaVersion\":1,\"instance\":\"D123456\",\"code\":\"W-LEX-0001\","
      "\"phase\":\"source.lex\",\"severity\":\"error\",\"primary\":{\"source\":\"lex\","
      "\"startByte\":2,\"endByte\":3},\"labels\":[{\"role\":\"opening-delimiter\","
      "\"span\":{\"source\":\"lex\",\"startByte\":0,\"endByte\":2}}],\"facts\":{"
      "\"construct\":\"block-comment\",\"delimiter\":\"block-comment-close\","
      "\"reachedEof\":true},\"notes\":[],\"fixes\":[],\"root\":null}";
  if (w_seed_diagnostic_lex_record(
          instance, sizeof(instance) - 1, lex_source_id,
          sizeof(lex_source_id) - 1, &unterminated_comment, sizeof(source),
          output, sizeof(output), &lex_result) != W_SEED_DIAGNOSTIC_OK ||
      lex_result.written_bytes != sizeof(expected_comment) - 1 ||
      memcmp(output, expected_comment, sizeof(expected_comment) - 1) != 0) {
    return 26;
  }
  static const struct {
    w_seed_literal_kind literal;
    const char *construct;
    const char *delimiter;
  } literal_profiles[] = {
      {W_SEED_LITERAL_STRING, "string-literal", "quote"},
      {W_SEED_LITERAL_RAW_STRING, "string-literal", "raw-quote"},
      {W_SEED_LITERAL_MULTILINE_STRING, "string-literal", "triple-quote"},
      {W_SEED_LITERAL_RAW_MULTILINE_STRING, "string-literal",
       "raw-triple-quote"},
      {W_SEED_LITERAL_BYTE_STRING, "byte-string-literal", "byte-quote"},
      {W_SEED_LITERAL_SCALAR, "scalar-literal", "apostrophe"},
      {W_SEED_LITERAL_BYTE_SCALAR, "byte-scalar-literal", "byte-apostrophe"},
  };
  for (size_t index = 0;
       index < sizeof(literal_profiles) / sizeof(literal_profiles[0]);
       index += 1) {
    w_seed_lex_error profile_error = unterminated;
    profile_error.literal = literal_profiles[index].literal;
    if (w_seed_diagnostic_lex_record(
            instance, sizeof(instance) - 1, lex_source_id,
            sizeof(lex_source_id) - 1, &profile_error, sizeof(source), output,
            sizeof(output), &lex_result) != W_SEED_DIAGNOSTIC_OK ||
        !contains_text(output, lex_result.written_bytes,
                       literal_profiles[index].construct) ||
        !contains_text(output, lex_result.written_bytes,
                       literal_profiles[index].delimiter)) {
      return 15;
    }
  }
  static const w_seed_lex_error boundary = {
      W_SEED_LEX_ERROR_UNTERMINATED_LITERAL, {2, 3}, {0, 1},
      W_SEED_LITERAL_STRING, 0, false};
  if (w_seed_diagnostic_lex_record(
          instance, sizeof(instance) - 1, lex_source_id,
          sizeof(lex_source_id) - 1, &boundary, sizeof(source), output,
          sizeof(output), &lex_result) != W_SEED_DIAGNOSTIC_OK ||
      !contains_text(output, lex_result.written_bytes, "\"reachedEof\":false")) {
    return 16;
  }
  static const w_seed_lex_error unsupported_lex = {
      W_SEED_LEX_ERROR_UNSUPPORTED_CONTROL, {0, 1}, {0, 0},
      W_SEED_LITERAL_NONE, 0, true};
  if (w_seed_diagnostic_lex_record(
          instance, sizeof(instance) - 1, lex_source_id,
          sizeof(lex_source_id) - 1,
          &unsupported_lex, sizeof(source), NULL, 0, &lex_result) !=
      W_SEED_DIAGNOSTIC_UNSUPPORTED) {
    return 17;
  }
  static const w_seed_lex_error out_of_range = {
      W_SEED_LEX_ERROR_UNTERMINATED_LITERAL, {5, 6}, {0, 1},
      W_SEED_LITERAL_STRING, 0, true};
  if (w_seed_diagnostic_lex_record(
          instance, sizeof(instance) - 1, lex_source_id,
          sizeof(lex_source_id) - 1, &out_of_range, sizeof(source), NULL, 0,
          &lex_result) != W_SEED_DIAGNOSTIC_UNSUPPORTED) {
    return 18;
  }
  static const w_seed_parse_issue parse_issue = {
      W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN, {1, 2}, {0, 1},
      W_SEED_LEX_ITEM_PUNCTUATION, W_SEED_PARSE_EXPECT_WORD};
  static const char expected_parse[] =
      "{\"schemaVersion\":1,\"instance\":\"D123456\",\"code\":\"W-PARSE-0001\","
      "\"phase\":\"source.parse\",\"severity\":\"error\",\"primary\":{\"source\":\"lex\","
      "\"startByte\":1,\"endByte\":2},\"labels\":[{\"role\":\"owner\",\"span\":{"
      "\"source\":\"lex\",\"startByte\":0,\"endByte\":1}}],\"facts\":{\"actual\":"
      "\"punctuation\",\"construct\":\"grammar owner\",\"expected\":[\"word\"]},"
      "\"notes\":[],\"fixes\":[],\"root\":null}";
  w_seed_diagnostic_result parse_result;
  if (w_seed_diagnostic_parse_record(
          instance, sizeof(instance) - 1, lex_source_id,
          sizeof(lex_source_id) - 1,
          &parse_issue, 4, output, sizeof(output), &parse_result) !=
          W_SEED_DIAGNOSTIC_OK ||
      parse_result.written_bytes != sizeof(expected_parse) - 1 ||
      memcmp(output, expected_parse, sizeof(expected_parse) - 1) != 0) {
    return 19;
  }
  w_seed_diagnostic_result parse_measured;
  if (w_seed_diagnostic_parse_record(
          instance, sizeof(instance) - 1, lex_source_id,
          sizeof(lex_source_id) - 1, &parse_issue, 4, NULL, 0,
          &parse_measured) != W_SEED_DIAGNOSTIC_CAPACITY ||
      parse_measured.required_bytes != parse_result.written_bytes) {
    return 27;
  }
  (void)memset(output, 0xA5, sizeof(output));
  if (w_seed_diagnostic_parse_record(
          instance, sizeof(instance) - 1, lex_source_id,
          sizeof(lex_source_id) - 1, &parse_issue, 4, output,
          parse_measured.required_bytes - 1, &parse_result) !=
          W_SEED_DIAGNOSTIC_CAPACITY ||
      parse_result.written_bytes != 0 ||
      !all_bytes_equal(output, sizeof(output), 0xA5)) {
    return 28;
  }
  static const struct {
    w_seed_parse_issue_kind kind;
    const char *code;
    const char *construct;
  } parse_profiles[] = {
      {W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN, "W-PARSE-0001", "grammar owner"},
      {W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE, "W-PARSE-0002", "grammar owner"},
      {W_SEED_PARSE_ISSUE_NO_CONTINUATION_OWNER, "W-PARSE-0004",
       "contextual continuation"},
      {W_SEED_PARSE_ISSUE_MIXED_ROOT, "W-PARSE-0006", "document root"},
      {W_SEED_PARSE_ISSUE_SPACED_HEAD, "W-PARSE-0013", "declaration head"},
      {W_SEED_PARSE_ISSUE_VALUE_IF_MISSING_ELSE, "W-PARSE-0021", "value if"},
  };
  for (size_t index = 0;
       index < sizeof(parse_profiles) / sizeof(parse_profiles[0]);
       index += 1) {
    w_seed_parse_issue profile_issue = parse_issue;
    profile_issue.kind = parse_profiles[index].kind;
    if (w_seed_diagnostic_parse_record(
            instance, sizeof(instance) - 1, lex_source_id,
            sizeof(lex_source_id) - 1, &profile_issue, 4, output,
            sizeof(output), &parse_result) != W_SEED_DIAGNOSTIC_OK ||
        !contains_text(output, parse_result.written_bytes,
                       parse_profiles[index].code) ||
        !contains_text(output, parse_result.written_bytes,
                       parse_profiles[index].construct)) {
      return 29;
    }
  }
  static const w_seed_parse_issue unsupported_parse = {
      W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM, {1, 2}, {0, 0},
      W_SEED_LEX_ITEM_WORD, W_SEED_PARSE_EXPECT_WORD};
  if (w_seed_diagnostic_parse_record(
          instance, sizeof(instance) - 1, lex_source_id,
          sizeof(lex_source_id) - 1,
          &unsupported_parse, 4, NULL, 0, &parse_result) !=
      W_SEED_DIAGNOSTIC_UNSUPPORTED) {
    return 20;
  }
  return 0;
}
