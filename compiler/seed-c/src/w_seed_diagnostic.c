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

typedef struct {
  const char *key;
  size_t length;
  w_seed_frontend_diagnostic_fact_kind kind;
} frontend_fact_spec;

typedef struct {
  const char *role;
  size_t length;
  size_t minimum;
  size_t maximum;
} frontend_label_spec;

typedef struct {
  const char *code;
  size_t length;
  const char *phase;
  const frontend_fact_spec *facts;
  size_t fact_count;
  const frontend_label_spec *labels;
  size_t label_count;
} frontend_code_profile;

#define FRONTEND_FACT_SPEC(name, fact_kind) \
  {name, sizeof(name) - 1u, fact_kind}
#define FRONTEND_LABEL_SPEC(name, minimum_count, maximum_count) \
  {name, sizeof(name) - 1u, minimum_count, maximum_count}

static const frontend_fact_spec frontend_sem_facts[] = {
    FRONTEND_FACT_SPEC("actual", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("expected", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_type0120_facts[] = {
    FRONTEND_FACT_SPEC("leftType", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("rightType", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_type0121_facts[] = {
    FRONTEND_FACT_SPEC("actualCase", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("allowedCases", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET),
    FRONTEND_FACT_SPEC("baseEnum", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("expectedType", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_type0122_facts[] = {
    FRONTEND_FACT_SPEC("actualType", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("candidateRoutes", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET),
    FRONTEND_FACT_SPEC("expectedType", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("reason", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_label0005_facts[] = {
    FRONTEND_FACT_SPEC("acceptedForms", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY),
    FRONTEND_FACT_SPEC("declaration", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("label", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_label0006_facts[] = {
    FRONTEND_FACT_SPEC("declaration", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("label", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("slot", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_match0001_facts[] = {
    FRONTEND_FACT_SPEC("missingCases", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET),
    FRONTEND_FACT_SPEC("subjectType", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_match0002_facts[] = {
    FRONTEND_FACT_SPEC("coveredBy", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("pattern", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("subjectType", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_match0003_facts[] = {
    FRONTEND_FACT_SPEC("context", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("expectedType", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("member", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_const0001_facts[] = {
    FRONTEND_FACT_SPEC("callChain", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY),
    FRONTEND_FACT_SPEC("operation", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("reason", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("symbol", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_contract0001_facts[] = {
    FRONTEND_FACT_SPEC("availableSlots", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET),
    FRONTEND_FACT_SPEC("head", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("slot", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_contract0002_facts[] = {
    FRONTEND_FACT_SPEC("actualKind", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("expectedKind", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("head", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("slot", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_contract0003_facts[] = {
    FRONTEND_FACT_SPEC("expectedType", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("head", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("predicateType", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_contract0004_facts[] = {
    FRONTEND_FACT_SPEC("head", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("slot", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("slotOrder", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY),
    FRONTEND_FACT_SPEC("violation", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_generic0001_facts[] = {
    FRONTEND_FACT_SPEC("domain", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("parameter", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("resolutionReason", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_generic0002_facts[] = {
    FRONTEND_FACT_SPEC("candidates", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET),
    FRONTEND_FACT_SPEC("equationSources", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET),
    FRONTEND_FACT_SPEC("parameter", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("reason", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};
static const frontend_fact_spec frontend_generic0003_facts[] = {
    FRONTEND_FACT_SPEC("externalLabel", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("kind", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("parameter", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
    FRONTEND_FACT_SPEC("position", W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER),
    FRONTEND_FACT_SPEC("reason", W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING),
};

static const frontend_label_spec frontend_branch_result_labels[] = {
    FRONTEND_LABEL_SPEC("branch-result", 2u, SIZE_MAX),
};
static const frontend_label_spec frontend_expected_type_labels[] = {
    FRONTEND_LABEL_SPEC("expected-type", 1u, 1u),
};
static const frontend_label_spec frontend_call_owner_expected_labels[] = {
    FRONTEND_LABEL_SPEC("call-owner", 1u, 1u),
    FRONTEND_LABEL_SPEC("expected-type", 1u, 1u),
};
static const frontend_label_spec frontend_match_subject_labels[] = {
    FRONTEND_LABEL_SPEC("match-subject", 1u, 1u),
};
static const frontend_label_spec frontend_match_case_subject_labels[] = {
    FRONTEND_LABEL_SPEC("covered-case", 1u, 1u),
    FRONTEND_LABEL_SPEC("match-subject", 1u, 1u),
};
static const frontend_label_spec frontend_const_owner_labels[] = {
    FRONTEND_LABEL_SPEC("const-owner", 1u, 1u),
};
static const frontend_label_spec frontend_contract_labels[] = {
    FRONTEND_LABEL_SPEC("contract-head", 1u, 1u),
    FRONTEND_LABEL_SPEC("slot-declaration", 0u, 1u),
};
static const frontend_label_spec frontend_generic_parameter_labels[] = {
    FRONTEND_LABEL_SPEC("generic-parameter", 1u, 1u),
};
static const frontend_label_spec frontend_generic_call_labels[] = {
    FRONTEND_LABEL_SPEC("call-owner", 1u, 1u),
    FRONTEND_LABEL_SPEC("generic-parameter", 1u, 1u),
};

static const frontend_code_profile frontend_profiles[] = {
    {"W-SEM-0001", 10u, "semantic.type", frontend_sem_facts, 2u, NULL, 0u},
    {"W-TYPE-0120", 11u, "semantic.type", frontend_type0120_facts, 2u,
     frontend_branch_result_labels, 1u},
    {"W-TYPE-0121", 11u, "semantic.type", frontend_type0121_facts, 4u,
     frontend_expected_type_labels, 1u},
    {"W-TYPE-0122", 11u, "semantic.type", frontend_type0122_facts, 4u,
     frontend_call_owner_expected_labels, 2u},
    {"W-LABEL-0005", 12u, "semantic.type", frontend_label0005_facts, 3u,
     NULL, 0u},
    {"W-LABEL-0006", 12u, "semantic.type", frontend_label0006_facts, 3u,
     NULL, 0u},
    {"W-MATCH-0001", 12u, "semantic.flow", frontend_match0001_facts, 2u,
     frontend_match_subject_labels, 1u},
    {"W-MATCH-0002", 12u, "semantic.flow", frontend_match0002_facts, 3u,
     frontend_match_case_subject_labels, 2u},
    {"W-MATCH-0003", 12u, "semantic.type", frontend_match0003_facts, 3u,
     NULL, 0u},
    {"W-CONST-0001", sizeof("W-CONST-0001") - 1u, "semantic.const", frontend_const0001_facts, 4u,
     frontend_const_owner_labels, 1u},
    {"W-CONTRACT-0001", sizeof("W-CONTRACT-0001") - 1u, "semantic.type", frontend_contract0001_facts, 3u,
     frontend_contract_labels, 2u},
    {"W-CONTRACT-0002", sizeof("W-CONTRACT-0002") - 1u, "semantic.type", frontend_contract0002_facts, 4u,
     frontend_contract_labels, 2u},
    {"W-CONTRACT-0003", sizeof("W-CONTRACT-0003") - 1u, "semantic.type", frontend_contract0003_facts, 3u,
     frontend_contract_labels, 2u},
    {"W-CONTRACT-0004", sizeof("W-CONTRACT-0004") - 1u, "semantic.type", frontend_contract0004_facts, 4u,
     frontend_contract_labels, 2u},
    {"W-GENERIC-0001", sizeof("W-GENERIC-0001") - 1u, "semantic.type", frontend_generic0001_facts, 3u,
     frontend_generic_parameter_labels, 1u},
    {"W-GENERIC-0002", sizeof("W-GENERIC-0002") - 1u, "semantic.type", frontend_generic0002_facts, 4u,
     frontend_generic_call_labels, 2u},
    {"W-GENERIC-0003", sizeof("W-GENERIC-0003") - 1u, "semantic.type", frontend_generic0003_facts, 5u,
     frontend_generic_parameter_labels, 1u},
};

_Static_assert(sizeof(frontend_profiles) / sizeof(frontend_profiles[0]) == 17u,
               "frontend diagnostic profiles must cover the active set");

#undef FRONTEND_FACT_SPEC
#undef FRONTEND_LABEL_SPEC

static const frontend_code_profile *frontend_profile_for(
    w_seed_frontend_text code) {
  for (size_t index = 0u;
       index < sizeof(frontend_profiles) / sizeof(frontend_profiles[0]);
       index += 1u) {
    if (code.length == frontend_profiles[index].length &&
        memcmp(code.data, frontend_profiles[index].code, code.length) == 0) {
      return &frontend_profiles[index];
    }
  }
  return NULL;
}

static bool valid_frontend_text(w_seed_frontend_text text, bool allow_empty) {
  if (!allow_empty && text.length == 0u) return false;
  if (text.length != 0u && text.data == NULL) return false;
  return valid_utf8_identity(text.data, text.length);
}

static bool frontend_source_valid(const w_seed_source *source) {
  if (source == NULL) return false;
  w_seed_source canonical = {0};
  w_seed_source_error error;
  if (!w_seed_source_init(source->bytes, &canonical, &error)) return false;
  return canonical.line_count == source->line_count &&
         canonical.bom_length == source->bom_length;
}

static int compare_frontend_text(w_seed_frontend_text left,
                                 w_seed_frontend_text right) {
  const size_t common = left.length < right.length ? left.length : right.length;
  const int comparison = common == 0u
                              ? 0
                              : memcmp(left.data, right.data, common);
  if (comparison != 0) return comparison;
  if (left.length < right.length) return -1;
  if (left.length > right.length) return 1;
  return 0;
}

static void writer_signed(diagnostic_writer *writer, int64_t value) {
  uint64_t magnitude = value < 0
                           ? (uint64_t)(-(value + 1)) + UINT64_C(1)
                           : (uint64_t)value;
  if (value < 0) writer_byte(writer, (uint8_t)'-');
  char digits[32];
  size_t length = 0u;
  do {
    digits[length] = (char)('0' + (magnitude % UINT64_C(10)));
    magnitude /= UINT64_C(10);
    length += 1u;
  } while (magnitude != 0u);
  while (length != 0u) {
    length -= 1u;
    writer_byte(writer, (uint8_t)digits[length]);
  }
}

static bool frontend_item_range_valid(
    const w_seed_frontend_output *frontend,
    const w_seed_frontend_diagnostic_fact *fact,
    size_t item_count_limit) {
  if (fact->item_count == 0u) {
    return fact->first_item == W_SEED_FRONTEND_NONE;
  }
  if (fact->first_item == W_SEED_FRONTEND_NONE ||
      (size_t)fact->first_item > item_count_limit ||
      (size_t)fact->item_count > item_count_limit - (size_t)fact->first_item ||
      frontend->diagnostic_items == NULL) {
    return false;
  }
  return true;
}

static bool frontend_validate_diagnostic(
    const w_seed_diagnostic_frontend_context *context,
    const w_seed_frontend_diagnostic *diagnostic,
    const frontend_code_profile *profile) {
  if (context == NULL || diagnostic == NULL || profile == NULL ||
      context->frontend_output == NULL || context->sources == NULL ||
      context->source_ids == NULL || context->source_count == 0u ||
      diagnostic->document_index >= context->source_count ||
      !valid_frontend_text(diagnostic->code, false) ||
      diagnostic->code.length != profile->length ||
      memcmp(diagnostic->code.data, profile->code, profile->length) != 0 ||
      diagnostic->fact_count != profile->fact_count) {
    return false;
  }
  const w_seed_frontend_output *frontend = context->frontend_output;
  const size_t diagnostic_limit = context->diagnostic_count;
  if (context->diagnostic_count > frontend->diagnostic_capacity ||
      context->diagnostic_fact_count > frontend->diagnostic_fact_capacity ||
      context->diagnostic_item_count > frontend->diagnostic_item_capacity ||
      context->diagnostic_label_count > frontend->diagnostic_label_capacity) {
    return false;
  }
  if (diagnostic->first_fact == W_SEED_FRONTEND_NONE ||
      (size_t)diagnostic->first_fact > frontend->diagnostic_fact_capacity ||
      (size_t)diagnostic->fact_count >
          frontend->diagnostic_fact_capacity - (size_t)diagnostic->first_fact ||
      diagnostic->first_label == W_SEED_FRONTEND_NONE ||
      (size_t)diagnostic->first_label > frontend->diagnostic_label_capacity ||
      (size_t)diagnostic->label_count >
          frontend->diagnostic_label_capacity - (size_t)diagnostic->first_label ||
      context->frontend_output->diagnostics == NULL ||
      diagnostic_limit == 0u) {
    return false;
  }
  const w_seed_frontend_text primary_source_id =
      context->source_ids[diagnostic->document_index];
  for (size_t source_index = 0u; source_index < context->source_count;
       source_index += 1u) {
    if (!frontend_source_valid(&context->sources[source_index])) return false;
    const w_seed_frontend_text source_id = context->source_ids[source_index];
    if (!valid_frontend_text(source_id, false)) {
      return false;
    }
    for (size_t prior = 0u; prior < source_index; prior += 1u) {
      if (compare_frontend_text(context->source_ids[prior], source_id) == 0) {
        return false;
      }
    }
  }
  if (!valid_frontend_text(primary_source_id, false)) {
    return false;
  }
  w_seed_source_error source_error;
  if (!w_seed_source_validate_span(&context->sources[diagnostic->document_index],
                                   diagnostic->primary, &source_error)) {
    return false;
  }
  const size_t item_limit = context->diagnostic_item_count;
  const size_t fact_limit = context->diagnostic_fact_count;
  const size_t label_limit = context->diagnostic_label_count;
  if ((profile->fact_count != 0u && frontend->diagnostic_facts == NULL) ||
      (diagnostic->label_count != 0u && frontend->diagnostic_labels == NULL)) {
    return false;
  }
  if ((size_t)diagnostic->first_fact > fact_limit ||
      (size_t)diagnostic->fact_count >
          fact_limit - (size_t)diagnostic->first_fact) {
    return false;
  }
  if ((size_t)diagnostic->first_label > label_limit ||
      (size_t)diagnostic->label_count >
          label_limit - (size_t)diagnostic->first_label) {
    return false;
  }
  for (size_t index = 0u; index < profile->fact_count; index += 1u) {
    const w_seed_frontend_diagnostic_fact *fact =
        &frontend->diagnostic_facts[(size_t)diagnostic->first_fact + index];
    const frontend_fact_spec *spec = &profile->facts[index];
    if (!valid_frontend_text(fact->key, false) ||
        fact->key.length != spec->length ||
        memcmp(fact->key.data, spec->key, spec->length) != 0 ||
        fact->kind != spec->kind ||
        !frontend_item_range_valid(frontend, fact, item_limit)) {
      return false;
    }
    if (fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER) {
      if (fact->text.data != NULL || fact->text.length != 0u ||
          fact->first_item != W_SEED_FRONTEND_NONE || fact->item_count != 0u) {
        return false;
      }
    } else if (fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING) {
      if (!valid_frontend_text(fact->text, false) || fact->integer_value != 0 ||
          fact->first_item != W_SEED_FRONTEND_NONE || fact->item_count != 0u) {
        return false;
      }
    } else {
      if (fact->text.data != NULL || fact->text.length != 0u ||
          fact->integer_value != 0) {
        return false;
      }
      for (size_t item = 0u; item < fact->item_count; item += 1u) {
        const w_seed_frontend_diagnostic_item *value =
            &frontend->diagnostic_items[(size_t)fact->first_item + item];
        if (!valid_frontend_text(value->text, false)) return false;
      if (fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET &&
          item != 0u &&
          compare_frontend_text(
              frontend->diagnostic_items[(size_t)fact->first_item + item - 1u]
                  .text,
              value->text) >= 0) {
          return false;
        }
      }
    }
  }
  for (size_t index = 0u; index < profile->label_count; index += 1u) {
    size_t count = 0u;
    for (size_t label_index = 0u; label_index < diagnostic->label_count;
         label_index += 1u) {
      const w_seed_frontend_diagnostic_label *label =
          &frontend->diagnostic_labels[(size_t)diagnostic->first_label +
                                       label_index];
      if (!valid_frontend_text(label->role, false) ||
          label->document_index >= context->source_count ||
          !w_seed_source_validate_span(
              &context->sources[label->document_index], label->span,
              &source_error)) {
        return false;
      }
      if (label->role.length == profile->labels[index].length &&
          memcmp(label->role.data, profile->labels[index].role,
                 profile->labels[index].length) == 0) {
        count += 1u;
      }
    }
    if (count < profile->labels[index].minimum ||
        count > profile->labels[index].maximum) {
      return false;
    }
  }
  size_t previous_role = 0u;
  bool have_previous_role = false;
  for (size_t label_index = 0u; label_index < diagnostic->label_count;
       label_index += 1u) {
    const w_seed_frontend_diagnostic_label *label =
        &frontend->diagnostic_labels[(size_t)diagnostic->first_label +
                                     label_index];
    bool known = false;
    size_t role_index = 0u;
    for (size_t index = 0u; index < profile->label_count; index += 1u) {
      if (label->role.length == profile->labels[index].length &&
          memcmp(label->role.data, profile->labels[index].role,
                 profile->labels[index].length) == 0) {
        known = true;
        role_index = index;
        break;
      }
    }
    if (!known) return false;
    if (have_previous_role && role_index < previous_role) return false;
    previous_role = role_index;
    have_previous_role = true;
  }
  return true;
}

static void write_frontend_fact_value(
    diagnostic_writer *writer, const w_seed_frontend_output *frontend,
    const w_seed_frontend_diagnostic_fact *fact) {
  if (fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER) {
    writer_signed(writer, fact->integer_value);
    return;
  }
  if (fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING) {
    writer_json_string(writer, fact->text.data, fact->text.length);
    return;
  }
  writer_byte(writer, (uint8_t)'[');
  for (size_t index = 0u; index < fact->item_count; index += 1u) {
    if (index != 0u) writer_byte(writer, (uint8_t)',');
    const w_seed_frontend_diagnostic_item *item =
        &frontend->diagnostic_items[(size_t)fact->first_item + index];
    writer_json_string(writer, item->text.data, item->text.length);
  }
  writer_byte(writer, (uint8_t)']');
}

static void write_frontend_record(
    diagnostic_writer *writer, const char *instance, size_t instance_length,
    const w_seed_diagnostic_frontend_context *context,
    const w_seed_frontend_diagnostic *diagnostic) {
  const w_seed_frontend_output *frontend = context->frontend_output;
  const w_seed_frontend_text primary_source_id =
      context->source_ids[diagnostic->document_index];
  const frontend_code_profile *profile = frontend_profile_for(diagnostic->code);
  writer_text(writer, "{\"schemaVersion\":1,\"instance\":");
  writer_json_string(writer, instance, instance_length);
  writer_text(writer, ",\"code\":");
  writer_json_string(writer, diagnostic->code.data, diagnostic->code.length);
  writer_text(writer, ",\"phase\":");
  writer_json_string(writer, profile->phase, strlen(profile->phase));
  writer_text(writer, ",\"severity\":\"error\",\"primary\":{\"source\":");
  writer_json_string(writer, primary_source_id.data, primary_source_id.length);
  writer_text(writer, ",\"startByte\":");
  writer_decimal(writer, diagnostic->primary.start_byte);
  writer_text(writer, ",\"endByte\":");
  writer_decimal(writer, diagnostic->primary.end_byte);
  writer_text(writer, "},\"labels\":[");
  for (size_t index = 0u; index < diagnostic->label_count; index += 1u) {
    if (index != 0u) writer_byte(writer, (uint8_t)',');
    const w_seed_frontend_diagnostic_label *label =
        &frontend->diagnostic_labels[(size_t)diagnostic->first_label + index];
    const w_seed_frontend_text label_source_id =
        context->source_ids[label->document_index];
    writer_text(writer, "{\"role\":");
    writer_json_string(writer, label->role.data, label->role.length);
    writer_text(writer, ",\"span\":{\"source\":");
    writer_json_string(writer, label_source_id.data, label_source_id.length);
    writer_text(writer, ",\"startByte\":");
    writer_decimal(writer, label->span.start_byte);
    writer_text(writer, ",\"endByte\":");
    writer_decimal(writer, label->span.end_byte);
    writer_text(writer, "}}");
  }
  writer_text(writer, "],\"facts\":{");
  for (size_t index = 0u; index < diagnostic->fact_count; index += 1u) {
    if (index != 0u) writer_byte(writer, (uint8_t)',');
    const w_seed_frontend_diagnostic_fact *fact =
        &frontend->diagnostic_facts[(size_t)diagnostic->first_fact + index];
    writer_json_string(writer, fact->key.data, fact->key.length);
    writer_byte(writer, (uint8_t)':');
    write_frontend_fact_value(writer, frontend, fact);
  }
  writer_text(writer, "},\"notes\":[],\"fixes\":[],\"root\":null}");
}

w_seed_diagnostic_status w_seed_diagnostic_frontend_record(
    const char *instance, size_t instance_length,
    const w_seed_diagnostic_frontend_context *context,
    size_t diagnostic_index, uint8_t *output, size_t output_capacity,
    w_seed_diagnostic_result *result) {
  clear_result(result, W_SEED_DIAGNOSTIC_INVALID);
  if (result == NULL || !valid_instance(instance, instance_length) ||
      context == NULL || context->frontend_output == NULL ||
      context->sources == NULL ||
      context->source_ids == NULL || context->source_count == 0u ||
      diagnostic_index >= context->frontend_output->diagnostic_capacity ||
      context->frontend_output->diagnostics == NULL) {
    if (result != NULL) result->status = W_SEED_DIAGNOSTIC_INVALID;
    return W_SEED_DIAGNOSTIC_INVALID;
  }
  if (diagnostic_index >= context->diagnostic_count) {
    result->status = W_SEED_DIAGNOSTIC_INVALID;
    return result->status;
  }
  const w_seed_frontend_diagnostic *diagnostic =
      &context->frontend_output->diagnostics[diagnostic_index];
  if (!valid_frontend_text(diagnostic->code, false)) {
    result->status = W_SEED_DIAGNOSTIC_INVALID;
    return result->status;
  }
  const frontend_code_profile *profile = frontend_profile_for(diagnostic->code);
  if (profile == NULL) {
    result->status = W_SEED_DIAGNOSTIC_UNSUPPORTED;
    return result->status;
  }
  if (!frontend_validate_diagnostic(context, diagnostic, profile)) {
    result->status = W_SEED_DIAGNOSTIC_INVALID;
    return result->status;
  }

  diagnostic_writer measure = {NULL, 0, 0, 0};
  write_frontend_record(&measure, instance, instance_length, context,
                        diagnostic);
  result->required_bytes = measure.required;
  result->primary_byte = diagnostic->primary.start_byte;
  if (output == NULL || output_capacity < measure.required) {
    result->status = W_SEED_DIAGNOSTIC_CAPACITY;
    return result->status;
  }
  diagnostic_writer writer = {output, output_capacity, 0, 0};
  write_frontend_record(&writer, instance, instance_length, context,
                        diagnostic);
  result->status = W_SEED_DIAGNOSTIC_OK;
  result->written_bytes = writer.written;
  return result->status;
}
