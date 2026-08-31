#include "w_seed_manifest.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "w_seed_sha256.h"
#include "w_seed_unicode.h"

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool empty;
} man0_range;

typedef enum {
  MAN0_MODE_MEASURE = 0,
  MAN0_MODE_EMIT,
} man0_mode;

typedef struct {
  const w_seed_manifest_source_input *source;
  const uint8_t *bytes;
  size_t length;
  size_t cursor;
  uint32_t document_index;
  uint32_t physical_root_ordinal;
  uint32_t depth;
  uint64_t scope_seed;
  uint64_t work;
  const w_seed_manifest_limits *limits;
  w_seed_manifest_scratch scratch;
  w_seed_manifest_counts *counts;
  w_seed_manifest_counts document_counts;
  w_seed_manifest_result *result;
  w_seed_manifest_phase phase;
  man0_mode mode;
  w_seed_manifest_output *output;
  uint32_t next_root;
  uint32_t next_node;
  uint32_t next_field;
  uint32_t next_edge;
  uint32_t next_canonical;
  bool package_seen;
  bool workspace_seen;
  const w_seed_manifest_program *verify_program;
  const w_seed_manifest_document *verify_document;
  uint32_t verify_root_match;
} man0_parser;

typedef struct {
  w_seed_span span;
  size_t decoded_length;
} man0_string;

typedef enum {
  MAN0_NUMBER_DECIMAL = 0,
  MAN0_NUMBER_BINARY,
  MAN0_NUMBER_OCTAL,
  MAN0_NUMBER_HEX,
} man0_number_form;

typedef enum {
  MAN0_NUMERIC_NUMBER = 0,
  MAN0_NUMERIC_SIZE,
  MAN0_NUMERIC_QUANTITY,
} man0_numeric_kind;

typedef struct {
  w_seed_span span;
  w_seed_span core_span;
  w_seed_span suffix_span;
  w_seed_span unit_span;
  man0_number_form form;
  man0_numeric_kind kind;
  uint32_t digit_count;
} man0_numeric;

typedef struct {
  uint8_t *destination;
  const uint8_t *comparison;
  size_t capacity;
  size_t length;
  bool compare_only;
} man0_writer;

static bool add_size(size_t left, size_t right, size_t *sum) {
  if (sum == NULL || left > SIZE_MAX - right) return false;
  *sum = left + right;
  return true;
}

static bool multiply_size(size_t left, size_t right, size_t *product) {
  if (product == NULL || (right != 0u && left > SIZE_MAX / right)) return false;
  *product = left * right;
  return true;
}

static bool add_u32(uint32_t left, uint32_t right, uint32_t *sum) {
  if (sum == NULL || left > UINT32_MAX - right) return false;
  *sum = left + right;
  return true;
}

static bool range_from(const void *pointer, size_t count, size_t element_size,
                       man0_range *range) {
  if (range == NULL) return false;
  range->start = (uintptr_t)pointer;
  range->end = (uintptr_t)pointer;
  range->empty = count == 0u || element_size == 0u;
  if (range->empty) return true;
  if (pointer == NULL) return false;
  size_t bytes = 0u;
  if (!multiply_size(count, element_size, &bytes) ||
      (uintptr_t)pointer > UINTPTR_MAX - bytes)
    return false;
  range->end = (uintptr_t)pointer + bytes;
  return true;
}

static bool ranges_overlap(man0_range left, man0_range right) {
  if (left.empty || right.empty) return false;
  return left.start < right.end && right.start < left.end;
}

static bool bytes_zero(const uint8_t *bytes, size_t length) {
  if (bytes == NULL && length != 0u) return false;
  for (size_t index = 0u; index < length; index += 1u)
    if (bytes[index] != 0u) return false;
  return true;
}

static w_seed_manifest_result result_baseline(void) {
  w_seed_manifest_result result;
  (void)memset(&result, 0, sizeof(result));
  result.status = W_SEED_MANIFEST_INVALID;
  result.phase = W_SEED_MANIFEST_PHASE_VALIDATE;
  result.error = W_SEED_MANIFEST_ERROR_NONE;
  result.backend_status = W_SEED_MANIFEST_BACKEND_NOT_CALLED;
  result.backend_phase = W_SEED_MANIFEST_BACKEND_PHASE_NONE;
  result.owner_guard_status = W_SEED_OWNER_GUARD_OK;
  result.document_index = W_SEED_MANIFEST_NONE;
  result.candidate_index = W_SEED_MANIFEST_NONE;
  result.byte_offset = W_SEED_MANIFEST_NO_BYTE;
  (void)memcpy(result.schema, W_SEED_MANIFEST_SCHEMA_VERSION,
               sizeof(result.schema));
  return result;
}

static w_seed_manifest_result fail_result(w_seed_manifest_result result,
                                          w_seed_manifest_status status,
                                          w_seed_manifest_phase phase,
                                          w_seed_manifest_error_kind error,
                                          uint32_t document_index,
                                          size_t byte_offset) {
  result.status = status;
  result.phase = phase;
  result.error = error;
  result.document_index = document_index;
  result.byte_offset = byte_offset;
  (void)memset(&result.written, 0, sizeof(result.written));
  (void)memset(result.semantic_digest, 0, sizeof(result.semantic_digest));
  (void)memset(result.provenance_digest, 0, sizeof(result.provenance_digest));
  (void)memset(result.receipt_digest, 0, sizeof(result.receipt_digest));
  return result;
}

w_seed_manifest_limits w_seed_manifest_default_limits(void) {
  return (w_seed_manifest_limits){
      W_SEED_MANIFEST_MAX_DOCUMENT_BYTES,
      W_SEED_MANIFEST_MAX_AGGREGATE_BYTES,
      W_SEED_MANIFEST_MAX_NESTING,
      W_SEED_MANIFEST_MAX_STRUCTURAL_NODES,
      W_SEED_MANIFEST_MAX_ROOTS_PER_DOCUMENT,
      W_SEED_MANIFEST_MAX_DOCUMENTS,
      W_SEED_MANIFEST_MAX_SCALAR_SOURCE_BYTES,
      W_SEED_MANIFEST_MAX_NUMBER_DIGITS,
      W_SEED_MANIFEST_MAX_DECODED_SCALAR_BYTES,
      W_SEED_MANIFEST_MAX_CANONICAL_BYTES,
      W_SEED_MANIFEST_MAX_WORK_UNITS,
  };
}

static bool limits_valid(w_seed_manifest_limits value) {
  return value.max_document_bytes != 0u &&
         value.max_document_bytes <= W_SEED_MANIFEST_MAX_DOCUMENT_BYTES &&
         value.max_aggregate_bytes != 0u &&
         value.max_aggregate_bytes <= W_SEED_MANIFEST_MAX_AGGREGATE_BYTES &&
         value.max_nesting != 0u &&
         value.max_nesting <= W_SEED_MANIFEST_MAX_NESTING &&
         value.max_structural_nodes != 0u &&
         value.max_structural_nodes <= W_SEED_MANIFEST_MAX_STRUCTURAL_NODES &&
         value.max_roots_per_document != 0u &&
         value.max_roots_per_document <=
             W_SEED_MANIFEST_MAX_ROOTS_PER_DOCUMENT &&
         value.max_documents != 0u &&
         value.max_documents <= W_SEED_MANIFEST_MAX_DOCUMENTS &&
         value.max_scalar_source_bytes != 0u &&
         value.max_scalar_source_bytes <= W_SEED_MANIFEST_MAX_SCALAR_SOURCE_BYTES &&
         value.max_number_digits != 0u &&
         value.max_number_digits <= W_SEED_MANIFEST_MAX_NUMBER_DIGITS &&
         value.max_decoded_scalar_bytes != 0u &&
         value.max_decoded_scalar_bytes <=
             W_SEED_MANIFEST_MAX_DECODED_SCALAR_BYTES &&
         value.max_canonical_bytes != 0u &&
         value.max_canonical_bytes <= W_SEED_MANIFEST_MAX_CANONICAL_BYTES &&
         value.max_work_units != 0u &&
         value.max_work_units <= W_SEED_MANIFEST_MAX_WORK_UNITS;
}

static bool binding_none(const w_seed_manifest_source_input *source) {
  return source != NULL &&
         source->binding_kind == W_SEED_MANIFEST_BINDING_NONE &&
         source->generation == 0u && source->candidate.generation == 0u &&
         source->candidate.directory_ordinal == 0u &&
         source->candidate.candidate_index == 0u &&
         bytes_zero(source->context_binding,
                    sizeof(source->context_binding)) &&
         bytes_zero(source->candidate_binding,
                    sizeof(source->candidate_binding));
}

static bool binding_owner(const w_seed_manifest_source_input *source) {
  return source != NULL &&
         source->binding_kind == W_SEED_MANIFEST_BINDING_OWNER_GUARD &&
         source->generation != 0u &&
         source->candidate.generation == source->generation &&
         source->candidate.directory_ordinal <= UINT32_MAX &&
         source->candidate.candidate_index <= UINT32_MAX;
}

static bool charge(man0_parser *parser, uint64_t amount) {
  if (parser == NULL) return false;
  if (amount > parser->limits->max_work_units ||
      parser->work > parser->limits->max_work_units - amount) {
    if (parser != NULL) {
      *parser->result = fail_result(*parser->result, W_SEED_MANIFEST_LIMIT,
                                    parser->phase,
                                    W_SEED_MANIFEST_ERROR_WORK_LIMIT,
                                    parser->document_index, parser->cursor);
    }
    return false;
  }
  parser->work += amount;
  return true;
}

static bool parser_fail(man0_parser *parser, w_seed_manifest_status status,
                        w_seed_manifest_error_kind error, size_t offset) {
  *parser->result = fail_result(*parser->result, status, parser->phase, error,
                                parser->document_index, offset);
  return false;
}

static bool advance(man0_parser *parser, size_t amount) {
  if (parser == NULL || amount > parser->length - parser->cursor)
    return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                       W_SEED_MANIFEST_ERROR_INVALID_TOKEN, parser->cursor);
  if (!charge(parser, (uint64_t)amount)) return false;
  parser->cursor += amount;
  return true;
}

static bool at_byte(const man0_parser *parser, uint8_t byte) {
  return parser->cursor < parser->length &&
         parser->bytes[parser->cursor] == byte;
}

static bool bytes_at(const man0_parser *parser, size_t offset,
                     const char *text, size_t length) {
  return offset <= parser->length && length <= parser->length - offset &&
         memcmp(parser->bytes + offset, text, length) == 0;
}

static bool consume_byte(man0_parser *parser, uint8_t byte) {
  if (!at_byte(parser, byte)) return false;
  return advance(parser, 1u);
}

static bool enter_nesting(man0_parser *parser, size_t offset) {
  if (parser->depth >= parser->limits->max_nesting)
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_NESTING_LIMIT, offset);
  parser->depth += 1u;
  return true;
}

static void leave_nesting(man0_parser *parser) { parser->depth -= 1u; }

static bool skip_trivia(man0_parser *parser) {
  for (;;) {
    if (parser->cursor >= parser->length) return true;
    const uint8_t byte = parser->bytes[parser->cursor];
    if (byte == (uint8_t)' ' || byte == (uint8_t)'\t' ||
        byte == (uint8_t)'\n') {
      if (!advance(parser, 1u)) return false;
      continue;
    }
    if (byte == (uint8_t)'\r') {
      const size_t where = parser->cursor;
      if (!bytes_at(parser, where, "\r\n", 2u))
        return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                           W_SEED_MANIFEST_ERROR_INVALID_TOKEN, where);
      if (!advance(parser, 2u)) return false;
      continue;
    }
    if (bytes_at(parser, parser->cursor, "//", 2u)) {
      if (!advance(parser, 2u)) return false;
      while (parser->cursor < parser->length &&
             parser->bytes[parser->cursor] != (uint8_t)'\n' &&
             parser->bytes[parser->cursor] != (uint8_t)'\r')
        if (!advance(parser, 1u)) return false;
      continue;
    }
    if (bytes_at(parser, parser->cursor, "/*", 2u)) {
      const size_t start = parser->cursor;
      uint32_t depth = 1u;
      if (!advance(parser, 2u)) return false;
      while (parser->cursor < parser->length && depth != 0u) {
        if (bytes_at(parser, parser->cursor, "/*", 2u)) {
          if (depth >= parser->limits->max_nesting)
            return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                               W_SEED_MANIFEST_ERROR_NESTING_LIMIT,
                               parser->cursor);
          depth += 1u;
          if (!advance(parser, 2u)) return false;
        } else if (bytes_at(parser, parser->cursor, "*/", 2u)) {
          depth -= 1u;
          if (!advance(parser, 2u)) return false;
        } else if (!advance(parser, 1u)) {
          return false;
        }
      }
      if (depth != 0u)
        return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                           W_SEED_MANIFEST_ERROR_UNTERMINATED_COMMENT, start);
      continue;
    }
    return true;
  }
}

static uint32_t decode_utf8(const uint8_t *bytes, size_t length, size_t offset,
                            size_t *width) {
  if (bytes == NULL || width == NULL || offset >= length) {
    if (width != NULL) *width = 0u;
    return UINT32_MAX;
  }
  const uint8_t first = bytes[offset];
  if (first < 0x80u) {
    *width = 1u;
    return first;
  }
  if (first < 0xe0u) {
    if (first < 0xc2u || length - offset < 2u ||
        (bytes[offset + 1u] & 0xc0u) != 0x80u) {
      *width = 0u;
      return UINT32_MAX;
    }
    *width = 2u;
    return ((uint32_t)(first & 0x1fu) << 6u) |
           (uint32_t)(bytes[offset + 1u] & 0x3fu);
  }
  if (first < 0xf0u) {
    if (length - offset < 3u ||
        (bytes[offset + 1u] & 0xc0u) != 0x80u ||
        (bytes[offset + 2u] & 0xc0u) != 0x80u ||
        (first == 0xe0u && bytes[offset + 1u] < 0xa0u) ||
        (first == 0xedu && bytes[offset + 1u] >= 0xa0u)) {
      *width = 0u;
      return UINT32_MAX;
    }
    *width = 3u;
    return ((uint32_t)(first & 0x0fu) << 12u) |
           ((uint32_t)(bytes[offset + 1u] & 0x3fu) << 6u) |
           (uint32_t)(bytes[offset + 2u] & 0x3fu);
  }
  if (first > 0xf4u || length - offset < 4u ||
      (bytes[offset + 1u] & 0xc0u) != 0x80u ||
      (bytes[offset + 2u] & 0xc0u) != 0x80u ||
      (bytes[offset + 3u] & 0xc0u) != 0x80u ||
      (first == 0xf0u && bytes[offset + 1u] < 0x90u) ||
      (first == 0xf4u && bytes[offset + 1u] >= 0x90u)) {
    *width = 0u;
    return UINT32_MAX;
  }
  *width = 4u;
  return ((uint32_t)(first & 0x07u) << 18u) |
         ((uint32_t)(bytes[offset + 1u] & 0x3fu) << 12u) |
         ((uint32_t)(bytes[offset + 2u] & 0x3fu) << 6u) |
         (uint32_t)(bytes[offset + 3u] & 0x3fu);
}

static bool ascii_identifier_start(uint8_t byte) {
  return byte == (uint8_t)'_' || (byte >= (uint8_t)'A' && byte <= (uint8_t)'Z') ||
         (byte >= (uint8_t)'a' && byte <= (uint8_t)'z');
}

static bool ascii_identifier_continue(uint8_t byte) {
  return ascii_identifier_start(byte) ||
         (byte >= (uint8_t)'0' && byte <= (uint8_t)'9');
}

static bool parse_identifier(man0_parser *parser, w_seed_span *span) {
  const size_t start = parser->cursor;
  if (start >= parser->length) return false;
  uint8_t byte = parser->bytes[start];
  if (byte < 0x80u) {
    if (!ascii_identifier_start(byte)) return false;
    if (!advance(parser, 1u)) return false;
  } else {
    size_t width = 0u;
    const uint32_t point = decode_utf8(parser->bytes, parser->length, start, &width);
    if (width == 0u || !w_seed_unicode_is_identifier_start(point)) return false;
    if (!advance(parser, width)) return false;
  }
  while (parser->cursor < parser->length) {
    byte = parser->bytes[parser->cursor];
    if (byte < 0x80u) {
      if (!ascii_identifier_continue(byte)) break;
      if (!advance(parser, 1u)) return false;
    } else {
      size_t width = 0u;
      const uint32_t point =
          decode_utf8(parser->bytes, parser->length, parser->cursor, &width);
      if (width == 0u || !w_seed_unicode_is_identifier_continue(point)) break;
      if (!advance(parser, width)) return false;
    }
  }
  span->start_byte = start;
  span->end_byte = parser->cursor;
  return true;
}

static bool span_text(const man0_parser *parser, w_seed_span span,
                      const char *text) {
  const size_t length = strlen(text);
  return span.end_byte - span.start_byte == length &&
         memcmp(parser->bytes + span.start_byte, text, length) == 0;
}

static uint64_t name_hash(const uint8_t *bytes, size_t length, uint64_t scope) {
  uint64_t hash = UINT64_C(1469598103934665603) ^ scope;
  for (size_t index = 0u; index < length; index += 1u) {
    hash ^= bytes[index];
    hash *= UINT64_C(1099511628211);
  }
  return hash == 0u ? UINT64_C(1) : hash;
}

static bool names_equal(man0_parser *parser, w_seed_span left,
                        w_seed_span right) {
  const size_t left_length = left.end_byte - left.start_byte;
  const size_t right_length = right.end_byte - right.start_byte;
  if (left_length != right_length) return false;
  if (!charge(parser, (uint64_t)left_length)) return false;
  return memcmp(parser->bytes + left.start_byte,
                parser->bytes + right.start_byte, left_length) == 0;
}

static bool insert_name(man0_parser *parser, uint64_t scope, w_seed_span name,
                        w_seed_manifest_error_kind duplicate_error) {
  const size_t capacity = parser->limits->max_structural_nodes;
  const size_t length = name.end_byte - name.start_byte;
  const uint64_t hash = name_hash(parser->bytes + name.start_byte, length, scope);
  size_t index = (size_t)(hash % (uint64_t)capacity);
  for (size_t probe = 0u; probe < capacity; probe += 1u) {
    w_seed_manifest_name_slot *slot = &parser->scratch.name_slots[index];
    if (!slot->occupied) {
      slot->occupied = true;
      slot->hash = hash;
      slot->scope = scope;
      slot->name_span = name;
      slot->has_name = true;
      return true;
    }
    if (slot->hash == hash && slot->scope == scope &&
        names_equal(parser, slot->name_span, name))
      return parser_fail(parser, W_SEED_MANIFEST_DUPLICATE, duplicate_error,
                         name.start_byte);
    if (parser->result->status == W_SEED_MANIFEST_LIMIT) return false;
    index = index + 1u == capacity ? 0u : index + 1u;
  }
  return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                     W_SEED_MANIFEST_ERROR_NODE_LIMIT, name.start_byte);
}

static bool count_structural(man0_parser *parser, uint32_t *family_count,
                             size_t offset) {
  if (parser->counts->structural_nodes >=
      parser->limits->max_structural_nodes)
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_NODE_LIMIT, offset);
  if (!add_u32(parser->counts->structural_nodes, 1u,
               &parser->counts->structural_nodes) ||
      !add_u32(parser->document_counts.structural_nodes, 1u,
               &parser->document_counts.structural_nodes) ||
      !add_u32(*family_count, 1u, family_count))
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_NODE_LIMIT, offset);
  return true;
}

static bool count_canonical(man0_parser *parser, size_t length, size_t offset) {
  if (length > UINT32_MAX || length > parser->limits->max_canonical_bytes ||
      parser->counts->canonical_bytes >
          parser->limits->max_canonical_bytes - length)
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_CANONICAL_LIMIT, offset);
  const uint32_t amount = (uint32_t)length;
  if (!add_u32(parser->counts->canonical_bytes, amount,
               &parser->counts->canonical_bytes) ||
      !add_u32(parser->document_counts.canonical_bytes, amount,
               &parser->document_counts.canonical_bytes))
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_CANONICAL_LIMIT, offset);
  return true;
}

static size_t utf8_encoded_length(uint32_t point) {
  if (point <= 0x7fu) return 1u;
  if (point <= 0x7ffu) return 2u;
  if (point <= 0xffffu) return 3u;
  return 4u;
}

static bool parse_hex_scalar(man0_parser *parser, uint32_t *point) {
  uint32_t value = 0u;
  size_t digits = 0u;
  while (parser->cursor < parser->length &&
         parser->bytes[parser->cursor] != (uint8_t)'}') {
    const uint8_t byte = parser->bytes[parser->cursor];
    uint32_t digit = 0u;
    if (byte >= (uint8_t)'0' && byte <= (uint8_t)'9')
      digit = (uint32_t)(byte - (uint8_t)'0');
    else if (byte >= (uint8_t)'a' && byte <= (uint8_t)'f')
      digit = 10u + (uint32_t)(byte - (uint8_t)'a');
    else if (byte >= (uint8_t)'A' && byte <= (uint8_t)'F')
      digit = 10u + (uint32_t)(byte - (uint8_t)'A');
    else
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         W_SEED_MANIFEST_ERROR_INVALID_STRING_ESCAPE,
                         parser->cursor);
    if (digits >= 6u || value > (UINT32_C(0x10ffff) - digit) / 16u)
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         W_SEED_MANIFEST_ERROR_INVALID_STRING_ESCAPE,
                         parser->cursor);
    value = value * 16u + digit;
    digits += 1u;
    if (!advance(parser, 1u)) return false;
  }
  if (digits == 0u || !consume_byte(parser, (uint8_t)'}') ||
      value > UINT32_C(0x10ffff) ||
      (value >= UINT32_C(0xd800) && value <= UINT32_C(0xdfff)))
    return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                       W_SEED_MANIFEST_ERROR_INVALID_STRING_ESCAPE,
                       parser->cursor);
  *point = value;
  return true;
}

static bool parse_string(man0_parser *parser, man0_string *string) {
  const size_t start = parser->cursor;
  if (!at_byte(parser, (uint8_t)'"')) return false;
  if (bytes_at(parser, start, "\"\"\"", 3u))
    return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                       W_SEED_MANIFEST_ERROR_EXECUTABLE_FORM, start);
  if (!advance(parser, 1u)) return false;
  size_t decoded = 0u;
  uint8_t *decoded_bytes = parser->scratch.bytes;
  const size_t decoded_capacity = parser->scratch.byte_capacity;
  while (parser->cursor < parser->length) {
    const uint8_t byte = parser->bytes[parser->cursor];
    if (byte == (uint8_t)'"') {
      if (!advance(parser, 1u)) return false;
      const size_t source_length = parser->cursor - start;
      if (source_length > parser->limits->max_scalar_source_bytes)
        return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                           W_SEED_MANIFEST_ERROR_SCALAR_SOURCE_LIMIT, start);
      string->span = (w_seed_span){start, parser->cursor};
      string->decoded_length = decoded;
      return true;
    }
    if (byte == (uint8_t)'\n' || byte == (uint8_t)'\r')
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         W_SEED_MANIFEST_ERROR_UNTERMINATED_STRING, start);
    size_t amount = 1u;
    uint32_t scalar = 0u;
    bool scalar_escape = false;
    uint8_t escaped_value = 0u;
    if (byte == (uint8_t)'\\') {
      const size_t escape = parser->cursor;
      if (!advance(parser, 1u)) return false;
      if (parser->cursor >= parser->length)
        return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                           W_SEED_MANIFEST_ERROR_UNTERMINATED_STRING, start);
      const uint8_t escaped = parser->bytes[parser->cursor];
      if (escaped == (uint8_t)'(')
        return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                           W_SEED_MANIFEST_ERROR_INTERPOLATION, escape);
      if (escaped == (uint8_t)'\\' || escaped == (uint8_t)'"' ||
          escaped == (uint8_t)'n' || escaped == (uint8_t)'r' ||
          escaped == (uint8_t)'t' || escaped == (uint8_t)'0') {
        escaped_value = escaped;
        if (!advance(parser, 1u)) return false;
      } else if (escaped == (uint8_t)'u' &&
                 bytes_at(parser, parser->cursor, "u{", 2u)) {
        if (!advance(parser, 2u)) return false;
        if (!parse_hex_scalar(parser, &scalar)) return false;
        amount = utf8_encoded_length(scalar);
        scalar_escape = true;
      } else {
        return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                           W_SEED_MANIFEST_ERROR_INVALID_STRING_ESCAPE,
                           parser->cursor);
      }
    } else if (byte < 0x80u) {
      if (!advance(parser, 1u)) return false;
    } else {
      size_t width = 0u;
      (void)decode_utf8(parser->bytes, parser->length, parser->cursor, &width);
      if (width == 0u)
        return parser_fail(parser, W_SEED_MANIFEST_UTF8,
                           W_SEED_MANIFEST_ERROR_INVALID_UTF8,
                           parser->cursor);
      amount = width;
      if (!advance(parser, width)) return false;
    }
    if (decoded > SIZE_MAX - amount ||
        decoded > parser->limits->max_decoded_scalar_bytes ||
        decoded > decoded_capacity ||
        amount > parser->limits->max_decoded_scalar_bytes - decoded ||
        amount > decoded_capacity - decoded)
      return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                         W_SEED_MANIFEST_ERROR_DECODED_SCALAR_LIMIT, start);
    if (decoded_bytes == NULL && amount != 0u)
      return parser_fail(parser, W_SEED_MANIFEST_CAPACITY,
                         W_SEED_MANIFEST_ERROR_NONE, start);
    if (scalar_escape) {
      if (scalar <= 0x7fu) {
        decoded_bytes[decoded] = (uint8_t)scalar;
      } else if (scalar <= 0x7ffu) {
        decoded_bytes[decoded] = (uint8_t)(0xc0u | (scalar >> 6u));
        decoded_bytes[decoded + 1u] = (uint8_t)(0x80u | (scalar & 0x3fu));
      } else if (scalar <= 0xffffu) {
        decoded_bytes[decoded] = (uint8_t)(0xe0u | (scalar >> 12u));
        decoded_bytes[decoded + 1u] =
            (uint8_t)(0x80u | ((scalar >> 6u) & 0x3fu));
        decoded_bytes[decoded + 2u] = (uint8_t)(0x80u | (scalar & 0x3fu));
      } else {
        decoded_bytes[decoded] = (uint8_t)(0xf0u | (scalar >> 18u));
        decoded_bytes[decoded + 1u] =
            (uint8_t)(0x80u | ((scalar >> 12u) & 0x3fu));
        decoded_bytes[decoded + 2u] =
            (uint8_t)(0x80u | ((scalar >> 6u) & 0x3fu));
        decoded_bytes[decoded + 3u] = (uint8_t)(0x80u | (scalar & 0x3fu));
      }
    } else if (byte == (uint8_t)'\\') {
      decoded_bytes[decoded] = escaped_value == (uint8_t)'n'
                                  ? (uint8_t)'\n'
                                  : escaped_value == (uint8_t)'r'
                                        ? (uint8_t)'\r'
                                        : escaped_value == (uint8_t)'t'
                                              ? (uint8_t)'\t'
                                              : escaped_value == (uint8_t)'0'
                                                    ? 0u
                                                    : escaped_value;
    } else {
      (void)memcpy(decoded_bytes + decoded,
                   parser->bytes + parser->cursor - amount, amount);
    }
    decoded += amount;
  }
  return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                     W_SEED_MANIFEST_ERROR_UNTERMINATED_STRING, start);
}

static bool digit_for_base(uint8_t byte, uint32_t base) {
  if (byte >= (uint8_t)'0' && byte <= (uint8_t)'9')
    return (uint32_t)(byte - (uint8_t)'0') < base;
  if (base == 16u && byte >= (uint8_t)'a' && byte <= (uint8_t)'f') return true;
  return base == 16u && byte >= (uint8_t)'A' && byte <= (uint8_t)'F';
}

static bool scan_digit_sequence(man0_parser *parser, uint32_t base,
                                uint32_t *digits) {
  bool previous_digit = false;
  uint32_t count = 0u;
  while (parser->cursor < parser->length) {
    const uint8_t byte = parser->bytes[parser->cursor];
    if (digit_for_base(byte, base)) {
      if (count == UINT32_MAX) return false;
      count += 1u;
      previous_digit = true;
      if (!advance(parser, 1u)) return false;
      continue;
    }
    if (byte == (uint8_t)'_' && previous_digit &&
        parser->cursor + 1u < parser->length &&
        digit_for_base(parser->bytes[parser->cursor + 1u], base)) {
      previous_digit = false;
      if (!advance(parser, 1u)) return false;
      continue;
    }
    break;
  }
  if (!previous_digit || count == 0u) return false;
  *digits += count;
  return true;
}

static bool parse_numeric(man0_parser *parser, man0_numeric *numeric) {
  const size_t start = parser->cursor;
  if (start >= parser->length ||
      parser->bytes[start] < (uint8_t)'0' ||
      parser->bytes[start] > (uint8_t)'9')
    return false;
  numeric->form = MAN0_NUMBER_DECIMAL;
  numeric->kind = MAN0_NUMERIC_NUMBER;
  numeric->suffix_span = (w_seed_span){start, start};
  numeric->unit_span = (w_seed_span){start, start};
  numeric->digit_count = 0u;

  uint32_t base = 10u;
  if (bytes_at(parser, start, "0b", 2u) ||
      bytes_at(parser, start, "0o", 2u) ||
      bytes_at(parser, start, "0x", 2u)) {
    const uint8_t prefix = parser->bytes[start + 1u];
    base = prefix == (uint8_t)'b' ? 2u : prefix == (uint8_t)'o' ? 8u : 16u;
    numeric->form = base == 2u   ? MAN0_NUMBER_BINARY
                    : base == 8u ? MAN0_NUMBER_OCTAL
                                 : MAN0_NUMBER_HEX;
    if (!advance(parser, 2u) ||
        !scan_digit_sequence(parser, base, &numeric->digit_count))
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         W_SEED_MANIFEST_ERROR_INVALID_TOKEN, start);
  } else {
    if (!scan_digit_sequence(parser, 10u, &numeric->digit_count))
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         W_SEED_MANIFEST_ERROR_INVALID_TOKEN, start);
    if (at_byte(parser, (uint8_t)'.') && parser->cursor + 1u < parser->length &&
        digit_for_base(parser->bytes[parser->cursor + 1u], 10u)) {
      if (!advance(parser, 1u) ||
          !scan_digit_sequence(parser, 10u, &numeric->digit_count))
        return false;
    }
    if (parser->cursor < parser->length &&
        (parser->bytes[parser->cursor] == (uint8_t)'e' ||
         parser->bytes[parser->cursor] == (uint8_t)'E')) {
      const size_t exponent = parser->cursor;
      if (!advance(parser, 1u)) return false;
      if (at_byte(parser, (uint8_t)'+') || at_byte(parser, (uint8_t)'-'))
        if (!advance(parser, 1u)) return false;
      const uint32_t before = numeric->digit_count;
      if (!scan_digit_sequence(parser, 10u, &numeric->digit_count))
        return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                           W_SEED_MANIFEST_ERROR_INVALID_TOKEN, exponent);
      if (numeric->digit_count == before) return false;
    }
  }
  numeric->core_span = (w_seed_span){start, parser->cursor};

  if (at_byte(parser, (uint8_t)'_')) {
    const size_t introducer = parser->cursor;
    if (!advance(parser, 1u)) return false;
    w_seed_span suffix;
    if (!parse_identifier(parser, &suffix))
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         W_SEED_MANIFEST_ERROR_INVALID_TOKEN, introducer);
    numeric->suffix_span = suffix;
  } else if (bytes_at(parser, parser->cursor, "KiB", 3u) ||
             bytes_at(parser, parser->cursor, "MiB", 3u) ||
             bytes_at(parser, parser->cursor, "GiB", 3u)) {
    numeric->kind = MAN0_NUMERIC_SIZE;
    numeric->unit_span = (w_seed_span){parser->cursor, parser->cursor + 3u};
    if (!advance(parser, 3u)) return false;
  } else if (at_byte(parser, (uint8_t)'B')) {
    numeric->kind = MAN0_NUMERIC_SIZE;
    numeric->unit_span = (w_seed_span){parser->cursor, parser->cursor + 1u};
    if (!advance(parser, 1u)) return false;
  } else if (at_byte(parser, (uint8_t)'<')) {
    numeric->kind = MAN0_NUMERIC_QUANTITY;
    if (!advance(parser, 1u)) return false;
    const size_t unit_start = parser->cursor;
    size_t unit_bytes = 0u;
    uint32_t parentheses = 0u;
    bool expect_term = true;
    bool can_exponent = false;
    while (parser->cursor < parser->length &&
           parser->bytes[parser->cursor] != (uint8_t)'>') {
      const uint8_t byte = parser->bytes[parser->cursor];
      if (byte == (uint8_t)' ' || byte == (uint8_t)'\t') {
        if (!advance(parser, 1u)) return false;
        continue;
      }
      if (expect_term && byte == (uint8_t)'(') {
        if (!enter_nesting(parser, parser->cursor)) return false;
        parentheses += 1u;
        unit_bytes += 1u;
        can_exponent = false;
        if (!advance(parser, 1u)) return false;
        continue;
      }
      if (expect_term) {
        w_seed_span unit_name;
        if (!parse_identifier(parser, &unit_name))
          return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                             W_SEED_MANIFEST_ERROR_INVALID_TOKEN,
                             parser->cursor);
        unit_bytes += unit_name.end_byte - unit_name.start_byte;
        while (at_byte(parser, (uint8_t)'.')) {
          unit_bytes += 1u;
          if (!advance(parser, 1u) || !parse_identifier(parser, &unit_name))
            return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                               W_SEED_MANIFEST_ERROR_INVALID_TOKEN,
                               parser->cursor);
          unit_bytes += unit_name.end_byte - unit_name.start_byte;
        }
        expect_term = false;
        can_exponent = true;
        continue;
      }
      if (byte == (uint8_t)'^' && can_exponent) {
        unit_bytes += 1u;
        if (!advance(parser, 1u)) return false;
        if (at_byte(parser, (uint8_t)'+') || at_byte(parser, (uint8_t)'-')) {
          unit_bytes += 1u;
          if (!advance(parser, 1u)) return false;
        }
        uint32_t exponent_digits = 0u;
        if (!scan_digit_sequence(parser, 10u, &exponent_digits))
          return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                             W_SEED_MANIFEST_ERROR_INVALID_TOKEN,
                             parser->cursor);
        unit_bytes += exponent_digits;
        can_exponent = false;
        continue;
      }
      if (byte == (uint8_t)')' && parentheses != 0u) {
        parentheses -= 1u;
        leave_nesting(parser);
        unit_bytes += 1u;
        can_exponent = true;
        if (!advance(parser, 1u)) return false;
        continue;
      }
      if (byte == (uint8_t)'*' || byte == (uint8_t)'/') {
        expect_term = true;
        can_exponent = false;
        unit_bytes += 1u;
        if (!advance(parser, 1u)) return false;
        continue;
      }
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         W_SEED_MANIFEST_ERROR_INVALID_TOKEN,
                         parser->cursor);
    }
    if (unit_bytes == 0u || expect_term || parentheses != 0u ||
        !at_byte(parser, (uint8_t)'>'))
        return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                           W_SEED_MANIFEST_ERROR_INVALID_TOKEN,
                           unit_start);
    numeric->unit_span = (w_seed_span){unit_start, parser->cursor};
    if (!advance(parser, 1u)) return false;
  }

  if (numeric->digit_count > parser->limits->max_number_digits)
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_NUMBER_DIGIT_LIMIT, start);
  if (parser->cursor - start > parser->limits->max_scalar_source_bytes)
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_SCALAR_SOURCE_LIMIT, start);
  if (parser->cursor < parser->length) {
    const uint8_t next = parser->bytes[parser->cursor];
    if (ascii_identifier_continue(next) || next >= 0x80u)
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         W_SEED_MANIFEST_ERROR_INVALID_TOKEN,
                         parser->cursor);
  }
  numeric->span = (w_seed_span){start, parser->cursor};
  return true;
}

static void write_u32_be(uint8_t *bytes, uint32_t value) {
  bytes[0] = (uint8_t)(value >> 24u);
  bytes[1] = (uint8_t)(value >> 16u);
  bytes[2] = (uint8_t)(value >> 8u);
  bytes[3] = (uint8_t)value;
}

static void write_u64_be(uint8_t *bytes, uint64_t value) {
  for (size_t index = 0u; index < 8u; index += 1u)
    bytes[index] = (uint8_t)(value >> (56u - (uint32_t)(index * 8u)));
}

static bool writer_append(man0_writer *writer, const uint8_t *bytes,
                          size_t amount) {
  if (writer == NULL || amount > SIZE_MAX - writer->length) return false;
  if (writer->compare_only) {
    if (writer->comparison == NULL || writer->length > writer->capacity ||
        amount > writer->capacity - writer->length ||
        (amount != 0u &&
         memcmp(writer->comparison + writer->length, bytes, amount) != 0))
      return false;
  } else if (writer->destination != NULL) {
    if (writer->length > writer->capacity || amount > writer->capacity - writer->length ||
        (amount != 0u && bytes == NULL))
      return false;
    if (amount != 0u)
      (void)memcpy(writer->destination + writer->length, bytes, amount);
  }
  writer->length += amount;
  return true;
}

static bool writer_frame_begin(man0_writer *writer, const char *tag,
                               size_t payload_length) {
  const size_t tag_length = strlen(tag);
  if (tag_length > UINT32_MAX) return false;
  uint8_t prefix[4];
  uint8_t payload_size[8];
  write_u32_be(prefix, (uint32_t)tag_length);
  write_u64_be(payload_size, (uint64_t)payload_length);
  return writer_append(writer, prefix, sizeof(prefix)) &&
         writer_append(writer, (const uint8_t *)tag, tag_length) &&
         writer_append(writer, payload_size, sizeof(payload_size));
}

static bool writer_frame(man0_writer *writer, const char *tag,
                         const uint8_t *payload, size_t payload_length) {
  return writer_frame_begin(writer, tag, payload_length) &&
         writer_append(writer, payload, payload_length);
}

static size_t u64_decimal(uint64_t value, uint8_t *digits) {
  uint8_t reverse[32];
  size_t length = 0u;
  do {
    reverse[length++] = (uint8_t)('0' + value % 10u);
    value /= 10u;
  } while (value != 0u);
  for (size_t index = 0u; index < length; index += 1u)
    digits[index] = reverse[length - 1u - index];
  return length;
}

static bool decimal_to_u64(const uint8_t *digits, size_t length,
                           uint64_t *value) {
  uint64_t result = 0u;
  for (size_t index = 0u; index < length; index += 1u) {
    const uint64_t digit = (uint64_t)(digits[index] - (uint8_t)'0');
    if (result > (UINT64_MAX - digit) / 10u) return false;
    result = result * 10u + digit;
  }
  *value = result;
  return true;
}

static bool decimal_add_u64(uint8_t *digits, size_t *length, size_t capacity,
                            uint64_t amount) {
  size_t index = *length;
  uint64_t carry = amount;
  while (index != 0u && carry != 0u) {
    index -= 1u;
    const uint64_t sum = (uint64_t)(digits[index] - (uint8_t)'0') +
                         carry % 10u;
    digits[index] = (uint8_t)('0' + sum % 10u);
    carry = carry / 10u + sum / 10u;
  }
  while (carry != 0u) {
    if (*length >= capacity) return false;
    (void)memmove(digits + 1u, digits, *length);
    digits[0] = (uint8_t)('0' + carry % 10u);
    *length += 1u;
    carry /= 10u;
  }
  return true;
}

static void decimal_subtract_u64(uint8_t *digits, size_t length,
                                 uint64_t amount) {
  size_t index = length;
  uint32_t borrow = 0u;
  while (index != 0u) {
    index -= 1u;
    const uint32_t subtrahend = (uint32_t)(amount % 10u) + borrow;
    amount /= 10u;
    const uint32_t digit = (uint32_t)(digits[index] - (uint8_t)'0');
    if (digit < subtrahend) {
      digits[index] = (uint8_t)('0' + digit + 10u - subtrahend);
      borrow = 1u;
    } else {
      digits[index] = (uint8_t)('0' + digit - subtrahend);
      borrow = 0u;
    }
  }
}

static void trim_decimal_leading(uint8_t *digits, size_t *length) {
  size_t leading = 0u;
  while (leading + 1u < *length && digits[leading] == (uint8_t)'0')
    leading += 1u;
  if (leading != 0u) {
    (void)memmove(digits, digits + leading, *length - leading);
    *length -= leading;
  }
}

typedef struct {
  uint8_t radix;
  const uint8_t *digits;
  size_t digits_length;
  const uint8_t *coefficient;
  size_t coefficient_length;
  const uint8_t *exponent;
  size_t exponent_length;
  bool exponent_negative;
} man0_numeric_parts;

static bool writer_quantity_frame(man0_writer *writer, const uint8_t *source,
                                  w_seed_span span) {
  size_t compact = 0u;
  for (size_t index = span.start_byte; index < span.end_byte; index += 1u)
    if (source[index] != (uint8_t)' ' && source[index] != (uint8_t)'\t') {
      if (compact == SIZE_MAX) return false;
      compact += 1u;
    }
  const size_t tag_length = sizeof("unit") - 1u;
  uint8_t prefix[4];
  uint8_t payload_size[8];
  write_u32_be(prefix, (uint32_t)tag_length);
  write_u64_be(payload_size, (uint64_t)compact);
  if (!writer_append(writer, prefix, sizeof(prefix)) ||
      !writer_append(writer, (const uint8_t *)"unit", tag_length) ||
      !writer_append(writer, payload_size, sizeof(payload_size)))
    return false;
  for (size_t index = span.start_byte; index < span.end_byte; index += 1u) {
    const uint8_t byte = source[index];
    if (byte != (uint8_t)' ' && byte != (uint8_t)'\t' &&
        !writer_append(writer, &byte, 1u))
      return false;
  }
  return true;
}

static bool normalize_numeric(man0_parser *parser, const man0_numeric *numeric,
                              uint8_t *destination, size_t destination_capacity,
                              bool compare_only, size_t *canonical_length) {
  uint8_t *buffer = parser->scratch.bytes;
  const size_t capacity = parser->scratch.byte_capacity;
  const size_t workspace = W_SEED_MANIFEST_SCALAR_SCRATCH_OVERHEAD / 2u;
  const uint8_t *source = parser->bytes;
  man0_numeric_parts parts;
  (void)memset(&parts, 0, sizeof(parts));
  parts.radix = 10u;
  if (canonical_length == NULL || capacity < workspace ||
      numeric->digit_count > parser->limits->max_decoded_scalar_bytes ||
      numeric->digit_count > capacity - workspace)
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_DECODED_SCALAR_LIMIT,
                       numeric->span.start_byte);
  const uint64_t scalar_length =
      (uint64_t)(numeric->span.end_byte - numeric->span.start_byte);
  if (scalar_length > (UINT64_MAX - UINT64_C(256)) / UINT64_C(8) ||
      !charge(parser, scalar_length * UINT64_C(8) + UINT64_C(256)))
    return false;

  if (numeric->form != MAN0_NUMBER_DECIMAL) {
    parts.radix = numeric->form == MAN0_NUMBER_BINARY
                      ? 2u
                      : numeric->form == MAN0_NUMBER_OCTAL ? 8u : 16u;
    size_t digit_length = 0u;
    for (size_t index = numeric->core_span.start_byte + 2u;
         index < numeric->core_span.end_byte; index += 1u) {
      uint8_t byte = source[index];
      if (byte == (uint8_t)'_') continue;
      if (byte >= (uint8_t)'A' && byte <= (uint8_t)'F')
        byte = (uint8_t)(byte - (uint8_t)'A' + (uint8_t)'a');
      buffer[workspace + digit_length] = byte;
      digit_length += 1u;
    }
    size_t leading = 0u;
    while (leading + 1u < digit_length &&
           buffer[workspace + leading] == (uint8_t)'0')
      leading += 1u;
    if (leading != 0u) {
      (void)memmove(buffer + workspace, buffer + workspace + leading,
                    digit_length - leading);
      digit_length -= leading;
    }
    parts.digits = buffer + workspace;
    parts.digits_length = digit_length;
  } else {
    bool fraction = false;
    bool exponent_part = false;
    bool explicit_negative = false;
    size_t fraction_digits = 0u;
    size_t coefficient_length = 0u;
    size_t exponent_length = 0u;
    size_t raw_coefficient_length = 0u;
    size_t raw_exponent_length = 0u;
    for (size_t index = numeric->core_span.start_byte;
         index < numeric->core_span.end_byte; index += 1u) {
      const uint8_t byte = source[index];
      if (byte == (uint8_t)'_') continue;
      if (!exponent_part && byte == (uint8_t)'.') {
        fraction = true;
        continue;
      }
      if (!exponent_part && (byte == (uint8_t)'e' || byte == (uint8_t)'E')) {
        exponent_part = true;
        continue;
      }
      if (exponent_part && (byte == (uint8_t)'+' || byte == (uint8_t)'-')) {
        explicit_negative = byte == (uint8_t)'-';
        continue;
      }
      if (!exponent_part) {
        if (raw_coefficient_length >= capacity - workspace)
          return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                             W_SEED_MANIFEST_ERROR_DECODED_SCALAR_LIMIT,
                             numeric->span.start_byte);
        buffer[workspace + raw_coefficient_length] = byte;
        raw_coefficient_length += 1u;
        if (fraction) fraction_digits += 1u;
      } else {
        if (raw_coefficient_length > capacity - workspace - raw_exponent_length)
          return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                             W_SEED_MANIFEST_ERROR_DECODED_SCALAR_LIMIT,
                             numeric->span.start_byte);
        buffer[workspace + raw_coefficient_length + raw_exponent_length] = byte;
        raw_exponent_length += 1u;
      }
    }
    coefficient_length = raw_coefficient_length;
    size_t leading = 0u;
    while (leading < coefficient_length &&
           buffer[workspace + leading] == (uint8_t)'0')
      leading += 1u;
    if (leading == coefficient_length) {
      buffer[workspace] = (uint8_t)'0';
      coefficient_length = 1u;
    } else {
      if (leading != 0u) {
        (void)memmove(buffer + workspace, buffer + workspace + leading,
                      coefficient_length - leading);
        coefficient_length -= leading;
      }
      while (coefficient_length > 1u &&
             buffer[workspace + coefficient_length - 1u] == (uint8_t)'0') {
        coefficient_length -= 1u;
      }
    }

    const bool coefficient_zero = buffer[workspace] == (uint8_t)'0' &&
                                  coefficient_length == 1u;
    const size_t trailing_zeros = coefficient_zero
        ? 0u
        : raw_coefficient_length - leading - coefficient_length;
    uint8_t *exponent_digits = buffer + workspace + raw_coefficient_length;
    exponent_length = raw_exponent_length;
    size_t exponent_leading = 0u;
    while (exponent_leading < exponent_length &&
           exponent_digits[exponent_leading] == (uint8_t)'0')
      exponent_leading += 1u;
    if (exponent_leading == exponent_length) {
      exponent_digits[0] = (uint8_t)'0';
      exponent_length = 1u;
      explicit_negative = false;
    } else if (exponent_leading != 0u) {
      (void)memmove(exponent_digits, exponent_digits + exponent_leading,
                    exponent_length - exponent_leading);
      exponent_length -= exponent_leading;
    }

    const int64_t adjustment = (int64_t)trailing_zeros -
                               (int64_t)fraction_digits;
    bool result_negative = explicit_negative;
    if (coefficient_zero) {
      exponent_digits[0] = (uint8_t)'0';
      exponent_length = 1u;
      result_negative = false;
    } else if (adjustment != 0) {
      const bool adjustment_negative = adjustment < 0;
      const uint64_t adjustment_magnitude = adjustment_negative
          ? (uint64_t)(-(adjustment + 1)) + 1u
          : (uint64_t)adjustment;
      const bool explicit_zero = exponent_length == 1u &&
                                 exponent_digits[0] == (uint8_t)'0';
      if (explicit_zero) {
        exponent_length = u64_decimal(adjustment_magnitude, exponent_digits);
        result_negative = adjustment_negative;
      } else if (result_negative == adjustment_negative) {
        if (!decimal_add_u64(exponent_digits, &exponent_length,
                             capacity - (workspace + raw_coefficient_length),
                             adjustment_magnitude))
          return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                             W_SEED_MANIFEST_ERROR_DECODED_SCALAR_LIMIT,
                             numeric->span.start_byte);
      } else {
        uint64_t explicit_magnitude = 0u;
        const bool fits = decimal_to_u64(exponent_digits, exponent_length,
                                         &explicit_magnitude);
        if (!fits || explicit_magnitude > adjustment_magnitude) {
          decimal_subtract_u64(exponent_digits, exponent_length,
                               adjustment_magnitude);
          trim_decimal_leading(exponent_digits, &exponent_length);
        } else if (explicit_magnitude == adjustment_magnitude) {
          exponent_digits[0] = (uint8_t)'0';
          exponent_length = 1u;
          result_negative = false;
        } else {
          exponent_length = u64_decimal(adjustment_magnitude -
                                            explicit_magnitude,
                                        exponent_digits);
          result_negative = adjustment_negative;
        }
      }
    }
    parts.coefficient = buffer + workspace;
    parts.coefficient_length = coefficient_length;
    parts.exponent = exponent_digits;
    parts.exponent_length = exponent_length;
    parts.exponent_negative = result_negative &&
                              !(exponent_length == 1u &&
                                exponent_digits[0] == (uint8_t)'0');
  }

  man0_writer sizing = {NULL, NULL, SIZE_MAX, 0u, false};
  if (!writer_append(&sizing, &parts.radix, 1u) ||
      !writer_frame(&sizing, "digits", parts.digits, parts.digits_length) ||
      !writer_frame(&sizing, "coefficient", parts.coefficient,
                    parts.coefficient_length))
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_CANONICAL_LIMIT,
                       numeric->span.start_byte);
  if (parts.exponent_negative) {
    const size_t exponent_payload = parts.exponent_length + 1u;
    uint8_t sign = (uint8_t)'-';
    if (!writer_frame_begin(&sizing, "exponent", exponent_payload) ||
        !writer_append(&sizing, &sign, 1u) ||
        !writer_append(&sizing, parts.exponent, parts.exponent_length))
      return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                         W_SEED_MANIFEST_ERROR_CANONICAL_LIMIT,
                         numeric->span.start_byte);
  } else if (!writer_frame(&sizing, "exponent", parts.exponent,
                           parts.exponent_length)) {
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_CANONICAL_LIMIT,
                       numeric->span.start_byte);
  }
  const size_t suffix_length = numeric->suffix_span.end_byte -
                               numeric->suffix_span.start_byte;
  if (!writer_frame(&sizing, "suffix", source + numeric->suffix_span.start_byte,
                    suffix_length))
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_CANONICAL_LIMIT,
                       numeric->span.start_byte);
  if (numeric->kind != MAN0_NUMERIC_NUMBER) {
    if (numeric->kind == MAN0_NUMERIC_SIZE) {
      const size_t unit_length =
          numeric->unit_span.end_byte - numeric->unit_span.start_byte;
      if (!writer_frame(&sizing, "unit", source + numeric->unit_span.start_byte,
                        unit_length))
        return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                           W_SEED_MANIFEST_ERROR_CANONICAL_LIMIT,
                           numeric->span.start_byte);
    } else {
      if (!writer_quantity_frame(&sizing, source, numeric->unit_span))
        return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                           W_SEED_MANIFEST_ERROR_CANONICAL_LIMIT,
                           numeric->span.start_byte);
    }
  }

  if (sizing.length > parser->limits->max_canonical_bytes)
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_CANONICAL_LIMIT,
                       numeric->span.start_byte);
  man0_writer writer = {destination, destination, destination_capacity, 0u,
                        compare_only};
  if (!writer_append(&writer, &parts.radix, 1u) ||
      !writer_frame(&writer, "digits", parts.digits, parts.digits_length) ||
      !writer_frame(&writer, "coefficient", parts.coefficient,
                    parts.coefficient_length))
    return compare_only
               ? false
               : parser_fail(parser, W_SEED_MANIFEST_CAPACITY,
                             W_SEED_MANIFEST_ERROR_NONE,
                             numeric->span.start_byte);
  if (parts.exponent_negative) {
    const size_t exponent_payload = parts.exponent_length + 1u;
    const uint8_t sign = (uint8_t)'-';
    if (!writer_frame_begin(&writer, "exponent", exponent_payload) ||
        !writer_append(&writer, &sign, 1u) ||
        !writer_append(&writer, parts.exponent, parts.exponent_length))
      return compare_only
                 ? false
                 : parser_fail(parser, W_SEED_MANIFEST_CAPACITY,
                               W_SEED_MANIFEST_ERROR_NONE,
                               numeric->span.start_byte);
  } else if (!writer_frame(&writer, "exponent", parts.exponent,
                           parts.exponent_length)) {
    return compare_only
               ? false
               : parser_fail(parser, W_SEED_MANIFEST_CAPACITY,
                             W_SEED_MANIFEST_ERROR_NONE,
                             numeric->span.start_byte);
  }
  if (!writer_frame(&writer, "suffix", source + numeric->suffix_span.start_byte,
                    suffix_length))
    return compare_only
               ? false
               : parser_fail(parser, W_SEED_MANIFEST_CAPACITY,
                             W_SEED_MANIFEST_ERROR_NONE,
                             numeric->span.start_byte);
  if (numeric->kind != MAN0_NUMERIC_NUMBER) {
    const bool ok = numeric->kind == MAN0_NUMERIC_SIZE
                        ? writer_frame(&writer, "unit",
                                       source + numeric->unit_span.start_byte,
                                       numeric->unit_span.end_byte -
                                           numeric->unit_span.start_byte)
                        : writer_quantity_frame(&writer, source,
                                                numeric->unit_span);
    if (!ok)
      return compare_only
                 ? false
                 : parser_fail(parser, W_SEED_MANIFEST_CAPACITY,
                               W_SEED_MANIFEST_ERROR_NONE,
                               numeric->span.start_byte);
  }
  if (writer.length != sizing.length)
    return compare_only
               ? false
               : parser_fail(parser, W_SEED_MANIFEST_CAPACITY,
                             W_SEED_MANIFEST_ERROR_NONE,
                             numeric->span.start_byte);
  *canonical_length = sizing.length;
  return true;
}

static bool count_item(man0_parser *parser, uint32_t *global,
                       uint32_t *document, size_t offset) {
  if (!count_structural(parser, global, offset)) return false;
  if (!add_u32(*document, 1u, document))
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_NODE_LIMIT, offset);
  return true;
}

static bool verify_node_static(const man0_parser *parser, uint32_t index,
                               w_seed_manifest_node_kind kind,
                               uint32_t parent, uint32_t ordinal,
                               size_t start) {
  if (parser->verify_program == NULL || index >= parser->verify_program->node_count)
    return false;
  const w_seed_manifest_node *node = &parser->verify_program->nodes[index];
  return node->kind == kind && node->document_index == parser->document_index &&
         node->parent_node == parent && node->source_ordinal == ordinal &&
         node->source_span.start_byte == start;
}

static bool reserve_node(man0_parser *parser, w_seed_manifest_node_kind kind,
                         uint32_t parent, uint32_t ordinal, size_t start,
                         uint32_t *index) {
  const uint32_t value = parser->counts->nodes;
  if (!count_item(parser, &parser->counts->nodes,
                  &parser->document_counts.nodes, start))
    return false;
  if (index != NULL) *index = value;
  if (parser->mode == MAN0_MODE_EMIT) {
    if (parser->output == NULL || value >= parser->output->node_capacity ||
        parser->output->nodes == NULL)
      return parser_fail(parser, W_SEED_MANIFEST_CAPACITY,
                         W_SEED_MANIFEST_ERROR_NONE, start);
    parser->output->nodes[value] = (w_seed_manifest_node){
        kind, parser->document_index, parent, ordinal,
        {start, start},
        {W_SEED_MANIFEST_NO_BYTE, W_SEED_MANIFEST_NO_BYTE},
        W_SEED_MANIFEST_NONE, 0u, {W_SEED_MANIFEST_NONE, 0u}, false};
  } else if (parser->verify_program != NULL &&
             !verify_node_static(parser, value, kind, parent, ordinal, start)) {
    return false;
  }
  return true;
}

static bool finish_node(man0_parser *parser, uint32_t index, w_seed_span span) {
  if (parser->mode == MAN0_MODE_EMIT) {
    parser->output->nodes[index].source_span = span;
    return true;
  }
  if (parser->verify_program == NULL) return true;
  return parser->verify_program != NULL && index < parser->verify_program->node_count &&
         parser->verify_program->nodes[index].source_span.start_byte == span.start_byte &&
         parser->verify_program->nodes[index].source_span.end_byte == span.end_byte;
}

static bool finish_node_children(man0_parser *parser, uint32_t index,
                                 uint32_t first, uint32_t count) {
  if (parser->mode == MAN0_MODE_EMIT) {
    parser->output->nodes[index].first_child = first;
    parser->output->nodes[index].child_count = count;
    return true;
  }
  if (parser->verify_program == NULL) return true;
  return parser->verify_program != NULL && index < parser->verify_program->node_count &&
         parser->verify_program->nodes[index].child_count == count;
}

static bool finish_node_name(man0_parser *parser, uint32_t index,
                             w_seed_span name) {
  if (parser->mode == MAN0_MODE_EMIT) {
    parser->output->nodes[index].name_span = name;
    return true;
  }
  if (parser->verify_program == NULL) return true;
  if (parser->verify_program == NULL || index >= parser->verify_program->node_count)
    return false;
  const w_seed_span expected = parser->verify_program->nodes[index].name_span;
  return expected.start_byte == name.start_byte && expected.end_byte == name.end_byte;
}

static bool finish_node_bool(man0_parser *parser, uint32_t index, bool value) {
  if (parser->mode == MAN0_MODE_EMIT) {
    parser->output->nodes[index].boolean_value = value;
    return true;
  }
  if (parser->verify_program == NULL) return true;
  return parser->verify_program != NULL && index < parser->verify_program->node_count &&
         parser->verify_program->nodes[index].boolean_value == value;
}

static bool finish_node_canonical(man0_parser *parser, uint32_t index,
                                  uint32_t offset, uint32_t length) {
  if (parser->mode == MAN0_MODE_EMIT) {
    parser->output->nodes[index].canonical = (w_seed_manifest_canonical_bytes){
        offset, length};
    return true;
  }
  if (parser->verify_program == NULL) return true;
  if (parser->verify_program == NULL || index >= parser->verify_program->node_count)
    return false;
  const w_seed_manifest_canonical_bytes expected =
      parser->verify_program->nodes[index].canonical;
  return expected.offset == offset && expected.length == length;
}

static int compare_source_spans(const man0_parser *parser, w_seed_span left,
                                w_seed_span right) {
  const size_t left_length = left.end_byte - left.start_byte;
  const size_t right_length = right.end_byte - right.start_byte;
  const size_t common = left_length < right_length ? left_length : right_length;
  const int result = common == 0u
                         ? 0
                         : memcmp(parser->bytes + left.start_byte,
                                  parser->bytes + right.start_byte, common);
  if (result != 0) return result;
  return left_length < right_length ? -1 : left_length > right_length ? 1 : 0;
}

static bool verify_field_match(const man0_parser *parser, uint32_t owner,
                               w_seed_span name, w_seed_span source,
                               uint32_t value_node) {
  if (parser->verify_program == NULL || owner >= parser->verify_program->node_count)
    return false;
  const w_seed_manifest_node *record = &parser->verify_program->nodes[owner];
  if (record->first_child == W_SEED_MANIFEST_NONE ||
      record->first_child > parser->verify_program->field_count ||
      record->child_count > parser->verify_program->field_count - record->first_child)
    return false;
  for (uint32_t offset = 0u; offset < record->child_count; offset += 1u) {
    const w_seed_manifest_field *field =
        &parser->verify_program->fields[record->first_child + offset];
    if (compare_source_spans(parser, field->name_span, name) == 0)
      return field->owner_record == owner && field->value_node == value_node &&
             field->name_span.start_byte == name.start_byte &&
             field->name_span.end_byte == name.end_byte &&
             field->source_span.start_byte == source.start_byte &&
             field->source_span.end_byte == source.end_byte;
  }
  return false;
}

static bool reserve_field(man0_parser *parser, uint32_t owner, w_seed_span name,
                          uint32_t *index) {
  const uint32_t value = parser->counts->fields;
  if (!count_item(parser, &parser->counts->fields,
                  &parser->document_counts.fields, name.start_byte))
    return false;
  if (index != NULL) *index = value;
  if (parser->mode == MAN0_MODE_EMIT) {
    parser->output->fields[value] =
        (w_seed_manifest_field){owner, 0u, W_SEED_MANIFEST_NONE, name, name};
  }
  return true;
}

static bool reserve_edge(man0_parser *parser, w_seed_manifest_edge_kind kind,
                         uint32_t owner, uint32_t ordinal, w_seed_span label,
                         bool has_label, size_t start, uint32_t *index) {
  const uint32_t value = parser->counts->edges;
  if (!count_item(parser, &parser->counts->edges,
                  &parser->document_counts.edges, start))
    return false;
  if (index != NULL) *index = value;
  if (parser->mode == MAN0_MODE_EMIT) {
    parser->output->edges[value] = (w_seed_manifest_edge){
        kind, owner, ordinal, W_SEED_MANIFEST_NONE, has_label, label,
        {start, start}};
  } else if (parser->verify_program != NULL) {
    bool found = false;
    for (uint32_t candidate = 0u;
         candidate < parser->verify_program->edge_count; candidate += 1u) {
      const w_seed_manifest_edge *expected =
          &parser->verify_program->edges[candidate];
      if (expected->kind == kind && expected->owner_node == owner &&
          expected->ordinal == ordinal && expected->has_label == has_label &&
          expected->label_span.start_byte == label.start_byte &&
          expected->label_span.end_byte == label.end_byte &&
          expected->source_span.start_byte == start) {
        if (found) return false;
        found = true;
        if (index != NULL) *index = candidate;
      }
    }
    if (!found) return false;
  }
  return true;
}

static bool finish_field(man0_parser *parser, uint32_t index, uint32_t owner,
                         w_seed_span name, w_seed_span source,
                         uint32_t value_node) {
  if (parser->mode == MAN0_MODE_EMIT) {
    parser->output->fields[index].owner_record = owner;
    parser->output->fields[index].value_node = value_node;
    parser->output->fields[index].name_span = name;
    parser->output->fields[index].source_span = source;
    return true;
  }
  if (parser->verify_program == NULL) return true;
  return verify_field_match(parser, owner, name, source, value_node);
}

static bool finish_edge(man0_parser *parser, uint32_t index, uint32_t value_node,
                        w_seed_span source) {
  if (parser->mode == MAN0_MODE_EMIT) {
    parser->output->edges[index].value_node = value_node;
    parser->output->edges[index].source_span = source;
    return true;
  }
  if (parser->verify_program == NULL) return true;
  if (parser->verify_program == NULL || index >= parser->verify_program->edge_count)
    return false;
  const w_seed_manifest_edge *edge = &parser->verify_program->edges[index];
  return edge->value_node == value_node && edge->source_span.start_byte == source.start_byte &&
         edge->source_span.end_byte == source.end_byte;
}

static bool parse_value(man0_parser *parser, uint32_t parent, uint32_t ordinal,
                        uint32_t *node_index, w_seed_span *value_span);

static bool parse_record_value(man0_parser *parser, uint32_t parent,
                               uint32_t ordinal, uint32_t *node_index,
                               w_seed_span *value_span) {
  const size_t start = parser->cursor;
  uint32_t node = W_SEED_MANIFEST_NONE;
  if (!reserve_node(parser, W_SEED_MANIFEST_NODE_RECORD, parent, ordinal, start,
                    &node) ||
      !consume_byte(parser, (uint8_t)'{') || !enter_nesting(parser, start) ||
      !skip_trivia(parser))
    return false;
  const uint32_t first_field = parser->counts->fields;
  const uint64_t scope = ++parser->scope_seed;
  uint32_t physical_field = 0u;
  bool first = true;
  while (!at_byte(parser, (uint8_t)'}')) {
    if (!first) {
      if (consume_byte(parser, (uint8_t)',') && !skip_trivia(parser)) return false;
      if (at_byte(parser, (uint8_t)'}')) break;
    }
    w_seed_span name;
    if (!parse_identifier(parser, &name))
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         W_SEED_MANIFEST_ERROR_FIELD_REQUIRED,
                         parser->cursor);
    if (!insert_name(parser, scope, name,
                     W_SEED_MANIFEST_ERROR_FIELD_DUPLICATE))
      return false;
    uint32_t field = W_SEED_MANIFEST_NONE;
    if (!reserve_field(parser, node, name, &field) || !skip_trivia(parser))
      return false;
    if (!consume_byte(parser, (uint8_t)':'))
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         W_SEED_MANIFEST_ERROR_COLON_REQUIRED,
                         parser->cursor);
    if (!skip_trivia(parser)) return false;
    const uint32_t physical_ordinal = physical_field;
    uint32_t child = W_SEED_MANIFEST_NONE;
    w_seed_span child_span;
    if (!parse_value(parser, node, physical_ordinal, &child, &child_span) ||
        !finish_field(parser, field, node, name,
                      (w_seed_span){name.start_byte, child_span.end_byte}, child) ||
        !skip_trivia(parser))
      return false;
    physical_field += 1u;
    first = false;
  }
  if (!consume_byte(parser, (uint8_t)'}'))
    return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                       W_SEED_MANIFEST_ERROR_VALUE_REQUIRED, start);
  leave_nesting(parser);
  const w_seed_span span = {start, parser->cursor};
  const uint32_t field_count = physical_field;
  if (!finish_node_children(parser, node, first_field, field_count) ||
      !finish_node(parser, node, span))
    return false;
  if (node_index != NULL) *node_index = node;
  if (value_span != NULL) *value_span = span;
  return true;
}

static bool parse_list_value(man0_parser *parser, uint32_t parent,
                             uint32_t ordinal, uint32_t *node_index,
                             w_seed_span *value_span) {
  const size_t start = parser->cursor;
  uint32_t node = W_SEED_MANIFEST_NONE;
  if (!reserve_node(parser, W_SEED_MANIFEST_NODE_LIST, parent, ordinal, start,
                    &node) ||
      !consume_byte(parser, (uint8_t)'[') || !enter_nesting(parser, start) ||
      !skip_trivia(parser))
    return false;
  const uint32_t first_edge = parser->counts->edges;
  bool first = true;
  uint32_t physical = 0u;
  while (!at_byte(parser, (uint8_t)']')) {
    if (!first) {
      if (consume_byte(parser, (uint8_t)',') && !skip_trivia(parser)) return false;
      if (at_byte(parser, (uint8_t)']')) break;
    }
    const size_t item_start = parser->cursor;
    uint32_t edge = W_SEED_MANIFEST_NONE;
    if (!reserve_edge(parser, W_SEED_MANIFEST_EDGE_LIST_ITEM, node, physical,
                      (w_seed_span){W_SEED_MANIFEST_NO_BYTE,
                                    W_SEED_MANIFEST_NO_BYTE}, false,
                      item_start, &edge))
      return false;
    uint32_t child = W_SEED_MANIFEST_NONE;
    w_seed_span child_span;
    if (!parse_value(parser, node, physical, &child, &child_span) ||
        !finish_edge(parser, edge, child, child_span) || !skip_trivia(parser))
      return false;
    physical += 1u;
    first = false;
  }
  if (!consume_byte(parser, (uint8_t)']'))
    return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                       W_SEED_MANIFEST_ERROR_VALUE_REQUIRED, start);
  leave_nesting(parser);
  const w_seed_span span = {start, parser->cursor};
  if (!finish_node_children(parser, node, first_edge, physical) ||
      !finish_node(parser, node, span))
    return false;
  if (node_index != NULL) *node_index = node;
  if (value_span != NULL) *value_span = span;
  return true;
}

static bool parse_member_or_constructor(man0_parser *parser, uint32_t parent,
                                        uint32_t ordinal, uint32_t *node_index,
                                        w_seed_span *value_span) {
  const size_t start = parser->cursor;
  if (!consume_byte(parser, (uint8_t)'.')) return false;
  w_seed_span name;
  if (!parse_identifier(parser, &name))
    return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                       W_SEED_MANIFEST_ERROR_VALUE_REQUIRED, start);
  if (!at_byte(parser, (uint8_t)'(')) {
    uint32_t node = W_SEED_MANIFEST_NONE;
    if (!reserve_node(parser, W_SEED_MANIFEST_NODE_MEMBER, parent, ordinal, start,
                      &node) ||
        !finish_node_name(parser, node, name) ||
        !finish_node(parser, node, (w_seed_span){start, parser->cursor}))
      return false;
    if (node_index != NULL) *node_index = node;
    if (value_span != NULL) *value_span = (w_seed_span){start, parser->cursor};
    return true;
  }
  uint32_t node = W_SEED_MANIFEST_NONE;
  if (!reserve_node(parser, W_SEED_MANIFEST_NODE_CONSTRUCTOR, parent, ordinal,
                    start, &node) ||
      !finish_node_name(parser, node, name) ||
      !consume_byte(parser, (uint8_t)'(') || !enter_nesting(parser, start) ||
      !skip_trivia(parser))
    return false;
  const uint32_t first_edge = parser->counts->edges;
  const uint64_t scope = ++parser->scope_seed;
  if (at_byte(parser, (uint8_t)')'))
    return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                       W_SEED_MANIFEST_ERROR_VALUE_REQUIRED, parser->cursor);
  bool first = true;
  uint32_t physical = 0u;
  while (!at_byte(parser, (uint8_t)')')) {
    if (!first) {
      if (!consume_byte(parser, (uint8_t)','))
        return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                           W_SEED_MANIFEST_ERROR_COMMA_REQUIRED,
                           parser->cursor);
      if (!skip_trivia(parser)) return false;
      if (at_byte(parser, (uint8_t)')')) break;
    }
    const size_t argument_start = parser->cursor;
    const size_t saved_cursor = parser->cursor;
    w_seed_span label = {W_SEED_MANIFEST_NO_BYTE, W_SEED_MANIFEST_NO_BYTE};
    bool has_label = parse_identifier(parser, &label);
    if (!has_label) {
      if (parser->result->error != W_SEED_MANIFEST_ERROR_NONE) return false;
      parser->cursor = saved_cursor;
    } else {
      if (!skip_trivia(parser)) return false;
      if (consume_byte(parser, (uint8_t)':')) {
        if (!insert_name(parser, scope, label,
                         W_SEED_MANIFEST_ERROR_CONSTRUCTOR_LABEL_DUPLICATE) ||
            !skip_trivia(parser))
          return false;
        has_label = true;
      } else {
        parser->cursor = saved_cursor;
        has_label = false;
        label = (w_seed_span){W_SEED_MANIFEST_NO_BYTE,
                              W_SEED_MANIFEST_NO_BYTE};
      }
    }
    uint32_t edge = W_SEED_MANIFEST_NONE;
    if (!reserve_edge(parser, W_SEED_MANIFEST_EDGE_CONSTRUCTOR_ARGUMENT, node,
                      physical, label, has_label, argument_start, &edge))
      return false;
    uint32_t child = W_SEED_MANIFEST_NONE;
    w_seed_span child_span;
    if (!parse_value(parser, node, physical, &child, &child_span) ||
        !finish_edge(parser, edge, child,
                     (w_seed_span){argument_start, child_span.end_byte}) ||
        !skip_trivia(parser))
      return false;
    physical += 1u;
    first = false;
  }
  if (!consume_byte(parser, (uint8_t)')'))
    return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                       W_SEED_MANIFEST_ERROR_VALUE_REQUIRED, start);
  leave_nesting(parser);
  const w_seed_span span = {start, parser->cursor};
  if (!finish_node_children(parser, node, first_edge, physical) ||
      !finish_node(parser, node, span))
    return false;
  if (node_index != NULL) *node_index = node;
  if (value_span != NULL) *value_span = span;
  return true;
}

static bool copy_or_compare_scalar(man0_parser *parser, const uint8_t *bytes,
                                   size_t length, uint32_t *offset) {
  const uint32_t current = parser->counts->canonical_bytes;
  if (length > UINT32_MAX || current > UINT32_MAX - (uint32_t)length)
    return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_ERROR_CANONICAL_LIMIT,
                       parser->cursor);
  if (parser->mode == MAN0_MODE_EMIT) {
    if (parser->output == NULL || current > parser->output->canonical_byte_capacity ||
        length > parser->output->canonical_byte_capacity - current ||
        (length != 0u && parser->output->canonical_bytes == NULL))
      return parser_fail(parser, W_SEED_MANIFEST_CAPACITY,
                         W_SEED_MANIFEST_ERROR_NONE, parser->cursor);
    if (length != 0u) (void)memcpy(parser->output->canonical_bytes + current, bytes,
                                    length);
  } else if (parser->verify_program != NULL) {
    if (current > parser->verify_program->canonical_byte_count ||
        length > parser->verify_program->canonical_byte_count - current ||
        (length != 0u && parser->verify_program->canonical_bytes == NULL) ||
        (length != 0u &&
         memcmp(parser->verify_program->canonical_bytes + current, bytes,
                length) != 0))
      return false;
  }
  if (!count_canonical(parser, length, parser->cursor)) return false;
  if (offset != NULL) *offset = current;
  return true;
}

static bool parse_value(man0_parser *parser, uint32_t parent, uint32_t ordinal,
                        uint32_t *node_index, w_seed_span *value_span) {
  if (parser->cursor >= parser->length)
    return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                       W_SEED_MANIFEST_ERROR_VALUE_REQUIRED,
                       parser->cursor);
  if (at_byte(parser, (uint8_t)'{'))
    return parse_record_value(parser, parent, ordinal, node_index, value_span);
  if (at_byte(parser, (uint8_t)'['))
    return parse_list_value(parser, parent, ordinal, node_index, value_span);
  if (at_byte(parser, (uint8_t)'.'))
    return parse_member_or_constructor(parser, parent, ordinal, node_index,
                                       value_span);
  const size_t start = parser->cursor;
  if (at_byte(parser, (uint8_t)'"')) {
    man0_string string;
    if (!parse_string(parser, &string)) return false;
    uint32_t node = W_SEED_MANIFEST_NONE;
    if (!reserve_node(parser, W_SEED_MANIFEST_NODE_STRING, parent, ordinal, start,
                      &node))
      return false;
    uint32_t canonical = W_SEED_MANIFEST_NONE;
    if (!copy_or_compare_scalar(parser, parser->scratch.bytes,
                                string.decoded_length, &canonical) ||
        !finish_node_canonical(parser, node, canonical,
                               (uint32_t)string.decoded_length) ||
        !finish_node(parser, node, string.span))
      return false;
    if (node_index != NULL) *node_index = node;
    if (value_span != NULL) *value_span = string.span;
    return true;
  }
  if (parser->bytes[parser->cursor] >= (uint8_t)'0' &&
      parser->bytes[parser->cursor] <= (uint8_t)'9') {
    man0_numeric numeric;
    if (!parse_numeric(parser, &numeric)) return false;
    if (numeric.digit_count > parser->limits->max_decoded_scalar_bytes)
      return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                         W_SEED_MANIFEST_ERROR_DECODED_SCALAR_LIMIT, start);
    uint32_t node = W_SEED_MANIFEST_NONE;
    if (!reserve_node(parser,
                      numeric.kind == MAN0_NUMERIC_SIZE
                          ? W_SEED_MANIFEST_NODE_SIZE
                          : numeric.kind == MAN0_NUMERIC_QUANTITY
                                ? W_SEED_MANIFEST_NODE_QUANTITY
                                : W_SEED_MANIFEST_NODE_NUMBER,
                      parent, ordinal, start, &node))
      return false;
    const uint32_t current = parser->counts->canonical_bytes;
    uint8_t *destination = NULL;
    const uint8_t *comparison = NULL;
    size_t destination_capacity = 0u;
    bool compare_only = false;
    if (parser->mode == MAN0_MODE_EMIT) {
      if (parser->output == NULL || parser->output->canonical_bytes == NULL ||
          current >= parser->output->canonical_byte_capacity)
        return parser_fail(parser, W_SEED_MANIFEST_CAPACITY,
                           W_SEED_MANIFEST_ERROR_NONE, start);
      destination = parser->output->canonical_bytes + current;
      destination_capacity = parser->output->canonical_byte_capacity - current;
    } else if (parser->verify_program != NULL) {
      if (current >= parser->verify_program->canonical_byte_count)
        return false;
      comparison = parser->verify_program->canonical_bytes + current;
      destination_capacity = parser->verify_program->canonical_byte_count - current;
      destination = (uint8_t *)comparison;
      compare_only = true;
    }
    size_t canonical_length = 0u;
    if (!normalize_numeric(parser, &numeric, destination, destination_capacity,
                            compare_only, &canonical_length) ||
        canonical_length > UINT32_MAX ||
        !count_canonical(parser, canonical_length, start) ||
        !finish_node_canonical(parser, node, current,
                               (uint32_t)canonical_length) ||
        !finish_node(parser, node, numeric.span))
      return false;
    if (node_index != NULL) *node_index = node;
    if (value_span != NULL) *value_span = numeric.span;
    return true;
  }
  w_seed_span identifier;
  if (parse_identifier(parser, &identifier)) {
    const bool value = span_text(parser, identifier, "true");
    if (!value && !span_text(parser, identifier, "false"))
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         W_SEED_MANIFEST_ERROR_EXECUTABLE_FORM, start);
    uint32_t node = W_SEED_MANIFEST_NONE;
    if (!reserve_node(parser, W_SEED_MANIFEST_NODE_BOOL, parent, ordinal, start,
                      &node) ||
        !finish_node_bool(parser, node, value) ||
        !finish_node(parser, node, identifier))
      return false;
    if (node_index != NULL) *node_index = node;
    if (value_span != NULL) *value_span = identifier;
    return true;
  }
  return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                     W_SEED_MANIFEST_ERROR_VALUE_REQUIRED, start);
}

static bool parse_document(man0_parser *parser) {
  if (!skip_trivia(parser)) return false;
  if (parser->cursor == parser->length)
    return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                       W_SEED_MANIFEST_ERROR_ROOT_REQUIRED, 0u);
  uint32_t roots = 0u;
  while (parser->cursor < parser->length) {
    const size_t start = parser->cursor;
    w_seed_span keyword;
    if (!parse_identifier(parser, &keyword))
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         roots == 0u ? W_SEED_MANIFEST_ERROR_ROOT_REQUIRED
                                     : W_SEED_MANIFEST_ERROR_TRAILING_SOURCE,
                         start);
    bool *seen = NULL;
    if (span_text(parser, keyword, "package")) seen = &parser->package_seen;
    else if (span_text(parser, keyword, "workspace")) seen = &parser->workspace_seen;
    else
      return parser_fail(parser, W_SEED_MANIFEST_SYNTAX,
                         roots == 0u ? W_SEED_MANIFEST_ERROR_ROOT_INVALID
                                     : W_SEED_MANIFEST_ERROR_TRAILING_SOURCE,
                         start);
    if (*seen)
      return parser_fail(parser, W_SEED_MANIFEST_DUPLICATE,
                         W_SEED_MANIFEST_ERROR_ROOT_DUPLICATE, start);
    if (roots >= parser->limits->max_roots_per_document)
      return parser_fail(parser, W_SEED_MANIFEST_LIMIT,
                         W_SEED_MANIFEST_ERROR_ROOT_LIMIT, start);
    *seen = true;
    const uint32_t physical_root = roots;
    roots += 1u;
    const uint32_t root_index = parser->counts->roots;
    if (!count_item(parser, &parser->counts->roots,
                    &parser->document_counts.roots, start) ||
        !skip_trivia(parser))
      return false;
    parser->physical_root_ordinal = physical_root;
    uint32_t record_node = W_SEED_MANIFEST_NONE;
    w_seed_span record_span;
    if (!parse_record_value(parser, W_SEED_MANIFEST_NONE, physical_root,
                            &record_node, &record_span) ||
        !skip_trivia(parser))
      return false;
    if (parser->mode == MAN0_MODE_EMIT) {
      if (parser->output == NULL || root_index >= parser->output->root_capacity ||
          parser->output->roots == NULL)
        return parser_fail(parser, W_SEED_MANIFEST_CAPACITY,
                           W_SEED_MANIFEST_ERROR_NONE, start);
      parser->output->roots[root_index] = (w_seed_manifest_root){
          (uint32_t)parser->document_index, physical_root,
          span_text(parser, keyword, "package")
              ? W_SEED_MANIFEST_ROOT_PACKAGE
              : W_SEED_MANIFEST_ROOT_WORKSPACE,
          record_node, keyword, {start, record_span.end_byte}};
    } else if (parser->verify_program != NULL) {
      const w_seed_manifest_root_kind kind =
          span_text(parser, keyword, "package")
              ? W_SEED_MANIFEST_ROOT_PACKAGE
              : W_SEED_MANIFEST_ROOT_WORKSPACE;
      const w_seed_manifest_document *document = parser->verify_document;
      if (document == NULL) return false;
      bool matched = false;
      for (uint32_t offset = 0u; offset < document->root_count; offset += 1u) {
        const w_seed_manifest_root *expected =
            &parser->verify_program->roots[document->first_root + offset];
        if (expected->kind == kind) {
          matched = expected->record_node == record_node &&
                    expected->document_index == parser->document_index &&
                    expected->keyword_span.start_byte == keyword.start_byte &&
                    expected->keyword_span.end_byte == keyword.end_byte &&
                    expected->source_span.start_byte == start &&
                    expected->source_span.end_byte == record_span.end_byte &&
                    expected->ordinal == offset;
          break;
        }
      }
      if (!matched) return false;
    }
  }
  if (parser->mode == MAN0_MODE_EMIT) {
    const uint32_t first_root = parser->counts->roots - roots;
    for (uint32_t index = 1u; index < roots; index += 1u) {
      w_seed_manifest_root item = parser->output->roots[first_root + index];
      uint32_t position = index;
      while (position != 0u &&
             parser->output->roots[first_root + position - 1u].kind > item.kind) {
        parser->output->roots[first_root + position] =
            parser->output->roots[first_root + position - 1u];
        position -= 1u;
      }
      parser->output->roots[first_root + position] = item;
    }
    for (uint32_t index = 0u; index < roots; index += 1u)
      parser->output->roots[first_root + index].ordinal = index;
  }
  return true;
}

static bool measure_ranges_valid(const w_seed_manifest_input *input,
                                 w_seed_manifest_counts *counts) {
  man0_range ranges[W_SEED_MANIFEST_MAX_DOCUMENTS + 5u];
  size_t length = 0u;
  if (!range_from(input, 1u, sizeof(*input), &ranges[length++]) ||
      !range_from(input->documents, input->document_count,
                  sizeof(*input->documents), &ranges[length++]) ||
      !range_from(counts, 1u, sizeof(*counts), &ranges[length++]) ||
      !range_from(input->scratch.name_slots, input->scratch.name_slot_capacity,
                  sizeof(*input->scratch.name_slots), &ranges[length++]) ||
      !range_from(input->scratch.bytes, input->scratch.byte_capacity, 1u,
                  &ranges[length++]))
    return false;
  for (size_t index = 0u; index < input->document_count; index += 1u)
    if (!range_from(input->documents[index].bytes.data,
                    input->documents[index].bytes.length, 1u,
                    &ranges[length++]))
      return false;
  for (size_t left = 0u; left < length; left += 1u)
    for (size_t right = left + 1u; right < length; right += 1u)
      if (ranges_overlap(ranges[left], ranges[right])) return false;
  return true;
}

static w_seed_manifest_result manifest_measure_impl(
    const w_seed_manifest_input *input, w_seed_manifest_counts *counts,
    bool allow_owner_binding) {
  w_seed_manifest_result result = result_baseline();
  if (input == NULL || counts == NULL || !limits_valid(input->limits)) return result;
  result.limits = input->limits;
  if (input->document_count == 0u ||
      input->document_count > input->limits.max_documents ||
      input->document_count > W_SEED_MANIFEST_MAX_DOCUMENTS ||
      input->documents == NULL)
    return fail_result(result, W_SEED_MANIFEST_LIMIT,
                       W_SEED_MANIFEST_PHASE_VALIDATE,
                       W_SEED_MANIFEST_ERROR_DOCUMENT_LIMIT,
                       W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NO_BYTE);
  size_t scratch_bytes = 0u;
  if (!add_size(input->limits.max_decoded_scalar_bytes,
                W_SEED_MANIFEST_SCALAR_SCRATCH_OVERHEAD, &scratch_bytes) ||
      input->scratch.name_slot_capacity < input->limits.max_structural_nodes ||
      input->scratch.byte_capacity < scratch_bytes)
    return fail_result(result, W_SEED_MANIFEST_CAPACITY,
                       W_SEED_MANIFEST_PHASE_VALIDATE,
                       W_SEED_MANIFEST_ERROR_NONE, W_SEED_MANIFEST_NONE,
                       W_SEED_MANIFEST_NO_BYTE);
  if (!measure_ranges_valid(input, counts))
    return fail_result(result, W_SEED_MANIFEST_ALIAS,
                       W_SEED_MANIFEST_PHASE_VALIDATE,
                       W_SEED_MANIFEST_ERROR_NONE, W_SEED_MANIFEST_NONE,
                       W_SEED_MANIFEST_NO_BYTE);
  size_t aggregate = 0u;
  for (size_t index = 0u; index < input->document_count; index += 1u) {
    if (!(allow_owner_binding ? binding_owner(&input->documents[index])
                             : binding_none(&input->documents[index])))
      return result;
    const size_t size = input->documents[index].bytes.length;
    if (size == 0u)
      return fail_result(result, W_SEED_MANIFEST_SYNTAX,
                         W_SEED_MANIFEST_PHASE_MEASURE,
                         W_SEED_MANIFEST_ERROR_SOURCE_EMPTY, (uint32_t)index,
                         0u);
    if (size > input->limits.max_document_bytes)
      return fail_result(result, W_SEED_MANIFEST_LIMIT,
                         W_SEED_MANIFEST_PHASE_MEASURE,
                         W_SEED_MANIFEST_ERROR_SOURCE_TOO_LARGE,
                         (uint32_t)index, input->limits.max_document_bytes);
    if (!add_size(aggregate, size, &aggregate) ||
        aggregate > input->limits.max_aggregate_bytes)
      return fail_result(result, W_SEED_MANIFEST_LIMIT,
                         W_SEED_MANIFEST_PHASE_MEASURE,
                         W_SEED_MANIFEST_ERROR_AGGREGATE_TOO_LARGE,
                         (uint32_t)index, 0u);
  }
  (void)memset(input->scratch.name_slots, 0,
               input->limits.max_structural_nodes *
                   sizeof(*input->scratch.name_slots));
  w_seed_manifest_counts measured;
  (void)memset(&measured, 0, sizeof(measured));
  uint64_t batch_work = 0u;
  for (size_t index = 0u; index < input->document_count; index += 1u) {
    const w_seed_manifest_source_input *source = &input->documents[index];
    const uint64_t source_work = (uint64_t)source->bytes.length;
    if (source_work > input->limits.max_work_units - batch_work) {
      result = fail_result(result, W_SEED_MANIFEST_LIMIT,
                           W_SEED_MANIFEST_PHASE_MEASURE,
                           W_SEED_MANIFEST_ERROR_WORK_LIMIT,
                           (uint32_t)index, 0u);
      return result;
    }
    batch_work += source_work;
    w_seed_source validated;
    w_seed_source_error source_error;
    if (!w_seed_source_init(source->bytes, &validated, &source_error))
      return fail_result(result, W_SEED_MANIFEST_UTF8,
                         W_SEED_MANIFEST_PHASE_MEASURE,
                         W_SEED_MANIFEST_ERROR_INVALID_UTF8, (uint32_t)index,
                         source_error.byte_offset);
    if (w_seed_source_has_bom(&validated))
      return fail_result(result, W_SEED_MANIFEST_BOM,
                         W_SEED_MANIFEST_PHASE_MEASURE,
                         W_SEED_MANIFEST_ERROR_UTF8_BOM, (uint32_t)index, 0u);
    for (size_t byte = 0u; byte < source->bytes.length; byte += 1u) {
      if (batch_work >= input->limits.max_work_units) {
        result = fail_result(result, W_SEED_MANIFEST_LIMIT,
                             W_SEED_MANIFEST_PHASE_MEASURE,
                             W_SEED_MANIFEST_ERROR_WORK_LIMIT,
                             (uint32_t)index, byte);
        return result;
      }
      batch_work += 1u;
      if (source->bytes.data[byte] == 0u)
        return fail_result(result, W_SEED_MANIFEST_SYNTAX,
                           W_SEED_MANIFEST_PHASE_MEASURE,
                           W_SEED_MANIFEST_ERROR_INVALID_TOKEN,
                           (uint32_t)index, byte);
    }
    man0_parser parser;
    (void)memset(&parser, 0, sizeof(parser));
    parser.source = source;
    parser.bytes = source->bytes.data;
    parser.length = source->bytes.length;
    parser.document_index = (uint32_t)index;
    parser.scope_seed = ((uint64_t)index + 1u) << 32u;
    parser.limits = &input->limits;
    parser.scratch = input->scratch;
    parser.counts = &measured;
    parser.result = &result;
    parser.phase = W_SEED_MANIFEST_PHASE_MEASURE;
    parser.mode = MAN0_MODE_MEASURE;
    parser.work = batch_work;
    if (!parse_document(&parser)) return result;
    batch_work = parser.work;
    if (!add_u32(measured.documents, 1u, &measured.documents))
      return fail_result(result, W_SEED_MANIFEST_LIMIT,
                         W_SEED_MANIFEST_PHASE_MEASURE,
                         W_SEED_MANIFEST_ERROR_DOCUMENT_LIMIT,
                         (uint32_t)index, 0u);
  }
  *counts = measured;
  result.status = W_SEED_MANIFEST_OK;
  result.phase = W_SEED_MANIFEST_PHASE_MEASURE;
  result.required = measured;
  return result;
}

w_seed_manifest_result w_seed_manifest_measure(
    const w_seed_manifest_input *input, w_seed_manifest_counts *counts) {
  return manifest_measure_impl(input, counts, false);
}

static bool counts_equal(w_seed_manifest_counts left,
                         w_seed_manifest_counts right) {
  return left.documents == right.documents && left.roots == right.roots &&
         left.nodes == right.nodes && left.fields == right.fields &&
         left.edges == right.edges &&
         left.canonical_bytes == right.canonical_bytes &&
         left.structural_nodes == right.structural_nodes;
}

static bool range_count_valid(uint32_t first, uint32_t count,
                              size_t total) {
  return (size_t)first <= total && (size_t)count <= total - (size_t)first;
}

static bool span_absent(w_seed_span span) {
  return span.start_byte == W_SEED_MANIFEST_NO_BYTE &&
         span.end_byte == W_SEED_MANIFEST_NO_BYTE;
}

static bool span_in_source(w_seed_span span, size_t length) {
  return span.start_byte <= span.end_byte && span.end_byte <= length;
}

static bool manifest_output_capacity_valid(
    const w_seed_manifest_output *output, w_seed_manifest_counts counts) {
  if (output == NULL) return false;
#define MANIFEST_OUTPUT_COUNT(field, capacity_field)                         \
  if ((size_t)counts.field > output->capacity_field ||                       \
      (counts.field != 0u && output->field == NULL))                         \
    return false
  MANIFEST_OUTPUT_COUNT(documents, document_capacity);
  MANIFEST_OUTPUT_COUNT(roots, root_capacity);
  MANIFEST_OUTPUT_COUNT(nodes, node_capacity);
  MANIFEST_OUTPUT_COUNT(fields, field_capacity);
  MANIFEST_OUTPUT_COUNT(edges, edge_capacity);
#undef MANIFEST_OUTPUT_COUNT
  return (size_t)counts.canonical_bytes <= output->canonical_byte_capacity &&
         (counts.canonical_bytes == 0u || output->canonical_bytes != NULL);
}

static bool manifest_output_ranges(const w_seed_manifest_output *output,
                                   man0_range *ranges, size_t *count) {
  if (output == NULL || ranges == NULL || count == NULL) return false;
  *count = 0u;
#define MANIFEST_OUTPUT_RANGE(field, capacity_field)                         \
  if (*count >= W_SEED_MANIFEST_MAX_DOCUMENTS + 16u ||                        \
      !range_from(output->field, output->capacity_field, sizeof(*output->field),\
                  &ranges[(*count)++]))                                         \
    return false
  MANIFEST_OUTPUT_RANGE(documents, document_capacity);
  MANIFEST_OUTPUT_RANGE(roots, root_capacity);
  MANIFEST_OUTPUT_RANGE(nodes, node_capacity);
  MANIFEST_OUTPUT_RANGE(fields, field_capacity);
  MANIFEST_OUTPUT_RANGE(edges, edge_capacity);
#undef MANIFEST_OUTPUT_RANGE
  if (*count >= W_SEED_MANIFEST_MAX_DOCUMENTS + 16u ||
      !range_from(output->canonical_bytes, output->canonical_byte_capacity, 1u,
                  &ranges[(*count)++]))
    return false;
  return true;
}

static bool ranges_pairwise_disjoint(const man0_range *ranges, size_t count) {
  if (ranges == NULL) return false;
  for (size_t left = 0u; left < count; left += 1u)
    for (size_t right = left + 1u; right < count; right += 1u)
      if (ranges_overlap(ranges[left], ranges[right])) return false;
  return true;
}

static bool output_aliases_input(const w_seed_manifest_input *input,
                                 const w_seed_manifest_output *output) {
  if (input == NULL || output == NULL) return true;
  man0_range input_ranges[W_SEED_MANIFEST_MAX_DOCUMENTS + 8u];
  man0_range output_ranges[8u];
  man0_range output_descriptor;
  size_t input_count = 0u;
  size_t output_count = 0u;
  if (!range_from(input, 1u, sizeof(*input), &input_ranges[input_count++]) ||
      !range_from(input->documents, input->document_count,
                  sizeof(*input->documents), &input_ranges[input_count++]) ||
      !range_from(input->scratch.name_slots, input->scratch.name_slot_capacity,
                  sizeof(*input->scratch.name_slots),
                  &input_ranges[input_count++]) ||
      !range_from(input->scratch.bytes, input->scratch.byte_capacity, 1u,
                  &input_ranges[input_count++]) ||
      !range_from(output, 1u, sizeof(*output), &output_descriptor) ||
      !manifest_output_ranges(output, output_ranges, &output_count))
    return true;
  for (size_t index = 0u; index < input->document_count; index += 1u)
    if (!range_from(input->documents[index].bytes.data,
                    input->documents[index].bytes.length, 1u,
                    &input_ranges[input_count++]))
      return true;
  if (!ranges_pairwise_disjoint(output_ranges, output_count)) return true;
  for (size_t right = 0u; right < input_count; right += 1u)
    if (ranges_overlap(output_descriptor, input_ranges[right])) return true;
  for (size_t index = 0u; index < output_count; index += 1u)
    if (ranges_overlap(output_descriptor, output_ranges[index])) return true;
  for (size_t left = 0u; left < output_count; left += 1u)
    for (size_t right = 0u; right < input_count; right += 1u)
      if (ranges_overlap(output_ranges[left], input_ranges[right])) return true;
  return false;
}

static void digest_u32(w_seed_sha256_state *state, uint32_t value) {
  uint8_t bytes[4];
  write_u32_be(bytes, value);
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void digest_u64(w_seed_sha256_state *state, uint64_t value) {
  uint8_t bytes[8];
  write_u64_be(bytes, value);
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static bool digest_frame_size(const char *tag, uint64_t payload_length,
                              uint64_t *size) {
  const size_t tag_length = strlen(tag);
  if (size == NULL || tag_length > UINT32_MAX ||
      payload_length > UINT64_MAX - UINT64_C(12) - (uint64_t)tag_length)
    return false;
  *size = UINT64_C(12) + (uint64_t)tag_length + payload_length;
  return true;
}

static bool digest_add(uint64_t left, uint64_t right, uint64_t *sum) {
  if (sum == NULL || left > UINT64_MAX - right) return false;
  *sum = left + right;
  return true;
}

static bool digest_frame_prefix(w_seed_sha256_state *state, const char *tag,
                                uint64_t payload_length) {
  const size_t tag_length = strlen(tag);
  if (tag_length > UINT32_MAX) return false;
  digest_u32(state, (uint32_t)tag_length);
  w_seed_sha256_update(state, (const uint8_t *)tag, tag_length);
  digest_u64(state, payload_length);
  return true;
}

static bool digest_frame_bytes(w_seed_sha256_state *state, const char *tag,
                               const uint8_t *bytes, size_t length) {
  return digest_frame_prefix(state, tag, (uint64_t)length) &&
         (length == 0u || bytes != NULL) &&
         (length == 0u || (w_seed_sha256_update(state, bytes, length), true));
}

static bool binding_none_valid(const w_seed_manifest_document *document) {
  return document != NULL &&
         document->binding_kind == W_SEED_MANIFEST_BINDING_NONE &&
         document->generation == 0u && document->candidate.generation == 0u &&
         document->candidate.directory_ordinal == 0u &&
         document->candidate.candidate_index == 0u &&
         bytes_zero(document->context_binding,
                    sizeof(document->context_binding)) &&
         bytes_zero(document->candidate_binding,
                    sizeof(document->candidate_binding));
}

static bool digest_binding(const w_seed_manifest_document *document,
                           w_seed_sha256_state *state) {
  if (document == NULL || state == NULL) return false;
  if (document->binding_kind == W_SEED_MANIFEST_BINDING_NONE) {
    const uint8_t kind = 0u;
    const uint8_t zero[32] = {0};
    w_seed_sha256_update(state, &kind, 1u);
    digest_u64(state, 0u);
    if (!digest_frame_prefix(state, "candidate-ref", 16u)) return false;
    digest_u64(state, 0u);
    digest_u32(state, 0u);
    digest_u32(state, 0u);
    w_seed_sha256_update(state, zero, sizeof(zero));
    w_seed_sha256_update(state, zero, sizeof(zero));
    return true;
  }
  if (document->binding_kind != W_SEED_MANIFEST_BINDING_OWNER_GUARD)
    return false;
  const uint8_t kind = 1u;
  w_seed_sha256_update(state, &kind, 1u);
  digest_u64(state, document->generation);
  if (!digest_frame_prefix(state, "candidate-ref", 16u)) return false;
  digest_u64(state, document->candidate.generation);
  if (document->candidate.directory_ordinal > UINT32_MAX ||
      document->candidate.candidate_index > UINT32_MAX)
    return false;
  digest_u32(state, (uint32_t)document->candidate.directory_ordinal);
  digest_u32(state, (uint32_t)document->candidate.candidate_index);
  w_seed_sha256_update(state, document->context_binding, 32u);
  w_seed_sha256_update(state, document->candidate_binding, 32u);
  return true;
}

static bool digest_binding_size(const w_seed_manifest_document *document,
                                uint64_t *size) {
  uint64_t candidate = 0u;
  if (document == NULL || size == NULL ||
      !digest_frame_size("candidate-ref", 16u, &candidate) ||
      document->binding_kind > W_SEED_MANIFEST_BINDING_OWNER_GUARD)
    return false;
  *size = UINT64_C(1) + UINT64_C(8) + candidate + UINT64_C(64);
  return true;
}

static void digest_limits(w_seed_sha256_state *state,
                          const w_seed_manifest_limits *limits) {
  digest_u32(state, limits->max_document_bytes);
  digest_u32(state, limits->max_aggregate_bytes);
  digest_u32(state, limits->max_nesting);
  digest_u32(state, limits->max_structural_nodes);
  digest_u32(state, limits->max_roots_per_document);
  digest_u32(state, limits->max_documents);
  digest_u32(state, limits->max_scalar_source_bytes);
  digest_u32(state, limits->max_number_digits);
  digest_u32(state, limits->max_decoded_scalar_bytes);
  digest_u32(state, limits->max_canonical_bytes);
  digest_u64(state, limits->max_work_units);
}

static void digest_counts(w_seed_sha256_state *state,
                          w_seed_manifest_counts counts, bool document) {
  digest_u32(state, document ? 1u : counts.documents);
  digest_u32(state, counts.roots);
  digest_u32(state, counts.nodes);
  digest_u32(state, counts.fields);
  digest_u32(state, counts.edges);
  digest_u32(state, counts.canonical_bytes);
  digest_u32(state, counts.structural_nodes);
}

static bool semantic_value_size(const w_seed_manifest_program *program,
                                uint32_t node_index, uint32_t depth,
                                uint64_t *size);

static void semantic_value_update(const w_seed_manifest_program *program,
                                  uint32_t node_index,
                                  w_seed_sha256_state *state, uint32_t depth);

static bool semantic_roots_size(const w_seed_manifest_program *program,
                                const w_seed_manifest_document *document,
                                uint64_t *size) {
  if (program == NULL || document == NULL || size == NULL) return false;
  uint64_t payload = 4u;
  for (uint32_t index = 0u; index < document->root_count; index += 1u) {
    const w_seed_manifest_root *root =
        &program->roots[document->first_root + index];
    uint64_t value_size = 0u;
    uint64_t root_frame = 0u;
    uint64_t item_frame = 0u;
    if (!semantic_value_size(program, root->record_node, 0u, &value_size) ||
        !digest_add(1u, value_size, &root_frame) ||
        !digest_frame_size("root", root_frame, &root_frame) ||
        !digest_frame_size("item", root_frame, &item_frame) ||
        !digest_add(payload, item_frame, &payload))
      return false;
  }
  return digest_frame_size("roots", payload, size);
}

static void semantic_roots_update(const w_seed_manifest_program *program,
                                  const w_seed_manifest_document *document,
                                  w_seed_sha256_state *state) {
  uint64_t sequence_size = 0u;
  (void)semantic_roots_size(program, document, &sequence_size);
  const uint64_t payload = sequence_size -
                           (UINT64_C(12) + (sizeof("roots") - 1u));
  (void)digest_frame_prefix(state, "roots", payload);
  digest_u32(state, document->root_count);
  for (uint32_t index = 0u; index < document->root_count; index += 1u) {
    const w_seed_manifest_root *root =
        &program->roots[document->first_root + index];
    uint64_t value_size = 0u;
    (void)semantic_value_size(program, root->record_node, 0u, &value_size);
    const uint64_t root_payload = 1u + value_size;
    uint64_t root_size = 0u;
    (void)digest_frame_size("root", root_payload, &root_size);
    (void)digest_frame_prefix(state, "item", root_size);
    (void)digest_frame_prefix(state, "root", root_payload);
    const uint8_t kind = (uint8_t)root->kind;
    w_seed_sha256_update(state, &kind, 1u);
    semantic_value_update(program, root->record_node, state, 0u);
  }
}

static bool document_semantic_digest(const w_seed_manifest_program *program,
                                     const w_seed_manifest_document *document,
                                     uint8_t digest[32]) {
  uint64_t roots_size = 0u;
  uint64_t schema_size = 0u;
  uint64_t payload_size = 0u;
  if (!semantic_roots_size(program, document, &roots_size) ||
      !digest_frame_size("schema", sizeof(W_SEED_MANIFEST_SCHEMA_VERSION) - 1u,
                         &schema_size) ||
      !digest_add(schema_size, roots_size, &payload_size))
    return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  if (!digest_frame_prefix(&state, W_SEED_MANIFEST_DOCUMENT_SEMANTIC_TAG,
                           payload_size) ||
      !digest_frame_bytes(&state, "schema",
                          (const uint8_t *)W_SEED_MANIFEST_SCHEMA_VERSION,
                          sizeof(W_SEED_MANIFEST_SCHEMA_VERSION) - 1u))
    return false;
  semantic_roots_update(program, document, &state);
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool document_provenance_digest(
    const w_seed_manifest_document *document, uint32_t ordinal,
    uint8_t digest[32]) {
  uint64_t schema_size = 0u;
  uint64_t ordinal_size = 0u;
  uint64_t binding_size = 0u;
  uint64_t source_size = 0u;
  uint64_t payload_size = 0u;
  if (document == NULL || digest == NULL ||
      !digest_frame_size("schema", sizeof(W_SEED_MANIFEST_SCHEMA_VERSION) - 1u,
                         &schema_size) ||
      !digest_frame_size("candidate-ordinal", 4u, &ordinal_size) ||
      !digest_binding_size(document, &binding_size) ||
      !digest_frame_size("source-digest", 32u, &source_size) ||
      !digest_add(schema_size, ordinal_size, &payload_size) ||
      !digest_add(payload_size, binding_size, &payload_size) ||
      !digest_add(payload_size, source_size, &payload_size))
    return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  if (!digest_frame_prefix(&state, W_SEED_MANIFEST_DOCUMENT_PROVENANCE_TAG,
                           payload_size) ||
      !digest_frame_bytes(&state, "schema",
                          (const uint8_t *)W_SEED_MANIFEST_SCHEMA_VERSION,
                          sizeof(W_SEED_MANIFEST_SCHEMA_VERSION) - 1u) ||
      !digest_frame_prefix(&state, "candidate-ordinal", 4u))
    return false;
  digest_u32(&state, ordinal);
  if (!digest_binding(document, &state) ||
      !digest_frame_bytes(&state, "source-digest", document->source_digest,
                          sizeof(document->source_digest)))
    return false;
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool document_receipt_digest(
    const w_seed_manifest_limits *limits,
    const w_seed_manifest_document *document, uint8_t digest[32]) {
  uint64_t schema_size = 0u;
  uint64_t limits_size = 0u;
  uint64_t counts_size = 0u;
  uint64_t binding_size = 0u;
  uint64_t source_size = 0u;
  uint64_t semantic_size = 0u;
  uint64_t provenance_size = 0u;
  uint64_t payload_size = 0u;
  if (limits == NULL || document == NULL || digest == NULL ||
      !digest_frame_size("schema", sizeof(W_SEED_MANIFEST_SCHEMA_VERSION) - 1u,
                         &schema_size) ||
      !digest_frame_size("limits", 48u, &limits_size) ||
      !digest_frame_size("counts", 28u, &counts_size) ||
      !digest_binding_size(document, &binding_size) ||
      !digest_frame_size("binding", binding_size, &binding_size) ||
      !digest_frame_size("source-digest", 32u, &source_size) ||
      !digest_frame_size("semantic-digest", 32u, &semantic_size) ||
      !digest_frame_size("provenance-digest", 32u, &provenance_size) ||
      !digest_add(schema_size, limits_size, &payload_size) ||
      !digest_add(payload_size, counts_size, &payload_size) ||
      !digest_add(payload_size, binding_size, &payload_size) ||
      !digest_add(payload_size, source_size, &payload_size) ||
      !digest_add(payload_size, semantic_size, &payload_size) ||
      !digest_add(payload_size, provenance_size, &payload_size))
    return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  if (!digest_frame_prefix(&state, W_SEED_MANIFEST_DOCUMENT_RECEIPT_TAG,
                           payload_size) ||
      !digest_frame_bytes(&state, "schema",
                          (const uint8_t *)W_SEED_MANIFEST_SCHEMA_VERSION,
                          sizeof(W_SEED_MANIFEST_SCHEMA_VERSION) - 1u) ||
      !digest_frame_prefix(&state, "limits", 48u))
    return false;
  digest_limits(&state, limits);
  (void)digest_frame_prefix(&state, "counts", 28u);
  digest_counts(&state, document->counts, true);
  (void)digest_frame_prefix(&state, "binding", binding_size -
                                             (UINT64_C(12) +
                                              (sizeof("binding") - 1u)));
  if (!digest_binding(document, &state) ||
      !digest_frame_bytes(&state, "source-digest", document->source_digest, 32u) ||
      !digest_frame_bytes(&state, "semantic-digest", document->semantic_digest, 32u) ||
      !digest_frame_bytes(&state, "provenance-digest", document->provenance_digest,
                          32u))
    return false;
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool batch_digest(const w_seed_manifest_program *program,
                         const w_seed_manifest_limits *limits,
                         const w_seed_manifest_counts *counts,
                         uint8_t semantic[32], uint8_t provenance[32],
                         uint8_t receipt[32]) {
  if (program == NULL || limits == NULL || counts == NULL || semantic == NULL ||
      provenance == NULL || receipt == NULL)
    return false;
  uint8_t document_semantic[W_SEED_MANIFEST_MAX_DOCUMENTS][32];
  uint8_t document_provenance[W_SEED_MANIFEST_MAX_DOCUMENTS][32];
  uint8_t document_receipt[W_SEED_MANIFEST_MAX_DOCUMENTS][32];
  uint64_t schema_size = 0u;
  uint64_t docs_semantic_payload = 4u;
  uint64_t docs_provenance_payload = 4u;
  uint64_t docs_receipt_payload = 4u;
  for (uint32_t index = 0u; index < counts->documents; index += 1u) {
    const w_seed_manifest_document *document = &program->documents[index];
    const uint64_t item_size = UINT64_C(4) + UINT64_C(4) + UINT64_C(8) +
                               UINT64_C(32);
    if (!document_semantic_digest(program, document, document_semantic[index]) ||
        !document_provenance_digest(document, index, document_provenance[index]) ||
        !document_receipt_digest(limits, document, document_receipt[index]) ||
        !digest_add(docs_semantic_payload, item_size,
                    &docs_semantic_payload) ||
        !digest_add(docs_provenance_payload, item_size,
                    &docs_provenance_payload) ||
        !digest_add(docs_receipt_payload, item_size,
                    &docs_receipt_payload))
      return false;
  }
  if (!digest_frame_size("schema", sizeof(W_SEED_MANIFEST_SCHEMA_VERSION) - 1u,
                         &schema_size))
    return false;
  uint64_t semantic_payload = 0u;
  uint64_t provenance_payload = 0u;
  if (!digest_frame_size("documents", docs_semantic_payload, &docs_semantic_payload) ||
      !digest_frame_size("documents", docs_provenance_payload, &docs_provenance_payload) ||
      !digest_add(schema_size, docs_semantic_payload, &semantic_payload) ||
      !digest_add(schema_size, docs_provenance_payload, &provenance_payload))
    return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  (void)digest_frame_prefix(&state, W_SEED_MANIFEST_BATCH_SEMANTIC_TAG,
                            semantic_payload);
  (void)digest_frame_bytes(&state, "schema",
                           (const uint8_t *)W_SEED_MANIFEST_SCHEMA_VERSION,
                           sizeof(W_SEED_MANIFEST_SCHEMA_VERSION) - 1u);
  (void)digest_frame_prefix(&state, "documents", docs_semantic_payload -
                                                (UINT64_C(12) +
                                                 (sizeof("documents") - 1u)));
  digest_u32(&state, counts->documents);
  for (uint32_t index = 0u; index < counts->documents; index += 1u) {
    (void)digest_frame_prefix(&state, "item", 32u);
    w_seed_sha256_update(&state, document_semantic[index], 32u);
  }
  w_seed_sha256_final(&state, semantic);

  w_seed_sha256_init(&state);
  (void)digest_frame_prefix(&state, W_SEED_MANIFEST_BATCH_PROVENANCE_TAG,
                            provenance_payload);
  (void)digest_frame_bytes(&state, "schema",
                           (const uint8_t *)W_SEED_MANIFEST_SCHEMA_VERSION,
                           sizeof(W_SEED_MANIFEST_SCHEMA_VERSION) - 1u);
  (void)digest_frame_prefix(&state, "documents", docs_provenance_payload -
                                                (UINT64_C(12) +
                                                 (sizeof("documents") - 1u)));
  digest_u32(&state, counts->documents);
  for (uint32_t index = 0u; index < counts->documents; index += 1u) {
    (void)digest_frame_prefix(&state, "item", 32u);
    w_seed_sha256_update(&state, document_provenance[index], 32u);
  }
  w_seed_sha256_final(&state, provenance);

  uint64_t limits_size = 0u;
  uint64_t counts_size = 0u;
  uint64_t semantic_digest_size = 0u;
  uint64_t provenance_digest_size = 0u;
  uint64_t document_sequence_size = 0u;
  uint64_t receipt_payload = 0u;
  if (!digest_frame_size("limits", 48u, &limits_size) ||
      !digest_frame_size("counts", 28u, &counts_size) ||
      !digest_frame_size("semantic-digest", 32u, &semantic_digest_size) ||
      !digest_frame_size("provenance-digest", 32u, &provenance_digest_size) ||
      !digest_frame_size("documents", docs_receipt_payload, &document_sequence_size) ||
      !digest_add(schema_size, limits_size, &receipt_payload) ||
      !digest_add(receipt_payload, counts_size, &receipt_payload) ||
      !digest_add(receipt_payload, semantic_digest_size, &receipt_payload) ||
      !digest_add(receipt_payload, provenance_digest_size, &receipt_payload) ||
      !digest_add(receipt_payload, document_sequence_size, &receipt_payload))
    return false;
  w_seed_sha256_init(&state);
  (void)digest_frame_prefix(&state, W_SEED_MANIFEST_BATCH_RECEIPT_TAG,
                            receipt_payload);
  (void)digest_frame_bytes(&state, "schema",
                           (const uint8_t *)W_SEED_MANIFEST_SCHEMA_VERSION,
                           sizeof(W_SEED_MANIFEST_SCHEMA_VERSION) - 1u);
  (void)digest_frame_prefix(&state, "limits", 48u);
  digest_limits(&state, limits);
  (void)digest_frame_prefix(&state, "counts", 28u);
  digest_counts(&state, *counts, false);
  (void)digest_frame_bytes(&state, "semantic-digest", semantic, 32u);
  (void)digest_frame_bytes(&state, "provenance-digest", provenance, 32u);
   (void)digest_frame_prefix(&state, "documents", docs_receipt_payload);
  digest_u32(&state, counts->documents);
  for (uint32_t index = 0u; index < counts->documents; index += 1u) {
    (void)digest_frame_prefix(&state, "item", 32u);
    w_seed_sha256_update(&state, document_receipt[index], 32u);
  }
  w_seed_sha256_final(&state, receipt);
  return true;
}

static bool semantic_value_size(const w_seed_manifest_program *program,
                                uint32_t node_index, uint32_t depth,
                                uint64_t *size);

static bool semantic_sequence_size(const w_seed_manifest_program *program,
                                   const char *tag, uint32_t first,
                                   uint32_t count, bool fields, uint32_t depth,
                                   uint64_t *size) {
  if (program == NULL || size == NULL || depth > W_SEED_MANIFEST_MAX_NESTING)
    return false;
  uint64_t payload = 4u;
  for (uint32_t index = 0u; index < count; index += 1u) {
    uint64_t item_payload = 0u;
    uint64_t item_size = 0u;
    if (fields) {
      const w_seed_manifest_field *field = &program->fields[first + index];
      const size_t name_length = field->name_span.end_byte -
                                 field->name_span.start_byte;
      uint64_t name_frame = 0u;
      uint64_t value_frame = 0u;
      uint64_t field_frame = 0u;
      uint64_t item_frame = 0u;
      if (!digest_frame_size("name", (uint64_t)name_length, &name_frame) ||
          !semantic_value_size(program, field->value_node, depth + 1u,
                               &value_frame) ||
          !digest_add(name_frame, value_frame, &item_payload) ||
          !digest_frame_size("field", item_payload, &field_frame) ||
          !digest_frame_size("item", field_frame, &item_frame) ||
          !digest_add(payload, item_frame, &payload))
        return false;
    } else {
      const w_seed_manifest_edge *edge = &program->edges[first + index];
      if (!semantic_value_size(program, edge->value_node, depth + 1u,
                               &item_payload))
        return false;
      if (strcmp(tag, "arguments") == 0) {
        uint64_t option_size = 1u;
        if (edge->has_label) {
          const size_t label_length = edge->label_span.end_byte -
                                      edge->label_span.start_byte;
          uint64_t label_frame = 0u;
          if (!digest_frame_size("label", (uint64_t)label_length,
                                 &label_frame) ||
              !digest_add(option_size, label_frame, &option_size))
            return false;
        }
        if (!digest_add(option_size, item_payload, &item_payload) ||
            !digest_frame_size("argument", item_payload, &item_payload))
          return false;
      }
      if (!digest_frame_size("item", item_payload, &item_size) ||
          !digest_add(payload, item_size, &payload))
        return false;
    }
  }
  return digest_frame_size(tag, payload, size);
}

static bool semantic_value_size(const w_seed_manifest_program *program,
                                uint32_t node_index, uint32_t depth,
                                uint64_t *size) {
  if (program == NULL || size == NULL || node_index >= program->node_count ||
      depth > W_SEED_MANIFEST_MAX_NESTING)
    return false;
  const w_seed_manifest_node *node = &program->nodes[node_index];
  uint64_t payload = 1u;
  switch (node->kind) {
    case W_SEED_MANIFEST_NODE_RECORD:
      if (!semantic_sequence_size(program, "fields", node->first_child,
                                  node->child_count, true, depth, &payload))
        return false;
      /* The sequence is already a full frame. */
      if (payload > UINT64_MAX - 1u) return false;
      payload += 1u;
      break;
    case W_SEED_MANIFEST_NODE_LIST:
      if (!semantic_sequence_size(program, "items", node->first_child,
                                  node->child_count, false, depth, &payload))
        return false;
      if (payload > UINT64_MAX - 1u) return false;
      payload += 1u;
      break;
    case W_SEED_MANIFEST_NODE_CONSTRUCTOR: {
      const size_t name_length = node->name_span.end_byte -
                                 node->name_span.start_byte;
      uint64_t name_frame = 0u;
      if (!digest_frame_size("name", (uint64_t)name_length, &name_frame))
        return false;
      uint64_t args = 0u;
      if (!semantic_sequence_size(program, "arguments", node->first_child,
                                  node->child_count, false, depth, &args))
        return false;
      if (!digest_add(1u, name_frame, &payload) ||
          !digest_add(payload, args, &payload))
        return false;
      break;
    }
    case W_SEED_MANIFEST_NODE_MEMBER: {
      const size_t name_length = node->name_span.end_byte -
                                 node->name_span.start_byte;
      uint64_t name_frame = 0u;
      if (!digest_frame_size("name", (uint64_t)name_length, &name_frame) ||
          !digest_add(payload, name_frame, &payload))
        return false;
      break;
    }
    case W_SEED_MANIFEST_NODE_STRING:
    case W_SEED_MANIFEST_NODE_NUMBER:
    case W_SEED_MANIFEST_NODE_SIZE:
    case W_SEED_MANIFEST_NODE_QUANTITY: {
      uint64_t scalar = 0u;
      if (!digest_frame_size("scalar", node->canonical.length, &scalar) ||
          !digest_add(payload, scalar, &payload))
        return false;
      break;
    }
    case W_SEED_MANIFEST_NODE_BOOL:
      if (!digest_add(payload, 1u, &payload)) return false;
      break;
    default:
      return false;
  }
  return digest_frame_size("value", payload, size);
}

static void semantic_value_update(const w_seed_manifest_program *program,
                                  uint32_t node_index,
                                  w_seed_sha256_state *state, uint32_t depth);

static void semantic_sequence_update(const w_seed_manifest_program *program,
                                     const char *tag, uint32_t first,
                                     uint32_t count, bool fields,
                                     w_seed_sha256_state *state, uint32_t depth) {
  uint64_t sequence_size = 0u;
  (void)semantic_sequence_size(program, tag, first, count, fields, depth,
                               &sequence_size);
  uint64_t sequence_payload = sequence_size -
                              (UINT64_C(12) + (uint64_t)(strlen(tag)));
  (void)digest_frame_prefix(state, tag, sequence_payload);
  digest_u32(state, count);
  for (uint32_t index = 0u; index < count; index += 1u) {
    uint64_t item_payload = 0u;
    uint64_t item_size = 0u;
    if (fields) {
      const w_seed_manifest_field *field = &program->fields[first + index];
      uint64_t name_frame = 0u;
      const size_t name_length = field->name_span.end_byte -
                                 field->name_span.start_byte;
      (void)digest_frame_size("name", (uint64_t)name_length, &name_frame);
      uint64_t value_size = 0u;
      (void)semantic_value_size(program, field->value_node, depth + 1u,
                                &value_size);
      (void)digest_add(name_frame, value_size, &item_payload);
      uint64_t field_frame = 0u;
      (void)digest_frame_size("field", item_payload, &field_frame);
      (void)digest_frame_size("item", field_frame, &item_size);
      (void)digest_frame_prefix(state, "item", field_frame);
      (void)digest_frame_prefix(state, "field", name_frame + value_size);
      const uint32_t document_index =
          program->nodes[field->owner_record].document_index;
      (void)digest_frame_bytes(state, "name",
                               program->documents[document_index].source.data +
                                   field->name_span.start_byte,
                               name_length);
      semantic_value_update(program, field->value_node, state, depth + 1u);
    } else {
      const w_seed_manifest_edge *edge = &program->edges[first + index];
      uint64_t value_size = 0u;
      (void)semantic_value_size(program, edge->value_node, depth + 1u,
                                &value_size);
      if (strcmp(tag, "arguments") == 0) {
        uint64_t option_size = 1u;
        if (edge->has_label) {
          const size_t label_length = edge->label_span.end_byte -
                                      edge->label_span.start_byte;
          uint64_t label_frame = 0u;
          (void)digest_frame_size("label", (uint64_t)label_length,
                                  &label_frame);
          option_size += label_frame;
        }
        item_payload = option_size + value_size;
        (void)digest_frame_size("argument", item_payload, &item_payload);
      } else {
        item_payload = value_size;
      }
      (void)digest_frame_size("item", item_payload, &item_size);
      (void)digest_frame_prefix(state, "item", item_payload);
      if (strcmp(tag, "arguments") == 0) {
        uint64_t argument_payload = item_payload -
                                    (UINT64_C(12) +
                                     (sizeof("argument") - 1u));
        (void)digest_frame_prefix(state, "argument", argument_payload);
        const uint8_t has_label = edge->has_label ? 1u : 0u;
        w_seed_sha256_update(state, &has_label, 1u);
        if (edge->has_label) {
          const uint32_t document_index =
              program->nodes[edge->owner_node].document_index;
          const size_t label_length = edge->label_span.end_byte -
                                      edge->label_span.start_byte;
          (void)digest_frame_bytes(
              state, "label",
              program->documents[document_index].source.data +
                  edge->label_span.start_byte,
              label_length);
        }
        semantic_value_update(program, edge->value_node, state, depth + 1u);
      } else {
        semantic_value_update(program, edge->value_node, state, depth + 1u);
      }
    }
  }
}

static void semantic_value_update(const w_seed_manifest_program *program,
                                  uint32_t node_index,
                                  w_seed_sha256_state *state, uint32_t depth) {
  const w_seed_manifest_node *node = &program->nodes[node_index];
  uint64_t payload = 1u;
  uint64_t child_size = 0u;
  switch (node->kind) {
    case W_SEED_MANIFEST_NODE_RECORD:
      (void)semantic_sequence_size(program, "fields", node->first_child,
                                   node->child_count, true, depth, &child_size);
      payload += child_size;
      break;
    case W_SEED_MANIFEST_NODE_LIST:
      (void)semantic_sequence_size(program, "items", node->first_child,
                                   node->child_count, false, depth, &child_size);
      payload += child_size;
      break;
    case W_SEED_MANIFEST_NODE_CONSTRUCTOR: {
      const uint32_t document_index = node->document_index;
      const size_t name_length = node->name_span.end_byte -
                                 node->name_span.start_byte;
      (void)digest_frame_size("name", (uint64_t)name_length, &child_size);
      payload += child_size;
      (void)semantic_sequence_size(program, "arguments", node->first_child,
                                   node->child_count, false, depth, &child_size);
      payload += child_size;
      (void)document_index;
      break;
    }
    case W_SEED_MANIFEST_NODE_MEMBER: {
      const size_t name_length = node->name_span.end_byte -
                                 node->name_span.start_byte;
      (void)digest_frame_size("name", (uint64_t)name_length, &child_size);
      payload += child_size;
      break;
    }
    case W_SEED_MANIFEST_NODE_STRING:
    case W_SEED_MANIFEST_NODE_NUMBER:
    case W_SEED_MANIFEST_NODE_SIZE:
    case W_SEED_MANIFEST_NODE_QUANTITY:
      (void)digest_frame_size("scalar", node->canonical.length, &child_size);
      payload += child_size;
      break;
    case W_SEED_MANIFEST_NODE_BOOL:
      payload += 1u;
      break;
    default:
      return;
  }
  (void)digest_frame_prefix(state, "value", payload);
  const uint8_t kind = (uint8_t)node->kind;
  w_seed_sha256_update(state, &kind, 1u);
  switch (node->kind) {
    case W_SEED_MANIFEST_NODE_RECORD:
      semantic_sequence_update(program, "fields", node->first_child,
                               node->child_count, true, state, depth);
      break;
    case W_SEED_MANIFEST_NODE_LIST:
      semantic_sequence_update(program, "items", node->first_child,
                               node->child_count, false, state, depth);
      break;
    case W_SEED_MANIFEST_NODE_CONSTRUCTOR: {
      const w_seed_manifest_document *document =
          &program->documents[node->document_index];
      const size_t name_length = node->name_span.end_byte -
                                 node->name_span.start_byte;
      (void)digest_frame_bytes(state, "name",
                               document->source.data + node->name_span.start_byte,
                               name_length);
      semantic_sequence_update(program, "arguments", node->first_child,
                               node->child_count, false, state, depth);
      break;
    }
    case W_SEED_MANIFEST_NODE_MEMBER: {
      const w_seed_manifest_document *document =
          &program->documents[node->document_index];
      const size_t name_length = node->name_span.end_byte -
                                 node->name_span.start_byte;
      (void)digest_frame_bytes(state, "name",
                               document->source.data + node->name_span.start_byte,
                               name_length);
      break;
    }
    case W_SEED_MANIFEST_NODE_STRING:
    case W_SEED_MANIFEST_NODE_NUMBER:
    case W_SEED_MANIFEST_NODE_SIZE:
    case W_SEED_MANIFEST_NODE_QUANTITY:
      (void)digest_frame_bytes(state, "scalar",
                               program->canonical_bytes + node->canonical.offset,
                               node->canonical.length);
      break;
    case W_SEED_MANIFEST_NODE_BOOL: {
      const uint8_t value = node->boolean_value ? 1u : 0u;
      w_seed_sha256_update(state, &value, 1u);
      break;
    }
    default:
      break;
  }
}

static bool manifest_counts_valid(w_seed_manifest_counts counts,
                                  w_seed_manifest_limits limits) {
  return counts.documents != 0u && counts.documents <= limits.max_documents &&
         counts.documents <= W_SEED_MANIFEST_MAX_DOCUMENTS &&
         counts.roots != 0u && counts.nodes != 0u &&
         counts.structural_nodes != 0u &&
         counts.structural_nodes <= limits.max_structural_nodes &&
         counts.canonical_bytes <= limits.max_canonical_bytes;
}

static bool manifest_output_self_valid(const w_seed_manifest_output *output) {
  if (output == NULL) return false;
  man0_range descriptor;
  man0_range ranges[8u];
  size_t count = 0u;
  if (!range_from(output, 1u, sizeof(*output), &descriptor) ||
      !manifest_output_ranges(output, ranges, &count) ||
      !ranges_pairwise_disjoint(ranges, count))
    return false;
  for (size_t index = 0u; index < count; index += 1u)
    if (ranges_overlap(descriptor, ranges[index])) return false;
  return true;
}

static bool source_digest(w_seed_byte_view source, uint8_t digest[32]) {
  if (digest == NULL || source.length > UINT64_MAX ||
      (source.length != 0u && source.data == NULL))
    return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  if (!digest_frame_prefix(&state, W_SEED_MANIFEST_DOCUMENT_SOURCE_TAG,
                           (uint64_t)source.length) ||
      (source.length != 0u && source.data == NULL))
    return false;
  if (source.length != 0u) w_seed_sha256_update(&state, source.data, source.length);
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool result_ok(const w_seed_manifest_result *result,
                      bool guarded) {
  if (result == NULL || result->status != W_SEED_MANIFEST_OK ||
      (guarded ? result->phase != W_SEED_MANIFEST_PHASE_COMMIT
                : (result->phase != W_SEED_MANIFEST_PHASE_RUN &&
                   result->phase != W_SEED_MANIFEST_PHASE_COMMIT)) ||
      memcmp(result->schema, W_SEED_MANIFEST_SCHEMA_VERSION,
             sizeof(result->schema)) != 0 ||
      result->error != W_SEED_MANIFEST_ERROR_NONE ||
      (!guarded &&
       (result->backend_status != W_SEED_MANIFEST_BACKEND_NOT_CALLED ||
        result->backend_phase != W_SEED_MANIFEST_BACKEND_PHASE_NONE ||
        result->owner_guard_status != W_SEED_OWNER_GUARD_OK ||
        result->owner_guard_revalidate_called)) ||
      (guarded &&
       (result->backend_status != W_SEED_MANIFEST_BACKEND_OK ||
        result->backend_phase != W_SEED_MANIFEST_BACKEND_PHASE_CLOSE ||
        result->owner_guard_status != W_SEED_OWNER_GUARD_OK ||
        !result->owner_guard_revalidate_called)) ||
      result->document_index != W_SEED_MANIFEST_NONE ||
      result->candidate_index != W_SEED_MANIFEST_NONE ||
      result->byte_offset != W_SEED_MANIFEST_NO_BYTE ||
      result->required_byte_capacity != 0u ||
      !limits_valid(result->limits) ||
      !counts_equal(result->required, result->written) ||
      !manifest_counts_valid(result->written, result->limits))
    return false;
  return true;
}

static bool program_ranges_table(const w_seed_manifest_program *program,
                                 man0_range *ranges, size_t *count) {
  if (program == NULL || ranges == NULL || count == NULL) return false;
  *count = 0u;
#define MANIFEST_PROGRAM_RANGE(field, capacity_field)                         \
  if (*count >= 8u ||                                                           \
      !range_from(program->field, program->capacity_field,                      \
                  sizeof(*program->field), &ranges[(*count)++]))               \
    return false
  MANIFEST_PROGRAM_RANGE(documents, document_capacity);
  MANIFEST_PROGRAM_RANGE(roots, root_capacity);
  MANIFEST_PROGRAM_RANGE(nodes, node_capacity);
  MANIFEST_PROGRAM_RANGE(fields, field_capacity);
  MANIFEST_PROGRAM_RANGE(edges, edge_capacity);
#undef MANIFEST_PROGRAM_RANGE
  if (*count >= 8u ||
      !range_from(program->canonical_bytes, program->canonical_byte_capacity,
                  1u, &ranges[(*count)++]))
    return false;
  return true;
}

static bool program_aliases(const w_seed_manifest_program *program) {
  if (program == NULL) return true;
  man0_range descriptor;
  man0_range ranges[8u];
  size_t count = 0u;
  if (!range_from(program, 1u, sizeof(*program), &descriptor) ||
      !program_ranges_table(program, ranges, &count) ||
      !ranges_pairwise_disjoint(ranges, count))
    return true;
  for (size_t index = 0u; index < count; index += 1u)
    if (ranges_overlap(descriptor, ranges[index])) return true;
  return false;
}

static bool program_envelope_valid(const w_seed_manifest_program *program,
                                   w_seed_manifest_counts counts,
                                   w_seed_manifest_limits limits) {
  if (program == NULL || !manifest_counts_valid(counts, limits) ||
      program->document_count != (size_t)counts.documents ||
      program->root_count != (size_t)counts.roots ||
      program->node_count != (size_t)counts.nodes ||
      program->field_count != (size_t)counts.fields ||
      program->edge_count != (size_t)counts.edges ||
      program->canonical_byte_count != (size_t)counts.canonical_bytes ||
      program->document_count > program->document_capacity ||
      program->root_count > program->root_capacity ||
      program->node_count > program->node_capacity ||
      program->field_count > program->field_capacity ||
      program->edge_count > program->edge_capacity ||
      program->canonical_byte_count > program->canonical_byte_capacity ||
      (program->document_count != 0u && program->documents == NULL) ||
      (program->root_count != 0u && program->roots == NULL) ||
      (program->node_count != 0u && program->nodes == NULL) ||
      (program->field_count != 0u && program->fields == NULL) ||
      (program->edge_count != 0u && program->edges == NULL) ||
      (program->canonical_byte_count != 0u &&
       program->canonical_bytes == NULL))
    return false;
  man0_range ranges[8u];
  size_t range_count = 0u;
  if (!program_ranges_table(program, ranges, &range_count) ||
      !ranges_pairwise_disjoint(ranges, range_count))
    return false;
  return true;
}

static bool span_present_in_source(w_seed_span span, size_t length) {
  return !span_absent(span) && span_in_source(span, length);
}

static bool source_span_compare(const w_seed_byte_view source,
                                w_seed_span left, w_seed_span right,
                                int *comparison) {
  if (comparison == NULL || !span_in_source(left, source.length) ||
      !span_in_source(right, source.length))
    return false;
  const size_t left_length = left.end_byte - left.start_byte;
  const size_t right_length = right.end_byte - right.start_byte;
  const size_t common = left_length < right_length ? left_length : right_length;
  if (common != 0u && source.data == NULL) return false;
  const int result = common == 0u
                         ? 0
                         : memcmp(source.data + left.start_byte,
                                  source.data + right.start_byte, common);
  *comparison = result != 0
                    ? result
                    : left_length < right_length
                          ? -1
                          : left_length > right_length ? 1 : 0;
  return true;
}

static bool program_shape_valid(const w_seed_manifest_program *program,
                                const w_seed_manifest_result *result,
                                bool guarded) {
  if (!result_ok(result, guarded) ||
      !program_envelope_valid(program, result->written, result->limits) ||
      program_aliases(program))
    return false;
  const w_seed_manifest_counts counts = result->written;
  size_t aggregate = 0u;
  uint32_t root_cursor = 0u;
  uint32_t node_cursor = 0u;
  uint32_t field_cursor = 0u;
  uint32_t edge_cursor = 0u;
  uint32_t canonical_cursor = 0u;
  man0_range source_ranges[W_SEED_MANIFEST_MAX_DOCUMENTS];
  size_t source_range_count = 0u;
  for (uint32_t document_index = 0u; document_index < counts.documents;
       document_index += 1u) {
    const w_seed_manifest_document *document =
        &program->documents[document_index];
    const uint64_t document_structural =
        (uint64_t)document->root_count + (uint64_t)document->node_count +
        (uint64_t)document->field_count + (uint64_t)document->edge_count;
    if (document->source.length == 0u ||
        document->source.length > result->limits.max_document_bytes ||
        document->source.data == NULL ||
        !range_from(document->source.data, document->source.length, 1u,
                    &source_ranges[source_range_count++]) ||
        !(guarded ?
             (document->binding_kind ==
                  W_SEED_MANIFEST_BINDING_OWNER_GUARD &&
              document->generation != 0u &&
              document->candidate.generation == document->generation &&
              document->candidate.candidate_index == document_index &&
              document->candidate.directory_ordinal <= UINT32_MAX)
             : binding_none_valid(document)) ||
        document->counts.roots != document->root_count ||
        document->counts.nodes != document->node_count ||
        document->counts.fields != document->field_count ||
        document->counts.edges != document->edge_count ||
        document->counts.canonical_bytes != document->canonical_byte_count ||
        document_structural > UINT32_MAX ||
        document->counts.structural_nodes != (uint32_t)document_structural ||
        document->first_root != root_cursor ||
        document->first_node != node_cursor ||
        document->first_field != field_cursor ||
        document->first_edge != edge_cursor ||
        document->first_canonical_byte != canonical_cursor ||
        !range_count_valid(document->first_root, document->root_count,
                           program->root_count) ||
        !range_count_valid(document->first_node, document->node_count,
                           program->node_count) ||
        !range_count_valid(document->first_field, document->field_count,
                           program->field_count) ||
        !range_count_valid(document->first_edge, document->edge_count,
                           program->edge_count) ||
        !range_count_valid(document->first_canonical_byte,
                           document->canonical_byte_count,
                           program->canonical_byte_count)) {
      return false;
    }
    if (!add_u32(root_cursor, document->root_count, &root_cursor) ||
        !add_u32(node_cursor, document->node_count, &node_cursor) ||
        !add_u32(field_cursor, document->field_count, &field_cursor) ||
        !add_u32(edge_cursor, document->edge_count, &edge_cursor) ||
        !add_u32(canonical_cursor, document->canonical_byte_count,
                 &canonical_cursor) ||
        !add_size(aggregate, document->source.length, &aggregate))
      return false;
  }
  if (source_range_count == 0u || !ranges_pairwise_disjoint(source_ranges,
                                                              source_range_count) ||
      root_cursor != counts.roots || node_cursor != counts.nodes ||
      field_cursor != counts.fields || edge_cursor != counts.edges ||
      canonical_cursor != counts.canonical_bytes ||
      aggregate > result->limits.max_aggregate_bytes) {
    return false;
  }

  const uint64_t structural_total =
      (uint64_t)counts.roots + (uint64_t)counts.nodes +
      (uint64_t)counts.fields + (uint64_t)counts.edges;
  if (structural_total > UINT32_MAX ||
      counts.structural_nodes != (uint32_t)structural_total) {
    return false;
  }

  canonical_cursor = 0u;

  for (uint32_t index = 0u; index < counts.roots; index += 1u) {
    const w_seed_manifest_root *root = &program->roots[index];
    if (root->document_index >= counts.documents ||
        root->kind > W_SEED_MANIFEST_ROOT_WORKSPACE)
      return false;
    const w_seed_manifest_document *document =
        &program->documents[root->document_index];
    if (index < document->first_root ||
        index - document->first_root >= document->root_count ||
        root->ordinal >= document->root_count ||
        root->record_node < document->first_node ||
        root->record_node - document->first_node >= document->node_count ||
        !span_present_in_source(root->keyword_span, document->source.length) ||
        !span_present_in_source(root->source_span, document->source.length)) {
      return false;
    }
  }

  for (uint32_t index = 0u; index < counts.nodes; index += 1u) {
    const w_seed_manifest_node *node = &program->nodes[index];
    if (node->document_index >= counts.documents ||
        node->kind > W_SEED_MANIFEST_NODE_BOOL)
      return false;
    const w_seed_manifest_document *document =
        &program->documents[node->document_index];
    if (index < document->first_node ||
        index - document->first_node >= document->node_count ||
        !span_present_in_source(node->source_span, document->source.length)) {
      return false;
    }
    if (node->parent_node == W_SEED_MANIFEST_NONE) {
      if (node->kind != W_SEED_MANIFEST_NODE_RECORD ||
          node->source_ordinal >= document->root_count)
        return false;
      bool found_root = false;
      for (uint32_t root_index = document->first_root;
           root_index < document->first_root + document->root_count;
           root_index += 1u) {
        if (program->roots[root_index].record_node == index) {
          if (found_root ||
              program->roots[root_index].ordinal > document->root_count)
            return false;
          found_root = true;
        }
      }
      if (!found_root) return false;
    } else {
      if (node->parent_node >= counts.nodes || node->parent_node >= index)
        return false;
      const w_seed_manifest_node *parent = &program->nodes[node->parent_node];
      if (parent->document_index != node->document_index ||
          (parent->kind != W_SEED_MANIFEST_NODE_RECORD &&
           parent->kind != W_SEED_MANIFEST_NODE_LIST &&
           parent->kind != W_SEED_MANIFEST_NODE_CONSTRUCTOR) ||
          node->source_ordinal >= parent->child_count)
        return false;
    }
    const bool has_name = node->kind == W_SEED_MANIFEST_NODE_MEMBER ||
                          node->kind == W_SEED_MANIFEST_NODE_CONSTRUCTOR;
    if (has_name) {
      if (!span_present_in_source(node->name_span, document->source.length))
        return false;
    } else if (!span_absent(node->name_span)) {
      return false;
    }
    const bool has_children = node->kind == W_SEED_MANIFEST_NODE_RECORD ||
                              node->kind == W_SEED_MANIFEST_NODE_LIST ||
                              node->kind == W_SEED_MANIFEST_NODE_CONSTRUCTOR;
    if (has_children) {
      const size_t total = node->kind == W_SEED_MANIFEST_NODE_RECORD
                               ? program->field_count
                               : program->edge_count;
      if (node->first_child == W_SEED_MANIFEST_NONE ||
          !range_count_valid(node->first_child, node->child_count, total) ||
          (node->kind == W_SEED_MANIFEST_NODE_CONSTRUCTOR &&
           node->child_count == 0u))
        return false;
    } else if (node->first_child != W_SEED_MANIFEST_NONE ||
               node->child_count != 0u) {
      return false;
    }
    const bool scalar = node->kind == W_SEED_MANIFEST_NODE_STRING ||
                        node->kind == W_SEED_MANIFEST_NODE_NUMBER ||
                        node->kind == W_SEED_MANIFEST_NODE_SIZE ||
                        node->kind == W_SEED_MANIFEST_NODE_QUANTITY;
    if (scalar) {
      if (node->canonical.offset == W_SEED_MANIFEST_NONE ||
          !range_count_valid(node->canonical.offset, node->canonical.length,
                             program->canonical_byte_count))
        return false;
    } else if (node->canonical.offset != W_SEED_MANIFEST_NONE ||
               node->canonical.length != 0u ||
               (node->kind != W_SEED_MANIFEST_NODE_BOOL &&
                node->boolean_value)) {
      return false;
    }
  }

  for (uint32_t index = 0u; index < counts.fields; index += 1u) {
    const w_seed_manifest_field *field = &program->fields[index];
    if (field->owner_record >= counts.nodes ||
        field->value_node >= counts.nodes) {
      return false;
    }
    const w_seed_manifest_node *owner = &program->nodes[field->owner_record];
    const w_seed_manifest_node *value = &program->nodes[field->value_node];
    if (owner->kind != W_SEED_MANIFEST_NODE_RECORD ||
        field->owner_record > field->value_node ||
        value->parent_node != field->owner_record ||
        value->document_index != owner->document_index ||
        index < owner->first_child ||
        index - owner->first_child >= owner->child_count ||
        field->ordinal != index - owner->first_child) {
      return false;
    }
    const w_seed_manifest_document *document =
        &program->documents[owner->document_index];
    if (!span_present_in_source(field->name_span, document->source.length) ||
        !span_present_in_source(field->source_span, document->source.length) ||
        field->source_span.start_byte > field->name_span.start_byte ||
        field->source_span.end_byte < field->name_span.end_byte) {
      return false;
    }
    if (index != owner->first_child) {
      const w_seed_manifest_field *prior = &program->fields[index - 1u];
      if (prior->owner_record == field->owner_record) {
        int comparison = 0;
        if (!source_span_compare(document->source, prior->name_span,
                                 field->name_span, &comparison) ||
            comparison >= 0) {
          return false;
        }
      }
    }
  }

  for (uint32_t index = 0u; index < counts.edges; index += 1u) {
    const w_seed_manifest_edge *edge = &program->edges[index];
    if (edge->owner_node >= counts.nodes || edge->value_node >= counts.nodes) {
      return false;
    }
    const w_seed_manifest_node *owner = &program->nodes[edge->owner_node];
    const w_seed_manifest_node *value = &program->nodes[edge->value_node];
    const bool list_owner = owner->kind == W_SEED_MANIFEST_NODE_LIST;
    const bool constructor_owner =
        owner->kind == W_SEED_MANIFEST_NODE_CONSTRUCTOR;
    if ((!list_owner && !constructor_owner) ||
        edge->kind != (list_owner ? W_SEED_MANIFEST_EDGE_LIST_ITEM
                                  : W_SEED_MANIFEST_EDGE_CONSTRUCTOR_ARGUMENT) ||
        edge->owner_node > edge->value_node ||
        value->parent_node != edge->owner_node ||
        value->document_index != owner->document_index ||
        index < owner->first_child ||
        index - owner->first_child >= owner->child_count ||
        edge->ordinal != index - owner->first_child) {
      return false;
    }
    const w_seed_manifest_document *document =
        &program->documents[owner->document_index];
    if (!span_present_in_source(edge->source_span, document->source.length)) {
      return false;
    }
    if (list_owner) {
      if (edge->has_label || !span_absent(edge->label_span)) {
        return false;
      }
    } else if (edge->has_label) {
      if (!span_present_in_source(edge->label_span, document->source.length) ||
          edge->source_span.start_byte > edge->label_span.start_byte ||
          edge->source_span.end_byte < edge->label_span.end_byte) {
        return false;
      }
    } else if (!span_absent(edge->label_span)) {
      return false;
    }
  }

  for (uint32_t index = 0u; index < counts.nodes; index += 1u) {
    const w_seed_manifest_node *node = &program->nodes[index];
    const bool scalar = node->kind == W_SEED_MANIFEST_NODE_STRING ||
                        node->kind == W_SEED_MANIFEST_NODE_NUMBER ||
                        node->kind == W_SEED_MANIFEST_NODE_SIZE ||
                        node->kind == W_SEED_MANIFEST_NODE_QUANTITY;
    if (scalar) {
      if (node->canonical.offset != canonical_cursor ||
          !add_u32(canonical_cursor, node->canonical.length,
                   &canonical_cursor)) {
        return false;
      }
    }
  }
  if (canonical_cursor != counts.canonical_bytes) {
    return false;
  }
  return true;
}

static int compare_emit_fields(w_seed_byte_view source,
                               const w_seed_manifest_field *left,
                               const w_seed_manifest_field *right) {
  if (left->owner_record < right->owner_record) return -1;
  if (left->owner_record > right->owner_record) return 1;
  int comparison = 0;
  if (!source_span_compare(source, left->name_span, right->name_span,
                           &comparison))
    return 0;
  return comparison;
}

static int compare_emit_edges(const w_seed_manifest_edge *left,
                              const w_seed_manifest_edge *right) {
  if (left->owner_node < right->owner_node) return -1;
  if (left->owner_node > right->owner_node) return 1;
  if (left->ordinal < right->ordinal) return -1;
  if (left->ordinal > right->ordinal) return 1;
  return 0;
}

static bool normalize_document_arenas(
    w_seed_manifest_output *output, uint32_t first_node, uint32_t node_count,
    uint32_t first_field, uint32_t field_count, uint32_t first_edge,
    uint32_t edge_count, w_seed_byte_view source) {
  if (output == NULL) return false;
  uint32_t field_end = 0u;
  uint32_t edge_end = 0u;
  uint32_t node_end = 0u;
  if (!add_u32(first_field, field_count, &field_end) ||
      !add_u32(first_edge, edge_count, &edge_end) ||
      !add_u32(first_node, node_count, &node_end))
    return false;
  for (uint32_t offset = 1u; offset < field_count; offset += 1u) {
    w_seed_manifest_field item = output->fields[first_field + offset];
    uint32_t position = offset;
    while (position != 0u &&
           compare_emit_fields(source,
                               &output->fields[first_field + position - 1u],
                               &item) > 0) {
      output->fields[first_field + position] =
          output->fields[first_field + position - 1u];
      position -= 1u;
    }
    output->fields[first_field + position] = item;
  }
  for (uint32_t offset = 1u; offset < edge_count; offset += 1u) {
    w_seed_manifest_edge item = output->edges[first_edge + offset];
    uint32_t position = offset;
    while (position != 0u &&
           compare_emit_edges(&output->edges[first_edge + position - 1u],
                              &item) > 0) {
      output->edges[first_edge + position] =
          output->edges[first_edge + position - 1u];
      position -= 1u;
    }
    output->edges[first_edge + position] = item;
  }

  uint32_t field_cursor = first_field;
  for (uint32_t node_index = first_node; node_index < node_end;
       node_index += 1u) {
    w_seed_manifest_node *node = &output->nodes[node_index];
    node->first_child = W_SEED_MANIFEST_NONE;
    node->child_count = 0u;
    if (node->kind != W_SEED_MANIFEST_NODE_RECORD) continue;
    node->first_child = field_cursor;
    while (field_cursor < field_end &&
           output->fields[field_cursor].owner_record == node_index) {
      if (node->child_count == UINT32_MAX) return false;
      output->fields[field_cursor].ordinal = node->child_count;
      node->child_count += 1u;
      field_cursor += 1u;
    }
    if (field_cursor < field_end &&
        output->fields[field_cursor].owner_record < node_index)
      return false;
  }
  if (field_cursor != field_end) return false;

  uint32_t edge_cursor = first_edge;
  for (uint32_t node_index = first_node; node_index < node_end;
       node_index += 1u) {
    w_seed_manifest_node *node = &output->nodes[node_index];
    if (node->kind != W_SEED_MANIFEST_NODE_LIST &&
        node->kind != W_SEED_MANIFEST_NODE_CONSTRUCTOR)
      continue;
    node->first_child = edge_cursor;
    while (edge_cursor < edge_end &&
           output->edges[edge_cursor].owner_node == node_index) {
      if (node->child_count == UINT32_MAX) return false;
      output->edges[edge_cursor].ordinal = node->child_count;
      node->child_count += 1u;
      edge_cursor += 1u;
    }
    if (edge_cursor < edge_end &&
        output->edges[edge_cursor].owner_node < node_index)
      return false;
  }
  return edge_cursor == edge_end;
}

static bool emit_batch(const w_seed_manifest_input *input,
                       w_seed_manifest_output *output,
                       w_seed_manifest_result *result,
                       w_seed_manifest_counts expected,
                       w_seed_manifest_counts *written) {
  if (input == NULL || output == NULL || result == NULL || written == NULL)
    return false;
  w_seed_manifest_counts counts;
  (void)memset(&counts, 0, sizeof(counts));
  (void)memset(input->scratch.name_slots, 0,
               input->limits.max_structural_nodes *
                   sizeof(*input->scratch.name_slots));
  uint64_t batch_work = 0u;
  for (size_t index = 0u; index < input->document_count; index += 1u) {
    const w_seed_manifest_source_input *source = &input->documents[index];
    const uint64_t source_work = (uint64_t)source->bytes.length;
    if (source_work > input->limits.max_work_units - batch_work) {
      *result = fail_result(*result, W_SEED_MANIFEST_LIMIT,
                            W_SEED_MANIFEST_PHASE_RUN,
                            W_SEED_MANIFEST_ERROR_WORK_LIMIT,
                            (uint32_t)index, 0u);
      return false;
    }
    batch_work += source_work;
    for (size_t byte = 0u; byte < source->bytes.length; byte += 1u) {
      if (batch_work >= input->limits.max_work_units) {
        *result = fail_result(*result, W_SEED_MANIFEST_LIMIT,
                              W_SEED_MANIFEST_PHASE_RUN,
                              W_SEED_MANIFEST_ERROR_WORK_LIMIT,
                              (uint32_t)index, byte);
        return false;
      }
      batch_work += 1u;
      if (source->bytes.data[byte] == 0u) {
        *result = fail_result(*result, W_SEED_MANIFEST_SYNTAX,
                              W_SEED_MANIFEST_PHASE_RUN,
                              W_SEED_MANIFEST_ERROR_INVALID_TOKEN,
                              (uint32_t)index, byte);
        return false;
      }
    }
    man0_parser parser;
    (void)memset(&parser, 0, sizeof(parser));
    parser.source = source;
    parser.bytes = source->bytes.data;
    parser.length = source->bytes.length;
    parser.document_index = (uint32_t)index;
    parser.scope_seed = ((uint64_t)index + 1u) << 32u;
    parser.limits = &input->limits;
    parser.scratch = input->scratch;
    parser.counts = &counts;
    parser.result = result;
    parser.phase = W_SEED_MANIFEST_PHASE_RUN;
    parser.mode = MAN0_MODE_EMIT;
    parser.output = output;
    parser.work = batch_work;
    const uint32_t first_root = counts.roots;
    const uint32_t first_node = counts.nodes;
    const uint32_t first_field = counts.fields;
    const uint32_t first_edge = counts.edges;
    const uint32_t first_canonical = counts.canonical_bytes;
    if (!parse_document(&parser) ||
        !normalize_document_arenas(
            output, first_node, counts.nodes - first_node, first_field,
            counts.fields - first_field, first_edge, counts.edges - first_edge,
            source->bytes))
      return false;
    batch_work = parser.work;
    if (!add_u32(counts.documents, 1u, &counts.documents)) {
      *result = fail_result(*result, W_SEED_MANIFEST_LIMIT,
                            W_SEED_MANIFEST_PHASE_RUN,
                            W_SEED_MANIFEST_ERROR_DOCUMENT_LIMIT,
                            (uint32_t)index, 0u);
      return false;
    }
    w_seed_manifest_document *document = &output->documents[index];
    (void)memset(document, 0, sizeof(*document));
    document->source = source->bytes;
    document->binding_kind = source->binding_kind;
    document->generation = source->generation;
    document->candidate = source->candidate;
    (void)memcpy(document->context_binding, source->context_binding,
                 sizeof(document->context_binding));
    (void)memcpy(document->candidate_binding, source->candidate_binding,
                 sizeof(document->candidate_binding));
    document->first_root = first_root;
    document->root_count = counts.roots - first_root;
    document->first_node = first_node;
    document->node_count = counts.nodes - first_node;
    document->first_field = first_field;
    document->field_count = counts.fields - first_field;
    document->first_edge = first_edge;
    document->edge_count = counts.edges - first_edge;
    document->first_canonical_byte = first_canonical;
    document->canonical_byte_count = counts.canonical_bytes - first_canonical;
    document->counts = (w_seed_manifest_counts){
        1u, document->root_count, document->node_count, document->field_count,
        document->edge_count, document->canonical_byte_count,
        document->root_count + document->node_count + document->field_count +
            document->edge_count};
    if (!source_digest(document->source, document->source_digest)) {
      *result = fail_result(*result, W_SEED_MANIFEST_FAULT,
                            W_SEED_MANIFEST_PHASE_RUN,
                            W_SEED_MANIFEST_ERROR_NONE,
                            (uint32_t)index, 0u);
      return false;
    }
  }
  if (!counts_equal(counts, expected)) return false;
  *written = counts;
  return true;
}

static bool publish_digests(w_seed_manifest_program *program,
                            w_seed_manifest_limits limits,
                            w_seed_manifest_counts counts,
                            w_seed_manifest_result *result) {
  if (program == NULL || result == NULL) return false;
  for (uint32_t index = 0u; index < counts.documents; index += 1u) {
    w_seed_manifest_document *document =
        (w_seed_manifest_document *)&program->documents[index];
    if (!document_semantic_digest(program, document, document->semantic_digest) ||
        !document_provenance_digest(document, index,
                                    document->provenance_digest) ||
        !document_receipt_digest(&limits, document, document->receipt_digest))
      return false;
  }
  return batch_digest(program, &limits, &counts, result->semantic_digest,
                      result->provenance_digest, result->receipt_digest);
}

static bool program_source_aliases(const w_seed_manifest_program *program);

w_seed_manifest_result w_seed_manifest_run(
    const w_seed_manifest_input *input, w_seed_manifest_output *output) {
  w_seed_manifest_counts measured_counts;
  (void)memset(&measured_counts, 0, sizeof(measured_counts));
  w_seed_manifest_result result =
      w_seed_manifest_measure(input, &measured_counts);
  if (result.status != W_SEED_MANIFEST_OK) return result;
  result.phase = W_SEED_MANIFEST_PHASE_RUN;
  (void)memset(&result.written, 0, sizeof(result.written));
  if (output == NULL ||
      !manifest_output_capacity_valid(output, measured_counts))
    return fail_result(result, W_SEED_MANIFEST_CAPACITY,
                       W_SEED_MANIFEST_PHASE_RUN,
                       W_SEED_MANIFEST_ERROR_NONE, W_SEED_MANIFEST_NONE,
                       W_SEED_MANIFEST_NO_BYTE);
  if (!manifest_output_self_valid(output) || output_aliases_input(input, output)) {
    result = fail_result(result, W_SEED_MANIFEST_ALIAS,
                         W_SEED_MANIFEST_PHASE_VALIDATE,
                         W_SEED_MANIFEST_ERROR_NONE, W_SEED_MANIFEST_NONE,
                         W_SEED_MANIFEST_NO_BYTE);
    (void)memset(&result.required, 0, sizeof(result.required));
    return result;
  }
  w_seed_manifest_counts written;
  (void)memset(&written, 0, sizeof(written));
  if (!emit_batch(input, output, &result, measured_counts, &written)) {
    if (result.status == W_SEED_MANIFEST_OK)
      result = fail_result(result, W_SEED_MANIFEST_FAULT,
                           W_SEED_MANIFEST_PHASE_RUN,
                           W_SEED_MANIFEST_ERROR_NONE,
                           W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NO_BYTE);
    (void)memset(&result.required, 0, sizeof(result.required));
    return result;
  }
  result.required = written;
  result.written = written;
  w_seed_manifest_program program;
  const bool bridge_ok =
      w_seed_manifest_program_from_output(output, &result, &program);
  const bool digest_ok = bridge_ok &&
      publish_digests(&program, result.limits, written, &result);
  if (!bridge_ok || !digest_ok) {
    (void)memset(&result.required, 0, sizeof(result.required));
    result = fail_result(result, W_SEED_MANIFEST_FAULT,
                         W_SEED_MANIFEST_PHASE_RUN,
                         W_SEED_MANIFEST_ERROR_NONE,
                         W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NO_BYTE);
    return result;
  }
  result.status = W_SEED_MANIFEST_OK;
  result.phase = W_SEED_MANIFEST_PHASE_RUN;
  return result;
}

static bool manifest_program_from_output_impl(
    const w_seed_manifest_output *output,
    const w_seed_manifest_result *result,
    w_seed_manifest_program *program, bool guarded) {
  if (output == NULL || result == NULL || program == NULL ||
      !result_ok(result, guarded) ||
      !manifest_output_capacity_valid(output, result->written) ||
      !manifest_output_self_valid(output)) {
    return false;
  }
  man0_range output_descriptor;
  man0_range result_descriptor;
  man0_range program_descriptor;
  man0_range output_ranges[8u];
  size_t output_range_count = 0u;
  if (!range_from(output, 1u, sizeof(*output), &output_descriptor) ||
      !range_from(result, 1u, sizeof(*result), &result_descriptor) ||
      !range_from(program, 1u, sizeof(*program), &program_descriptor) ||
      !manifest_output_ranges(output, output_ranges, &output_range_count))
    return false;
  if (ranges_overlap(output_descriptor, result_descriptor) ||
      ranges_overlap(output_descriptor, program_descriptor) ||
      ranges_overlap(result_descriptor, program_descriptor)) {
    return false;
  }
  for (size_t index = 0u; index < output_range_count; index += 1u)
    if (ranges_overlap(output_ranges[index], result_descriptor) ||
        ranges_overlap(output_ranges[index], program_descriptor)) {
      return false;
    }
  w_seed_manifest_program candidate = {
      output->documents, (size_t)result->written.documents,
      output->document_capacity, output->roots,
      (size_t)result->written.roots, output->root_capacity, output->nodes,
      (size_t)result->written.nodes, output->node_capacity, output->fields,
      (size_t)result->written.fields, output->field_capacity, output->edges,
      (size_t)result->written.edges, output->edge_capacity,
      output->canonical_bytes, (size_t)result->written.canonical_bytes,
      output->canonical_byte_capacity};
  if (!program_shape_valid(&candidate, result, guarded) ||
      program_source_aliases(&candidate)) {
    return false;
  }
  for (size_t index = 0u; index < candidate.document_count; index += 1u) {
    man0_range source;
    if (!range_from(candidate.documents[index].source.data,
                    candidate.documents[index].source.length, 1u, &source))
      return false;
    if (ranges_overlap(source, output_descriptor) ||
        ranges_overlap(source, result_descriptor) ||
        ranges_overlap(source, program_descriptor))
      return false;
    for (size_t backing = 0u; backing < output_range_count; backing += 1u)
      if (ranges_overlap(source, output_ranges[backing])) return false;
  }
  *program = candidate;
  return true;
}

bool w_seed_manifest_program_from_output(
    const w_seed_manifest_output *output,
    const w_seed_manifest_result *result,
    w_seed_manifest_program *program) {
  return manifest_program_from_output_impl(output, result, program, false);
}

static bool program_source_aliases(const w_seed_manifest_program *program) {
  if (program == NULL) return true;
  man0_range descriptor;
  man0_range backing[8u];
  size_t backing_count = 0u;
  if (!range_from(program, 1u, sizeof(*program), &descriptor) ||
      !program_ranges_table(program, backing, &backing_count))
    return true;
  for (size_t index = 0u; index < program->document_count; index += 1u) {
    man0_range source;
    if (!range_from(program->documents[index].source.data,
                    program->documents[index].source.length, 1u, &source))
      return true;
    if (ranges_overlap(descriptor, source)) return true;
    for (size_t backing_index = 0u; backing_index < backing_count;
         backing_index += 1u)
      if (ranges_overlap(backing[backing_index], source)) return true;
  }
  return false;
}

static bool verify_scratch_valid(const w_seed_manifest_scratch *scratch,
                                 w_seed_manifest_limits limits) {
  if (scratch == NULL ||
      scratch->name_slot_capacity < limits.max_structural_nodes ||
      scratch->byte_capacity <
          (size_t)limits.max_decoded_scalar_bytes +
              W_SEED_MANIFEST_SCALAR_SCRATCH_OVERHEAD ||
      (scratch->name_slot_capacity != 0u && scratch->name_slots == NULL) ||
      (scratch->byte_capacity != 0u && scratch->bytes == NULL))
    return false;
  man0_range ranges[2u];
  return range_from(scratch->name_slots, scratch->name_slot_capacity,
                    sizeof(*scratch->name_slots), &ranges[0]) &&
         range_from(scratch->bytes, scratch->byte_capacity, 1u, &ranges[1]) &&
         !ranges_overlap(ranges[0], ranges[1]);
}

static bool manifest_verify_impl(const w_seed_manifest_program *program,
                                 const w_seed_manifest_result *result,
                                 const w_seed_manifest_scratch *scratch,
                                 bool guarded) {
  if (!result_ok(result, guarded) ||
      !program_shape_valid(program, result, guarded) ||
      program_source_aliases(program) ||
      !verify_scratch_valid(scratch, result->limits)) {
    return false;
  }
  man0_range program_descriptor;
  man0_range result_descriptor;
  man0_range scratch_descriptor;
  man0_range program_ranges[8u];
  man0_range scratch_ranges[2u];
  size_t program_range_count = 0u;
  if (!range_from(program, 1u, sizeof(*program), &program_descriptor) ||
      !range_from(result, 1u, sizeof(*result), &result_descriptor) ||
      !range_from(scratch, 1u, sizeof(*scratch), &scratch_descriptor) ||
      !program_ranges_table(program, program_ranges, &program_range_count) ||
      !range_from(scratch->name_slots, scratch->name_slot_capacity,
                  sizeof(*scratch->name_slots), &scratch_ranges[0]) ||
      !range_from(scratch->bytes, scratch->byte_capacity, 1u,
                  &scratch_ranges[1]))
      return false;
  if (ranges_overlap(program_descriptor, result_descriptor) ||
      ranges_overlap(program_descriptor, scratch_descriptor) ||
      ranges_overlap(result_descriptor, scratch_descriptor))
      return false;
  for (size_t index = 0u; index < program_range_count; index += 1u)
    if (ranges_overlap(program_ranges[index], result_descriptor) ||
        ranges_overlap(program_ranges[index], scratch_descriptor))
      return false;
  for (size_t index = 0u; index < 2u; index += 1u) {
    if (ranges_overlap(scratch_ranges[index], scratch_descriptor) ||
        ranges_overlap(scratch_ranges[index], program_descriptor) ||
        ranges_overlap(scratch_ranges[index], result_descriptor))
      return false;
    for (size_t backing = 0u; backing < program_range_count; backing += 1u)
      if (ranges_overlap(scratch_ranges[index], program_ranges[backing])) {
        return false;
      }
  }
  for (size_t document_index = 0u; document_index < program->document_count;
       document_index += 1u) {
    man0_range source;
    if (!range_from(program->documents[document_index].source.data,
                    program->documents[document_index].source.length, 1u,
                    &source) ||
        ranges_overlap(source, result_descriptor) ||
        ranges_overlap(source, scratch_descriptor))
      return false;
    for (size_t scratch_index = 0u; scratch_index < 2u; scratch_index += 1u)
      if (ranges_overlap(source, scratch_ranges[scratch_index])) return false;
  }

  w_seed_manifest_counts parsed;
  (void)memset(&parsed, 0, sizeof(parsed));
  w_seed_manifest_result verify_result = result_baseline();
  verify_result.limits = result->limits;
  verify_result.phase = W_SEED_MANIFEST_PHASE_VERIFY;
  uint64_t work = 0u;
  for (uint32_t document_index = 0u;
       document_index < result->written.documents; document_index += 1u) {
    const w_seed_manifest_document *document =
        &program->documents[document_index];
    const uint64_t source_work = (uint64_t)document->source.length;
    if (source_work > result->limits.max_work_units - work) {
      return false;
    }
    work += source_work;
    for (size_t byte = 0u; byte < document->source.length; byte += 1u) {
      if (work >= result->limits.max_work_units) {
        return false;
      }
      work += 1u;
      if (document->source.data[byte] == 0u) {
        return false;
      }
    }
    (void)memset(scratch->name_slots, 0,
                 result->limits.max_structural_nodes *
                     sizeof(*scratch->name_slots));
    man0_parser parser;
    (void)memset(&parser, 0, sizeof(parser));
    parser.source = NULL;
    parser.bytes = document->source.data;
    parser.length = document->source.length;
    parser.document_index = document_index;
    parser.scope_seed = ((uint64_t)document_index + 1u) << 32u;
    parser.limits = &result->limits;
    parser.scratch = *scratch;
    parser.counts = &parsed;
    parser.result = &verify_result;
    parser.phase = W_SEED_MANIFEST_PHASE_VERIFY;
    parser.mode = MAN0_MODE_MEASURE;
    parser.work = work;
    parser.verify_program = program;
    parser.verify_document = document;
    if (!parse_document(&parser)) {
      return false;
    }
    parser.document_counts.documents = 1u;
    work = parser.work;
    if (!counts_equal(parser.document_counts, document->counts) ||
        parser.document_counts.documents != 1u)
      return false;
    parsed.documents += 1u;
  }
  if (!counts_equal(parsed, result->written))
    return false;
  uint8_t semantic[32];
  uint8_t provenance[32];
  uint8_t receipt[32];
  for (uint32_t index = 0u; index < result->written.documents; index += 1u) {
    const w_seed_manifest_document *document = &program->documents[index];
    if (!source_digest(document->source, semantic) ||
        memcmp(semantic, document->source_digest, sizeof(semantic)) != 0 ||
        !document_semantic_digest(program, document, semantic) ||
        memcmp(semantic, document->semantic_digest, sizeof(semantic)) != 0 ||
        !document_provenance_digest(document, index, provenance) ||
        memcmp(provenance, document->provenance_digest, sizeof(provenance)) != 0 ||
        !document_receipt_digest(&result->limits, document, receipt) ||
        memcmp(receipt, document->receipt_digest, sizeof(receipt)) != 0)
        return false;
  }
  if (!batch_digest(program, &result->limits, &result->written, semantic,
                    provenance, receipt) ||
      memcmp(semantic, result->semantic_digest, sizeof(semantic)) != 0 ||
      memcmp(provenance, result->provenance_digest, sizeof(provenance)) != 0 ||
      memcmp(receipt, result->receipt_digest, sizeof(receipt)) != 0)
    return false;
  return true;
}

bool w_seed_manifest_verify(const w_seed_manifest_program *program,
                            const w_seed_manifest_result *result,
                            const w_seed_manifest_scratch *scratch) {
  return manifest_verify_impl(program, result, scratch, false);
}

bool w_seed_manifest_guarded_verify(
    const w_seed_manifest_program *program,
    const w_seed_manifest_result *result,
    const w_seed_manifest_scratch *scratch) {
  return manifest_verify_impl(program, result, scratch, true);
}

static bool manifest_backend_status_valid(
    w_seed_manifest_backend_status status) {
  return (int)status >= (int)W_SEED_MANIFEST_BACKEND_OK &&
         (int)status <= (int)W_SEED_MANIFEST_BACKEND_FAULT;
}

static bool manifest_backend_phase_valid(w_seed_manifest_backend_phase phase) {
  return (int)phase >= (int)W_SEED_MANIFEST_BACKEND_PHASE_NONE &&
         (int)phase <= (int)W_SEED_MANIFEST_BACKEND_PHASE_CLOSE;
}

static bool candidate_ref_equal(w_seed_owner_guard_candidate_ref left,
                                w_seed_owner_guard_candidate_ref right) {
  return left.generation == right.generation &&
         left.directory_ordinal == right.directory_ordinal &&
         left.candidate_index == right.candidate_index;
}

static bool digest_bytes_equal(const uint8_t left[32],
                               const uint8_t right[32]) {
  return left != NULL && right != NULL && memcmp(left, right, 32u) == 0;
}

static bool backend_result_zero_digests(
    const w_seed_manifest_backend_result *result) {
  return result != NULL && result->byte_count == 0u &&
         bytes_zero(result->source_digest, sizeof(result->source_digest)) &&
         bytes_zero(result->context_binding,
                    sizeof(result->context_binding)) &&
         bytes_zero(result->candidate_binding,
                    sizeof(result->candidate_binding));
}

static bool backend_result_envelope_valid(
    const w_seed_manifest_backend_result *result, uint64_t generation,
    w_seed_owner_guard_candidate_ref candidate, size_t byte_capacity,
    size_t byte_limit) {
  if (result == NULL || !manifest_backend_status_valid(result->status) ||
      !manifest_backend_phase_valid(result->phase) ||
      result->status == W_SEED_MANIFEST_BACKEND_NOT_CALLED ||
      result->generation != generation ||
      !candidate_ref_equal(result->candidate, candidate))
    return false;
  switch (result->status) {
    case W_SEED_MANIFEST_BACKEND_OK:
      return result->phase == W_SEED_MANIFEST_BACKEND_PHASE_CLOSE &&
             result->byte_count == result->required_byte_capacity &&
             result->byte_count <= byte_capacity &&
             result->byte_count <= byte_limit;
    case W_SEED_MANIFEST_BACKEND_CAPACITY:
      return result->phase == W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_EOF &&
             backend_result_zero_digests(result) &&
             result->required_byte_capacity > byte_capacity &&
             result->required_byte_capacity <= byte_limit;
    case W_SEED_MANIFEST_BACKEND_LIMIT:
      return result->phase == W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_EOF &&
             backend_result_zero_digests(result) && byte_limit < SIZE_MAX &&
             result->required_byte_capacity == byte_limit + 1u;
    default:
      return result->phase != W_SEED_MANIFEST_BACKEND_PHASE_NONE &&
             backend_result_zero_digests(result) &&
             result->required_byte_capacity == 0u;
  }
}

static w_seed_manifest_status manifest_status_from_owner(
    w_seed_owner_guard_status status) {
  switch (status) {
    case W_SEED_OWNER_GUARD_INVALID:
      return W_SEED_MANIFEST_INVALID;
    case W_SEED_OWNER_GUARD_CAPACITY:
      return W_SEED_MANIFEST_CAPACITY;
    case W_SEED_OWNER_GUARD_MUTATED:
      return W_SEED_MANIFEST_MUTATED;
    case W_SEED_OWNER_GUARD_BOUNDARY:
      return W_SEED_MANIFEST_BOUNDARY;
    case W_SEED_OWNER_GUARD_REPARSE:
      return W_SEED_MANIFEST_REPARSE;
    case W_SEED_OWNER_GUARD_UNSUPPORTED:
      return W_SEED_MANIFEST_UNSUPPORTED;
    case W_SEED_OWNER_GUARD_IO:
      return W_SEED_MANIFEST_IO;
    case W_SEED_OWNER_GUARD_FAULT:
      return W_SEED_MANIFEST_FAULT;
    case W_SEED_OWNER_GUARD_OK:
      return W_SEED_MANIFEST_OK;
  }
  return W_SEED_MANIFEST_FAULT;
}

static w_seed_manifest_status manifest_status_from_backend(
    w_seed_manifest_backend_status status) {
  switch (status) {
    case W_SEED_MANIFEST_BACKEND_OK:
      return W_SEED_MANIFEST_OK;
    case W_SEED_MANIFEST_BACKEND_CAPACITY:
      return W_SEED_MANIFEST_CAPACITY;
    case W_SEED_MANIFEST_BACKEND_LIMIT:
      return W_SEED_MANIFEST_LIMIT;
    case W_SEED_MANIFEST_BACKEND_MUTATED:
      return W_SEED_MANIFEST_MUTATED;
    case W_SEED_MANIFEST_BACKEND_BOUNDARY:
      return W_SEED_MANIFEST_BOUNDARY;
    case W_SEED_MANIFEST_BACKEND_REPARSE:
      return W_SEED_MANIFEST_REPARSE;
    case W_SEED_MANIFEST_BACKEND_UNSUPPORTED:
      return W_SEED_MANIFEST_UNSUPPORTED;
    case W_SEED_MANIFEST_BACKEND_IO:
      return W_SEED_MANIFEST_IO;
    case W_SEED_MANIFEST_BACKEND_INVALID:
      return W_SEED_MANIFEST_INVALID;
    case W_SEED_MANIFEST_BACKEND_FAULT:
      return W_SEED_MANIFEST_FAULT;
    case W_SEED_MANIFEST_BACKEND_NOT_CALLED:
      return W_SEED_MANIFEST_FAULT;
  }
  return W_SEED_MANIFEST_FAULT;
}

static bool pointer_aligned_manifest(const void *pointer, size_t alignment) {
  return pointer != NULL && alignment != 0u &&
         (uintptr_t)pointer % (uintptr_t)alignment == (uintptr_t)0u;
}

static bool guarded_add_range(man0_range *ranges, size_t *count,
                             size_t capacity, const void *pointer,
                             size_t element_count, size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity ||
      !range_from(pointer, element_count, element_size, &ranges[*count]))
    return false;
  *count += 1u;
  return true;
}

static bool guarded_ranges_valid(const w_seed_manifest_guarded_input *input,
                                 const w_seed_owner_guard *guard,
                                 const w_seed_manifest_backend *backend,
                                 const w_seed_manifest_program *program,
                                 size_t candidate_count) {
#define MAN0_GUARDED_RANGE_CAPACITY \
  (W_SEED_MANIFEST_MAX_DOCUMENTS * 3u + 64u)
  man0_range ranges[MAN0_GUARDED_RANGE_CAPACITY];
  size_t count = 0u;
  if (!guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY, input,
                         1u, sizeof(*input)) ||
      !guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY, guard,
                         1u, sizeof(*guard)) ||
      !guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY, backend,
                         1u, sizeof(*backend)) ||
      !guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY, program,
                         1u, sizeof(*program)) ||
      !guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY,
                         guard->backend.context,
                         guard->backend_context_size, 1u) ||
      !guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY,
                         guard->storage.staged,
                         guard->storage.staged_capacity,
                         sizeof(*guard->storage.staged)) ||
      !guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY,
                         guard->storage.revalidation,
                         guard->storage.revalidation_capacity,
                         sizeof(*guard->storage.revalidation)) ||
      !guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY,
                         guard->storage.published_candidates,
                         guard->storage.published_candidate_capacity,
                         sizeof(*guard->storage.published_candidates)) ||
      !guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY,
                         input->storage.read_slots,
                         input->storage.read_slot_capacity,
                         sizeof(*input->storage.read_slots)) ||
      !guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY,
                         input->storage.staged_sources,
                         input->storage.staged_source_capacity,
                         sizeof(*input->storage.staged_sources)) ||
      !guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY,
                         input->storage.scratch.name_slots,
                         input->storage.scratch.name_slot_capacity,
                         sizeof(*input->storage.scratch.name_slots)) ||
      !guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY,
                         input->storage.scratch.bytes,
                         input->storage.scratch.byte_capacity, 1u))
    return false;

  man0_range staged_ranges[8u];
  man0_range published_ranges[8u];
  size_t staged_count = 0u;
  size_t published_count = 0u;
  if (!manifest_output_ranges(&input->storage.staged, staged_ranges,
                              &staged_count) ||
      !manifest_output_ranges(&input->storage.published, published_ranges,
                              &published_count))
    return false;
  for (size_t index = 0u; index < staged_count; index += 1u) {
    if (count >= MAN0_GUARDED_RANGE_CAPACITY) return false;
    ranges[count++] = staged_ranges[index];
  }
  for (size_t index = 0u; index < published_count; index += 1u) {
    if (count >= MAN0_GUARDED_RANGE_CAPACITY) return false;
    ranges[count++] = published_ranges[index];
  }

  for (size_t index = 0u; index < candidate_count; index += 1u) {
    const w_seed_manifest_read_slot *slot = &input->storage.read_slots[index];
    if (!guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY,
                           slot->first_bytes, slot->first_capacity, 1u) ||
        !guarded_add_range(ranges, &count, MAN0_GUARDED_RANGE_CAPACITY,
                           slot->second_bytes, slot->second_capacity, 1u))
      return false;
  }
  return ranges_pairwise_disjoint(ranges, count);
#undef MAN0_GUARDED_RANGE_CAPACITY
}

static bool guarded_fixed_valid(const w_seed_manifest_guarded_input *input,
                                const w_seed_owner_guard *guard,
                                const w_seed_manifest_backend *backend,
                                const w_seed_manifest_program *program) {
  man0_range fixed_ranges[4u];
  if (input == NULL || guard == NULL || backend == NULL || program == NULL ||
      !pointer_aligned_manifest(input, _Alignof(w_seed_manifest_guarded_input)) ||
      !pointer_aligned_manifest(guard, _Alignof(w_seed_owner_guard)) ||
      !pointer_aligned_manifest(backend, _Alignof(w_seed_manifest_backend)) ||
      !pointer_aligned_manifest(program, _Alignof(w_seed_manifest_program)) ||
      !range_from(input, 1u, sizeof(*input), &fixed_ranges[0]) ||
      !range_from(guard, 1u, sizeof(*guard), &fixed_ranges[1]) ||
      !range_from(backend, 1u, sizeof(*backend), &fixed_ranges[2]) ||
      !range_from(program, 1u, sizeof(*program), &fixed_ranges[3]) ||
      !ranges_pairwise_disjoint(fixed_ranges, 4u) ||
      !limits_valid(input->limits) || guard->owner != guard ||
      backend->owner != backend || backend->guard != guard ||
      backend->context == NULL ||
      backend->context != guard->backend.context ||
      backend->context_size != guard->backend_context_size ||
      backend->generation != guard->generation ||
      backend->read_candidate == NULL ||
      guard->lifecycle != W_SEED_OWNER_GUARD_LIVE_OBSERVED ||
      guard->disposition != W_SEED_OWNER_GUARD_CANDIDATES_OBSERVED ||
      !guard->session_live || guard->candidate_count == 0u ||
      guard->candidate_count > W_SEED_MANIFEST_MAX_DOCUMENTS ||
      guard->candidate_count > input->limits.max_documents ||
      input->storage.read_slot_capacity < guard->candidate_count ||
      input->storage.staged_source_capacity < guard->candidate_count ||
      input->storage.read_slots == NULL ||
      input->storage.staged_sources == NULL ||
      !pointer_aligned_manifest(input->storage.scratch.name_slots,
                                _Alignof(w_seed_manifest_name_slot)) ||
      !pointer_aligned_manifest(input->storage.staged_sources,
                                _Alignof(w_seed_manifest_source_input)) ||
      !pointer_aligned_manifest(input->storage.read_slots,
                                _Alignof(w_seed_manifest_read_slot)))
    return false;
  return true;
}

static bool guarded_stale_binding(const w_seed_manifest_guarded_input *input,
                                 const w_seed_owner_guard *guard,
                                 const w_seed_manifest_backend *backend) {
  return input != NULL && guard != NULL && backend != NULL &&
         pointer_aligned_manifest(input, _Alignof(w_seed_manifest_guarded_input)) &&
         pointer_aligned_manifest(guard, _Alignof(w_seed_owner_guard)) &&
         pointer_aligned_manifest(backend, _Alignof(w_seed_manifest_backend)) &&
         (guard->owner != guard || backend->owner != backend ||
          backend->guard != guard ||
          backend->context != guard->backend.context ||
          backend->context_size != guard->backend_context_size ||
          backend->generation != guard->generation);
}

static bool guarded_root_envelopes_valid(
    const w_seed_manifest_guarded_input *input,
    const w_seed_owner_guard *guard,
    const w_seed_manifest_backend *backend,
    const w_seed_manifest_program *program) {
  man0_range ranges[4u];
  if (input == NULL || guard == NULL || backend == NULL || program == NULL ||
      !pointer_aligned_manifest(input, _Alignof(w_seed_manifest_guarded_input)) ||
      !pointer_aligned_manifest(guard, _Alignof(w_seed_owner_guard)) ||
      !pointer_aligned_manifest(backend, _Alignof(w_seed_manifest_backend)) ||
      !pointer_aligned_manifest(program, _Alignof(w_seed_manifest_program)) ||
      !range_from(input, 1u, sizeof(*input), &ranges[0]) ||
      !range_from(guard, 1u, sizeof(*guard), &ranges[1]) ||
      !range_from(backend, 1u, sizeof(*backend), &ranges[2]) ||
      !range_from(program, 1u, sizeof(*program), &ranges[3]))
    return false;
  return ranges_pairwise_disjoint(ranges, 4u);
}

static bool guarded_output_alignment_valid(
    const w_seed_manifest_output *output) {
  return output != NULL &&
         (output->document_capacity == 0u ||
          pointer_aligned_manifest(output->documents,
                                   _Alignof(w_seed_manifest_document))) &&
         (output->root_capacity == 0u ||
          pointer_aligned_manifest(output->roots,
                                   _Alignof(w_seed_manifest_root))) &&
         (output->node_capacity == 0u ||
          pointer_aligned_manifest(output->nodes,
                                   _Alignof(w_seed_manifest_node))) &&
         (output->field_capacity == 0u ||
          pointer_aligned_manifest(output->fields,
                                   _Alignof(w_seed_manifest_field))) &&
         (output->edge_capacity == 0u ||
          pointer_aligned_manifest(output->edges,
                                   _Alignof(w_seed_manifest_edge))) &&
         (output->canonical_byte_capacity == 0u ||
          pointer_aligned_manifest(output->canonical_bytes,
                                   _Alignof(uint8_t)));
}

static bool guarded_live_valid(const w_seed_manifest_guarded_input *input,
                               const w_seed_owner_guard *guard,
                               w_seed_owner_guard_view *view) {
  if (input == NULL || guard == NULL || view == NULL ||
      !verify_scratch_valid(&input->storage.scratch, input->limits) ||
      !guarded_output_alignment_valid(&input->storage.staged) ||
      !guarded_output_alignment_valid(&input->storage.published) ||
      !manifest_output_self_valid(&input->storage.staged) ||
      !manifest_output_self_valid(&input->storage.published) ||
      !w_seed_owner_guard_get_view(guard, view))
    return false;
  if (view->lifecycle != W_SEED_OWNER_GUARD_LIVE_OBSERVED ||
      view->disposition != W_SEED_OWNER_GUARD_CANDIDATES_OBSERVED ||
      view->generation != guard->generation ||
      view->candidate_count != guard->candidate_count ||
      view->candidates == NULL ||
      view->candidate_count > input->limits.max_documents)
    return false;
  for (size_t index = 0u; index < view->candidate_count; index += 1u) {
    const w_seed_owner_guard_candidate_ref *candidate =
        &view->candidates[index];
    if (candidate->generation != view->generation ||
        candidate->candidate_index != index ||
        candidate->directory_ordinal > UINT32_MAX)
      return false;
    const w_seed_manifest_read_slot *slot = &input->storage.read_slots[index];
    if (slot->first_capacity == 0u ||
        slot->first_capacity > input->limits.max_document_bytes ||
        (slot->first_bytes == NULL && slot->first_capacity != 0u))
      return false;
  }
  return true;
}

static void guarded_record_backend(w_seed_manifest_result *result,
                                   const w_seed_manifest_backend_result *backend,
                                   uint32_t candidate_index) {
  if (result == NULL || backend == NULL) return;
  result->backend_status = backend->status;
  result->backend_phase = backend->phase;
  result->candidate_index = candidate_index;
}

static void guarded_record_owner(w_seed_manifest_result *result,
                                 const w_seed_owner_guard_result *owner) {
  if (result == NULL || owner == NULL) return;
  result->owner_guard_status = owner->status;
}

static w_seed_manifest_result guarded_failure(
    w_seed_manifest_result result, w_seed_manifest_status status,
    w_seed_manifest_phase phase, w_seed_manifest_error_kind error,
    uint32_t document_index, uint32_t candidate_index, size_t byte_offset,
    bool clear_required) {
  result = fail_result(result, status, phase, error, document_index,
                       byte_offset);
  result.candidate_index = candidate_index;
  if (clear_required) (void)memset(&result.required, 0, sizeof(result.required));
  return result;
}

static bool guarded_copy_source_input(
    w_seed_manifest_source_input *destination,
    const w_seed_manifest_read_slot *slot,
    const w_seed_owner_guard_candidate_ref *candidate,
    const w_seed_manifest_backend_result *backend, bool second) {
  if (destination == NULL || slot == NULL || candidate == NULL ||
      backend == NULL || backend->status != W_SEED_MANIFEST_BACKEND_OK)
    return false;
  (void)memset(destination, 0, sizeof(*destination));
  destination->bytes.data = second ? slot->second_bytes : slot->first_bytes;
  destination->bytes.length = backend->byte_count;
  destination->binding_kind = W_SEED_MANIFEST_BINDING_OWNER_GUARD;
  destination->generation = backend->generation;
  destination->candidate = *candidate;
  (void)memcpy(destination->context_binding, backend->context_binding,
               sizeof(destination->context_binding));
  (void)memcpy(destination->candidate_binding, backend->candidate_binding,
               sizeof(destination->candidate_binding));
  return true;
}

static void guarded_set_first_digest(
    uint8_t (*digests)[W_SEED_MANIFEST_DIGEST_BYTES], size_t index,
    const uint8_t digest[W_SEED_MANIFEST_DIGEST_BYTES]) {
  (void)memcpy(digests[index], digest, W_SEED_MANIFEST_DIGEST_BYTES);
}

static bool guarded_second_capacity_valid(
    const w_seed_manifest_guarded_input *input,
    const w_seed_manifest_counts *counts) {
  if (input == NULL || counts == NULL ||
      counts->documents > input->storage.read_slot_capacity)
    return false;
  for (uint32_t index = 0u; index < counts->documents; index += 1u) {
    const w_seed_manifest_read_slot *slot = &input->storage.read_slots[index];
    const size_t length = input->storage.staged_sources[index].bytes.length;
    if (length > input->limits.max_document_bytes ||
        slot->second_capacity < length ||
        (length != 0u && slot->second_bytes == NULL) ||
        slot->second_capacity > input->limits.max_document_bytes)
      return false;
  }
  return true;
}

static bool guarded_output_capacity_valid(
    const w_seed_manifest_guarded_input *input,
    w_seed_manifest_counts counts) {
  return input != NULL &&
         manifest_output_capacity_valid(&input->storage.staged, counts) &&
         manifest_output_capacity_valid(&input->storage.published, counts);
}

w_seed_manifest_result w_seed_manifest_guarded_run(
    w_seed_manifest_guarded_input *input,
    w_seed_manifest_program *program) {
  w_seed_manifest_result result = result_baseline();
  if (input == NULL || program == NULL ||
      !pointer_aligned_manifest(input,
                                _Alignof(w_seed_manifest_guarded_input)) ||
      !pointer_aligned_manifest(program,
                                _Alignof(w_seed_manifest_program)))
    return result;
  man0_range input_range;
  man0_range program_range;
  if (!range_from(input, 1u, sizeof(*input), &input_range) ||
      !range_from(program, 1u, sizeof(*program), &program_range) ||
      ranges_overlap(input_range, program_range))
    return result;
  if (!limits_valid(input->limits)) return result;
  result.limits = input->limits;

  const w_seed_owner_guard *guard = input->guard;
  const w_seed_manifest_backend *backend = input->backend;
  w_seed_owner_guard_view view;
  (void)memset(&view, 0, sizeof(view));
  if (!guarded_root_envelopes_valid(input, guard, backend, program)) {
    return guarded_failure(result, W_SEED_MANIFEST_INVALID,
                           W_SEED_MANIFEST_PHASE_VALIDATE,
                           W_SEED_MANIFEST_ERROR_NONE,
                           W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NONE,
                           W_SEED_MANIFEST_NO_BYTE, true);
  }
  if (guarded_stale_binding(input, guard, backend)) {
    return guarded_failure(result, W_SEED_MANIFEST_STALE,
                           W_SEED_MANIFEST_PHASE_VALIDATE,
                           W_SEED_MANIFEST_ERROR_NONE,
                           W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NONE,
                           W_SEED_MANIFEST_NO_BYTE, true);
  }
  if (!guarded_fixed_valid(input, guard, backend, program)) {
    return guarded_failure(result, W_SEED_MANIFEST_INVALID,
                           W_SEED_MANIFEST_PHASE_VALIDATE,
                           W_SEED_MANIFEST_ERROR_NONE,
                           W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NONE,
                           W_SEED_MANIFEST_NO_BYTE, true);
  }
  if (!guarded_ranges_valid(input, guard, backend, program,
                            guard->candidate_count)) {
    return guarded_failure(result, W_SEED_MANIFEST_ALIAS,
                           W_SEED_MANIFEST_PHASE_VALIDATE,
                           W_SEED_MANIFEST_ERROR_NONE,
                           W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NONE,
                           W_SEED_MANIFEST_NO_BYTE, true);
  }
  if (!guarded_live_valid(input, guard, &view)) {
    return guarded_failure(result, W_SEED_MANIFEST_INVALID,
                           W_SEED_MANIFEST_PHASE_VALIDATE,
                           W_SEED_MANIFEST_ERROR_NONE,
                           W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NONE,
                           W_SEED_MANIFEST_NO_BYTE, true);
  }

  w_seed_owner_guard_candidate_ref candidates[W_SEED_MANIFEST_MAX_DOCUMENTS];
  (void)memset(candidates, 0, sizeof(candidates));
  for (size_t index = 0u; index < view.candidate_count; index += 1u)
    candidates[index] = view.candidates[index];

  uint8_t first_backend_digest[W_SEED_MANIFEST_MAX_DOCUMENTS][32];
  uint8_t first_context_binding[W_SEED_MANIFEST_MAX_DOCUMENTS][32];
  uint8_t first_candidate_binding[W_SEED_MANIFEST_MAX_DOCUMENTS][32];
  (void)memset(first_backend_digest, 0, sizeof(first_backend_digest));
  (void)memset(first_context_binding, 0, sizeof(first_context_binding));
  (void)memset(first_candidate_binding, 0,
               sizeof(first_candidate_binding));

  for (size_t index = 0u; index < view.candidate_count; index += 1u) {
    const w_seed_manifest_read_slot *slot = &input->storage.read_slots[index];
    const w_seed_manifest_backend_result backend_result = input->backend->read_candidate(
        input->backend->context, view.generation, candidates[index],
        slot->first_bytes, slot->first_capacity,
        input->limits.max_document_bytes);
    if (!backend_result_envelope_valid(
            &backend_result, view.generation, candidates[index], slot->first_capacity,
            input->limits.max_document_bytes)) {
      result.backend_status = W_SEED_MANIFEST_BACKEND_FAULT;
      result.backend_phase = W_SEED_MANIFEST_BACKEND_PHASE_VALIDATE;
      return guarded_failure(result, W_SEED_MANIFEST_FAULT,
                             W_SEED_MANIFEST_PHASE_READ_FIRST,
                             W_SEED_MANIFEST_ERROR_NONE, (uint32_t)index,
                             (uint32_t)index, W_SEED_MANIFEST_NO_BYTE, true);
    }
    guarded_record_backend(&result, &backend_result, (uint32_t)index);
    if (backend_result.status != W_SEED_MANIFEST_BACKEND_OK) {
      const w_seed_manifest_status status =
          manifest_status_from_backend(backend_result.status);
      const w_seed_manifest_error_kind error =
          backend_result.status == W_SEED_MANIFEST_BACKEND_LIMIT
              ? W_SEED_MANIFEST_ERROR_SOURCE_TOO_LARGE
              : W_SEED_MANIFEST_ERROR_NONE;
      result.required_byte_capacity = backend_result.required_byte_capacity;
      return guarded_failure(result, status, W_SEED_MANIFEST_PHASE_READ_FIRST,
                             error, (uint32_t)index, (uint32_t)index,
                             W_SEED_MANIFEST_NO_BYTE, true);
    }
    uint8_t core_digest[32];
    if (!source_digest(
            (w_seed_byte_view){slot->first_bytes, backend_result.byte_count},
            core_digest) ||
        !digest_bytes_equal(core_digest, backend_result.source_digest)) {
      return guarded_failure(result, W_SEED_MANIFEST_MUTATED,
                             W_SEED_MANIFEST_PHASE_READ_FIRST,
                             W_SEED_MANIFEST_ERROR_NONE, (uint32_t)index,
                             (uint32_t)index, W_SEED_MANIFEST_NO_BYTE, true);
    }
    guarded_set_first_digest(first_backend_digest, index,
                             backend_result.source_digest);
    (void)memcpy(first_context_binding[index], backend_result.context_binding, 32u);
    (void)memcpy(first_candidate_binding[index], backend_result.candidate_binding,
                 32u);
    if (!guarded_copy_source_input(
            &input->storage.staged_sources[index], slot, &candidates[index],
            &backend_result, false))
      return guarded_failure(result, W_SEED_MANIFEST_FAULT,
                             W_SEED_MANIFEST_PHASE_READ_FIRST,
                             W_SEED_MANIFEST_ERROR_NONE, (uint32_t)index,
                             (uint32_t)index, W_SEED_MANIFEST_NO_BYTE, true);
  }

  w_seed_manifest_input first_input = {
      input->storage.staged_sources, view.candidate_count, input->limits,
      input->storage.scratch};
  w_seed_manifest_counts measured;
  (void)memset(&measured, 0, sizeof(measured));
  w_seed_manifest_result measure_result =
      manifest_measure_impl(&first_input, &measured, true);
  if (measure_result.status != W_SEED_MANIFEST_OK) {
    measure_result.backend_status = result.backend_status;
    measure_result.backend_phase = result.backend_phase;
    measure_result.candidate_index = result.candidate_index;
    return measure_result;
  }
  result.required = measured;
  result.phase = W_SEED_MANIFEST_PHASE_MEASURE;
  if (measured.documents != view.candidate_count ||
      !guarded_second_capacity_valid(input, &measured) ||
      !guarded_output_capacity_valid(input, measured)) {
    return guarded_failure(result, W_SEED_MANIFEST_CAPACITY,
                           W_SEED_MANIFEST_PHASE_MEASURE,
                           W_SEED_MANIFEST_ERROR_NONE,
                           W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NONE,
                           W_SEED_MANIFEST_NO_BYTE, false);
  }

  w_seed_owner_guard_result owner_result;
  (void)memset(&owner_result, 0, sizeof(owner_result));
  result.owner_guard_revalidate_called = true;
  const w_seed_owner_guard_status owner_status =
      w_seed_owner_guard_revalidate(input->guard, &owner_result);
  guarded_record_owner(&result, &owner_result);
  if (owner_status != W_SEED_OWNER_GUARD_OK) {
    return guarded_failure(result, manifest_status_from_owner(owner_status),
                           W_SEED_MANIFEST_PHASE_REVALIDATE_OWNER_GUARD,
                           W_SEED_MANIFEST_ERROR_NONE,
                           W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NONE,
                           W_SEED_MANIFEST_NO_BYTE, true);
  }
  w_seed_owner_guard_view reconfirmed;
  (void)memset(&reconfirmed, 0, sizeof(reconfirmed));
  if (!w_seed_owner_guard_get_view(input->guard, &reconfirmed) ||
      reconfirmed.lifecycle != W_SEED_OWNER_GUARD_LIVE_RECONFIRMED ||
      reconfirmed.disposition != W_SEED_OWNER_GUARD_CANDIDATES_RECONFIRMED ||
      reconfirmed.generation != view.generation ||
      reconfirmed.candidate_count != view.candidate_count ||
      reconfirmed.candidates == NULL) {
    return guarded_failure(result, W_SEED_MANIFEST_STALE,
                           W_SEED_MANIFEST_PHASE_REVALIDATE_OWNER_GUARD,
                           W_SEED_MANIFEST_ERROR_NONE,
                           W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NONE,
                           W_SEED_MANIFEST_NO_BYTE, true);
  }
  for (size_t index = 0u; index < reconfirmed.candidate_count; index += 1u)
    if (!candidate_ref_equal(reconfirmed.candidates[index], candidates[index]))
      return guarded_failure(result, W_SEED_MANIFEST_STALE,
                             W_SEED_MANIFEST_PHASE_REVALIDATE_OWNER_GUARD,
                             W_SEED_MANIFEST_ERROR_NONE,
                             W_SEED_MANIFEST_NONE, (uint32_t)index,
                             W_SEED_MANIFEST_NO_BYTE, true);

  for (size_t index = 0u; index < reconfirmed.candidate_count; index += 1u) {
    const w_seed_manifest_read_slot *slot = &input->storage.read_slots[index];
    const w_seed_manifest_backend_result backend_result = input->backend->read_candidate(
        input->backend->context, reconfirmed.generation, candidates[index],
        slot->second_bytes, slot->second_capacity,
        input->limits.max_document_bytes);
    if (!backend_result_envelope_valid(
            &backend_result, reconfirmed.generation, candidates[index],
            slot->second_capacity, input->limits.max_document_bytes)) {
      result.backend_status = W_SEED_MANIFEST_BACKEND_FAULT;
      result.backend_phase = W_SEED_MANIFEST_BACKEND_PHASE_VALIDATE;
      return guarded_failure(result, W_SEED_MANIFEST_FAULT,
                             W_SEED_MANIFEST_PHASE_READ_SECOND,
                             W_SEED_MANIFEST_ERROR_NONE, (uint32_t)index,
                             (uint32_t)index, W_SEED_MANIFEST_NO_BYTE, true);
    }
    guarded_record_backend(&result, &backend_result, (uint32_t)index);
    if (backend_result.status != W_SEED_MANIFEST_BACKEND_OK) {
      const w_seed_manifest_status status =
          backend_result.status == W_SEED_MANIFEST_BACKEND_LIMIT ||
                  backend_result.status == W_SEED_MANIFEST_BACKEND_CAPACITY
              ? W_SEED_MANIFEST_MUTATED
              : manifest_status_from_backend(backend_result.status);
      return guarded_failure(result, status, W_SEED_MANIFEST_PHASE_READ_SECOND,
                             W_SEED_MANIFEST_ERROR_NONE, (uint32_t)index,
                             (uint32_t)index, W_SEED_MANIFEST_NO_BYTE, true);
    }
    uint8_t core_digest[32];
    if (!source_digest(
            (w_seed_byte_view){slot->second_bytes, backend_result.byte_count},
            core_digest))
      return guarded_failure(result, W_SEED_MANIFEST_FAULT,
                             W_SEED_MANIFEST_PHASE_READ_SECOND,
                             W_SEED_MANIFEST_ERROR_NONE, (uint32_t)index,
                             (uint32_t)index, W_SEED_MANIFEST_NO_BYTE, true);
    const size_t first_length = input->storage.staged_sources[index].bytes.length;
    size_t mismatch = W_SEED_MANIFEST_NO_BYTE;
    if (backend_result.byte_count != first_length)
      mismatch = backend_result.byte_count < first_length ? backend_result.byte_count
                                                   : first_length;
    else if (first_length != 0u &&
             memcmp(slot->first_bytes, slot->second_bytes, first_length) != 0) {
      for (size_t offset = 0u; offset < first_length; offset += 1u)
        if (slot->first_bytes[offset] != slot->second_bytes[offset]) {
          mismatch = offset;
          break;
        }
    }
    if (mismatch != W_SEED_MANIFEST_NO_BYTE ||
        !digest_bytes_equal(core_digest, first_backend_digest[index]) ||
        !digest_bytes_equal(backend_result.source_digest,
                            first_backend_digest[index]) ||
        !digest_bytes_equal(backend_result.context_binding,
                            first_context_binding[index]) ||
        !digest_bytes_equal(backend_result.candidate_binding,
                            first_candidate_binding[index])) {
      return guarded_failure(result, W_SEED_MANIFEST_MUTATED,
                             W_SEED_MANIFEST_PHASE_COMPARE_WAVES,
                             W_SEED_MANIFEST_ERROR_NONE, (uint32_t)index,
                             (uint32_t)index, mismatch, true);
    }
    if (!guarded_copy_source_input(
            &input->storage.staged_sources[index], slot, &candidates[index],
            &backend_result, true))
      return guarded_failure(result, W_SEED_MANIFEST_FAULT,
                             W_SEED_MANIFEST_PHASE_READ_SECOND,
                             W_SEED_MANIFEST_ERROR_NONE, (uint32_t)index,
                             (uint32_t)index, W_SEED_MANIFEST_NO_BYTE, true);
  }

  result.status = W_SEED_MANIFEST_OK;
  result.phase = W_SEED_MANIFEST_PHASE_COMMIT;
  result.error = W_SEED_MANIFEST_ERROR_NONE;
  result.document_index = W_SEED_MANIFEST_NONE;
  result.candidate_index = W_SEED_MANIFEST_NONE;
  result.byte_offset = W_SEED_MANIFEST_NO_BYTE;
  result.backend_status = W_SEED_MANIFEST_BACKEND_OK;
  result.backend_phase = W_SEED_MANIFEST_BACKEND_PHASE_CLOSE;
  result.required = measured;
  result.written = measured;
  if (!emit_batch(&first_input, &input->storage.staged, &result, measured,
                  &result.written)) {
    return guarded_failure(result, W_SEED_MANIFEST_FAULT,
                           W_SEED_MANIFEST_PHASE_RUN,
                           W_SEED_MANIFEST_ERROR_NONE,
                           W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NONE,
                           W_SEED_MANIFEST_NO_BYTE, true);
  }
  w_seed_manifest_program staged_program;
  if (!manifest_program_from_output_impl(
          &input->storage.staged, &result, &staged_program, true) ||
      !publish_digests(&staged_program, result.limits, measured, &result) ||
      !manifest_verify_impl(&staged_program, &result,
                            &input->storage.scratch, true)) {
    return guarded_failure(result, W_SEED_MANIFEST_FAULT,
                           W_SEED_MANIFEST_PHASE_VERIFY,
                           W_SEED_MANIFEST_ERROR_NONE,
                           W_SEED_MANIFEST_NONE, W_SEED_MANIFEST_NONE,
                           W_SEED_MANIFEST_NO_BYTE, true);
  }

  (void)memcpy(input->storage.published.documents,
               input->storage.staged.documents,
               (size_t)measured.documents * sizeof(*input->storage.staged.documents));
  (void)memcpy(input->storage.published.roots, input->storage.staged.roots,
               (size_t)measured.roots * sizeof(*input->storage.staged.roots));
  (void)memcpy(input->storage.published.nodes, input->storage.staged.nodes,
               (size_t)measured.nodes * sizeof(*input->storage.staged.nodes));
  (void)memcpy(input->storage.published.fields, input->storage.staged.fields,
               (size_t)measured.fields * sizeof(*input->storage.staged.fields));
  (void)memcpy(input->storage.published.edges, input->storage.staged.edges,
               (size_t)measured.edges * sizeof(*input->storage.staged.edges));
  (void)memcpy(input->storage.published.canonical_bytes,
               input->storage.staged.canonical_bytes,
               (size_t)measured.canonical_bytes);
  w_seed_manifest_program committed = {
      input->storage.published.documents, measured.documents,
      input->storage.published.document_capacity, input->storage.published.roots,
      measured.roots, input->storage.published.root_capacity,
      input->storage.published.nodes, measured.nodes,
      input->storage.published.node_capacity, input->storage.published.fields,
      measured.fields, input->storage.published.field_capacity,
      input->storage.published.edges, measured.edges,
      input->storage.published.edge_capacity,
      input->storage.published.canonical_bytes, measured.canonical_bytes,
      input->storage.published.canonical_byte_capacity};
  *program = committed;
  result.status = W_SEED_MANIFEST_OK;
  result.phase = W_SEED_MANIFEST_PHASE_COMMIT;
  return result;
}
