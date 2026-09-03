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

typedef struct {
  const char *key;
  w_seed_frontend_diagnostic_fact_kind kind;
  const char *text;
  int64_t integer_value;
  const char *items[3];
  size_t item_count;
} frontend_matrix_fact_spec;

typedef struct {
  const char *role;
  size_t document_index;
  w_seed_span span;
} frontend_matrix_label_spec;

typedef struct {
  const char *code;
  const char *phase;
  size_t fact_count;
  frontend_matrix_fact_spec facts[5];
  size_t label_count;
  frontend_matrix_label_spec labels[2];
} frontend_matrix_case;

#define MATRIX_STRING(key_value, text_value)                                   \
  {key_value, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING, text_value, 0,           \
   {NULL, NULL, NULL}, 0u}
#define MATRIX_INTEGER(key_value, integer_value)                               \
  {key_value, W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER, NULL, integer_value,   \
   {NULL, NULL, NULL}, 0u}
#define MATRIX_ARRAY(key_value, first, second, third, count_value)             \
  {key_value, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY, NULL, 0,           \
   {first, second, third}, count_value}
#define MATRIX_SET(key_value, first, second, third, count_value)               \
  {key_value, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET, NULL, 0,             \
   {first, second, third}, count_value}
#define MATRIX_LABEL(role_value, document_value, start_value, end_value)       \
  {role_value, document_value, {start_value, end_value}}

static const frontend_matrix_case frontend_matrix_cases[] = {
    {"W-SEM-0001", "semantic.type", 2u,
     {MATRIX_STRING("actual", "actual-value"),
      MATRIX_STRING("expected", "Expected"), {0}, {0}, {0}},
     0u, {{0}}},
    {"W-TYPE-0120", "semantic.type", 2u,
     {MATRIX_STRING("leftType", "i32"), MATRIX_STRING("rightType", "i64"),
      {0}, {0}, {0}},
     2u, {MATRIX_LABEL("branch-result", 0u, 0u, 1u),
          MATRIX_LABEL("branch-result", 1u, 1u, 3u)}},
    {"W-TYPE-0121", "semantic.type", 4u,
     {MATRIX_STRING("actualCase", "closed"),
      MATRIX_SET("allowedCases", "closed", "open", NULL, 2u),
      MATRIX_STRING("baseEnum", "State"),
      MATRIX_STRING("expectedType", "State"),
      {0}},
     1u, {MATRIX_LABEL("expected-type", 1u, 1u, 3u), {0}}},
    {"W-TYPE-0122", "semantic.type", 4u,
     {MATRIX_STRING("actualType", "i64"),
      MATRIX_SET("candidateRoutes", NULL, NULL, NULL, 0u),
      MATRIX_STRING("expectedType", "u64"),
      MATRIX_STRING("reason", "no-implicit-route"),
      {0}},
     2u, {MATRIX_LABEL("call-owner", 0u, 0u, 1u),
          MATRIX_LABEL("expected-type", 1u, 1u, 3u)}},
    {"W-LABEL-0005", "semantic.type", 3u,
     {MATRIX_ARRAY("acceptedForms", "positional", "named:value", NULL, 2u),
      MATRIX_STRING("declaration", "render"), MATRIX_STRING("label", "value"),
      {0}, {0}},
     0u, {{0}}},
    {"W-LABEL-0006", "semantic.type", 3u,
     {MATRIX_STRING("declaration", "render"), MATRIX_STRING("label", "value"),
      MATRIX_STRING("slot", "value"), {0}, {0}},
     0u, {{0}}},
    {"W-MATCH-0001", "semantic.flow", 2u,
     {MATRIX_SET("missingCases", "closed", "open", NULL, 2u),
      MATRIX_STRING("subjectType", "State"), {0}, {0}, {0}},
     1u, {MATRIX_LABEL("match-subject", 0u, 0u, 1u), {0}}},
    {"W-MATCH-0002", "semantic.flow", 3u,
     {MATRIX_STRING("coveredBy", "default"), MATRIX_STRING("pattern", "open"),
      MATRIX_STRING("subjectType", "State"), {0}, {0}},
     2u, {MATRIX_LABEL("covered-case", 0u, 0u, 1u),
          MATRIX_LABEL("match-subject", 1u, 1u, 3u)}},
    {"W-MATCH-0003", "semantic.type", 3u,
     {MATRIX_STRING("context", "switch"), MATRIX_STRING("expectedType", "State"),
      MATRIX_STRING("member", "open"), {0}, {0}},
     0u, {{0}}},
    {"W-CONST-0001", "semantic.const", 4u,
     {MATRIX_ARRAY("callChain", "outer", "inner", NULL, 2u),
      MATRIX_STRING("operation", "compare"),
      MATRIX_STRING("reason", "not-const-safe"),
      MATRIX_STRING("symbol", "inner"),
      {0}},
     1u, {MATRIX_LABEL("const-owner", 1u, 1u, 3u), {0}}},
    {"W-CONTRACT-0001", "semantic.type", 3u,
     {MATRIX_SET("availableSlots", "columns", "rows", NULL, 2u),
      MATRIX_STRING("head", "Matrix"), MATRIX_STRING("slot", "depth"),
      {0}, {0}},
     1u, {MATRIX_LABEL("contract-head", 0u, 0u, 1u), {0}}},
    {"W-CONTRACT-0002", "semantic.type", 4u,
     {MATRIX_STRING("actualKind", "value:i64"),
      MATRIX_STRING("expectedKind", "value:u64"), MATRIX_STRING("head", "Matrix"),
      MATRIX_STRING("slot", "rows"), {0}},
     2u, {MATRIX_LABEL("contract-head", 0u, 0u, 1u),
          MATRIX_LABEL("slot-declaration", 1u, 1u, 3u)}},
    {"W-CONTRACT-0003", "semantic.type", 3u,
     {MATRIX_STRING("expectedType", "Bool"), MATRIX_STRING("head", "Matrix"),
      MATRIX_STRING("predicateType", "i64"), {0}, {0}},
     2u, {MATRIX_LABEL("contract-head", 0u, 0u, 1u),
          MATRIX_LABEL("slot-declaration", 1u, 1u, 3u)}},
    {"W-CONTRACT-0004", "semantic.type", 4u,
     {MATRIX_STRING("head", "Matrix"), MATRIX_STRING("slot", "rows"),
      MATRIX_ARRAY("slotOrder", "Element", "rows", "columns", 3u),
      MATRIX_STRING("violation", "duplicate"), {0}},
     2u, {MATRIX_LABEL("contract-head", 0u, 0u, 1u),
          MATRIX_LABEL("slot-declaration", 1u, 1u, 3u)}},
    {"W-GENERIC-0001", "semantic.type", 3u,
     {MATRIX_STRING("domain", "Unknown"), MATRIX_STRING("parameter", "T"),
      MATRIX_STRING("resolutionReason", "unresolved-domain"), {0}, {0}},
     1u, {MATRIX_LABEL("generic-parameter", 1u, 1u, 3u), {0}}},
    {"W-GENERIC-0002", "semantic.type", 4u,
     {MATRIX_SET("candidates", NULL, NULL, NULL, 0u),
      MATRIX_SET("equationSources", NULL, NULL, NULL, 0u),
      MATRIX_STRING("parameter", "T"),
      MATRIX_STRING("reason", "missing-required-argument"),
      {0}},
     2u, {MATRIX_LABEL("call-owner", 0u, 0u, 1u),
          MATRIX_LABEL("generic-parameter", 1u, 1u, 3u)}},
    {"W-GENERIC-0003", "semantic.type", 5u,
     {MATRIX_STRING("externalLabel", "value"),
      MATRIX_STRING("kind", "value:i64"), MATRIX_STRING("parameter", "T"),
      MATRIX_INTEGER("position", 0), MATRIX_STRING("reason", "extra-argument")},
     1u, {MATRIX_LABEL("generic-parameter", 1u, 1u, 3u), {0}}},
};

_Static_assert(sizeof(frontend_matrix_cases) / sizeof(frontend_matrix_cases[0]) ==
                   17u,
               "frontend adapter matrix must cover all active profiles");

#undef MATRIX_STRING
#undef MATRIX_INTEGER
#undef MATRIX_ARRAY
#undef MATRIX_SET
#undef MATRIX_LABEL

static bool matrix_append_bytes(char *output, size_t capacity, size_t *length,
                                const char *bytes, size_t byte_count) {
  if (output == NULL || length == NULL || bytes == NULL ||
      byte_count > capacity - (*length <= capacity ? *length : capacity)) {
    return false;
  }
  (void)memcpy(output + *length, bytes, byte_count);
  *length += byte_count;
  return true;
}

static bool matrix_append_literal(char *output, size_t capacity, size_t *length,
                                  const char *literal) {
  return matrix_append_bytes(output, capacity, length, literal,
                             strlen(literal));
}

static bool matrix_append_decimal(char *output, size_t capacity, size_t *length,
                                  uint64_t value) {
  char digits[32];
  size_t digit_count = 0u;
  do {
    digits[digit_count] = (char)('0' + (value % UINT64_C(10)));
    value /= UINT64_C(10);
    digit_count += 1u;
  } while (value != 0u);
  while (digit_count != 0u) {
    digit_count -= 1u;
    if (!matrix_append_bytes(output, capacity, length, &digits[digit_count],
                             1u))
      return false;
  }
  return true;
}

static bool matrix_append_json_text(char *output, size_t capacity,
                                    size_t *length, const char *text,
                                    size_t text_length) {
  if (!matrix_append_literal(output, capacity, length, "\"")) return false;
  for (size_t index = 0u; index < text_length; index += 1u) {
    const char byte = text[index];
    if (byte == '\"' || byte == '\\') {
      if (!matrix_append_bytes(output, capacity, length, "\\", 1u) ||
          !matrix_append_bytes(output, capacity, length, &byte, 1u))
        return false;
    } else {
      if (!matrix_append_bytes(output, capacity, length, &byte, 1u))
        return false;
    }
  }
  return matrix_append_literal(output, capacity, length, "\"");
}

static bool frontend_matrix_expected_json(
    const frontend_matrix_case *spec,
    const w_seed_frontend_diagnostic_fact *facts,
    const w_seed_frontend_diagnostic_item *items, size_t item_count,
    const w_seed_frontend_diagnostic_label *labels, char *expected,
    size_t expected_capacity, size_t *expected_length) {
  if (spec == NULL || facts == NULL || expected == NULL ||
      expected_length == NULL)
    return false;
  size_t length = 0u;
  if (!matrix_append_literal(expected, expected_capacity, &length,
                             "{\"schemaVersion\":1,\"instance\":\"D900000\",\"code\":"))
    return false;
  if (!matrix_append_json_text(expected, expected_capacity, &length, spec->code,
                               strlen(spec->code)) ||
      !matrix_append_literal(expected, expected_capacity, &length,
                             ",\"phase\":") ||
      !matrix_append_json_text(expected, expected_capacity, &length, spec->phase,
                               strlen(spec->phase)) ||
      !matrix_append_literal(
          expected, expected_capacity, &length,
          ",\"severity\":\"error\",\"primary\":{\"source\":\"semantic\",\"startByte\":2,\"endByte\":3},\"labels\":["))
    return false;
  for (size_t index = 0u; index < spec->label_count; index += 1u) {
    if (index != 0u &&
        !matrix_append_literal(expected, expected_capacity, &length, ","))
      return false;
    const w_seed_frontend_diagnostic_label *label = &labels[index];
    if (!matrix_append_literal(expected, expected_capacity, &length,
                               "{\"role\":") ||
        !matrix_append_json_text(expected, expected_capacity, &length,
                                 label->role.data, label->role.length) ||
        !matrix_append_literal(expected, expected_capacity, &length,
                               ",\"span\":{\"source\":") ||
        !matrix_append_json_text(expected, expected_capacity, &length,
                                 label->document_index == 0u ? "semantic"
                                                              : "semantic/doc-one",
                                 label->document_index == 0u ? 8u : 16u) ||
        !matrix_append_literal(expected, expected_capacity, &length,
                               ",\"startByte\":") ||
        !matrix_append_decimal(expected, expected_capacity, &length,
                               label->span.start_byte) ||
        !matrix_append_literal(expected, expected_capacity, &length,
                               ",\"endByte\":") ||
        !matrix_append_decimal(expected, expected_capacity, &length,
                               label->span.end_byte) ||
        !matrix_append_literal(expected, expected_capacity, &length, "}}"))
      return false;
  }
  if (!matrix_append_literal(expected, expected_capacity, &length,
                             "],\"facts\":{"))
    return false;
  size_t ignored_items = 0u;
  for (size_t index = 0u; index < spec->fact_count; index += 1u) {
    if (index != 0u &&
        !matrix_append_literal(expected, expected_capacity, &length, ","))
      return false;
    const w_seed_frontend_diagnostic_fact *fact = &facts[index];
    if (!matrix_append_json_text(expected, expected_capacity, &length,
                                 fact->key.data, fact->key.length) ||
        !matrix_append_literal(expected, expected_capacity, &length, ":"))
      return false;
    if (fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER) {
      if (!matrix_append_decimal(expected, expected_capacity, &length,
                                 (uint64_t)fact->integer_value))
        return false;
    } else if (fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING) {
      if (!matrix_append_json_text(expected, expected_capacity, &length,
                                   fact->text.data, fact->text.length))
        return false;
    } else {
      if (!matrix_append_literal(expected, expected_capacity, &length, "["))
        return false;
      for (size_t item = 0u; item < fact->item_count; item += 1u) {
        if (item != 0u &&
            !matrix_append_literal(expected, expected_capacity, &length, ","))
          return false;
        const w_seed_frontend_diagnostic_item *value =
            &items[(size_t)fact->first_item + item];
        if (!matrix_append_json_text(expected, expected_capacity, &length,
                                     value->text.data, value->text.length))
          return false;
        ignored_items += 1u;
      }
      if (!matrix_append_literal(expected, expected_capacity, &length, "]"))
        return false;
    }
  }
  (void)item_count;
  (void)ignored_items;
  if (!matrix_append_literal(expected, expected_capacity, &length,
                             "},\"notes\":[],\"fixes\":[],\"root\":null}"))
    return false;
  *expected_length = length;
  return true;
}

static bool frontend_matrix_case_passes(size_t case_index) {
  const frontend_matrix_case *spec = &frontend_matrix_cases[case_index];
  static const uint8_t source_a[] = {'x', '\n', '1', '\n', 'A', ' ', 'B', '\n'};
  static const uint8_t source_b[] = {'p', 'r', 'e', ' ', 0xc3u, 0xa9u, ' ',
                                     's', 'u', 'f', 'f', 'i', 'x', '\n'};
  static const w_seed_frontend_text source_ids[2] = {
      {"semantic", 8u}, {"semantic/doc-one", 16u}};
  w_seed_source sources[2];
  w_seed_source_error source_error;
  if (!w_seed_source_init((w_seed_byte_view){source_a, sizeof(source_a)},
                          &sources[0], &source_error) ||
      !w_seed_source_init((w_seed_byte_view){source_b, sizeof(source_b)},
                          &sources[1], &source_error))
    return false;
  w_seed_frontend_diagnostic diagnostics[1] = {0};
  w_seed_frontend_diagnostic_fact facts[5] = {0};
  w_seed_frontend_diagnostic_item items[16] = {0};
  w_seed_frontend_diagnostic_label labels[2] = {0};
  size_t item_cursor = 0u;
  for (size_t index = 0u; index < spec->fact_count; index += 1u) {
    const frontend_matrix_fact_spec *input = &spec->facts[index];
    facts[index].key = (w_seed_frontend_text){input->key, strlen(input->key)};
    facts[index].kind = input->kind;
    facts[index].first_item = W_SEED_FRONTEND_NONE;
    facts[index].item_count = 0u;
    if (input->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING) {
      facts[index].text = (w_seed_frontend_text){input->text,
                                                 strlen(input->text)};
    } else if (input->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER) {
      facts[index].integer_value = input->integer_value;
    } else {
      facts[index].item_count = (uint32_t)input->item_count;
      if (input->item_count != 0u) {
        facts[index].first_item = (uint32_t)item_cursor;
        for (size_t item = 0u; item < input->item_count; item += 1u) {
          items[item_cursor + item].text =
              (w_seed_frontend_text){input->items[item],
                                     strlen(input->items[item])};
        }
      }
      item_cursor += input->item_count;
    }
  }
  for (size_t index = 0u; index < spec->label_count; index += 1u) {
    const frontend_matrix_label_spec *input = &spec->labels[index];
    labels[index].role = (w_seed_frontend_text){input->role, strlen(input->role)};
    labels[index].span = input->span;
    labels[index].document_index = input->document_index;
  }
  diagnostics[0] = (w_seed_frontend_diagnostic){
      {spec->code, strlen(spec->code)}, {2u, 3u}, 0u, 0u,
      (uint32_t)spec->fact_count, 0u, (uint32_t)spec->label_count};
  w_seed_frontend_output frontend = {0};
  frontend.diagnostics = diagnostics;
  frontend.diagnostic_capacity = 1u;
  frontend.diagnostic_facts = facts;
  frontend.diagnostic_fact_capacity = 5u;
  frontend.diagnostic_items = items;
  frontend.diagnostic_item_capacity = 16u;
  frontend.diagnostic_labels = labels;
  frontend.diagnostic_label_capacity = 2u;
  const w_seed_diagnostic_frontend_context context = {
      sources, source_ids, 2u, &frontend, 1u, spec->fact_count, item_cursor,
      spec->label_count};
  static const char instance[] = "D900000";
  uint8_t output[4096];
  uint8_t repeat[4096];
  w_seed_diagnostic_result result = {0};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
          W_SEED_DIAGNOSTIC_CAPACITY ||
      result.required_bytes == 0u || result.primary_byte != 2u ||
      result.written_bytes != 0u || result.required_bytes > sizeof(output))
    return false;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, output, sizeof(output),
          &result) != W_SEED_DIAGNOSTIC_OK)
    return false;
  char expected[4096];
  size_t expected_length = 0u;
  if (!frontend_matrix_expected_json(spec, facts, items, item_cursor, labels,
                                     expected, sizeof(expected),
                                     &expected_length) ||
      result.written_bytes != expected_length ||
      memcmp(output, expected, expected_length) != 0)
    return false;
  w_seed_diagnostic_result repeat_result = {0};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, repeat, sizeof(repeat),
          &repeat_result) != W_SEED_DIAGNOSTIC_OK ||
      repeat_result.written_bytes != result.written_bytes ||
      memcmp(output, repeat, result.written_bytes) != 0)
    return false;
  return true;
}

static bool frontend_adapter_invalid_cases_pass(void) {
  static const uint8_t source_a[] = {'x', '\n', '1', '\n', 'A', ' ', 'B', '\n'};
  static const uint8_t source_b[] = {'p', 'r', 'e', ' ', 0xc3u, 0xa9u, ' ',
                                     's', 'u', 'f', 'f', 'i', 'x', '\n'};
  w_seed_source sources[2];
  w_seed_source_error source_error;
  if (!w_seed_source_init((w_seed_byte_view){source_a, sizeof(source_a)},
                          &sources[0], &source_error) ||
      !w_seed_source_init((w_seed_byte_view){source_b, sizeof(source_b)},
                          &sources[1], &source_error))
    return false;
  w_seed_frontend_text source_ids[2] = {{"semantic", 8u},
                                        {"semantic/doc-one", 16u}};
  w_seed_frontend_diagnostic diagnostics[1] = {{
      {"W-MATCH-0002", sizeof("W-MATCH-0002") - 1u}, {2u, 3u}, 0u, 0u, 3u,
      0u, 2u}};
  w_seed_frontend_diagnostic_fact facts[5] = {0};
  facts[0] = (w_seed_frontend_diagnostic_fact){
      {"coveredBy", 9u}, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
      {"default", 7u}, 0, W_SEED_FRONTEND_NONE, 0u};
  facts[1] = (w_seed_frontend_diagnostic_fact){
      {"pattern", 7u}, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING, {"open", 4u},
      0, W_SEED_FRONTEND_NONE, 0u};
  facts[2] = (w_seed_frontend_diagnostic_fact){
      {"subjectType", 11u}, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
      {"State", 5u}, 0, W_SEED_FRONTEND_NONE, 0u};
  w_seed_frontend_diagnostic_label labels[2] = {
      {{"covered-case", 12u}, {0u, 1u}, 0u},
      {{"match-subject", 13u}, {1u, 3u}, 1u}};
  w_seed_frontend_output frontend = {0};
  frontend.diagnostics = diagnostics;
  frontend.diagnostic_capacity = 1u;
  frontend.diagnostic_facts = facts;
  frontend.diagnostic_fact_capacity = 5u;
  frontend.diagnostic_labels = labels;
  frontend.diagnostic_label_capacity = 2u;
  w_seed_diagnostic_frontend_context context = {
      sources, source_ids, 2u, &frontend, 1u, 3u, 0u, 2u};
  static const char instance[] = "D900001";
  w_seed_diagnostic_result result = {0};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_CAPACITY)
    return false;

  source_ids[1] = source_ids[0];
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  source_ids[1] = (w_seed_frontend_text){"semantic/doc-one", 16u};
  source_ids[1] = (w_seed_frontend_text){"\xc3", 1u};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  source_ids[1] = (w_seed_frontend_text){NULL, 0u};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  source_ids[1] = (w_seed_frontend_text){"semantic/doc-one", 16u};

  const w_seed_frontend_diagnostic_label saved_label = labels[0];
  labels[0] = labels[1];
  labels[1] = saved_label;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  labels[0] = saved_label;
  labels[1] = (w_seed_frontend_diagnostic_label){{"match-subject", 13u},
                                                 {1u, 3u}, 1u};

  context.diagnostic_fact_count = 2u;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  context.diagnostic_fact_count = 3u;
  context.diagnostic_count = 0u;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  context.diagnostic_count = 1u;

  facts[0].integer_value = 1;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  facts[0].integer_value = 0;
  facts[0].first_item = 0u;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  facts[0].first_item = W_SEED_FRONTEND_NONE;

  w_seed_frontend_diagnostic array_diagnostic = {
      {"W-CONST-0001", sizeof("W-CONST-0001") - 1u}, {2u, 3u}, 0u, 0u, 4u,
      0u, 1u};
  w_seed_frontend_diagnostic_fact array_facts[4] = {
      {{"callChain", 9u}, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY,
       {NULL, 0u}, 0, 0u, 2u},
      {{"operation", 9u}, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
       {"compare", 7u}, 0, W_SEED_FRONTEND_NONE, 0u},
      {{"reason", 6u}, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
       {"not-const-safe", 14u}, 0, W_SEED_FRONTEND_NONE, 0u},
      {{"symbol", 6u}, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
       {"inner", 5u}, 0, W_SEED_FRONTEND_NONE, 0u}};
  w_seed_frontend_diagnostic_item array_items[2] = {
      {{"outer", 5u}}, {{"inner", 5u}}};
  w_seed_frontend_diagnostic_label array_labels[1] = {
      {{"const-owner", 11u}, {1u, 3u}, 1u}};
  frontend.diagnostics = &array_diagnostic;
  frontend.diagnostic_facts = array_facts;
  frontend.diagnostic_items = array_items;
  frontend.diagnostic_item_capacity = 2u;
  frontend.diagnostic_labels = array_labels;
  frontend.diagnostic_label_capacity = 1u;
  context.diagnostic_count = 1u;
  context.diagnostic_fact_count = 4u;
  context.diagnostic_item_count = 2u;
  context.diagnostic_label_count = 1u;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_CAPACITY)
    return false;
  array_items[0].text = (w_seed_frontend_text){"\xc3", 1u};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  array_items[0].text = (w_seed_frontend_text){"outer", 5u};
  array_facts[0].text = (w_seed_frontend_text){"hidden", 6u};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  array_facts[0].text = (w_seed_frontend_text){NULL, 0u};
  array_facts[0].integer_value = 1;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;

  w_seed_frontend_diagnostic integer_diagnostic = {
      {"W-GENERIC-0003", sizeof("W-GENERIC-0003") - 1u}, {2u, 3u}, 0u, 0u,
      5u, 0u, 1u};
  w_seed_frontend_diagnostic_fact integer_facts[5] = {
      {{"externalLabel", 13u}, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
       {"value", 5u}, 0, W_SEED_FRONTEND_NONE, 0u},
      {{"kind", 4u}, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
       {"value:i64", 9u}, 0, W_SEED_FRONTEND_NONE, 0u},
      {{"parameter", 9u}, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
       {"T", 1u}, 0, W_SEED_FRONTEND_NONE, 0u},
      {{"position", 8u}, W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER,
       {NULL, 0u}, 0, W_SEED_FRONTEND_NONE, 0u},
      {{"reason", 6u}, W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
       {"extra-argument", 14u}, 0, W_SEED_FRONTEND_NONE, 0u}};
  w_seed_frontend_diagnostic_label integer_labels[1] = {
      {{"generic-parameter", 17u}, {1u, 3u}, 1u}};
  frontend.diagnostics = &integer_diagnostic;
  frontend.diagnostic_facts = integer_facts;
  frontend.diagnostic_items = NULL;
  frontend.diagnostic_item_capacity = 0u;
  frontend.diagnostic_labels = integer_labels;
  frontend.diagnostic_label_capacity = 1u;
  context.diagnostic_fact_count = 5u;
  context.diagnostic_item_count = 0u;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_CAPACITY)
    return false;
  integer_facts[3].text = (w_seed_frontend_text){"hidden", 6u};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  integer_facts[3].text = (w_seed_frontend_text){NULL, 0u};
  integer_facts[3].first_item = 0u;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  integer_facts[3].first_item = W_SEED_FRONTEND_NONE;
  integer_facts[3].item_count = 1u;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &context, 0u, NULL, 0u, &result) !=
      W_SEED_DIAGNOSTIC_INVALID)
    return false;
  return true;
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

  static const uint8_t frontend_bytes_a[] = {'x', '\n', '1', '\n', 'A',
                                              ' ', 'B', '\n'};
  static const uint8_t frontend_bytes_b[] = {'p', 'r', 'e', ' ', 0xc3u,
                                              0xa9u, ' ', 's', 'u', 'f', 'f',
                                              'i', 'x', '\n'};
  w_seed_source frontend_sources[2];
  w_seed_source_error frontend_source_error;
  if (!w_seed_source_init((w_seed_byte_view){frontend_bytes_a,
                                             sizeof(frontend_bytes_a)},
                          &frontend_sources[0], &frontend_source_error) ||
      !w_seed_source_init((w_seed_byte_view){frontend_bytes_b,
                                             sizeof(frontend_bytes_b)},
                          &frontend_sources[1], &frontend_source_error)) {
    return 30;
  }
  static const w_seed_frontend_text frontend_source_ids[2] = {
      {"semantic", sizeof("semantic") - 1u},
      {"semantic/doc-one", sizeof("semantic/doc-one") - 1u},
  };
  w_seed_frontend_diagnostic diagnostics[1];
  w_seed_frontend_diagnostic_fact diagnostic_facts[5];
  w_seed_frontend_diagnostic_item diagnostic_items[8];
  w_seed_frontend_diagnostic_label diagnostic_labels[2];
  w_seed_frontend_output frontend_output;
  (void)memset(&frontend_output, 0, sizeof(frontend_output));
  frontend_output.diagnostics = diagnostics;
  frontend_output.diagnostic_capacity = 1u;
  frontend_output.diagnostic_facts = diagnostic_facts;
  frontend_output.diagnostic_fact_capacity =
      sizeof(diagnostic_facts) / sizeof(diagnostic_facts[0]);
  frontend_output.diagnostic_items = diagnostic_items;
  frontend_output.diagnostic_item_capacity =
      sizeof(diagnostic_items) / sizeof(diagnostic_items[0]);
  frontend_output.diagnostic_labels = diagnostic_labels;
  frontend_output.diagnostic_label_capacity =
      sizeof(diagnostic_labels) / sizeof(diagnostic_labels[0]);
  w_seed_diagnostic_frontend_context frontend_context = {
      frontend_sources, frontend_source_ids, 2u, &frontend_output, 1u, 5u,
      8u, 2u};
  const w_seed_frontend_text actual_text = {"1", 1u};
  const w_seed_frontend_text bool_text = {"Bool", 4u};
  diagnostic_facts[0] = (w_seed_frontend_diagnostic_fact){
      .key = {"actual", 6u},
      .kind = W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
      .text = actual_text,
      .integer_value = 0,
      .first_item = W_SEED_FRONTEND_NONE,
      .item_count = 0u};
  diagnostic_facts[1] = (w_seed_frontend_diagnostic_fact){
      .key = {"expected", 8u},
      .kind = W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
      .text = bool_text,
      .integer_value = 0,
      .first_item = W_SEED_FRONTEND_NONE,
      .item_count = 0u};
  diagnostics[0] = (w_seed_frontend_diagnostic){
      {"W-SEM-0001", sizeof("W-SEM-0001") - 1u}, {2u, 3u}, 0u, 0u, 2u,
      0u, 0u};
  static const char expected_frontend[] =
      "{\"schemaVersion\":1,\"instance\":\"D123456\","
      "\"code\":\"W-SEM-0001\",\"phase\":\"semantic.type\","
      "\"severity\":\"error\",\"primary\":{\"source\":\"semantic\","
      "\"startByte\":2,\"endByte\":3},\"labels\":[],\"facts\":{"
      "\"actual\":\"1\",\"expected\":\"Bool\"},\"notes\":[],"
      "\"fixes\":[],\"root\":null}";
  w_seed_diagnostic_result frontend_result;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) !=
          W_SEED_DIAGNOSTIC_CAPACITY ||
      frontend_result.required_bytes != sizeof(expected_frontend) - 1u ||
      frontend_result.primary_byte != 2u) {
    return 31;
  }
  (void)memset(output, 0xA5, sizeof(output));
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, output,
          frontend_result.required_bytes - 1u, &frontend_result) !=
          W_SEED_DIAGNOSTIC_CAPACITY ||
      frontend_result.written_bytes != 0u ||
      !all_bytes_equal(output, sizeof(output), 0xA5)) {
    return 32;
  }
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, output,
          sizeof(output), &frontend_result) !=
          W_SEED_DIAGNOSTIC_OK ||
      frontend_result.written_bytes != sizeof(expected_frontend) - 1u ||
      memcmp(output, expected_frontend, sizeof(expected_frontend) - 1u) != 0) {
    return 33;
  }
  const w_seed_source saved_unreferenced_source = frontend_sources[1];
  frontend_sources[1] = (w_seed_source){0};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) != W_SEED_DIAGNOSTIC_INVALID) {
    return 51;
  }
  frontend_sources[1] = saved_unreferenced_source;

  /* The adapter rejects malformed facts before measuring or touching output. */
  w_seed_frontend_diagnostic saved_diagnostic = diagnostics[0];
  diagnostics[0].fact_count = 1u;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 34;
  }
  diagnostics[0] = saved_diagnostic;
  const w_seed_frontend_diagnostic_fact saved_actual = diagnostic_facts[0];
  diagnostic_facts[0].key = (w_seed_frontend_text){"wrong", 5u};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 35;
  }
  diagnostic_facts[0] = saved_actual;
  diagnostic_facts[0].kind = W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 36;
  }
  diagnostic_facts[0] = saved_actual;
  diagnostic_facts[0].text = (w_seed_frontend_text){NULL, 0u};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 45;
  }
  diagnostic_facts[0] = saved_actual;
  diagnostics[0].document_index = 2u;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 37;
  }
  diagnostics[0] = saved_diagnostic;
  diagnostics[0].primary = (w_seed_span){3u, 2u};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 38;
  }
  diagnostics[0] = saved_diagnostic;
  static const w_seed_frontend_diagnostic unknown_code = {
      {"W-UNKNOWN-0001", sizeof("W-UNKNOWN-0001") - 1u}, {2u, 3u}, 0u,
      0u, 2u, 0u, 0u};
  diagnostics[0] = unknown_code;
  (void)memset(output, 0xA5, sizeof(output));
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, output,
          sizeof(output), &frontend_result) !=
          W_SEED_DIAGNOSTIC_UNSUPPORTED ||
      frontend_result.written_bytes != 0u ||
      !all_bytes_equal(output, sizeof(output), 0xA5)) {
    return 39;
  }
  diagnostics[0] = saved_diagnostic;

  /* Exercise array, set, integer, and a cross-document label. */
  static const w_seed_frontend_text call_chain[2] = {{"outer", 5u},
                                                      {"inner", 5u}};
  diagnostic_items[0].text = call_chain[0];
  diagnostic_items[1].text = call_chain[1];
  diagnostic_facts[0] = (w_seed_frontend_diagnostic_fact){
      .key = {"callChain", 9u},
      .kind = W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY,
      .text = {NULL, 0u},
      .integer_value = 0,
      .first_item = 0u,
      .item_count = 2u};
  diagnostic_facts[1] = (w_seed_frontend_diagnostic_fact){
      .key = {"operation", 9u},
      .kind = W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
      .text = {"compare", 7u},
      .integer_value = 0,
      .first_item = W_SEED_FRONTEND_NONE,
      .item_count = 0u};
  diagnostic_facts[2] = (w_seed_frontend_diagnostic_fact){
      .key = {"reason", 6u},
      .kind = W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
      .text = {"not const-safe", 14u},
      .integer_value = 0,
      .first_item = W_SEED_FRONTEND_NONE,
      .item_count = 0u};
  diagnostic_facts[3] = (w_seed_frontend_diagnostic_fact){
      .key = {"symbol", 6u},
      .kind = W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
      .text = {"inner", 5u},
      .integer_value = 0,
      .first_item = W_SEED_FRONTEND_NONE,
      .item_count = 0u};
  diagnostic_labels[0] = (w_seed_frontend_diagnostic_label){
      {"const-owner", 11u}, {0u, 1u}, 1u};
  diagnostics[0] = (w_seed_frontend_diagnostic){
      {"W-CONST-0001", sizeof("W-CONST-0001") - 1u}, {2u, 3u}, 0u, 0u, 4u,
      0u, 1u};
  frontend_context.diagnostic_fact_count = 4u;
  frontend_context.diagnostic_item_count = 2u;
  frontend_context.diagnostic_label_count = 1u;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, output,
          sizeof(output), &frontend_result) !=
          W_SEED_DIAGNOSTIC_OK ||
      !contains_text(output, frontend_result.written_bytes, "outer") ||
      !contains_text(output, frontend_result.written_bytes,
                     "semantic/doc-one")) {
    return 40;
  }
  const w_seed_frontend_diagnostic saved_const = diagnostics[0];
  const w_seed_frontend_diagnostic_label saved_label = diagnostic_labels[0];
  diagnostic_labels[0].role = (w_seed_frontend_text){"bad-role", 8u};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 41;
  }
  diagnostic_labels[0] = saved_label;
  diagnostics[0] = saved_const;
  diagnostic_facts[0].first_item = 7u;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 42;
  }
  diagnostic_facts[0].first_item = 0u;

  /* Set order and duplicate checks, plus an exact cross-document span. */
  diagnostic_facts[0] = (w_seed_frontend_diagnostic_fact){
      .key = {"actualCase", 10u},
      .kind = W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
      .text = {"C", 1u},
      .integer_value = 0,
      .first_item = W_SEED_FRONTEND_NONE,
      .item_count = 0u};
  diagnostic_facts[1] = (w_seed_frontend_diagnostic_fact){
      .key = {"allowedCases", 12u},
      .kind = W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET,
      .text = {NULL, 0u},
      .integer_value = 0,
      .first_item = 0u,
      .item_count = 2u};
  diagnostic_items[0].text = (w_seed_frontend_text){"A", 1u};
  diagnostic_items[1].text = (w_seed_frontend_text){"B", 1u};
  diagnostic_facts[2] = (w_seed_frontend_diagnostic_fact){
      .key = {"baseEnum", 8u},
      .kind = W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
      .text = {"E", 1u},
      .integer_value = 0,
      .first_item = W_SEED_FRONTEND_NONE,
      .item_count = 0u};
  diagnostic_facts[3] = (w_seed_frontend_diagnostic_fact){
      .key = {"expectedType", 12u},
      .kind = W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
      .text = {"E<[A,B]>", 8u},
      .integer_value = 0,
      .first_item = W_SEED_FRONTEND_NONE,
      .item_count = 0u};
  diagnostic_labels[0] = (w_seed_frontend_diagnostic_label){
      {"expected-type", 13u}, {0u, 1u}, 1u};
  diagnostics[0] = (w_seed_frontend_diagnostic){
      {"W-TYPE-0121", sizeof("W-TYPE-0121") - 1u}, {2u, 3u}, 0u, 0u, 4u,
      0u, 1u};
  frontend_context.diagnostic_fact_count = 4u;
  frontend_context.diagnostic_item_count = 2u;
  frontend_context.diagnostic_label_count = 1u;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) !=
      W_SEED_DIAGNOSTIC_CAPACITY) {
    return 43;
  }
  diagnostic_items[1].text = diagnostic_items[0].text;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 44;
  }
  diagnostic_items[1].text = (w_seed_frontend_text){"B", 1u};
  diagnostic_items[0].text = (w_seed_frontend_text){"B", 1u};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) !=
      W_SEED_DIAGNOSTIC_INVALID) {
    return 45;
  }
  diagnostic_items[0].text = (w_seed_frontend_text){"A", 1u};
  diagnostic_items[1].text = (w_seed_frontend_text){"B", 1u};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) != W_SEED_DIAGNOSTIC_CAPACITY) {
    return 48;
  }
  diagnostic_facts[1].text = (w_seed_frontend_text){"hidden", 6u};
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) != W_SEED_DIAGNOSTIC_INVALID) {
    return 49;
  }
  diagnostic_facts[1].text = (w_seed_frontend_text){NULL, 0u};
  diagnostic_facts[1].integer_value = 1;
  if (w_seed_diagnostic_frontend_record(
          instance, sizeof(instance) - 1u, &frontend_context, 0u, NULL, 0u,
          &frontend_result) != W_SEED_DIAGNOSTIC_INVALID) {
    return 50;
  }
  diagnostic_facts[1].integer_value = 0;
  for (size_t matrix_index = 0u;
       matrix_index < sizeof(frontend_matrix_cases) /
                           sizeof(frontend_matrix_cases[0]);
       matrix_index += 1u) {
    if (!frontend_matrix_case_passes(matrix_index)) return 46;
  }
  if (!frontend_adapter_invalid_cases_pass()) return 47;
  return 0;
}
