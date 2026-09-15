#include "w_seed_accelerated_invocation0.h"

#include <limits.h>
#include <string.h>

#include "w_seed_sha256.h"
#include "w_seed_source.h"
#include "w_seed_unicode.h"

static const char ACCINV0_SEMANTIC_TAG[] =
    "w-seed-accelerated-invocation0-semantic-1";
static const char ACCINV0_PROVENANCE_TAG[] =
    "w-seed-accelerated-invocation0-provenance-1";

typedef struct {
  uintptr_t begin;
  uintptr_t end;
  bool active;
} accinv0_range;

typedef struct {
  w_seed_accelerated_invocation0_counts counts;
  w_seed_accelerated_invocation0_record invocation;
  w_seed_frontend_text domain_name;
  w_seed_frontend_text module_name;
  w_seed_frontend_text kernel_label;
  w_seed_frontend_text function_name;
  uint32_t frontend_module_count;
  uint32_t frontend_function_count;
  uint32_t frontend_statement_count;
  uint32_t frontend_expression_count;
  uint32_t frontend_type_count;
  uint32_t frontend_domain_count;
  uint32_t frontend_accelerator_module_count;
  uint32_t frontend_accelerator_kernel_count;
  uint32_t gpu_module_count;
  uint32_t gpu_kernel_count;
  uint8_t frontend_receipt_digest[
      W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
  uint8_t gpu_semantic_digest[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
  uint8_t gpu_provenance_digest[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
  uint8_t semantic_digest[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
  uint8_t provenance_digest[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
} accinv0_scan;

static bool add_size(size_t left, size_t right, size_t *result) {
  if (result == NULL || right > SIZE_MAX - left) return false;
  *result = left + right;
  return true;
}

static bool range_make(const void *pointer, size_t count, size_t element_size,
                       accinv0_range *range) {
  if (range == NULL || element_size == 0u) return false;
  *range = (accinv0_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || count > SIZE_MAX / element_size) return false;
  const size_t bytes = count * element_size;
  if (bytes > (size_t)UINTPTR_MAX) return false;
  const uintptr_t begin = (uintptr_t)pointer;
  const uintptr_t length = (uintptr_t)bytes;
  if (begin > UINTPTR_MAX - length) return false;
  *range = (accinv0_range){begin, begin + length, true};
  return true;
}

static bool range_append(accinv0_range *ranges, size_t capacity, size_t *count,
                         const void *pointer, size_t elements,
                         size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity) return false;
  if (elements == 0u) return true;
  if (pointer == NULL) return false;
  accinv0_range range;
  if (!range_make(pointer, elements, element_size, &range)) return false;
  ranges[*count] = range;
  *count += 1u;
  return true;
}

static bool ranges_overlap(accinv0_range left, accinv0_range right) {
  return left.active && right.active && left.begin < right.end &&
         right.begin < left.end;
}

static bool destinations_exclude(const accinv0_range *destinations,
                                 size_t destination_count,
                                 const void *pointer, size_t elements,
                                 size_t element_size) {
  accinv0_range source;
  if (!range_make(pointer, elements, element_size, &source)) return false;
  for (size_t index = 0u; index < destination_count; index += 1u)
    if (ranges_overlap(destinations[index], source)) return false;
  return true;
}

static bool ranges_pairwise_disjoint(const accinv0_range *ranges,
                                     size_t count) {
  if (ranges == NULL) return false;
  for (size_t left = 0u; left < count; left += 1u)
    for (size_t right = left + 1u; right < count; right += 1u)
      if (ranges_overlap(ranges[left], ranges[right])) return false;
  return true;
}

static bool text_valid(w_seed_frontend_text text) {
  return text.length == 0u || text.data != NULL;
}

static bool text_equal(w_seed_frontend_text left,
                       w_seed_frontend_text right) {
  return left.length == right.length && text_valid(left) &&
         text_valid(right) &&
         (left.length == 0u || memcmp(left.data, right.data, left.length) == 0);
}

static bool text_nonempty(w_seed_frontend_text text) {
  return text.length != 0u && text.data != NULL;
}

static uint32_t decode_utf8(const uint8_t *bytes, size_t *width) {
  const uint8_t first = bytes[0];
  if (first < 0x80u) {
    *width = 1u;
    return first;
  }
  if (first < 0xe0u) {
    *width = 2u;
    return (((uint32_t)first & UINT32_C(0x1f)) << 6) |
           ((uint32_t)bytes[1] & UINT32_C(0x3f));
  }
  if (first < 0xf0u) {
    *width = 3u;
    return (((uint32_t)first & UINT32_C(0x0f)) << 12) |
           (((uint32_t)bytes[1] & UINT32_C(0x3f)) << 6) |
           ((uint32_t)bytes[2] & UINT32_C(0x3f));
  }
  *width = 4u;
  return (((uint32_t)first & UINT32_C(0x07)) << 18) |
         (((uint32_t)bytes[1] & UINT32_C(0x3f)) << 12) |
         (((uint32_t)bytes[2] & UINT32_C(0x3f)) << 6) |
         ((uint32_t)bytes[3] & UINT32_C(0x3f));
}

static bool identifier(w_seed_frontend_text text) {
  if (!text_nonempty(text)) return false;
  w_seed_source source;
  w_seed_source_error error;
  if (!w_seed_source_init(
          (w_seed_byte_view){(const uint8_t *)text.data, text.length},
          &source, &error))
    return false;
  size_t offset = 0u;
  bool first = true;
  while (offset < text.length) {
    size_t width = 0u;
    const uint32_t point =
        decode_utf8((const uint8_t *)text.data + offset, &width);
    if (width == 0u || width > text.length - offset ||
        (first && !w_seed_unicode_is_identifier_start(point)) ||
        (!first && !w_seed_unicode_is_identifier_continue(point)))
      return false;
    first = false;
    offset += width;
  }
  return !first;
}

static bool domain_identifier(w_seed_frontend_text text) {
  if (text.length < 2u || text.data == NULL || text.data[0] != '.')
    return false;
  return identifier((w_seed_frontend_text){text.data + 1u, text.length - 1u});
}

static bool span_ordered(w_seed_span span) {
  return span.start_byte <= span.end_byte;
}

static bool span_contains(w_seed_span owner, w_seed_span child) {
  return span_ordered(owner) && span_ordered(child) &&
         owner.start_byte <= child.start_byte && child.end_byte <= owner.end_byte;
}

static bool source_span_valid(const w_seed_frontend_document *document,
                              w_seed_span span) {
  return document != NULL && document->source != NULL &&
         w_seed_source_validate_span(document->source, span, NULL);
}

static bool array_valid(const void *pointer, size_t count, size_t capacity,
                        size_t element_size) {
  return element_size != 0u && count <= capacity &&
         (count == 0u || pointer != NULL) &&
         (count == 0u || count <= SIZE_MAX / element_size);
}

static bool frontend_counts_equal(const w_seed_frontend_counts *left,
                                  const w_seed_frontend_counts *right) {
  if (left == NULL || right == NULL) return false;
#define ACCINV0_COUNT(field) \
  if (left->field != right->field) return false
  ACCINV0_COUNT(modules);
  ACCINV0_COUNT(imports);
  ACCINV0_COUNT(import_items);
  ACCINV0_COUNT(structs);
  ACCINV0_COUNT(fields);
  ACCINV0_COUNT(type_declarations);
  ACCINV0_COUNT(aliases);
  ACCINV0_COUNT(types);
  ACCINV0_COUNT(functions);
  ACCINV0_COUNT(parameters);
  ACCINV0_COUNT(entries);
  ACCINV0_COUNT(statements);
  ACCINV0_COUNT(expressions);
  ACCINV0_COUNT(interpolation_segments);
  ACCINV0_COUNT(arguments);
  ACCINV0_COUNT(symbols);
  ACCINV0_COUNT(facts);
  ACCINV0_COUNT(diagnostics);
  ACCINV0_COUNT(diagnostic_facts);
  ACCINV0_COUNT(diagnostic_items);
  ACCINV0_COUNT(diagnostic_labels);
  ACCINV0_COUNT(receipt_bytes);
  ACCINV0_COUNT(enums);
  ACCINV0_COUNT(enum_cases);
  ACCINV0_COUNT(enum_case_parameters);
  ACCINV0_COUNT(switch_arms);
  ACCINV0_COUNT(pattern_captures);
  ACCINV0_COUNT(enum_subset_members);
  ACCINV0_COUNT(enum_membership_cases);
  ACCINV0_COUNT(generic_parameters);
  ACCINV0_COUNT(generic_applications);
  ACCINV0_COUNT(generic_arguments);
  ACCINV0_COUNT(typed_const_expressions);
  ACCINV0_COUNT(const_values);
  ACCINV0_COUNT(const_elements);
  ACCINV0_COUNT(const_bytes);
  ACCINV0_COUNT(const_declarations);
  ACCINV0_COUNT(accelerator_modules);
  ACCINV0_COUNT(accelerator_kernels);
#undef ACCINV0_COUNT
  return true;
}

static bool gpu_text(const w_seed_gpu_module_program *program, size_t offset,
                     size_t bytes, w_seed_frontend_text *text) {
  if (program == NULL || text == NULL || offset > program->text_bytes ||
      bytes > program->text_bytes - offset)
    return false;
  *text = (w_seed_frontend_text){
      bytes == 0u ? NULL : (const char *)program->text + offset, bytes};
  return true;
}

static void hash_u32(w_seed_sha256_state *state, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8),
                            (uint8_t)(value >> 16),
                            (uint8_t)(value >> 24)};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void hash_u64(w_seed_sha256_state *state, uint64_t value) {
  uint8_t bytes[8];
  for (size_t index = 0u; index < sizeof(bytes); index += 1u)
    bytes[index] = (uint8_t)(value >> (index * 8u));
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void hash_span(w_seed_sha256_state *state, w_seed_span span) {
  hash_u64(state, (uint64_t)span.start_byte);
  hash_u64(state, (uint64_t)span.end_byte);
}

static void hash_text(w_seed_sha256_state *state,
                      w_seed_frontend_text text) {
  hash_u64(state, (uint64_t)text.length);
  if (text.length != 0u)
    w_seed_sha256_update(state, (const uint8_t *)text.data, text.length);
}

static void hash_bytes(w_seed_sha256_state *state, const uint8_t *bytes,
                       size_t count) {
  hash_u64(state, (uint64_t)count);
  if (count != 0u) w_seed_sha256_update(state, bytes, count);
}

static void record_hash_semantic(
    w_seed_sha256_state *state,
    const w_seed_accelerated_invocation0_record *record,
    w_seed_frontend_text domain_name, w_seed_frontend_text module_name,
    w_seed_frontend_text kernel_label, w_seed_frontend_text function_name,
    const uint8_t gpu_semantic_digest[32]) {
  hash_u32(state, (uint32_t)record->domain_kind);
  hash_u32(state, (uint32_t)record->submission);
  hash_u32(state, record->domain_capabilities);
  hash_u32(state, record->domain_maximum);
  hash_text(state, domain_name);
  hash_text(state, module_name);
  hash_text(state, kernel_label);
  hash_text(state, function_name);
  hash_u32(state, record->result_bit_width);
  hash_u32(state, record->result_is_signed ? 1u : 0u);
  hash_bytes(state, gpu_semantic_digest, 32u);
}

static void record_hash_provenance(
    w_seed_sha256_state *state,
    const w_seed_accelerated_invocation0_record *record,
    const uint8_t frontend_receipt_digest[32],
    const uint8_t gpu_semantic_digest[32],
    const uint8_t gpu_provenance_digest[32]) {
  hash_u32(state, record->frontend_module_index);
  hash_u32(state, record->frontend_owner_function_index);
  hash_u32(state, record->frontend_accelerator_module_index);
  hash_u32(state, record->frontend_accelerator_kernel_index);
  hash_u32(state, record->gpu_module_index);
  hash_u32(state, record->gpu_kernel_index);
  hash_u32(state, record->source_launch_expression);
  hash_u32(state, record->source_call_expression);
  hash_u32(state, record->source_await_expression);
  hash_u32(state, record->source_launch_binding_statement);
  hash_u32(state, record->source_result_binding_statement);
  hash_u32(state, record->result_type_index);
  hash_u32(state, record->kernel_return_type_index);
  hash_u32(state, record->domain_index);
  hash_u32(state, (uint32_t)record->domain_kind);
  hash_u32(state, (uint32_t)record->submission);
  hash_u32(state, record->domain_capabilities);
  hash_u32(state, record->domain_maximum);
  hash_u32(state, record->result_bit_width);
  hash_u32(state, record->result_is_signed ? 1u : 0u);
  hash_span(state, record->launch_span);
  hash_span(state, record->call_span);
  hash_span(state, record->await_span);
  hash_span(state, record->launch_binding_span);
  hash_span(state, record->result_binding_span);
  hash_bytes(state, frontend_receipt_digest, 32u);
  hash_bytes(state, gpu_semantic_digest, 32u);
  hash_bytes(state, gpu_provenance_digest, 32u);
}

static void scan_digests(accinv0_scan *scan) {
  w_seed_sha256_state semantic;
  w_seed_sha256_state provenance;
  w_seed_sha256_init(&semantic);
  w_seed_sha256_init(&provenance);
  hash_bytes(&semantic, (const uint8_t *)ACCINV0_SEMANTIC_TAG,
             sizeof(ACCINV0_SEMANTIC_TAG) - 1u);
  hash_bytes(&provenance, (const uint8_t *)ACCINV0_PROVENANCE_TAG,
             sizeof(ACCINV0_PROVENANCE_TAG) - 1u);
  hash_u64(&semantic, (uint64_t)scan->counts.invocations);
  hash_u64(&semantic, (uint64_t)scan->counts.text_bytes);
  hash_u64(&provenance, (uint64_t)scan->counts.invocations);
  hash_u64(&provenance, (uint64_t)scan->counts.text_bytes);
  record_hash_semantic(&semantic, &scan->invocation, scan->domain_name,
                       scan->module_name, scan->kernel_label,
                       scan->function_name, scan->gpu_semantic_digest);
  record_hash_provenance(&provenance, &scan->invocation,
                         scan->frontend_receipt_digest,
                         scan->gpu_semantic_digest,
                         scan->gpu_provenance_digest);
  w_seed_sha256_final(&semantic, scan->semantic_digest);
  w_seed_sha256_update(&provenance, scan->semantic_digest,
                       sizeof(scan->semantic_digest));
  w_seed_sha256_final(&provenance, scan->provenance_digest);
}

static bool program_text(const w_seed_accelerated_invocation0_program *program,
                         size_t offset, size_t bytes,
                         w_seed_frontend_text *text) {
  if (program == NULL || text == NULL || offset > program->text_bytes ||
      bytes > program->text_bytes - offset)
    return false;
  *text = (w_seed_frontend_text){
      bytes == 0u ? NULL : (const char *)program->text + offset, bytes};
  return true;
}

static bool program_metadata_valid(
    const w_seed_accelerated_invocation0_program *program) {
  return program != NULL && program->frontend_module_count != 0u &&
         program->frontend_domain_count != 0u &&
         program->frontend_accelerator_module_count != 0u &&
         program->frontend_accelerator_kernel_count != 0u &&
         program->gpu_module_count != 0u && program->gpu_kernel_count != 0u &&
         program->frontend_function_count != 0u &&
         program->frontend_statement_count != 0u &&
         program->frontend_expression_count != 0u &&
         program->frontend_type_count != 0u;
}

static bool program_records_valid(
    const w_seed_accelerated_invocation0_program *program,
    w_seed_frontend_text *domain_name, w_seed_frontend_text *module_name,
    w_seed_frontend_text *kernel_label, w_seed_frontend_text *function_name) {
  if (!program_metadata_valid(program) || program->invocation_count != 1u ||
      !array_valid(program->invocations, program->invocation_count,
                   program->invocation_capacity,
                   sizeof(*program->invocations)) ||
      !array_valid(program->text, program->text_bytes, program->text_capacity,
                   sizeof(*program->text)) ||
      domain_name == NULL || module_name == NULL || kernel_label == NULL ||
      function_name == NULL)
    return false;
  const w_seed_accelerated_invocation0_record *record =
      &program->invocations[0];
  if (record->frontend_module_index >= program->frontend_module_count ||
      record->frontend_owner_function_index >= program->frontend_function_count ||
      record->frontend_accelerator_module_index >=
          program->frontend_accelerator_module_count ||
      record->frontend_accelerator_kernel_index >=
          program->frontend_accelerator_kernel_count ||
      record->gpu_module_index >= program->gpu_module_count ||
      record->gpu_kernel_index >= program->gpu_kernel_count ||
      record->source_launch_expression >= program->frontend_expression_count ||
      record->source_call_expression >= program->frontend_expression_count ||
      record->source_await_expression >= program->frontend_expression_count ||
      record->source_launch_binding_statement >=
          program->frontend_statement_count ||
      record->source_result_binding_statement >=
           program->frontend_statement_count ||
      record->result_type_index >= program->frontend_type_count ||
      record->kernel_return_type_index >= program->frontend_type_count ||
      record->domain_index == W_SEED_ACCELERATED_INVOCATION0_NONE ||
      record->domain_index >= program->frontend_domain_count ||
      record->domain_kind != W_SEED_FRONTEND_DOMAIN_ACCELERATED ||
      (record->submission != W_SEED_FRONTEND_DOMAIN_MODE_SERIAL &&
       record->submission != W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT) ||
      record->domain_capabilities !=
          W_SEED_FRONTEND_DOMAIN_CAPABILITY_DEVICE ||
      record->domain_maximum == 0u || record->result_bit_width != 32u ||
      !record->result_is_signed ||
      record->result_type_index != record->kernel_return_type_index ||
      record->source_call_expression >= record->source_launch_expression ||
      record->source_launch_expression >= record->source_await_expression ||
      record->source_launch_binding_statement >=
          record->source_result_binding_statement ||
      !span_ordered(record->launch_span) || !span_ordered(record->call_span) ||
      !span_ordered(record->await_span) ||
      !span_ordered(record->launch_binding_span) ||
      !span_ordered(record->result_binding_span) ||
      !span_contains(record->launch_span, record->call_span) ||
      !span_contains(record->launch_binding_span, record->call_span) ||
      !span_contains(record->result_binding_span, record->await_span) ||
      record->call_span.end_byte > record->await_span.start_byte ||
      record->launch_binding_span.end_byte >
          record->result_binding_span.start_byte)
    return false;
  if (!program_text(program, record->domain_name_offset,
                    record->domain_name_bytes, domain_name) ||
      !program_text(program, record->module_name_offset,
                    record->module_name_bytes, module_name) ||
      !program_text(program, record->kernel_label_offset,
                    record->kernel_label_bytes, kernel_label) ||
      !program_text(program, record->function_name_offset,
                    record->function_name_bytes, function_name) ||
      !domain_identifier(*domain_name) || !identifier(*module_name) ||
      !identifier(*kernel_label) || !identifier(*function_name))
    return false;
  size_t text_cursor = 0u;
  if (record->domain_name_offset != text_cursor ||
      !add_size(text_cursor, record->domain_name_bytes, &text_cursor) ||
      record->module_name_offset != text_cursor ||
      !add_size(text_cursor, record->module_name_bytes, &text_cursor) ||
      record->kernel_label_offset != text_cursor ||
      !add_size(text_cursor, record->kernel_label_bytes, &text_cursor) ||
      record->function_name_offset != text_cursor ||
      !add_size(text_cursor, record->function_name_bytes, &text_cursor))
    return false;
  return text_cursor == program->text_bytes;
}

static bool program_ranges_free(
    const w_seed_accelerated_invocation0_program *program,
    const w_seed_accelerated_invocation0_result *result) {
  accinv0_range ranges[8];
  size_t count = 0u;
  if (!range_append(ranges, 8u, &count, program, 1u, sizeof(*program)) ||
      !range_append(ranges, 8u, &count, result, 1u, sizeof(*result)) ||
      !range_append(ranges, 8u, &count, program->invocations,
                    program->invocation_capacity,
                    sizeof(*program->invocations)) ||
      !range_append(ranges, 8u, &count, program->text, program->text_capacity,
                    sizeof(*program->text)))
    return false;
  return ranges_pairwise_disjoint(ranges, count);
}

static bool frontend_sources_exclude(
    const accinv0_range *destinations, size_t destination_count,
    const w_seed_accelerated_invocation0_input *input) {
  if (destinations == NULL || input == NULL ||
      input->frontend_input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL)
    return false;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  const w_seed_frontend_output *frontend_output = input->frontend_output;
  const w_seed_frontend_result *frontend_result = input->frontend_result;
#define ACCINV0_SOURCE(pointer, elements, type)                                \
  if (!destinations_exclude(destinations, destination_count, pointer,          \
                            elements, sizeof(type)))                           \
    return false
#define ACCINV0_TEXT(value)                                                     \
  if (!destinations_exclude(destinations, destination_count, (value).data,     \
                            (value).length, sizeof(char)))                     \
    return false
  ACCINV0_SOURCE(input, 1u, w_seed_accelerated_invocation0_input);
  ACCINV0_SOURCE(frontend_input, 1u, w_seed_frontend_input);
  ACCINV0_SOURCE(frontend_output, 1u, w_seed_frontend_output);
  ACCINV0_SOURCE(frontend_result, 1u, w_seed_frontend_result);
  ACCINV0_TEXT(frontend_result->schema_version);
  ACCINV0_SOURCE(frontend_input->documents, frontend_input->document_count,
                 w_seed_frontend_document);
  ACCINV0_SOURCE(frontend_input->domains, frontend_input->domain_count,
                 w_seed_frontend_domain);
  ACCINV0_SOURCE(frontend_input->resolved_imports,
                 frontend_input->resolved_import_count,
                 w_seed_frontend_import);
  for (size_t index = 0u; index < frontend_input->document_count; index += 1u) {
    const w_seed_frontend_document *document =
        &frontend_input->documents[index];
    if (document->source == NULL) return false;
    ACCINV0_SOURCE(document->source, 1u, w_seed_source);
    const w_seed_byte_view source_bytes = w_seed_source_bytes(document->source);
    ACCINV0_SOURCE(source_bytes.data, source_bytes.length, uint8_t);
    ACCINV0_SOURCE(document->nodes, document->node_count, w_seed_cst_node);
    ACCINV0_TEXT(document->logical_source_id);
    ACCINV0_TEXT(document->module_id);
    ACCINV0_TEXT(document->local_module_name);
  }
  for (size_t index = 0u; index < frontend_input->domain_count; index += 1u)
    ACCINV0_TEXT(frontend_input->domains[index].name);
#define ACCINV0_FRONTEND_ARRAY(field, capacity_field, type)                    \
  ACCINV0_SOURCE(frontend_output->field, frontend_output->capacity_field,      \
                 type)
  ACCINV0_FRONTEND_ARRAY(modules, module_capacity, w_seed_frontend_module);
  ACCINV0_FRONTEND_ARRAY(const_declarations, const_declaration_capacity,
                         w_seed_frontend_const_declaration);
  ACCINV0_FRONTEND_ARRAY(accelerator_modules, accelerator_module_capacity,
                         w_seed_frontend_accelerator_module);
  ACCINV0_FRONTEND_ARRAY(accelerator_kernels, accelerator_kernel_capacity,
                         w_seed_frontend_accelerator_kernel);
  ACCINV0_FRONTEND_ARRAY(functions, function_capacity, w_seed_frontend_function);
  ACCINV0_FRONTEND_ARRAY(parameters, parameter_capacity,
                         w_seed_frontend_parameter);
  ACCINV0_FRONTEND_ARRAY(statements, statement_capacity,
                         w_seed_frontend_statement);
  ACCINV0_FRONTEND_ARRAY(expressions, expression_capacity,
                         w_seed_frontend_expression);
  ACCINV0_FRONTEND_ARRAY(types, type_capacity, w_seed_frontend_type);
  ACCINV0_FRONTEND_ARRAY(receipt, receipt_capacity, uint8_t);
#undef ACCINV0_FRONTEND_ARRAY
  for (size_t index = 0u; index < frontend_result->written.modules; index += 1u) {
    const w_seed_frontend_module *module = &frontend_output->modules[index];
    ACCINV0_TEXT(module->source_id);
    ACCINV0_TEXT(module->module_id);
    ACCINV0_TEXT(module->local_module_name);
  }
  for (size_t index = 0u;
       index < frontend_result->written.const_declarations; index += 1u)
    ACCINV0_TEXT(frontend_output->const_declarations[index].name);
  for (size_t index = 0u;
       index < frontend_result->written.accelerator_kernels; index += 1u)
    ACCINV0_TEXT(frontend_output->accelerator_kernels[index].label);
  for (size_t index = 0u; index < frontend_result->written.functions; index += 1u)
    ACCINV0_TEXT(frontend_output->functions[index].name);
  for (size_t index = 0u;
       index < frontend_result->written.statements; index += 1u)
    ACCINV0_TEXT(frontend_output->statements[index].binding_name);
  for (size_t index = 0u;
       index < frontend_result->written.expressions; index += 1u)
    ACCINV0_TEXT(frontend_output->expressions[index].member_name);
#undef ACCINV0_TEXT
#undef ACCINV0_SOURCE
  return true;
}

static bool gpu_sources_exclude(
    const accinv0_range *destinations, size_t destination_count,
    const w_seed_accelerated_invocation0_input *input) {
  if (destinations == NULL || input == NULL ||
      input->gpu_module_program == NULL ||
      input->gpu_module_result == NULL)
    return false;
  const w_seed_gpu_module_program *program = input->gpu_module_program;
#define ACCINV0_GPU_SOURCE(pointer, elements, type)                            \
  if (!destinations_exclude(destinations, destination_count, pointer,          \
                            elements, sizeof(type)))                           \
    return false
  ACCINV0_GPU_SOURCE(program, 1u, w_seed_gpu_module_program);
  ACCINV0_GPU_SOURCE(input->gpu_module_result, 1u, w_seed_gpu_module_result);
  ACCINV0_GPU_SOURCE(program->modules, program->module_capacity,
                    w_seed_gpu_module_record);
  ACCINV0_GPU_SOURCE(program->kernels, program->kernel_capacity,
                    w_seed_gpu_module_kernel);
  ACCINV0_GPU_SOURCE(program->text, program->text_capacity, uint8_t);
  ACCINV0_GPU_SOURCE(program->frontend_receipt,
                    program->frontend_receipt_capacity, uint8_t);
#undef ACCINV0_GPU_SOURCE
  return true;
}

static bool run_destinations_free(
    const w_seed_accelerated_invocation0_input *input,
    const w_seed_accelerated_invocation0_output *output,
    const w_seed_accelerated_invocation0_result *result) {
  accinv0_range destinations[4];
  size_t destination_count = 0u;
  if (!range_append(destinations, 4u, &destination_count, output, 1u,
                    sizeof(*output)) ||
      !range_append(destinations, 4u, &destination_count, result, 1u,
                    sizeof(*result)) ||
      !range_append(destinations, 4u, &destination_count, output->invocations,
                    output->invocation_capacity,
                    sizeof(*output->invocations)) ||
      !range_append(destinations, 4u, &destination_count, output->text,
                    output->text_capacity, sizeof(*output->text)) ||
      !ranges_pairwise_disjoint(destinations, destination_count))
    return false;
  return frontend_sources_exclude(destinations, destination_count, input) &&
         gpu_sources_exclude(destinations, destination_count, input);
}

static bool measure_destinations_free(
    const w_seed_accelerated_invocation0_input *input,
    const w_seed_accelerated_invocation0_counts *counts,
    const w_seed_accelerated_invocation0_result *result) {
  accinv0_range destinations[2];
  size_t destination_count = 0u;
  if (!range_append(destinations, 2u, &destination_count, counts, 1u,
                    sizeof(*counts)) ||
      !range_append(destinations, 2u, &destination_count, result, 1u,
                    sizeof(*result)) ||
      !ranges_pairwise_disjoint(destinations, destination_count))
    return false;
  return frontend_sources_exclude(destinations, destination_count, input) &&
         gpu_sources_exclude(destinations, destination_count, input);
}

static w_seed_accelerated_invocation0_status preflight(
    const w_seed_accelerated_invocation0_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL ||
      input->gpu_module_program == NULL || input->gpu_module_result == NULL)
    return W_SEED_ACCELERATED_INVOCATION0_INVALID_ARGUMENT;
  const w_seed_frontend_result *frontend_result = input->frontend_result;
  const w_seed_frontend_output *frontend_output = input->frontend_output;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  if (!text_equal(frontend_result->schema_version,
                  (w_seed_frontend_text){W_SEED_FRONTEND_SCHEMA_VERSION,
                                         sizeof(W_SEED_FRONTEND_SCHEMA_VERSION) -
                                             1u}))
    return W_SEED_ACCELERATED_INVOCATION0_INVALID_SCHEMA;
  if (frontend_result->status != W_SEED_FRONTEND_OK)
    return W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED;
  if (!frontend_counts_equal(&frontend_result->required,
                             &frontend_result->written) ||
      frontend_result->receipt_bytes !=
          frontend_result->written.receipt_bytes)
    return W_SEED_ACCELERATED_INVOCATION0_INCONSISTENT;
  if (frontend_input->document_count == 0u ||
      frontend_input->documents == NULL ||
      frontend_input->document_count != frontend_result->written.modules ||
      frontend_input->domain_count == 0u || frontend_input->domains == NULL ||
      frontend_result->written.modules > UINT32_MAX ||
      frontend_input->domain_count > UINT32_MAX ||
      frontend_result->written.functions > UINT32_MAX ||
      frontend_result->written.statements > UINT32_MAX ||
      frontend_result->written.expressions > UINT32_MAX ||
       frontend_result->written.types > UINT32_MAX ||
      frontend_result->written.accelerator_modules > UINT32_MAX ||
      frontend_result->written.accelerator_kernels > UINT32_MAX)
    return W_SEED_ACCELERATED_INVOCATION0_INCONSISTENT;
#define ACCINV0_FRONTEND_VALID(field, capacity_field, type)                   \
  if (!array_valid(frontend_output->field, frontend_result->written.field,    \
                   frontend_output->capacity_field, sizeof(type)))           \
    return W_SEED_ACCELERATED_INVOCATION0_INCONSISTENT
  ACCINV0_FRONTEND_VALID(modules, module_capacity, w_seed_frontend_module);
  ACCINV0_FRONTEND_VALID(const_declarations, const_declaration_capacity,
                         w_seed_frontend_const_declaration);
  ACCINV0_FRONTEND_VALID(accelerator_modules, accelerator_module_capacity,
                         w_seed_frontend_accelerator_module);
  ACCINV0_FRONTEND_VALID(accelerator_kernels, accelerator_kernel_capacity,
                         w_seed_frontend_accelerator_kernel);
  ACCINV0_FRONTEND_VALID(functions, function_capacity, w_seed_frontend_function);
  ACCINV0_FRONTEND_VALID(parameters, parameter_capacity,
                         w_seed_frontend_parameter);
  ACCINV0_FRONTEND_VALID(statements, statement_capacity,
                         w_seed_frontend_statement);
  ACCINV0_FRONTEND_VALID(expressions, expression_capacity,
                         w_seed_frontend_expression);
  ACCINV0_FRONTEND_VALID(types, type_capacity, w_seed_frontend_type);
  if (!array_valid(frontend_output->receipt, frontend_result->receipt_bytes,
                   frontend_output->receipt_capacity, sizeof(uint8_t)))
    return W_SEED_ACCELERATED_INVOCATION0_INCONSISTENT;
#undef ACCINV0_FRONTEND_VALID
  if (frontend_result->written.facts != 0u ||
      frontend_result->written.diagnostics != 0u ||
      !w_seed_gpu_module_verify(input->gpu_module_program,
                                input->gpu_module_result))
    return W_SEED_ACCELERATED_INVOCATION0_INCONSISTENT;
  const w_seed_gpu_module_result *gpu_result = input->gpu_module_result;
  if (gpu_result->written.modules > UINT32_MAX ||
      gpu_result->written.kernels > UINT32_MAX ||
      gpu_result->written.modules == 0u || gpu_result->written.kernels == 0u)
    return W_SEED_ACCELERATED_INVOCATION0_INCONSISTENT;
  return W_SEED_ACCELERATED_INVOCATION0_OK;
}

static bool frontend_receipt_digest(const w_seed_accelerated_invocation0_input
                                        *input,
                                    uint8_t digest[32]) {
  if (input == NULL || digest == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL ||
      input->frontend_output->receipt == NULL)
    return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, input->frontend_output->receipt,
                       input->frontend_result->written.receipt_bytes);
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool scan_find_source(const w_seed_accelerated_invocation0_input *input,
                             accinv0_scan *scan) {
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *frontend_result = input->frontend_result;
  const w_seed_gpu_module_program *gpu_program = input->gpu_module_program;
  if (scan == NULL || !frontend_receipt_digest(input,
                                                scan->frontend_receipt_digest))
    return false;
  if (memcmp(scan->frontend_receipt_digest,
             input->gpu_module_result->frontend_receipt_digest,
             sizeof(scan->frontend_receipt_digest)) != 0)
    return false;
  (void)memcpy(scan->gpu_semantic_digest,
               input->gpu_module_result->semantic_digest,
               sizeof(scan->gpu_semantic_digest));
  (void)memcpy(scan->gpu_provenance_digest,
               input->gpu_module_result->provenance_digest,
               sizeof(scan->gpu_provenance_digest));
  scan->frontend_module_count = (uint32_t)frontend_result->written.modules;
  scan->frontend_function_count = (uint32_t)frontend_result->written.functions;
  scan->frontend_statement_count = (uint32_t)frontend_result->written.statements;
  scan->frontend_expression_count =
      (uint32_t)frontend_result->written.expressions;
  scan->frontend_type_count = (uint32_t)frontend_result->written.types;
  scan->frontend_domain_count = (uint32_t)frontend_input->domain_count;
  scan->frontend_accelerator_module_count =
      (uint32_t)frontend_result->written.accelerator_modules;
  scan->frontend_accelerator_kernel_count =
      (uint32_t)frontend_result->written.accelerator_kernels;
  scan->gpu_module_count = (uint32_t)gpu_program->module_count;
  scan->gpu_kernel_count = (uint32_t)gpu_program->kernel_count;

  const w_seed_frontend_expression *launch = NULL;
  uint32_t launch_index = W_SEED_ACCELERATED_INVOCATION0_NONE;
  size_t launch_count = 0u;
  for (size_t index = 0u; index < frontend_result->written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *candidate = &output->expressions[index];
    if (candidate->kind !=
        W_SEED_FRONTEND_EXPR_SPAWN_ACCELERATED_DOMAIN_LAUNCH)
      continue;
    if (!candidate->supported || index >= (size_t)UINT32_MAX ||
        launch_count != 0u)
      return false;
    launch = candidate;
    launch_index = (uint32_t)index;
    launch_count += 1u;
  }
  if (launch_count != 1u || launch == NULL ||
      launch->domain_index == W_SEED_ACCELERATED_INVOCATION0_NONE ||
      (size_t)launch->domain_index >= frontend_input->domain_count)
    return false;
  const w_seed_frontend_domain *domain =
      &frontend_input->domains[launch->domain_index];
  if (domain->kind != W_SEED_FRONTEND_DOMAIN_ACCELERATED ||
      domain->mode != launch->domain_mode ||
      domain->kind != launch->domain_kind ||
      domain->capabilities != launch->domain_capabilities ||
      domain->maximum != launch->domain_maximum ||
      domain->capabilities != W_SEED_FRONTEND_DOMAIN_CAPABILITY_DEVICE ||
      domain->maximum == 0u || !domain_identifier(domain->name))
    return false;
  if (launch->task_call_expression == W_SEED_ACCELERATED_INVOCATION0_NONE ||
      (size_t)launch->task_call_expression >=
          frontend_result->written.expressions ||
      launch->inferred_type == W_SEED_ACCELERATED_INVOCATION0_NONE ||
      (size_t)launch->inferred_type >= frontend_result->written.types ||
      output->types[launch->inferred_type].kind != W_SEED_FRONTEND_TYPE_TASK ||
      output->types[launch->inferred_type].task_result_type ==
          W_SEED_ACCELERATED_INVOCATION0_NONE)
    return false;
  const uint32_t call_index = launch->task_call_expression;
  const w_seed_frontend_expression *call = &output->expressions[call_index];
  if (call->kind != W_SEED_FRONTEND_EXPR_CALL || !call->supported ||
      call->module_index >= frontend_result->written.modules ||
      call->owner_function >= frontend_result->written.functions ||
      call->left == W_SEED_ACCELERATED_INVOCATION0_NONE ||
      (size_t)call->left >= frontend_result->written.expressions ||
      call->resolved_callee_kind !=
          W_SEED_FRONTEND_CALLEE_ACCELERATOR_MODULE_FIELD ||
      call->resolved_accelerator_module_index ==
          W_SEED_ACCELERATED_INVOCATION0_NONE ||
      call->resolved_accelerator_kernel_index ==
          W_SEED_ACCELERATED_INVOCATION0_NONE ||
      call->resolved_function_index == W_SEED_ACCELERATED_INVOCATION0_NONE)
    return false;
  const w_seed_frontend_expression *callee = &output->expressions[call->left];
  if (callee->kind != W_SEED_FRONTEND_EXPR_MEMBER || !callee->supported ||
      callee->module_index != call->module_index ||
      callee->owner_function != call->owner_function ||
      callee->resolved_callee_kind !=
          W_SEED_FRONTEND_CALLEE_ACCELERATOR_MODULE_FIELD ||
      callee->resolved_accelerator_module_index !=
          call->resolved_accelerator_module_index ||
      callee->resolved_accelerator_kernel_index !=
          call->resolved_accelerator_kernel_index ||
      callee->resolved_function_index != call->resolved_function_index ||
      callee->member_name.length == 0u)
    return false;
  if (launch->module_index != call->module_index ||
      launch->owner_function != call->owner_function)
    return false;
  const uint32_t module_index = call->resolved_accelerator_module_index;
  const uint32_t kernel_index = call->resolved_accelerator_kernel_index;
  const uint32_t function_index = call->resolved_function_index;
  if (module_index >= frontend_result->written.accelerator_modules ||
      kernel_index >= frontend_result->written.accelerator_kernels ||
      function_index >= frontend_result->written.functions)
    return false;
  const w_seed_frontend_accelerator_module *module =
      &output->accelerator_modules[module_index];
  const w_seed_frontend_accelerator_kernel *kernel =
      &output->accelerator_kernels[kernel_index];
  const w_seed_frontend_function *function = &output->functions[function_index];
  if (module->module_index != call->module_index ||
      kernel->module_index != call->module_index ||
      kernel->owner_accelerator_module != module_index ||
       kernel->function_index != function_index ||
      !text_equal(kernel->label, callee->member_name) ||
      module->const_declaration_index >= frontend_result->written.const_declarations ||
       function->module_index != call->module_index ||
       function->return_type >= frontend_result->written.types ||
       function->parameter_count != 0u || call->argument_count != 0u ||
      function->return_type != call->inferred_type ||
      call->inferred_type != output->types[launch->inferred_type].task_result_type)
    return false;
  const w_seed_frontend_type *result_type = &output->types[call->inferred_type];
  if (result_type->kind != W_SEED_FRONTEND_TYPE_INTEGER ||
      !result_type->is_signed || result_type->bit_width != 32u ||
      function->is_async || function->is_throws || function->is_unsafe ||
      function->has_borrow_clause || function->is_anonymous_entry)
    return false;

  const w_seed_frontend_module *frontend_module =
      &output->modules[call->module_index];
  if (frontend_module->document_index >= frontend_input->document_count)
    return false;
  const w_seed_frontend_document *document =
      &frontend_input->documents[frontend_module->document_index];
  if (!source_span_valid(document, launch->span) ||
      !source_span_valid(document, call->span) ||
      !source_span_valid(document, callee->span) ||
      !span_contains(launch->span, call->span) ||
      !span_contains(call->span, callee->span))
    return false;

  size_t gpu_module_index = SIZE_MAX;
  for (size_t index = 0u; index < gpu_program->module_count; index += 1u) {
    const w_seed_gpu_module_record *candidate = &gpu_program->modules[index];
    if (candidate->frontend_accelerator_module_index != module_index) continue;
    if (gpu_module_index != SIZE_MAX) return false;
    gpu_module_index = index;
    if (candidate->frontend_module_index != call->module_index ||
        candidate->frontend_const_declaration_index !=
            module->const_declaration_index)
      return false;
  }
  if (gpu_module_index == SIZE_MAX) return false;
  size_t gpu_kernel_index = SIZE_MAX;
  for (size_t index = 0u; index < gpu_program->kernel_count; index += 1u) {
    const w_seed_gpu_module_kernel *candidate = &gpu_program->kernels[index];
    if (candidate->frontend_accelerator_kernel_index != kernel_index ||
        candidate->owner_module != gpu_module_index)
      continue;
    if (gpu_kernel_index != SIZE_MAX) return false;
    gpu_kernel_index = index;
    if (candidate->frontend_function_index != function_index ||
        candidate->frontend_return_type_index != function->return_type ||
        candidate->return_bit_width != result_type->bit_width ||
        !candidate->return_is_signed)
      return false;
    w_seed_frontend_text ignored;
    if (!gpu_text(gpu_program, candidate->label_offset,
                  candidate->label_bytes, &ignored) ||
        !text_equal(ignored, kernel->label) ||
        !gpu_text(gpu_program, candidate->function_name_offset,
                  candidate->function_name_bytes, &ignored))
      return false;
    scan->kernel_label = (w_seed_frontend_text){
        (const char *)gpu_program->text + candidate->label_offset,
        candidate->label_bytes};
    scan->function_name = (w_seed_frontend_text){
        (const char *)gpu_program->text + candidate->function_name_offset,
        candidate->function_name_bytes};
  }
  if (gpu_kernel_index == SIZE_MAX || !identifier(scan->function_name)) return false;
  const w_seed_gpu_module_record *gpu_module =
      &gpu_program->modules[gpu_module_index];
  if (!gpu_text(gpu_program, gpu_module->const_name_offset,
                gpu_module->const_name_bytes, &scan->module_name) ||
      !identifier(scan->module_name) ||
      !gpu_text(gpu_program, gpu_program->kernels[gpu_kernel_index].label_offset,
                gpu_program->kernels[gpu_kernel_index].label_bytes,
                &scan->kernel_label))
    return false;
  scan->domain_name = domain->name;
  scan->invocation.frontend_module_index = call->module_index;
  scan->invocation.frontend_owner_function_index = call->owner_function;
  scan->invocation.frontend_accelerator_module_index = module_index;
  scan->invocation.frontend_accelerator_kernel_index = kernel_index;
  if (gpu_module_index > UINT32_MAX || gpu_kernel_index > UINT32_MAX)
    return false;
  scan->invocation.gpu_module_index = (uint32_t)gpu_module_index;
  scan->invocation.gpu_kernel_index = (uint32_t)gpu_kernel_index;
  scan->invocation.source_launch_expression = launch_index;
  scan->invocation.source_call_expression = call_index;
  scan->invocation.source_launch_binding_statement =
      W_SEED_ACCELERATED_INVOCATION0_NONE;
  scan->invocation.source_result_binding_statement =
      W_SEED_ACCELERATED_INVOCATION0_NONE;
  scan->invocation.result_type_index = call->inferred_type;
  scan->invocation.kernel_return_type_index = function->return_type;
  scan->invocation.domain_index = launch->domain_index;
  scan->invocation.domain_kind = launch->domain_kind;
  scan->invocation.submission = launch->domain_mode;
  scan->invocation.domain_capabilities = launch->domain_capabilities;
  scan->invocation.domain_maximum = launch->domain_maximum;
  scan->invocation.result_bit_width = result_type->bit_width;
  scan->invocation.result_is_signed = result_type->is_signed;
  scan->invocation.launch_span = launch->span;
  scan->invocation.call_span = call->span;

  uint32_t launch_binding = W_SEED_ACCELERATED_INVOCATION0_NONE;
  size_t launch_bindings = 0u;
  for (size_t index = 0u; index < frontend_result->written.statements;
       index += 1u) {
    const w_seed_frontend_statement *statement = &output->statements[index];
    if (statement->expression_index != launch_index ||
        statement->owner_function != call->owner_function)
      continue;
    if (launch_bindings != 0u ||
        statement->kind != W_SEED_FRONTEND_STMT_LET ||
        statement->module_index != call->module_index ||
        statement->effective_type != launch->inferred_type ||
        !text_nonempty(statement->binding_name) || index > UINT32_MAX)
      return false;
    launch_binding = (uint32_t)index;
    launch_bindings += 1u;
    scan->invocation.launch_binding_span = statement->span;
  }
  if (launch_bindings != 1u) return false;
  scan->invocation.source_launch_binding_statement = launch_binding;

  uint32_t await_index = W_SEED_ACCELERATED_INVOCATION0_NONE;
  size_t await_count = 0u;
  for (size_t index = 0u; index < frontend_result->written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *candidate = &output->expressions[index];
    if (candidate->kind != W_SEED_FRONTEND_EXPR_AWAIT ||
        candidate->task_binding_statement != launch_binding ||
        candidate->owner_function != call->owner_function)
      continue;
    if (await_count != 0u || !candidate->supported || index > UINT32_MAX ||
        candidate->module_index != call->module_index ||
        candidate->inferred_type != call->inferred_type ||
        candidate->task_result_type != call->inferred_type)
      return false;
    await_index = (uint32_t)index;
    await_count += 1u;
    scan->invocation.await_span = candidate->span;
  }
  if (await_count != 1u) return false;
  scan->invocation.source_await_expression = await_index;
  size_t result_bindings = 0u;
  for (size_t index = 0u; index < frontend_result->written.statements;
       index += 1u) {
    const w_seed_frontend_statement *statement = &output->statements[index];
    if (statement->expression_index != await_index ||
        statement->owner_function != call->owner_function)
      continue;
    if (result_bindings != 0u ||
        statement->kind != W_SEED_FRONTEND_STMT_LET ||
        statement->module_index != call->module_index ||
        statement->effective_type != call->inferred_type || index > UINT32_MAX)
      return false;
    scan->invocation.source_result_binding_statement = (uint32_t)index;
    scan->invocation.result_binding_span = statement->span;
    result_bindings += 1u;
  }
  if (result_bindings != 1u ||
      !source_span_valid(document, scan->invocation.launch_binding_span) ||
      !source_span_valid(document, scan->invocation.result_binding_span) ||
      !source_span_valid(document, scan->invocation.await_span) ||
      scan->invocation.call_span.end_byte >
          scan->invocation.await_span.start_byte ||
      scan->invocation.launch_binding_span.end_byte >
          scan->invocation.result_binding_span.start_byte ||
      !span_contains(scan->invocation.launch_binding_span, call->span) ||
      !span_contains(scan->invocation.result_binding_span,
                     scan->invocation.await_span))
    return false;

  scan->counts.invocations = 1u;
  size_t text_cursor = 0u;
  scan->invocation.domain_name_offset = text_cursor;
  scan->invocation.domain_name_bytes = scan->domain_name.length;
  if (!add_size(text_cursor, scan->domain_name.length, &text_cursor)) return false;
  scan->invocation.module_name_offset = text_cursor;
  scan->invocation.module_name_bytes = scan->module_name.length;
  if (!add_size(text_cursor, scan->module_name.length, &text_cursor)) return false;
  scan->invocation.kernel_label_offset = text_cursor;
  scan->invocation.kernel_label_bytes = scan->kernel_label.length;
  if (!add_size(text_cursor, scan->kernel_label.length, &text_cursor)) return false;
  scan->invocation.function_name_offset = text_cursor;
  scan->invocation.function_name_bytes = scan->function_name.length;
  if (!add_size(text_cursor, scan->function_name.length, &text_cursor)) return false;
  scan->counts.text_bytes = text_cursor;
  scan_digests(scan);
  return true;
}

static w_seed_accelerated_invocation0_result result_from_scan(
    const accinv0_scan *scan, bool written) {
  w_seed_accelerated_invocation0_result result;
  (void)memset(&result, 0, sizeof(result));
  result.status = W_SEED_ACCELERATED_INVOCATION0_OK;
  result.required = scan->counts;
  if (written) result.written = scan->counts;
  (void)memcpy(result.schema,
               W_SEED_ACCELERATED_INVOCATION0_SCHEMA_VERSION,
               sizeof(result.schema));
  (void)memcpy(result.frontend_schema, W_SEED_FRONTEND_SCHEMA_VERSION,
               sizeof(result.frontend_schema));
  (void)memcpy(result.gpu_module_schema, W_SEED_GPU_MODULE_SCHEMA_VERSION,
               sizeof(result.gpu_module_schema));
  result.frontend_module_count = scan->frontend_module_count;
  result.frontend_function_count = scan->frontend_function_count;
  result.frontend_statement_count = scan->frontend_statement_count;
  result.frontend_expression_count = scan->frontend_expression_count;
  result.frontend_type_count = scan->frontend_type_count;
  result.frontend_domain_count = scan->frontend_domain_count;
  result.frontend_accelerator_module_count =
      scan->frontend_accelerator_module_count;
  result.frontend_accelerator_kernel_count =
      scan->frontend_accelerator_kernel_count;
  result.gpu_module_count = scan->gpu_module_count;
  result.gpu_kernel_count = scan->gpu_kernel_count;
  (void)memcpy(result.frontend_receipt_digest, scan->frontend_receipt_digest,
               sizeof(result.frontend_receipt_digest));
  (void)memcpy(result.gpu_semantic_digest, scan->gpu_semantic_digest,
               sizeof(result.gpu_semantic_digest));
  (void)memcpy(result.gpu_provenance_digest, scan->gpu_provenance_digest,
               sizeof(result.gpu_provenance_digest));
  (void)memcpy(result.semantic_digest, scan->semantic_digest,
               sizeof(result.semantic_digest));
  (void)memcpy(result.provenance_digest, scan->provenance_digest,
               sizeof(result.provenance_digest));
  return result;
}

static bool result_matches_program(
    const w_seed_accelerated_invocation0_program *program,
    const w_seed_accelerated_invocation0_result *result) {
  if (program == NULL || result == NULL ||
      result->status != W_SEED_ACCELERATED_INVOCATION0_OK ||
      memcmp(result->schema, W_SEED_ACCELERATED_INVOCATION0_SCHEMA_VERSION,
             sizeof(result->schema)) != 0 ||
      memcmp(result->frontend_schema, W_SEED_FRONTEND_SCHEMA_VERSION,
             sizeof(result->frontend_schema)) != 0 ||
      memcmp(result->gpu_module_schema, W_SEED_GPU_MODULE_SCHEMA_VERSION,
             sizeof(result->gpu_module_schema)) != 0 ||
      result->required.invocations != result->written.invocations ||
      result->required.text_bytes != result->written.text_bytes ||
      result->written.invocations != program->invocation_count ||
      result->written.text_bytes != program->text_bytes ||
      result->frontend_module_count != program->frontend_module_count ||
      result->frontend_function_count != program->frontend_function_count ||
      result->frontend_statement_count != program->frontend_statement_count ||
      result->frontend_expression_count != program->frontend_expression_count ||
      result->frontend_type_count != program->frontend_type_count ||
      result->frontend_domain_count != program->frontend_domain_count ||
      result->frontend_accelerator_module_count !=
          program->frontend_accelerator_module_count ||
      result->frontend_accelerator_kernel_count !=
          program->frontend_accelerator_kernel_count ||
      result->gpu_module_count != program->gpu_module_count ||
      result->gpu_kernel_count != program->gpu_kernel_count ||
      memcmp(result->frontend_receipt_digest, program->frontend_receipt_digest,
             sizeof(result->frontend_receipt_digest)) != 0 ||
      memcmp(result->gpu_semantic_digest, program->gpu_semantic_digest,
             sizeof(result->gpu_semantic_digest)) != 0 ||
      memcmp(result->gpu_provenance_digest, program->gpu_provenance_digest,
             sizeof(result->gpu_provenance_digest)) != 0)
    return false;
  return true;
}

static bool program_digests(const w_seed_accelerated_invocation0_program
                                *program,
                            uint8_t semantic_digest[32],
                            uint8_t provenance_digest[32]) {
  w_seed_frontend_text domain_name;
  w_seed_frontend_text module_name;
  w_seed_frontend_text kernel_label;
  w_seed_frontend_text function_name;
  if (semantic_digest == NULL || provenance_digest == NULL ||
      !program_records_valid(program, &domain_name, &module_name, &kernel_label,
                             &function_name))
    return false;
  w_seed_sha256_state semantic;
  w_seed_sha256_state provenance;
  w_seed_sha256_init(&semantic);
  w_seed_sha256_init(&provenance);
  hash_bytes(&semantic, (const uint8_t *)ACCINV0_SEMANTIC_TAG,
             sizeof(ACCINV0_SEMANTIC_TAG) - 1u);
  hash_bytes(&provenance, (const uint8_t *)ACCINV0_PROVENANCE_TAG,
             sizeof(ACCINV0_PROVENANCE_TAG) - 1u);
  hash_u64(&semantic, (uint64_t)program->invocation_count);
  hash_u64(&semantic, (uint64_t)program->text_bytes);
  hash_u64(&provenance, (uint64_t)program->invocation_count);
  hash_u64(&provenance, (uint64_t)program->text_bytes);
  record_hash_semantic(&semantic, &program->invocations[0], domain_name,
                       module_name, kernel_label, function_name,
                       program->gpu_semantic_digest);
  record_hash_provenance(&provenance, &program->invocations[0],
                         program->frontend_receipt_digest,
                         program->gpu_semantic_digest,
                         program->gpu_provenance_digest);
  w_seed_sha256_final(&semantic, semantic_digest);
  w_seed_sha256_update(&provenance, semantic_digest, 32u);
  w_seed_sha256_final(&provenance, provenance_digest);
  return true;
}

static bool output_capacity_valid(
    const w_seed_accelerated_invocation0_output *output,
    const w_seed_accelerated_invocation0_counts *counts) {
  return output != NULL && counts != NULL &&
         array_valid(output->invocations, counts->invocations,
                     output->invocation_capacity,
                     sizeof(*output->invocations)) &&
         array_valid(output->text, counts->text_bytes, output->text_capacity,
                     sizeof(*output->text));
}

w_seed_accelerated_invocation0_status w_seed_accelerated_invocation0_measure(
    const w_seed_accelerated_invocation0_input *input,
    w_seed_accelerated_invocation0_counts *counts,
    w_seed_accelerated_invocation0_result *result) {
  if (counts == NULL || result == NULL)
    return W_SEED_ACCELERATED_INVOCATION0_INVALID_ARGUMENT;
  const w_seed_accelerated_invocation0_status ready = preflight(input);
  if (ready != W_SEED_ACCELERATED_INVOCATION0_OK) return ready;
  if (!measure_destinations_free(input, counts, result))
    return W_SEED_ACCELERATED_INVOCATION0_ALIAS;
  accinv0_scan scan;
  (void)memset(&scan, 0, sizeof(scan));
  if (!scan_find_source(input, &scan))
    return W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED;
  const w_seed_accelerated_invocation0_result candidate =
      result_from_scan(&scan, false);
  *counts = scan.counts;
  *result = candidate;
  return W_SEED_ACCELERATED_INVOCATION0_OK;
}

w_seed_accelerated_invocation0_status w_seed_accelerated_invocation0_run(
    const w_seed_accelerated_invocation0_input *input,
    const w_seed_accelerated_invocation0_output *output,
    w_seed_accelerated_invocation0_result *result) {
  if (output == NULL || result == NULL)
    return W_SEED_ACCELERATED_INVOCATION0_INVALID_ARGUMENT;
  const w_seed_accelerated_invocation0_status ready = preflight(input);
  if (ready != W_SEED_ACCELERATED_INVOCATION0_OK) return ready;
  accinv0_scan scan;
  (void)memset(&scan, 0, sizeof(scan));
  if (!scan_find_source(input, &scan))
    return W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED;
  if (!output_capacity_valid(output, &scan.counts))
    return W_SEED_ACCELERATED_INVOCATION0_CAPACITY;
  if (!run_destinations_free(input, output, result))
    return W_SEED_ACCELERATED_INVOCATION0_ALIAS;
  (void)memcpy(output->text + scan.invocation.domain_name_offset,
               scan.domain_name.data, scan.domain_name.length);
  (void)memcpy(output->text + scan.invocation.module_name_offset,
               scan.module_name.data, scan.module_name.length);
  (void)memcpy(output->text + scan.invocation.kernel_label_offset,
               scan.kernel_label.data, scan.kernel_label.length);
  (void)memcpy(output->text + scan.invocation.function_name_offset,
               scan.function_name.data, scan.function_name.length);
  output->invocations[0] = scan.invocation;
  const w_seed_accelerated_invocation0_result candidate =
      result_from_scan(&scan, true);
  *result = candidate;
  return W_SEED_ACCELERATED_INVOCATION0_OK;
}

bool w_seed_accelerated_invocation0_program_from_output(
    const w_seed_accelerated_invocation0_output *output,
    const w_seed_accelerated_invocation0_result *result,
    w_seed_accelerated_invocation0_program *program) {
  if (output == NULL || result == NULL || program == NULL ||
      result->status != W_SEED_ACCELERATED_INVOCATION0_OK ||
      result->required.invocations != result->written.invocations ||
      result->required.text_bytes != result->written.text_bytes ||
      !output_capacity_valid(output, &result->written))
    return false;

  w_seed_accelerated_invocation0_program candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  candidate.invocations = output->invocations;
  candidate.invocation_count = result->written.invocations;
  candidate.invocation_capacity = output->invocation_capacity;
  candidate.text = output->text;
  candidate.text_bytes = result->written.text_bytes;
  candidate.text_capacity = output->text_capacity;
  candidate.frontend_module_count = result->frontend_module_count;
  candidate.frontend_function_count = result->frontend_function_count;
  candidate.frontend_statement_count = result->frontend_statement_count;
  candidate.frontend_expression_count = result->frontend_expression_count;
  candidate.frontend_type_count = result->frontend_type_count;
  candidate.frontend_domain_count = result->frontend_domain_count;
  candidate.frontend_accelerator_module_count =
      result->frontend_accelerator_module_count;
  candidate.frontend_accelerator_kernel_count =
      result->frontend_accelerator_kernel_count;
  candidate.gpu_module_count = result->gpu_module_count;
  candidate.gpu_kernel_count = result->gpu_kernel_count;
  (void)memcpy(candidate.frontend_receipt_digest,
               result->frontend_receipt_digest,
               sizeof(candidate.frontend_receipt_digest));
  (void)memcpy(candidate.gpu_semantic_digest, result->gpu_semantic_digest,
               sizeof(candidate.gpu_semantic_digest));
  (void)memcpy(candidate.gpu_provenance_digest,
               result->gpu_provenance_digest,
               sizeof(candidate.gpu_provenance_digest));

  accinv0_range ranges[6];
  size_t range_count = 0u;
  if (!range_append(ranges, 6u, &range_count, output, 1u, sizeof(*output)) ||
      !range_append(ranges, 6u, &range_count, result, 1u, sizeof(*result)) ||
      !range_append(ranges, 6u, &range_count, output->invocations,
                    output->invocation_capacity,
                    sizeof(*output->invocations)) ||
      !range_append(ranges, 6u, &range_count, output->text,
                    output->text_capacity, sizeof(*output->text)) ||
      !ranges_pairwise_disjoint(ranges, range_count))
    return false;
  accinv0_range program_range;
  if (!range_make(program, 1u, sizeof(*program), &program_range)) return false;
  for (size_t index = 0u; index < range_count; index += 1u)
    if (ranges_overlap(program_range, ranges[index])) return false;
  if (!w_seed_accelerated_invocation0_verify(&candidate, result)) return false;
  *program = candidate;
  return true;
}

bool w_seed_accelerated_invocation0_verify(
    const w_seed_accelerated_invocation0_program *program,
    const w_seed_accelerated_invocation0_result *result) {
  if (!result_matches_program(program, result) ||
      !program_ranges_free(program, result))
    return false;
  uint8_t semantic[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
  uint8_t provenance[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
  if (!program_digests(program, semantic, provenance)) return false;
  return memcmp(semantic, result->semantic_digest, sizeof(semantic)) == 0 &&
         memcmp(provenance, result->provenance_digest,
                sizeof(provenance)) == 0;
}
