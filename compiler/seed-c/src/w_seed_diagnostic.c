#include "w_seed_diagnostic.h"

#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

typedef struct {
  uint8_t *output;
  size_t capacity;
  size_t written;
  size_t required;
} diagnostic_writer;

static void saturating_increment(size_t *value) {
  if (*value != SIZE_MAX) *value += 1;
}

static bool valid_utf8_identity(const char *value, size_t length) {
  if (length != 0 && value == NULL) return false;
  size_t offset = 0;
  while (offset < length) {
    const uint8_t first = (uint8_t)value[offset];
    if (first == 0) return false;
    if (first < 0x80u) {
      offset += 1;
      continue;
    }
    size_t width = 0;
    uint32_t code_point = 0;
    if (first >= 0xc2u && first <= 0xdfu) {
      width = 2;
      code_point = first & 0x1fu;
    } else if (first >= 0xe0u && first <= 0xefu) {
      width = 3;
      code_point = first & 0x0fu;
    } else if (first >= 0xf0u && first <= 0xf4u) {
      width = 4;
      code_point = first & 0x07u;
    } else {
      return false;
    }
    if (width > length - offset) return false;
    for (size_t index = 1; index < width; index += 1) {
      const uint8_t continuation = (uint8_t)value[offset + index];
      if ((continuation & 0xc0u) != 0x80u) return false;
      code_point = (code_point << 6) | (continuation & 0x3fu);
    }
    if ((width == 3 && code_point < 0x800u) ||
        (width == 4 && code_point < 0x10000u) ||
        (code_point >= 0xd800u && code_point <= 0xdfffu) ||
        code_point > 0x10ffffu) {
      return false;
    }
    offset += width;
  }
  return true;
}

static bool valid_instance(const char *value, size_t length) {
  if (value == NULL || length != 7 || value[0] != 'D') return false;
  for (size_t index = 1; index < length; index += 1) {
    if (value[index] < '0' || value[index] > '9') return false;
  }
  return true;
}

static bool valid_bytes(const uint8_t *value, size_t length) {
  return length == 0 || value != NULL;
}

static void writer_byte(diagnostic_writer *writer, uint8_t value) {
  saturating_increment(&writer->required);
  if (writer->output != NULL && writer->written < writer->capacity) {
    writer->output[writer->written] = value;
    writer->written += 1;
  }
}

static void writer_text(diagnostic_writer *writer, const char *text) {
  const size_t length = strlen(text);
  for (size_t index = 0; index < length; index += 1) {
    writer_byte(writer, (uint8_t)text[index]);
  }
}

static void writer_decimal(diagnostic_writer *writer, size_t value) {
  char digits[3 * sizeof(size_t) + 1];
  size_t length = 0;
  do {
    digits[length] = (char)('0' + (value % 10));
    value /= 10;
    length += 1;
  } while (value != 0);
  while (length != 0) {
    length -= 1;
    writer_byte(writer, (uint8_t)digits[length]);
  }
}

static void writer_json_string(diagnostic_writer *writer, const char *value,
                               size_t length) {
  static const char hex[] = "0123456789abcdef";
  writer_byte(writer, (uint8_t)'"');
  for (size_t index = 0; index < length; index += 1) {
    const uint8_t byte = (uint8_t)value[index];
    switch (byte) {
      case '"':
        writer_text(writer, "\\\"");
        break;
      case '\\':
        writer_text(writer, "\\\\");
        break;
      case '\b':
        writer_text(writer, "\\b");
        break;
      case '\f':
        writer_text(writer, "\\f");
        break;
      case '\n':
        writer_text(writer, "\\n");
        break;
      case '\r':
        writer_text(writer, "\\r");
        break;
      case '\t':
        writer_text(writer, "\\t");
        break;
      default:
        if (byte < 0x20u) {
          writer_text(writer, "\\u00");
          writer_byte(writer, (uint8_t)hex[byte >> 4]);
          writer_byte(writer, (uint8_t)hex[byte & 0x0fu]);
        } else {
          writer_byte(writer, byte);
        }
        break;
    }
  }
  writer_byte(writer, (uint8_t)'"');
}

static void writer_digest(diagnostic_writer *writer, const uint8_t digest[32]) {
  static const char hex[] = "0123456789abcdef";
  writer_text(writer, "sha256:");
  for (size_t index = 0; index < 32; index += 1) {
    writer_byte(writer, (uint8_t)hex[digest[index] >> 4]);
    writer_byte(writer, (uint8_t)hex[digest[index] & 0x0fu]);
  }
}

static void write_format_record(diagnostic_writer *writer,
                                const char *instance, size_t instance_length,
                                const char *source_id, size_t source_id_length,
                                const uint8_t *source, size_t source_length,
                                const uint8_t *canonical,
                                size_t canonical_length,
                                const uint8_t source_digest[32],
                                const uint8_t canonical_digest[32],
                                size_t primary_byte) {
  writer_text(writer, "{\"schemaVersion\":1,\"instance\":");
  writer_json_string(writer, instance, instance_length);
  writer_text(writer, ",\"code\":\"W-FMT-0001\",\"phase\":\"source.format\",\"severity\":\"error\",\"primary\":{\"source\":");
  writer_json_string(writer, source_id, source_id_length);
  writer_text(writer, ",\"startByte\":");
  writer_decimal(writer, primary_byte);
  writer_text(writer, ",\"endByte\":");
  writer_decimal(writer, primary_byte);
  writer_text(writer, "},\"labels\":[],\"facts\":{\"canonicalDigest\":\"");
  writer_digest(writer, canonical_digest);
  writer_text(writer, "\",\"sourceDigest\":\"");
  writer_digest(writer, source_digest);
  writer_text(writer, "\"},\"notes\":[],\"fixes\":[{\"id\":\"format-source\",\"titleKey\":\"fix.format.source\",\"applicability\":\"machine\",\"preconditions\":[{\"source\":");
  writer_json_string(writer, source_id, source_id_length);
  writer_text(writer, ",\"digest\":\"");
  writer_digest(writer, source_digest);
  writer_text(writer, "\"}],\"edits\":[{\"source\":");
  writer_json_string(writer, source_id, source_id_length);
  writer_text(writer, ",\"startByte\":0,\"endByte\":");
  writer_decimal(writer, source_length);
  writer_text(writer, ",\"text\":");
  writer_json_string(writer, (const char *)canonical, canonical_length);
  writer_text(writer, "}]}],\"root\":null}");
  (void)source;
}

static void clear_result(w_seed_diagnostic_result *result,
                         w_seed_diagnostic_status status) {
  if (result == NULL) return;
  (void)memset(result, 0, sizeof(*result));
  result->status = status;
}

w_seed_diagnostic_status w_seed_diagnostic_format_record(
    const char *instance, size_t instance_length, const char *source_id,
    size_t source_id_length, const uint8_t *source, size_t source_length,
    const uint8_t *canonical, size_t canonical_length, uint8_t *output,
    size_t output_capacity, w_seed_diagnostic_result *result) {
  clear_result(result, W_SEED_DIAGNOSTIC_INVALID);
  if (result == NULL || !valid_instance(instance, instance_length) ||
      !valid_utf8_identity(source_id, source_id_length) ||
      !valid_bytes(source, source_length) ||
      !valid_bytes(canonical, canonical_length) ||
      !valid_utf8_identity((const char *)source, source_length) ||
      !valid_utf8_identity((const char *)canonical, canonical_length)) {
    if (result != NULL) result->status = W_SEED_DIAGNOSTIC_INVALID;
    return W_SEED_DIAGNOSTIC_INVALID;
  }
  size_t primary_byte = 0;
  const size_t common = source_length < canonical_length ? source_length
                                                          : canonical_length;
  while (primary_byte < common && source[primary_byte] == canonical[primary_byte]) {
    primary_byte += 1;
  }
  if (primary_byte == common && source_length == canonical_length) {
    result->status = W_SEED_DIAGNOSTIC_NO_RECORD;
    result->primary_byte = primary_byte;
    return result->status;
  }

  uint8_t source_digest[32];
  uint8_t canonical_digest[32];
  w_seed_sha256_state source_hash;
  w_seed_sha256_state canonical_hash;
  w_seed_sha256_init(&source_hash);
  w_seed_sha256_update(&source_hash, source, source_length);
  w_seed_sha256_final(&source_hash, source_digest);
  w_seed_sha256_init(&canonical_hash);
  w_seed_sha256_update(&canonical_hash, canonical, canonical_length);
  w_seed_sha256_final(&canonical_hash, canonical_digest);

  diagnostic_writer measure = {NULL, 0, 0, 0};
  write_format_record(&measure, instance, instance_length, source_id,
                      source_id_length, source, source_length, canonical,
                      canonical_length, source_digest, canonical_digest,
                      primary_byte);
  result->required_bytes = measure.required;
  result->primary_byte = primary_byte;
  if (output == NULL || output_capacity < measure.required) {
    result->status = W_SEED_DIAGNOSTIC_CAPACITY;
    return result->status;
  }
  diagnostic_writer writer = {output, output_capacity, 0, 0};
  write_format_record(&writer, instance, instance_length, source_id,
                      source_id_length, source, source_length, canonical,
                      canonical_length, source_digest, canonical_digest,
                      primary_byte);
  result->status = W_SEED_DIAGNOSTIC_OK;
  result->written_bytes = writer.written;
  return result->status;
}

static bool literal_profile(w_seed_literal_kind literal,
                            const char **construct,
                            const char **delimiter) {
  *construct = NULL;
  *delimiter = NULL;
  switch (literal) {
    case W_SEED_LITERAL_STRING:
      *construct = "string-literal";
      *delimiter = "quote";
      return true;
    case W_SEED_LITERAL_RAW_STRING:
      *construct = "string-literal";
      *delimiter = "raw-quote";
      return true;
    case W_SEED_LITERAL_MULTILINE_STRING:
      *construct = "string-literal";
      *delimiter = "triple-quote";
      return true;
    case W_SEED_LITERAL_RAW_MULTILINE_STRING:
      *construct = "string-literal";
      *delimiter = "raw-triple-quote";
      return true;
    case W_SEED_LITERAL_BYTE_STRING:
      *construct = "byte-string-literal";
      *delimiter = "byte-quote";
      return true;
    case W_SEED_LITERAL_SCALAR:
      *construct = "scalar-literal";
      *delimiter = "apostrophe";
      return true;
    case W_SEED_LITERAL_BYTE_SCALAR:
      *construct = "byte-scalar-literal";
      *delimiter = "byte-apostrophe";
      return true;
    case W_SEED_LITERAL_NONE:
      break;
  }
  return false;
}

static void write_primary(diagnostic_writer *writer, const char *source_id,
                          size_t source_id_length, w_seed_span primary) {
  writer_text(writer, "\"primary\":{\"source\":");
  writer_json_string(writer, source_id, source_id_length);
  writer_text(writer, ",\"startByte\":");
  writer_decimal(writer, primary.start_byte);
  writer_text(writer, ",\"endByte\":");
  writer_decimal(writer, primary.end_byte);
  writer_byte(writer, (uint8_t)'}');
}

static void write_common_tail(diagnostic_writer *writer) {
  writer_text(writer, ",\"notes\":[],\"fixes\":[],\"root\":null}");
}

static void write_label(diagnostic_writer *writer, const char *source_id,
                        size_t source_id_length, const char *role,
                        w_seed_span span) {
  writer_text(writer, "{\"role\":");
  writer_json_string(writer, role, strlen(role));
  writer_text(writer, ",\"span\":{\"source\":");
  writer_json_string(writer, source_id, source_id_length);
  writer_text(writer, ",\"startByte\":");
  writer_decimal(writer, span.start_byte);
  writer_text(writer, ",\"endByte\":");
  writer_decimal(writer, span.end_byte);
  writer_byte(writer, (uint8_t)'}');
  writer_byte(writer, (uint8_t)'}');
}

static void write_lex_record(diagnostic_writer *writer, const char *instance,
                             size_t instance_length, const char *source_id,
                             size_t source_id_length,
                             const w_seed_lex_error *error,
                             const char *construct, const char *delimiter) {
  writer_text(writer, "{\"schemaVersion\":1,\"instance\":");
  writer_json_string(writer, instance, instance_length);
  writer_text(writer, ",\"code\":\"W-LEX-0001\",\"phase\":\"source.lex\",\"severity\":\"error\",");
  write_primary(writer, source_id, source_id_length, error->primary);
  writer_text(writer, ",\"labels\":[");
  write_label(writer, source_id, source_id_length, "opening-delimiter",
              error->opening);
  writer_text(writer, "],\"facts\":{\"construct\":");
  writer_json_string(writer, construct, strlen(construct));
  writer_text(writer, ",\"delimiter\":");
  writer_json_string(writer, delimiter, strlen(delimiter));
  writer_text(writer, ",\"reachedEof\":");
  writer_text(writer, error->reached_eof ? "true}" : "false}");
  write_common_tail(writer);
}

static const char *actual_kind_name(w_seed_lex_item_kind kind) {
  switch (kind) {
    case W_SEED_LEX_ITEM_SOURCE_PREFIX:
      return "source-prefix";
    case W_SEED_LEX_ITEM_TRIVIA:
      return "trivia";
    case W_SEED_LEX_ITEM_WORD:
      return "word";
    case W_SEED_LEX_ITEM_NUMBER:
      return "number";
    case W_SEED_LEX_ITEM_PUNCTUATION:
      return "punctuation";
    case W_SEED_LEX_ITEM_LITERAL_EVENT:
      return "literal";
    case W_SEED_LEX_ITEM_FOREIGN_BODY:
      return "foreign-body";
    case W_SEED_LEX_ITEM_UNKNOWN:
      return "unknown";
    case W_SEED_LEX_ITEM_EOF:
      return "eof";
  }
  return NULL;
}

static const char *parse_code(w_seed_parse_issue_kind kind,
                              const char **construct) {
  switch (kind) {
    case W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN:
      *construct = "grammar owner";
      return "W-PARSE-0001";
    case W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE:
      *construct = "grammar owner";
      return "W-PARSE-0002";
    case W_SEED_PARSE_ISSUE_NO_CONTINUATION_OWNER:
      *construct = "contextual continuation";
      return "W-PARSE-0004";
    case W_SEED_PARSE_ISSUE_MIXED_ROOT:
      *construct = "document root";
      return "W-PARSE-0006";
    case W_SEED_PARSE_ISSUE_SPACED_HEAD:
      *construct = "declaration head";
      return "W-PARSE-0013";
    case W_SEED_PARSE_ISSUE_VALUE_IF_MISSING_ELSE:
      *construct = "value if";
      return "W-PARSE-0021";
    case W_SEED_PARSE_ISSUE_NONE:
    case W_SEED_PARSE_ISSUE_UNSUPPORTED_ROOT:
    case W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM:
    case W_SEED_PARSE_ISSUE_FOREIGN_UNSUPPORTED:
    case W_SEED_PARSE_ISSUE_FOREIGN_SCANNER:
    case W_SEED_PARSE_ISSUE_LEXER:
    case W_SEED_PARSE_ISSUE_CAPACITY:
      break;
  }
  return NULL;
}

static size_t expected_count(uint32_t expected_mask) {
  size_t count = 0;
  if ((expected_mask & W_SEED_PARSE_EXPECT_WORD) != 0) count += 1;
  if ((expected_mask & W_SEED_PARSE_EXPECT_PUNCTUATION) != 0) count += 1;
  if ((expected_mask & W_SEED_PARSE_EXPECT_EXPRESSION) != 0) count += 1;
  if ((expected_mask & W_SEED_PARSE_EXPECT_STATEMENT) != 0) count += 1;
  return count;
}

static void write_expected(diagnostic_writer *writer, uint32_t expected_mask) {
  static const char *const names[] = {"word", "punctuation", "expression",
                                      "statement"};
  static const uint32_t masks[] = {
      W_SEED_PARSE_EXPECT_WORD, W_SEED_PARSE_EXPECT_PUNCTUATION,
      W_SEED_PARSE_EXPECT_EXPRESSION, W_SEED_PARSE_EXPECT_STATEMENT};
  writer_byte(writer, (uint8_t)'[');
  size_t emitted = 0;
  for (size_t index = 0; index < 4; index += 1) {
    if ((expected_mask & masks[index]) == 0) continue;
    if (emitted != 0) writer_byte(writer, (uint8_t)',');
    writer_json_string(writer, names[index], strlen(names[index]));
    emitted += 1;
  }
  writer_byte(writer, (uint8_t)']');
}

static void write_parse_record(diagnostic_writer *writer, const char *instance,
                               size_t instance_length, const char *source_id,
                               size_t source_id_length,
                               const w_seed_parse_issue *issue,
                               const char *code, const char *construct,
                               bool has_owner) {
  writer_text(writer, "{\"schemaVersion\":1,\"instance\":");
  writer_json_string(writer, instance, instance_length);
  writer_text(writer, ",\"code\":");
  writer_json_string(writer, code, strlen(code));
  writer_text(writer, ",\"phase\":\"source.parse\",\"severity\":\"error\",");
  write_primary(writer, source_id, source_id_length, issue->primary);
  writer_text(writer, ",\"labels\":[");
  if (has_owner) {
    write_label(writer, source_id, source_id_length, "owner", issue->owner);
  }
  writer_text(writer, "],\"facts\":{\"actual\":");
  const char *actual = actual_kind_name(issue->actual_kind);
  writer_json_string(writer, actual == NULL ? "unknown" : actual,
                     strlen(actual == NULL ? "unknown" : actual));
  writer_text(writer, ",\"construct\":");
  writer_json_string(writer, construct, strlen(construct));
  writer_text(writer, ",\"expected\":");
  write_expected(writer, issue->expected_mask);
  writer_byte(writer, (uint8_t)'}');
  write_common_tail(writer);
}

static bool valid_span_for_length(w_seed_span span, size_t length) {
  return span.start_byte <= span.end_byte && span.end_byte <= length;
}

static w_seed_diagnostic_status finish_lex_record(
    const char *instance, size_t instance_length, const char *source_id,
    size_t source_id_length, const w_seed_lex_error *error,
    const char *construct, const char *delimiter, uint8_t *output,
    size_t output_capacity, w_seed_diagnostic_result *result) {
  diagnostic_writer measure = {NULL, 0, 0, 0};
  write_lex_record(&measure, instance, instance_length, source_id,
                   source_id_length, error, construct, delimiter);
  result->required_bytes = measure.required;
  result->primary_byte = error->primary.start_byte;
  if (output == NULL || output_capacity < measure.required) {
    result->status = W_SEED_DIAGNOSTIC_CAPACITY;
    return result->status;
  }
  diagnostic_writer writer = {output, output_capacity, 0, 0};
  write_lex_record(&writer, instance, instance_length, source_id,
                   source_id_length, error, construct, delimiter);
  result->status = W_SEED_DIAGNOSTIC_OK;
  result->written_bytes = writer.written;
  return result->status;
}

w_seed_diagnostic_status w_seed_diagnostic_lex_record(
    const char *instance, size_t instance_length, const char *source_id,
    size_t source_id_length, const w_seed_lex_error *error, size_t source_length,
    uint8_t *output, size_t output_capacity,
    w_seed_diagnostic_result *result) {
  clear_result(result, W_SEED_DIAGNOSTIC_INVALID);
  if (result == NULL || !valid_instance(instance, instance_length) ||
      !valid_utf8_identity(source_id, source_id_length) || error == NULL) {
    if (result != NULL) result->status = W_SEED_DIAGNOSTIC_INVALID;
    return W_SEED_DIAGNOSTIC_INVALID;
  }
  const char *construct = NULL;
  const char *delimiter = NULL;
  if (error->kind == W_SEED_LEX_ERROR_UNTERMINATED_LITERAL) {
    if (!literal_profile(error->literal, &construct, &delimiter)) {
      result->status = W_SEED_DIAGNOSTIC_UNSUPPORTED;
      return result->status;
    }
  } else if (error->kind == W_SEED_LEX_ERROR_UNTERMINATED_COMMENT) {
    construct = "block-comment";
    delimiter = "block-comment-close";
  } else {
    result->status = W_SEED_DIAGNOSTIC_UNSUPPORTED;
    return result->status;
  }
  if (delimiter == NULL || !valid_span_for_length(error->primary, source_length) ||
      !valid_span_for_length(error->opening, source_length) ||
      error->opening.start_byte == error->opening.end_byte) {
    result->status = W_SEED_DIAGNOSTIC_UNSUPPORTED;
    return result->status;
  }
  return finish_lex_record(instance, instance_length, source_id,
                           source_id_length, error, construct, delimiter,
                           output, output_capacity, result);
}

static w_seed_diagnostic_status finish_parse_record(
    const char *instance, size_t instance_length, const char *source_id,
    size_t source_id_length, const w_seed_parse_issue *issue,
    const char *code, const char *construct, bool has_owner, uint8_t *output,
    size_t output_capacity, w_seed_diagnostic_result *result) {
  diagnostic_writer measure = {NULL, 0, 0, 0};
  write_parse_record(&measure, instance, instance_length, source_id,
                     source_id_length, issue, code, construct, has_owner);
  result->required_bytes = measure.required;
  result->primary_byte = issue->primary.start_byte;
  if (output == NULL || output_capacity < measure.required) {
    result->status = W_SEED_DIAGNOSTIC_CAPACITY;
    return result->status;
  }
  diagnostic_writer writer = {output, output_capacity, 0, 0};
  write_parse_record(&writer, instance, instance_length, source_id,
                     source_id_length, issue, code, construct, has_owner);
  result->status = W_SEED_DIAGNOSTIC_OK;
  result->written_bytes = writer.written;
  return result->status;
}

w_seed_diagnostic_status w_seed_diagnostic_parse_record(
    const char *instance, size_t instance_length, const char *source_id,
    size_t source_id_length, const w_seed_parse_issue *issue, size_t source_length,
    uint8_t *output, size_t output_capacity,
    w_seed_diagnostic_result *result) {
  clear_result(result, W_SEED_DIAGNOSTIC_INVALID);
  if (result == NULL || !valid_instance(instance, instance_length) ||
      !valid_utf8_identity(source_id, source_id_length) || issue == NULL ||
      !valid_span_for_length(issue->primary, source_length) ||
      expected_count(issue->expected_mask) == 0) {
    if (result != NULL) result->status = W_SEED_DIAGNOSTIC_INVALID;
    return W_SEED_DIAGNOSTIC_INVALID;
  }
  const char *construct = NULL;
  const char *code = parse_code(issue->kind, &construct);
  const char *actual = actual_kind_name(issue->actual_kind);
  if (code == NULL || construct == NULL || actual == NULL) {
    result->status = W_SEED_DIAGNOSTIC_UNSUPPORTED;
    return result->status;
  }
  const bool has_owner = issue->owner.start_byte != issue->owner.end_byte;
  if (has_owner && !valid_span_for_length(issue->owner, source_length)) {
    result->status = W_SEED_DIAGNOSTIC_UNSUPPORTED;
    return result->status;
  }
  return finish_parse_record(instance, instance_length, source_id,
                             source_id_length, issue, code, construct,
                             has_owner, output, output_capacity, result);
}

static bool is_semantic_type_diagnostic(
    const w_seed_frontend_diagnostic *diagnostic) {
  static const char code[] = "W-SEM-0001";
  return diagnostic != NULL &&
         diagnostic->kind == W_SEED_FRONTEND_DIAGNOSTIC_SEMANTIC &&
         diagnostic->code.data != NULL && diagnostic->code.length == 10u &&
         memcmp(diagnostic->code.data, code, sizeof(code) - 1u) == 0;
}

static bool valid_frontend_fact_text(w_seed_frontend_text text) {
  return text.data != NULL && text.length != 0u &&
         valid_utf8_identity(text.data, text.length);
}

static void write_frontend_record(
    diagnostic_writer *writer, const char *instance, size_t instance_length,
    const char *source_id, size_t source_id_length,
    const w_seed_frontend_diagnostic *diagnostic) {
  writer_text(writer, "{\"schemaVersion\":1,\"instance\":");
  writer_json_string(writer, instance, instance_length);
  writer_text(writer,
              ",\"code\":\"W-SEM-0001\",\"phase\":\"semantic.type\","
              "\"severity\":\"error\",\"primary\":{\"source\":");
  writer_json_string(writer, source_id, source_id_length);
  writer_text(writer, ",\"startByte\":");
  writer_decimal(writer, diagnostic->primary.start_byte);
  writer_text(writer, ",\"endByte\":");
  writer_decimal(writer, diagnostic->primary.end_byte);
  writer_text(writer, "},\"labels\":[],\"facts\":{\"actual\":");
  writer_json_string(writer, diagnostic->actual.data, diagnostic->actual.length);
  writer_text(writer, ",\"expected\":");
  writer_json_string(writer, diagnostic->expected.data,
                     diagnostic->expected.length);
  writer_text(writer, "},\"notes\":[],\"fixes\":[],\"root\":null}");
}

w_seed_diagnostic_status w_seed_diagnostic_frontend_record(
    const char *instance, size_t instance_length, const char *source_id,
    size_t source_id_length, const w_seed_source *source,
    const w_seed_frontend_diagnostic *diagnostic, uint8_t *output,
    size_t output_capacity, w_seed_diagnostic_result *result) {
  clear_result(result, W_SEED_DIAGNOSTIC_INVALID);
  if (result == NULL || !valid_instance(instance, instance_length) ||
      !valid_utf8_identity(source_id, source_id_length) || source == NULL ||
      diagnostic == NULL) {
    if (result != NULL) result->status = W_SEED_DIAGNOSTIC_INVALID;
    return W_SEED_DIAGNOSTIC_INVALID;
  }
  if (!is_semantic_type_diagnostic(diagnostic)) {
    result->status = W_SEED_DIAGNOSTIC_UNSUPPORTED;
    return result->status;
  }
  if (diagnostic->document_index != 0u) {
    result->status = W_SEED_DIAGNOSTIC_UNSUPPORTED;
    return result->status;
  }
  if (!valid_frontend_fact_text(diagnostic->actual) ||
      !valid_frontend_fact_text(diagnostic->expected)) {
    result->status = W_SEED_DIAGNOSTIC_INVALID;
    return result->status;
  }
  w_seed_source_error source_error;
  if (!w_seed_source_validate_span(source, diagnostic->primary,
                                   &source_error)) {
    result->status = W_SEED_DIAGNOSTIC_UNSUPPORTED;
    return result->status;
  }

  diagnostic_writer measure = {NULL, 0, 0, 0};
  write_frontend_record(&measure, instance, instance_length, source_id,
                        source_id_length, diagnostic);
  result->required_bytes = measure.required;
  result->primary_byte = diagnostic->primary.start_byte;
  if (output == NULL || output_capacity < measure.required) {
    result->status = W_SEED_DIAGNOSTIC_CAPACITY;
    return result->status;
  }
  diagnostic_writer writer = {output, output_capacity, 0, 0};
  write_frontend_record(&writer, instance, instance_length, source_id,
                        source_id_length, diagnostic);
  result->status = W_SEED_DIAGNOSTIC_OK;
  result->written_bytes = writer.written;
  return result->status;
}
