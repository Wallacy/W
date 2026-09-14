#include "w_seed_gpu_module.h"

#include <limits.h>
#include <string.h>

#include "w_seed_sha256.h"
#include "w_seed_source.h"
#include "w_seed_unicode.h"

static const char GPU_MODULE_SEMANTIC_TAG[] =
    "w-seed-gpu-module-semantic-1";
static const char GPU_MODULE_PROVENANCE_TAG[] =
    "w-seed-gpu-module-provenance-1";

typedef struct {
  uintptr_t begin;
  uintptr_t end;
  bool active;
} gpu_module_range;

typedef struct {
  w_seed_gpu_module_counts counts;
  uint8_t receipt_digest[W_SEED_GPU_MODULE_SHA256_BYTES];
  uint8_t semantic_digest[W_SEED_GPU_MODULE_SHA256_BYTES];
  uint8_t provenance_digest[W_SEED_GPU_MODULE_SHA256_BYTES];
} gpu_module_scan;

_Static_assert(CHAR_BIT == 8, "GPU module records require 8-bit bytes");

static bool add_size(size_t left, size_t right, size_t *result) {
  if (result == NULL || right > SIZE_MAX - left) return false;
  *result = left + right;
  return true;
}

static bool counts_equal(const w_seed_gpu_module_counts *left,
                         const w_seed_gpu_module_counts *right) {
  return left != NULL && right != NULL && left->modules == right->modules &&
         left->kernels == right->kernels &&
         left->text_bytes == right->text_bytes &&
         left->frontend_receipt_bytes == right->frontend_receipt_bytes;
}

static bool frontend_counts_equal(const w_seed_frontend_counts *left,
                                  const w_seed_frontend_counts *right) {
  if (left == NULL || right == NULL) return false;
#define GPU_MODULE_COUNT(field) if (left->field != right->field) return false
  GPU_MODULE_COUNT(modules);
  GPU_MODULE_COUNT(imports);
  GPU_MODULE_COUNT(import_items);
  GPU_MODULE_COUNT(structs);
  GPU_MODULE_COUNT(fields);
  GPU_MODULE_COUNT(type_declarations);
  GPU_MODULE_COUNT(aliases);
  GPU_MODULE_COUNT(types);
  GPU_MODULE_COUNT(functions);
  GPU_MODULE_COUNT(parameters);
  GPU_MODULE_COUNT(entries);
  GPU_MODULE_COUNT(statements);
  GPU_MODULE_COUNT(expressions);
  GPU_MODULE_COUNT(interpolation_segments);
  GPU_MODULE_COUNT(arguments);
  GPU_MODULE_COUNT(symbols);
  GPU_MODULE_COUNT(facts);
  GPU_MODULE_COUNT(diagnostics);
  GPU_MODULE_COUNT(diagnostic_facts);
  GPU_MODULE_COUNT(diagnostic_items);
  GPU_MODULE_COUNT(diagnostic_labels);
  GPU_MODULE_COUNT(receipt_bytes);
  GPU_MODULE_COUNT(enums);
  GPU_MODULE_COUNT(enum_cases);
  GPU_MODULE_COUNT(enum_case_parameters);
  GPU_MODULE_COUNT(switch_arms);
  GPU_MODULE_COUNT(pattern_captures);
  GPU_MODULE_COUNT(enum_subset_members);
  GPU_MODULE_COUNT(enum_membership_cases);
  GPU_MODULE_COUNT(generic_parameters);
  GPU_MODULE_COUNT(generic_applications);
  GPU_MODULE_COUNT(generic_arguments);
  GPU_MODULE_COUNT(typed_const_expressions);
  GPU_MODULE_COUNT(const_values);
  GPU_MODULE_COUNT(const_elements);
  GPU_MODULE_COUNT(const_bytes);
  GPU_MODULE_COUNT(const_declarations);
  GPU_MODULE_COUNT(accelerator_modules);
  GPU_MODULE_COUNT(accelerator_kernels);
#undef GPU_MODULE_COUNT
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
  if (!text_valid(text) || text.length == 0u) return false;
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
    if ((first && !w_seed_unicode_is_identifier_start(point)) ||
        (!first && !w_seed_unicode_is_identifier_continue(point)))
      return false;
    first = false;
    offset += width;
  }
  return true;
}

static bool span_equal(w_seed_span left, w_seed_span right) {
  return left.start_byte == right.start_byte && left.end_byte == right.end_byte;
}

static bool span_contains(w_seed_span owner, w_seed_span child) {
  return owner.start_byte <= child.start_byte &&
         child.end_byte <= owner.end_byte;
}

static bool source_span_valid(const w_seed_frontend_document *document,
                              w_seed_span span) {
  return document != NULL && document->source != NULL &&
         w_seed_source_validate_span(document->source, span, NULL);
}

static bool utf8_text(w_seed_frontend_text text, bool allow_empty) {
  if (!text_valid(text) || (!allow_empty && text.length == 0u)) return false;
  w_seed_source source;
  w_seed_source_error error;
  return w_seed_source_init(
      (w_seed_byte_view){(const uint8_t *)text.data, text.length}, &source,
      &error);
}

static bool array_valid(const void *pointer, size_t count, size_t capacity,
                        size_t element_size) {
  return count <= capacity && (count == 0u || pointer != NULL) &&
         (count == 0u || count <= SIZE_MAX / element_size);
}

static bool range_valid(size_t first, size_t count, size_t total) {
  return first <= total && count <= total - first;
}

static void hash_u16(w_seed_sha256_state *state, uint16_t value) {
  const uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
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

static void hash_slice(w_seed_sha256_state *state, const uint8_t *text,
                       size_t offset, size_t bytes) {
  hash_u64(state, (uint64_t)bytes);
  if (bytes != 0u) w_seed_sha256_update(state, text + offset, bytes);
}

static bool decode_positive_i32(const w_seed_frontend_expression *expression,
                                int64_t *payload) {
  if (expression == NULL || payload == NULL || !expression->has_integer_value)
    return false;
  uint64_t value = 0u;
  for (size_t index = 0u; index < 8u; index += 1u)
    value |= (uint64_t)expression->integer_value[index] << (index * 8u);
  for (size_t index = 8u; index < sizeof(expression->integer_value); index += 1u)
    if (expression->integer_value[index] != 0u) return false;
  if (value > (uint64_t)INT32_MAX) return false;
  *payload = (int64_t)value;
  return true;
}

static bool frontend_shape(const w_seed_gpu_module_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL)
    return false;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (result->status != W_SEED_FRONTEND_OK ||
      !text_equal(result->schema_version,
                  (w_seed_frontend_text){W_SEED_FRONTEND_SCHEMA_VERSION,
                                         sizeof(W_SEED_FRONTEND_SCHEMA_VERSION) -
                                             1u}) ||
      !frontend_counts_equal(&result->required, &result->written) ||
      result->receipt_bytes != result->written.receipt_bytes ||
      frontend_input->document_count == 0u ||
      frontend_input->document_count != result->written.modules ||
      result->written.modules > UINT32_MAX ||
      result->written.const_declarations > UINT32_MAX ||
      result->written.accelerator_modules > UINT32_MAX ||
      result->written.accelerator_kernels > UINT32_MAX ||
      result->written.functions > UINT32_MAX ||
      result->written.statements > UINT32_MAX ||
      result->written.expressions > UINT32_MAX ||
      result->written.types > UINT32_MAX ||
      frontend_input->documents == NULL ||
      result->written.accelerator_modules == 0u ||
      result->written.accelerator_kernels == 0u ||
      result->written.facts != 0u || result->written.diagnostics != 0u)
    return false;
#define GPU_MODULE_ARRAY(field, capacity_field, type)                         \
  if (!array_valid(output->field, result->written.field,                     \
                   output->capacity_field, sizeof(type))) return false
  GPU_MODULE_ARRAY(modules, module_capacity, w_seed_frontend_module);
  GPU_MODULE_ARRAY(const_declarations, const_declaration_capacity,
                   w_seed_frontend_const_declaration);
  GPU_MODULE_ARRAY(accelerator_modules, accelerator_module_capacity,
                   w_seed_frontend_accelerator_module);
  GPU_MODULE_ARRAY(accelerator_kernels, accelerator_kernel_capacity,
                   w_seed_frontend_accelerator_kernel);
  GPU_MODULE_ARRAY(functions, function_capacity, w_seed_frontend_function);
  GPU_MODULE_ARRAY(statements, statement_capacity, w_seed_frontend_statement);
  GPU_MODULE_ARRAY(expressions, expression_capacity,
                   w_seed_frontend_expression);
  GPU_MODULE_ARRAY(types, type_capacity, w_seed_frontend_type);
  if (!array_valid(output->receipt, result->written.receipt_bytes,
                   output->receipt_capacity, sizeof(uint8_t)))
    return false;
#undef GPU_MODULE_ARRAY
  for (size_t index = 0u; index < frontend_input->document_count; index += 1u) {
    const w_seed_frontend_document *document =
        &frontend_input->documents[index];
    if (document->source == NULL || document->nodes == NULL ||
        !utf8_text(document->logical_source_id, false) ||
        !utf8_text(document->module_id, false) ||
        !identifier(document->local_module_name))
      return false;
    const w_seed_byte_view source_bytes = w_seed_source_bytes(document->source);
    if (
        document->node_count != document->parse.node_count ||
        document->parse.status != W_SEED_PARSE_COMPLETE ||
        document->parse.issue_count != 0u ||
        document->parse.root >= document->node_count ||
        document->parse.consumed_byte != source_bytes.length)
      return false;
  }
  return true;
}

static w_seed_gpu_module_status frontend_preflight(
    const w_seed_gpu_module_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL)
    return W_SEED_GPU_MODULE_INVALID_ARGUMENT;
  const w_seed_frontend_result *result = input->frontend_result;
  if (!text_equal(result->schema_version,
                  (w_seed_frontend_text){W_SEED_FRONTEND_SCHEMA_VERSION,
                                         sizeof(W_SEED_FRONTEND_SCHEMA_VERSION) -
                                             1u}))
    return W_SEED_GPU_MODULE_INVALID_SCHEMA;
  if (result->status != W_SEED_FRONTEND_OK)
    return W_SEED_GPU_MODULE_UNSUPPORTED;
  if (!frontend_counts_equal(&result->required, &result->written) ||
      result->receipt_bytes != result->written.receipt_bytes)
    return W_SEED_GPU_MODULE_INCONSISTENT;
  if (!frontend_shape(input)) return W_SEED_GPU_MODULE_INCONSISTENT;
  return W_SEED_GPU_MODULE_OK;
}

static bool kernel_body(const w_seed_gpu_module_input *input,
                        const w_seed_frontend_accelerator_kernel *binding,
                        const w_seed_frontend_function **function_out,
                        const w_seed_frontend_statement **statement_out,
                        const w_seed_frontend_expression **expression_out,
                        int64_t *payload_out) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (binding->function_index >= result->written.functions) return false;
  const w_seed_frontend_function *function =
      &output->functions[binding->function_index];
  if (function->module_index != binding->module_index ||
      !identifier(function->name) || function->parameter_count != 0u ||
      function->statement_count != 1u ||
      function->first_statement >= result->written.statements ||
      function->return_type >= result->written.types || function->is_const ||
      function->is_async || function->is_throws || function->is_unsafe ||
      function->has_borrow_clause || function->is_anonymous_entry)
    return false;
  const w_seed_frontend_type *type = &output->types[function->return_type];
  if (type->kind != W_SEED_FRONTEND_TYPE_INTEGER || !type->is_signed ||
      type->bit_width != 32u)
    return false;
  const w_seed_frontend_statement *statement =
      &output->statements[function->first_statement];
  if (statement->kind != W_SEED_FRONTEND_STMT_RETURN ||
      statement->module_index != function->module_index ||
      statement->owner_function != binding->function_index ||
      statement->expression_index >= result->written.expressions)
    return false;
  const w_seed_frontend_expression *expression =
      &output->expressions[statement->expression_index];
  int64_t payload = 0;
  if (expression->kind != W_SEED_FRONTEND_EXPR_INTEGER ||
      expression->module_index != function->module_index ||
      expression->owner_function != binding->function_index ||
      expression->inferred_type != function->return_type ||
      !expression->supported || !decode_positive_i32(expression, &payload))
    return false;
  *function_out = function;
  *statement_out = statement;
  *expression_out = expression;
  *payload_out = payload;
  return true;
}

static bool scan_frontend(const w_seed_gpu_module_input *input,
                          gpu_module_scan *scan) {
  if (scan == NULL || !frontend_shape(input)) return false;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  gpu_module_scan candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  candidate.counts.modules = result->written.accelerator_modules;
  candidate.counts.kernels = result->written.accelerator_kernels;
  candidate.counts.frontend_receipt_bytes = result->written.receipt_bytes;

  w_seed_sha256_state semantic;
  w_seed_sha256_state provenance;
  w_seed_sha256_init(&semantic);
  w_seed_sha256_init(&provenance);
  w_seed_sha256_update(&semantic, (const uint8_t *)GPU_MODULE_SEMANTIC_TAG,
                       sizeof(GPU_MODULE_SEMANTIC_TAG) - 1u);
  w_seed_sha256_update(&provenance,
                       (const uint8_t *)GPU_MODULE_PROVENANCE_TAG,
                       sizeof(GPU_MODULE_PROVENANCE_TAG) - 1u);
  hash_u64(&semantic, (uint64_t)candidate.counts.modules);
  hash_u64(&semantic, (uint64_t)candidate.counts.kernels);
  hash_u64(&provenance, (uint64_t)candidate.counts.modules);
  hash_u64(&provenance, (uint64_t)candidate.counts.kernels);

  w_seed_sha256_state receipt;
  w_seed_sha256_init(&receipt);
  w_seed_sha256_update(&receipt, output->receipt,
                       result->written.receipt_bytes);
  w_seed_sha256_final(&receipt, candidate.receipt_digest);
  w_seed_sha256_update(&provenance, candidate.receipt_digest,
                       sizeof(candidate.receipt_digest));

  size_t kernel_cursor = 0u;
  size_t text_bytes = 0u;
  for (size_t module_index = 0u;
       module_index < result->written.accelerator_modules; module_index += 1u) {
    const w_seed_frontend_accelerator_module *module =
        &output->accelerator_modules[module_index];
    if (module->module_index >= result->written.modules ||
        module->const_declaration_index >= result->written.const_declarations ||
        module->first_kernel != kernel_cursor || module->kernel_count == 0u ||
        !range_valid(module->first_kernel, module->kernel_count,
                     result->written.accelerator_kernels))
      return false;
    const w_seed_frontend_module *frontend_module =
        &output->modules[module->module_index];
    if (frontend_module->document_index >= frontend_input->document_count)
      return false;
    const w_seed_frontend_document *document =
        &frontend_input->documents[frontend_module->document_index];
    const w_seed_frontend_const_declaration *declaration =
        &output->const_declarations[module->const_declaration_index];
    if (declaration->module_index != module->module_index ||
        !identifier(declaration->name) ||
        declaration->initializer_expression != W_SEED_FRONTEND_NONE ||
        declaration->declared_type != W_SEED_FRONTEND_NONE ||
        declaration->effective_type != W_SEED_FRONTEND_NONE ||
        declaration->has_explicit_type || declaration->lowerable ||
        !source_span_valid(document, frontend_module->span) ||
        !source_span_valid(document, declaration->span) ||
        !source_span_valid(document, declaration->body_span) ||
        !source_span_valid(document, module->span) ||
        !span_equal(declaration->body_span, module->span) ||
        !span_contains(declaration->span, module->span) ||
        !text_equal(frontend_module->source_id, document->logical_source_id) ||
        !text_equal(frontend_module->module_id, document->module_id) ||
        !text_equal(frontend_module->local_module_name,
                    document->local_module_name))
      return false;
    const w_seed_frontend_text module_texts[] = {
        frontend_module->source_id, frontend_module->module_id,
        frontend_module->local_module_name, declaration->name};
    for (size_t index = 0u;
         index < sizeof(module_texts) / sizeof(module_texts[0]); index += 1u) {
      if (!text_valid(module_texts[index]) ||
          !add_size(text_bytes, module_texts[index].length, &text_bytes))
        return false;
      hash_text(&semantic, module_texts[index]);
    }
    hash_u64(&semantic, (uint64_t)module->kernel_count);
    hash_u32(&provenance, module->module_index);
    hash_u32(&provenance, module->const_declaration_index);
    hash_span(&provenance, frontend_module->span);
    hash_span(&provenance, declaration->span);

    for (size_t ordinal = 0u; ordinal < module->kernel_count; ordinal += 1u) {
      const size_t binding_index = kernel_cursor + ordinal;
      const w_seed_frontend_accelerator_kernel *binding =
          &output->accelerator_kernels[binding_index];
      const w_seed_frontend_function *function = NULL;
      const w_seed_frontend_statement *statement = NULL;
      const w_seed_frontend_expression *expression = NULL;
      int64_t payload = 0;
      if (binding->module_index != module->module_index ||
          binding->owner_accelerator_module != module_index ||
          binding->ordinal != ordinal || !identifier(binding->label) ||
          !source_span_valid(document, binding->span) ||
          !span_contains(module->span, binding->span) ||
          !kernel_body(input, binding, &function, &statement, &expression,
                       &payload) ||
          !source_span_valid(document, function->span) ||
          !source_span_valid(document, function->body_span) ||
          !source_span_valid(document, statement->span) ||
          !source_span_valid(document, expression->span) ||
          !span_contains(frontend_module->span, function->span) ||
          !span_contains(function->span, function->body_span) ||
          !span_contains(function->body_span, statement->span) ||
          !span_contains(statement->span, expression->span))
        return false;
      for (size_t prior = 0u; prior < ordinal; prior += 1u)
        if (text_equal(binding->label,
                       output->accelerator_kernels[kernel_cursor + prior].label))
          return false;
      if (!add_size(text_bytes, binding->label.length, &text_bytes) ||
          !add_size(text_bytes, function->name.length, &text_bytes))
        return false;
      hash_text(&semantic, binding->label);
      hash_text(&semantic, function->name);
      hash_u16(&semantic, 32u);
      hash_u32(&semantic, 1u);
      hash_u64(&semantic, (uint64_t)payload);
      hash_u32(&provenance, binding->function_index);
      hash_u32(&provenance, function->first_statement);
      hash_u32(&provenance, statement->expression_index);
      hash_u32(&provenance, function->return_type);
      hash_span(&provenance, binding->span);
      hash_span(&provenance, function->span);
      hash_span(&provenance, function->body_span);
      hash_span(&provenance, statement->span);
      hash_span(&provenance, expression->span);
    }
    if (!add_size(kernel_cursor, module->kernel_count, &kernel_cursor))
      return false;
  }
  if (kernel_cursor != result->written.accelerator_kernels) return false;
  candidate.counts.text_bytes = text_bytes;
  w_seed_sha256_final(&semantic, candidate.semantic_digest);
  w_seed_sha256_update(&provenance, candidate.semantic_digest,
                       sizeof(candidate.semantic_digest));
  w_seed_sha256_final(&provenance, candidate.provenance_digest);
  *scan = candidate;
  return true;
}

static bool memory_range(const void *pointer, size_t count, size_t element_size,
                         gpu_module_range *result) {
  if (result == NULL) return false;
  *result = (gpu_module_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || count > SIZE_MAX / element_size) return false;
  const size_t bytes = count * element_size;
  const uintptr_t begin = (uintptr_t)pointer;
  if (bytes > (size_t)UINTPTR_MAX || begin > UINTPTR_MAX - (uintptr_t)bytes)
    return false;
  *result = (gpu_module_range){begin, begin + (uintptr_t)bytes, true};
  return true;
}

static bool overlaps(gpu_module_range left, gpu_module_range right) {
  return left.active && right.active && left.begin < right.end &&
         right.begin < left.end;
}

static bool append_range(gpu_module_range *ranges, size_t capacity,
                         size_t *count, const void *pointer, size_t elements,
                         size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity) return false;
  gpu_module_range range;
  if (!memory_range(pointer, elements, element_size, &range)) return false;
  if (range.active) ranges[(*count)++] = range;
  return true;
}

static bool destinations_exclude(const gpu_module_range *destinations,
                                 size_t destination_count,
                                 const void *pointer, size_t elements,
                                 size_t element_size) {
  gpu_module_range source;
  if (!memory_range(pointer, elements, element_size, &source)) return false;
  for (size_t index = 0u; index < destination_count; index += 1u)
    if (overlaps(destinations[index], source)) return false;
  return true;
}

static bool destinations_exclude_input(
    const gpu_module_range *destinations, size_t destination_count,
    const w_seed_gpu_module_input *input) {
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  const w_seed_frontend_output *frontend = input->frontend_output;
  const w_seed_frontend_result *frontend_result = input->frontend_result;
#define GPU_MODULE_SOURCE(pointer, elements, type)                            \
  if (!destinations_exclude(destinations, destination_count, pointer,         \
                            elements, sizeof(type)))                          \
    return false
  GPU_MODULE_SOURCE(input, 1u, w_seed_gpu_module_input);
  GPU_MODULE_SOURCE(frontend_input, 1u, w_seed_frontend_input);
  GPU_MODULE_SOURCE(frontend, 1u, w_seed_frontend_output);
  GPU_MODULE_SOURCE(frontend_result, 1u, w_seed_frontend_result);
  GPU_MODULE_SOURCE(frontend_result->schema_version.data,
                    frontend_result->schema_version.length, char);
  GPU_MODULE_SOURCE(frontend_input->documents, frontend_input->document_count,
                    w_seed_frontend_document);
  GPU_MODULE_SOURCE(frontend->modules, frontend_result->written.modules,
                    w_seed_frontend_module);
  GPU_MODULE_SOURCE(frontend->const_declarations,
                    frontend_result->written.const_declarations,
                    w_seed_frontend_const_declaration);
  GPU_MODULE_SOURCE(frontend->accelerator_modules,
                    frontend_result->written.accelerator_modules,
                    w_seed_frontend_accelerator_module);
  GPU_MODULE_SOURCE(frontend->accelerator_kernels,
                    frontend_result->written.accelerator_kernels,
                    w_seed_frontend_accelerator_kernel);
  GPU_MODULE_SOURCE(frontend->functions, frontend_result->written.functions,
                    w_seed_frontend_function);
  GPU_MODULE_SOURCE(frontend->statements, frontend_result->written.statements,
                    w_seed_frontend_statement);
  GPU_MODULE_SOURCE(frontend->expressions, frontend_result->written.expressions,
                    w_seed_frontend_expression);
  GPU_MODULE_SOURCE(frontend->types, frontend_result->written.types,
                    w_seed_frontend_type);
  GPU_MODULE_SOURCE(frontend->receipt, frontend_result->written.receipt_bytes,
                    uint8_t);
  for (size_t index = 0u; index < frontend_input->document_count; index += 1u) {
    const w_seed_frontend_document *document =
        &frontend_input->documents[index];
    const w_seed_byte_view bytes = w_seed_source_bytes(document->source);
    GPU_MODULE_SOURCE(bytes.data, bytes.length, uint8_t);
    GPU_MODULE_SOURCE(document->nodes, document->node_count, w_seed_cst_node);
    GPU_MODULE_SOURCE(document->logical_source_id.data,
                      document->logical_source_id.length, char);
    GPU_MODULE_SOURCE(document->module_id.data, document->module_id.length,
                      char);
    GPU_MODULE_SOURCE(document->local_module_name.data,
                      document->local_module_name.length, char);
  }
#undef GPU_MODULE_SOURCE
  return true;
}

static bool output_alias_free(const w_seed_gpu_module_input *input,
                              const w_seed_gpu_module_output *output,
                              const w_seed_gpu_module_result *result,
                              const w_seed_gpu_module_counts *counts) {
  gpu_module_range destinations[5];
  size_t destination_count = 0u;
  if (!append_range(destinations, 5u, &destination_count, output->modules,
                    counts->modules, sizeof(*output->modules)) ||
      !append_range(destinations, 5u, &destination_count, output->kernels,
                    counts->kernels, sizeof(*output->kernels)) ||
      !append_range(destinations, 5u, &destination_count, output->text,
                    counts->text_bytes, 1u) ||
      !append_range(destinations, 5u, &destination_count,
                    output->frontend_receipt,
                    counts->frontend_receipt_bytes, 1u) ||
      !append_range(destinations, 5u, &destination_count, result, 1u,
                    sizeof(*result)))
    return false;
  for (size_t left = 0u; left < destination_count; left += 1u)
    for (size_t right = left + 1u; right < destination_count; right += 1u)
      if (overlaps(destinations[left], destinations[right])) return false;

  return destinations_exclude(destinations, destination_count, output, 1u,
                              sizeof(*output)) &&
         destinations_exclude_input(destinations, destination_count, input);
}

static void copy_text(uint8_t *destination, size_t *offset,
                      w_seed_frontend_text text, size_t *record_offset,
                      size_t *record_bytes) {
  *record_offset = *offset;
  *record_bytes = text.length;
  if (text.length != 0u)
    (void)memcpy(destination + *offset, text.data, text.length);
  *offset += text.length;
}

static void emit_records(const w_seed_gpu_module_input *input,
                         w_seed_gpu_module_output *output) {
  const w_seed_frontend_output *frontend = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  size_t text_offset = 0u;
  size_t kernel_cursor = 0u;
  for (size_t module_index = 0u;
       module_index < result->written.accelerator_modules; module_index += 1u) {
    const w_seed_frontend_accelerator_module *source =
        &frontend->accelerator_modules[module_index];
    const w_seed_frontend_module *frontend_module =
        &frontend->modules[source->module_index];
    const w_seed_frontend_const_declaration *declaration =
        &frontend->const_declarations[source->const_declaration_index];
    w_seed_gpu_module_record record;
    (void)memset(&record, 0, sizeof(record));
    record.frontend_module_index = source->module_index;
    record.frontend_accelerator_module_index = (uint32_t)module_index;
    record.frontend_const_declaration_index = source->const_declaration_index;
    record.source_span = frontend_module->span;
    record.const_span = declaration->span;
    record.first_kernel = kernel_cursor;
    record.kernel_count = source->kernel_count;
    copy_text(output->text, &text_offset, frontend_module->source_id,
              &record.source_id_offset, &record.source_id_bytes);
    copy_text(output->text, &text_offset, frontend_module->module_id,
              &record.module_id_offset, &record.module_id_bytes);
    copy_text(output->text, &text_offset, frontend_module->local_module_name,
              &record.local_module_name_offset,
              &record.local_module_name_bytes);
    copy_text(output->text, &text_offset, declaration->name,
              &record.const_name_offset, &record.const_name_bytes);
    output->modules[module_index] = record;
    for (size_t ordinal = 0u; ordinal < source->kernel_count; ordinal += 1u) {
      const size_t binding_index = kernel_cursor + ordinal;
      const w_seed_frontend_accelerator_kernel *binding =
          &frontend->accelerator_kernels[binding_index];
      const w_seed_frontend_function *function =
          &frontend->functions[binding->function_index];
      const w_seed_frontend_statement *statement =
          &frontend->statements[function->first_statement];
      const w_seed_frontend_expression *expression =
          &frontend->expressions[statement->expression_index];
      int64_t payload = 0;
      (void)decode_positive_i32(expression, &payload);
      w_seed_gpu_module_kernel kernel;
      (void)memset(&kernel, 0, sizeof(kernel));
      kernel.owner_module = module_index;
      kernel.ordinal = ordinal;
      kernel.frontend_accelerator_kernel_index = (uint32_t)binding_index;
      kernel.frontend_function_index = binding->function_index;
      kernel.frontend_statement_index = function->first_statement;
      kernel.frontend_expression_index = statement->expression_index;
      kernel.frontend_return_type_index = function->return_type;
      kernel.field_span = binding->span;
      kernel.function_span = function->span;
      kernel.body_span = function->body_span;
      kernel.statement_span = statement->span;
      kernel.expression_span = expression->span;
      copy_text(output->text, &text_offset, binding->label,
                &kernel.label_offset, &kernel.label_bytes);
      copy_text(output->text, &text_offset, function->name,
                &kernel.function_name_offset, &kernel.function_name_bytes);
      kernel.return_bit_width = 32u;
      kernel.return_is_signed = true;
      kernel.payload = payload;
      output->kernels[binding_index] = kernel;
    }
    kernel_cursor += source->kernel_count;
  }
  if (result->written.receipt_bytes != 0u)
    (void)memcpy(output->frontend_receipt, frontend->receipt,
                 result->written.receipt_bytes);
}

static w_seed_gpu_module_result result_for_scan(const gpu_module_scan *scan,
                                                 bool written) {
  w_seed_gpu_module_result result;
  (void)memset(&result, 0, sizeof(result));
  result.status = W_SEED_GPU_MODULE_OK;
  result.required = scan->counts;
  if (written) result.written = scan->counts;
  (void)memcpy(result.schema, W_SEED_GPU_MODULE_SCHEMA_VERSION,
               sizeof(result.schema));
  (void)memcpy(result.frontend_schema, W_SEED_FRONTEND_SCHEMA_VERSION,
               sizeof(result.frontend_schema));
  result.frontend_receipt_bytes = scan->counts.frontend_receipt_bytes;
  (void)memcpy(result.frontend_receipt_digest, scan->receipt_digest,
               sizeof(result.frontend_receipt_digest));
  (void)memcpy(result.semantic_digest, scan->semantic_digest,
               sizeof(result.semantic_digest));
  (void)memcpy(result.provenance_digest, scan->provenance_digest,
               sizeof(result.provenance_digest));
  return result;
}

w_seed_gpu_module_status w_seed_gpu_module_measure(
    const w_seed_gpu_module_input *input, w_seed_gpu_module_counts *counts,
    w_seed_gpu_module_result *result) {
  if (counts == NULL || result == NULL) return W_SEED_GPU_MODULE_INVALID_ARGUMENT;
  gpu_module_range counts_range;
  gpu_module_range result_range;
  if (!memory_range(counts, 1u, sizeof(*counts), &counts_range) ||
      !memory_range(result, 1u, sizeof(*result), &result_range) ||
      overlaps(counts_range, result_range))
    return W_SEED_GPU_MODULE_ALIAS;
  const w_seed_gpu_module_status preflight = frontend_preflight(input);
  if (preflight != W_SEED_GPU_MODULE_OK) return preflight;
  gpu_module_scan scan;
  if (!scan_frontend(input, &scan)) return W_SEED_GPU_MODULE_UNSUPPORTED;
  const gpu_module_range destinations[] = {counts_range, result_range};
  if (!destinations_exclude_input(destinations,
                                  sizeof(destinations) /
                                      sizeof(destinations[0]),
                                  input))
    return W_SEED_GPU_MODULE_ALIAS;
  const w_seed_gpu_module_result candidate = result_for_scan(&scan, false);
  *counts = scan.counts;
  *result = candidate;
  return W_SEED_GPU_MODULE_OK;
}

w_seed_gpu_module_status w_seed_gpu_module_run(
    const w_seed_gpu_module_input *input, w_seed_gpu_module_output *output,
    w_seed_gpu_module_result *result) {
  if (output == NULL || result == NULL)
    return W_SEED_GPU_MODULE_INVALID_ARGUMENT;
  const w_seed_gpu_module_status preflight = frontend_preflight(input);
  if (preflight != W_SEED_GPU_MODULE_OK) return preflight;
  gpu_module_scan scan;
  if (!scan_frontend(input, &scan)) return W_SEED_GPU_MODULE_UNSUPPORTED;
  if (!array_valid(output->modules, scan.counts.modules,
                   output->module_capacity, sizeof(*output->modules)) ||
      !array_valid(output->kernels, scan.counts.kernels,
                   output->kernel_capacity, sizeof(*output->kernels)) ||
      !array_valid(output->text, scan.counts.text_bytes,
                   output->text_capacity, 1u) ||
      !array_valid(output->frontend_receipt,
                   scan.counts.frontend_receipt_bytes,
                   output->frontend_receipt_capacity, 1u))
    return W_SEED_GPU_MODULE_CAPACITY;
  if (!output_alias_free(input, output, result, &scan.counts))
    return W_SEED_GPU_MODULE_ALIAS;
  const w_seed_gpu_module_result candidate = result_for_scan(&scan, true);
  emit_records(input, output);
  *result = candidate;
  return W_SEED_GPU_MODULE_OK;
}

static bool slice_valid(size_t offset, size_t bytes, size_t total) {
  return offset <= total && bytes <= total - offset;
}

static w_seed_frontend_text program_text(const w_seed_gpu_module_program *program,
                                         size_t offset, size_t bytes) {
  w_seed_frontend_text text = {0};
  if (bytes != 0u) text.data = (const char *)program->text + offset;
  text.length = bytes;
  return text;
}

static bool span_ordered(w_seed_span span) {
  return span.start_byte <= span.end_byte;
}

static bool program_digests(const w_seed_gpu_module_program *program,
                            uint8_t semantic_digest[32],
                            uint8_t provenance_digest[32],
                            uint8_t receipt_digest[32]) {
  w_seed_sha256_state semantic;
  w_seed_sha256_state provenance;
  w_seed_sha256_state receipt;
  w_seed_sha256_init(&semantic);
  w_seed_sha256_init(&provenance);
  w_seed_sha256_init(&receipt);
  w_seed_sha256_update(&semantic, (const uint8_t *)GPU_MODULE_SEMANTIC_TAG,
                       sizeof(GPU_MODULE_SEMANTIC_TAG) - 1u);
  w_seed_sha256_update(&provenance,
                       (const uint8_t *)GPU_MODULE_PROVENANCE_TAG,
                       sizeof(GPU_MODULE_PROVENANCE_TAG) - 1u);
  hash_u64(&semantic, (uint64_t)program->module_count);
  hash_u64(&semantic, (uint64_t)program->kernel_count);
  hash_u64(&provenance, (uint64_t)program->module_count);
  hash_u64(&provenance, (uint64_t)program->kernel_count);
  w_seed_sha256_update(&receipt, program->frontend_receipt,
                       program->frontend_receipt_bytes);
  w_seed_sha256_final(&receipt, receipt_digest);
  w_seed_sha256_update(&provenance, receipt_digest, 32u);
  size_t kernel_cursor = 0u;
  size_t text_cursor = 0u;
  for (size_t index = 0u; index < program->module_count; index += 1u) {
    const w_seed_gpu_module_record *module = &program->modules[index];
    const size_t offsets[] = {module->source_id_offset, module->module_id_offset,
                              module->local_module_name_offset,
                              module->const_name_offset};
    const size_t lengths[] = {module->source_id_bytes, module->module_id_bytes,
                              module->local_module_name_bytes,
                              module->const_name_bytes};
    if (module->frontend_accelerator_module_index != index ||
        module->first_kernel != kernel_cursor || module->kernel_count == 0u ||
        !range_valid(module->first_kernel, module->kernel_count,
                     program->kernel_count) ||
        !span_ordered(module->source_span) ||
        !span_ordered(module->const_span) ||
        !span_contains(module->source_span, module->const_span))
      return false;
    for (size_t text = 0u; text < 4u; text += 1u) {
      if (offsets[text] != text_cursor ||
          !slice_valid(offsets[text], lengths[text], program->text_bytes))
        return false;
      hash_slice(&semantic, program->text, offsets[text], lengths[text]);
      text_cursor += lengths[text];
    }
    if (!utf8_text(program_text(program, module->source_id_offset,
                                module->source_id_bytes), false) ||
        !utf8_text(program_text(program, module->module_id_offset,
                                module->module_id_bytes), false) ||
        !identifier(program_text(program, module->local_module_name_offset,
                                 module->local_module_name_bytes)) ||
        !identifier(program_text(program, module->const_name_offset,
                                 module->const_name_bytes)))
      return false;
    hash_u64(&semantic, (uint64_t)module->kernel_count);
    hash_u32(&provenance, module->frontend_module_index);
    hash_u32(&provenance, module->frontend_const_declaration_index);
    hash_span(&provenance, module->source_span);
    hash_span(&provenance, module->const_span);
    for (size_t ordinal = 0u; ordinal < module->kernel_count; ordinal += 1u) {
      const size_t kernel_index = kernel_cursor + ordinal;
      const w_seed_gpu_module_kernel *kernel = &program->kernels[kernel_index];
      if (kernel->owner_module != index || kernel->ordinal != ordinal ||
          kernel->frontend_accelerator_kernel_index != kernel_index ||
          kernel->return_bit_width != 32u || !kernel->return_is_signed ||
          kernel->payload < 0 || kernel->payload > INT32_MAX ||
          !span_ordered(kernel->field_span) ||
          !span_ordered(kernel->function_span) ||
          !span_ordered(kernel->body_span) ||
          !span_ordered(kernel->statement_span) ||
          !span_ordered(kernel->expression_span) ||
          !span_contains(module->const_span, kernel->field_span) ||
          !span_contains(module->source_span, kernel->function_span) ||
          !span_contains(kernel->function_span, kernel->body_span) ||
          !span_contains(kernel->body_span, kernel->statement_span) ||
          !span_contains(kernel->statement_span, kernel->expression_span) ||
          kernel->label_offset != text_cursor ||
          !slice_valid(kernel->label_offset, kernel->label_bytes,
                       program->text_bytes))
        return false;
      if (!identifier(program_text(program, kernel->label_offset,
                                   kernel->label_bytes)))
        return false;
      for (size_t prior = 0u; prior < ordinal; prior += 1u) {
        const w_seed_gpu_module_kernel *prior_kernel =
            &program->kernels[kernel_cursor + prior];
        if (text_equal(program_text(program, kernel->label_offset,
                                    kernel->label_bytes),
                       program_text(program, prior_kernel->label_offset,
                                    prior_kernel->label_bytes)))
          return false;
      }
      hash_slice(&semantic, program->text, kernel->label_offset,
                 kernel->label_bytes);
      text_cursor += kernel->label_bytes;
      if (kernel->function_name_offset != text_cursor ||
          !slice_valid(kernel->function_name_offset,
                       kernel->function_name_bytes, program->text_bytes))
        return false;
      if (!identifier(program_text(program, kernel->function_name_offset,
                                   kernel->function_name_bytes)))
        return false;
      hash_slice(&semantic, program->text, kernel->function_name_offset,
                 kernel->function_name_bytes);
      text_cursor += kernel->function_name_bytes;
      hash_u16(&semantic, kernel->return_bit_width);
      hash_u32(&semantic, kernel->return_is_signed ? 1u : 0u);
      hash_u64(&semantic, (uint64_t)kernel->payload);
      hash_u32(&provenance, kernel->frontend_function_index);
      hash_u32(&provenance, kernel->frontend_statement_index);
      hash_u32(&provenance, kernel->frontend_expression_index);
      hash_u32(&provenance, kernel->frontend_return_type_index);
      hash_span(&provenance, kernel->field_span);
      hash_span(&provenance, kernel->function_span);
      hash_span(&provenance, kernel->body_span);
      hash_span(&provenance, kernel->statement_span);
      hash_span(&provenance, kernel->expression_span);
    }
    kernel_cursor += module->kernel_count;
  }
  if (kernel_cursor != program->kernel_count || text_cursor != program->text_bytes)
    return false;
  w_seed_sha256_final(&semantic, semantic_digest);
  w_seed_sha256_update(&provenance, semantic_digest, 32u);
  w_seed_sha256_final(&provenance, provenance_digest);
  return true;
}

static bool program_alias_free(const w_seed_gpu_module_program *program,
                               const w_seed_gpu_module_result *result) {
  gpu_module_range ranges[6];
  size_t count = 0u;
  if (!append_range(ranges, 6u, &count, program, 1u, sizeof(*program)) ||
      !append_range(ranges, 6u, &count, program->modules,
                    program->module_count, sizeof(*program->modules)) ||
      !append_range(ranges, 6u, &count, program->kernels,
                    program->kernel_count, sizeof(*program->kernels)) ||
      !append_range(ranges, 6u, &count, program->text, program->text_bytes,
                    sizeof(*program->text)) ||
      !append_range(ranges, 6u, &count, program->frontend_receipt,
                    program->frontend_receipt_bytes,
                    sizeof(*program->frontend_receipt)) ||
      !append_range(ranges, 6u, &count, result, 1u, sizeof(*result)))
    return false;
  for (size_t left = 0u; left < count; left += 1u)
    for (size_t right = left + 1u; right < count; right += 1u)
      if (overlaps(ranges[left], ranges[right])) return false;
  return true;
}

bool w_seed_gpu_module_verify(const w_seed_gpu_module_program *program,
                              const w_seed_gpu_module_result *result) {
  if (program == NULL || result == NULL ||
      result->status != W_SEED_GPU_MODULE_OK ||
      memcmp(result->schema, W_SEED_GPU_MODULE_SCHEMA_VERSION,
             sizeof(result->schema)) != 0 ||
      memcmp(result->frontend_schema, W_SEED_FRONTEND_SCHEMA_VERSION,
             sizeof(result->frontend_schema)) != 0 ||
      !counts_equal(&result->required, &result->written) ||
      result->written.modules != program->module_count ||
      result->written.kernels != program->kernel_count ||
      result->written.text_bytes != program->text_bytes ||
      result->written.frontend_receipt_bytes !=
          program->frontend_receipt_bytes ||
      result->frontend_receipt_bytes != program->frontend_receipt_bytes ||
      program->module_count == 0u || program->kernel_count == 0u ||
      program->frontend_receipt_bytes == 0u ||
      !array_valid(program->modules, program->module_count,
                   program->module_capacity, sizeof(*program->modules)) ||
      !array_valid(program->kernels, program->kernel_count,
                   program->kernel_capacity, sizeof(*program->kernels)) ||
      !array_valid(program->text, program->text_bytes, program->text_capacity,
                   1u) ||
      !array_valid(program->frontend_receipt,
                   program->frontend_receipt_bytes,
                   program->frontend_receipt_capacity, 1u))
    return false;
  if (!program_alias_free(program, result)) return false;
  uint8_t semantic[32];
  uint8_t provenance[32];
  uint8_t receipt[32];
  return program_digests(program, semantic, provenance, receipt) &&
         memcmp(semantic, result->semantic_digest, 32u) == 0 &&
         memcmp(provenance, result->provenance_digest, 32u) == 0 &&
         memcmp(receipt, result->frontend_receipt_digest, 32u) == 0;
}

bool w_seed_gpu_module_program_from_output(
    const w_seed_gpu_module_output *output,
    const w_seed_gpu_module_result *result,
    w_seed_gpu_module_program *program) {
  if (output == NULL || result == NULL || program == NULL ||
      result->status != W_SEED_GPU_MODULE_OK ||
      !counts_equal(&result->required, &result->written))
    return false;
  gpu_module_range destination;
  if (!memory_range(program, 1u, sizeof(*program), &destination)) return false;
  gpu_module_range sources[6];
  size_t source_count = 0u;
  if (!append_range(sources, 6u, &source_count, output, 1u, sizeof(*output)) ||
      !append_range(sources, 6u, &source_count, result, 1u, sizeof(*result)) ||
      !append_range(sources, 6u, &source_count, output->modules,
                    result->written.modules, sizeof(*output->modules)) ||
      !append_range(sources, 6u, &source_count, output->kernels,
                    result->written.kernels, sizeof(*output->kernels)) ||
      !append_range(sources, 6u, &source_count, output->text,
                    result->written.text_bytes, 1u) ||
      !append_range(sources, 6u, &source_count, output->frontend_receipt,
                    result->written.frontend_receipt_bytes, 1u))
    return false;
  for (size_t index = 0u; index < source_count; index += 1u)
    if (overlaps(destination, sources[index])) return false;
  const w_seed_gpu_module_program candidate = {
      .modules = output->modules,
      .module_count = result->written.modules,
      .module_capacity = output->module_capacity,
      .kernels = output->kernels,
      .kernel_count = result->written.kernels,
      .kernel_capacity = output->kernel_capacity,
      .text = output->text,
      .text_bytes = result->written.text_bytes,
      .text_capacity = output->text_capacity,
      .frontend_receipt = output->frontend_receipt,
      .frontend_receipt_bytes = result->written.frontend_receipt_bytes,
      .frontend_receipt_capacity = output->frontend_receipt_capacity};
  if (!w_seed_gpu_module_verify(&candidate, result)) return false;
  *program = candidate;
  return true;
}
