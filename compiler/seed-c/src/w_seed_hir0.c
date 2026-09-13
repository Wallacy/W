#include "w_seed_hir0.h"

#include <limits.h>
#include <string.h>

#include "w_seed_sha256.h"

_Static_assert(CHAR_BIT == 8, "w-seed HIR0 requires 8-bit bytes");

enum {
  HIR0_DIGEST_BYTES = 32,
  HIR0_RECEIPT_SCHEMA_BYTES = 16,
  HIR0_RECEIPT_COUNT_FIELDS = 30,
  /* M2 keeps branch-local mutation bounded without adding storage to the
   * public frontend schema. The existing nesting bound is also a safe upper
   * bound for the number of simple statements in one accepted arm. */
  HIR0_MAX_BRANCH_ASSIGNMENTS = W_SEED_HIR0_MAX_NESTING,
  /* Every frontend function owns a CST function node.  Reuse that existing
   * source bound for verifier scratch instead of adding an arbitrary HIR
   * ceiling or allocating. */
  HIR0_DIRECT_FUNCTION_BITSET_BYTES =
      (W_SEED_FRONTEND_MAX_CST_NODES + 7u) / 8u,
  HIR0_RECEIPT_BYTES = HIR0_RECEIPT_SCHEMA_BYTES +
                       HIR0_RECEIPT_COUNT_FIELDS * 8 + HIR0_DIGEST_BYTES * 2,
};

static const char HIR0_UNIT_NAME[] = "()";
static const char HIR0_STRING_NAME[] = "String";
static const char HIR0_I64_NAME[] = "i64";
static const char HIR0_BOOL_NAME[] = "Bool";
static const char HIR0_USIZE_NAME[] = "usize";
static const char HIR0_SLOT_NAME[] = ".default";
static const char HIR0_PROCESS_PROFILE[] = "native-process@1";
static const char HIR0_PROCESS_MODULE[] = "std.process";
static const char HIR0_PROCESS_ARGUMENTS[] = "Arguments";
static const char HIR0_PROCESS_CONTEXT[] = "Context";
static const char HIR0_PROCESS_EXIT_CODE[] = "ExitCode";
static const char HIR0_PROCESS_SUCCESS[] = "success";
static const char HIR0_PROCESS_IS_EMPTY[] = "isEmpty";
static const char HIR0_PROCESS_COUNT[] = "count";
static const char HIR0_PROCESS_FAILURE[] = "failure";
/* This literal binds the semantic digest to the compiler-owned, versioned
 * wrapper-release ABI axiom.  It is not evidence that a provider ran. */
static const char HIR0_PROCESS_RELEASE_ABI[] =
    "std.process@1/{stdProcessArgumentsDrop,"
    "stdProcessContextDrop}:wrapper-release-v1";

static w_seed_hir0_label_kind hir_label_kind(
    w_seed_frontend_label_kind kind) {
  switch (kind) {
    case W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY:
      return W_SEED_HIR0_LABEL_POSITIONAL_ONLY;
    case W_SEED_FRONTEND_LABEL_REQUIRED:
      return W_SEED_HIR0_LABEL_REQUIRED;
  }
  return W_SEED_HIR0_LABEL_POSITIONAL_ONLY;
}

static bool hir_label_valid(w_seed_hir0_label_kind kind,
                            w_seed_hir0_text label, bool text_is_valid) {
  if (!text_is_valid || kind > W_SEED_HIR0_LABEL_REQUIRED) return false;
  if (kind == W_SEED_HIR0_LABEL_POSITIONAL_ONLY) return label.count == 0u;
  if (kind == W_SEED_HIR0_LABEL_REQUIRED) return label.count != 0u;
  return true;
}

typedef enum {
  HIR0_PREPARE_READY = 0,
  HIR0_PREPARE_FRONTEND,
  HIR0_PREPARE_UNSUPPORTED,
  HIR0_PREPARE_INVALID,
} hir0_prepare_status;

static bool add_size(size_t left, size_t right, size_t *out) {
  if (out == NULL || right > SIZE_MAX - left) return false;
  *out = left + right;
  return true;
}

static bool count_u32(size_t value) { return value <= (size_t)UINT32_MAX; }

static uint32_t hir0_enum_carrier_width(size_t case_count) {
  size_t representable = 1u;
  uint32_t bits = 0u;
  while (representable < case_count) {
    if (representable > SIZE_MAX / 2u || bits == UINT32_MAX) return 0u;
    representable *= 2u;
    bits += 1u;
  }
  return bits == 0u ? 1u : bits;
}

static bool range_valid(size_t first, size_t count, size_t total) {
  return first <= total && count <= total - first;
}

static bool text_valid(w_seed_frontend_text text) {
  return text.length == 0u || text.data != NULL;
}

static bool frontend_label_valid(w_seed_frontend_label_kind kind,
                                 w_seed_frontend_text label) {
  if (!text_valid(label) || kind > W_SEED_FRONTEND_LABEL_REQUIRED)
    return false;
  if (kind == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY) return label.length == 0u;
  if (kind == W_SEED_FRONTEND_LABEL_REQUIRED) return label.length != 0u;
  return true;
}

static bool text_equal(w_seed_frontend_text left,
                       w_seed_frontend_text right);

/* HIR0 canonicalizes every required host parameter label to the public
 * external_parameter.name. This closed subset accepts only required and
 * positional-only callable policies. */
static bool frontend_host_label_matches(
    w_seed_frontend_label_kind kind, w_seed_frontend_text name,
    w_seed_frontend_text label) {
  if (!frontend_label_valid(kind, label)) return false;
  if (kind == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY)
    return label.length == 0u;
  return name.length != 0u && text_equal(name, label);
}

static bool frontend_parameter_label_matches(
    const w_seed_frontend_parameter *parameter,
    w_seed_frontend_text argument_label) {
  if (parameter == NULL || !frontend_label_valid(parameter->label_kind,
                                                   parameter->label) ||
      !text_valid(argument_label))
    return false;
  if (parameter->label_kind == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY)
    return argument_label.length == 0u;
  return text_equal(parameter->label, argument_label);
}

static bool frontend_enum_parameter_label_matches(
    const w_seed_frontend_enum_case_parameter *parameter,
    w_seed_frontend_text argument_label) {
  if (parameter == NULL || !text_valid(parameter->label) ||
      !text_valid(argument_label))
    return false;
  if (!parameter->has_label) return argument_label.length == 0u;
  return parameter->label.length != 0u &&
         text_equal(parameter->label, argument_label);
}

static bool text_is(w_seed_frontend_text text, const char *literal) {
  if (literal == NULL) return false;
  const size_t length = strlen(literal);
  return text.length == length &&
         (length == 0u ||
          (text.data != NULL && memcmp(text.data, literal, length) == 0));
}

static bool text_equal(w_seed_frontend_text left,
                       w_seed_frontend_text right) {
  return left.length == right.length &&
         (left.length == 0u ||
          (left.data != NULL && right.data != NULL &&
           memcmp(left.data, right.data, left.length) == 0));
}

static bool hir_text_valid(const w_seed_hir0_program *program,
                           w_seed_hir0_text text);

static bool hir_text_equal(const w_seed_hir0_program *program,
                           w_seed_hir0_text left, w_seed_hir0_text right) {
  if (!hir_text_valid(program, left) || !hir_text_valid(program, right) ||
      left.count != right.count)
    return false;
  if (left.count == 0u) return true;
  return memcmp(program->text_bytes + left.offset,
                program->text_bytes + right.offset, left.count) == 0;
}

static bool hir_text_is(const w_seed_hir0_program *program,
                        w_seed_hir0_text text, const char *literal) {
  if (!hir_text_valid(program, text) || literal == NULL) return false;
  const size_t length = strlen(literal);
  return text.count == length &&
         (length == 0u ||
          memcmp(program->text_bytes + text.offset, literal, length) == 0);
}

static bool slice_valid(uint32_t offset, uint32_t count, size_t total) {
  return (size_t)offset <= total && (size_t)count <= total - (size_t)offset;
}

static bool hir_text_valid(const w_seed_hir0_program *program,
                           w_seed_hir0_text text) {
  return program != NULL && slice_valid(text.offset, text.count,
                                        program->text_byte_count) &&
         (text.count == 0u || program->text_bytes != NULL);
}

static bool hir_host_label_matches(const w_seed_hir0_program *program,
                                   w_seed_hir0_label_kind kind,
                                   w_seed_hir0_text name,
                                   w_seed_hir0_text label) {
  if (!hir_text_valid(program, name) || !hir_text_valid(program, label) ||
      !hir_label_valid(kind, label, true))
    return false;
  if (kind == W_SEED_HIR0_LABEL_POSITIONAL_ONLY)
    return label.count == 0u;
  return name.count != 0u && hir_text_equal(program, name, label);
}

static bool byte_slice_valid(const w_seed_hir0_program *program,
                             uint32_t offset, uint32_t count) {
  return program != NULL && slice_valid(offset, count, program->value_byte_count) &&
         (count == 0u || program->value_bytes != NULL);
}

static bool span_valid(w_seed_span span, size_t source_length) {
  return span.start_byte <= span.end_byte &&
         span.end_byte <= source_length;
}

static bool span_equal(w_seed_span left, w_seed_span right) {
  return left.start_byte == right.start_byte &&
         left.end_byte == right.end_byte;
}

static bool frontend_type_supported(const w_seed_frontend_type *type) {
  if (type == NULL || !text_valid(type->spelling) ||
      type->task_result_type != W_SEED_FRONTEND_NONE)
    return false;
  if (type->kind == W_SEED_FRONTEND_TYPE_UNIT)
    return text_is(type->spelling, HIR0_UNIT_NAME);
  if (type->kind == W_SEED_FRONTEND_TYPE_STRING)
    return text_is(type->spelling, HIR0_STRING_NAME);
  if (type->kind == W_SEED_FRONTEND_TYPE_BOOL)
    return text_is(type->spelling, HIR0_BOOL_NAME);
  if (type->kind == W_SEED_FRONTEND_TYPE_INTEGER)
    return type->is_signed && type->bit_width == 64u &&
           text_is(type->spelling, HIR0_I64_NAME);
  return false;
}

static bool frontend_type_is_usize(const w_seed_frontend_type *type) {
  return type != NULL && text_valid(type->spelling) &&
         type->task_result_type == W_SEED_FRONTEND_NONE &&
         type->kind == W_SEED_FRONTEND_TYPE_INTEGER && !type->is_signed &&
         type->bit_width == (uint16_t)W_SEED_FRONTEND_TARGET_USIZE_BITS &&
         text_is(type->spelling, HIR0_USIZE_NAME);
}

static bool frontend_has_public_process(const w_seed_hir0_input *input) {
  return input != NULL && input->frontend_input != NULL &&
         input->frontend_input->external_module_count == 1u &&
         input->frontend_input->external_modules != NULL &&
         input->frontend_input->external_modules[0].symbol_count == 7u;
}

static bool frontend_local_enum_records_ok(const w_seed_hir0_input *input);

static bool frontend_local_enum_type_supported(
    const w_seed_hir0_input *input, const w_seed_frontend_type *type) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || type == NULL ||
      type->kind != W_SEED_FRONTEND_TYPE_ENUM ||
      type->enum_base_index == W_SEED_FRONTEND_NONE ||
      type->external_module_index != W_SEED_FRONTEND_NONE ||
      type->external_symbol_index != W_SEED_FRONTEND_NONE ||
      type->first_subset_member != W_SEED_FRONTEND_NONE ||
      type->subset_member_count != 0u ||
      (size_t)type->enum_base_index >= input->frontend_result->written.enums ||
      input->frontend_output->enums == NULL ||
      input->frontend_output->types == NULL)
    return false;
  const w_seed_frontend_enum *decl =
      &input->frontend_output->enums[type->enum_base_index];
  return decl->type_index != W_SEED_FRONTEND_NONE &&
         text_equal(decl->name, type->spelling) &&
         decl->type_index < input->frontend_result->written.types &&
         input->frontend_output->types[decl->type_index].kind ==
             W_SEED_FRONTEND_TYPE_ENUM &&
         input->frontend_output->types[decl->type_index].enum_base_index ==
             type->enum_base_index;
}

/* A subset type is a view of one closed local enum. The normalized member
 * range is validated separately, once per frontend output, before any HIR
 * output is touched. Keep this predicate limited to the type-level identity
 * facts so callers can use it while validating individual references. */
static bool frontend_local_enum_subset_type_supported(
    const w_seed_hir0_input *input, const w_seed_frontend_type *type) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || type == NULL ||
      type->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET ||
      !text_valid(type->spelling) || type->spelling.length == 0u ||
      type->enum_base_index == W_SEED_FRONTEND_NONE ||
      type->subset_member_count == 0u ||
      type->external_module_index != W_SEED_FRONTEND_NONE ||
      type->external_symbol_index != W_SEED_FRONTEND_NONE ||
      type->first_subset_member == W_SEED_FRONTEND_NONE ||
      (size_t)type->enum_base_index >= input->frontend_result->written.enums ||
      input->frontend_output->enums == NULL ||
      input->frontend_output->types == NULL)
    return false;
  const w_seed_frontend_enum *decl =
      &input->frontend_output->enums[type->enum_base_index];
  return decl->type_index != W_SEED_FRONTEND_NONE &&
         (size_t)decl->type_index < input->frontend_result->written.types &&
         input->frontend_output->types[decl->type_index].kind ==
             W_SEED_FRONTEND_TYPE_ENUM &&
         input->frontend_output->types[decl->type_index].enum_base_index ==
             type->enum_base_index;
}

static bool frontend_local_enum_subset_contains_case(
    const w_seed_hir0_input *input, const w_seed_frontend_type *type,
    uint32_t type_index, uint32_t enum_case_index) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || type == NULL ||
      !frontend_local_enum_subset_type_supported(input, type) ||
      type_index == W_SEED_FRONTEND_NONE ||
      (size_t)type_index >= input->frontend_result->written.types ||
      enum_case_index == W_SEED_FRONTEND_NONE ||
      (size_t)enum_case_index >= input->frontend_result->written.enum_cases ||
      input->frontend_output->enum_subset_members == NULL)
    return false;
  if (type->subset_member_count == 0u) return false;
  if ((size_t)type->first_subset_member + type->subset_member_count >
      input->frontend_result->written.enum_subset_members)
    return false;
  for (uint32_t ordinal = 0u; ordinal < type->subset_member_count; ordinal += 1u) {
    const w_seed_frontend_enum_subset_member *member =
        &input->frontend_output->enum_subset_members[
            (size_t)type->first_subset_member + ordinal];
    if (member->owner_type == type_index &&
        member->enum_base_index == type->enum_base_index &&
        member->enum_case_index == enum_case_index)
      return true;
  }
  return false;
}

/* HIR16 accepts only resolver-owned nominal types from the bounded external
 * process table. The pair is atomic. A partial pair is never a type identity. */
static bool frontend_external_type_pair_valid(
    const w_seed_hir0_input *input, uint32_t module_index,
    uint32_t symbol_index) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_input->external_modules == NULL ||
      module_index == W_SEED_FRONTEND_NONE ||
      symbol_index == W_SEED_FRONTEND_NONE ||
      (size_t)module_index >= input->frontend_input->external_module_count)
    return false;
  const w_seed_frontend_external_module *module =
      &input->frontend_input->external_modules[module_index];
  if (module->symbols == NULL || (size_t)symbol_index >= module->symbol_count)
    return false;
  const w_seed_frontend_external_symbol *symbol =
      &module->symbols[symbol_index];
  return symbol->kind == W_SEED_FRONTEND_EXTERNAL_TYPE && symbol->exported &&
         text_valid(module->module_id) && text_valid(symbol->name) &&
         text_valid(symbol->return_type) && text_valid(symbol->receiver_type);
}

static bool frontend_span_ok(const w_seed_frontend_document *document,
                             w_seed_span span);

static bool frontend_hir_type_supported(
    const w_seed_hir0_input *input, const w_seed_frontend_type *type) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || type == NULL)
    return false;
  if (type->kind == W_SEED_FRONTEND_TYPE_TASK) {
    if (!text_is(type->spelling, "Task") ||
        type->task_result_type == W_SEED_FRONTEND_NONE ||
        type->element_type != type->task_result_type ||
        (size_t)type->task_result_type >=
            input->frontend_result->written.types)
      return false;
    const w_seed_frontend_type *result =
        &input->frontend_output->types[type->task_result_type];
    return frontend_type_supported(result) &&
           (result->kind == W_SEED_FRONTEND_TYPE_BOOL ||
            (result->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
             result->is_signed && result->bit_width == 64u));
  }
  if (type->task_result_type != W_SEED_FRONTEND_NONE) return false;
  if (frontend_type_supported(type)) return true;
  if (frontend_has_public_process(input) && frontend_type_is_usize(type))
    return true;
  if (frontend_local_enum_type_supported(input, type)) return true;
  if (frontend_local_enum_subset_type_supported(input, type)) return true;
  return type != NULL && type->kind == W_SEED_FRONTEND_TYPE_NOMINAL &&
         type->external_module_index != W_SEED_FRONTEND_NONE &&
         type->external_symbol_index != W_SEED_FRONTEND_NONE &&
         frontend_external_type_pair_valid(
             input, type->external_module_index, type->external_symbol_index);
}

static bool frontend_enum_subset_same_set(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, const w_seed_frontend_type *left,
    const w_seed_frontend_type *right);

static bool frontend_supported_types_equal(
    const w_seed_frontend_type *left, const w_seed_frontend_type *right);

static bool frontend_supported_types_equal_for_input(
    const w_seed_hir0_input *input, const w_seed_frontend_type *left,
    const w_seed_frontend_type *right) {
  if (left == NULL || right == NULL) return false;
  const bool left_task = left->kind == W_SEED_FRONTEND_TYPE_TASK;
  const bool right_task = right->kind == W_SEED_FRONTEND_TYPE_TASK;
  if (left_task || right_task) {
    if (!left_task || !right_task || input == NULL ||
        input->frontend_output == NULL || input->frontend_result == NULL ||
        left->task_result_type == W_SEED_FRONTEND_NONE ||
        left->task_result_type != right->task_result_type ||
        (size_t)left->task_result_type >= input->frontend_result->written.types)
      return false;
    return frontend_type_supported(
        &input->frontend_output->types[left->task_result_type]);
  }
  const bool left_subset =
      left->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  const bool right_subset =
      right->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  if (left_subset || right_subset)
    return left_subset && right_subset && input != NULL &&
           frontend_enum_subset_same_set(input->frontend_output,
                                         input->frontend_result, left, right);
  return frontend_supported_types_equal(left, right);
}

/* Assignment is directional for the one nominal widening admitted by the
 * bounded enum-subset slice.  A value carrying a subset is safe to pass to
 * its payloadless base enum, but a base value may never be treated as a
 * subset.  Equal normalized subsets remain equivalent through the canonical
 * identity predicate above. */
static bool frontend_type_assignable_for_input(
    const w_seed_hir0_input *input, const w_seed_frontend_type *from,
    const w_seed_frontend_type *to) {
  if (from == NULL || to == NULL) return false;
  if (frontend_supported_types_equal_for_input(input, from, to)) return true;
  if (from->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET ||
      to->kind != W_SEED_FRONTEND_TYPE_ENUM ||
      !frontend_local_enum_subset_type_supported(input, from) ||
      !frontend_local_enum_type_supported(input, to))
    return false;
  return from->enum_base_index == to->enum_base_index;
}

static bool frontend_supported_types_equal(
    const w_seed_frontend_type *left, const w_seed_frontend_type *right) {
  if (left == NULL || right == NULL || left->kind != right->kind)
    return false;
  if (left->kind == W_SEED_FRONTEND_TYPE_ENUM)
    return left->enum_base_index != W_SEED_FRONTEND_NONE &&
           left->enum_base_index == right->enum_base_index;
  if (frontend_type_is_usize(left) || frontend_type_is_usize(right))
    return frontend_type_is_usize(left) && frontend_type_is_usize(right);
  if (!frontend_type_supported(left) || !frontend_type_supported(right))
    return false;
  if (left->kind == W_SEED_FRONTEND_TYPE_INTEGER)
    return left->is_signed == right->is_signed &&
           left->bit_width == right->bit_width;
  return true;
}

static bool frontend_counts_equal(const w_seed_frontend_counts *left,
                                  const w_seed_frontend_counts *right) {
  if (left == NULL || right == NULL) return false;
#define HIR0_COUNT(field) if (left->field != right->field) return false
  HIR0_COUNT(modules);
  HIR0_COUNT(imports);
  HIR0_COUNT(import_items);
  HIR0_COUNT(structs);
  HIR0_COUNT(fields);
  HIR0_COUNT(type_declarations);
  HIR0_COUNT(aliases);
  HIR0_COUNT(types);
  HIR0_COUNT(functions);
  HIR0_COUNT(parameters);
  HIR0_COUNT(entries);
  HIR0_COUNT(statements);
  HIR0_COUNT(expressions);
  HIR0_COUNT(interpolation_segments);
  HIR0_COUNT(arguments);
  HIR0_COUNT(symbols);
  HIR0_COUNT(facts);
  HIR0_COUNT(diagnostics);
  HIR0_COUNT(diagnostic_facts);
  HIR0_COUNT(diagnostic_items);
  HIR0_COUNT(diagnostic_labels);
  HIR0_COUNT(receipt_bytes);
  HIR0_COUNT(enums);
  HIR0_COUNT(enum_cases);
  HIR0_COUNT(enum_case_parameters);
  HIR0_COUNT(switch_arms);
  HIR0_COUNT(pattern_captures);
  HIR0_COUNT(enum_subset_members);
  HIR0_COUNT(enum_membership_cases);
  HIR0_COUNT(generic_parameters);
  HIR0_COUNT(generic_applications);
  HIR0_COUNT(generic_arguments);
  HIR0_COUNT(typed_const_expressions);
  HIR0_COUNT(const_values);
  HIR0_COUNT(const_elements);
  HIR0_COUNT(const_bytes);
  HIR0_COUNT(const_declarations);
#undef HIR0_COUNT
  return true;
}

static bool hir_counts_equal(const w_seed_hir0_counts *left,
                             const w_seed_hir0_counts *right) {
  if (left == NULL || right == NULL) return false;
#define HIR0_COUNT(field) if (left->field != right->field) return false
  HIR0_COUNT(modules);
  HIR0_COUNT(identities);
  HIR0_COUNT(types);
  HIR0_COUNT(functions);
  HIR0_COUNT(parameters);
  HIR0_COUNT(blocks);
  HIR0_COUNT(block_arguments);
  HIR0_COUNT(edge_arguments);
  HIR0_COUNT(switch_edges);
  HIR0_COUNT(switch_captures);
  HIR0_COUNT(instructions);
  HIR0_COUNT(bindings);
  HIR0_COUNT(calls);
  HIR0_COUNT(host_parameters);
  HIR0_COUNT(arguments);
  HIR0_COUNT(enum_payloads);
  HIR0_COUNT(requirements);
  HIR0_COUNT(values);
  HIR0_COUNT(interpolation_segments);
  HIR0_COUNT(terminators);
  HIR0_COUNT(entries);
  HIR0_COUNT(text_bytes);
  HIR0_COUNT(value_bytes);
  HIR0_COUNT(external_modules);
  HIR0_COUNT(external_symbols);
  HIR0_COUNT(enums);
  HIR0_COUNT(enum_cases);
  HIR0_COUNT(enum_case_parameters);
  HIR0_COUNT(enum_subsets);
  HIR0_COUNT(enum_subset_members);
  HIR0_COUNT(receipt_bytes);
#undef HIR0_COUNT
  return true;
}

static bool frontend_array_ok(const void *pointer, size_t count,
                              size_t capacity, size_t element_size) {
  if (count > capacity || (count != 0u && pointer == NULL)) return false;
  return count == 0u || count <= SIZE_MAX / element_size;
}

static bool frontend_shape_ok(const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL)
    return false;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (result->status != W_SEED_FRONTEND_OK ||
      !text_is(result->schema_version, W_SEED_FRONTEND_SCHEMA_VERSION) ||
      !frontend_counts_equal(&result->required, &result->written) ||
      result->receipt_bytes != result->written.receipt_bytes ||
      result->written.receipt_bytes > output->receipt_capacity ||
      (result->written.receipt_bytes != 0u && output->receipt == NULL) ||
       frontend_input->documents == NULL || frontend_input->document_count == 0u ||
       frontend_input->document_count > W_SEED_FRONTEND_MAX_DOCUMENTS ||
      result->written.functions > W_SEED_FRONTEND_MAX_CST_NODES ||
      frontend_input->external_module_count >
          W_SEED_FRONTEND_MAX_EXTERNAL_MODULES ||
      (frontend_input->external_module_count != 0u &&
       frontend_input->external_modules == NULL) ||
      (frontend_input->resolved_import_count != 0u &&
       frontend_input->resolved_imports == NULL) ||
      (!frontend_input->import_resolution_complete &&
       frontend_input->resolved_import_count != 0u) ||
      (frontend_input->external_module_count != 0u &&
       !frontend_input->import_resolution_complete))
    return false;
#define HIR0_FRONTEND_ARRAY(field, capacity_field, type)                       \
  if (!frontend_array_ok(output->field, result->written.field,               \
                         output->capacity_field, sizeof(type))) return false
  HIR0_FRONTEND_ARRAY(modules, module_capacity, w_seed_frontend_module);
  HIR0_FRONTEND_ARRAY(imports, import_capacity, w_seed_frontend_import);
  HIR0_FRONTEND_ARRAY(import_items, import_item_capacity,
                      w_seed_frontend_import_item);
  HIR0_FRONTEND_ARRAY(enums, enum_capacity, w_seed_frontend_enum);
  HIR0_FRONTEND_ARRAY(enum_cases, enum_case_capacity,
                      w_seed_frontend_enum_case);
  HIR0_FRONTEND_ARRAY(enum_case_parameters, enum_case_parameter_capacity,
                      w_seed_frontend_enum_case_parameter);
  HIR0_FRONTEND_ARRAY(types, type_capacity, w_seed_frontend_type);
  HIR0_FRONTEND_ARRAY(functions, function_capacity, w_seed_frontend_function);
  HIR0_FRONTEND_ARRAY(parameters, parameter_capacity, w_seed_frontend_parameter);
  HIR0_FRONTEND_ARRAY(entries, entry_capacity, w_seed_frontend_entry);
  HIR0_FRONTEND_ARRAY(statements, statement_capacity, w_seed_frontend_statement);
  HIR0_FRONTEND_ARRAY(expressions, expression_capacity, w_seed_frontend_expression);
  HIR0_FRONTEND_ARRAY(interpolation_segments, interpolation_segment_capacity,
                      w_seed_frontend_interpolation_segment);
  HIR0_FRONTEND_ARRAY(arguments, argument_capacity, w_seed_frontend_argument);
  HIR0_FRONTEND_ARRAY(switch_arms, switch_arm_capacity,
                      w_seed_frontend_switch_arm);
  HIR0_FRONTEND_ARRAY(pattern_captures, pattern_capture_capacity,
                      w_seed_frontend_pattern_capture);
  HIR0_FRONTEND_ARRAY(enum_subset_members, enum_subset_member_capacity,
                      w_seed_frontend_enum_subset_member);
  HIR0_FRONTEND_ARRAY(enum_membership_cases, enum_membership_case_capacity,
                      w_seed_frontend_enum_membership_case);
  HIR0_FRONTEND_ARRAY(const_bytes, const_bytes_capacity, uint8_t);
#undef HIR0_FRONTEND_ARRAY
  for (size_t module = 0u;
       module < frontend_input->external_module_count; module += 1u) {
    const w_seed_frontend_external_module *external_module =
        &frontend_input->external_modules[module];
    if (!text_valid(external_module->module_id) ||
        external_module->symbol_count > W_SEED_FRONTEND_MAX_EXTERNAL_SYMBOLS ||
        (external_module->symbol_count != 0u &&
         external_module->symbols == NULL))
      return false;
    for (size_t symbol = 0u; symbol < external_module->symbol_count;
         symbol += 1u) {
      const w_seed_frontend_external_symbol *external_symbol =
          &external_module->symbols[symbol];
      if (!text_valid(external_symbol->name) ||
          !text_valid(external_symbol->return_type) ||
          !text_valid(external_symbol->receiver_type) ||
          external_symbol->parameter_count >
              W_SEED_FRONTEND_MAX_EXTERNAL_PARAMETERS ||
          (external_symbol->parameter_count != 0u &&
           external_symbol->parameters == NULL))
        return false;
      for (size_t parameter = 0u;
           parameter < external_symbol->parameter_count; parameter += 1u) {
        const w_seed_frontend_external_parameter *external_parameter =
            &external_symbol->parameters[parameter];
        if (!text_valid(external_parameter->name) ||
            !text_valid(external_parameter->type) ||
            external_parameter->label_kind > W_SEED_FRONTEND_LABEL_REQUIRED)
          return false;
      }
    }
  }
   if (result->written.modules != frontend_input->document_count ||
       result->written.functions == 0u ||
       result->written.entries != 1u || result->written.types == 0u ||
      frontend_input->host_scope == NULL ||
      frontend_input->host_scope->symbols == NULL ||
      frontend_input->host_scope->symbol_count == 0u ||
      frontend_input->host_scope->symbol_count > W_SEED_FRONTEND_MAX_HOST_SYMBOLS)
    return false;
  return true;
}

/* The external process validator below intentionally remains a closed
 * single-module ABI check.  HIR0's ordinary path, however, admits a bounded
 * resolver-owned local-document graph. Keep that path explicit so an
 * external-module ordinal can never be mistaken for a local document index. */
static bool frontend_local_import_records_ok(
    const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL)
    return false;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (frontend_input->external_module_count != 0u ||
      frontend_input->resolved_import_count != result->written.imports ||
      (result->written.imports != 0u &&
       (frontend_input->resolved_imports == NULL || output->imports == NULL)) ||
      (result->written.import_items != 0u && output->import_items == NULL))
    return false;
  if (!frontend_input->import_resolution_complete)
    return result->written.imports == 0u && result->written.import_items == 0u &&
           frontend_input->resolved_import_count == 0u;
  size_t import_cursor = 0u;
  size_t item_cursor = 0u;
  for (size_t module_index = 0u;
       module_index < result->written.modules; module_index += 1u) {
    const w_seed_frontend_module *module = &output->modules[module_index];
    if (module->document_index >= frontend_input->document_count ||
        module->first_import != import_cursor ||
        !range_valid(module->first_import, module->import_count,
                     result->written.imports) ||
        !range_valid(module->first_import, module->import_count,
                     frontend_input->resolved_import_count))
      return false;
    const w_seed_frontend_document *document =
        &frontend_input->documents[module->document_index];
    for (size_t ordinal = 0u; ordinal < module->import_count; ordinal += 1u) {
      const size_t import_index = import_cursor + ordinal;
      const w_seed_frontend_import *import = &output->imports[import_index];
      const w_seed_frontend_resolved_import *edge =
          &frontend_input->resolved_imports[import_index];
      if (import->module_index != module_index ||
          !text_valid(import->path) || import->path.length == 0u ||
          !text_valid(import->alias) ||
          !frontend_span_ok(document, import->span) ||
          import->direct_import_ordinal != ordinal ||
          import->target_kind != W_SEED_FRONTEND_IMPORT_LOCAL_DOCUMENT ||
          import->target_index >= frontend_input->document_count ||
          import->target_index == module->document_index ||
          edge->source_document_index != module->document_index ||
          edge->direct_import_ordinal != ordinal ||
          edge->target_kind != W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT ||
          edge->target_index != import->target_index ||
          !span_equal(edge->import_declaration_span, import->span))
        return false;
      const w_seed_frontend_module *target_module =
          &output->modules[import->target_index];
      if (!text_equal(import->path, target_module->module_id)) return false;
      if (!range_valid(import->first_item, import->item_count,
                       result->written.import_items) ||
          import->first_item != item_cursor)
        return false;
      for (size_t item_ordinal = 0u; item_ordinal < import->item_count;
           item_ordinal += 1u) {
        const w_seed_frontend_import_item *item =
            &output->import_items[item_cursor + item_ordinal];
        if (item->module_index != module_index || !text_valid(item->name) ||
            item->name.length == 0u || !text_valid(item->local_name) ||
            item->local_name.length == 0u ||
            !frontend_span_ok(document, item->span))
          return false;
        size_t target_matches = 0u;
        for (size_t function_index = target_module->first_function;
             function_index <
             (size_t)target_module->first_function + target_module->function_count;
             function_index += 1u) {
          const w_seed_frontend_function *target_function =
              &output->functions[function_index];
          if (target_function->exported &&
              text_equal(target_function->name, item->name))
            target_matches += 1u;
        }
        if (target_matches != 1u) return false;
      }
      if (!add_size(item_cursor, import->item_count, &item_cursor))
        return false;
    }
    if (!add_size(import_cursor, module->import_count, &import_cursor))
      return false;
  }
  if (import_cursor != result->written.imports ||
      item_cursor != result->written.import_items)
    return false;

  /* Resolve the local-document graph independently of the resolver's edge
   * walk. The fixed stacks are bounded by the public document limit and keep
   * cycle rejection allocation-free, including for forged frontend output. */
  uint8_t states[W_SEED_FRONTEND_MAX_DOCUMENTS] = {0u};
  size_t stack[W_SEED_FRONTEND_MAX_DOCUMENTS];
  size_t import_positions[W_SEED_FRONTEND_MAX_DOCUMENTS];
  for (size_t start = 0u; start < result->written.modules; start += 1u) {
    if (states[start] != 0u) continue;
    size_t depth = 0u;
    stack[0] = start;
    import_positions[0] = 0u;
    states[start] = 1u;
    for (;;) {
      const size_t current = stack[depth];
      const w_seed_frontend_module *module = &output->modules[current];
      bool descended = false;
      while (import_positions[depth] < module->import_count) {
        const size_t ordinal = import_positions[depth];
        import_positions[depth] += 1u;
        const size_t import_index = (size_t)module->first_import + ordinal;
        const size_t target = output->imports[import_index].target_index;
        if (states[target] == 1u) return false;
        if (states[target] == 0u) {
          if (depth + 1u >= result->written.modules) return false;
          depth += 1u;
          stack[depth] = target;
          import_positions[depth] = 0u;
          states[target] = 1u;
          descended = true;
          break;
        }
      }
      if (descended) continue;
      states[current] = 2u;
      if (depth == 0u) break;
      depth -= 1u;
    }
  }
  return true;
}

static bool frontend_external_process_records_ok(
    const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL)
    return false;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (frontend_input->external_module_count == 0u) {
    if (!frontend_input->import_resolution_complete)
      return frontend_input->resolved_import_count == 0u;
    return frontend_local_import_records_ok(input);
  }
  if (frontend_input->external_module_count != 1u ||
      frontend_input->resolved_import_count != 1u ||
      result->written.imports != 1u ||
      (result->written.import_items != 1u &&
       result->written.import_items != 3u) ||
      frontend_input->external_modules == NULL ||
      frontend_input->resolved_imports == NULL || output->imports == NULL ||
      output->import_items == NULL ||
      result->written.modules != 1u)
    return false;

  const w_seed_frontend_external_module *module =
      &frontend_input->external_modules[0];
  if (!text_is(module->module_id, HIR0_PROCESS_MODULE) ||
      module->symbols == NULL ||
      (module->symbol_count != 4u && module->symbol_count != 7u))
    return false;
  static const char *const symbol_names[] = {
      HIR0_PROCESS_ARGUMENTS, HIR0_PROCESS_CONTEXT, HIR0_PROCESS_EXIT_CODE,
      HIR0_PROCESS_SUCCESS, HIR0_PROCESS_IS_EMPTY, HIR0_PROCESS_FAILURE,
      HIR0_PROCESS_COUNT};
  for (size_t index = 0u; index < module->symbol_count; index += 1u) {
    const w_seed_frontend_external_symbol *symbol = &module->symbols[index];
    if (!text_is(symbol->name, symbol_names[index]) || !symbol->exported ||
        !text_valid(symbol->return_type) ||
        !text_valid(symbol->receiver_type))
      return false;
    if (index < 3u) {
      if (symbol->kind != W_SEED_FRONTEND_EXTERNAL_TYPE ||
          symbol->parameter_count != 0u || symbol->parameters != NULL ||
          !text_equal(symbol->return_type, symbol->name) ||
          symbol->is_const || symbol->receiver_type.length != 0u)
        return false;
    } else if (index == 3u) {
      if (symbol->kind != W_SEED_FRONTEND_EXTERNAL_VALUE ||
          symbol->parameter_count != 0u || symbol->parameters != NULL ||
          !symbol->is_const ||
          !text_is(symbol->return_type, HIR0_PROCESS_EXIT_CODE) ||
          !text_is(symbol->receiver_type, HIR0_PROCESS_EXIT_CODE))
        return false;
    } else if (index == 4u) {
      if (module->symbol_count != 7u ||
          symbol->kind != W_SEED_FRONTEND_EXTERNAL_VALUE ||
          symbol->parameter_count != 0u || symbol->parameters != NULL ||
          !symbol->is_const || !text_is(symbol->return_type, HIR0_BOOL_NAME) ||
          !text_is(symbol->receiver_type, HIR0_PROCESS_ARGUMENTS))
        return false;
    } else if (index == 5u) {
      if (module->symbol_count != 7u ||
          symbol->kind != W_SEED_FRONTEND_EXTERNAL_VALUE ||
          !symbol->is_const || symbol->parameter_count != 1u ||
          symbol->parameters == NULL ||
          !text_is(symbol->return_type, HIR0_PROCESS_EXIT_CODE) ||
          !text_is(symbol->receiver_type, HIR0_PROCESS_EXIT_CODE) ||
          !text_is(symbol->parameters[0].name, "code") ||
          !text_is(symbol->parameters[0].type, HIR0_I64_NAME) ||
          symbol->parameters[0].label_kind !=
              W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY)
        return false;
    } else if (index == 6u) {
      if (module->symbol_count != 7u ||
          symbol->kind != W_SEED_FRONTEND_EXTERNAL_VALUE ||
          symbol->parameter_count != 0u || symbol->parameters != NULL ||
          !symbol->is_const || !text_is(symbol->return_type, HIR0_USIZE_NAME) ||
          !text_is(symbol->receiver_type, HIR0_PROCESS_ARGUMENTS))
        return false;
    } else {
      return false;
    }
  }

  const w_seed_frontend_module *frontend_module = &output->modules[0];
  const w_seed_frontend_import *import = &output->imports[0];
  const bool flattened = result->written.import_items == 1u;
  if (frontend_module->first_import != 0u ||
      frontend_module->import_count != 1u || import->module_index != 0u ||
      !text_is(import->path, HIR0_PROCESS_MODULE) ||
      !text_valid(import->alias) || import->first_item != 0u ||
      import->item_count != (flattened ? 1u : 3u) ||
      import->direct_import_ordinal != 0u ||
      import->target_kind != W_SEED_FRONTEND_IMPORT_EXTERNAL_MODULE ||
      import->target_index != 0u ||
      !frontend_span_ok(&frontend_input->documents[0], import->span))
    return false;
  if (flattened) {
    const w_seed_frontend_import_item *item = &output->import_items[0];
    if (!text_is(import->alias, "std") || item->module_index != 0u ||
        !text_equal(item->name, import->alias) ||
        !text_equal(item->local_name, import->alias) ||
        !frontend_span_ok(&frontend_input->documents[0], item->span))
      return false;
  }
  const w_seed_frontend_resolved_import *edge =
      &frontend_input->resolved_imports[0];
  if (edge->source_document_index != 0u || edge->direct_import_ordinal != 0u ||
      edge->target_kind != W_SEED_FRONTEND_RESOLVED_IMPORT_EXTERNAL_MODULE ||
      edge->target_index != 0u || !span_equal(edge->import_declaration_span,
                                              import->span))
    return false;
  for (size_t index = 0u; !flattened && index < 3u; index += 1u) {
    const w_seed_frontend_import_item *item = &output->import_items[index];
    if (item->module_index != 0u ||
        !text_is(item->name, symbol_names[index]) ||
        !text_valid(item->local_name) || item->local_name.length == 0u ||
        !frontend_span_ok(&frontend_input->documents[0], item->span))
      return false;
    for (size_t prior = 0u; prior < index; prior += 1u)
      if (text_equal(item->local_name, output->import_items[prior].local_name))
        return false;
  }
  return true;
}

static bool frontend_external_exit_code_case_ok(
    const w_seed_hir0_input *input,
    const w_seed_frontend_expression *value) {
  if (input == NULL || value == NULL ||
      !frontend_external_process_records_ok(input) ||
      value->kind != W_SEED_FRONTEND_EXPR_ENUM_CASE || !value->supported ||
      value->enum_index != W_SEED_FRONTEND_NONE ||
      value->enum_case_index != W_SEED_FRONTEND_NONE ||
      value->resolved_callee_kind !=
          W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL ||
      value->resolved_external_module_index != 0u ||
      value->resolved_external_symbol_index != 3u ||
      value->member_name.length != sizeof(HIR0_PROCESS_SUCCESS) - 1u ||
      !text_is(value->member_name, HIR0_PROCESS_SUCCESS) ||
      value->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE ||
      value->resolved_function_index != W_SEED_FRONTEND_NONE ||
      value->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
      value->resolved_local_ordinal != W_SEED_FRONTEND_NONE ||
      value->resolved_const_declaration != W_SEED_FRONTEND_NONE ||
      value->first_switch_arm != W_SEED_FRONTEND_NONE ||
      value->switch_arm_count != 0u ||
      value->first_membership_case != W_SEED_FRONTEND_NONE ||
      value->membership_case_count != 0u ||
      value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
      value->const_byte_offset != W_SEED_FRONTEND_NONE ||
      value->const_byte_count != 0u || value->has_bool_value ||
      value->has_integer_value || value->first_interpolation_segment !=
                                       W_SEED_FRONTEND_NONE ||
      value->interpolation_segment_count != 0u || value->left !=
                                                     W_SEED_FRONTEND_NONE ||
      value->right != W_SEED_FRONTEND_NONE ||
      value->inferred_type == W_SEED_FRONTEND_NONE ||
      (size_t)value->inferred_type >= input->frontend_result->written.types)
    return false;
  const w_seed_frontend_type *type =
      &input->frontend_output->types[value->inferred_type];
  return type->kind == W_SEED_FRONTEND_TYPE_NOMINAL &&
         type->external_module_index == 0u &&
         type->external_symbol_index == 2u;
}

static bool frontend_external_enum_case_value_ok(
    const w_seed_hir0_input *input,
    const w_seed_frontend_expression *value, uint32_t symbol_index) {
  if (input == NULL || value == NULL ||
      !frontend_external_process_records_ok(input) ||
      input->frontend_input->external_modules[0].symbol_count != 7u ||
      (symbol_index != 3u && symbol_index != 5u) ||
      value->kind != W_SEED_FRONTEND_EXPR_ENUM_CASE || !value->supported ||
      value->enum_index != W_SEED_FRONTEND_NONE ||
      value->enum_case_index != W_SEED_FRONTEND_NONE ||
      value->resolved_callee_kind !=
          W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL ||
      value->resolved_external_module_index != 0u ||
      value->resolved_external_symbol_index != symbol_index ||
      value->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE ||
      value->resolved_function_index != W_SEED_FRONTEND_NONE ||
      value->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
      value->resolved_local_ordinal != W_SEED_FRONTEND_NONE ||
      value->resolved_const_declaration != W_SEED_FRONTEND_NONE ||
      value->resolved_pattern_capture != W_SEED_FRONTEND_NONE ||
      value->first_switch_arm != W_SEED_FRONTEND_NONE ||
      value->switch_arm_count != 0u ||
      value->first_membership_case != W_SEED_FRONTEND_NONE ||
      value->membership_case_count != 0u ||
      value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
      value->const_byte_offset != W_SEED_FRONTEND_NONE ||
      value->const_byte_count != 0u || value->has_bool_value ||
      value->has_integer_value ||
      value->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
      value->interpolation_segment_count != 0u ||
      value->left != W_SEED_FRONTEND_NONE ||
      value->right != W_SEED_FRONTEND_NONE ||
      value->first_argument != W_SEED_FRONTEND_NONE ||
      value->argument_count != 0u || !text_valid(value->member_name) ||
      value->member_name.length == 0u ||
      (size_t)value->inferred_type >= input->frontend_result->written.types ||
      value->inferred_type == W_SEED_FRONTEND_NONE)
    return false;
  const w_seed_frontend_external_symbol *symbol =
      &input->frontend_input->external_modules[0].symbols[symbol_index];
  const w_seed_frontend_type *type =
      &input->frontend_output->types[value->inferred_type];
  return text_equal(value->member_name, symbol->name) &&
         type->kind == W_SEED_FRONTEND_TYPE_NOMINAL &&
         type->external_module_index == 0u &&
         type->external_symbol_index == 2u;
}

static bool host_shape_ok(const w_seed_frontend_host_prelude *scope) {
  if (scope == NULL || !text_valid(scope->profile) || scope->profile.length == 0u ||
      scope->symbols == NULL || scope->symbol_count == 0u ||
      scope->symbol_count > W_SEED_FRONTEND_MAX_HOST_SYMBOLS)
    return false;
  size_t parameters = 0u;
  for (size_t index = 0u; index < scope->symbol_count; index += 1u) {
    const w_seed_frontend_host_prelude_symbol *symbol = &scope->symbols[index];
    if (!text_valid(symbol->name) || symbol->name.length == 0u ||
        symbol->kind != W_SEED_FRONTEND_EXTERNAL_VALUE ||
        !text_valid(symbol->return_type) || !text_is(symbol->return_type, "()") ||
        symbol->parameter_count > W_SEED_FRONTEND_MAX_HOST_PARAMETERS ||
        (symbol->parameter_count != 0u && symbol->parameters == NULL) ||
        symbol->requirement_count > W_SEED_FRONTEND_MAX_HOST_REQUIREMENTS ||
        (symbol->requirement_count != 0u && symbol->requirements == NULL) ||
        !add_size(parameters, symbol->parameter_count, &parameters) ||
        parameters > W_SEED_FRONTEND_MAX_HOST_PARAMETERS)
      return false;
    for (size_t prior = 0u; prior < index; prior += 1u)
      if (text_equal(symbol->name, scope->symbols[prior].name)) return false;
    for (size_t parameter = 0u; parameter < symbol->parameter_count;
         parameter += 1u) {
      const w_seed_frontend_external_parameter *value =
          &symbol->parameters[parameter];
      if (!text_valid(value->name) || !text_valid(value->type) ||
          !text_is(value->type, HIR0_STRING_NAME) ||
          value->label_kind > W_SEED_FRONTEND_LABEL_REQUIRED ||
          (value->label_kind != W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
           value->name.length == 0u))
        return false;
    }
    for (size_t requirement = 0u; requirement < symbol->requirement_count;
         requirement += 1u) {
      const w_seed_frontend_host_requirement *value =
          &symbol->requirements[requirement];
      if (!text_valid(value->name) || value->name.length == 0u) return false;
      for (size_t prior = 0u; prior < requirement; prior += 1u)
        if (text_equal(value->name, symbol->requirements[prior].name))
          return false;
    }
  }
  return true;
}

static bool frontend_span_ok(const w_seed_frontend_document *document,
                             w_seed_span span) {
  return document != NULL && document->source != NULL &&
         w_seed_source_validate_span(document->source, span, NULL);
}

static bool frontend_sources_ok(const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_input->documents == NULL)
    return false;
  for (size_t index = 0u; index < input->frontend_input->document_count;
       index += 1u) {
    const w_seed_frontend_document *document =
        &input->frontend_input->documents[index];
    if (document->source == NULL) return false;
    const w_seed_byte_view bytes = w_seed_source_bytes(document->source);
    if (bytes.length != 0u && bytes.data == NULL) return false;
  }
  return true;
}

/* The bounded enum surface owns dense case and case-parameter ranges plus a
 * canonical TYPE record. HIR0 admits only the scalar payload types that the
 * native lowering can carry without allocation. This check runs before any
 * HIR output is touched, so forged ownership/ranges are rejected
 * transactionally. */
static bool frontend_local_enum_records_ok(const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (result->written.enums == 0u)
    return result->written.enum_cases == 0u &&
           result->written.enum_case_parameters == 0u;
  if (output->enums == NULL || output->enum_cases == NULL ||
      result->written.enum_cases == 0u)
    return false;
  size_t case_cursor = 0u;
  size_t payload_cursor = 0u;
  for (size_t index = 0u; index < result->written.enums; index += 1u) {
    const w_seed_frontend_enum *decl = &output->enums[index];
    if (decl->module_index >= result->written.modules ||
        !text_valid(decl->name) || decl->name.length == 0u ||
        decl->has_generic_parameters ||
        decl->conformance_type != W_SEED_FRONTEND_NONE ||
        decl->first_case != case_cursor ||
        !range_valid(decl->first_case, decl->case_count,
                     result->written.enum_cases) ||
        decl->case_count == 0u || decl->type_index == W_SEED_FRONTEND_NONE ||
        (size_t)decl->type_index >= result->written.types ||
        !frontend_span_ok(&input->frontend_input->documents[
                              output->modules[decl->module_index].document_index],
                          decl->span))
      return false;
    const w_seed_frontend_type *type = &output->types[decl->type_index];
    if (type->kind != W_SEED_FRONTEND_TYPE_ENUM ||
        type->enum_base_index != index ||
        type->first_subset_member != W_SEED_FRONTEND_NONE ||
        type->subset_member_count != 0u ||
        !text_equal(type->spelling, decl->name))
      return false;
    for (size_t prior = 0u; prior < index; prior += 1u)
      if (output->enums[prior].module_index == decl->module_index &&
          text_equal(output->enums[prior].name, decl->name))
        return false;
    for (size_t ordinal = 0u; ordinal < decl->case_count; ordinal += 1u) {
      const size_t case_index = case_cursor + ordinal;
      const w_seed_frontend_enum_case *value = &output->enum_cases[case_index];
      if (value->module_index != decl->module_index ||
          value->owner_enum != index || !text_valid(value->name) ||
          value->name.length == 0u || value->first_payload != payload_cursor ||
          !range_valid(value->first_payload, value->payload_count,
                       result->written.enum_case_parameters) ||
          !frontend_span_ok(&input->frontend_input->documents[
                                output->modules[decl->module_index].document_index],
                            value->span))
        return false;
      for (size_t prior = 0u; prior < ordinal; prior += 1u)
        if (text_equal(value->name,
                       output->enum_cases[case_cursor + prior].name))
          return false;
      for (size_t payload_ordinal = 0u;
           payload_ordinal < value->payload_count; payload_ordinal += 1u) {
        const size_t payload_index = payload_cursor + payload_ordinal;
        const w_seed_frontend_enum_case_parameter *payload =
            &output->enum_case_parameters[payload_index];
        if (payload->module_index != decl->module_index ||
            payload->owner_case != case_index || !text_valid(payload->label) ||
            payload->has_label != (payload->label.length != 0u) ||
            payload->type_index == W_SEED_FRONTEND_NONE ||
            (size_t)payload->type_index >= result->written.types ||
            !frontend_hir_type_supported(
                input, &output->types[payload->type_index]) ||
            !frontend_span_ok(
                &input->frontend_input->documents[
                    output->modules[decl->module_index].document_index],
                payload->span))
          return false;
        if (payload->has_label)
          for (size_t prior = 0u; prior < payload_ordinal; prior += 1u) {
            const w_seed_frontend_enum_case_parameter *previous =
                &output->enum_case_parameters[payload_cursor + prior];
            if (previous->has_label &&
                text_equal(previous->label, payload->label))
              return false;
          }
      }
      if (!add_size(payload_cursor, value->payload_count, &payload_cursor))
        return false;
    }
    if (!add_size(case_cursor, decl->case_count, &case_cursor)) return false;
  }
  return case_cursor == result->written.enum_cases &&
         payload_cursor == result->written.enum_case_parameters;
}

/* Validate the frontend's normalized subset projection before HIR emission.
 * Every member range is dense, caller-owned, and strictly increasing in the
 * base enum's declaration order. This also closes the first HIR slice to
 * payloadless cases: payload constructors remain a later feature. */
static bool frontend_local_enum_subset_records_ok(
    const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (result->written.enum_subset_members != 0u &&
      output->enum_subset_members == NULL)
    return false;
  size_t member_cursor = 0u;
  for (size_t type_index = 0u; type_index < result->written.types;
       type_index += 1u) {
    const w_seed_frontend_type *type = &output->types[type_index];
    if (type->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET) continue;
    if (!frontend_local_enum_subset_type_supported(input, type) ||
        type->first_subset_member != member_cursor ||
        type->subset_member_count == 0u ||
        !range_valid(type->first_subset_member, type->subset_member_count,
                     result->written.enum_subset_members))
      return false;
    const w_seed_frontend_enum *decl =
        &output->enums[type->enum_base_index];
    const size_t document_index = output->modules[decl->module_index].document_index;
    if (type->subset_member_count == decl->case_count) return false;
    for (size_t case_ordinal = 0u; case_ordinal < decl->case_count;
         case_ordinal += 1u)
      if (output->enum_cases[(size_t)decl->first_case + case_ordinal]
              .payload_count != 0u)
        return false;
    uint32_t previous_case = W_SEED_FRONTEND_NONE;
    for (size_t ordinal = 0u; ordinal < type->subset_member_count; ordinal += 1u) {
      const size_t member_index = member_cursor + ordinal;
      const w_seed_frontend_enum_subset_member *member =
          &output->enum_subset_members[member_index];
      if (member->owner_type != type_index ||
          member->enum_base_index != type->enum_base_index ||
          member->enum_case_index == W_SEED_FRONTEND_NONE ||
          (size_t)member->enum_case_index >= result->written.enum_cases ||
          (size_t)member->enum_case_index < decl->first_case ||
          (size_t)member->enum_case_index >=
              (size_t)decl->first_case + decl->case_count ||
          (previous_case != W_SEED_FRONTEND_NONE &&
           previous_case >= member->enum_case_index) ||
          output->enum_cases[member->enum_case_index].payload_count != 0u ||
          !frontend_span_ok(&input->frontend_input->documents[document_index],
                            member->source_span))
        return false;
      previous_case = member->enum_case_index;
    }
    if (!add_size(member_cursor, type->subset_member_count, &member_cursor))
      return false;
  }
  return member_cursor == result->written.enum_subset_members;
}

/* Type records are arena entries, not declarations: builtin and nominal
 * records are owned by the document whose function/statement/expression
 * references them. Derive that owner from normalized relations before
 * checking the source span. This avoids treating documents[0] as a global
 * source for a type emitted by an imported module. */
static bool frontend_type_owner_module(const w_seed_hir0_input *input,
                                       size_t type_index,
                                       size_t *owner_module) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || owner_module == NULL ||
      type_index >= input->frontend_result->written.types)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  const w_seed_frontend_type *type = &output->types[type_index];
  const bool shared_synthetic_builtin =
      type->span.start_byte == 0u && type->span.end_byte == 0u &&
      (frontend_type_supported(type) || frontend_type_is_usize(type));
  bool found = false;
  size_t owner = 0u;
#define HIR0_TYPE_OWNER(candidate)                                             \
  do {                                                                         \
    const size_t hir0_candidate_module = (candidate);                         \
    if (hir0_candidate_module >= result->written.modules) return false;       \
    if (found && owner != hir0_candidate_module &&                             \
        !shared_synthetic_builtin)                                             \
      return false;                                                            \
    owner = hir0_candidate_module;                                             \
    found = true;                                                              \
  } while (0)
  if (type->kind == W_SEED_FRONTEND_TYPE_ENUM ||
      type->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET) {
    if (type->enum_base_index == W_SEED_FRONTEND_NONE ||
        (size_t)type->enum_base_index >= result->written.enums)
      return false;
    HIR0_TYPE_OWNER(output->enums[type->enum_base_index].module_index);
  }
  for (size_t index = 0u; index < result->written.enums; index += 1u) {
    const w_seed_frontend_enum *decl = &output->enums[index];
    if (decl->type_index == type_index) HIR0_TYPE_OWNER(decl->module_index);
  }
  for (size_t index = 0u; index < result->written.enum_case_parameters;
       index += 1u) {
    const w_seed_frontend_enum_case_parameter *parameter =
        &output->enum_case_parameters[index];
    if (parameter->type_index == type_index) HIR0_TYPE_OWNER(
        parameter->module_index);
  }
  for (size_t index = 0u; index < result->written.aliases; index += 1u)
    if (output->aliases[index].type_index == type_index)
      HIR0_TYPE_OWNER(output->aliases[index].module_index);
  for (size_t index = 0u; index < result->written.type_declarations; index += 1u)
    if (output->type_declarations[index].type_index == type_index)
      HIR0_TYPE_OWNER(output->type_declarations[index].module_index);
  for (size_t index = 0u; index < result->written.fields; index += 1u)
    if (output->fields[index].type_index == type_index)
      HIR0_TYPE_OWNER(output->fields[index].module_index);
  for (size_t index = 0u; index < result->written.const_declarations; index += 1u) {
    const w_seed_frontend_const_declaration *decl =
        &output->const_declarations[index];
    if (decl->declared_type == type_index || decl->effective_type == type_index)
      HIR0_TYPE_OWNER(decl->module_index);
  }
  for (size_t index = 0u; index < result->written.functions; index += 1u) {
    const w_seed_frontend_function *function = &output->functions[index];
    if (function->return_type == type_index)
      HIR0_TYPE_OWNER(function->module_index);
    for (size_t ordinal = 0u; ordinal < function->parameter_count; ordinal += 1u) {
      const size_t parameter_index = (size_t)function->first_parameter + ordinal;
      if (parameter_index >= result->written.parameters) return false;
      if (output->parameters[parameter_index].type_index == type_index)
        HIR0_TYPE_OWNER(function->module_index);
    }
  }
  for (size_t index = 0u; index < result->written.statements; index += 1u) {
    const w_seed_frontend_statement *statement = &output->statements[index];
    if (statement->module_index >= result->written.modules) return false;
    if (statement->declared_type == type_index ||
        statement->effective_type == type_index)
      HIR0_TYPE_OWNER(statement->module_index);
  }
  for (size_t index = 0u; index < result->written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &output->expressions[index];
    if (expression->module_index >= result->written.modules) return false;
    if (expression->inferred_type == type_index)
      HIR0_TYPE_OWNER(expression->module_index);
  }
#undef HIR0_TYPE_OWNER
  if (!found) {
    if (!shared_synthetic_builtin) return false;
    owner = 0u;
  }
  *owner_module = owner;
  return true;
}

static bool frontend_type_records_ok(const w_seed_hir0_input *input) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (!frontend_local_enum_records_ok(input) ||
      !frontend_local_enum_subset_records_ok(input))
    return false;
  for (size_t index = 0u; index < result->written.types; index += 1u) {
    const w_seed_frontend_type *type = &output->types[index];
    size_t owner_module = 0u;
    if (!frontend_hir_type_supported(input, type) ||
        !frontend_type_owner_module(input, index, &owner_module) ||
        owner_module >= result->written.modules ||
        !frontend_span_ok(
            &input->frontend_input
                 ->documents[output->modules[owner_module].document_index],
            type->span))
      return false;
  }
  return true;
}

static bool frontend_enum_subset_range_ok(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, const w_seed_frontend_type *type) {
  return output != NULL && result != NULL && type != NULL &&
         type->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET &&
         type->first_subset_member != W_SEED_FRONTEND_NONE &&
         type->subset_member_count != 0u && output->enum_subset_members != NULL &&
         range_valid(type->first_subset_member, type->subset_member_count,
                     result->written.enum_subset_members);
}

static bool frontend_enum_subset_same_set(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, const w_seed_frontend_type *left,
    const w_seed_frontend_type *right) {
  if (!frontend_enum_subset_range_ok(output, result, left) ||
      !frontend_enum_subset_range_ok(output, result, right) ||
      left->enum_base_index != right->enum_base_index ||
      left->subset_member_count != right->subset_member_count)
    return false;
  for (size_t ordinal = 0u; ordinal < left->subset_member_count;
       ordinal += 1u)
    if (output->enum_subset_members[(size_t)left->first_subset_member + ordinal]
            .enum_case_index !=
        output->enum_subset_members[(size_t)right->first_subset_member + ordinal]
            .enum_case_index)
      return false;
  return true;
}

/* Distinct aliases that normalize to the same (base enum, declaration-order
 * case sequence) are one semantic HIR type. The first frontend occurrence is
 * the canonical representative; later aliases retain provenance in the
 * frontend only and map to that same caller-owned HIR record. */
static bool frontend_enum_subset_canonical(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t target_index,
    size_t *canonical_index, size_t *canonical_ordinal) {
  if (output == NULL || result == NULL || canonical_index == NULL ||
      canonical_ordinal == NULL || target_index >= result->written.types ||
      output->types == NULL ||
      !frontend_enum_subset_range_ok(output, result, &output->types[target_index]))
    return false;
  size_t ordinal = 0u;
  for (size_t candidate = 0u; candidate <= target_index; candidate += 1u) {
    const w_seed_frontend_type *candidate_type = &output->types[candidate];
    if (!frontend_enum_subset_range_ok(output, result, candidate_type))
      continue;
    bool has_prior = false;
    for (size_t prior = 0u; prior < candidate; prior += 1u)
      if (frontend_enum_subset_same_set(output, result, candidate_type,
                                        &output->types[prior])) {
        has_prior = true;
        break;
      }
    if (has_prior) continue;
    if (candidate == target_index) {
      *canonical_index = candidate;
      *canonical_ordinal = ordinal;
      return true;
    }
    ordinal += 1u;
  }
  for (size_t candidate = 0u; candidate < target_index; candidate += 1u) {
    if (!frontend_enum_subset_same_set(output, result, &output->types[target_index],
                                       &output->types[candidate]))
      continue;
    size_t canonical_ordinal_value = 0u;
    for (size_t unique = 0u; unique < candidate; unique += 1u) {
      const w_seed_frontend_type *unique_type = &output->types[unique];
      if (!frontend_enum_subset_range_ok(output, result, unique_type))
        continue;
      bool has_prior = false;
      for (size_t prior = 0u; prior < unique; prior += 1u)
        if (frontend_enum_subset_same_set(output, result, unique_type,
                                          &output->types[prior])) {
          has_prior = true;
          break;
        }
      if (!has_prior) canonical_ordinal_value += 1u;
    }
    *canonical_index = candidate;
    *canonical_ordinal = canonical_ordinal_value;
    return true;
  }
  return false;
}

/* Task is a frontend-only virtual value in Async0. It may appear only as an
 * inferred immutable launch binding and as the identifier consumed directly
 * by its await wrapper. Signatures, payloads, annotations, copies, arguments,
 * and other expression positions must not be scalarized accidentally. */
static bool frontend_task_usage_ok(const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  for (size_t function = 0u; function < result->written.functions; function += 1u) {
    const w_seed_frontend_function *value = &output->functions[function];
    if (value->return_type < result->written.types &&
        output->types[value->return_type].kind == W_SEED_FRONTEND_TYPE_TASK)
      return false;
  }
  for (size_t parameter = 0u; parameter < result->written.parameters;
       parameter += 1u)
    if (output->parameters[parameter].type_index < result->written.types &&
        output->types[output->parameters[parameter].type_index].kind ==
            W_SEED_FRONTEND_TYPE_TASK)
      return false;
  for (size_t payload = 0u; payload < result->written.enum_case_parameters;
       payload += 1u)
    if (output->enum_case_parameters[payload].type_index < result->written.types &&
        output->types[output->enum_case_parameters[payload].type_index].kind ==
            W_SEED_FRONTEND_TYPE_TASK)
      return false;
  for (size_t statement = 0u; statement < result->written.statements;
       statement += 1u) {
    const w_seed_frontend_statement *value = &output->statements[statement];
    if (value->declared_type < result->written.types &&
        output->types[value->declared_type].kind == W_SEED_FRONTEND_TYPE_TASK)
      return false;
    if (value->effective_type >= result->written.types ||
        output->types[value->effective_type].kind != W_SEED_FRONTEND_TYPE_TASK)
      continue;
    if (value->kind != W_SEED_FRONTEND_STMT_LET ||
        value->declared_type != W_SEED_FRONTEND_NONE ||
        value->expression_index >= result->written.expressions ||
        output->expressions[value->expression_index].kind !=
            W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH)
      return false;
  }
  for (size_t expression = 0u; expression < result->written.expressions;
       expression += 1u) {
    const w_seed_frontend_expression *value = &output->expressions[expression];
    if (value->inferred_type >= result->written.types ||
        output->types[value->inferred_type].kind != W_SEED_FRONTEND_TYPE_TASK)
      continue;
    if (value->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH) continue;
    if (value->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
        expression + 1u >= result->written.expressions)
      return false;
    const w_seed_frontend_expression *consumer =
        &output->expressions[expression + 1u];
    if (consumer->kind != W_SEED_FRONTEND_EXPR_AWAIT ||
        consumer->left != expression ||
        consumer->task_binding_statement != value->resolved_binding_statement)
      return false;
  }
  return true;
}

/* A well-formed Task whose result is outside Async0's scalar subset is a
 * closed unsupported family, not a malformed frontend record.  Keep malformed
 * Task links on the ordinary invalid-input path below. */
static bool frontend_task_result_is_unsupported(
    const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || input->frontend_output->types == NULL)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  for (size_t index = 0u; index < result->written.types; index += 1u) {
    const w_seed_frontend_type *type = &output->types[index];
    if (type->kind != W_SEED_FRONTEND_TYPE_TASK) continue;
    if (!text_is(type->spelling, "Task") ||
        type->task_result_type == W_SEED_FRONTEND_NONE ||
        type->element_type != type->task_result_type ||
        (size_t)type->task_result_type >= result->written.types)
      continue;
    const w_seed_frontend_type *task_result =
        &output->types[type->task_result_type];
    if (task_result->kind == W_SEED_FRONTEND_TYPE_BOOL ||
        (task_result->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
         task_result->is_signed && task_result->bit_width == 64u))
      continue;
    return true;
  }
  return false;
}

typedef struct {
  uint32_t function_indices[W_SEED_HIR0_MAX_NESTING];
  size_t count;
} frontend_elision_path;

/* Preflight the body proof used by a structured scalar elision before HIR
 * output is written.  Direct-entry facts are published only after emission,
 * so this small frontend-side proof prevents a later verifier rejection from
 * partially committing caller-owned output.  It is intentionally conservative:
 * the bounded seed admits scalar pure local calls only. */
static bool frontend_elision_scalar_type_ok(
    const w_seed_hir0_input *input, uint32_t type_index) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || type_index == W_SEED_FRONTEND_NONE ||
      (size_t)type_index >= input->frontend_result->written.types)
    return false;
  const w_seed_frontend_type *type =
      &input->frontend_output->types[type_index];
  return type->kind == W_SEED_FRONTEND_TYPE_UNIT ||
         type->kind == W_SEED_FRONTEND_TYPE_BOOL ||
         (type->kind == W_SEED_FRONTEND_TYPE_INTEGER && type->is_signed &&
          type->bit_width == 64u);
}

static bool frontend_elision_task_value_type_ok(
    const w_seed_hir0_input *input, uint32_t type_index) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || type_index == W_SEED_FRONTEND_NONE ||
      (size_t)type_index >= input->frontend_result->written.types)
    return false;
  const w_seed_frontend_type *type =
      &input->frontend_output->types[type_index];
  return type->kind == W_SEED_FRONTEND_TYPE_BOOL ||
         (type->kind == W_SEED_FRONTEND_TYPE_INTEGER && type->is_signed &&
          type->bit_width == 64u);
}

static bool frontend_elision_function_never(
    const w_seed_hir0_input *input, size_t function_index,
    frontend_elision_path *path, size_t depth) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || path == NULL ||
      function_index >= input->frontend_result->written.functions ||
      function_index >= W_SEED_FRONTEND_MAX_CST_NODES ||
      depth >= W_SEED_HIR0_MAX_NESTING ||
      path->count >= W_SEED_HIR0_MAX_NESTING)
    return false;
  for (size_t active = 0u; active < path->count; active += 1u)
    if (path->function_indices[active] == function_index) return false;
  path->function_indices[path->count] = (uint32_t)function_index;
  path->count += 1u;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  const w_seed_frontend_function *function =
      &output->functions[function_index];
  bool valid = !function->is_throws && !function->is_unsafe &&
               !function->has_borrow_clause &&
               frontend_elision_scalar_type_ok(input, function->return_type);
  for (size_t parameter = 0u;
       valid && parameter < function->parameter_count; parameter += 1u) {
    const size_t parameter_index = (size_t)function->first_parameter + parameter;
    valid = parameter_index < result->written.parameters &&
            frontend_elision_scalar_type_ok(
                input, output->parameters[parameter_index].type_index);
  }
  if (valid &&
      !range_valid(function->first_statement, function->statement_count,
                   result->written.statements))
    valid = false;
  for (size_t statement = 0u;
       valid && statement < result->written.statements; statement += 1u) {
    const w_seed_frontend_statement *value = &output->statements[statement];
    if (value->owner_function != function_index) continue;
    if (value->kind == W_SEED_FRONTEND_STMT_UNSUPPORTED)
      valid = false;
    else if ((value->kind == W_SEED_FRONTEND_STMT_LET ||
              value->kind == W_SEED_FRONTEND_STMT_VAR) &&
             (value->effective_type == W_SEED_FRONTEND_NONE ||
              !frontend_elision_scalar_type_ok(input, value->effective_type)))
      valid = false;
  }
  for (size_t expression = 0u;
       valid && expression < result->written.expressions; expression += 1u) {
    const w_seed_frontend_expression *value = &output->expressions[expression];
    if (value->owner_function != function_index) continue;
    if (!value->supported || value->inferred_type == W_SEED_FRONTEND_NONE ||
        !frontend_elision_scalar_type_ok(input, value->inferred_type) ||
        value->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH ||
        value->kind == W_SEED_FRONTEND_EXPR_AWAIT ||
        value->kind == W_SEED_FRONTEND_EXPR_EXECUTION_YIELD) {
      valid = false;
      break;
    }
    if (value->kind != W_SEED_FRONTEND_EXPR_CALL) continue;
    if (value->resolved_callee_kind !=
            W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION ||
        value->resolved_function_index == W_SEED_FRONTEND_NONE ||
        (size_t)value->resolved_function_index >= result->written.functions) {
      valid = false;
      break;
    }
    const w_seed_frontend_function *callee =
        &output->functions[value->resolved_function_index];
    if (callee->is_async || callee->is_anonymous_entry ||
        !frontend_elision_function_never(input, value->resolved_function_index,
                                         path, depth + 1u))
      valid = false;
  }
  path->count -= 1u;
  return valid;
}

/* Prove a suspended virtual-Task slice without pretending the common async
 * entry is synchronous. The target owns one or more root-level
 * `await execution#yield()` markers, scalar values only, and otherwise only
 * pure scalar expressions and bindings; nested calls remain outside this
 * proof. Yield is a scheduling opportunity rather than an observable barrier,
 * so a closed launch/join scope may discharge each marker without allocating
 * a Task/frame. */
static bool frontend_elision_function_static_yields(
    const w_seed_hir0_input *input, size_t function_index) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL ||
      function_index >= input->frontend_result->written.functions)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  const w_seed_frontend_function *function = &output->functions[function_index];
  bool valid = function->is_async && !function->is_const &&
               !function->is_throws && !function->is_unsafe &&
               !function->has_borrow_clause && !function->is_anonymous_entry &&
               frontend_elision_task_value_type_ok(input,
                                                    function->return_type) &&
               range_valid(function->first_statement, function->statement_count,
                           result->written.statements);
  size_t yield_count = 0u;
  for (size_t parameter = 0u;
       valid && parameter < function->parameter_count; parameter += 1u) {
    const size_t parameter_index = (size_t)function->first_parameter + parameter;
    valid = parameter_index < result->written.parameters &&
            frontend_elision_task_value_type_ok(
                input, output->parameters[parameter_index].type_index);
  }
  for (size_t statement = 0u;
       valid && statement < result->written.statements; statement += 1u) {
    const w_seed_frontend_statement *value = &output->statements[statement];
    if (value->owner_function != function_index) continue;
    if (value->kind == W_SEED_FRONTEND_STMT_UNSUPPORTED ||
        value->kind == W_SEED_FRONTEND_STMT_IF ||
        value->kind == W_SEED_FRONTEND_STMT_WHILE ||
        value->kind == W_SEED_FRONTEND_STMT_REPEAT ||
        value->kind == W_SEED_FRONTEND_STMT_FOR ||
        value->kind == W_SEED_FRONTEND_STMT_GUARD)
      valid = false;
    else if ((value->kind == W_SEED_FRONTEND_STMT_LET ||
              value->kind == W_SEED_FRONTEND_STMT_VAR) &&
             (value->effective_type == W_SEED_FRONTEND_NONE ||
              !frontend_elision_task_value_type_ok(input,
                                                    value->effective_type)))
      valid = false;
  }
  for (size_t expression = 0u;
       valid && expression < result->written.expressions; expression += 1u) {
    const w_seed_frontend_expression *value = &output->expressions[expression];
    if (value->owner_function != function_index) continue;
    if (value->kind == W_SEED_FRONTEND_EXPR_EXECUTION_YIELD) {
      yield_count += 1u;
      continue;
    }
    if (!value->supported || value->inferred_type == W_SEED_FRONTEND_NONE ||
        !frontend_elision_task_value_type_ok(input, value->inferred_type) ||
        value->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH ||
        value->kind == W_SEED_FRONTEND_EXPR_AWAIT) {
      valid = false;
      break;
    }
    /* The static-yield witness has no nested calls. This keeps the
     * source preflight and independent HIR proof identical; ordinary callees
     * can be admitted later with a fixed-point proof of the composed body. */
    if (value->kind == W_SEED_FRONTEND_EXPR_CALL) valid = false;
  }
  return valid && yield_count != 0u;
}

/* Existing ordinary targets retain the W-1577 path and are checked by HIR's
 * suspension proof. An explicit async target additionally requires this
 * preflight because its direct-entry fact is published only after emission. */
static bool frontend_structured_elision_preflight(
    const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  bool has_launch = false;
  for (size_t expression = 0u;
       expression < result->written.expressions; expression += 1u)
    if (output->expressions[expression].kind ==
        W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH) {
      has_launch = true;
      break;
    }
  if (!has_launch) return true;
  frontend_elision_path path = {0};
  for (size_t expression = 0u;
       expression < result->written.expressions; expression += 1u) {
    const w_seed_frontend_expression *launch =
        &output->expressions[expression];
    if (launch->kind != W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH) continue;
    if (launch->task_call_expression == W_SEED_FRONTEND_NONE ||
        (size_t)launch->task_call_expression >= result->written.expressions)
      return false;
    const w_seed_frontend_expression *call =
        &output->expressions[launch->task_call_expression];
    if (call->kind != W_SEED_FRONTEND_EXPR_CALL ||
        call->resolved_callee_kind !=
            W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION ||
        call->resolved_function_index == W_SEED_FRONTEND_NONE ||
        (size_t)call->resolved_function_index >= result->written.functions)
      return false;
    const w_seed_frontend_function *target =
        &output->functions[call->resolved_function_index];
    if (target->is_throws || target->is_anonymous_entry)
      return false;
    if (target->is_async &&
        (target->is_const ||
          (!frontend_elision_function_never(
              input, call->resolved_function_index, &path, 0u) &&
          !frontend_elision_function_static_yields(
              input, call->resolved_function_index))))
      return false;
  }
  return true;
}

static bool frontend_enum_subset_unique_count(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t *count) {
  if (output == NULL || result == NULL || count == NULL || output->types == NULL)
    return false;
  size_t unique_count = 0u;
  for (size_t type_index = 0u; type_index < result->written.types;
       type_index += 1u) {
    const w_seed_frontend_type *type = &output->types[type_index];
    if (type->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET)
      continue;
    size_t canonical_index = 0u;
    size_t canonical_ordinal = 0u;
    if (!frontend_enum_subset_canonical(output, result, type_index,
                                        &canonical_index, &canonical_ordinal))
      return false;
    (void)canonical_ordinal;
    if (canonical_index == type_index && !add_size(unique_count, 1u,
                                                   &unique_count))
      return false;
  }
  *count = unique_count;
  return true;
}

static bool frontend_enum_subset_aliases_ok(
    const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  for (size_t index = 0u; index < result->written.aliases; index += 1u) {
    const w_seed_frontend_alias *alias = &output->aliases[index];
    if (alias->module_index >= result->written.modules ||
        alias->type_index == W_SEED_FRONTEND_NONE ||
        (size_t)alias->type_index >= result->written.types ||
        output->types[alias->type_index].kind !=
            W_SEED_FRONTEND_TYPE_ENUM_SUBSET ||
        !frontend_local_enum_subset_type_supported(
            input, &output->types[alias->type_index]) ||
        output->enums[output->types[alias->type_index].enum_base_index]
                .module_index != alias->module_index ||
        !text_valid(alias->name) || alias->name.length == 0u ||
        !frontend_span_ok(
            &input->frontend_input->documents[
                output->modules[alias->module_index].document_index],
            alias->span))
      return false;
  }
  return true;
}

static bool frontend_enum_payload_types_supported(
    const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  for (size_t index = 0u; index < result->written.enum_case_parameters;
       index += 1u) {
    const uint32_t type_index = output->enum_case_parameters[index].type_index;
    if (type_index == W_SEED_FRONTEND_NONE ||
        (size_t)type_index >= result->written.types ||
        !frontend_type_supported(&output->types[type_index]) ||
        (output->types[type_index].kind != W_SEED_FRONTEND_TYPE_INTEGER &&
         output->types[type_index].kind != W_SEED_FRONTEND_TYPE_BOOL))
      return false;
  }
  return true;
}

static bool frontend_module_ranges_ok(const w_seed_hir0_input *input) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  size_t function_cursor = 0u;
  size_t entry_cursor = 0u;
  for (size_t index = 0u; index < result->written.modules; index += 1u) {
    const w_seed_frontend_module *module = &output->modules[index];
    if (module->document_index != index ||
        module->document_index >= frontend_input->document_count ||
        !text_valid(module->source_id) || !text_valid(module->module_id) ||
        !text_valid(module->local_module_name) ||
         !frontend_span_ok(&frontend_input->documents[module->document_index],
                           module->span) ||
        !text_equal(module->source_id,
                    frontend_input->documents[module->document_index]
                        .logical_source_id) ||
        !text_equal(module->module_id,
                    frontend_input->documents[module->document_index].module_id) ||
        !text_equal(module->local_module_name,
                    frontend_input->documents[module->document_index]
                        .local_module_name) ||
         module->first_function != function_cursor ||
         module->first_entry != entry_cursor ||
         !range_valid(module->first_function, module->function_count,
                     result->written.functions) ||
        !range_valid(module->first_entry, module->entry_count,
                     result->written.entries) ||
        module->first_function > UINT32_MAX || module->first_entry > UINT32_MAX)
      return false;
    for (size_t prior = 0u; prior < index; prior += 1u)
      if (text_equal(module->module_id, output->modules[prior].module_id))
        return false;
    function_cursor += module->function_count;
    entry_cursor += module->entry_count;
  }
  return function_cursor == result->written.functions &&
         entry_cursor == result->written.entries;
}

static bool frontend_function_ranges_ok(const w_seed_hir0_input *input) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  size_t parameter_cursor = 0u;
  size_t statement_cursor = 0u;
  for (size_t index = 0u; index < result->written.functions; index += 1u) {
    const w_seed_frontend_function *function = &output->functions[index];
    if (function->module_index >= result->written.modules ||
        !text_valid(function->name) || function->name.length == 0u ||
        function->return_type == W_SEED_FRONTEND_NONE ||
        (size_t)function->return_type >= result->written.types ||
        frontend_type_is_usize(&output->types[function->return_type]) ||
        (output->types[function->return_type].kind !=
             W_SEED_FRONTEND_TYPE_UNIT &&
         !frontend_hir_type_supported(input,
             &output->types[function->return_type])) ||
        !frontend_span_ok(&input->frontend_input->documents[
                              output->modules[function->module_index]
                                  .document_index],
                          function->span) ||
         !frontend_span_ok(&input->frontend_input->documents[
                               output->modules[function->module_index]
                                   .document_index],
                           function->body_span) ||
         function->first_parameter != parameter_cursor ||
         function->first_statement != statement_cursor ||
         function->first_parameter > UINT32_MAX ||
        !range_valid(function->first_parameter, function->parameter_count,
                     result->written.parameters) ||
        !range_valid(function->first_statement, function->statement_count,
                     result->written.statements))
      return false;
    const w_seed_frontend_module *module =
        &output->modules[function->module_index];
    if (!range_valid(module->first_function, module->function_count,
                     result->written.functions) ||
        index < module->first_function ||
        index >= (size_t)module->first_function + module->function_count)
      return false;
    for (size_t prior = 0u; prior < index; prior += 1u)
      if (output->functions[prior].module_index == function->module_index &&
          text_equal(output->functions[prior].name, function->name))
        return false;
    for (size_t parameter = 0u; parameter < function->parameter_count;
         parameter += 1u) {
      const size_t parameter_index = (size_t)function->first_parameter + parameter;
      const w_seed_frontend_parameter *value = &output->parameters[parameter_index];
      if (value->owner_function != index || value->module_index != function->module_index ||
          value->type_index == W_SEED_FRONTEND_NONE ||
          (size_t)value->type_index >= result->written.types ||
          frontend_type_is_usize(&output->types[value->type_index]) ||
          !frontend_hir_type_supported(input, &output->types[value->type_index]) ||
          !frontend_span_ok(&input->frontend_input->documents[
                                module->document_index],
                            value->span) ||
          !text_valid(value->name) ||
           !frontend_label_valid(value->label_kind, value->label))
        return false;
    }
    parameter_cursor += function->parameter_count;
    statement_cursor += function->statement_count;
  }
  return parameter_cursor == result->written.parameters &&
         statement_cursor == result->written.statements;
}

static bool frontend_entry_records_ok(const w_seed_hir0_input *input) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (result->written.entries != 1u || result->written.modules == 0u ||
      output->modules[0].entry_count != 1u ||
      output->entries == NULL || output->entries[0].module_index != 0u)
    return false;
  for (size_t module = 1u; module < result->written.modules; module += 1u)
    if (output->modules[module].entry_count != 0u) return false;
  for (size_t index = 0u; index < result->written.entries; index += 1u) {
    const w_seed_frontend_entry *entry = &output->entries[index];
    if (entry->module_index >= result->written.modules || !entry->valid ||
        entry->target_function == W_SEED_FRONTEND_NONE ||
        (size_t)entry->target_function >= result->written.functions ||
        !frontend_span_ok(&input->frontend_input->documents[
                              output->modules[entry->module_index]
                                  .document_index],
                          entry->span))
      return false;
    const w_seed_frontend_module *module = &output->modules[entry->module_index];
    if (index < module->first_entry ||
        index >= (size_t)module->first_entry + module->entry_count)
      return false;
    const w_seed_frontend_function *function =
        &output->functions[entry->target_function];
    if (function->module_index != entry->module_index ||
        (entry->is_body != function->is_anonymous_entry) ||
        (entry->is_body
             ? entry->target.length != 0u
             : entry->target.length == 0u ||
                   !text_equal(function->name, entry->target)))
      return false;
    if (result->written.modules > 1u && entry->is_body) return false;
  }
  return true;
}

static bool frontend_symbol_exactly_one(
    const w_seed_frontend_output *output, size_t symbol_count,
    w_seed_frontend_symbol_kind kind, uint32_t module_index,
    uint32_t owner_index) {
  size_t matches = 0u;
  for (size_t index = 0u; index < symbol_count; index += 1u) {
    const w_seed_frontend_symbol *symbol = &output->symbols[index];
    if (symbol->kind == kind && symbol->module_index == module_index &&
        symbol->owner_index == owner_index)
      if (!add_size(matches, 1u, &matches)) return false;
  }
  return matches == 1u;
}

/* The legacy symbol projection is source-order canonical for one document.
 * With multiple documents, declaration families remain dense per module but
 * their cross-family source order is not represented in the HIR input. Check
 * the same projection relationally instead: every symbol must name exactly
 * one caller-owned record, and every expected record must have one symbol. */
static bool frontend_symbol_records_multi_ok(
    const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  /* These families are closed by collect(). Let that barrier classify them
   * as unsupported instead of using this auxiliary projection as authority. */
  if (result->written.structs != 0u || result->written.fields != 0u ||
      result->written.type_declarations != 0u ||
      result->written.const_declarations != 0u)
    return true;
  size_t binding_count = 0u;
  for (size_t index = 0u; index < result->written.statements; index += 1u) {
    const w_seed_frontend_statement *statement = &output->statements[index];
    if ((statement->kind == W_SEED_FRONTEND_STMT_LET ||
         statement->kind == W_SEED_FRONTEND_STMT_VAR) &&
        statement->binding_name.length != 0u &&
        !add_size(binding_count, 1u, &binding_count))
      return false;
  }
  size_t expected = result->written.modules;
  if (!add_size(expected, result->written.functions, &expected) ||
      !add_size(expected, result->written.parameters, &expected) ||
      !add_size(expected, result->written.entries, &expected) ||
      !add_size(expected, result->written.enums, &expected) ||
      !add_size(expected, result->written.enum_cases, &expected) ||
      !add_size(expected, result->written.aliases, &expected) ||
      !add_size(expected, binding_count, &expected) ||
      result->written.symbols != expected ||
      (expected != 0u && output->symbols == NULL))
    return false;
  for (size_t index = 0u; index < result->written.symbols; index += 1u) {
    const w_seed_frontend_symbol *symbol = &output->symbols[index];
    if (symbol->module_index >= result->written.modules ||
        !text_valid(symbol->name) ||
        !frontend_span_ok(
            &input->frontend_input->documents[
                output->modules[symbol->module_index].document_index],
            symbol->span))
      return false;
    switch (symbol->kind) {
      case W_SEED_FRONTEND_SYMBOL_MODULE: {
        if (symbol->owner_index != symbol->module_index ||
            !text_equal(symbol->name,
                        output->modules[symbol->module_index].module_id) ||
            !symbol->exported || symbol->type_index != W_SEED_FRONTEND_NONE)
          return false;
        break;
      }
      case W_SEED_FRONTEND_SYMBOL_ENUM: {
        if (symbol->owner_index >= result->written.enums) return false;
        const w_seed_frontend_enum *decl =
            &output->enums[symbol->owner_index];
        if (decl->module_index != symbol->module_index ||
            !text_equal(symbol->name, decl->name) ||
            symbol->exported != decl->exported ||
            symbol->type_index != decl->type_index)
          return false;
        break;
      }
      case W_SEED_FRONTEND_SYMBOL_ENUM_CASE: {
        if (symbol->owner_index >= result->written.enum_cases) return false;
        const w_seed_frontend_enum_case *case_value =
            &output->enum_cases[symbol->owner_index];
        if (case_value->module_index != symbol->module_index ||
            case_value->owner_enum >= result->written.enums ||
            !text_equal(symbol->name, case_value->name) ||
            symbol->exported != output->enums[case_value->owner_enum].exported ||
            symbol->type_index !=
                output->enums[case_value->owner_enum].type_index)
          return false;
        break;
      }
      case W_SEED_FRONTEND_SYMBOL_ALIAS: {
        if (symbol->owner_index >= result->written.aliases) return false;
        const w_seed_frontend_alias *alias = &output->aliases[symbol->owner_index];
        if (alias->module_index != symbol->module_index ||
            !text_equal(symbol->name, alias->name) ||
            symbol->exported != alias->exported ||
            symbol->type_index != alias->type_index)
          return false;
        break;
      }
      case W_SEED_FRONTEND_SYMBOL_FUNCTION: {
        if (symbol->owner_index >= result->written.functions) return false;
        const w_seed_frontend_function *function =
            &output->functions[symbol->owner_index];
        if (function->module_index != symbol->module_index ||
            !text_equal(symbol->name, function->name) ||
            symbol->exported != function->exported ||
            symbol->type_index != function->return_type)
          return false;
        break;
      }
      case W_SEED_FRONTEND_SYMBOL_PARAMETER: {
        if (symbol->owner_index >= result->written.parameters) return false;
        const w_seed_frontend_parameter *parameter =
            &output->parameters[symbol->owner_index];
        if (parameter->module_index != symbol->module_index ||
            !text_equal(symbol->name, parameter->name) || symbol->exported ||
            symbol->type_index != parameter->type_index)
          return false;
        break;
      }
      case W_SEED_FRONTEND_SYMBOL_BINDING: {
        if (symbol->owner_index >= result->written.statements) return false;
        const w_seed_frontend_statement *statement =
            &output->statements[symbol->owner_index];
        if ((statement->kind != W_SEED_FRONTEND_STMT_LET &&
             statement->kind != W_SEED_FRONTEND_STMT_VAR) ||
            statement->binding_name.length == 0u ||
            statement->module_index != symbol->module_index ||
            !text_equal(symbol->name, statement->binding_name) ||
            symbol->exported || symbol->type_index != statement->effective_type)
          return false;
        break;
      }
      case W_SEED_FRONTEND_SYMBOL_ENTRY: {
        if (symbol->owner_index >= result->written.entries) return false;
        const w_seed_frontend_entry *entry = &output->entries[symbol->owner_index];
        if (entry->module_index != symbol->module_index || symbol->exported ||
            symbol->type_index != W_SEED_FRONTEND_NONE ||
            (entry->is_body ? symbol->name.length != 0u
                            : !text_equal(symbol->name, entry->target)))
          return false;
        break;
      }
      default:
        return false;
    }
  }
  for (size_t module = 0u; module < result->written.modules; module += 1u)
    if (!frontend_symbol_exactly_one(
            output, result->written.symbols, W_SEED_FRONTEND_SYMBOL_MODULE,
            (uint32_t)module, (uint32_t)module))
      return false;
  for (size_t index = 0u; index < result->written.enums; index += 1u)
    if (!frontend_symbol_exactly_one(
            output, result->written.symbols, W_SEED_FRONTEND_SYMBOL_ENUM,
            output->enums[index].module_index, (uint32_t)index))
      return false;
  for (size_t index = 0u; index < result->written.enum_cases; index += 1u)
    if (!frontend_symbol_exactly_one(
            output, result->written.symbols, W_SEED_FRONTEND_SYMBOL_ENUM_CASE,
            output->enum_cases[index].module_index, (uint32_t)index))
      return false;
  for (size_t index = 0u; index < result->written.aliases; index += 1u)
    if (!frontend_symbol_exactly_one(
            output, result->written.symbols, W_SEED_FRONTEND_SYMBOL_ALIAS,
            output->aliases[index].module_index, (uint32_t)index))
      return false;
  for (size_t index = 0u; index < result->written.functions; index += 1u)
    if (!frontend_symbol_exactly_one(
            output, result->written.symbols, W_SEED_FRONTEND_SYMBOL_FUNCTION,
            output->functions[index].module_index, (uint32_t)index))
      return false;
  for (size_t index = 0u; index < result->written.parameters; index += 1u)
    if (!frontend_symbol_exactly_one(
            output, result->written.symbols, W_SEED_FRONTEND_SYMBOL_PARAMETER,
            output->parameters[index].module_index, (uint32_t)index))
      return false;
  for (size_t index = 0u; index < result->written.statements; index += 1u) {
    const w_seed_frontend_statement *statement = &output->statements[index];
    if ((statement->kind == W_SEED_FRONTEND_STMT_LET ||
         statement->kind == W_SEED_FRONTEND_STMT_VAR) &&
        statement->binding_name.length != 0u &&
        !frontend_symbol_exactly_one(
            output, result->written.symbols, W_SEED_FRONTEND_SYMBOL_BINDING,
            statement->module_index, (uint32_t)index))
      return false;
  }
  for (size_t index = 0u; index < result->written.entries; index += 1u)
    if (!frontend_symbol_exactly_one(
            output, result->written.symbols, W_SEED_FRONTEND_SYMBOL_ENTRY,
            output->entries[index].module_index, (uint32_t)index))
      return false;
  return true;
}

/* The frontend symbol table is an auxiliary index, not an HIR0 record family.
 * The one-module HIR0 subset nevertheless requires its exact canonical
 * projection so a forged symbol cannot contradict the graph that HIR0 copies
 * from modules/functions/entries. HIR lowering never reads symbols as an
 * authority. */
static bool frontend_symbol_records_ok(const w_seed_hir0_input *input) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (result->written.modules > 1u)
    return frontend_symbol_records_multi_ok(input);
  size_t expected = 1u;
  for (size_t statement = 0u; statement < result->written.statements;
       statement += 1u) {
    const w_seed_frontend_statement *source = &output->statements[statement];
    if ((source->kind == W_SEED_FRONTEND_STMT_LET ||
         source->kind == W_SEED_FRONTEND_STMT_VAR) &&
        source->binding_name.length != 0u &&
        !add_size(expected, 1u, &expected))
      return false;
  }
  if (!add_size(expected, result->written.functions, &expected) ||
      !add_size(expected, result->written.parameters, &expected) ||
      !add_size(expected, result->written.entries, &expected) ||
      !add_size(expected, result->written.enums, &expected) ||
      !add_size(expected, result->written.enum_cases, &expected) ||
      !add_size(expected, result->written.aliases, &expected) ||
      result->written.symbols != expected)
    return false;
  const w_seed_frontend_module *module = &output->modules[0];
  const w_seed_frontend_symbol *module_symbol = &output->symbols[0];
  if (module_symbol->kind != W_SEED_FRONTEND_SYMBOL_MODULE ||
      module_symbol->module_index != 0u || module_symbol->owner_index != 0u ||
      !text_equal(module_symbol->name, module->module_id) ||
      !module_symbol->exported ||
      !frontend_span_ok(&input->frontend_input->documents[0], module_symbol->span))
    return false;
  size_t symbol_cursor = 1u;
  for (size_t enum_index = 0u; enum_index < result->written.enums;
       enum_index += 1u) {
    const w_seed_frontend_enum *decl = &output->enums[enum_index];
    const w_seed_frontend_symbol *symbol = &output->symbols[symbol_cursor++];
    if (symbol->kind != W_SEED_FRONTEND_SYMBOL_ENUM ||
        symbol->module_index != decl->module_index ||
        symbol->owner_index != enum_index || !text_equal(symbol->name, decl->name) ||
        symbol->exported != decl->exported || symbol->type_index != decl->type_index ||
        !frontend_span_ok(&input->frontend_input->documents[0], symbol->span))
      return false;
    for (size_t ordinal = 0u; ordinal < decl->case_count; ordinal += 1u) {
      const size_t case_index = (size_t)decl->first_case + ordinal;
      const w_seed_frontend_enum_case *case_value = &output->enum_cases[case_index];
      const w_seed_frontend_symbol *case_symbol = &output->symbols[symbol_cursor++];
      if (case_symbol->kind != W_SEED_FRONTEND_SYMBOL_ENUM_CASE ||
          case_symbol->module_index != case_value->module_index ||
          case_symbol->owner_index != case_index ||
          !text_equal(case_symbol->name, case_value->name) ||
          case_symbol->exported != decl->exported ||
          case_symbol->type_index != decl->type_index ||
          !frontend_span_ok(&input->frontend_input->documents[0],
                            case_symbol->span))
        return false;
    }
  }
  for (size_t alias_index = 0u; alias_index < result->written.aliases;
       alias_index += 1u) {
    const w_seed_frontend_alias *alias = &output->aliases[alias_index];
    const w_seed_frontend_symbol *symbol = &output->symbols[symbol_cursor++];
    if (symbol->kind != W_SEED_FRONTEND_SYMBOL_ALIAS ||
        symbol->module_index != alias->module_index ||
        symbol->owner_index != alias_index ||
        !text_equal(symbol->name, alias->name) ||
        symbol->exported != alias->exported ||
        symbol->type_index != alias->type_index ||
        !frontend_span_ok(&input->frontend_input->documents[0], symbol->span))
      return false;
  }
  for (size_t function = 0u; function < result->written.functions; function += 1u) {
    const w_seed_frontend_function *source = &output->functions[function];
    for (size_t parameter = 0u; parameter < source->parameter_count;
         parameter += 1u) {
      const size_t parameter_index = (size_t)source->first_parameter + parameter;
      const w_seed_frontend_parameter *parameter_source =
          &output->parameters[parameter_index];
      const w_seed_frontend_symbol *symbol = &output->symbols[symbol_cursor++];
      if (symbol->kind != W_SEED_FRONTEND_SYMBOL_PARAMETER ||
          symbol->module_index != source->module_index ||
          symbol->owner_index != parameter_index ||
          !text_equal(symbol->name, parameter_source->name) || symbol->exported ||
          symbol->type_index != parameter_source->type_index ||
          !frontend_span_ok(&input->frontend_input->documents[0], symbol->span))
         return false;
    }
    for (size_t statement = 0u; statement < source->statement_count;
         statement += 1u) {
      const size_t statement_index = (size_t)source->first_statement + statement;
      const w_seed_frontend_statement *statement_source =
          &output->statements[statement_index];
      if ((statement_source->kind != W_SEED_FRONTEND_STMT_LET &&
           statement_source->kind != W_SEED_FRONTEND_STMT_VAR) ||
          statement_source->binding_name.length == 0u)
        continue;
      const w_seed_frontend_symbol *binding = &output->symbols[symbol_cursor++];
      if (binding->kind != W_SEED_FRONTEND_SYMBOL_BINDING ||
          binding->module_index != statement_source->module_index ||
          binding->owner_index != statement_index ||
          !text_equal(binding->name, statement_source->binding_name) ||
          binding->exported || binding->type_index != statement_source->effective_type ||
          !frontend_span_ok(&input->frontend_input->documents[0], binding->span))
        return false;
    }
    const w_seed_frontend_symbol *symbol = &output->symbols[symbol_cursor++];
    if (symbol->kind != W_SEED_FRONTEND_SYMBOL_FUNCTION ||
        symbol->module_index != source->module_index ||
        symbol->owner_index != function || !text_equal(symbol->name, source->name) ||
        symbol->exported != source->exported ||
        symbol->type_index != source->return_type ||
        !frontend_span_ok(&input->frontend_input->documents[0], symbol->span))
      return false;
  }
  for (size_t entry = 0u; entry < result->written.entries; entry += 1u) {
    const w_seed_frontend_symbol *symbol = &output->symbols[symbol_cursor++];
    const w_seed_frontend_entry *source = &output->entries[entry];
    if (symbol->kind != W_SEED_FRONTEND_SYMBOL_ENTRY ||
        symbol->module_index != source->module_index ||
        symbol->owner_index != entry ||
        (source->is_body ? symbol->name.length != 0u
                         : !text_equal(symbol->name, source->target)) ||
        symbol->exported || symbol->type_index != W_SEED_FRONTEND_NONE ||
        !frontend_span_ok(&input->frontend_input->documents[0], symbol->span))
      return false;
  }
  return symbol_cursor == result->written.symbols;
}

static bool frontend_integer_i64(const w_seed_frontend_expression *value,
                                 int64_t *out) {
  if (value == NULL || out == NULL || !value->has_integer_value) return false;
  uint64_t magnitude = 0u;
  for (size_t index = 8u; index < sizeof(value->integer_value); index += 1u)
    if (value->integer_value[index] != 0u) return false;
  for (size_t index = 0u; index < 8u; index += 1u)
    magnitude |= (uint64_t)value->integer_value[index] << (index * 8u);
  if (magnitude > (uint64_t)INT64_MAX) return false;
  *out = (int64_t)magnitude;
  return true;
}

static bool frontend_integer_u64(const w_seed_frontend_expression *value,
                                 uint64_t *out) {
  if (value == NULL || out == NULL || !value->has_integer_value) return false;
  uint64_t magnitude = 0u;
  for (size_t index = sizeof(uint64_t); index < sizeof(value->integer_value);
       index += 1u)
    if (value->integer_value[index] != 0u) return false;
  for (size_t index = 0u; index < sizeof(uint64_t); index += 1u)
    magnitude |= (uint64_t)value->integer_value[index] << (index * 8u);
  *out = magnitude;
  return true;
}

static bool frontend_value_common_ok(
    const w_seed_hir0_input *input, const w_seed_frontend_expression *value,
    size_t module_index, size_t function_index, size_t document_index) {
  if (value == NULL || !value->supported || value->module_index != module_index ||
      value->owner_function != function_index ||
      ((value->kind != W_SEED_FRONTEND_EXPR_CALL) &&
       (value->first_argument != W_SEED_FRONTEND_NONE ||
        value->argument_count != 0u)) ||
      (value->inferred_type != W_SEED_FRONTEND_NONE &&
       ((size_t)value->inferred_type >= input->frontend_result->written.types ||
        !frontend_hir_type_supported(input,
            &input->frontend_output->types[value->inferred_type]))) ||
      (value->resolved_pattern_capture != W_SEED_FRONTEND_NONE &&
       value->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER) ||
      ((value->kind != W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH &&
        value->kind != W_SEED_FRONTEND_EXPR_AWAIT) &&
       (value->task_result_type != W_SEED_FRONTEND_NONE ||
        value->task_call_expression != W_SEED_FRONTEND_NONE ||
        value->task_binding_statement != W_SEED_FRONTEND_NONE)) ||
      !frontend_span_ok(&input->frontend_input->documents[document_index],
                        value->span))
    return false;
  return true;
}

static bool frontend_value_has_no_resolution(
    const w_seed_frontend_expression *value) {
  return value != NULL &&
         value->enum_index == W_SEED_FRONTEND_NONE &&
         value->enum_case_index == W_SEED_FRONTEND_NONE &&
         value->first_switch_arm == W_SEED_FRONTEND_NONE &&
         value->switch_arm_count == 0u &&
         value->first_membership_case == W_SEED_FRONTEND_NONE &&
         value->membership_case_count == 0u &&
         value->resolved_parameter_ordinal == W_SEED_FRONTEND_NONE &&
         value->resolved_function_index == W_SEED_FRONTEND_NONE &&
         value->resolved_callee_kind == W_SEED_FRONTEND_CALLEE_NONE &&
         value->resolved_host_symbol_index == W_SEED_FRONTEND_NONE &&
         value->resolved_external_module_index == W_SEED_FRONTEND_NONE &&
         value->resolved_external_symbol_index == W_SEED_FRONTEND_NONE &&
         value->resolved_local_ordinal == W_SEED_FRONTEND_NONE &&
         value->resolved_const_declaration == W_SEED_FRONTEND_NONE &&
         value->resolved_pattern_capture == W_SEED_FRONTEND_NONE &&
         value->member_name.length == 0u && text_valid(value->member_name);
}

/* Unary records carry no callable, binding, loop, or switch resolution.  Enum
 * fields are not part of the unary expression contract and are intentionally
 * ignored here; the frontend's scalar prefix path does not initialize those
 * unrelated fields before normalization. */
static bool frontend_unary_has_no_resolution(
    const w_seed_frontend_expression *value) {
  return value != NULL &&
         value->first_switch_arm == W_SEED_FRONTEND_NONE &&
         value->switch_arm_count == 0u &&
         value->first_membership_case == W_SEED_FRONTEND_NONE &&
         value->membership_case_count == 0u &&
         value->resolved_parameter_ordinal == W_SEED_FRONTEND_NONE &&
         value->resolved_function_index == W_SEED_FRONTEND_NONE &&
         value->resolved_callee_kind == W_SEED_FRONTEND_CALLEE_NONE &&
         value->resolved_host_symbol_index == W_SEED_FRONTEND_NONE &&
         value->resolved_external_module_index == W_SEED_FRONTEND_NONE &&
         value->resolved_external_symbol_index == W_SEED_FRONTEND_NONE &&
         value->resolved_local_ordinal == W_SEED_FRONTEND_NONE &&
         value->resolved_const_declaration == W_SEED_FRONTEND_NONE &&
         value->resolved_pattern_capture == W_SEED_FRONTEND_NONE &&
         value->member_name.length == 0u && text_valid(value->member_name);
}

static w_seed_hir0_binary_operator hir_binary_operator(
    w_seed_frontend_text text);

static w_seed_hir0_logical_operator hir_logical_operator(
    w_seed_frontend_text text) {
  if (text_is(text, "&&")) return W_SEED_HIR0_LOGICAL_AND;
  if (text_is(text, "||")) return W_SEED_HIR0_LOGICAL_OR;
  return W_SEED_HIR0_LOGICAL_NONE;
}

static bool frontend_call_expression_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index, size_t statement_index,
    uint32_t root_index, bool result_value, size_t *expression_cursor,
    size_t *interpolation_segment_cursor, size_t *const_byte_cursor,
    size_t *call_total, size_t *argument_total, size_t *value_total,
    size_t *segment_total, size_t *value_bytes, size_t *logical_total);

static bool frontend_external_member_value_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index,
    const w_seed_frontend_expression *value);

static bool frontend_usize_count_comparison_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index,
    const w_seed_frontend_expression *value);

static bool frontend_usize_count_comparison_literal_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index,
    const w_seed_frontend_expression *value);

static bool frontend_external_type_is(const w_seed_hir0_input *input,
                                      uint32_t type_index,
                                      uint32_t module_index,
                                      uint32_t symbol_index);

/* The caller has already validated each expression's type-arena range. */
static bool frontend_expression_is_i64(
    const w_seed_frontend_output *output,
    const w_seed_frontend_expression *expression) {
  if (expression->inferred_type == W_SEED_FRONTEND_NONE) return false;
  const w_seed_frontend_type *type = &output->types[expression->inferred_type];
  return type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
         type->bit_width == 64u && type->is_signed;
}

static bool frontend_expression_is_bool(
    const w_seed_frontend_output *output,
    const w_seed_frontend_expression *expression) {
  return output != NULL && expression != NULL &&
         expression->inferred_type != W_SEED_FRONTEND_NONE &&
         output->types[expression->inferred_type].kind ==
             W_SEED_FRONTEND_TYPE_BOOL;
}

static bool frontend_expression_is_usize(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result,
    const w_seed_frontend_expression *expression) {
  return output != NULL && result != NULL && expression != NULL &&
         expression->inferred_type != W_SEED_FRONTEND_NONE &&
         (size_t)expression->inferred_type < result->written.types &&
         frontend_type_is_usize(&output->types[expression->inferred_type]);
}

static bool frontend_pattern_capture_read_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, uint32_t expression_index,
    const w_seed_frontend_expression *value) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || value == NULL ||
      value->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
      value->resolved_pattern_capture == W_SEED_FRONTEND_NONE ||
      value->resolved_pattern_capture >=
          input->frontend_result->written.pattern_captures)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_pattern_capture *capture =
      &output->pattern_captures[value->resolved_pattern_capture];
  if (capture->module_index != module_index ||
      capture->owner_switch_arm >=
          input->frontend_result->written.switch_arms ||
      capture->type_index == W_SEED_FRONTEND_NONE ||
      capture->type_index >= input->frontend_result->written.types ||
      value->inferred_type == W_SEED_FRONTEND_NONE ||
      value->inferred_type >= input->frontend_result->written.types ||
      !frontend_supported_types_equal_for_input(
          input, &output->types[capture->type_index],
          &output->types[value->inferred_type]) ||
      !text_equal(capture->name, value->spelling) ||
      value->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE ||
      value->resolved_binding_statement != W_SEED_FRONTEND_NONE)
    return false;
  const w_seed_frontend_switch_arm *arm =
      &output->switch_arms[capture->owner_switch_arm];
  if (!arm->supported || arm->module_index != module_index ||
      arm->owner_expression == W_SEED_FRONTEND_NONE ||
      arm->owner_expression >= input->frontend_result->written.expressions ||
      arm->first_capture == W_SEED_FRONTEND_NONE ||
      capture->ordinal >= arm->capture_count ||
      (size_t)arm->first_capture + capture->ordinal !=
          value->resolved_pattern_capture ||
      arm->result_expression == W_SEED_FRONTEND_NONE ||
      arm->result_expression >= input->frontend_result->written.expressions)
    return false;
  const w_seed_frontend_expression *root =
      &output->expressions[arm->owner_expression];
  if (root->kind != W_SEED_FRONTEND_EXPR_SWITCH ||
      root->owner_function != function_index ||
      root->first_switch_arm == W_SEED_FRONTEND_NONE ||
      capture->owner_switch_arm < root->first_switch_arm ||
      capture->owner_switch_arm >=
          (size_t)root->first_switch_arm + root->switch_arm_count)
    return false;
  const size_t arm_ordinal = capture->owner_switch_arm - root->first_switch_arm;
  const size_t first_expression =
      arm_ordinal == 0u
          ? (size_t)arm->owner_expression + 1u
          : (size_t)output
                    ->switch_arms[(size_t)root->first_switch_arm +
                                  arm_ordinal - 1u]
                    .result_expression +
                1u;
  return expression_index >= first_expression &&
         expression_index <= arm->result_expression;
}

static bool frontend_type_is_scalar(const w_seed_frontend_type *type) {
  return type != NULL &&
         (type->kind == W_SEED_FRONTEND_TYPE_BOOL ||
          (type->kind == W_SEED_FRONTEND_TYPE_INTEGER && type->is_signed &&
           type->bit_width == 64u) ||
          frontend_type_is_usize(type));
}

static bool frontend_local_enum_case_value_ok(
    const w_seed_hir0_input *input, const w_seed_frontend_expression *value) {
  /* collect() validates all enum declarations and case partitions once before
   * it walks expression values.  Keep this per-value check linear. */
  if (input == NULL || value == NULL ||
      value->kind != W_SEED_FRONTEND_EXPR_ENUM_CASE || !value->supported ||
      value->enum_index == W_SEED_FRONTEND_NONE ||
      (size_t)value->enum_index >= input->frontend_result->written.enums ||
      value->enum_case_index == W_SEED_FRONTEND_NONE ||
      (size_t)value->enum_case_index >= input->frontend_result->written.enum_cases ||
      value->inferred_type == W_SEED_FRONTEND_NONE ||
      (size_t)value->inferred_type >= input->frontend_result->written.types ||
      value->left != W_SEED_FRONTEND_NONE ||
      value->right != W_SEED_FRONTEND_NONE ||
      value->first_switch_arm != W_SEED_FRONTEND_NONE ||
      value->switch_arm_count != 0u ||
      value->first_membership_case != W_SEED_FRONTEND_NONE ||
      value->membership_case_count != 0u ||
      value->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE ||
      value->resolved_function_index != W_SEED_FRONTEND_NONE ||
      value->resolved_callee_kind != W_SEED_FRONTEND_CALLEE_NONE ||
      value->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
      value->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
      value->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
      value->resolved_local_ordinal != W_SEED_FRONTEND_NONE ||
      value->resolved_const_declaration != W_SEED_FRONTEND_NONE ||
      value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
      value->const_byte_offset != W_SEED_FRONTEND_NONE ||
      value->const_byte_count != 0u || value->has_bool_value ||
      value->has_integer_value || value->first_interpolation_segment !=
                                       W_SEED_FRONTEND_NONE ||
      value->interpolation_segment_count != 0u || value->member_name.length != 0u ||
      !text_valid(value->member_name))
    return false;
  const w_seed_frontend_enum *decl =
      &input->frontend_output->enums[value->enum_index];
  const w_seed_frontend_type *type =
      &input->frontend_output->types[value->inferred_type];
  const w_seed_frontend_enum_case *case_value =
      &input->frontend_output->enum_cases[value->enum_case_index];
  const bool base_type = frontend_local_enum_type_supported(input, type);
  const bool subset_type = frontend_local_enum_subset_type_supported(input, type);
  const bool case_in_base =
      case_value->owner_enum == value->enum_index &&
      (size_t)value->enum_case_index >= decl->first_case &&
      (size_t)value->enum_case_index <
          (size_t)decl->first_case + decl->case_count;
  if (!case_in_base || type->enum_base_index != value->enum_index)
    return false;
  if (base_type) return true;
  return subset_type && frontend_local_enum_subset_contains_case(
                             input, type, value->inferred_type,
                             value->enum_case_index);
}

/* W-1560's first HIR consumer accepts a deliberately small natural-loop
 * expression language.  It is scalar and side-effect free, and every local
 * read is either a function parameter or one of the carried roots.  The root
 * array is bounded by the existing branch-assignment scratch capacity; it is
 * not a product-level two-carrier limit. */
static bool frontend_loop_scalar_tree_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index, uint32_t expression_index,
    const uint32_t *root_statements, size_t root_count, bool *uses_root,
    size_t depth) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || uses_root == NULL ||
      root_statements == NULL || root_count == 0u ||
      root_count > HIR0_MAX_BRANCH_ASSIGNMENTS ||
      depth > W_SEED_HIR0_MAX_NESTING ||
      expression_index == W_SEED_FRONTEND_NONE ||
      (size_t)expression_index >= input->frontend_result->written.expressions)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_expression *value =
      &output->expressions[expression_index];
  if (!frontend_value_common_ok(input, value, module_index, function_index,
                                document_index) ||
      value->inferred_type == W_SEED_FRONTEND_NONE ||
      (size_t)value->inferred_type >= input->frontend_result->written.types ||
      value->const_byte_offset != W_SEED_FRONTEND_NONE ||
      value->const_byte_count != 0u ||
      value->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
      value->interpolation_segment_count != 0u)
    return false;
  if (value->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER) {
    if (value->left != W_SEED_FRONTEND_NONE ||
        value->right != W_SEED_FRONTEND_NONE || value->has_bool_value ||
        value->has_integer_value || value->resolved_function_index !=
                                         W_SEED_FRONTEND_NONE ||
        value->resolved_callee_kind != W_SEED_FRONTEND_CALLEE_NONE ||
        value->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
        value->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
        value->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
        value->resolved_local_ordinal != W_SEED_FRONTEND_NONE ||
        value->resolved_const_declaration != W_SEED_FRONTEND_NONE ||
        value->member_name.length != 0u || !text_valid(value->member_name) ||
        !frontend_type_is_scalar(&output->types[value->inferred_type]))
      return false;
    if (value->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE)
      return value->resolved_binding_statement == W_SEED_FRONTEND_NONE;
    bool carried_root = false;
    for (size_t ordinal = 0u; ordinal < root_count; ordinal += 1u)
      if (value->resolved_binding_statement == root_statements[ordinal]) {
        carried_root = true;
        break;
      }
    if (!carried_root) return false;
    *uses_root = true;
    return true;
  }
  if (value->kind == W_SEED_FRONTEND_EXPR_INTEGER)
    return frontend_expression_is_i64(output, value) &&
           value->has_integer_value && !value->has_bool_value &&
           frontend_value_has_no_resolution(value);
  if (value->kind == W_SEED_FRONTEND_EXPR_BOOL)
    return frontend_expression_is_bool(output, value) && value->has_bool_value &&
           !value->has_integer_value && frontend_value_has_no_resolution(value);
  if (value->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS)
    return value->left != W_SEED_FRONTEND_NONE &&
           value->right == W_SEED_FRONTEND_NONE &&
           frontend_value_has_no_resolution(value) &&
           frontend_loop_scalar_tree_ok(input, module_index, function_index,
                                        document_index, value->left,
                                        root_statements, root_count, uses_root,
                                        depth + 1u);
  if (value->kind == W_SEED_FRONTEND_EXPR_UNARY)
    return value->left != W_SEED_FRONTEND_NONE &&
           value->right == W_SEED_FRONTEND_NONE &&
           (text_is(value->operator_text, "!") ||
            text_is(value->operator_text, "-")) &&
           frontend_unary_has_no_resolution(value) &&
           frontend_loop_scalar_tree_ok(input, module_index, function_index,
                                        document_index, value->left,
                                        root_statements, root_count, uses_root,
                                        depth + 1u);
  if (value->kind != W_SEED_FRONTEND_EXPR_BINARY ||
      value->left == W_SEED_FRONTEND_NONE ||
      value->right == W_SEED_FRONTEND_NONE ||
      hir_logical_operator(value->operator_text) != W_SEED_HIR0_LOGICAL_NONE ||
      !frontend_value_has_no_resolution(value))
    return false;
  return frontend_loop_scalar_tree_ok(
             input, module_index, function_index, document_index, value->left,
             root_statements, root_count, uses_root, depth + 1u) &&
         frontend_loop_scalar_tree_ok(
             input, module_index, function_index, document_index, value->right,
             root_statements, root_count, uses_root, depth + 1u);
}

/* Scalar-if arms are deliberately narrower than the ordinary HIR value
 * language.  This structural walk is independent of the dense postorder
 * cursor below, so a forged frontend record cannot smuggle a call, effect, or
 * aggregate through an otherwise well-shaped tree. Nested scalar-if values
 * are allowed only through this same bounded scalar grammar. */
static bool frontend_scalar_if_tree_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index, uint32_t root_index,
    bool allow_logical, size_t depth) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || depth > W_SEED_HIR0_MAX_NESTING ||
      root_index == W_SEED_FRONTEND_NONE ||
      (size_t)root_index >= input->frontend_result->written.expressions)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_expression *value = &output->expressions[root_index];
  if (!frontend_value_common_ok(input, value, module_index, function_index,
                                document_index) ||
      value->const_byte_offset != W_SEED_FRONTEND_NONE ||
      value->const_byte_count != 0u ||
      value->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
      value->interpolation_segment_count != 0u)
    return false;
  if (value->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER) {
    if (value->enum_index != W_SEED_FRONTEND_NONE ||
        value->enum_case_index != W_SEED_FRONTEND_NONE ||
        value->first_switch_arm != W_SEED_FRONTEND_NONE ||
        value->switch_arm_count != 0u ||
        value->first_membership_case != W_SEED_FRONTEND_NONE ||
        value->membership_case_count != 0u ||
        value->resolved_function_index != W_SEED_FRONTEND_NONE ||
        value->resolved_callee_kind != W_SEED_FRONTEND_CALLEE_NONE ||
        value->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
        value->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
        value->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
        value->resolved_local_ordinal != W_SEED_FRONTEND_NONE ||
        value->resolved_const_declaration != W_SEED_FRONTEND_NONE ||
        value->member_name.length != 0u || !text_valid(value->member_name) ||
        ((value->resolved_parameter_ordinal == W_SEED_FRONTEND_NONE) ==
         (value->resolved_binding_statement == W_SEED_FRONTEND_NONE)))
      return false;
  } else if (!frontend_value_has_no_resolution(value) ||
             value->resolved_binding_statement != W_SEED_FRONTEND_NONE) {
    return false;
  }
  if (value->kind == W_SEED_FRONTEND_EXPR_IF) {
    if (value->left == W_SEED_FRONTEND_NONE ||
        value->right == W_SEED_FRONTEND_NONE ||
        value->else_expression == W_SEED_FRONTEND_NONE ||
        (size_t)value->left >= input->frontend_result->written.expressions ||
        (size_t)value->right >= input->frontend_result->written.expressions ||
        (size_t)value->else_expression >=
            input->frontend_result->written.expressions ||
        value->inferred_type == W_SEED_FRONTEND_NONE ||
        (size_t)value->inferred_type >= input->frontend_result->written.types)
      return false;
    const w_seed_frontend_expression *condition =
        &output->expressions[value->left];
    const w_seed_frontend_expression *then_value =
        &output->expressions[value->right];
    const w_seed_frontend_expression *else_value =
        &output->expressions[value->else_expression];
    if (condition->inferred_type == W_SEED_FRONTEND_NONE ||
        (size_t)condition->inferred_type >=
            input->frontend_result->written.types ||
        then_value->inferred_type == W_SEED_FRONTEND_NONE ||
        (size_t)then_value->inferred_type >=
            input->frontend_result->written.types ||
        else_value->inferred_type == W_SEED_FRONTEND_NONE ||
        (size_t)else_value->inferred_type >=
            input->frontend_result->written.types ||
        !frontend_expression_is_bool(output, condition) ||
        !frontend_type_is_scalar(&output->types[value->inferred_type]) ||
        !frontend_type_is_scalar(&output->types[then_value->inferred_type]) ||
        !frontend_type_is_scalar(&output->types[else_value->inferred_type]) ||
        !frontend_supported_types_equal_for_input(
            input,
            &output->types[value->inferred_type],
            &output->types[then_value->inferred_type]) ||
        !frontend_supported_types_equal_for_input(
            input,
            &output->types[then_value->inferred_type],
            &output->types[else_value->inferred_type]) ||
        !frontend_scalar_if_tree_ok(input, module_index, function_index,
                                    document_index, value->left, true,
                                    depth + 1u) ||
        !frontend_scalar_if_tree_ok(input, module_index, function_index,
                                    document_index, value->right, false,
                                    depth + 1u) ||
        !frontend_scalar_if_tree_ok(input, module_index, function_index,
                                    document_index, value->else_expression,
                                    false, depth + 1u))
      return false;
    return true;
  }
  if (value->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS)
    return !value->has_bool_value && !value->has_integer_value &&
           value->right == W_SEED_FRONTEND_NONE && value->left !=
           W_SEED_FRONTEND_NONE &&
           frontend_scalar_if_tree_ok(input, module_index, function_index,
                                      document_index, value->left,
                                      allow_logical, depth + 1u);
  if (value->kind == W_SEED_FRONTEND_EXPR_UNARY) {
    const bool logical_not = text_is(value->operator_text, "!");
    const bool numeric_negate = text_is(value->operator_text, "-");
    return ((logical_not && frontend_expression_is_bool(output, value)) ||
            (numeric_negate && frontend_expression_is_i64(output, value))) &&
           !value->has_bool_value && !value->has_integer_value &&
           value->right == W_SEED_FRONTEND_NONE &&
           value->left != W_SEED_FRONTEND_NONE &&
           (size_t)value->left < input->frontend_result->written.expressions &&
           ((logical_not && frontend_expression_is_bool(
                                output, &output->expressions[value->left])) ||
            (numeric_negate && frontend_expression_is_i64(
                                   output, &output->expressions[value->left]))) &&
           frontend_scalar_if_tree_ok(input, module_index, function_index,
                                      document_index, value->left,
                                      allow_logical, depth + 1u);
  }
  if (value->kind == W_SEED_FRONTEND_EXPR_BINARY) {
    if (frontend_usize_count_comparison_ok(
            input, module_index, function_index, document_index, value))
      return true;
    const w_seed_hir0_logical_operator logical =
        hir_logical_operator(value->operator_text);
    if (logical != W_SEED_HIR0_LOGICAL_NONE && !allow_logical) return false;
    if (value->has_bool_value || value->has_integer_value) return false;
    return value->left != W_SEED_FRONTEND_NONE &&
           value->right != W_SEED_FRONTEND_NONE &&
           frontend_scalar_if_tree_ok(input, module_index, function_index,
                                      document_index, value->left,
                                      allow_logical, depth + 1u) &&
           frontend_scalar_if_tree_ok(input, module_index, function_index,
                                      document_index, value->right,
                                      allow_logical, depth + 1u);
  }
  if (value->kind == W_SEED_FRONTEND_EXPR_INTEGER)
    return frontend_expression_is_i64(output, value) &&
           value->has_integer_value && !value->has_bool_value;
  if (value->kind == W_SEED_FRONTEND_EXPR_BOOL)
    return frontend_expression_is_bool(output, value) &&
           value->has_bool_value && !value->has_integer_value;
  if (value->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER)
    return value->inferred_type != W_SEED_FRONTEND_NONE &&
           !value->has_bool_value && !value->has_integer_value &&
           (frontend_expression_is_i64(output, value) ||
            frontend_expression_is_bool(output, value));
  return false;
}

/* Frontend expressions are append-only postorder records. This walk consumes
 * exactly one dense subtree and measures the normalized HIR value tree. A
 * finite depth bound keeps validation stack use independent of hostile input. */
static bool frontend_usize_literal_leaf_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index, uint32_t root_index,
    size_t *expression_cursor, size_t *value_total) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || expression_cursor == NULL ||
      value_total == NULL || root_index == W_SEED_FRONTEND_NONE ||
      (size_t)root_index >= input->frontend_result->written.expressions ||
      (size_t)root_index != *expression_cursor ||
      !frontend_usize_count_comparison_literal_ok(
          input, module_index, function_index, document_index,
          &input->frontend_output->expressions[root_index]) ||
      !add_size(*value_total, 1u, value_total) ||
      !add_size(*expression_cursor, 1u, expression_cursor))
    return false;
  return true;
}

static bool frontend_value_tree_ok(
    const w_seed_hir0_input *input, size_t module_index, size_t function_index,
    size_t document_index, size_t use_statement, uint32_t root_index,
    size_t depth,
    size_t *expression_cursor, size_t *segment_cursor,
    size_t *const_byte_cursor, size_t *value_total, size_t *segment_total,
    size_t *value_bytes, size_t *call_total, size_t *argument_total,
    size_t *logical_total) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (depth > W_SEED_HIR0_MAX_NESTING || expression_cursor == NULL ||
      segment_cursor == NULL ||
      const_byte_cursor == NULL || value_total == NULL ||
      segment_total == NULL || value_bytes == NULL || call_total == NULL ||
      argument_total == NULL || logical_total == NULL ||
      root_index == W_SEED_FRONTEND_NONE ||
      (size_t)root_index >= result->written.expressions)
    return false;
  const w_seed_frontend_expression *value = &output->expressions[root_index];
  if (!frontend_value_common_ok(input, value, module_index, function_index,
                                document_index))
    return false;

  if (value->kind == W_SEED_FRONTEND_EXPR_IF) {
    if (value->left == W_SEED_FRONTEND_NONE ||
        value->right == W_SEED_FRONTEND_NONE ||
        value->else_expression == W_SEED_FRONTEND_NONE ||
        (size_t)value->left >= result->written.expressions ||
        (size_t)value->right >= result->written.expressions ||
        (size_t)value->else_expression >= result->written.expressions ||
        value->inferred_type == W_SEED_FRONTEND_NONE ||
        (size_t)value->inferred_type >= result->written.types ||
        !frontend_scalar_if_tree_ok(input, module_index, function_index,
                                    document_index, value->left, true,
                                    depth + 1u) ||
        !frontend_scalar_if_tree_ok(input, module_index, function_index,
                                    document_index, value->right, false,
                                    depth + 1u) ||
        !frontend_scalar_if_tree_ok(input, module_index, function_index,
                                    document_index, value->else_expression,
                                    false, depth + 1u) ||
        !frontend_expression_is_bool(output, &output->expressions[value->left]) ||
        output->expressions[value->right].inferred_type ==
            W_SEED_FRONTEND_NONE ||
        output->expressions[value->else_expression].inferred_type ==
            W_SEED_FRONTEND_NONE ||
        (size_t)output->expressions[value->right].inferred_type >=
            result->written.types ||
        (size_t)output->expressions[value->else_expression].inferred_type >=
            result->written.types ||
        !frontend_type_is_scalar(&output->types[value->inferred_type]) ||
        !frontend_type_is_scalar(
            &output->types[output->expressions[value->right].inferred_type]) ||
        !frontend_type_is_scalar(&output->types[output->expressions[
                                            value->else_expression]
                                            .inferred_type]) ||
        !frontend_supported_types_equal_for_input(
            input,
            &output->types[value->inferred_type],
            &output->types[output->expressions[value->right].inferred_type]) ||
        !frontend_supported_types_equal_for_input(
            input,
            &output->types[output->expressions[value->right].inferred_type],
            &output->types[output->expressions[value->else_expression]
                                 .inferred_type]) ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u || value->has_bool_value ||
        value->has_integer_value || value->first_interpolation_segment !=
                                         W_SEED_FRONTEND_NONE ||
        value->interpolation_segment_count != 0u ||
        !frontend_value_tree_ok(input, module_index, function_index,
                                document_index, use_statement, value->left,
                                depth + 1u, expression_cursor, segment_cursor,
                                const_byte_cursor, value_total, segment_total,
                                value_bytes, call_total, argument_total,
                                logical_total) ||
        !frontend_value_tree_ok(input, module_index, function_index,
                                document_index, use_statement, value->right,
                                depth + 1u, expression_cursor, segment_cursor,
                                const_byte_cursor, value_total, segment_total,
                                value_bytes, call_total, argument_total,
                                logical_total) ||
        !frontend_value_tree_ok(
            input, module_index, function_index, document_index, use_statement,
            value->else_expression, depth + 1u, expression_cursor,
            segment_cursor, const_byte_cursor, value_total, segment_total,
            value_bytes, call_total, argument_total, logical_total) ||
        (size_t)root_index != *expression_cursor ||
        !add_size(*value_total, 1u, value_total) ||
        !add_size(*logical_total, 1u, logical_total) ||
        !add_size(*expression_cursor, 1u, expression_cursor))
      return false;
    return true;
  }

  if (value->kind == W_SEED_FRONTEND_EXPR_UNARY) {
    const bool logical_not = text_is(value->operator_text, "!");
    const bool numeric_negate = text_is(value->operator_text, "-");
    if ((!logical_not && !numeric_negate) ||
        value->left == W_SEED_FRONTEND_NONE ||
        (size_t)value->left >= result->written.expressions ||
        value->right != W_SEED_FRONTEND_NONE ||
        value->inferred_type == W_SEED_FRONTEND_NONE ||
        (size_t)value->inferred_type >= result->written.types ||
        (logical_not
             ? !frontend_expression_is_bool(output, value)
             : !frontend_expression_is_i64(output, value)) ||
        output->expressions[value->left].inferred_type ==
            W_SEED_FRONTEND_NONE ||
        (size_t)output->expressions[value->left].inferred_type >=
            result->written.types ||
        (logical_not
             ? !frontend_expression_is_bool(
                   output, &output->expressions[value->left])
             : !frontend_expression_is_i64(
                   output, &output->expressions[value->left])) ||
        !frontend_unary_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u || value->has_bool_value ||
        value->has_integer_value ||
        !frontend_value_tree_ok(input, module_index, function_index,
                                document_index, use_statement, value->left,
                                depth + 1u, expression_cursor, segment_cursor,
                                const_byte_cursor, value_total, segment_total,
                                value_bytes, call_total, argument_total,
                                logical_total) ||
        (size_t)root_index != *expression_cursor ||
        !add_size(*value_total, 1u, value_total) ||
        !add_size(*expression_cursor, 1u, expression_cursor))
      return false;
    return true;
  }

  if (value->kind == W_SEED_FRONTEND_EXPR_BINARY) {
    const w_seed_hir0_logical_operator logical =
        hir_logical_operator(value->operator_text);
    if (logical != W_SEED_HIR0_LOGICAL_NONE) {
      if (value->left == W_SEED_FRONTEND_NONE ||
          value->right == W_SEED_FRONTEND_NONE ||
          (size_t)value->left >= result->written.expressions ||
          (size_t)value->right >= result->written.expressions ||
          value->inferred_type == W_SEED_FRONTEND_NONE ||
          output->types[value->inferred_type].kind !=
              W_SEED_FRONTEND_TYPE_BOOL ||
          output->expressions[value->left].inferred_type ==
              W_SEED_FRONTEND_NONE ||
          output->expressions[value->right].inferred_type ==
              W_SEED_FRONTEND_NONE ||
          output->types[output->expressions[value->left].inferred_type].kind !=
              W_SEED_FRONTEND_TYPE_BOOL ||
          output->types[output->expressions[value->right].inferred_type].kind !=
              W_SEED_FRONTEND_TYPE_BOOL ||
          !frontend_value_has_no_resolution(value) ||
          value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
          value->const_byte_offset != W_SEED_FRONTEND_NONE ||
          value->const_byte_count != 0u || value->has_bool_value ||
          value->has_integer_value ||
          !frontend_value_tree_ok(input, module_index, function_index,
                                  document_index, use_statement, value->left,
                                  depth + 1u, expression_cursor, segment_cursor,
                                  const_byte_cursor, value_total, segment_total,
                                  value_bytes, call_total, argument_total,
                                  logical_total) ||
          !frontend_value_tree_ok(input, module_index, function_index,
                                  document_index, use_statement, value->right,
                                  depth + 1u, expression_cursor, segment_cursor,
                                  const_byte_cursor, value_total, segment_total,
                                  value_bytes, call_total, argument_total,
                                  logical_total) ||
          (size_t)root_index != *expression_cursor ||
          !add_size(*value_total, 2u, value_total) ||
          !add_size(*logical_total, 1u, logical_total) ||
          !add_size(*expression_cursor, 1u, expression_cursor))
        return false;
      return true;
    }
    if (frontend_usize_count_comparison_ok(
            input, module_index, function_index, document_index, value)) {
      const bool left_literal =
          frontend_usize_count_comparison_literal_ok(
              input, module_index, function_index, document_index,
              &output->expressions[value->left]);
      const bool right_literal =
          frontend_usize_count_comparison_literal_ok(
              input, module_index, function_index, document_index,
              &output->expressions[value->right]);
      if (!(left_literal
                ? frontend_usize_literal_leaf_ok(
                      input, module_index, function_index, document_index,
                      value->left, expression_cursor, value_total)
                : frontend_value_tree_ok(
                      input, module_index, function_index, document_index,
                      use_statement, value->left, depth + 1u,
                      expression_cursor, segment_cursor, const_byte_cursor,
                      value_total, segment_total, value_bytes, call_total,
                      argument_total, logical_total)) ||
          !(right_literal
                ? frontend_usize_literal_leaf_ok(
                      input, module_index, function_index, document_index,
                      value->right, expression_cursor, value_total)
                : frontend_value_tree_ok(
                      input, module_index, function_index, document_index,
                      use_statement, value->right, depth + 1u,
                      expression_cursor, segment_cursor, const_byte_cursor,
                      value_total, segment_total, value_bytes, call_total,
                      argument_total, logical_total)) ||
          (size_t)root_index != *expression_cursor ||
          !add_size(*value_total, 1u, value_total) ||
          !add_size(*expression_cursor, 1u, expression_cursor))
        return false;
      return true;
    }
    const w_seed_hir0_binary_operator operation =
        hir_binary_operator(value->operator_text);
    const bool comparison = operation >= W_SEED_HIR0_BINARY_EQUAL &&
                            operation <= W_SEED_HIR0_BINARY_GREATER_EQUAL;
    if (value->left == W_SEED_FRONTEND_NONE ||
        value->right == W_SEED_FRONTEND_NONE ||
        !frontend_value_tree_ok(input, module_index, function_index,
                                document_index, use_statement, value->left,
                                depth + 1u, expression_cursor, segment_cursor,
                                const_byte_cursor, value_total, segment_total,
                                value_bytes, call_total, argument_total,
                                logical_total) ||
        !frontend_value_tree_ok(input, module_index, function_index,
                                document_index, use_statement, value->right,
                                depth + 1u, expression_cursor, segment_cursor,
                                const_byte_cursor, value_total, segment_total,
                                value_bytes, call_total, argument_total,
                                logical_total) ||
        (size_t)root_index != *expression_cursor ||
        value->inferred_type == W_SEED_FRONTEND_NONE ||
        (comparison ? output->types[value->inferred_type].kind !=
                          W_SEED_FRONTEND_TYPE_BOOL
                    : !frontend_expression_is_i64(output, value)) ||
        !frontend_expression_is_i64(output, &output->expressions[value->left]) ||
        !frontend_expression_is_i64(output, &output->expressions[value->right]) ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u || value->has_bool_value ||
        value->has_integer_value ||
        (uint32_t)operation > (uint32_t)W_SEED_HIR0_BINARY_GREATER_EQUAL ||
        !add_size(*value_total, 1u, value_total) ||
        !add_size(*expression_cursor, 1u, expression_cursor))
      return false;
    return true;
  }

  if (value->kind == W_SEED_FRONTEND_EXPR_CALL) {
    return frontend_call_expression_ok(
        input, module_index, function_index, document_index, use_statement,
        root_index, true, expression_cursor, segment_cursor,
        const_byte_cursor, call_total, argument_total, value_total,
        segment_total, value_bytes, logical_total);
  }

  if (value->kind == W_SEED_FRONTEND_EXPR_MEMBER) {
    if (!frontend_external_member_value_ok(input, module_index, function_index,
                                           document_index, value) ||
        (size_t)value->left != *expression_cursor ||
        (size_t)root_index != *expression_cursor + 1u ||
        !add_size(*expression_cursor, 2u, expression_cursor) ||
        !add_size(*value_total, 2u, value_total))
      return false;
    return true;
  }

  if (value->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS) {
    if (value->left == W_SEED_FRONTEND_NONE ||
        value->right != W_SEED_FRONTEND_NONE ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u || value->has_bool_value ||
        value->has_integer_value ||
        !frontend_value_tree_ok(input, module_index, function_index,
                                document_index, use_statement, value->left,
                                depth + 1u,
                                expression_cursor, segment_cursor,
                                const_byte_cursor, value_total, segment_total,
                                value_bytes, call_total, argument_total,
                                logical_total) ||
        (size_t)root_index != *expression_cursor ||
        !add_size(*expression_cursor, 1u, expression_cursor))
      return false;
    return true;
  }

  if (value->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING) {
    if (value->left != W_SEED_FRONTEND_NONE ||
        value->right != W_SEED_FRONTEND_NONE ||
        (value->inferred_type != W_SEED_FRONTEND_NONE &&
         output->types[value->inferred_type].kind !=
             W_SEED_FRONTEND_TYPE_STRING) ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u || value->has_bool_value ||
        value->has_integer_value ||
        value->first_interpolation_segment != *segment_cursor ||
        !range_valid(value->first_interpolation_segment,
                     value->interpolation_segment_count,
                     result->written.interpolation_segments))
      return false;
    for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_frontend_interpolation_segment *segment =
          &output->interpolation_segments[*segment_cursor];
      if (segment->owner_expression != root_index ||
          segment->ordinal != ordinal ||
          !frontend_span_ok(&input->frontend_input->documents[document_index],
                            segment->span))
        return false;
      if (segment->kind == W_SEED_FRONTEND_INTERPOLATION_TEXT) {
        if (segment->expression_index != W_SEED_FRONTEND_NONE ||
            segment->const_byte_offset == W_SEED_FRONTEND_NONE ||
            (size_t)segment->const_byte_offset != *const_byte_cursor ||
            !range_valid(segment->const_byte_offset, segment->const_byte_count,
                         result->written.const_bytes) ||
            !add_size(*const_byte_cursor, segment->const_byte_count,
                      const_byte_cursor) ||
            !add_size(*value_bytes, segment->const_byte_count, value_bytes))
          return false;
      } else if (segment->kind ==
                 W_SEED_FRONTEND_INTERPOLATION_EXPRESSION) {
        if (segment->const_byte_offset != W_SEED_FRONTEND_NONE ||
            segment->const_byte_count != 0u ||
            !frontend_value_tree_ok(
                input, module_index, function_index, document_index,
                use_statement,
                segment->expression_index, depth + 1u, expression_cursor,
                segment_cursor, const_byte_cursor, value_total, segment_total,
                value_bytes, call_total, argument_total, logical_total))
          return false;
      } else {
        return false;
      }
      if (!add_size(*segment_cursor, 1u, segment_cursor) ||
          !add_size(*segment_total, 1u, segment_total))
        return false;
    }
    if ((size_t)root_index != *expression_cursor ||
        !add_size(*value_total, 1u, value_total) ||
        !add_size(*expression_cursor, 1u, expression_cursor))
      return false;
    return true;
  }

  if (value->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE) {
    const bool local_case = value->enum_index != W_SEED_FRONTEND_NONE;
    if (!(local_case ? frontend_local_enum_case_value_ok(input, value)
                     : frontend_external_exit_code_case_ok(input, value)) ||
        (size_t)root_index != *expression_cursor ||
        !add_size(*value_total, 1u, value_total) ||
        !add_size(*expression_cursor, 1u, expression_cursor))
      return false;
    return true;
  }

  if ((size_t)root_index != *expression_cursor ||
      value->left != W_SEED_FRONTEND_NONE ||
      value->right != W_SEED_FRONTEND_NONE ||
      value->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
      value->interpolation_segment_count != 0u)
    return false;
  if (value->kind == W_SEED_FRONTEND_EXPR_STRING) {
    if ((value->inferred_type != W_SEED_FRONTEND_NONE &&
         output->types[value->inferred_type].kind !=
             W_SEED_FRONTEND_TYPE_STRING) ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->has_bool_value || value->has_integer_value ||
        value->const_byte_offset == W_SEED_FRONTEND_NONE ||
        (size_t)value->const_byte_offset != *const_byte_cursor ||
        !range_valid(value->const_byte_offset, value->const_byte_count,
                     result->written.const_bytes) ||
        !add_size(*const_byte_cursor, value->const_byte_count,
                  const_byte_cursor) ||
        !add_size(*value_bytes, value->const_byte_count, value_bytes))
      return false;
  } else if (value->kind == W_SEED_FRONTEND_EXPR_INTEGER) {
    int64_t ignored = 0;
    if (value->inferred_type == W_SEED_FRONTEND_NONE ||
        output->types[value->inferred_type].kind !=
            W_SEED_FRONTEND_TYPE_INTEGER ||
        !frontend_integer_i64(value, &ignored) ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u || value->has_bool_value)
      return false;
  } else if (value->kind == W_SEED_FRONTEND_EXPR_BOOL) {
    if (value->inferred_type == W_SEED_FRONTEND_NONE ||
        output->types[value->inferred_type].kind != W_SEED_FRONTEND_TYPE_BOOL ||
        !value->has_bool_value || value->has_integer_value ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u)
      return false;
  } else if (value->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER) {
    if (value->inferred_type == W_SEED_FRONTEND_NONE ||
        !frontend_hir_type_supported(input,
            &output->types[value->inferred_type]) ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u || value->has_bool_value ||
        value->has_integer_value ||
        value->resolved_function_index != W_SEED_FRONTEND_NONE ||
        value->resolved_callee_kind != W_SEED_FRONTEND_CALLEE_NONE ||
        value->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
        value->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
        value->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
        value->resolved_local_ordinal != W_SEED_FRONTEND_NONE ||
        value->resolved_const_declaration != W_SEED_FRONTEND_NONE ||
        value->member_name.length != 0u || !text_valid(value->member_name))
      return false;
    if (value->resolved_pattern_capture != W_SEED_FRONTEND_NONE) {
      if (!frontend_pattern_capture_read_ok(
              input, module_index, function_index, root_index, value))
        return false;
    } else if (value->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE) {
      const w_seed_frontend_function *function =
          &output->functions[function_index];
      if (value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
          value->resolved_parameter_ordinal >= function->parameter_count)
        return false;
      const size_t parameter_index =
          (size_t)function->first_parameter +
          value->resolved_parameter_ordinal;
      if (parameter_index >= result->written.parameters)
        return false;
      const w_seed_frontend_parameter *parameter =
          &output->parameters[parameter_index];
      if (parameter->owner_function != function_index ||
          parameter->module_index != module_index ||
          !frontend_supported_types_equal_for_input(
              input,
              &output->types[parameter->type_index],
              &output->types[value->inferred_type]) ||
          !text_equal(parameter->name, value->spelling) ||
          frontend_external_type_is(input, parameter->type_index, 0u, 0u) ||
          frontend_external_type_is(input, parameter->type_index, 0u, 1u))
        return false;
    } else {
      if (value->resolved_binding_statement == W_SEED_FRONTEND_NONE ||
          (size_t)value->resolved_binding_statement >= use_statement ||
          (size_t)value->resolved_binding_statement >=
              result->written.statements)
        return false;
      const w_seed_frontend_statement *binding =
          &output->statements[value->resolved_binding_statement];
      if ((binding->kind != W_SEED_FRONTEND_STMT_LET &&
           binding->kind != W_SEED_FRONTEND_STMT_VAR) ||
          binding->owner_function != function_index ||
          binding->module_index != module_index ||
          binding->effective_type != value->inferred_type ||
          !text_equal(binding->binding_name, value->spelling))
        return false;
    }
  } else {
    return false;
  }
  return add_size(*value_total, 1u, value_total) &&
         add_size(*expression_cursor, 1u, expression_cursor);
}

static bool frontend_string_value_root(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, uint32_t expression_index,
    size_t depth) {
  if (output == NULL || result == NULL || output->expressions == NULL ||
      depth > 256u || (size_t)expression_index >= result->written.expressions)
    return false;
  const w_seed_frontend_expression *value =
      &output->expressions[expression_index];
  if (value->kind == W_SEED_FRONTEND_EXPR_STRING ||
      value->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING)
    return true;
  if (value->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS &&
      value->left != W_SEED_FRONTEND_NONE)
    return frontend_string_value_root(output, result, value->left, depth + 1u);
  return value->inferred_type != W_SEED_FRONTEND_NONE &&
         (size_t)value->inferred_type < result->written.types &&
         output->types[value->inferred_type].kind ==
             W_SEED_FRONTEND_TYPE_STRING;
}

/* A local function identity is global across the frontend function arena.
 * Same-module calls use lexical lookup; cross-module calls additionally need
 * one resolver-owned named import from the caller to an exported target. */
static bool frontend_local_call_target_ok(
    const w_seed_hir0_input *input, size_t caller_module,
    w_seed_frontend_text callee_name, size_t target_function_index) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL ||
      caller_module >= input->frontend_result->written.modules ||
      target_function_index >= input->frontend_result->written.functions)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  const w_seed_frontend_function *target =
      &output->functions[target_function_index];
  if (target->module_index == caller_module)
    return text_equal(callee_name, target->name);
  if (!target->exported || target->module_index >= result->written.modules)
    return false;
  const w_seed_frontend_module *module = &output->modules[caller_module];
  size_t matches = 0u;
  for (size_t ordinal = 0u; ordinal < module->import_count; ordinal += 1u) {
    const size_t import_index = (size_t)module->first_import + ordinal;
    if (import_index >= result->written.imports) return false;
    const w_seed_frontend_import *import = &output->imports[import_index];
    if (import->module_index != caller_module ||
        import->target_kind != W_SEED_FRONTEND_IMPORT_LOCAL_DOCUMENT ||
        import->target_index != target->module_index ||
        !range_valid(import->first_item, import->item_count,
                     result->written.import_items))
      continue;
    for (size_t item_ordinal = 0u; item_ordinal < import->item_count;
         item_ordinal += 1u) {
      const w_seed_frontend_import_item *item =
          &output->import_items[(size_t)import->first_item + item_ordinal];
      if (item->module_index == caller_module &&
          text_equal(item->local_name, callee_name) &&
          text_equal(item->name, target->name) &&
          !add_size(matches, 1u, &matches))
        return false;
    }
  }
  return matches == 1u;
}

/* Validate one direct call expression while consuming the frontend's dense
 * postorder expression and argument ranges. A call used as a statement must
 * return Unit. A call used as a binding initializer must be local and return
 * one scalar value; that result becomes one HIR SSA value. */
static bool frontend_call_expression_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index, size_t statement_index,
    uint32_t root_index, bool result_value, size_t *expression_cursor,
    size_t *interpolation_segment_cursor, size_t *const_byte_cursor,
    size_t *call_total, size_t *argument_total, size_t *value_total,
    size_t *segment_total, size_t *value_bytes, size_t *logical_total) {
  if (input == NULL || expression_cursor == NULL ||
      interpolation_segment_cursor == NULL || const_byte_cursor == NULL ||
      call_total == NULL || argument_total == NULL || value_total == NULL ||
      segment_total == NULL || value_bytes == NULL || logical_total == NULL)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (root_index == W_SEED_FRONTEND_NONE ||
      (size_t)root_index >= result->written.expressions)
    return false;
  const w_seed_frontend_expression *call = &output->expressions[root_index];
  const bool host_call =
      call->resolved_callee_kind ==
      W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL;
  const bool local_call =
      call->resolved_callee_kind == W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION;
  if (call->kind != W_SEED_FRONTEND_EXPR_CALL || !call->supported ||
      call->module_index != module_index ||
      call->owner_function != function_index ||
      call->left == W_SEED_FRONTEND_NONE ||
      (size_t)call->left >= result->written.expressions ||
      (size_t)call->left != *expression_cursor ||
      call->right != W_SEED_FRONTEND_NONE ||
      call->first_argument != *argument_total ||
      !range_valid(call->first_argument, call->argument_count,
                   result->written.arguments) ||
      !frontend_span_ok(&input->frontend_input->documents[document_index],
                        call->span))
    return false;
  const w_seed_frontend_expression *callee = &output->expressions[call->left];
  const bool enum_constructor =
      callee->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE &&
      callee->enum_index != W_SEED_FRONTEND_NONE;
  if (enum_constructor) {
    if (!result_value || host_call || local_call ||
        call->resolved_callee_kind != W_SEED_FRONTEND_CALLEE_NONE ||
        call->resolved_function_index != W_SEED_FRONTEND_NONE ||
        call->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
        call->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
        call->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
        !frontend_local_enum_case_value_ok(input, callee) ||
        callee->module_index != module_index ||
        callee->owner_function != function_index ||
        call->enum_index != callee->enum_index ||
        call->enum_case_index != callee->enum_case_index ||
        call->inferred_type == W_SEED_FRONTEND_NONE ||
        !frontend_supported_types_equal_for_input(
            input, &output->types[call->inferred_type],
            &output->types[callee->inferred_type]) ||
        !add_size(*expression_cursor, 1u, expression_cursor))
      return false;
    const w_seed_frontend_enum_case *enum_case =
        &output->enum_cases[callee->enum_case_index];
    if (enum_case->payload_count == 0u ||
        enum_case->payload_count != call->argument_count)
      return false;
    for (size_t argument_ordinal = 0u;
         argument_ordinal < call->argument_count; argument_ordinal += 1u) {
      const size_t argument_index =
          (size_t)call->first_argument + argument_ordinal;
      const w_seed_frontend_argument *argument =
          &output->arguments[argument_index];
      if (argument->module_index != module_index ||
          argument->owner_expression != call->left ||
          argument->expression_index == W_SEED_FRONTEND_NONE ||
          (size_t)argument->expression_index >= result->written.expressions ||
          argument->resolved_parameter_ordinal == W_SEED_FRONTEND_NONE ||
          argument->resolved_parameter_ordinal >= enum_case->payload_count ||
          !frontend_span_ok(&input->frontend_input->documents[document_index],
                            argument->span))
        return false;
      for (size_t prior = 0u; prior < argument_ordinal; prior += 1u)
        if (output->arguments[(size_t)call->first_argument + prior]
                .resolved_parameter_ordinal ==
            argument->resolved_parameter_ordinal)
          return false;
      const w_seed_frontend_enum_case_parameter *parameter =
          &output->enum_case_parameters[(size_t)enum_case->first_payload +
                                        argument->resolved_parameter_ordinal];
      const w_seed_frontend_expression *argument_value =
          &output->expressions[argument->expression_index];
      if (parameter->owner_case != callee->enum_case_index ||
          parameter->type_index == W_SEED_FRONTEND_NONE ||
          !frontend_enum_parameter_label_matches(parameter, argument->label) ||
          argument_value->inferred_type == W_SEED_FRONTEND_NONE ||
          !frontend_type_assignable_for_input(
              input,
              &output->types[argument_value->inferred_type],
              &output->types[parameter->type_index]) ||
          !frontend_value_tree_ok(
              input, module_index, function_index, document_index,
              statement_index, argument->expression_index, 0u,
              expression_cursor, interpolation_segment_cursor,
              const_byte_cursor, value_total, segment_total, value_bytes,
              call_total, argument_total, logical_total) ||
          !add_size(*argument_total, 1u, argument_total))
        return false;
    }
    return (size_t)root_index == *expression_cursor &&
           add_size(*expression_cursor, 1u, expression_cursor) &&
           add_size(*value_total, 1u, value_total);
  }
  const bool external_enum_constructor =
      callee->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE &&
      callee->enum_index == W_SEED_FRONTEND_NONE &&
      callee->resolved_callee_kind ==
          W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL &&
      callee->resolved_external_module_index == 0u &&
      callee->resolved_external_symbol_index == 5u;
  if (external_enum_constructor) {
    if (!result_value || host_call || local_call ||
        call->resolved_callee_kind !=
            W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL ||
        call->resolved_external_module_index != 0u ||
        call->resolved_external_symbol_index != 5u ||
        call->argument_count != 1u ||
        !frontend_external_enum_case_value_ok(input, callee, 5u) ||
        !add_size(*expression_cursor, 1u, expression_cursor))
      return false;
    const w_seed_frontend_external_symbol *failure =
        &input->frontend_input->external_modules[0].symbols[5];
    const w_seed_frontend_argument *argument =
        &output->arguments[call->first_argument];
    if (failure->parameter_count != 1u || failure->parameters == NULL ||
        !text_is(failure->parameters[0].name, "code") ||
        !text_is(failure->parameters[0].type, HIR0_I64_NAME) ||
        failure->parameters[0].label_kind !=
            W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY ||
        argument->module_index != module_index ||
        argument->owner_expression != call->left ||
        argument->expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)argument->expression_index >= result->written.expressions ||
        argument->resolved_parameter_ordinal != 0u ||
        argument->label.length != 0u ||
        !frontend_span_ok(&input->frontend_input->documents[document_index],
                          argument->span))
      return false;
    const w_seed_frontend_expression *payload =
        &output->expressions[argument->expression_index];
    int64_t code = 0;
    if (payload->kind != W_SEED_FRONTEND_EXPR_INTEGER ||
        !frontend_expression_is_i64(output, payload) ||
        !frontend_integer_i64(payload, &code) || code < 1 || code > 255 ||
        !frontend_value_tree_ok(
            input, module_index, function_index, document_index,
            statement_index, argument->expression_index, 0u,
            expression_cursor, interpolation_segment_cursor,
            const_byte_cursor, value_total, segment_total, value_bytes,
            call_total, argument_total, logical_total) ||
        !add_size(*argument_total, 1u, argument_total))
      return false;
    return (size_t)root_index == *expression_cursor &&
           add_size(*expression_cursor, 1u, expression_cursor) &&
           add_size(*value_total, 1u, value_total);
  }
  if ((!host_call && !local_call) || (result_value && !local_call))
    return false;
  if (callee->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
      !callee->supported || callee->module_index != module_index ||
      callee->owner_function != function_index ||
      callee->resolved_callee_kind != call->resolved_callee_kind ||
      callee->resolved_function_index != call->resolved_function_index ||
      callee->resolved_host_symbol_index != call->resolved_host_symbol_index ||
      callee->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
      callee->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
      !frontend_span_ok(&input->frontend_input->documents[document_index],
                        callee->span) ||
      !add_size(*expression_cursor, 1u, expression_cursor))
    return false;

  const w_seed_frontend_host_prelude_symbol *host = NULL;
  const w_seed_frontend_function *target_function = NULL;
  size_t parameter_count = 0u;
  uint32_t return_type = W_SEED_FRONTEND_NONE;
  if (host_call) {
    if (call->resolved_function_index != W_SEED_FRONTEND_NONE ||
        call->resolved_host_symbol_index == W_SEED_FRONTEND_NONE ||
        (size_t)call->resolved_host_symbol_index >=
            input->frontend_input->host_scope->symbol_count)
      return false;
    host = &input->frontend_input->host_scope
                ->symbols[call->resolved_host_symbol_index];
    if (!text_equal(callee->spelling, host->name) ||
        !text_is(host->return_type, "()"))
      return false;
    parameter_count = host->parameter_count;
  } else {
    if (call->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
        call->resolved_function_index == W_SEED_FRONTEND_NONE ||
        (size_t)call->resolved_function_index >= result->written.functions)
      return false;
    target_function = &output->functions[call->resolved_function_index];
    if (!frontend_local_call_target_ok(
            input, module_index, callee->spelling,
            (size_t)call->resolved_function_index) ||
        target_function->return_type == W_SEED_FRONTEND_NONE ||
        (size_t)target_function->return_type >= result->written.types)
      return false;
    return_type = target_function->return_type;
    const w_seed_frontend_type_kind return_kind =
        output->types[return_type].kind;
    if ((!result_value && return_kind != W_SEED_FRONTEND_TYPE_UNIT) ||
        (result_value && return_kind != W_SEED_FRONTEND_TYPE_INTEGER &&
         return_kind != W_SEED_FRONTEND_TYPE_BOOL &&
         !frontend_local_enum_type_supported(input,
                                             &output->types[return_type])) ||
        (return_kind == W_SEED_FRONTEND_TYPE_INTEGER &&
         (!output->types[return_type].is_signed ||
          output->types[return_type].bit_width != 64u)))
      return false;
    parameter_count = target_function->parameter_count;
  }
  if (parameter_count != call->argument_count ||
      (!host_call &&
       (call->inferred_type == W_SEED_FRONTEND_NONE ||
        !frontend_supported_types_equal_for_input(
            input, &output->types[call->inferred_type],
            &output->types[return_type]))))
    return false;

  for (size_t argument_ordinal = 0u;
       argument_ordinal < call->argument_count; argument_ordinal += 1u) {
    const size_t argument_index =
        (size_t)call->first_argument + argument_ordinal;
    const w_seed_frontend_argument *argument =
        &output->arguments[argument_index];
    if (argument->module_index != module_index ||
        argument->owner_expression != call->left ||
        argument->expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)argument->expression_index >= result->written.expressions ||
        argument->resolved_parameter_ordinal == W_SEED_FRONTEND_NONE ||
        argument->resolved_parameter_ordinal >= parameter_count ||
        !frontend_span_ok(&input->frontend_input->documents[document_index],
                          argument->span))
      return false;
    for (size_t prior = 0u; prior < argument_ordinal; prior += 1u) {
      const w_seed_frontend_argument *prior_argument =
          &output->arguments[(size_t)call->first_argument + prior];
      if (prior_argument->resolved_parameter_ordinal ==
          argument->resolved_parameter_ordinal)
        return false;
    }
    const w_seed_frontend_expression *value =
        &output->expressions[argument->expression_index];
    uint32_t expected_type = W_SEED_FRONTEND_NONE;
    if (host_call) {
      const w_seed_frontend_external_parameter *host_parameter =
          &host->parameters[argument->resolved_parameter_ordinal];
      if (!text_is(host_parameter->type, HIR0_STRING_NAME) ||
          !frontend_host_label_matches(host_parameter->label_kind,
                                       host_parameter->name, argument->label))
        return false;
    } else {
      const size_t parameter_index =
          (size_t)target_function->first_parameter +
          argument->resolved_parameter_ordinal;
      if (parameter_index >= result->written.parameters)
        return false;
      const w_seed_frontend_parameter *parameter =
          &output->parameters[parameter_index];
      if (!frontend_parameter_label_matches(parameter, argument->label))
        return false;
      expected_type = parameter->type_index;
    }
    if ((local_call &&
         (expected_type == W_SEED_FRONTEND_NONE ||
          value->inferred_type == W_SEED_FRONTEND_NONE ||
         !frontend_type_assignable_for_input(
              input, &output->types[value->inferred_type],
              &output->types[expected_type]))) ||
        (host_call && !frontend_string_value_root(
                          output, result, argument->expression_index, 0u)) ||
        !frontend_value_tree_ok(
            input, module_index, function_index, document_index,
            statement_index, argument->expression_index, 0u,
            expression_cursor, interpolation_segment_cursor,
            const_byte_cursor, value_total, segment_total, value_bytes,
            call_total, argument_total, logical_total) ||
        !add_size(*argument_total, 1u, argument_total))
      return false;
  }
  if ((size_t)root_index != *expression_cursor ||
      !add_size(*expression_cursor, 1u, expression_cursor) ||
      !add_size(*call_total, 1u, call_total) ||
      (result_value && !add_size(*value_total, 1u, value_total)))
    return false;
  return true;
}

/* Async0 retains structured launch/join evidence but admits only local calls
 * whose ordinary entry is proven non-suspending.  An explicit async
 * declaration may use its W-1484 direct entry when that proof is available;
 * the runtime carrier is still the callee's scalar result and the wrapper
 * records add no HIR values. */
static bool frontend_async_launch_expression_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index, size_t statement_index,
    uint32_t root_index, size_t *expression_cursor,
    size_t *interpolation_segment_cursor, size_t *const_byte_cursor,
    size_t *call_total, size_t *argument_total, size_t *value_total,
    size_t *segment_total, size_t *value_bytes, size_t *logical_total) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || root_index == W_SEED_FRONTEND_NONE ||
      (size_t)root_index >= input->frontend_result->written.expressions)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  const w_seed_frontend_expression *launch = &output->expressions[root_index];
  if (!frontend_value_common_ok(input, launch, module_index, function_index,
                                document_index) ||
      launch->kind != W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH ||
      !launch->supported || !text_is(launch->operator_text, "async") ||
      launch->left != launch->task_call_expression ||
      launch->task_call_expression == W_SEED_FRONTEND_NONE ||
      (size_t)launch->task_call_expression >= result->written.expressions ||
      launch->right != W_SEED_FRONTEND_NONE ||
      launch->first_argument != W_SEED_FRONTEND_NONE ||
      launch->argument_count != 0u ||
      launch->inferred_type == W_SEED_FRONTEND_NONE ||
      (size_t)launch->inferred_type >= result->written.types ||
      output->types[launch->inferred_type].kind != W_SEED_FRONTEND_TYPE_TASK ||
      output->types[launch->inferred_type].task_result_type !=
          launch->task_result_type ||
      launch->task_binding_statement != W_SEED_FRONTEND_NONE ||
      launch->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
      launch->else_expression != W_SEED_FRONTEND_NONE ||
      launch->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
      launch->interpolation_segment_count != 0u ||
      launch->const_byte_offset != W_SEED_FRONTEND_NONE ||
      launch->const_byte_count != 0u || launch->has_bool_value ||
      launch->has_integer_value || !frontend_value_has_no_resolution(launch))
    return false;
  const w_seed_frontend_expression *call =
      &output->expressions[launch->task_call_expression];
  if (call->kind != W_SEED_FRONTEND_EXPR_CALL ||
      call->resolved_callee_kind != W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION ||
      call->resolved_function_index == W_SEED_FRONTEND_NONE ||
      (size_t)call->resolved_function_index >= result->written.functions ||
      output->functions[call->resolved_function_index].is_throws ||
      call->inferred_type != launch->task_result_type ||
      !frontend_call_expression_ok(
          input, module_index, function_index, document_index, statement_index,
          launch->task_call_expression, true, expression_cursor,
          interpolation_segment_cursor, const_byte_cursor, call_total,
          argument_total, value_total, segment_total, value_bytes,
          logical_total) ||
      (size_t)root_index != *expression_cursor ||
      !add_size(*expression_cursor, 1u, expression_cursor))
    return false;
  return true;
}

static bool frontend_await_expression_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index, size_t statement_index,
    uint32_t root_index, size_t *expression_cursor,
    size_t *interpolation_segment_cursor, size_t *const_byte_cursor,
    size_t *call_total, size_t *argument_total, size_t *value_total,
    size_t *segment_total, size_t *value_bytes, size_t *logical_total) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || root_index == W_SEED_FRONTEND_NONE ||
      (size_t)root_index >= input->frontend_result->written.expressions)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  const w_seed_frontend_expression *awaited = &output->expressions[root_index];
  if (!frontend_value_common_ok(input, awaited, module_index, function_index,
                                document_index) ||
      awaited->kind != W_SEED_FRONTEND_EXPR_AWAIT || !awaited->supported ||
      !text_is(awaited->operator_text, "await") ||
      awaited->left == W_SEED_FRONTEND_NONE ||
      (size_t)awaited->left >= result->written.expressions ||
      awaited->right != W_SEED_FRONTEND_NONE ||
      awaited->first_argument != W_SEED_FRONTEND_NONE ||
      awaited->argument_count != 0u ||
      awaited->task_call_expression != W_SEED_FRONTEND_NONE ||
      awaited->task_binding_statement == W_SEED_FRONTEND_NONE ||
      (size_t)awaited->task_binding_statement >= statement_index ||
      (size_t)awaited->task_binding_statement >= result->written.statements ||
      awaited->inferred_type == W_SEED_FRONTEND_NONE ||
      awaited->inferred_type != awaited->task_result_type ||
      awaited->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
      awaited->else_expression != W_SEED_FRONTEND_NONE ||
      awaited->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
      awaited->interpolation_segment_count != 0u ||
      awaited->const_byte_offset != W_SEED_FRONTEND_NONE ||
      awaited->const_byte_count != 0u || awaited->has_bool_value ||
      awaited->has_integer_value || !frontend_value_has_no_resolution(awaited))
    return false;
  const w_seed_frontend_expression *task = &output->expressions[awaited->left];
  const w_seed_frontend_statement *launch_statement =
      &output->statements[awaited->task_binding_statement];
  if (task->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER || !task->supported ||
      task->resolved_binding_statement != awaited->task_binding_statement ||
      task->inferred_type == W_SEED_FRONTEND_NONE ||
      (size_t)task->inferred_type >= result->written.types ||
      output->types[task->inferred_type].kind != W_SEED_FRONTEND_TYPE_TASK ||
      output->types[task->inferred_type].task_result_type !=
          awaited->task_result_type ||
      launch_statement->kind != W_SEED_FRONTEND_STMT_LET ||
      launch_statement->owner_function != function_index ||
      launch_statement->module_index != module_index ||
      launch_statement->declared_type != W_SEED_FRONTEND_NONE ||
      launch_statement->effective_type != task->inferred_type ||
      launch_statement->expression_index == W_SEED_FRONTEND_NONE ||
      (size_t)launch_statement->expression_index >=
          result->written.expressions ||
      output->expressions[launch_statement->expression_index].kind !=
          W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH ||
      !frontend_value_tree_ok(
          input, module_index, function_index, document_index, statement_index,
          awaited->left, 0u, expression_cursor, interpolation_segment_cursor,
          const_byte_cursor, value_total, segment_total, value_bytes,
          call_total, argument_total, logical_total) ||
      (size_t)root_index != *expression_cursor ||
      !add_size(*expression_cursor, 1u, expression_cursor))
    return false;
  return true;
}

/* The normalized statement table is a graph, not a flat source-order list:
 * an IF record owns two sibling chains while its own next_sibling belongs to
 * the enclosing chain.  Validate that graph before consuming expression
 * ranges.  Every statement must have exactly one incoming structural edge,
 * and all edges must point forward in the append-only record range. */
static bool frontend_statement_relations_ok(const w_seed_hir0_input *input,
                                            size_t function_index,
                                            size_t *if_total) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || if_total == NULL ||
      input->frontend_output->functions == NULL ||
      (input->frontend_output->statements == NULL &&
       input->frontend_result->written.statements != 0u) ||
      function_index >= input->frontend_result->written.functions)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  const w_seed_frontend_function *function = &output->functions[function_index];
  const size_t first = function->first_statement;
  const size_t count = function->statement_count;
  if (!range_valid(function->first_statement, function->statement_count,
                   result->written.statements) ||
      (count != 0u && first > UINT32_MAX))
    return false;
  if (count == 0u) return true;
  size_t local_if_count = 0u;
  for (size_t offset = 0u; offset < count; offset += 1u) {
    const size_t index = first + offset;
    const w_seed_frontend_statement *statement = &output->statements[index];
    if (statement->module_index != function->module_index ||
        statement->owner_function != function_index)
      return false;
    const size_t document_index =
        output->modules[function->module_index].document_index;
    if (!frontend_span_ok(&input->frontend_input->documents[document_index],
                          statement->span))
      return false;
    if (statement->next_sibling != W_SEED_FRONTEND_NONE &&
        ((size_t)statement->next_sibling < first ||
         (size_t)statement->next_sibling >= first + count ||
         statement->next_sibling <= index))
      return false;
    const bool structured =
        statement->kind == W_SEED_FRONTEND_STMT_IF ||
        statement->kind == W_SEED_FRONTEND_STMT_GUARD ||
        statement->kind == W_SEED_FRONTEND_STMT_FOR ||
        statement->kind == W_SEED_FRONTEND_STMT_REPEAT ||
        statement->first_child != W_SEED_FRONTEND_NONE ||
        statement->child_count != 0u ||
        statement->else_child != W_SEED_FRONTEND_NONE;
    if (statement->condition_expression != W_SEED_FRONTEND_NONE &&
        statement->condition_expression != statement->expression_index)
      return false;
    if (statement->kind == W_SEED_FRONTEND_STMT_IF) {
      if (statement->condition_expression == W_SEED_FRONTEND_NONE ||
          !add_size(local_if_count, 1u, &local_if_count))
        return false;
    } else if (statement->kind == W_SEED_FRONTEND_STMT_GUARD &&
               statement->condition_expression == W_SEED_FRONTEND_NONE) {
      return false;
    } else if (!structured &&
               statement->condition_expression != W_SEED_FRONTEND_NONE) {
      return false;
    }
    if ((statement->first_child != W_SEED_FRONTEND_NONE &&
         ((size_t)statement->first_child < first ||
          (size_t)statement->first_child >= first + count ||
          statement->first_child <= index)) ||
        (statement->else_child != W_SEED_FRONTEND_NONE &&
         ((size_t)statement->else_child < first ||
          (size_t)statement->else_child >= first + count ||
          statement->else_child <= index)))
      return false;
    if ((statement->first_child == W_SEED_FRONTEND_NONE) !=
        (statement->child_count == 0u))
      return false;
    if (statement->first_child != W_SEED_FRONTEND_NONE) {
      uint32_t cursor = statement->first_child;
      size_t guard = 0u;
      while (guard < statement->child_count) {
        if (cursor == W_SEED_FRONTEND_NONE ||
            (size_t)cursor < first || (size_t)cursor >= first + count)
          return false;
        cursor = output->statements[cursor].next_sibling;
        guard += 1u;
      }
      if (cursor != W_SEED_FRONTEND_NONE) return false;
    }
    if (!structured &&
        (statement->first_child != W_SEED_FRONTEND_NONE ||
         statement->child_count != 0u ||
         statement->else_child != W_SEED_FRONTEND_NONE ||
         statement->range_lower_expression != W_SEED_FRONTEND_NONE ||
         statement->range_upper_expression != W_SEED_FRONTEND_NONE ||
         statement->loop_local_ordinal != W_SEED_FRONTEND_NONE))
      return false;
  }
  for (size_t target_offset = 0u; target_offset < count; target_offset += 1u) {
    const uint32_t target = (uint32_t)(first + target_offset);
    size_t incoming = target_offset == 0u ? 1u : 0u;
    for (size_t source_offset = 0u; source_offset < count;
         source_offset += 1u) {
      const w_seed_frontend_statement *source =
          &output->statements[first + source_offset];
      if (source->next_sibling == target) incoming += 1u;
      if (source->first_child == target) incoming += 1u;
      if (source->else_child == target) incoming += 1u;
    }
    if (incoming != 1u) return false;
  }
  if (!count_u32(local_if_count)) return false;
  *if_total += local_if_count;
  return true;
}

typedef struct {
  uint32_t then_statement;
  uint32_t else_statement;
  uint32_t declaration_statement;
  uint32_t then_rhs;
  uint32_t else_rhs;
} hir0_branch_assignment;

typedef struct {
  uint32_t count;
  hir0_branch_assignment entries[HIR0_MAX_BRANCH_ASSIGNMENTS];
} hir0_branch_assignment_merge;

static bool frontend_function_has_terminal_if(const w_seed_hir0_input *input,
                                              size_t function_index);

static bool frontend_function_root_contains(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t function,
    uint32_t target_statement) {
  if (output == NULL || result == NULL || function >= result->written.functions)
    return false;
  const w_seed_frontend_function *owner = &output->functions[function];
  uint32_t cursor = owner->statement_count == 0u
                        ? W_SEED_FRONTEND_NONE
                        : owner->first_statement;
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < result->written.statements) {
    if (cursor == target_statement) return true;
    cursor = output->statements[cursor].next_sibling;
    guard += 1u;
  }
  return false;
}

static size_t frontend_function_root_ordinal(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t function,
    uint32_t target_statement) {
  if (output == NULL || result == NULL || function >= result->written.functions)
    return SIZE_MAX;
  const w_seed_frontend_function *owner = &output->functions[function];
  uint32_t cursor = owner->statement_count == 0u
                        ? W_SEED_FRONTEND_NONE
                        : owner->first_statement;
  size_t ordinal = 0u;
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < result->written.statements) {
    if (cursor == target_statement) return ordinal;
    cursor = output->statements[cursor].next_sibling;
    ordinal += 1u;
    guard += 1u;
  }
  return SIZE_MAX;
}

static bool frontend_i64_merge_value_shape(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t function,
    uint32_t if_statement, uint32_t expression, const uint32_t *assigned,
    size_t assigned_count, uint32_t allowed_binding, size_t depth) {
  if (output == NULL || result == NULL || expression == W_SEED_FRONTEND_NONE ||
      (size_t)expression >= result->written.expressions ||
      depth > W_SEED_HIR0_MAX_NESTING)
    return false;
  const w_seed_frontend_expression *value = &output->expressions[expression];
  if (value->inferred_type == W_SEED_FRONTEND_NONE ||
      (size_t)value->inferred_type >= result->written.types ||
      output->types[value->inferred_type].kind != W_SEED_FRONTEND_TYPE_INTEGER ||
      !output->types[value->inferred_type].is_signed ||
      output->types[value->inferred_type].bit_width != 64u)
    return false;
  if (value->kind == W_SEED_FRONTEND_EXPR_INTEGER) return true;
  if (value->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER) {
    if (value->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE) {
      const w_seed_frontend_function *owner = &output->functions[function];
      return value->resolved_binding_statement == W_SEED_FRONTEND_NONE &&
             value->resolved_parameter_ordinal < owner->parameter_count;
    }
    if (value->resolved_binding_statement == W_SEED_FRONTEND_NONE ||
        (size_t)value->resolved_binding_statement >= if_statement ||
        (size_t)value->resolved_binding_statement >=
            result->written.statements ||
        !frontend_function_root_contains(
            output, result, function, value->resolved_binding_statement))
      return false;
    const w_seed_frontend_statement *binding =
        &output->statements[value->resolved_binding_statement];
    if ((binding->kind != W_SEED_FRONTEND_STMT_LET &&
         binding->kind != W_SEED_FRONTEND_STMT_VAR) ||
        binding->owner_function != function ||
        binding->effective_type != value->inferred_type ||
        !text_equal(binding->binding_name, value->spelling))
      return false;
    for (size_t index = 0u; index < assigned_count; index += 1u)
      if (assigned != NULL && assigned[index] ==
                                  value->resolved_binding_statement &&
          value->resolved_binding_statement != allowed_binding)
        return false;
    return true;
  }
  if (value->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS)
    return frontend_i64_merge_value_shape(output, result, function,
                                          if_statement, value->left, assigned,
                                          assigned_count, allowed_binding,
                                          depth + 1u);
  if (value->kind == W_SEED_FRONTEND_EXPR_UNARY)
    return text_is(value->operator_text, "-") &&
           frontend_i64_merge_value_shape(output, result, function,
                                          if_statement, value->left, assigned,
                                          assigned_count, allowed_binding,
                                          depth + 1u);
  if (value->kind != W_SEED_FRONTEND_EXPR_BINARY ||
      (!text_is(value->operator_text, "+") &&
       !text_is(value->operator_text, "-") &&
       !text_is(value->operator_text, "*") &&
       !text_is(value->operator_text, "/") &&
       !text_is(value->operator_text, "%")))
    return false;
  return frontend_i64_merge_value_shape(output, result, function, if_statement,
                                        value->left, assigned, assigned_count,
                                        allowed_binding, depth + 1u) &&
         frontend_i64_merge_value_shape(output, result, function, if_statement,
                                        value->right, assigned, assigned_count,
                                        allowed_binding, depth + 1u);
}

static bool frontend_branch_assignment_record(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t function,
    uint32_t if_statement, uint32_t statement_index,
    const uint32_t *assigned, size_t assigned_count,
    hir0_branch_assignment *record) {
  if (output == NULL || result == NULL || record == NULL ||
      (size_t)statement_index >= result->written.statements)
    return false;
  const w_seed_frontend_statement *statement =
      &output->statements[statement_index];
  if (statement->kind != W_SEED_FRONTEND_STMT_EXPRESSION ||
      statement->owner_function != function ||
      statement->first_child != W_SEED_FRONTEND_NONE ||
      statement->child_count != 0u ||
      statement->else_child != W_SEED_FRONTEND_NONE ||
      statement->expression_index == W_SEED_FRONTEND_NONE ||
      (size_t)statement->expression_index >= result->written.expressions)
    return false;
  const w_seed_frontend_expression *assignment =
      &output->expressions[statement->expression_index];
  if (assignment->kind != W_SEED_FRONTEND_EXPR_ASSIGNMENT ||
      !text_is(assignment->operator_text, "=") ||
      assignment->left == W_SEED_FRONTEND_NONE ||
      assignment->right == W_SEED_FRONTEND_NONE ||
      (size_t)assignment->left >= result->written.expressions ||
      (size_t)assignment->right >= result->written.expressions)
    return false;
  const w_seed_frontend_expression *target =
      &output->expressions[assignment->left];
  if (target->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
      target->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE ||
      target->resolved_binding_statement == W_SEED_FRONTEND_NONE ||
      (size_t)target->resolved_binding_statement >= if_statement ||
      (size_t)target->resolved_binding_statement >=
          result->written.statements ||
      !frontend_function_root_contains(
          output, result, function, target->resolved_binding_statement))
    return false;
  const w_seed_frontend_statement *declaration =
      &output->statements[target->resolved_binding_statement];
  if (declaration->kind != W_SEED_FRONTEND_STMT_VAR ||
      declaration->owner_function != function ||
      declaration->effective_type == W_SEED_FRONTEND_NONE ||
      (size_t)declaration->effective_type >= result->written.types ||
      output->types[declaration->effective_type].kind !=
          W_SEED_FRONTEND_TYPE_INTEGER ||
      !output->types[declaration->effective_type].is_signed ||
      output->types[declaration->effective_type].bit_width != 64u ||
      target->inferred_type != declaration->effective_type ||
      !text_equal(target->spelling, declaration->binding_name) ||
      !frontend_i64_merge_value_shape(output, result, function, if_statement,
                                      assignment->right, assigned,
                                      assigned_count,
                                      target->resolved_binding_statement, 0u))
    return false;
  const w_seed_frontend_expression *rhs =
      &output->expressions[assignment->right];
  if (rhs->inferred_type != declaration->effective_type)
    return false;
  *record = (hir0_branch_assignment){
      .then_statement = statement_index,
      .else_statement = W_SEED_FRONTEND_NONE,
      .declaration_statement = target->resolved_binding_statement,
      .then_rhs = assignment->right,
      .else_rhs = W_SEED_FRONTEND_NONE};
  return true;
}

static bool frontend_branch_assignment_merge(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t function,
    uint32_t if_statement, hir0_branch_assignment_merge *merge) {
  if (output == NULL || result == NULL || merge == NULL ||
      (size_t)if_statement >= result->written.statements)
    return false;
  const w_seed_frontend_statement *branch = &output->statements[if_statement];
  if (branch->kind != W_SEED_FRONTEND_STMT_IF ||
      branch->owner_function != function ||
      !frontend_function_root_contains(output, result, function, if_statement) ||
      branch->first_child == W_SEED_FRONTEND_NONE ||
      branch->else_child == W_SEED_FRONTEND_NONE ||
      (size_t)branch->first_child >= result->written.statements ||
      (size_t)branch->else_child >= result->written.statements)
    return false;
  (void)memset(merge, 0, sizeof(*merge));
  uint32_t cursor = branch->first_child;
  uint32_t then_assigned[HIR0_MAX_BRANCH_ASSIGNMENTS] = {0u};
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < result->written.statements) {
    if (merge->count >= HIR0_MAX_BRANCH_ASSIGNMENTS)
      return false;
    hir0_branch_assignment record;
    if (!frontend_branch_assignment_record(output, result, function,
                                            if_statement, cursor,
                                            then_assigned, merge->count,
                                            &record))
      return false;
    for (size_t index = 0u; index < merge->count; index += 1u)
      if (merge->entries[index].declaration_statement ==
          record.declaration_statement)
        return false;
    merge->entries[merge->count] = record;
    then_assigned[merge->count] = record.declaration_statement;
    merge->count += 1u;
    cursor = output->statements[cursor].next_sibling;
    guard += 1u;
  }
  if (cursor != W_SEED_FRONTEND_NONE || merge->count == 0u) return false;
  const uint32_t then_count = merge->count;
  cursor = branch->else_child;
  uint32_t else_assigned[HIR0_MAX_BRANCH_ASSIGNMENTS] = {0u};
  guard = 0u;
  uint32_t else_count = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < result->written.statements) {
    if (else_count >= HIR0_MAX_BRANCH_ASSIGNMENTS) return false;
    hir0_branch_assignment record;
    if (!frontend_branch_assignment_record(output, result, function,
                                            if_statement, cursor,
                                            else_assigned, else_count,
                                            &record))
      return false;
    size_t match = SIZE_MAX;
    for (size_t index = 0u; index < then_count; index += 1u) {
      if (merge->entries[index].declaration_statement ==
          record.declaration_statement) {
        if (match != SIZE_MAX) return false;
        match = index;
      }
    }
    if (match == SIZE_MAX) return false;
    merge->entries[match].else_statement = record.then_statement;
    merge->entries[match].else_rhs = record.then_rhs;
    else_assigned[else_count] = record.declaration_statement;
    else_count += 1u;
    cursor = output->statements[cursor].next_sibling;
    guard += 1u;
  }
  if (cursor != W_SEED_FRONTEND_NONE || else_count != then_count)
    return false;
  for (size_t index = 0u; index < then_count; index += 1u)
    if (merge->entries[index].else_statement == W_SEED_FRONTEND_NONE)
      return false;
  /* The first pass rejects dependencies on an already-written root.  Repeat
   * the pure-RHS proof with the complete target set so the opposite source
   * order is rejected as well; only the assignment's own pre-branch root may
   * be read. */
  for (size_t index = 0u; index < then_count; index += 1u) {
    uint32_t targets[HIR0_MAX_BRANCH_ASSIGNMENTS] = {0u};
    for (size_t target = 0u; target < then_count; target += 1u)
      targets[target] = merge->entries[target].declaration_statement;
    if (!frontend_i64_merge_value_shape(
            output, result, function, if_statement,
            merge->entries[index].then_rhs, targets, then_count,
            merge->entries[index].declaration_statement, 0u) ||
        !frontend_i64_merge_value_shape(
            output, result, function, if_statement,
            merge->entries[index].else_rhs, targets, then_count,
            merge->entries[index].declaration_statement, 0u))
      return false;
  }
  /* Stable insertion sort by direct root-chain order, independent of either
   * arm's assignment order. */
  for (size_t index = 1u; index < then_count; index += 1u) {
    const hir0_branch_assignment key = merge->entries[index];
    const size_t key_order = frontend_function_root_ordinal(
        output, result, function, key.declaration_statement);
    if (key_order == SIZE_MAX) return false;
    size_t cursor_index = index;
    while (cursor_index != 0u) {
      const size_t previous_order = frontend_function_root_ordinal(
          output, result, function,
          merge->entries[cursor_index - 1u].declaration_statement);
      if (previous_order <= key_order) break;
      merge->entries[cursor_index] = merge->entries[cursor_index - 1u];
      cursor_index -= 1u;
    }
    merge->entries[cursor_index] = key;
  }
  return true;
}

static bool frontend_assignment_merge_owner(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t function,
    uint32_t assignment_statement, uint32_t *owner) {
  if (output == NULL || result == NULL || owner == NULL) return false;
  for (size_t index = 0u; index < result->written.statements; index += 1u) {
    hir0_branch_assignment_merge merge;
    if (frontend_branch_assignment_merge(output, result, function,
                                         (uint32_t)index, &merge))
      for (size_t entry = 0u; entry < merge.count; entry += 1u)
        if (merge.entries[entry].then_statement == assignment_statement ||
            merge.entries[entry].else_statement == assignment_statement) {
          *owner = (uint32_t)index;
          return true;
        }
  }
  return false;
}

typedef struct {
  const w_seed_hir0_input *input;
  const w_seed_frontend_output *output;
  const w_seed_frontend_result *result;
  size_t function_index;
  size_t module_index;
  size_t document_index;
  size_t *expression_cursor;
  size_t *interpolation_segment_cursor;
  size_t *const_byte_cursor;
  size_t *bindings;
  size_t *calls;
  size_t *yields;
  size_t *arguments;
  size_t *values;
  size_t *segments;
  size_t *value_bytes;
  size_t *if_total;
  size_t *logical_total;
  size_t *merge_total;
  size_t *while_total;
  size_t *loop_carrier_total;
  size_t *switch_total;
  size_t *switch_edge_total;
  size_t *switch_capture_total;
  bool has_value_return;
  bool loop_seen;
  bool loop_continuation_seen;
  uint32_t loop_root_statements[HIR0_MAX_BRANCH_ASSIGNMENTS];
  size_t loop_root_count;
  bool allow_branch_return;
} hir0_statement_walk;

static bool hir0_walk_statement_chain(hir0_statement_walk *walk,
                                      uint32_t first_statement, bool branch,
                                      size_t depth);

static bool frontend_assignment_expression_ok(
    hir0_statement_walk *walk, size_t statement_index,
    uint32_t root_index) {
  if (walk == NULL || root_index == W_SEED_FRONTEND_NONE ||
      (size_t)root_index >= walk->result->written.expressions)
    return false;
  const w_seed_frontend_expression *root =
      &walk->output->expressions[root_index];
  if (!frontend_value_common_ok(walk->input, root, walk->module_index,
                                walk->function_index, walk->document_index) ||
      root->kind != W_SEED_FRONTEND_EXPR_ASSIGNMENT ||
      !text_is(root->operator_text, "=") ||
      root->left == W_SEED_FRONTEND_NONE ||
      root->right == W_SEED_FRONTEND_NONE ||
      (root->inferred_type != W_SEED_FRONTEND_NONE &&
       ((size_t)root->inferred_type >= walk->result->written.types ||
        walk->output->types[root->inferred_type].kind !=
            W_SEED_FRONTEND_TYPE_UNIT)) ||
      !frontend_value_has_no_resolution(root) ||
      root->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
      root->const_byte_offset != W_SEED_FRONTEND_NONE ||
      root->const_byte_count != 0u || root->has_bool_value ||
      root->has_integer_value ||
      root->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
      root->interpolation_segment_count != 0u ||
      (size_t)root->left != *walk->expression_cursor)
    return false;
  const w_seed_frontend_expression *target =
      &walk->output->expressions[root->left];
  if (!frontend_value_common_ok(walk->input, target, walk->module_index,
                                walk->function_index, walk->document_index) ||
      target->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
      target->left != W_SEED_FRONTEND_NONE ||
      target->right != W_SEED_FRONTEND_NONE ||
      target->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE ||
      target->resolved_binding_statement == W_SEED_FRONTEND_NONE ||
      (size_t)target->resolved_binding_statement >= statement_index ||
      (size_t)target->resolved_binding_statement >=
          walk->result->written.statements ||
      target->resolved_function_index != W_SEED_FRONTEND_NONE ||
      target->resolved_callee_kind != W_SEED_FRONTEND_CALLEE_NONE ||
      target->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
      target->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
      target->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
      target->resolved_local_ordinal != W_SEED_FRONTEND_NONE ||
      target->resolved_const_declaration != W_SEED_FRONTEND_NONE ||
      target->member_name.length != 0u || !text_valid(target->member_name) ||
      target->const_byte_offset != W_SEED_FRONTEND_NONE ||
      target->const_byte_count != 0u || target->has_bool_value ||
      target->has_integer_value ||
      target->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
      target->interpolation_segment_count != 0u)
    return false;
  const w_seed_frontend_statement *declaration =
      &walk->output->statements[target->resolved_binding_statement];
  if (declaration->kind != W_SEED_FRONTEND_STMT_VAR ||
      declaration->owner_function != walk->function_index ||
      declaration->module_index != walk->module_index ||
      declaration->effective_type == W_SEED_FRONTEND_NONE ||
      declaration->effective_type != target->inferred_type ||
      !text_equal(declaration->binding_name, target->spelling))
    return false;
  *walk->expression_cursor += 1u;
  if (!frontend_value_tree_ok(
          walk->input, walk->module_index, walk->function_index,
          walk->document_index, statement_index, root->right, 0u,
          walk->expression_cursor, walk->interpolation_segment_cursor,
          walk->const_byte_cursor, walk->values, walk->segments,
          walk->value_bytes, walk->calls, walk->arguments,
          walk->logical_total))
    return false;
  const w_seed_frontend_expression *replacement =
      &walk->output->expressions[root->right];
  if (replacement->inferred_type != declaration->effective_type ||
      (size_t)root_index != *walk->expression_cursor)
    return false;
  *walk->expression_cursor += 1u;
  return add_size(*walk->bindings, 1u, walk->bindings);
}

/* The bounded switch slice is deliberately a statement-level return form.
 * The frontend already retains the normalized arm records; this second walk
 * proves the exact closed enum/case partition while consuming the same dense
 * postorder expression stream as the ordinary value walker. */
static bool frontend_switch_flat_value_ok(
    const w_seed_hir0_input *input, size_t expression, size_t depth) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || depth > W_SEED_HIR0_MAX_NESTING ||
      expression == W_SEED_FRONTEND_NONE ||
      expression >= input->frontend_result->written.expressions)
    return false;
  const w_seed_frontend_expression *value =
      &input->frontend_output->expressions[expression];
  if (value->kind == W_SEED_FRONTEND_EXPR_IF ||
      (value->kind == W_SEED_FRONTEND_EXPR_BINARY &&
       hir_logical_operator(value->operator_text) !=
           W_SEED_HIR0_LOGICAL_NONE) ||
      value->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING)
    return false;
  if (value->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS ||
      value->kind == W_SEED_FRONTEND_EXPR_UNARY)
    return frontend_switch_flat_value_ok(input, value->left, depth + 1u);
  if (value->kind == W_SEED_FRONTEND_EXPR_BINARY)
    return frontend_switch_flat_value_ok(input, value->left, depth + 1u) &&
           frontend_switch_flat_value_ok(input, value->right, depth + 1u);
  if (value->kind == W_SEED_FRONTEND_EXPR_CALL) {
    if (!range_valid(value->first_argument, value->argument_count,
                     input->frontend_result->written.arguments))
      return false;
    for (size_t ordinal = 0u; ordinal < value->argument_count; ordinal += 1u)
      if (!frontend_switch_flat_value_ok(
              input,
              input->frontend_output
                  ->arguments[(size_t)value->first_argument + ordinal]
                  .expression_index,
              depth + 1u))
        return false;
    return true;
  }
  return value->kind == W_SEED_FRONTEND_EXPR_INTEGER ||
         value->kind == W_SEED_FRONTEND_EXPR_BOOL ||
         value->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER;
}

static bool frontend_switch_return_ok(hir0_statement_walk *walk,
                                      uint32_t statement_index,
                                      uint32_t root_index) {
  if (walk == NULL || root_index == W_SEED_FRONTEND_NONE ||
      walk->switch_total == NULL || walk->switch_edge_total == NULL ||
      walk->switch_capture_total == NULL ||
      (size_t)root_index >= walk->result->written.expressions)
    return false;
  const w_seed_frontend_expression *root =
      &walk->output->expressions[root_index];
  if (root->kind != W_SEED_FRONTEND_EXPR_SWITCH || !root->supported ||
      root->left == W_SEED_FRONTEND_NONE ||
      root->right != W_SEED_FRONTEND_NONE ||
      root->first_argument != W_SEED_FRONTEND_NONE ||
      root->argument_count != 0u || root->enum_index == W_SEED_FRONTEND_NONE ||
      root->enum_case_index != W_SEED_FRONTEND_NONE ||
      root->first_switch_arm == W_SEED_FRONTEND_NONE ||
      root->switch_arm_count == 0u ||
      (size_t)root->first_switch_arm + root->switch_arm_count >
          walk->result->written.switch_arms ||
      root->inferred_type == W_SEED_FRONTEND_NONE ||
      (size_t)root->inferred_type >= walk->result->written.types ||
      !frontend_type_is_scalar(
          &walk->output->types[root->inferred_type]) ||
      (walk->output->functions[walk->function_index].return_type ==
           W_SEED_FRONTEND_NONE ||
       (size_t)walk->output->functions[walk->function_index].return_type >=
           walk->result->written.types) ||
      !frontend_supported_types_equal_for_input(
          walk->input,
          &walk->output->types[root->inferred_type],
          &walk->output->types[walk->output->functions[walk->function_index]
                                    .return_type]) ||
      !frontend_value_common_ok(walk->input, root, walk->module_index,
                                walk->function_index, walk->document_index))
    return false;

  if ((size_t)root->left >= walk->result->written.expressions)
    return false;
  const w_seed_frontend_expression *subject =
      &walk->output->expressions[root->left];
  if (subject->inferred_type == W_SEED_FRONTEND_NONE ||
      (size_t)subject->inferred_type >= walk->result->written.types)
    return false;
  const w_seed_frontend_type *subject_type =
      &walk->output->types[subject->inferred_type];
  const bool subject_is_enum = subject_type->kind == W_SEED_FRONTEND_TYPE_ENUM;
  const bool subject_is_subset =
      subject_type->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  if ((!subject_is_enum && !subject_is_subset) ||
      subject_type->enum_base_index != root->enum_index ||
      (subject_is_enum
           ? !frontend_local_enum_type_supported(walk->input, subject_type)
           : !frontend_local_enum_subset_type_supported(walk->input,
                                                        subject_type)) ||
      (size_t)root->enum_index >= walk->result->written.enums)
    return false;
  const w_seed_frontend_enum *decl = &walk->output->enums[root->enum_index];
  const size_t expected_arm_count =
      subject_is_subset ? subject_type->subset_member_count : decl->case_count;
  if (decl->case_count == 0u || expected_arm_count == 0u ||
      root->switch_arm_count != expected_arm_count ||
      (size_t)root->left != *walk->expression_cursor)
    return false;
  if (!frontend_value_tree_ok(
          walk->input, walk->module_index, walk->function_index,
          walk->document_index, statement_index, root->left, 0u,
          walk->expression_cursor, walk->interpolation_segment_cursor,
          walk->const_byte_cursor, walk->values, walk->segments,
          walk->value_bytes, walk->calls, walk->arguments,
          walk->logical_total) ||
      (size_t)root_index != *walk->expression_cursor)
    return false;
  /* The switch root is a control-flow record, not a HIR value. */
  *walk->expression_cursor += 1u;

  for (size_t ordinal = 0u; ordinal < root->switch_arm_count; ordinal += 1u) {
    const w_seed_frontend_switch_arm *arm =
        &walk->output->switch_arms[(size_t)root->first_switch_arm + ordinal];
    if (arm->module_index != walk->module_index ||
        arm->owner_expression != root_index ||
        arm->pattern_kind != W_SEED_FRONTEND_SWITCH_PATTERN_ENUM_CASE ||
        !arm->supported || arm->enum_index != root->enum_index ||
        arm->enum_case_index == W_SEED_FRONTEND_NONE ||
        (size_t)arm->enum_case_index < decl->first_case ||
        (size_t)arm->enum_case_index >=
            (size_t)decl->first_case + decl->case_count ||
        (subject_is_subset &&
         !frontend_local_enum_subset_contains_case(
             walk->input, subject_type, subject->inferred_type,
             arm->enum_case_index)) ||
        arm->result_expression == W_SEED_FRONTEND_NONE ||
        !frontend_span_ok(&walk->input->frontend_input
                               ->documents[walk->document_index],
                          arm->span) ||
        !frontend_span_ok(&walk->input->frontend_input
                               ->documents[walk->document_index],
                          arm->pattern_span))
      return false;
    const w_seed_frontend_enum_case *case_value =
        &walk->output->enum_cases[arm->enum_case_index];
    if (case_value->owner_enum != root->enum_index ||
        arm->first_capture != *walk->switch_capture_total ||
        arm->capture_count > case_value->payload_count ||
        !range_valid(arm->first_capture, arm->capture_count,
                     walk->result->written.pattern_captures))
      return false;
    for (size_t capture_ordinal = 0u;
         capture_ordinal < arm->capture_count; capture_ordinal += 1u) {
      const size_t capture_index =
          (size_t)arm->first_capture + capture_ordinal;
      const w_seed_frontend_pattern_capture *capture =
          &walk->output->pattern_captures[capture_index];
      if (capture->module_index != walk->module_index ||
          capture->owner_switch_arm !=
              (size_t)root->first_switch_arm + ordinal ||
          capture->ordinal != capture_ordinal ||
          capture->parameter_ordinal >= case_value->payload_count ||
          capture->type_index == W_SEED_FRONTEND_NONE ||
          capture->type_index >= walk->result->written.types ||
          !frontend_type_is_scalar(
              &walk->output->types[capture->type_index]) ||
          !text_valid(capture->name) || capture->name.length == 0u ||
          !frontend_span_ok(&walk->input->frontend_input
                                 ->documents[walk->document_index],
                            capture->span))
        return false;
      for (size_t prior = 0u; prior < capture_ordinal; prior += 1u)
        if (walk->output
                ->pattern_captures[(size_t)arm->first_capture + prior]
                .parameter_ordinal == capture->parameter_ordinal ||
            text_equal(walk->output->pattern_captures[
                           (size_t)arm->first_capture + prior].name,
                       capture->name))
          return false;
      const w_seed_frontend_enum_case_parameter *parameter =
          &walk->output->enum_case_parameters[
              (size_t)case_value->first_payload +
              capture->parameter_ordinal];
      if (parameter->owner_case != arm->enum_case_index ||
          !frontend_supported_types_equal_for_input(
              walk->input,
              &walk->output->types[parameter->type_index],
              &walk->output->types[capture->type_index]))
        return false;
    }
    if (!add_size(*walk->switch_capture_total, arm->capture_count,
                  walk->switch_capture_total))
      return false;
    for (size_t prior = 0u; prior < ordinal; prior += 1u) {
      const w_seed_frontend_switch_arm *previous =
          &walk->output->switch_arms[(size_t)root->first_switch_arm + prior];
      if (previous->enum_case_index == arm->enum_case_index) return false;
    }
    if ((size_t)arm->result_expression >= walk->result->written.expressions ||
        walk->output->expressions[arm->result_expression].inferred_type ==
            W_SEED_FRONTEND_NONE ||
        (size_t)walk->output->expressions[arm->result_expression]
                .inferred_type >= walk->result->written.types ||
        walk->output->expressions[arm->result_expression].inferred_type !=
            root->inferred_type ||
        !frontend_type_is_scalar(
            &walk->output->types[walk->output->expressions[
                arm->result_expression].inferred_type]) ||
        !frontend_switch_flat_value_ok(walk->input, arm->result_expression,
                                       0u) ||
        !frontend_value_tree_ok(
            walk->input, walk->module_index, walk->function_index,
            walk->document_index, statement_index, arm->result_expression,
            0u, walk->expression_cursor, walk->interpolation_segment_cursor,
            walk->const_byte_cursor, walk->values, walk->segments,
            walk->value_bytes, walk->calls, walk->arguments,
            walk->logical_total))
      return false;
  }
  return add_size(*walk->switch_total, 1u, walk->switch_total) &&
         add_size(*walk->switch_edge_total, root->switch_arm_count,
                  walk->switch_edge_total);
}

static bool binding_index_for_statement(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t function,
    size_t use_statement, uint32_t target_statement, uint32_t *out);

static bool hir0_walk_statement(hir0_statement_walk *walk, uint32_t index,
                                bool branch, size_t depth) {
  if (walk == NULL || walk->input == NULL || walk->output == NULL ||
      walk->result == NULL || index == W_SEED_FRONTEND_NONE ||
      (size_t)index >= walk->result->written.statements)
    return false;
  const w_seed_frontend_statement *statement =
      &walk->output->statements[index];
  if (statement->module_index != walk->module_index ||
      statement->owner_function != walk->function_index ||
      !frontend_span_ok(&walk->input->frontend_input
                             ->documents[walk->document_index],
                        statement->span))
    return false;
  if (branch && statement->kind != W_SEED_FRONTEND_STMT_LET &&
      statement->kind != W_SEED_FRONTEND_STMT_EXPRESSION &&
      statement->kind != W_SEED_FRONTEND_STMT_IF &&
      !(statement->kind == W_SEED_FRONTEND_STMT_RETURN &&
        walk->allow_branch_return))
    return false;
  if (statement->kind == W_SEED_FRONTEND_STMT_LET ||
      statement->kind == W_SEED_FRONTEND_STMT_VAR) {
    if (walk->loop_seen || !text_valid(statement->binding_name) ||
        statement->binding_name.length == 0u ||
        statement->effective_type == W_SEED_FRONTEND_NONE ||
        (size_t)statement->effective_type >= walk->result->written.types ||
        !frontend_hir_type_supported(walk->input,
            &walk->output->types[statement->effective_type]) ||
        (statement->declared_type != W_SEED_FRONTEND_NONE &&
         (size_t)statement->declared_type >= walk->result->written.types) ||
        (statement->declared_type != W_SEED_FRONTEND_NONE &&
         !frontend_type_assignable_for_input(
             walk->input,
             &walk->output->types[statement->effective_type],
             &walk->output->types[statement->declared_type])) ||
        statement->expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)statement->expression_index >=
            walk->result->written.expressions)
      return false;
    const w_seed_frontend_expression *initializer =
        &walk->output->expressions[statement->expression_index];
    if (initializer->inferred_type != statement->effective_type)
      return false;
    if (initializer->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH ||
        initializer->kind == W_SEED_FRONTEND_EXPR_AWAIT) {
      if (branch || statement->kind != W_SEED_FRONTEND_STMT_LET ||
          statement->declared_type != W_SEED_FRONTEND_NONE)
        return false;
      bool valid =
          initializer->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH
              ? frontend_async_launch_expression_ok(
                    walk->input, walk->module_index, walk->function_index,
                    walk->document_index, index, statement->expression_index,
                    walk->expression_cursor,
                    walk->interpolation_segment_cursor,
                    walk->const_byte_cursor, walk->calls, walk->arguments,
                    walk->values, walk->segments, walk->value_bytes,
                    walk->logical_total)
              : frontend_await_expression_ok(
                    walk->input, walk->module_index, walk->function_index,
                    walk->document_index, index, statement->expression_index,
                    walk->expression_cursor,
                    walk->interpolation_segment_cursor,
                    walk->const_byte_cursor, walk->calls, walk->arguments,
                    walk->values, walk->segments, walk->value_bytes,
                    walk->logical_total);
      uint32_t launch_binding = 0u;
      if (valid && initializer->kind == W_SEED_FRONTEND_EXPR_AWAIT)
        valid = binding_index_for_statement(
            walk->output, walk->result, walk->function_index, index,
            initializer->task_binding_statement, &launch_binding);
      (void)launch_binding;
      return valid && add_size(*walk->bindings, 1u, walk->bindings);
    }
    if (initializer->kind == W_SEED_FRONTEND_EXPR_CALL) {
      if (!frontend_call_expression_ok(
              walk->input, walk->module_index, walk->function_index,
              walk->document_index, index, statement->expression_index, true,
              walk->expression_cursor, walk->interpolation_segment_cursor,
              walk->const_byte_cursor, walk->calls, walk->arguments,
              walk->values, walk->segments, walk->value_bytes,
              walk->logical_total))
        return false;
    } else if (!frontend_value_tree_ok(
                   walk->input, walk->module_index, walk->function_index,
                   walk->document_index, index, statement->expression_index, 0u,
                   walk->expression_cursor,
                   walk->interpolation_segment_cursor,
                   walk->const_byte_cursor, walk->values, walk->segments,
                   walk->value_bytes, walk->calls, walk->arguments,
                   walk->logical_total)) {
      return false;
    }
    return add_size(*walk->bindings, 1u, walk->bindings);
  }
  if (statement->kind == W_SEED_FRONTEND_STMT_EXPRESSION) {
    if (statement->binding_name.length != 0u ||
        statement->declared_type != W_SEED_FRONTEND_NONE ||
        statement->effective_type != W_SEED_FRONTEND_NONE ||
        statement->expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)statement->expression_index >=
            walk->result->written.expressions)
      return false;
    const w_seed_frontend_expression *expression =
        &walk->output->expressions[statement->expression_index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_EXECUTION_YIELD) {
      const w_seed_frontend_function *function =
          &walk->output->functions[walk->function_index];
      if (branch || walk->loop_seen || !function->is_async ||
          function->is_const || !expression->supported ||
          statement->expression_index != *walk->expression_cursor ||
          expression->inferred_type == W_SEED_FRONTEND_NONE ||
          (size_t)expression->inferred_type >= walk->result->written.types ||
          walk->output->types[expression->inferred_type].kind !=
              W_SEED_FRONTEND_TYPE_UNIT ||
          expression->left != W_SEED_FRONTEND_NONE ||
          expression->right != W_SEED_FRONTEND_NONE ||
          expression->first_argument != W_SEED_FRONTEND_NONE ||
          expression->argument_count != 0u ||
          expression->const_byte_offset != W_SEED_FRONTEND_NONE ||
          expression->const_byte_count != 0u || expression->has_bool_value ||
          expression->has_integer_value ||
          expression->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
          expression->interpolation_segment_count != 0u ||
          !frontend_value_has_no_resolution(expression) ||
          !text_is(expression->operator_text, "yield") ||
          !frontend_value_common_ok(walk->input, expression,
                                    walk->module_index,
                                    walk->function_index,
                                    walk->document_index) ||
          !add_size(*walk->expression_cursor, 1u,
                    walk->expression_cursor) ||
          !add_size(*walk->yields, 1u, walk->yields))
        return false;
      return true;
    }
    if (expression->kind == W_SEED_FRONTEND_EXPR_ASSIGNMENT) {
      if (branch ||
          (walk->loop_seen &&
           (walk->loop_continuation_seen || walk->loop_root_count == 0u)))
        return false;
      if (walk->loop_seen) {
        const uint32_t left = expression->left;
        if (left == W_SEED_FRONTEND_NONE ||
            (size_t)left >= walk->result->written.expressions)
          return false;
        const w_seed_frontend_expression *target =
            &walk->output->expressions[left];
        bool carried_root = false;
        for (size_t ordinal = 0u; ordinal < walk->loop_root_count; ordinal += 1u)
          if (target->resolved_binding_statement ==
              walk->loop_root_statements[ordinal]) {
            carried_root = true;
            break;
          }
        if (!carried_root || target->inferred_type == W_SEED_FRONTEND_NONE ||
            (size_t)target->inferred_type >= walk->result->written.types ||
            walk->output->types[target->inferred_type].kind !=
                W_SEED_FRONTEND_TYPE_INTEGER ||
            !walk->output->types[target->inferred_type].is_signed ||
            walk->output->types[target->inferred_type].bit_width != 64u)
          return false;
        bool uses_root = false;
        if (!frontend_loop_scalar_tree_ok(
                walk->input, walk->module_index, walk->function_index,
                walk->document_index, expression->right,
                walk->loop_root_statements, walk->loop_root_count, &uses_root,
                0u) ||
            !uses_root ||
            !frontend_assignment_expression_ok(
                walk, index, statement->expression_index))
          return false;
        walk->loop_continuation_seen = true;
        return true;
      }
      return frontend_assignment_expression_ok(
          walk, index, statement->expression_index);
    }
    if (walk->loop_seen) return false;
    return frontend_call_expression_ok(
        walk->input, walk->module_index, walk->function_index,
        walk->document_index, index, statement->expression_index, false,
        walk->expression_cursor, walk->interpolation_segment_cursor,
        walk->const_byte_cursor, walk->calls, walk->arguments, walk->values,
        walk->segments, walk->value_bytes, walk->logical_total);
  }
  if (statement->kind == W_SEED_FRONTEND_STMT_WHILE ||
      statement->kind == W_SEED_FRONTEND_STMT_REPEAT) {
    if (branch || walk->loop_seen || *walk->if_total != 0u ||
        statement->binding_name.length != 0u ||
        statement->declared_type != W_SEED_FRONTEND_NONE ||
        statement->effective_type != W_SEED_FRONTEND_NONE ||
        statement->condition_expression == W_SEED_FRONTEND_NONE ||
        statement->condition_expression != statement->expression_index ||
        (size_t)statement->condition_expression >=
            walk->result->written.expressions ||
        statement->first_child == W_SEED_FRONTEND_NONE ||
        (size_t)statement->first_child >= walk->result->written.statements ||
        statement->else_child != W_SEED_FRONTEND_NONE ||
        statement->range_lower_expression != W_SEED_FRONTEND_NONE ||
        statement->range_upper_expression != W_SEED_FRONTEND_NONE ||
        statement->loop_local_ordinal != W_SEED_FRONTEND_NONE)
      return false;
    uint32_t root_statements[HIR0_MAX_BRANCH_ASSIGNMENTS] = {0u};
    const w_seed_frontend_expression *assignments[
        HIR0_MAX_BRANCH_ASSIGNMENTS] = {0};
    const w_seed_frontend_statement *body_statements[
        HIR0_MAX_BRANCH_ASSIGNMENTS] = {0};
    size_t root_count = 0u;
    uint32_t body_cursor = statement->first_child;
    size_t body_guard = 0u;
    while (body_cursor != W_SEED_FRONTEND_NONE &&
           body_guard < walk->result->written.statements) {
      if (root_count >= HIR0_MAX_BRANCH_ASSIGNMENTS)
        return false;
      const w_seed_frontend_statement *body =
          &walk->output->statements[body_cursor];
      if (body->kind != W_SEED_FRONTEND_STMT_EXPRESSION ||
          body->expression_index == W_SEED_FRONTEND_NONE ||
          (size_t)body->expression_index >= walk->result->written.expressions)
        return false;
      const w_seed_frontend_expression *assignment =
          &walk->output->expressions[body->expression_index];
      if (assignment->kind != W_SEED_FRONTEND_EXPR_ASSIGNMENT ||
          assignment->left == W_SEED_FRONTEND_NONE ||
          assignment->right == W_SEED_FRONTEND_NONE ||
          (size_t)assignment->left >= walk->result->written.expressions ||
          (size_t)assignment->right >= walk->result->written.expressions)
        return false;
      const w_seed_frontend_expression *target =
          &walk->output->expressions[assignment->left];
      const uint32_t root_statement = target->resolved_binding_statement;
      if (target->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
          root_statement == W_SEED_FRONTEND_NONE ||
          (size_t)root_statement >= walk->result->written.statements ||
          !frontend_function_root_contains(walk->output, walk->result,
                                           walk->function_index,
                                           root_statement))
        return false;
      for (size_t prior = 0u; prior < root_count; prior += 1u)
        if (root_statements[prior] == root_statement) return false;
      const w_seed_frontend_statement *root =
          &walk->output->statements[root_statement];
      if (root->kind != W_SEED_FRONTEND_STMT_VAR ||
          root->effective_type == W_SEED_FRONTEND_NONE ||
          (size_t)root->effective_type >= walk->result->written.types ||
          walk->output->types[root->effective_type].kind !=
              W_SEED_FRONTEND_TYPE_INTEGER ||
          !walk->output->types[root->effective_type].is_signed ||
          walk->output->types[root->effective_type].bit_width != 64u)
        return false;
      root_statements[root_count] = root_statement;
      assignments[root_count] = assignment;
      body_statements[root_count] = body;
      root_count += 1u;
      body_cursor = body->next_sibling;
      body_guard += 1u;
    }
    if (body_cursor != W_SEED_FRONTEND_NONE || root_count == 0u ||
        root_count != statement->child_count || walk->loop_carrier_total == NULL)
      return false;
    bool condition_uses_root = false;
    const w_seed_frontend_expression *condition =
        &walk->output->expressions[statement->condition_expression];
    if (!frontend_expression_is_bool(walk->output, condition) ||
        !frontend_loop_scalar_tree_ok(
            walk->input, walk->module_index, walk->function_index,
            walk->document_index, statement->condition_expression,
            root_statements, root_count, &condition_uses_root, 0u) ||
        !condition_uses_root ||
        !frontend_value_tree_ok(
            walk->input, walk->module_index, walk->function_index,
            walk->document_index, index, statement->condition_expression, 0u,
            walk->expression_cursor, walk->interpolation_segment_cursor,
            walk->const_byte_cursor, walk->values, walk->segments,
            walk->value_bytes, walk->calls, walk->arguments,
            walk->logical_total))
      return false;
    for (size_t ordinal = 0u; ordinal < root_count; ordinal += 1u) {
      bool body_uses_root = false;
      if (!frontend_loop_scalar_tree_ok(
              walk->input, walk->module_index, walk->function_index,
              walk->document_index, assignments[ordinal]->right,
              root_statements, root_count, &body_uses_root, 0u) ||
          !body_uses_root ||
          !frontend_assignment_expression_ok(
              walk, (uint32_t)(body_statements[ordinal] - walk->output->statements),
              body_statements[ordinal]->expression_index))
        return false;
    }
    size_t loop_edge_values = 0u;
    if (!add_size(root_count, root_count, &loop_edge_values) ||
        !add_size(*walk->values, loop_edge_values, walk->values) ||
        !add_size(*walk->while_total, 1u, walk->while_total) ||
        !add_size(*walk->loop_carrier_total, root_count,
                  walk->loop_carrier_total))
      return false;
    (void)memcpy(walk->loop_root_statements, root_statements,
                 root_count * sizeof(root_statements[0]));
    walk->loop_root_count = root_count;
    walk->loop_continuation_seen = false;
    walk->loop_seen = true;
    return true;
  }
  if (statement->kind == W_SEED_FRONTEND_STMT_IF) {
    if (walk->loop_seen || depth >= W_SEED_HIR0_MAX_NESTING ||
        statement->condition_expression == W_SEED_FRONTEND_NONE ||
        statement->condition_expression != statement->expression_index ||
        (size_t)statement->condition_expression >=
            walk->result->written.expressions)
      return false;
    const w_seed_frontend_expression *condition =
        &walk->output->expressions[statement->condition_expression];
    if (condition->inferred_type == W_SEED_FRONTEND_NONE ||
        (size_t)condition->inferred_type >= walk->result->written.types ||
        walk->output->types[condition->inferred_type].kind !=
            W_SEED_FRONTEND_TYPE_BOOL ||
        !frontend_value_tree_ok(
            walk->input, walk->module_index, walk->function_index,
            walk->document_index, index, statement->condition_expression, 0u,
            walk->expression_cursor, walk->interpolation_segment_cursor,
            walk->const_byte_cursor, walk->values, walk->segments,
            walk->value_bytes, walk->calls, walk->arguments,
            walk->logical_total) ||
        !add_size(*walk->if_total, 1u, walk->if_total))
      return false;
    hir0_branch_assignment_merge merge;
    if (!branch && frontend_branch_assignment_merge(
                       walk->output, walk->result, walk->function_index, index,
                       &merge)) {
      const size_t binding_before = *walk->bindings;
      size_t assignment_count = 0u;
      uint32_t assignment_cursor = statement->first_child;
      size_t assignment_guard = 0u;
      while (assignment_cursor != W_SEED_FRONTEND_NONE &&
             assignment_guard < walk->result->written.statements) {
        if (!frontend_assignment_expression_ok(
                walk, assignment_cursor,
                walk->output->statements[assignment_cursor].expression_index))
          return false;
        assignment_cursor =
            walk->output->statements[assignment_cursor].next_sibling;
        assignment_count += 1u;
        assignment_guard += 1u;
      }
      if (assignment_cursor != W_SEED_FRONTEND_NONE ||
          assignment_count != merge.count)
        return false;
      assignment_cursor = statement->else_child;
      assignment_guard = 0u;
      while (assignment_cursor != W_SEED_FRONTEND_NONE &&
             assignment_guard < walk->result->written.statements) {
        if (!frontend_assignment_expression_ok(
                walk, assignment_cursor,
                walk->output->statements[assignment_cursor].expression_index))
          return false;
        assignment_cursor =
            walk->output->statements[assignment_cursor].next_sibling;
        assignment_guard += 1u;
      }
      if (assignment_cursor != W_SEED_FRONTEND_NONE ||
          assignment_guard != merge.count)
        return false;
      size_t expected_bindings = 0u;
      if (!add_size(merge.count, merge.count, &expected_bindings) ||
          *walk->bindings != binding_before + expected_bindings)
        return false;
      *walk->bindings = binding_before;
      return add_size(*walk->bindings, merge.count, walk->bindings) &&
             add_size(*walk->values, merge.count, walk->values) &&
             add_size(*walk->merge_total, merge.count, walk->merge_total);
    }
    if (!hir0_walk_statement_chain(walk, statement->first_child, true,
                                   depth + 1u) ||
        !hir0_walk_statement_chain(walk, statement->else_child, true,
                                   depth + 1u))
      return false;
    return true;
  }
  if (statement->kind == W_SEED_FRONTEND_STMT_RETURN) {
    const w_seed_frontend_function *function =
        &walk->output->functions[walk->function_index];
    if (statement->expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)statement->expression_index >=
            walk->result->written.expressions)
      return false;
    const w_seed_frontend_expression *return_expression =
        &walk->output->expressions[statement->expression_index];
    if (statement->expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)statement->expression_index >=
            walk->result->written.expressions ||
        function->return_type == W_SEED_FRONTEND_NONE ||
        (size_t)function->return_type >= walk->result->written.types ||
        return_expression->inferred_type == W_SEED_FRONTEND_NONE ||
        (size_t)return_expression->inferred_type >=
            walk->result->written.types ||
        ((walk->output->types[return_expression->inferred_type].kind ==
              W_SEED_FRONTEND_TYPE_ENUM_SUBSET ||
          walk->output->types[function->return_type].kind ==
              W_SEED_FRONTEND_TYPE_ENUM_SUBSET) &&
         !frontend_type_assignable_for_input(
             walk->input,
             &walk->output->types[return_expression->inferred_type],
             &walk->output->types[function->return_type])))
      return false;
    if ((branch && !walk->allow_branch_return) ||
        statement->next_sibling != W_SEED_FRONTEND_NONE ||
        statement->binding_name.length != 0u ||
        statement->declared_type != W_SEED_FRONTEND_NONE ||
        statement->effective_type != W_SEED_FRONTEND_NONE ||
        statement->expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)statement->expression_index >=
            walk->result->written.expressions ||
        walk->output->types[walk->output->functions[walk->function_index]
                                .return_type]
                .kind == W_SEED_FRONTEND_TYPE_UNIT ||
        ((walk->output->expressions[statement->expression_index].kind ==
              W_SEED_FRONTEND_EXPR_SWITCH)
             ? !frontend_switch_return_ok(walk, index,
                                           statement->expression_index)
             : !frontend_value_tree_ok(
                   walk->input, walk->module_index, walk->function_index,
                   walk->document_index, index, statement->expression_index,
                   0u, walk->expression_cursor,
                   walk->interpolation_segment_cursor,
                   walk->const_byte_cursor, walk->values, walk->segments,
                   walk->value_bytes, walk->calls, walk->arguments,
                   walk->logical_total)))
      return false;
    walk->has_value_return = true;
    return true;
  }
  return false;
}

static bool hir0_walk_statement_chain(hir0_statement_walk *walk,
                                      uint32_t first_statement, bool branch,
                                      size_t depth) {
  if (walk == NULL || first_statement == W_SEED_FRONTEND_NONE) return true;
  uint32_t cursor = first_statement;
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < walk->result->written.statements) {
    if (!hir0_walk_statement(walk, cursor, branch, depth)) return false;
    cursor = walk->output->statements[cursor].next_sibling;
    guard += 1u;
  }
  return cursor == W_SEED_FRONTEND_NONE;
}

static bool frontend_statement_and_expression_cfg_ok(
    const w_seed_hir0_input *input, size_t *binding_total, size_t *call_total,
    size_t *yield_total,
    size_t *argument_total, size_t *value_total, size_t *segment_total,
    size_t *value_bytes, size_t *text_bytes, size_t *if_total,
    size_t *logical_total, size_t *merge_total, size_t *while_total,
    size_t *loop_carrier_total, size_t *switch_total, size_t *switch_edge_total,
    size_t *switch_capture_total) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || binding_total == NULL ||
      call_total == NULL || yield_total == NULL || argument_total == NULL ||
      segment_total == NULL || value_bytes == NULL || text_bytes == NULL ||
      if_total == NULL || logical_total == NULL || merge_total == NULL ||
      while_total == NULL || switch_total == NULL || switch_edge_total == NULL ||
      switch_capture_total == NULL || loop_carrier_total == NULL)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  size_t bindings = 0u;
  size_t calls = 0u;
  size_t yields = 0u;
  size_t arguments = 0u;
  size_t values = 0u;
  size_t segments = 0u;
  size_t value_bytes_count = 0u;
  size_t expression_cursor = 0u;
  size_t interpolation_segment_cursor = 0u;
  size_t const_byte_cursor = 0u;
  size_t if_count = 0u;
  size_t logical_count = 0u;
  size_t merge_count = 0u;
  size_t while_count = 0u;
  size_t loop_carrier_count = 0u;
  size_t switch_count = 0u;
  size_t switch_edge_count = 0u;
  size_t switch_capture_count = 0u;
  for (size_t function_index = 0u;
       function_index < result->written.functions; function_index += 1u) {
    const w_seed_frontend_function *function =
        &output->functions[function_index];
    size_t relation_if_count = 0u;
    if (!frontend_statement_relations_ok(input, function_index,
                                         &relation_if_count))
      return false;
    size_t function_if_count = 0u;
    size_t function_logical_count = 0u;
    size_t function_merge_count = 0u;
    size_t function_while_count = 0u;
    size_t function_loop_carrier_count = 0u;
    size_t function_switch_count = 0u;
    size_t function_switch_edge_count = 0u;
    size_t function_switch_capture_count = switch_capture_count;
    const w_seed_frontend_type_kind return_kind =
        output->types[function->return_type].kind;
    hir0_statement_walk walk = {
        .input = input,
        .output = output,
        .result = result,
        .function_index = function_index,
        .module_index = function->module_index,
        .document_index = output->modules[function->module_index].document_index,
        .expression_cursor = &expression_cursor,
        .interpolation_segment_cursor = &interpolation_segment_cursor,
        .const_byte_cursor = &const_byte_cursor,
        .bindings = &bindings,
        .calls = &calls,
        .yields = &yields,
        .arguments = &arguments,
        .values = &values,
        .segments = &segments,
        .value_bytes = &value_bytes_count,
        .if_total = &function_if_count,
        .logical_total = &function_logical_count,
        .merge_total = &function_merge_count,
        .while_total = &function_while_count,
        .loop_carrier_total = &function_loop_carrier_count,
        .switch_total = &function_switch_count,
        .switch_edge_total = &function_switch_edge_count,
        .switch_capture_total = &function_switch_capture_count,
        .has_value_return = false,
        .loop_seen = false,
        .loop_continuation_seen = false,
        .loop_root_statements = {0u},
        .loop_root_count = 0u,
        .allow_branch_return = frontend_function_has_terminal_if(
            input, function_index),
    };
    /* An empty declaration (notably `entry {}` used as a separate test
     * entry) owns no statement record; its first_statement is the append
     * cursor at the end of the shared table. */
    const bool walked =
        function->statement_count == 0u ||
        hir0_walk_statement_chain(&walk, function->first_statement, false, 0u);
    if (!walked || relation_if_count != function_if_count ||
        (return_kind == W_SEED_FRONTEND_TYPE_UNIT && walk.has_value_return) ||
        (return_kind != W_SEED_FRONTEND_TYPE_UNIT &&
         (!walk.has_value_return ||
          (function_if_count > function_merge_count &&
           !frontend_function_has_terminal_if(input, function_index)))))
      return false;
    /* M2b accepts a switch only as the simple CFG root.  A function that also
     * owns an IF/logical/while CFG would require a composed layout proof that
     * this bounded emitter does not yet provide. */
    if (function_switch_count != 0u &&
        (function_if_count != 0u || function_logical_count != 0u ||
         function_while_count != 0u))
      return false;
    if (!add_size(if_count, function_if_count, &if_count)) return false;
    if (!add_size(logical_count, function_logical_count, &logical_count))
      return false;
    if (!add_size(merge_count, function_merge_count, &merge_count)) return false;
    if (!add_size(while_count, function_while_count, &while_count)) return false;
    if (!add_size(loop_carrier_count, function_loop_carrier_count,
                  &loop_carrier_count))
      return false;
    if (!add_size(switch_count, function_switch_count, &switch_count) ||
        !add_size(switch_edge_count, function_switch_edge_count,
                  &switch_edge_count))
      return false;
    switch_capture_count = function_switch_capture_count;
  }
  if (arguments != result->written.arguments ||
      expression_cursor != result->written.expressions ||
      interpolation_segment_cursor != result->written.interpolation_segments ||
      const_byte_cursor != result->written.const_bytes ||
      switch_capture_count != result->written.pattern_captures ||
      !count_u32(bindings) || !count_u32(calls) || !count_u32(yields) ||
      !count_u32(arguments) ||
      !count_u32(values) || !count_u32(segments) || !count_u32(if_count) ||
      !count_u32(logical_count) || !count_u32(merge_count) ||
      !count_u32(while_count) || !count_u32(switch_count) ||
      !count_u32(switch_edge_count) || !count_u32(loop_carrier_count))
    return false;
  *binding_total = bindings;
  *call_total = calls;
  *yield_total = yields;
  *argument_total = arguments;
  *value_total = values;
  *segment_total = segments;
  *value_bytes = value_bytes_count;
  *text_bytes = 0u;
  *if_total = if_count;
  *logical_total = logical_count;
  *merge_total = merge_count;
  *while_total = while_count;
  *loop_carrier_total = loop_carrier_count;
  *switch_total = switch_count;
  *switch_edge_total = switch_edge_count;
  *switch_capture_total = switch_capture_count;
  return true;
}

static bool frontend_statement_and_expression_ok(
    const w_seed_hir0_input *input, size_t *binding_total, size_t *call_total,
    size_t *yield_total,
    size_t *argument_total, size_t *value_total, size_t *segment_total,
    size_t *value_bytes, size_t *text_bytes, size_t *if_total,
    size_t *logical_total, size_t *merge_total, size_t *while_total,
    size_t *loop_carrier_total, size_t *switch_total, size_t *switch_edge_total,
    size_t *switch_capture_total) {
  return frontend_statement_and_expression_cfg_ok(
      input, binding_total, call_total, yield_total, argument_total, value_total,
      segment_total, value_bytes, text_bytes, if_total, logical_total,
      merge_total, while_total, loop_carrier_total, switch_total, switch_edge_total,
      switch_capture_total);
}

static bool frontend_enum_constructor_payload_count(
    const w_seed_hir0_input *input, size_t *total) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || total == NULL)
    return false;
  *total = 0u;
  const w_seed_frontend_output *output = input->frontend_output;
  for (size_t index = 0u;
       index < input->frontend_result->written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &output->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_CALL ||
        expression->left == W_SEED_FRONTEND_NONE ||
        (size_t)expression->left >=
            input->frontend_result->written.expressions)
      continue;
    const w_seed_frontend_expression *callee =
        &output->expressions[expression->left];
    if (callee->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE &&
        callee->enum_index != W_SEED_FRONTEND_NONE &&
        !add_size(*total, expression->argument_count, total))
      return false;
  }
  return count_u32(*total);
}

static bool frontend_external_enum_constructor_argument_count(
    const w_seed_hir0_input *input, size_t *total) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || total == NULL)
    return false;
  *total = 0u;
  const w_seed_frontend_output *output = input->frontend_output;
  for (size_t index = 0u;
       index < input->frontend_result->written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &output->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_CALL ||
        expression->left == W_SEED_FRONTEND_NONE ||
        (size_t)expression->left >=
            input->frontend_result->written.expressions)
      continue;
    const w_seed_frontend_expression *callee =
        &output->expressions[expression->left];
    if (callee->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE &&
        callee->enum_index == W_SEED_FRONTEND_NONE &&
        callee->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL &&
        callee->resolved_external_module_index == 0u &&
        callee->resolved_external_symbol_index == 5u &&
        !add_size(*total, expression->argument_count, total))
      return false;
  }
  return count_u32(*total);
}

static bool frontend_external_type_is(const w_seed_hir0_input *input,
                                      uint32_t type_index,
                                      uint32_t module_index,
                                      uint32_t symbol_index) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || type_index == W_SEED_FRONTEND_NONE ||
      (size_t)type_index >= input->frontend_result->written.types)
    return false;
  const w_seed_frontend_type *type = &input->frontend_output->types[type_index];
  return type->kind == W_SEED_FRONTEND_TYPE_NOMINAL &&
         type->external_module_index == module_index &&
         type->external_symbol_index == symbol_index &&
         frontend_external_type_pair_valid(input, module_index, symbol_index);
}

static bool frontend_external_member_value_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index,
    const w_seed_frontend_expression *value) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || value == NULL ||
      !frontend_external_process_records_ok(input) ||
      input->frontend_input->external_modules[0].symbol_count != 7u ||
      value->kind != W_SEED_FRONTEND_EXPR_MEMBER || !value->supported ||
      !text_is(value->operator_text, ".") ||
      value->module_index != module_index ||
      value->owner_function != function_index ||
      value->left == W_SEED_FRONTEND_NONE ||
      (size_t)value->left >= input->frontend_result->written.expressions ||
      value->right != W_SEED_FRONTEND_NONE ||
      value->first_argument != W_SEED_FRONTEND_NONE ||
      value->argument_count != 0u ||
      value->resolved_external_module_index != 0u ||
      (value->resolved_external_symbol_index != 4u &&
       value->resolved_external_symbol_index != 6u) ||
      value->member_name.length == 0u ||
      ((value->resolved_external_symbol_index == 4u &&
        !text_is(value->member_name, HIR0_PROCESS_IS_EMPTY)) ||
       (value->resolved_external_symbol_index == 6u &&
        !text_is(value->member_name, HIR0_PROCESS_COUNT))) ||
      value->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE ||
      value->resolved_function_index != W_SEED_FRONTEND_NONE ||
      value->resolved_callee_kind !=
          W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL ||
      value->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
      value->resolved_local_ordinal != W_SEED_FRONTEND_NONE ||
      value->resolved_const_declaration != W_SEED_FRONTEND_NONE ||
      value->resolved_pattern_capture != W_SEED_FRONTEND_NONE ||
      value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
      value->first_switch_arm != W_SEED_FRONTEND_NONE ||
      value->switch_arm_count != 0u ||
      value->first_membership_case != W_SEED_FRONTEND_NONE ||
      value->membership_case_count != 0u ||
      value->const_byte_offset != W_SEED_FRONTEND_NONE ||
      value->const_byte_count != 0u || value->has_bool_value ||
      value->has_integer_value ||
      value->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
      value->interpolation_segment_count != 0u ||
      value->inferred_type == W_SEED_FRONTEND_NONE ||
      (size_t)value->inferred_type >= input->frontend_result->written.types ||
      !frontend_span_ok(&input->frontend_input->documents[document_index],
                        value->span))
    return false;
  const w_seed_frontend_expression *receiver =
      &input->frontend_output->expressions[value->left];
  const w_seed_frontend_type *member_type =
      &input->frontend_output->types[value->inferred_type];
  if ((value->resolved_external_symbol_index == 4u &&
       member_type->kind != W_SEED_FRONTEND_TYPE_BOOL) ||
      (value->resolved_external_symbol_index == 6u &&
       !frontend_type_is_usize(member_type)) ||
      !text_equal(
          value->member_name,
          input->frontend_input->external_modules[0]
              .symbols[value->resolved_external_symbol_index]
              .name))
    return false;
  if (receiver->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
      !receiver->supported || receiver->module_index != module_index ||
      receiver->owner_function != function_index ||
      receiver->resolved_parameter_ordinal == W_SEED_FRONTEND_NONE ||
      receiver->resolved_parameter_ordinal >=
          input->frontend_output->functions[function_index].parameter_count ||
      receiver->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
      receiver->resolved_pattern_capture != W_SEED_FRONTEND_NONE ||
      receiver->resolved_function_index != W_SEED_FRONTEND_NONE ||
      receiver->resolved_callee_kind != W_SEED_FRONTEND_CALLEE_NONE ||
      receiver->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
      receiver->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
      receiver->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
      receiver->resolved_local_ordinal != W_SEED_FRONTEND_NONE ||
      receiver->resolved_const_declaration != W_SEED_FRONTEND_NONE ||
      receiver->member_name.length != 0u || !text_valid(receiver->member_name) ||
      receiver->left != W_SEED_FRONTEND_NONE ||
      receiver->right != W_SEED_FRONTEND_NONE ||
      receiver->first_argument != W_SEED_FRONTEND_NONE ||
      receiver->argument_count != 0u ||
      receiver->first_switch_arm != W_SEED_FRONTEND_NONE ||
      receiver->switch_arm_count != 0u ||
      receiver->first_membership_case != W_SEED_FRONTEND_NONE ||
      receiver->membership_case_count != 0u ||
      receiver->const_byte_offset != W_SEED_FRONTEND_NONE ||
      receiver->const_byte_count != 0u || receiver->has_bool_value ||
      receiver->has_integer_value ||
      receiver->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
      receiver->interpolation_segment_count != 0u ||
      !frontend_external_type_is(input, receiver->inferred_type, 0u, 0u) ||
      !text_equal(receiver->spelling,
                  input->frontend_output->parameters[
                      (size_t)input->frontend_output->functions[function_index]
                          .first_parameter + receiver->resolved_parameter_ordinal]
                      .name) ||
      !frontend_span_ok(&input->frontend_input->documents[document_index],
                        receiver->span))
    return false;
  const w_seed_frontend_parameter *parameter =
      &input->frontend_output->parameters[
          (size_t)input->frontend_output->functions[function_index]
                  .first_parameter + receiver->resolved_parameter_ordinal];
  return parameter->owner_function == function_index &&
         parameter->module_index == module_index &&
         parameter->label_kind == W_SEED_FRONTEND_LABEL_REQUIRED;
}

/* Keep the only usize comparison at the public process boundary.  The
 * frontend contextualizes an unsuffixed literal against Args.count as usize;
 * HIR preserves that logical type in a dedicated constant record.  This
 * predicate never admits the result as a general usize expression. */
static bool frontend_usize_count_comparison_literal_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index,
    const w_seed_frontend_expression *value) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || value == NULL ||
      !frontend_has_public_process(input) ||
      !frontend_value_common_ok(input, value, module_index, function_index,
                                document_index) ||
      value->kind != W_SEED_FRONTEND_EXPR_INTEGER ||
      value->left != W_SEED_FRONTEND_NONE ||
      value->right != W_SEED_FRONTEND_NONE ||
      value->first_argument != W_SEED_FRONTEND_NONE ||
      value->argument_count != 0u ||
      value->inferred_type == W_SEED_FRONTEND_NONE ||
      !frontend_expression_is_usize(input->frontend_output,
                                    input->frontend_result, value) ||
      !value->has_integer_value || value->has_bool_value ||
      !frontend_value_has_no_resolution(value) ||
      value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
      value->const_byte_offset != W_SEED_FRONTEND_NONE ||
      value->const_byte_count != 0u ||
      value->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
      value->interpolation_segment_count != 0u ||
      value->else_expression != W_SEED_FRONTEND_NONE ||
      value->enum_index != W_SEED_FRONTEND_NONE ||
      value->enum_case_index != W_SEED_FRONTEND_NONE)
    return false;
  uint64_t integer_value = 0u;
  return frontend_integer_u64(value, &integer_value);
}

static bool frontend_usize_count_comparison_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index,
    const w_seed_frontend_expression *value) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL || value == NULL ||
      !frontend_has_public_process(input) ||
      !frontend_value_common_ok(input, value, module_index, function_index,
                                document_index) ||
      value->kind != W_SEED_FRONTEND_EXPR_BINARY ||
      value->left == W_SEED_FRONTEND_NONE ||
      value->right == W_SEED_FRONTEND_NONE ||
      (size_t)value->left >= input->frontend_result->written.expressions ||
      (size_t)value->right >= input->frontend_result->written.expressions ||
      value->inferred_type == W_SEED_FRONTEND_NONE ||
      !frontend_expression_is_bool(input->frontend_output, value) ||
      !frontend_value_has_no_resolution(value) ||
      value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
      value->const_byte_offset != W_SEED_FRONTEND_NONE ||
      value->const_byte_count != 0u || value->has_bool_value ||
      value->has_integer_value || value->else_expression !=
                                       W_SEED_FRONTEND_NONE ||
      hir_logical_operator(value->operator_text) !=
          W_SEED_HIR0_LOGICAL_NONE ||
      (!text_is(value->operator_text, "==") &&
       !text_is(value->operator_text, "!=") &&
       !text_is(value->operator_text, "<") &&
       !text_is(value->operator_text, "<=") &&
       !text_is(value->operator_text, ">") &&
       !text_is(value->operator_text, ">=")))
    return false;
  const w_seed_frontend_expression *left =
      &input->frontend_output->expressions[value->left];
  const w_seed_frontend_expression *right =
      &input->frontend_output->expressions[value->right];
  const bool left_count =
      frontend_external_member_value_ok(input, module_index, function_index,
                                        document_index, left) &&
      left->resolved_external_symbol_index == 6u &&
      frontend_expression_is_usize(input->frontend_output,
                                   input->frontend_result, left);
  const bool right_count =
      frontend_external_member_value_ok(input, module_index, function_index,
                                        document_index, right) &&
      right->resolved_external_symbol_index == 6u &&
      frontend_expression_is_usize(input->frontend_output,
                                   input->frontend_result, right);
  const bool left_literal = frontend_usize_count_comparison_literal_ok(
      input, module_index, function_index, document_index, left);
  const bool right_literal = frontend_usize_count_comparison_literal_ok(
      input, module_index, function_index, document_index, right);
  return (left_count && right_literal) || (right_count && left_literal);
}

static bool frontend_statement_chain_ends_in_return(
    const w_seed_hir0_input *input, uint32_t first_statement) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL)
    return false;
  uint32_t cursor = first_statement;
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < input->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        &input->frontend_output->statements[cursor];
    if (statement->next_sibling == W_SEED_FRONTEND_NONE)
      return statement->kind == W_SEED_FRONTEND_STMT_RETURN;
    cursor = statement->next_sibling;
    guard += 1u;
  }
  return false;
}

static bool frontend_function_has_terminal_if(const w_seed_hir0_input *input,
                                              size_t function_index) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL ||
      function_index >= input->frontend_result->written.functions)
    return false;
  const w_seed_frontend_function *function =
      &input->frontend_output->functions[function_index];
  uint32_t cursor = function->statement_count == 0u
                        ? W_SEED_FRONTEND_NONE
                        : function->first_statement;
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < input->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        &input->frontend_output->statements[cursor];
    if (statement->next_sibling == W_SEED_FRONTEND_NONE)
      return statement->kind == W_SEED_FRONTEND_STMT_IF &&
             statement->first_child != W_SEED_FRONTEND_NONE &&
             statement->else_child != W_SEED_FRONTEND_NONE &&
             frontend_statement_chain_ends_in_return(input,
                                                     statement->first_child) &&
             frontend_statement_chain_ends_in_return(input,
                                                     statement->else_child);
    cursor = statement->next_sibling;
    guard += 1u;
  }
  return false;
}

/* The process adapter is selected from the resolved entry ABI, not from a
 * source spelling or a particular function ordinal. Helpers may precede the
 * entry target and its parameter labels/names are intentionally opaque. */
static bool frontend_process_entry_abi_ok(const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL ||
      !frontend_external_process_records_ok(input) ||
      input->frontend_input->external_modules[0].symbol_count != 7u ||
      !text_is(input->frontend_input->host_scope->profile,
               HIR0_PROCESS_PROFILE) ||
      input->frontend_result->written.entries != 1u ||
      input->frontend_output->entries == NULL)
    return false;
  const w_seed_frontend_entry *entry = &input->frontend_output->entries[0];
  if (!entry->valid || entry->is_body || entry->target_function ==
          W_SEED_FRONTEND_NONE ||
      (size_t)entry->target_function >=
          input->frontend_result->written.functions)
    return false;
  const size_t function_index = entry->target_function;
  const w_seed_frontend_function *function =
      &input->frontend_output->functions[function_index];
  if (function->module_index != 0u || !function->is_async || function->is_const ||
      function->is_throws || function->is_unsafe ||
      function->has_borrow_clause || function->is_anonymous_entry ||
      function->parameter_count != 2u ||
      !frontend_external_type_is(input, function->return_type, 0u, 2u) ||
      !range_valid(function->first_parameter, function->parameter_count,
                   input->frontend_result->written.parameters))
    return false;
  for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
    const w_seed_frontend_parameter *parameter =
        &input->frontend_output->parameters[(size_t)function->first_parameter +
                                            ordinal];
    if (parameter->owner_function != function_index ||
        parameter->module_index != 0u ||
        parameter->label_kind != W_SEED_FRONTEND_LABEL_REQUIRED ||
        !frontend_label_valid(parameter->label_kind, parameter->label) ||
        parameter->name.length == 0u || !text_valid(parameter->name) ||
        !frontend_external_type_is(input, parameter->type_index, 0u,
                                   (uint32_t)ordinal))
      return false;
  }
  return true;
}

/* This is the complete HIR16 process-handler contract. It is a bounded
 * consumer shape, not a general language or directEntry proof. */
static bool frontend_process_handler_ok(const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_output == NULL ||
      input->frontend_result == NULL ||
      !frontend_external_process_records_ok(input))
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (input->frontend_input->external_module_count == 0u)
    return true;
  if (input->frontend_input->external_modules[0].symbol_count == 7u)
    return frontend_process_entry_abi_ok(input);
  if (input->frontend_input->external_modules[0].symbol_count != 4u)
    return false;
  if (!text_is(input->frontend_input->host_scope->profile,
               HIR0_PROCESS_PROFILE))
    return false;
  if (result->written.functions != 1u || result->written.entries != 1u ||
      result->written.parameters != 2u)
    return false;
  const w_seed_frontend_function *function = &output->functions[0];
  const w_seed_frontend_entry *entry = &output->entries[0];
  if (!function->is_async || function->is_const || function->is_throws ||
      function->is_unsafe || function->has_borrow_clause ||
      function->is_anonymous_entry || entry->is_body ||
      entry->target_function != 0u ||
      !frontend_external_type_is(input, function->return_type, 0u, 2u) ||
      function->parameter_count != 2u || function->first_parameter != 0u)
    return false;
  for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
    const w_seed_frontend_parameter *parameter = &output->parameters[ordinal];
    if (parameter->owner_function != 0u || parameter->module_index != 0u ||
        parameter->label_kind != W_SEED_FRONTEND_LABEL_REQUIRED ||
        !text_is(parameter->name, ordinal == 0u ? "args" : "ctx") ||
        !text_equal(parameter->label, parameter->name) ||
        !frontend_external_type_is(input, parameter->type_index, 0u,
                                    (uint32_t)ordinal))
      return false;
  }
  if (function->statement_count != 1u || function->first_statement != 0u)
    return false;
  const w_seed_frontend_statement *statement = &output->statements[0];
  if (statement->kind != W_SEED_FRONTEND_STMT_RETURN ||
      statement->next_sibling != W_SEED_FRONTEND_NONE ||
      statement->expression_index == W_SEED_FRONTEND_NONE ||
      (size_t)statement->expression_index >= result->written.expressions)
    return false;
  return statement->kind == W_SEED_FRONTEND_STMT_RETURN &&
         statement->next_sibling == W_SEED_FRONTEND_NONE &&
         frontend_external_exit_code_case_ok(
             input, &output->expressions[statement->expression_index]);
}

static bool add_text_size(w_seed_frontend_text text, size_t *total) {
  return text_valid(text) && add_size(*total, text.length, total);
}

static bool text_size_for_input(const w_seed_hir0_input *input, size_t *total) {
  if (input == NULL || total == NULL) return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  size_t value = frontend_has_public_process(input) ? 20u : 15u;
  /* Canonical Unit, String, i64, Bool, and bounded public-process usize. */
  for (size_t index = 0u; index < result->written.modules; index += 1u) {
    const w_seed_frontend_module *module = &output->modules[index];
    if (!add_text_size(module->source_id, &value) ||
        !add_text_size(module->module_id, &value) ||
        !add_text_size(module->local_module_name, &value))
      return false;
  }
  for (size_t module_index = 0u;
       module_index < input->frontend_input->external_module_count;
       module_index += 1u) {
    const w_seed_frontend_external_module *module =
        &input->frontend_input->external_modules[module_index];
    if (!add_text_size(module->module_id, &value)) return false;
    for (size_t symbol_index = 0u; symbol_index < module->symbol_count;
         symbol_index += 1u) {
      const w_seed_frontend_external_symbol *symbol =
          &module->symbols[symbol_index];
      if (!add_text_size(symbol->name, &value))
        return false;
    }
  }
  for (size_t index = 0u; index < result->written.functions; index += 1u)
    if (!add_text_size(output->functions[index].name, &value)) return false;
  for (size_t index = 0u; index < result->written.enums; index += 1u) {
    const w_seed_frontend_enum *decl = &output->enums[index];
    if (!add_text_size(decl->name, &value)) return false;
    for (size_t ordinal = 0u; ordinal < decl->case_count; ordinal += 1u)
      if (!add_text_size(
              output->enum_cases[(size_t)decl->first_case + ordinal].name,
              &value))
        return false;
  }
  for (size_t index = 0u; index < result->written.enum_case_parameters;
       index += 1u)
    if (!add_text_size(output->enum_case_parameters[index].label, &value))
      return false;
  for (size_t index = 0u; index < result->written.parameters; index += 1u)
    if (!add_text_size(output->parameters[index].name, &value) ||
        !add_text_size(output->parameters[index].label, &value))
      return false;
  for (size_t index = 0u; index < result->written.statements; index += 1u) {
    const w_seed_frontend_statement *statement = &output->statements[index];
    if (statement->kind == W_SEED_FRONTEND_STMT_LET ||
        statement->kind == W_SEED_FRONTEND_STMT_VAR) {
      if (!add_text_size(statement->binding_name, &value)) return false;
      continue;
    }
    hir0_branch_assignment_merge merge;
    if (statement->kind == W_SEED_FRONTEND_STMT_IF &&
        frontend_branch_assignment_merge(
            output, result, statement->owner_function, (uint32_t)index,
            &merge)) {
      for (size_t entry = 0u; entry < merge.count; entry += 1u) {
        const w_seed_frontend_expression *assignment =
            &output->expressions[output->statements[
                merge.entries[entry].then_statement]
                                     .expression_index];
        if (!add_text_size(output->expressions[assignment->left].spelling,
                           &value))
          return false;
      }
      continue;
    }
    if (statement->kind != W_SEED_FRONTEND_STMT_EXPRESSION ||
        statement->expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)statement->expression_index >= result->written.expressions)
      continue;
    const w_seed_frontend_expression *assignment =
        &output->expressions[statement->expression_index];
    if (assignment->kind != W_SEED_FRONTEND_EXPR_ASSIGNMENT) continue;
    uint32_t merge_owner = W_SEED_FRONTEND_NONE;
    if (frontend_assignment_merge_owner(
            output, result, statement->owner_function, (uint32_t)index,
            &merge_owner))
      continue;
    if (assignment->left == W_SEED_FRONTEND_NONE ||
        (size_t)assignment->left >= result->written.expressions ||
        !add_text_size(output->expressions[assignment->left].spelling,
                       &value))
      return false;
  }
  for (size_t index = 0u; index < result->written.entries; index += 1u) {
    const w_seed_frontend_entry *entry = &output->entries[index];
    if ((size_t)entry->target_function >= result->written.functions ||
        !add_text_size(output->functions[entry->target_function].name,
                       &value) ||
        !add_text_size((w_seed_frontend_text){HIR0_SLOT_NAME,
                                              sizeof(HIR0_SLOT_NAME) - 1u},
                       &value))
      return false;
  }
  const w_seed_frontend_host_prelude *scope = input->frontend_input->host_scope;
  for (size_t index = 0u; index < scope->symbol_count; index += 1u) {
    const w_seed_frontend_host_prelude_symbol *symbol = &scope->symbols[index];
    if (!add_text_size(symbol->name, &value) ||
        !add_text_size(scope->profile, &value)) return false;
    for (size_t parameter = 0u; parameter < symbol->parameter_count; parameter += 1u) {
      const w_seed_frontend_external_parameter *host_parameter =
          &symbol->parameters[parameter];
      if (!add_text_size(host_parameter->name, &value) ||
          (host_parameter->label_kind !=
               W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
           !add_text_size(host_parameter->name, &value)))
        return false;
    }
    for (size_t requirement = 0u; requirement < symbol->requirement_count;
         requirement += 1u)
      if (!add_text_size(symbol->requirements[requirement].name, &value)) return false;
  }
  /* Labels are copied once per frontend argument because they are call facts. */
  for (size_t index = 0u; index < result->written.arguments; index += 1u)
    if (!add_text_size(output->arguments[index].label, &value)) return false;
  for (size_t index = 0u; index < result->written.pattern_captures;
       index += 1u)
    if (!add_text_size(output->pattern_captures[index].name, &value))
      return false;
  return count_u32(value) && value <= W_SEED_HIR0_MAX_TEXT_BYTES
             ? (*total = value, true)
             : false;
}

static hir0_prepare_status collect(const w_seed_hir0_input *input,
                                   w_seed_hir0_counts *counts) {
  if (counts == NULL) return HIR0_PREPARE_INVALID;
  (void)memset(counts, 0, sizeof(*counts));
  /* Keep the pre-existing closed-family classification for synthetic
   * frontend family counters that do not describe an actual subset type.
   * A real subset projection proceeds through the shape/record validators
   * below, where its caller-owned member range is checked in full. */
  if (input != NULL && input->frontend_result != NULL) {
    if (input->frontend_result->written.enum_membership_cases != 0u)
      return HIR0_PREPARE_UNSUPPORTED;
    if (input->frontend_result->written.enum_subset_members != 0u &&
        input->frontend_output != NULL && input->frontend_output->types != NULL &&
        input->frontend_result->written.types <=
            input->frontend_output->type_capacity) {
      bool has_subset_type = false;
      for (size_t type = 0u; type < input->frontend_result->written.types;
           type += 1u)
        if (input->frontend_output->types[type].kind ==
            W_SEED_FRONTEND_TYPE_ENUM_SUBSET) {
          has_subset_type = true;
          break;
        }
      if (!has_subset_type) return HIR0_PREPARE_UNSUPPORTED;
    }
  }
  if (!frontend_shape_ok(input)) return HIR0_PREPARE_FRONTEND;
  const w_seed_frontend_result *frontend_result = input->frontend_result;
  if (!frontend_sources_ok(input)) return HIR0_PREPARE_INVALID;
  if (!host_shape_ok(input->frontend_input->host_scope))
    return HIR0_PREPARE_INVALID;
  if (frontend_task_result_is_unsupported(input))
    return HIR0_PREPARE_UNSUPPORTED;
  /* HIR0 is intentionally closed. Classify frontend families that have no
   * HIR0 record before relational validators walk their optional arrays. This
   * preserves the unsupported-family result even when a caller supplies a
   * nonzero count with no backing storage. */
  if ((input->frontend_input->external_module_count == 0u &&
       !input->frontend_input->import_resolution_complete &&
       (frontend_result->written.imports != 0u ||
        frontend_result->written.import_items != 0u)) ||
      frontend_result->written.structs != 0u ||
      frontend_result->written.fields != 0u ||
      frontend_result->written.type_declarations != 0u ||
      (frontend_result->written.aliases != 0u &&
       !frontend_enum_subset_aliases_ok(input)) ||
      frontend_result->written.facts != 0u ||
      frontend_result->written.diagnostics != 0u ||
      frontend_result->written.diagnostic_facts != 0u ||
      frontend_result->written.diagnostic_items != 0u ||
      frontend_result->written.diagnostic_labels != 0u ||
      frontend_result->written.enum_membership_cases != 0u ||
      frontend_result->written.generic_parameters != 0u ||
      frontend_result->written.generic_applications != 0u ||
      frontend_result->written.generic_arguments != 0u ||
      frontend_result->written.typed_const_expressions != 0u ||
      frontend_result->written.const_values != 0u ||
      frontend_result->written.const_elements != 0u ||
      frontend_result->written.const_declarations != 0u ||
      frontend_result->written.parameters > W_SEED_HIR0_MAX_TEXT_BYTES)
    return HIR0_PREPARE_UNSUPPORTED;
  if (!frontend_module_ranges_ok(input) || !frontend_type_records_ok(input) ||
      !frontend_task_usage_ok(input) ||
      !frontend_function_ranges_ok(input) || !frontend_entry_records_ok(input) ||
      (frontend_result->written.aliases == 0u &&
       !frontend_symbol_records_ok(input)))
    return HIR0_PREPARE_INVALID;
  /* Structured launch target facts are consumed by the post-emission direct
   * entry publication.  Recompute the bounded body proof now as well so an
   * absent direct entry is reported before caller-owned HIR bytes change. */
  if (!frontend_structured_elision_preflight(input))
    return HIR0_PREPARE_UNSUPPORTED;
  /* An alias record is part of the accepted subset projection only when its
   * target is a validated local enum subset.  Classify all other alias
   * families at the existing closed-family barrier before symbol-count
   * validation, so a forged unsupported-family counter cannot masquerade as a
   * malformed HIR0 symbol table. */
  if (frontend_result->written.aliases != 0u &&
      !frontend_enum_subset_aliases_ok(input))
    return HIR0_PREPARE_UNSUPPORTED;
  if (frontend_result->written.aliases != 0u &&
      !frontend_symbol_records_ok(input))
    return HIR0_PREPARE_INVALID;
  if (!frontend_external_process_records_ok(input))
    return HIR0_PREPARE_INVALID;
  if (!frontend_enum_payload_types_supported(input))
    return HIR0_PREPARE_UNSUPPORTED;
  if (input->frontend_input->external_module_count != 0u &&
      !frontend_process_handler_ok(input))
    return HIR0_PREPARE_UNSUPPORTED;
  /* HIR0 accepts a bounded linear subset and nested Unit structured CFG. */
  size_t binding_count = 0u;
  size_t call_count = 0u;
  size_t yield_count = 0u;
  size_t argument_count = 0u;
  size_t value_count = 0u;
  size_t interpolation_segment_count = 0u;
  size_t enum_payload_count = 0u;
  size_t external_enum_constructor_argument_count = 0u;
  size_t value_bytes = 0u;
  size_t ignored_text = 0u;
  size_t if_count = 0u;
  size_t logical_count = 0u;
  size_t merge_count = 0u;
  size_t while_count = 0u;
  size_t loop_carrier_count = 0u;
  size_t switch_count = 0u;
  size_t switch_edge_count = 0u;
  size_t switch_capture_count = 0u;
  if (!frontend_statement_and_expression_ok(
                  input, &binding_count, &call_count, &yield_count,
                  &argument_count,
                  &value_count, &interpolation_segment_count, &value_bytes,
                  &ignored_text, &if_count, &logical_count, &merge_count,
                  &while_count, &loop_carrier_count, &switch_count,
                  &switch_edge_count,
                  &switch_capture_count))
    return HIR0_PREPARE_UNSUPPORTED;
  if (!frontend_enum_constructor_payload_count(input, &enum_payload_count) ||
      enum_payload_count > argument_count)
    return HIR0_PREPARE_UNSUPPORTED;
  if (!frontend_external_enum_constructor_argument_count(
          input, &external_enum_constructor_argument_count) ||
      external_enum_constructor_argument_count > argument_count -
                                                 enum_payload_count)
    return HIR0_PREPARE_UNSUPPORTED;
  /* The frontend switch-arm family is caller-owned.  Accept it only when
   * the closed statement walk consumed exactly one edge record per arm;
   * forged or partial arm ranges must not silently disappear at HIR0. */
  if (switch_edge_count != frontend_result->written.switch_arms)
    return HIR0_PREPARE_UNSUPPORTED;
  size_t text_bytes = 0u;
  if (!text_size_for_input(input, &text_bytes)) return HIR0_PREPARE_UNSUPPORTED;
  const size_t modules = frontend_result->written.modules;
  const size_t functions = frontend_result->written.functions;
  const size_t entries = frontend_result->written.entries;
  const size_t host_symbols = input->frontend_input->host_scope->symbol_count;
  size_t identities = 0u;
  if (!add_size(modules, functions, &identities) ||
      !add_size(identities, entries, &identities) ||
      !add_size(identities, host_symbols, &identities) ||
      !count_u32(identities) || !count_u32(functions) || !count_u32(entries) ||
      !count_u32(binding_count) || !count_u32(call_count) ||
      !count_u32(yield_count) ||
      !count_u32(argument_count) || !count_u32(value_count) ||
      !count_u32(interpolation_segment_count) || !count_u32(value_bytes) ||
      !count_u32(logical_count) || !count_u32(while_count) ||
      !count_u32(loop_carrier_count) || !count_u32(switch_count) ||
      !count_u32(switch_edge_count) ||
      !count_u32(switch_capture_count))
    return HIR0_PREPARE_UNSUPPORTED;
  counts->modules = modules;
  counts->identities = identities;
  counts->external_modules = input->frontend_input->external_module_count;
  counts->external_symbols =
      counts->external_modules == 0u
          ? 0u
          : input->frontend_input->external_modules[0].symbol_count;
  if (!count_u32(counts->external_modules) ||
      !count_u32(counts->external_symbols) ||
      !count_u32(frontend_result->written.enums) ||
      !add_size(4u, counts->external_modules == 0u ? 0u : 3u,
                &counts->types) ||
      !add_size(counts->types, frontend_result->written.enums,
                &counts->types) ||
      !count_u32(frontend_result->written.enum_subset_members))
    return HIR0_PREPARE_UNSUPPORTED;
  size_t enum_subset_type_count = 0u;
  size_t enum_subset_member_count = 0u;
  if (!frontend_enum_subset_unique_count(input->frontend_output,
                                         frontend_result,
                                         &enum_subset_type_count))
    return HIR0_PREPARE_INVALID;
  for (size_t type_index = 0u; type_index < frontend_result->written.types;
       type_index += 1u)
    if (input->frontend_output->types[type_index].kind ==
            W_SEED_FRONTEND_TYPE_ENUM_SUBSET) {
    size_t canonical_index = 0u;
    size_t canonical_ordinal = 0u;
    if (!frontend_enum_subset_canonical(
            input->frontend_output, frontend_result, type_index,
            &canonical_index, &canonical_ordinal))
      return HIR0_PREPARE_INVALID;
    (void)canonical_ordinal;
    if (canonical_index == type_index &&
        (!add_size(enum_subset_member_count,
                   input->frontend_output->types[type_index]
                       .subset_member_count,
                   &enum_subset_member_count)))
      return HIR0_PREPARE_UNSUPPORTED;
  }
  if (!add_size(counts->types, enum_subset_type_count, &counts->types) ||
      !add_size(counts->types, counts->external_symbols == 7u ? 1u : 0u,
                &counts->types) ||
      !count_u32(counts->types))
    return HIR0_PREPARE_UNSUPPORTED;
  counts->enums = frontend_result->written.enums;
  counts->enum_cases = frontend_result->written.enum_cases;
  counts->enum_case_parameters =
      frontend_result->written.enum_case_parameters;
  counts->enum_subsets = enum_subset_type_count;
  counts->enum_subset_members = enum_subset_member_count;
  if (!count_u32(counts->enums) || !count_u32(counts->enum_cases) ||
      !count_u32(counts->enum_case_parameters) ||
      !count_u32(counts->enum_subsets) ||
      !count_u32(counts->enum_subset_members))
    return HIR0_PREPARE_UNSUPPORTED;
  counts->functions = functions;
  counts->parameters = frontend_result->written.parameters;
  size_t block_count = functions;
  size_t diamond_count = 0u;
  size_t repeat_count = 0u;
  for (size_t statement = 0u;
       statement < frontend_result->written.statements; statement += 1u)
    if (input->frontend_output->statements[statement].kind ==
        W_SEED_FRONTEND_STMT_REPEAT &&
        !add_size(repeat_count, 1u, &repeat_count))
      return HIR0_PREPARE_UNSUPPORTED;
  if (!add_size(if_count, logical_count, &diamond_count) ||
      !add_size(diamond_count, while_count, &diamond_count) ||
      !add_size(block_count, switch_edge_count, &block_count) ||
      diamond_count > (SIZE_MAX - block_count) / 3u ||
      !add_size(block_count, diamond_count * 3u, &block_count) ||
      !add_size(block_count, repeat_count, &block_count) ||
      !count_u32(block_count))
    return HIR0_PREPARE_UNSUPPORTED;
  size_t terminal_if_count = 0u;
  for (size_t function = 0u; function < functions; function += 1u)
    if (frontend_function_has_terminal_if(input, function) &&
        !add_size(terminal_if_count, 1u, &terminal_if_count))
      return HIR0_PREPARE_UNSUPPORTED;
  if (terminal_if_count > block_count) return HIR0_PREPARE_UNSUPPORTED;
  block_count -= terminal_if_count;
  counts->blocks = block_count;
  counts->switch_edges = switch_edge_count;
  counts->switch_captures = switch_capture_count;
  size_t block_argument_count = 0u;
  if (!add_size(logical_count, merge_count, &block_argument_count) ||
      !add_size(block_argument_count, loop_carrier_count,
                &block_argument_count))
    return HIR0_PREPARE_UNSUPPORTED;
  if (!count_u32(block_argument_count)) return HIR0_PREPARE_UNSUPPORTED;
  counts->block_arguments = block_argument_count;
  if (block_argument_count > SIZE_MAX / 2u ||
      !add_size(block_argument_count, block_argument_count,
                &counts->edge_arguments) ||
      !count_u32(counts->edge_arguments))
    return HIR0_PREPARE_UNSUPPORTED;
  counts->bindings = binding_count;
  if (!add_size(binding_count, call_count, &counts->instructions) ||
      !add_size(counts->instructions, yield_count, &counts->instructions))
    return HIR0_PREPARE_UNSUPPORTED;
  if (!count_u32(counts->instructions)) return HIR0_PREPARE_UNSUPPORTED;
  counts->calls = call_count;
  size_t host_parameters = 0u;
  size_t requirements = 0u;
  for (size_t index = 0u; index < host_symbols; index += 1u) {
    if (!add_size(host_parameters,
                  input->frontend_input->host_scope->symbols[index]
                      .parameter_count,
                  &host_parameters) ||
        !add_size(requirements,
                  input->frontend_input->host_scope->symbols[index]
                      .requirement_count,
                  &requirements))
      return HIR0_PREPARE_UNSUPPORTED;
  }
  counts->host_parameters = host_parameters;
  counts->arguments = argument_count - enum_payload_count -
                      external_enum_constructor_argument_count;
  counts->enum_payloads = enum_payload_count;
  counts->requirements = requirements;
  counts->values = value_count;
  counts->interpolation_segments = interpolation_segment_count;
  counts->terminators = block_count;
  counts->entries = entries;
  counts->text_bytes = text_bytes;
  counts->value_bytes = value_bytes;
  counts->receipt_bytes = HIR0_RECEIPT_BYTES;
  if (text_bytes > W_SEED_HIR0_MAX_TEXT_BYTES ||
      value_bytes > W_SEED_HIR0_MAX_VALUE_BYTES ||
      host_parameters > UINT32_MAX || requirements > UINT32_MAX)
    return HIR0_PREPARE_UNSUPPORTED;
  return HIR0_PREPARE_READY;
}

static bool output_capacity_ok(const w_seed_hir0_output *output,
                               const w_seed_hir0_counts *counts) {
  if (output == NULL || counts == NULL) return false;
#define HIR0_OUTPUT(field, capacity_field)                                     \
  if (counts->field > output->capacity_field ||                               \
      (counts->field != 0u && output->field == NULL)) return false
  HIR0_OUTPUT(modules, module_capacity);
  HIR0_OUTPUT(identities, identity_capacity);
  HIR0_OUTPUT(types, type_capacity);
  HIR0_OUTPUT(enums, enum_capacity);
  HIR0_OUTPUT(enum_cases, enum_case_capacity);
  HIR0_OUTPUT(enum_case_parameters, enum_case_parameter_capacity);
  HIR0_OUTPUT(enum_subset_members, enum_subset_member_capacity);
  HIR0_OUTPUT(functions, function_capacity);
  HIR0_OUTPUT(parameters, parameter_capacity);
  HIR0_OUTPUT(blocks, block_capacity);
  HIR0_OUTPUT(block_arguments, block_argument_capacity);
  HIR0_OUTPUT(edge_arguments, edge_argument_capacity);
  HIR0_OUTPUT(switch_edges, switch_edge_capacity);
  HIR0_OUTPUT(switch_captures, switch_capture_capacity);
  HIR0_OUTPUT(instructions, instruction_capacity);
  HIR0_OUTPUT(bindings, binding_capacity);
  HIR0_OUTPUT(calls, call_capacity);
  HIR0_OUTPUT(host_parameters, host_parameter_capacity);
  HIR0_OUTPUT(arguments, argument_capacity);
  HIR0_OUTPUT(enum_payloads, enum_payload_capacity);
  HIR0_OUTPUT(requirements, requirement_capacity);
  HIR0_OUTPUT(values, value_capacity);
  HIR0_OUTPUT(interpolation_segments, interpolation_segment_capacity);
  HIR0_OUTPUT(terminators, terminator_capacity);
  HIR0_OUTPUT(entries, entry_capacity);
  HIR0_OUTPUT(external_modules, external_module_capacity);
  HIR0_OUTPUT(external_symbols, external_symbol_capacity);
#undef HIR0_OUTPUT
  return counts->text_bytes <= output->text_byte_capacity &&
         (counts->text_bytes == 0u || output->text_bytes != NULL) &&
         counts->value_bytes <= output->value_byte_capacity &&
         (counts->value_bytes == 0u || output->value_bytes != NULL) &&
         counts->receipt_bytes <= output->receipt_capacity &&
         (counts->receipt_bytes == 0u || output->receipt != NULL);
}

static bool pointer_range(const void *pointer, size_t bytes, uintptr_t *start,
                          uintptr_t *end) {
  if (start == NULL || end == NULL) return false;
  if (pointer == NULL || bytes == 0u) {
    *start = 0u;
    *end = 0u;
    return true;
  }
  *start = (uintptr_t)pointer;
  if (bytes > UINTPTR_MAX - *start) return false;
  *end = *start + (uintptr_t)bytes;
  return true;
}

static bool ranges_overlap(const void *left, size_t left_bytes,
                           const void *right, size_t right_bytes) {
  uintptr_t left_start = 0u;
  uintptr_t left_end = 0u;
  uintptr_t right_start = 0u;
  uintptr_t right_end = 0u;
  if (left == NULL || right == NULL || left_bytes == 0u || right_bytes == 0u)
    return false;
  if (!pointer_range(left, left_bytes, &left_start, &left_end) ||
      !pointer_range(right, right_bytes, &right_start, &right_end))
    return true;
  return left_start < right_end && right_start < left_end;
}

typedef struct {
  const void *pointer;
  size_t bytes;
} hir0_memory_range;

static bool range_table_add(hir0_memory_range *ranges, size_t *count,
                            size_t capacity, const void *pointer,
                            size_t elements, size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity ||
      (elements != 0u && pointer == NULL) ||
      (elements > SIZE_MAX / element_size))
    return false;
  const size_t bytes = elements * element_size;
  uintptr_t start = 0u;
  uintptr_t end = 0u;
  if (!pointer_range(pointer, bytes, &start, &end)) return false;
  (void)start;
  (void)end;
  ranges[*count] = (hir0_memory_range){pointer, bytes};
  *count += 1u;
  return true;
}

static bool output_range_table(const w_seed_hir0_output *output,
                               hir0_memory_range *ranges, size_t *count) {
  if (output == NULL || ranges == NULL || count == NULL) return false;
  *count = 0u;
#define HIR0_ADD_OUTPUT(field, capacity_field, type)                          \
  if (!range_table_add(ranges, count, 37u, output->field,                    \
                       output->capacity_field, sizeof(type))) return false
  HIR0_ADD_OUTPUT(modules, module_capacity, w_seed_hir0_module);
  HIR0_ADD_OUTPUT(identities, identity_capacity, w_seed_hir0_identity);
  HIR0_ADD_OUTPUT(types, type_capacity, w_seed_hir0_type);
  HIR0_ADD_OUTPUT(enums, enum_capacity, w_seed_hir0_enum);
  HIR0_ADD_OUTPUT(enum_cases, enum_case_capacity, w_seed_hir0_enum_case);
  HIR0_ADD_OUTPUT(enum_case_parameters, enum_case_parameter_capacity,
                  w_seed_hir0_enum_case_parameter);
  HIR0_ADD_OUTPUT(enum_subset_members, enum_subset_member_capacity,
                  w_seed_hir0_enum_subset_member);
  HIR0_ADD_OUTPUT(functions, function_capacity, w_seed_hir0_function);
  HIR0_ADD_OUTPUT(parameters, parameter_capacity, w_seed_hir0_parameter);
  HIR0_ADD_OUTPUT(blocks, block_capacity, w_seed_hir0_block);
  HIR0_ADD_OUTPUT(block_arguments, block_argument_capacity,
                  w_seed_hir0_block_argument);
  HIR0_ADD_OUTPUT(edge_arguments, edge_argument_capacity,
                  w_seed_hir0_edge_argument);
  HIR0_ADD_OUTPUT(switch_edges, switch_edge_capacity,
                  w_seed_hir0_switch_edge);
  HIR0_ADD_OUTPUT(switch_captures, switch_capture_capacity,
                  w_seed_hir0_switch_capture);
  HIR0_ADD_OUTPUT(instructions, instruction_capacity, w_seed_hir0_instruction);
  HIR0_ADD_OUTPUT(bindings, binding_capacity, w_seed_hir0_binding);
  HIR0_ADD_OUTPUT(calls, call_capacity, w_seed_hir0_call);
  HIR0_ADD_OUTPUT(host_parameters, host_parameter_capacity,
                  w_seed_hir0_host_parameter);
  HIR0_ADD_OUTPUT(arguments, argument_capacity, w_seed_hir0_argument);
  HIR0_ADD_OUTPUT(enum_payloads, enum_payload_capacity,
                  w_seed_hir0_enum_payload);
  HIR0_ADD_OUTPUT(requirements, requirement_capacity, w_seed_hir0_requirement);
  HIR0_ADD_OUTPUT(values, value_capacity, w_seed_hir0_value);
  HIR0_ADD_OUTPUT(interpolation_segments, interpolation_segment_capacity,
                  w_seed_hir0_interpolation_segment);
  HIR0_ADD_OUTPUT(terminators, terminator_capacity, w_seed_hir0_terminator);
  HIR0_ADD_OUTPUT(entries, entry_capacity, w_seed_hir0_entry);
  HIR0_ADD_OUTPUT(external_modules, external_module_capacity,
                  w_seed_hir0_external_module);
  HIR0_ADD_OUTPUT(external_symbols, external_symbol_capacity,
                  w_seed_hir0_external_symbol);
#undef HIR0_ADD_OUTPUT
  if (!range_table_add(ranges, count, 37u, output->text_bytes,
                       output->text_byte_capacity, sizeof(uint8_t)) ||
      !range_table_add(ranges, count, 37u, output->value_bytes,
                       output->value_byte_capacity, sizeof(uint8_t)) ||
      !range_table_add(ranges, count, 37u, output->receipt,
                       output->receipt_capacity, sizeof(uint8_t)))
    return false;
  return true;
}

static bool output_aliases(const w_seed_hir0_output *output,
                           const w_seed_hir0_counts *counts) {
  (void)counts;
  hir0_memory_range ranges[37];
  size_t count = 0u;
  if (!output_range_table(output, ranges, &count)) return true;
  for (size_t first = 0u; first < count; first += 1u)
    for (size_t second = first + 1u; second < count; second += 1u)
      if (ranges_overlap(ranges[first].pointer, ranges[first].bytes,
                         ranges[second].pointer, ranges[second].bytes))
        return true;
  return false;
}

static bool output_overlaps_memory(const hir0_memory_range *outputs,
                                   size_t output_count, const void *pointer,
                                   size_t bytes) {
  if (bytes != 0u && pointer == NULL) return true;
  for (size_t index = 0u; index < output_count; index += 1u)
    if (ranges_overlap(outputs[index].pointer, outputs[index].bytes, pointer,
                       bytes))
      return true;
  return false;
}

static bool output_overlaps_elements(const hir0_memory_range *outputs,
                                     size_t output_count, const void *pointer,
                                     size_t elements, size_t element_size) {
  if (elements > SIZE_MAX / element_size) return true;
  return output_overlaps_memory(outputs, output_count, pointer,
                                elements * element_size);
}

static bool output_overlaps_frontend_text(const hir0_memory_range *outputs,
                                          size_t output_count,
                                          w_seed_frontend_text text) {
  if (!text_valid(text)) return true;
  return output_overlaps_memory(outputs, output_count, text.data, text.length);
}

/* Lowering reads every range below before the first output write. Rejecting
 * all output/input overlap makes that read set stable through the commit. */
static bool output_overlaps_input(const w_seed_hir0_input *input,
                                  const w_seed_hir0_output *output,
                                  const w_seed_hir0_result *result) {
  if (input == NULL || output == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL ||
      result == NULL)
    return true;
  hir0_memory_range outputs[38];
  size_t output_count = 0u;
  if (!output_range_table(output, outputs, &output_count)) return true;
  if (output_count >= 38u) return true;
  if (output_overlaps_memory(outputs, output_count, result, sizeof(*result)))
    return true;
  outputs[output_count++] = (hir0_memory_range){result, sizeof(*result)};
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  const w_seed_frontend_output *frontend = input->frontend_output;
  const w_seed_frontend_result *frontend_result = input->frontend_result;
  if (output_overlaps_memory(outputs, output_count, output, sizeof(*output)) ||
      output_overlaps_memory(outputs, output_count, input, sizeof(*input)) ||
      output_overlaps_memory(outputs, output_count, frontend_input,
                             sizeof(*frontend_input)) ||
      output_overlaps_memory(outputs, output_count, frontend, sizeof(*frontend)) ||
      output_overlaps_memory(outputs, output_count, frontend_result,
                             sizeof(*frontend_result))) {
    return true;
  }
#define HIR0_INPUT_RANGE(field, type)                                         \
  if (output_overlaps_elements(outputs, output_count, frontend->field,        \
                                frontend_result->written.field, sizeof(type))) { \
    return true; \
  }
  HIR0_INPUT_RANGE(modules, w_seed_frontend_module);
  HIR0_INPUT_RANGE(imports, w_seed_frontend_import);
  HIR0_INPUT_RANGE(import_items, w_seed_frontend_import_item);
  HIR0_INPUT_RANGE(structs, w_seed_frontend_struct);
  HIR0_INPUT_RANGE(fields, w_seed_frontend_field);
  HIR0_INPUT_RANGE(type_declarations, w_seed_frontend_type_declaration);
  HIR0_INPUT_RANGE(aliases, w_seed_frontend_alias);
  HIR0_INPUT_RANGE(types, w_seed_frontend_type);
  HIR0_INPUT_RANGE(functions, w_seed_frontend_function);
  HIR0_INPUT_RANGE(parameters, w_seed_frontend_parameter);
  HIR0_INPUT_RANGE(arguments, w_seed_frontend_argument);
  HIR0_INPUT_RANGE(entries, w_seed_frontend_entry);
  HIR0_INPUT_RANGE(statements, w_seed_frontend_statement);
  HIR0_INPUT_RANGE(expressions, w_seed_frontend_expression);
  HIR0_INPUT_RANGE(interpolation_segments,
                   w_seed_frontend_interpolation_segment);
  HIR0_INPUT_RANGE(symbols, w_seed_frontend_symbol);
  HIR0_INPUT_RANGE(facts, w_seed_frontend_fact);
  HIR0_INPUT_RANGE(diagnostics, w_seed_frontend_diagnostic);
  HIR0_INPUT_RANGE(diagnostic_facts, w_seed_frontend_diagnostic_fact);
  HIR0_INPUT_RANGE(diagnostic_items, w_seed_frontend_diagnostic_item);
  HIR0_INPUT_RANGE(diagnostic_labels, w_seed_frontend_diagnostic_label);
  HIR0_INPUT_RANGE(enums, w_seed_frontend_enum);
  HIR0_INPUT_RANGE(enum_cases, w_seed_frontend_enum_case);
  HIR0_INPUT_RANGE(enum_case_parameters, w_seed_frontend_enum_case_parameter);
  HIR0_INPUT_RANGE(const_declarations, w_seed_frontend_const_declaration);
  HIR0_INPUT_RANGE(switch_arms, w_seed_frontend_switch_arm);
  HIR0_INPUT_RANGE(pattern_captures, w_seed_frontend_pattern_capture);
  HIR0_INPUT_RANGE(enum_subset_members, w_seed_frontend_enum_subset_member);
  HIR0_INPUT_RANGE(enum_membership_cases, w_seed_frontend_enum_membership_case);
  HIR0_INPUT_RANGE(generic_parameters, w_seed_frontend_generic_parameter);
  HIR0_INPUT_RANGE(generic_applications, w_seed_frontend_generic_application);
  HIR0_INPUT_RANGE(generic_arguments, w_seed_frontend_generic_argument);
  HIR0_INPUT_RANGE(typed_const_expressions,
                   w_seed_frontend_typed_const_expression);
  HIR0_INPUT_RANGE(const_values, w_seed_frontend_const_value);
  HIR0_INPUT_RANGE(const_elements, w_seed_frontend_const_element);
#undef HIR0_INPUT_RANGE
  for (size_t import = 0u; import < frontend_result->written.imports;
       import += 1u) {
    const w_seed_frontend_import *value = &frontend->imports[import];
    if (output_overlaps_frontend_text(outputs, output_count, value->path) ||
        output_overlaps_frontend_text(outputs, output_count, value->alias))
      return true;
  }
  for (size_t item = 0u; item < frontend_result->written.import_items;
       item += 1u) {
    const w_seed_frontend_import_item *value = &frontend->import_items[item];
    if (output_overlaps_frontend_text(outputs, output_count, value->name) ||
        output_overlaps_frontend_text(outputs, output_count, value->local_name))
      return true;
  }
  if (output_overlaps_memory(outputs, output_count, frontend->receipt,
                              frontend_result->written.receipt_bytes) ||
      output_overlaps_memory(outputs, output_count, frontend->const_bytes,
                              frontend_result->written.const_bytes))
    return true;
  if (output_overlaps_elements(outputs, output_count, frontend_input->documents,
                               frontend_input->document_count,
                               sizeof(w_seed_frontend_document)) ||
      output_overlaps_elements(outputs, output_count,
                               frontend_input->external_modules,
                               frontend_input->external_module_count,
                               sizeof(w_seed_frontend_external_module)) ||
      output_overlaps_elements(outputs, output_count,
                               frontend_input->resolved_imports,
                               frontend_input->resolved_import_count,
                               sizeof(w_seed_frontend_resolved_import)) ||
      output_overlaps_memory(outputs, output_count, frontend_input->host_scope,
                             sizeof(*frontend_input->host_scope)))
    return true;
  for (size_t external_module = 0u;
       external_module < frontend_input->external_module_count;
       external_module += 1u) {
    const w_seed_frontend_external_module *module =
        &frontend_input->external_modules[external_module];
    if (output_overlaps_frontend_text(outputs, output_count,
                                      module->module_id) ||
        output_overlaps_elements(outputs, output_count, module->symbols,
                                 module->symbol_count,
                                 sizeof(*module->symbols)))
      return true;
    for (size_t symbol = 0u; symbol < module->symbol_count; symbol += 1u) {
      const w_seed_frontend_external_symbol *value = &module->symbols[symbol];
      if (output_overlaps_frontend_text(outputs, output_count, value->name) ||
          output_overlaps_frontend_text(outputs, output_count,
                                        value->return_type) ||
          output_overlaps_frontend_text(outputs, output_count,
                                        value->receiver_type) ||
          output_overlaps_elements(outputs, output_count, value->parameters,
                                   value->parameter_count,
                                   sizeof(*value->parameters)))
        return true;
      for (size_t parameter = 0u; parameter < value->parameter_count;
           parameter += 1u) {
        const w_seed_frontend_external_parameter *item =
            &value->parameters[parameter];
        if (output_overlaps_frontend_text(outputs, output_count, item->name) ||
            output_overlaps_frontend_text(outputs, output_count, item->type))
          return true;
      }
    }
  }
  for (size_t document = 0u; document < frontend_input->document_count;
       document += 1u) {
    const w_seed_frontend_document *value = &frontend_input->documents[document];
    if (output_overlaps_memory(outputs, output_count, value->source,
                               sizeof(*value->source)) ||
        output_overlaps_elements(outputs, output_count, value->nodes,
                                 value->node_count, sizeof(*value->nodes)) ||
        (value->source != NULL &&
         output_overlaps_memory(outputs, output_count,
                                value->source->bytes.data,
                                value->source->bytes.length)) ||
        output_overlaps_frontend_text(outputs, output_count,
                                      value->logical_source_id) ||
        output_overlaps_frontend_text(outputs, output_count, value->module_id) ||
        output_overlaps_frontend_text(outputs, output_count,
                                      value->local_module_name))
      return true;
  }
  if (frontend_input->host_scope != NULL) {
    const w_seed_frontend_host_prelude *scope = frontend_input->host_scope;
    if (output_overlaps_frontend_text(outputs, output_count, scope->profile) ||
        output_overlaps_elements(outputs, output_count, scope->symbols,
                                 scope->symbol_count,
                                 sizeof(*scope->symbols)))
      return true;
    for (size_t symbol = 0u; symbol < scope->symbol_count; symbol += 1u) {
      const w_seed_frontend_host_prelude_symbol *value = &scope->symbols[symbol];
      if (output_overlaps_frontend_text(outputs, output_count, value->name) ||
          output_overlaps_frontend_text(outputs, output_count,
                                        value->return_type) ||
          output_overlaps_elements(outputs, output_count, value->parameters,
                                   value->parameter_count,
                                   sizeof(*value->parameters)) ||
          output_overlaps_elements(outputs, output_count, value->requirements,
                                   value->requirement_count,
                                   sizeof(*value->requirements)))
        return true;
      for (size_t parameter = 0u; parameter < value->parameter_count;
           parameter += 1u) {
        if (output_overlaps_frontend_text(
                outputs, output_count, value->parameters[parameter].name) ||
            output_overlaps_frontend_text(
                outputs, output_count, value->parameters[parameter].type))
          return true;
      }
      for (size_t requirement = 0u; requirement < value->requirement_count;
           requirement += 1u)
        if (output_overlaps_frontend_text(
                outputs, output_count, value->requirements[requirement].name))
          return true;
    }
  }
  for (size_t module = 0u; module < frontend_result->written.modules; module += 1u) {
    const w_seed_frontend_module *value = &frontend->modules[module];
    if (output_overlaps_frontend_text(outputs, output_count, value->source_id) ||
        output_overlaps_frontend_text(outputs, output_count, value->module_id) ||
        output_overlaps_frontend_text(outputs, output_count,
                                      value->local_module_name))
      return true;
  }
  for (size_t function = 0u; function < frontend_result->written.functions; function += 1u)
    if (output_overlaps_frontend_text(outputs, output_count,
                                      frontend->functions[function].name))
      return true;
  for (size_t enum_index = 0u; enum_index < frontend_result->written.enums;
       enum_index += 1u) {
    const w_seed_frontend_enum *decl = &frontend->enums[enum_index];
    if (output_overlaps_frontend_text(outputs, output_count, decl->name))
      return true;
    for (size_t ordinal = 0u; ordinal < decl->case_count; ordinal += 1u)
      if (output_overlaps_frontend_text(
              outputs, output_count,
              frontend->enum_cases[(size_t)decl->first_case + ordinal].name))
        return true;
  }
  for (size_t payload = 0u;
       payload < frontend_result->written.enum_case_parameters;
       payload += 1u)
    if (output_overlaps_frontend_text(
            outputs, output_count,
            frontend->enum_case_parameters[payload].label))
      return true;
  for (size_t capture = 0u;
       capture < frontend_result->written.pattern_captures; capture += 1u)
    if (output_overlaps_frontend_text(
            outputs, output_count, frontend->pattern_captures[capture].name))
      return true;
  for (size_t parameter = 0u; parameter < frontend_result->written.parameters;
       parameter += 1u)
    if (output_overlaps_frontend_text(outputs, output_count,
                                      frontend->parameters[parameter].name) ||
        output_overlaps_frontend_text(outputs, output_count,
                                      frontend->parameters[parameter].label))
      return true;
  for (size_t entry = 0u; entry < frontend_result->written.entries; entry += 1u)
    if (output_overlaps_frontend_text(outputs, output_count,
                                      frontend->entries[entry].target))
      return true;
  for (size_t argument = 0u; argument < frontend_result->written.arguments; argument += 1u)
    if (output_overlaps_frontend_text(outputs, output_count,
                                      frontend->arguments[argument].label))
      return true;
  for (size_t statement = 0u;
       statement < frontend_result->written.statements; statement += 1u)
    if (output_overlaps_frontend_text(
            outputs, output_count, frontend->statements[statement].binding_name))
      return true;
  for (size_t expression = 0u; expression < frontend_result->written.expressions;
       expression += 1u) {
    const w_seed_frontend_expression *value = &frontend->expressions[expression];
    if (output_overlaps_frontend_text(outputs, output_count, value->spelling) ||
        output_overlaps_frontend_text(outputs, output_count,
                                      value->operator_text) ||
        output_overlaps_frontend_text(outputs, output_count,
                                      value->member_name))
      return true;
  }
  return false;
}

static bool program_range_table(const w_seed_hir0_program *program,
                                hir0_memory_range *ranges, size_t *count) {
  if (program == NULL || ranges == NULL || count == NULL) return false;
  *count = 0u;
#define HIR0_ADD_PROGRAM(field, capacity_field, type)                         \
  if (!range_table_add(ranges, count, 37u, program->field,                   \
                       program->capacity_field, sizeof(type))) return false
  HIR0_ADD_PROGRAM(modules, module_capacity, w_seed_hir0_module);
  HIR0_ADD_PROGRAM(identities, identity_capacity, w_seed_hir0_identity);
  HIR0_ADD_PROGRAM(types, type_capacity, w_seed_hir0_type);
  HIR0_ADD_PROGRAM(enums, enum_capacity, w_seed_hir0_enum);
  HIR0_ADD_PROGRAM(enum_cases, enum_case_capacity, w_seed_hir0_enum_case);
  HIR0_ADD_PROGRAM(enum_case_parameters, enum_case_parameter_capacity,
                   w_seed_hir0_enum_case_parameter);
  HIR0_ADD_PROGRAM(enum_subset_members, enum_subset_member_capacity,
                   w_seed_hir0_enum_subset_member);
  HIR0_ADD_PROGRAM(functions, function_capacity, w_seed_hir0_function);
  HIR0_ADD_PROGRAM(parameters, parameter_capacity, w_seed_hir0_parameter);
  HIR0_ADD_PROGRAM(blocks, block_capacity, w_seed_hir0_block);
  HIR0_ADD_PROGRAM(block_arguments, block_argument_capacity,
                   w_seed_hir0_block_argument);
  HIR0_ADD_PROGRAM(edge_arguments, edge_argument_capacity,
                   w_seed_hir0_edge_argument);
  HIR0_ADD_PROGRAM(switch_edges, switch_edge_capacity,
                   w_seed_hir0_switch_edge);
  HIR0_ADD_PROGRAM(switch_captures, switch_capture_capacity,
                   w_seed_hir0_switch_capture);
  HIR0_ADD_PROGRAM(instructions, instruction_capacity, w_seed_hir0_instruction);
  HIR0_ADD_PROGRAM(bindings, binding_capacity, w_seed_hir0_binding);
  HIR0_ADD_PROGRAM(calls, call_capacity, w_seed_hir0_call);
  HIR0_ADD_PROGRAM(host_parameters, host_parameter_capacity,
                   w_seed_hir0_host_parameter);
  HIR0_ADD_PROGRAM(arguments, argument_capacity, w_seed_hir0_argument);
  HIR0_ADD_PROGRAM(enum_payloads, enum_payload_capacity,
                   w_seed_hir0_enum_payload);
  HIR0_ADD_PROGRAM(requirements, requirement_capacity, w_seed_hir0_requirement);
  HIR0_ADD_PROGRAM(values, value_capacity, w_seed_hir0_value);
  HIR0_ADD_PROGRAM(interpolation_segments, interpolation_segment_capacity,
                   w_seed_hir0_interpolation_segment);
  HIR0_ADD_PROGRAM(terminators, terminator_capacity, w_seed_hir0_terminator);
  HIR0_ADD_PROGRAM(entries, entry_capacity, w_seed_hir0_entry);
  HIR0_ADD_PROGRAM(external_modules, external_module_capacity,
                   w_seed_hir0_external_module);
  HIR0_ADD_PROGRAM(external_symbols, external_symbol_capacity,
                   w_seed_hir0_external_symbol);
#undef HIR0_ADD_PROGRAM
  if (!range_table_add(ranges, count, 37u, program->text_bytes,
                       program->text_byte_capacity, sizeof(uint8_t)) ||
      !range_table_add(ranges, count, 37u, program->value_bytes,
                       program->value_byte_capacity, sizeof(uint8_t)) ||
      !range_table_add(ranges, count, 37u, program->receipt,
                       program->receipt_capacity, sizeof(uint8_t)))
    return false;
  return true;
}

/* collect() proves text_size_for_input() and all individual source slices.
 * These helpers therefore have no failure path after the output commit point. */
static void append_text_unchecked(const w_seed_frontend_text source,
                                  uint8_t *buffer, size_t *offset,
                                  w_seed_hir0_text *out) {
  out->offset = (uint32_t)*offset;
  out->count = (uint32_t)source.length;
  if (source.length != 0u)
    (void)memcpy(buffer + *offset, source.data, source.length);
  *offset += source.length;
}

static void append_bytes_unchecked(const uint8_t *source, size_t count,
                                   uint8_t *buffer, size_t *offset,
                                   uint32_t *out_offset, uint32_t *out_count) {
  *out_offset = (uint32_t)*offset;
  *out_count = (uint32_t)count;
  if (count != 0u)
    (void)memcpy(buffer + *offset, source, count);
  *offset += count;
}

static uint32_t hir_type_from_frontend(const w_seed_frontend_output *output,
                                       const w_seed_frontend_result *result,
                                       uint32_t frontend_type) {
  if (frontend_type == W_SEED_FRONTEND_NONE || output == NULL || result == NULL ||
      (size_t)frontend_type >= result->written.types || output->types == NULL)
    return W_SEED_HIR0_NONE;
  const w_seed_frontend_type *type = &output->types[frontend_type];
  if (type->kind == W_SEED_FRONTEND_TYPE_TASK &&
      type->task_result_type != W_SEED_FRONTEND_NONE &&
      type->task_result_type != frontend_type &&
      (size_t)type->task_result_type < result->written.types) {
    return hir_type_from_frontend(output, result, type->task_result_type);
  }
  if (type->kind == W_SEED_FRONTEND_TYPE_UNIT) return 0u;
  if (type->kind == W_SEED_FRONTEND_TYPE_STRING) return 1u;
  if (type->kind == W_SEED_FRONTEND_TYPE_INTEGER && type->is_signed &&
      type->bit_width == 64u)
    return 2u;
  if (type->kind == W_SEED_FRONTEND_TYPE_BOOL) return 3u;
  if (type->kind == W_SEED_FRONTEND_TYPE_NOMINAL &&
      type->external_module_index == 0u &&
      type->external_symbol_index < 3u)
    return 4u + type->external_symbol_index;
  size_t external_types = 0u;
  for (size_t index = 0u; index < result->written.types; index += 1u)
    if (output->types[index].kind == W_SEED_FRONTEND_TYPE_NOMINAL &&
        output->types[index].external_module_index !=
            W_SEED_FRONTEND_NONE &&
        output->types[index].external_symbol_index !=
            W_SEED_FRONTEND_NONE) {
      external_types = 3u;
      break;
  }
  const size_t local_enum_type_base = 4u + external_types;
  size_t enum_subset_type_count = 0u;
  if (!frontend_enum_subset_unique_count(output, result,
                                         &enum_subset_type_count))
    return W_SEED_HIR0_NONE;
  if (type->kind == W_SEED_FRONTEND_TYPE_ENUM &&
      type->enum_base_index != W_SEED_FRONTEND_NONE) {
    return (uint32_t)(local_enum_type_base + type->enum_base_index);
  }
  if (type->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET &&
      type->enum_base_index != W_SEED_FRONTEND_NONE) {
    size_t canonical_index = 0u;
    size_t subset_ordinal = 0u;
    if (frontend_enum_subset_canonical(
            output, result, frontend_type, &canonical_index, &subset_ordinal)) {
      (void)canonical_index;
      return (uint32_t)(local_enum_type_base + result->written.enums +
                        subset_ordinal);
    }
  }
  if (frontend_type_is_usize(type)) {
    if (external_types == 3u)
      return (uint32_t)(local_enum_type_base + result->written.enums +
                        enum_subset_type_count);
  }
  return W_SEED_HIR0_NONE;
}

static uint32_t hir_host_identity_index(const w_seed_hir0_counts *counts,
                                        size_t host_index) {
  return (uint32_t)(counts->modules + counts->functions + counts->entries +
                    host_index);
}

static bool frontend_binding_event(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t index,
    bool *creates_binding, size_t *binding_count,
    uint32_t *target_statement) {
  if (output == NULL || result == NULL || creates_binding == NULL ||
      binding_count == NULL || target_statement == NULL ||
      index >= result->written.statements)
    return false;
  const w_seed_frontend_statement *statement = &output->statements[index];
  const size_t event_function = statement->owner_function;
  *creates_binding = statement->kind == W_SEED_FRONTEND_STMT_LET ||
                     statement->kind == W_SEED_FRONTEND_STMT_VAR;
  *binding_count = *creates_binding ? 1u : 0u;
  *target_statement = *creates_binding ? (uint32_t)index
                                       : W_SEED_FRONTEND_NONE;
  hir0_branch_assignment_merge merge;
  if (statement->kind == W_SEED_FRONTEND_STMT_IF &&
      frontend_branch_assignment_merge(output, result, event_function,
                                       (uint32_t)index, &merge)) {
    *creates_binding = true;
    *binding_count = merge.count;
    *target_statement = W_SEED_FRONTEND_NONE;
    return true;
  }
  if (statement->kind != W_SEED_FRONTEND_STMT_EXPRESSION ||
      statement->expression_index == W_SEED_FRONTEND_NONE ||
      (size_t)statement->expression_index >= result->written.expressions)
    return true;
  const w_seed_frontend_expression *expression =
      &output->expressions[statement->expression_index];
  if (expression->kind != W_SEED_FRONTEND_EXPR_ASSIGNMENT) return true;
  uint32_t merge_owner = W_SEED_FRONTEND_NONE;
  if (frontend_assignment_merge_owner(output, result, event_function,
                                      (uint32_t)index, &merge_owner)) {
    *creates_binding = false;
    *binding_count = 0u;
    *target_statement = W_SEED_FRONTEND_NONE;
    return true;
  }
  if (expression->left == W_SEED_FRONTEND_NONE ||
      (size_t)expression->left >= result->written.expressions)
    return false;
  *creates_binding = true;
  *binding_count = 1u;
  *target_statement =
      output->expressions[expression->left].resolved_binding_statement;
  return true;
}

static bool binding_index_for_statement(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t function,
    size_t use_statement, uint32_t target_statement, uint32_t *out) {
  if (output == NULL || result == NULL || out == NULL ||
      (size_t)target_statement >= result->written.statements ||
      (size_t)target_statement >= use_statement ||
      output->statements[target_statement].owner_function != function)
    return false;
  size_t binding = 0u;
  uint32_t latest = W_SEED_HIR0_NONE;
  uint32_t use_merge_owner = W_SEED_FRONTEND_NONE;
  (void)frontend_assignment_merge_owner(
      output, result, function, (uint32_t)use_statement, &use_merge_owner);
  for (size_t index = 0u; index < result->written.statements; index += 1u) {
    bool creates_binding = false;
    size_t event_binding_count = 0u;
    uint32_t assignment_target = W_SEED_FRONTEND_NONE;
    if (!frontend_binding_event(output, result, index, &creates_binding,
                                &event_binding_count, &assignment_target))
      return false;
    if (!creates_binding) continue;
    if (!count_u32(binding)) return false;
    if (index < use_statement && index != use_merge_owner) {
      hir0_branch_assignment_merge merge;
      if (output->statements[index].kind == W_SEED_FRONTEND_STMT_IF &&
          frontend_branch_assignment_merge(output, result, function,
                                           (uint32_t)index, &merge)) {
        for (size_t entry = 0u; entry < merge.count; entry += 1u)
          if (merge.entries[entry].declaration_statement == target_statement)
            latest = (uint32_t)(binding + entry);
      } else if (index == (size_t)target_statement ||
                 assignment_target == target_statement) {
        latest = (uint32_t)binding;
      }
    }
    if (!add_size(binding, event_binding_count, &binding)) return false;
  }
  if (latest == W_SEED_HIR0_NONE) return false;
  *out = latest;
  return true;
}

static bool root_binding_index_for_statement(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t function,
    uint32_t target_statement, uint32_t *out) {
  if (output == NULL || result == NULL || out == NULL ||
      (size_t)target_statement >= result->written.statements)
    return false;
  size_t binding = 0u;
  for (size_t index = 0u; index < result->written.statements; index += 1u) {
    const w_seed_frontend_statement *statement = &output->statements[index];
    const bool declaration = statement->kind == W_SEED_FRONTEND_STMT_LET ||
                             statement->kind == W_SEED_FRONTEND_STMT_VAR;
    bool creates_binding = false;
    size_t event_binding_count = 0u;
    uint32_t event_target = W_SEED_FRONTEND_NONE;
    if (!frontend_binding_event(output, result, index, &creates_binding,
                                &event_binding_count, &event_target))
      return false;
    (void)event_target;
    if (!creates_binding) continue;
    if (index == (size_t)target_statement) {
      if (!declaration || statement->owner_function != function ||
          !count_u32(binding))
        return false;
      *out = (uint32_t)binding;
      return true;
    }
    if (!add_size(binding, event_binding_count, &binding)) return false;
  }
  return false;
}

static w_seed_hir0_binary_operator hir_binary_operator(
    w_seed_frontend_text text) {
  if (text_is(text, "+")) return W_SEED_HIR0_BINARY_ADD;
  if (text_is(text, "-")) return W_SEED_HIR0_BINARY_SUBTRACT;
  if (text_is(text, "*")) return W_SEED_HIR0_BINARY_MULTIPLY;
  if (text_is(text, "/")) return W_SEED_HIR0_BINARY_DIVIDE;
  if (text_is(text, "%")) return W_SEED_HIR0_BINARY_REMAINDER;
  if (text_is(text, "==")) return W_SEED_HIR0_BINARY_EQUAL;
  if (text_is(text, "!=")) return W_SEED_HIR0_BINARY_NOT_EQUAL;
  if (text_is(text, "<")) return W_SEED_HIR0_BINARY_LESS;
  if (text_is(text, "<=")) return W_SEED_HIR0_BINARY_LESS_EQUAL;
  if (text_is(text, ">")) return W_SEED_HIR0_BINARY_GREATER;
  if (text_is(text, ">=")) return W_SEED_HIR0_BINARY_GREATER_EQUAL;
  return (w_seed_hir0_binary_operator)UINT32_MAX;
}

static bool source_digest(const w_seed_frontend_document *document,
                          uint8_t digest[32]) {
  if (document == NULL || document->source == NULL || digest == NULL) return false;
  const w_seed_byte_view bytes = w_seed_source_bytes(document->source);
  if (bytes.length != 0u && bytes.data == NULL) return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, bytes.data, bytes.length);
  w_seed_sha256_final(&state, digest);
  return true;
}

static void zero_bytes(void *pointer, size_t bytes) {
  if (bytes != 0u) (void)memset(pointer, 0, bytes);
}

typedef struct {
  const w_seed_hir0_counts *counts;
  w_seed_hir0_output *output;
  const w_seed_frontend_output *frontend;
  const w_seed_frontend_result *frontend_result;
  const w_seed_frontend_input *frontend_input;
  size_t function;
  size_t *text_offset;
  size_t *value_offset;
  size_t *instruction_offset;
  size_t *binding_offset;
  size_t *call_offset;
  size_t *argument_offset;
  size_t *enum_payload_index;
  size_t *value_index;
  size_t *interpolation_segment_index;
  size_t *block_argument_index;
  size_t *edge_argument_index;
  size_t *switch_edge_index;
  size_t *switch_capture_index;
  size_t statement_index;
  bool loop_active;
  bool loop_post_test;
  uint32_t loop_root_statements[HIR0_MAX_BRANCH_ASSIGNMENTS];
  size_t loop_root_count;
  size_t loop_header_block;
  size_t loop_body_block;
  size_t loop_exit_block;
} hir0_emit_context;

/* The frontend walk has already proved the loop body shape.  Re-derive the
 * carried-root list in each emission pass so layout, values, and terminators
 * consume exactly the same source-ordered carriers without adding frontend
 * state to the public HIR schema. */
static bool hir0_load_loop_roots(hir0_emit_context *context,
                                 uint32_t first_statement) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL ||
      first_statement == W_SEED_FRONTEND_NONE)
    return false;
  context->loop_root_count = 0u;
  uint32_t cursor = first_statement;
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < context->frontend_result->written.statements) {
    if (context->loop_root_count >= HIR0_MAX_BRANCH_ASSIGNMENTS)
      return false;
    const w_seed_frontend_statement *statement =
        &context->frontend->statements[cursor];
    if (statement->kind != W_SEED_FRONTEND_STMT_EXPRESSION ||
        statement->expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)statement->expression_index >=
            context->frontend_result->written.expressions)
      return false;
    const w_seed_frontend_expression *assignment =
        &context->frontend->expressions[statement->expression_index];
    if (assignment->kind != W_SEED_FRONTEND_EXPR_ASSIGNMENT ||
        assignment->left == W_SEED_FRONTEND_NONE ||
        (size_t)assignment->left >=
            context->frontend_result->written.expressions)
      return false;
    const w_seed_frontend_expression *target =
        &context->frontend->expressions[assignment->left];
    if (target->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
        target->resolved_binding_statement == W_SEED_FRONTEND_NONE)
      return false;
    for (size_t prior = 0u; prior < context->loop_root_count; prior += 1u)
      if (context->loop_root_statements[prior] ==
          target->resolved_binding_statement)
        return false;
    context->loop_root_statements[context->loop_root_count] =
        target->resolved_binding_statement;
    context->loop_root_count += 1u;
    cursor = statement->next_sibling;
    guard += 1u;
  }
  if (cursor != W_SEED_FRONTEND_NONE || context->loop_root_count == 0u)
    return false;
  /* Carrier ordinals are canonical root-declaration ordinals.  The body is
   * still emitted in statement order below; this sort only fixes the state
   * tuple/edge identity so independent assignment reordering cannot alter
   * the HIR carrier ABI. */
  for (size_t index = 1u; index < context->loop_root_count; index += 1u) {
    const uint32_t key = context->loop_root_statements[index];
    const size_t key_order = frontend_function_root_ordinal(
        context->frontend, context->frontend_result, context->function, key);
    if (key_order == SIZE_MAX) return false;
    size_t cursor_index = index;
    while (cursor_index != 0u) {
      const size_t previous_order = frontend_function_root_ordinal(
          context->frontend, context->frontend_result, context->function,
          context->loop_root_statements[cursor_index - 1u]);
      if (previous_order <= key_order) break;
      context->loop_root_statements[cursor_index] =
          context->loop_root_statements[cursor_index - 1u];
      cursor_index -= 1u;
    }
    context->loop_root_statements[cursor_index] = key;
  }
  return true;
}

static size_t hir0_loop_root_ordinal(const hir0_emit_context *context,
                                     uint32_t root_statement) {
  if (context == NULL || root_statement == W_SEED_FRONTEND_NONE) return SIZE_MAX;
  for (size_t ordinal = 0u; ordinal < context->loop_root_count; ordinal += 1u)
    if (context->loop_root_statements[ordinal] == root_statement) return ordinal;
  return SIZE_MAX;
}

static bool hir0_loop_assignment_for_root(
    const hir0_emit_context *context, uint32_t first_statement,
    uint32_t root_statement, uint32_t *assignment_expression) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL ||
      first_statement == W_SEED_FRONTEND_NONE ||
      root_statement == W_SEED_FRONTEND_NONE || assignment_expression == NULL)
    return false;
  uint32_t cursor = first_statement;
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < context->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        &context->frontend->statements[cursor];
    if (statement->kind != W_SEED_FRONTEND_STMT_EXPRESSION ||
        statement->expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)statement->expression_index >=
            context->frontend_result->written.expressions)
      return false;
    const w_seed_frontend_expression *assignment =
        &context->frontend->expressions[statement->expression_index];
    if (assignment->kind != W_SEED_FRONTEND_EXPR_ASSIGNMENT ||
        assignment->left == W_SEED_FRONTEND_NONE ||
        (size_t)assignment->left >=
            context->frontend_result->written.expressions)
      return false;
    const w_seed_frontend_expression *target =
        &context->frontend->expressions[assignment->left];
    if (target->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
        target->resolved_binding_statement == W_SEED_FRONTEND_NONE)
      return false;
    if (target->resolved_binding_statement == root_statement) {
      *assignment_expression = statement->expression_index;
      return true;
    }
    cursor = statement->next_sibling;
    guard += 1u;
  }
  return false;
}

static bool hir0_usize_count_comparison_source_ok(
    const hir0_emit_context *context,
    const w_seed_frontend_expression *source) {
  if (context == NULL || source == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend_input == NULL ||
      context->function >= context->frontend_result->written.functions)
    return false;
  const w_seed_frontend_function *function =
      &context->frontend->functions[context->function];
  if (function->module_index >= context->frontend_result->written.modules)
    return false;
  const w_seed_frontend_module *module =
      &context->frontend->modules[function->module_index];
  if (module->document_index >= context->frontend_input->document_count)
    return false;
  const w_seed_hir0_input input = {context->frontend_input, context->frontend,
                                   context->frontend_result};
  return frontend_usize_count_comparison_ok(
      &input, function->module_index, context->function, module->document_index,
      source);
}

static size_t hir0_expression_logical_count(const hir0_emit_context *context,
                                            uint32_t expression,
                                            size_t depth) {
  if (context == NULL || expression == W_SEED_FRONTEND_NONE ||
      depth > W_SEED_HIR0_MAX_NESTING ||
      (size_t)expression >= context->frontend_result->written.expressions)
    return 0u;
  const w_seed_frontend_expression *value =
      &context->frontend->expressions[expression];
  if (value->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH)
    return hir0_expression_logical_count(
        context, value->task_call_expression, depth + 1u);
  if (value->kind == W_SEED_FRONTEND_EXPR_AWAIT)
    return hir0_expression_logical_count(context, value->left, depth + 1u);
  if (value->kind == W_SEED_FRONTEND_EXPR_IF) {
    size_t total = hir0_expression_logical_count(
        context, value->left, depth + 1u);
    const size_t then_total = hir0_expression_logical_count(
        context, value->right, depth + 1u);
    const size_t else_total = hir0_expression_logical_count(
        context, value->else_expression, depth + 1u);
    if (total > SIZE_MAX - then_total ||
        total + then_total > SIZE_MAX - else_total)
      return 0u;
    total += then_total + else_total;
    if (total == SIZE_MAX) return 0u;
    return total + 1u;
  }
  if (value->kind == W_SEED_FRONTEND_EXPR_UNARY ||
      value->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS)
    return hir0_expression_logical_count(context, value->left, depth + 1u);
  if (value->kind == W_SEED_FRONTEND_EXPR_ASSIGNMENT)
    return hir0_expression_logical_count(context, value->right, depth + 1u);
  if (value->kind == W_SEED_FRONTEND_EXPR_BINARY) {
    size_t total = hir0_expression_logical_count(context, value->left,
                                                  depth + 1u);
    const size_t right = hir0_expression_logical_count(context, value->right,
                                                        depth + 1u);
    if (total > SIZE_MAX - right) return 0u;
    total += right;
    if (hir_logical_operator(value->operator_text) !=
        W_SEED_HIR0_LOGICAL_NONE) {
      if (total == SIZE_MAX) return 0u;
      total += 1u;
    }
    return total;
  }
  if (value->kind == W_SEED_FRONTEND_EXPR_CALL) {
    size_t total = 0u;
    for (size_t ordinal = 0u; ordinal < value->argument_count; ordinal += 1u) {
      const w_seed_frontend_argument *argument =
          &context->frontend->arguments[(size_t)value->first_argument + ordinal];
      const size_t nested = hir0_expression_logical_count(
          context, argument->expression_index, depth + 1u);
      if (total > SIZE_MAX - nested) return 0u;
      total += nested;
    }
    return total;
  }
  if (value->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING) {
    size_t total = 0u;
    for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_frontend_interpolation_segment *segment =
          &context->frontend->interpolation_segments[
              (size_t)value->first_interpolation_segment + ordinal];
      if (segment->kind != W_SEED_FRONTEND_INTERPOLATION_EXPRESSION) continue;
      const size_t nested = hir0_expression_logical_count(
          context, segment->expression_index, depth + 1u);
      if (total > SIZE_MAX - nested) return 0u;
      total += nested;
    }
    return total;
  }
  return 0u;
}

/* collect() has already proved this graph. Keep the walk bounded by the
 * statement range and by the same HIR nesting limit used by emission. */
static size_t hir0_region_if_count(const hir0_emit_context *context,
                                   uint32_t first_statement, size_t depth) {
  if (context == NULL || first_statement == W_SEED_FRONTEND_NONE) return 0u;
  size_t total = 0u;
  size_t guard = 0u;
  uint32_t cursor = first_statement;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < context->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        &context->frontend->statements[cursor];
    if (statement->kind == W_SEED_FRONTEND_STMT_IF ||
        statement->kind == W_SEED_FRONTEND_STMT_WHILE ||
        statement->kind == W_SEED_FRONTEND_STMT_REPEAT) {
      total += 1u;
      if (statement->kind == W_SEED_FRONTEND_STMT_IF) {
        total += hir0_region_if_count(context, statement->first_child,
                                      depth + 1u);
        total += hir0_region_if_count(context, statement->else_child,
                                      depth + 1u);
      }
    }
    cursor = statement->next_sibling;
    guard += 1u;
  }
  return total;
}

static size_t hir0_region_repeat_count(const hir0_emit_context *context,
                                       uint32_t first_statement,
                                       size_t depth) {
  if (context == NULL || first_statement == W_SEED_FRONTEND_NONE ||
      depth > W_SEED_HIR0_MAX_NESTING)
    return 0u;
  size_t total = 0u;
  size_t guard = 0u;
  uint32_t cursor = first_statement;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < context->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        &context->frontend->statements[cursor];
    if (statement->kind == W_SEED_FRONTEND_STMT_REPEAT) {
      if (total == SIZE_MAX) return 0u;
      total += 1u;
    } else if (statement->kind == W_SEED_FRONTEND_STMT_IF) {
      const size_t then_total = hir0_region_repeat_count(
          context, statement->first_child, depth + 1u);
      const size_t else_total = hir0_region_repeat_count(
          context, statement->else_child, depth + 1u);
      if (total > SIZE_MAX - then_total ||
          total + then_total > SIZE_MAX - else_total)
        return 0u;
      total += then_total + else_total;
    }
    cursor = statement->next_sibling;
    guard += 1u;
  }
  return total;
}

static size_t hir0_region_logical_count(const hir0_emit_context *context,
                                        uint32_t first_statement,
                                        size_t depth) {
  if (context == NULL || first_statement == W_SEED_FRONTEND_NONE ||
      depth > W_SEED_HIR0_MAX_NESTING)
    return 0u;
  size_t total = 0u;
  size_t guard = 0u;
  uint32_t cursor = first_statement;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < context->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        &context->frontend->statements[cursor];
    const size_t expression = hir0_expression_logical_count(
        context, statement->expression_index, 0u);
    if (total > SIZE_MAX - expression) return 0u;
    total += expression;
    if (statement->kind == W_SEED_FRONTEND_STMT_IF) {
      const size_t then_total = hir0_region_logical_count(
          context, statement->first_child, depth + 1u);
      const size_t else_total = hir0_region_logical_count(
          context, statement->else_child, depth + 1u);
      if (total > SIZE_MAX - then_total ||
          total + then_total > SIZE_MAX - else_total)
        return 0u;
      total += then_total + else_total;
    }
    cursor = statement->next_sibling;
    guard += 1u;
  }
  return total;
}

static size_t hir0_region_switch_extra_count(
    const hir0_emit_context *context, uint32_t first_statement,
    size_t depth) {
  if (context == NULL || first_statement == W_SEED_FRONTEND_NONE ||
      depth > W_SEED_HIR0_MAX_NESTING)
    return 0u;
  size_t total = 0u;
  size_t guard = 0u;
  uint32_t cursor = first_statement;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < context->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        &context->frontend->statements[cursor];
    if (statement->kind == W_SEED_FRONTEND_STMT_RETURN &&
        statement->expression_index != W_SEED_FRONTEND_NONE &&
        context->frontend->expressions[statement->expression_index].kind ==
            W_SEED_FRONTEND_EXPR_SWITCH) {
      const w_seed_frontend_expression *root =
          &context->frontend->expressions[statement->expression_index];
      if (total > SIZE_MAX - root->switch_arm_count) return 0u;
      total += root->switch_arm_count;
    } else if (statement->kind == W_SEED_FRONTEND_STMT_IF) {
      const size_t then_total = hir0_region_switch_extra_count(
          context, statement->first_child, depth + 1u);
      const size_t else_total = hir0_region_switch_extra_count(
          context, statement->else_child, depth + 1u);
      if (total > SIZE_MAX - then_total ||
          total + then_total > SIZE_MAX - else_total)
        return 0u;
      total += then_total + else_total;
    }
    cursor = statement->next_sibling;
    guard += 1u;
  }
  return total;
}

static bool hir0_chain_ends_in_return(const hir0_emit_context *context,
                                      uint32_t first_statement, size_t depth) {
  if (context == NULL || depth > W_SEED_HIR0_MAX_NESTING)
    return false;
  uint32_t cursor = first_statement;
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < context->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        &context->frontend->statements[cursor];
    if (statement->next_sibling == W_SEED_FRONTEND_NONE)
      return statement->kind == W_SEED_FRONTEND_STMT_RETURN;
    cursor = statement->next_sibling;
    guard += 1u;
  }
  return false;
}

static bool hir0_terminal_if(const hir0_emit_context *context,
                             const w_seed_frontend_statement *statement,
                             size_t depth) {
  return context != NULL && statement != NULL &&
         statement->kind == W_SEED_FRONTEND_STMT_IF &&
         statement->next_sibling == W_SEED_FRONTEND_NONE &&
         statement->first_child != W_SEED_FRONTEND_NONE &&
         statement->else_child != W_SEED_FRONTEND_NONE &&
         hir0_chain_ends_in_return(context, statement->first_child,
                                   depth + 1u) &&
         hir0_chain_ends_in_return(context, statement->else_child,
                                   depth + 1u);
}

static size_t hir0_region_block_count(const hir0_emit_context *context,
                                      uint32_t first_statement, size_t depth) {
  const size_t if_count = hir0_region_if_count(context, first_statement, depth);
  const size_t logical_count =
      hir0_region_logical_count(context, first_statement, depth);
  const size_t switch_extra =
      hir0_region_switch_extra_count(context, first_statement, depth);
  if (if_count > SIZE_MAX - logical_count) return 0u;
  const size_t diamond_count = if_count + logical_count;
  if (diamond_count > (SIZE_MAX - 1u) / 3u) return 0u;
  size_t diamond_blocks = 1u + diamond_count * 3u;
  const size_t repeat_count =
      hir0_region_repeat_count(context, first_statement, depth);
  if (repeat_count > SIZE_MAX - diamond_blocks) return 0u;
  diamond_blocks += repeat_count;
  uint32_t cursor = first_statement;
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < context->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        &context->frontend->statements[cursor];
    if (statement->next_sibling == W_SEED_FRONTEND_NONE) {
      if (hir0_terminal_if(context, statement, depth)) {
        if (diamond_blocks == 0u) return 0u;
        diamond_blocks -= 1u;
      }
      break;
    }
    cursor = statement->next_sibling;
    guard += 1u;
  }
  return switch_extra > SIZE_MAX - diamond_blocks
             ? 0u
             : diamond_blocks + switch_extra;
}

/* M2 emission is deliberately split into layout, instruction-value, and
 * terminator-value passes.  Layout reserves a caller-owned argument range
 * before descending into nested calls, which keeps every call's argument
 * range contiguous even when an argument contains another call. */
static size_t hir0_expression_layout_end_m2(const hir0_emit_context *context,
                                            uint32_t expression,
                                            size_t current_block, size_t depth);

static size_t hir0_expression_block_count_m2(const hir0_emit_context *context,
                                             uint32_t expression,
                                             size_t depth);

static uint32_t hir0_find_call_m2(const hir0_emit_context *context,
                                  uint32_t expression, size_t block_index) {
  if (context == NULL || expression == W_SEED_FRONTEND_NONE)
    return W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < context->counts->calls; index += 1u) {
    const w_seed_hir0_call *call = &context->output->calls[index];
    if (call->source_expression == expression &&
        call->owner_block == block_index)
      return (uint32_t)index;
  }
  return W_SEED_HIR0_NONE;
}

static bool hir0_is_local_enum_constructor(
    const hir0_emit_context *context,
    const w_seed_frontend_expression *expression) {
  if (context == NULL || expression == NULL ||
      expression->kind != W_SEED_FRONTEND_EXPR_CALL ||
      expression->left == W_SEED_FRONTEND_NONE ||
      (size_t)expression->left >=
          context->frontend_result->written.expressions)
    return false;
  const w_seed_frontend_expression *callee =
      &context->frontend->expressions[expression->left];
  return callee->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE &&
         callee->enum_index != W_SEED_FRONTEND_NONE;
}

/* External enum constructors are value expressions, not HIR calls.  The
 * process ExitCode failure case owns its single payload directly, just like
 * a local enum constructor owns its enum payload records. */
static bool hir0_is_external_enum_constructor(
    const hir0_emit_context *context,
    const w_seed_frontend_expression *expression) {
  if (context == NULL || expression == NULL ||
      expression->kind != W_SEED_FRONTEND_EXPR_CALL ||
      expression->left == W_SEED_FRONTEND_NONE ||
      (size_t)expression->left >=
          context->frontend_result->written.expressions)
    return false;
  const w_seed_frontend_expression *callee =
      &context->frontend->expressions[expression->left];
  return callee->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE &&
         callee->enum_index == W_SEED_FRONTEND_NONE &&
         callee->resolved_callee_kind ==
             W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL &&
         callee->resolved_external_module_index == 0u &&
         callee->resolved_external_symbol_index == 5u;
}

static uint32_t hir0_emit_value_m2(
    hir0_emit_context *context, uint32_t expression,
    w_seed_hir0_value_owner_kind owner_kind, uint32_t owner_index,
    uint32_t owner_ordinal, size_t current_block, size_t depth);

static void hir0_emit_edge_argument_m2(hir0_emit_context *context,
                                       size_t terminator_index,
                                       size_t target_block, uint32_t value_index,
                                       uint32_t ordinal) {
  w_seed_hir0_terminator *terminator =
      &context->output->terminators[terminator_index];
  const w_seed_hir0_block *target = &context->output->blocks[target_block];
  const size_t edge_index = *context->edge_argument_index;
  if (terminator->edge_argument_count == 0u)
    terminator->first_edge_argument = (uint32_t)edge_index;
  terminator->edge_argument_count += 1u;
  context->output->edge_arguments[edge_index] = (w_seed_hir0_edge_argument){
      .owner_terminator = (uint32_t)terminator_index,
      .owner_block = terminator->owner_block,
      .ordinal = ordinal,
      .value_index = value_index,
      .type_index = context->output
                        ->block_arguments[(size_t)target->first_block_argument +
                                          ordinal]
                        .type_index,
      .source_span = terminator->source_span};
  *context->edge_argument_index += 1u;
}

static uint32_t hir0_emit_bool_value_m2(hir0_emit_context *context, bool value,
                                        w_seed_span span, size_t block_index,
                                        uint32_t owner_ordinal) {
  const uint32_t result = (uint32_t)*context->value_index;
  context->output->values[*context->value_index] = (w_seed_hir0_value){
      .kind = W_SEED_HIR0_VALUE_CONST_BOOL,
      .owner_kind = W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
      .owner_index = (uint32_t)block_index,
      .owner_ordinal = owner_ordinal,
      .type_index = 3u,
      .binding_index = W_SEED_HIR0_NONE,
      .parameter_index = W_SEED_HIR0_NONE,
      .call_index = W_SEED_HIR0_NONE,
      .left_value = W_SEED_HIR0_NONE,
      .right_value = W_SEED_HIR0_NONE,
      .first_interpolation_segment = W_SEED_HIR0_NONE,
      .interpolation_segment_count = 0u,
      .binary_operator = W_SEED_HIR0_BINARY_ADD,
      .unary_operator = W_SEED_HIR0_UNARY_NOT,
      .block_argument_index = W_SEED_HIR0_NONE,
      .integer_value = 0,
      .bool_value = value,
      .byte_offset = 0u,
      .byte_count = 0u,
      .source_span = span};
  *context->value_index += 1u;
  return result;
}

static const w_seed_frontend_switch_arm *hir0_switch_arm_for_ordinal(
    const hir0_emit_context *context,
    const w_seed_frontend_expression *root, size_t ordinal);

static size_t hir0_emit_expression_values_m2(hir0_emit_context *context,
                                             uint32_t expression,
                                             size_t current_block,
                                             size_t statement_index,
                                             size_t depth);

static size_t hir0_emit_expression_values_m2(hir0_emit_context *context,
                                             uint32_t expression,
                                             size_t current_block,
                                             size_t statement_index,
                                             size_t depth) {
  if (context == NULL || expression == W_SEED_FRONTEND_NONE ||
      depth > W_SEED_HIR0_MAX_NESTING)
    return current_block;
  const w_seed_frontend_expression *source =
      &context->frontend->expressions[expression];
  if (source->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH)
    return hir0_emit_expression_values_m2(
        context, source->task_call_expression, current_block, statement_index,
        depth + 1u);
  if (source->kind == W_SEED_FRONTEND_EXPR_AWAIT)
    return hir0_emit_expression_values_m2(context, source->left, current_block,
                                          statement_index, depth + 1u);
  if (source->kind == W_SEED_FRONTEND_EXPR_IF) {
    const size_t condition_end = hir0_emit_expression_values_m2(
        context, source->left, current_block, statement_index, depth + 1u);
    const size_t then_block = condition_end + 1u;
    const size_t then_count = hir0_expression_block_count_m2(
        context, source->right, depth + 1u);
    const size_t else_block = then_block + then_count;
    const size_t else_count = hir0_expression_block_count_m2(
        context, source->else_expression, depth + 1u);
    (void)hir0_emit_expression_values_m2(
        context, source->right, then_block, statement_index, depth + 1u);
    (void)hir0_emit_expression_values_m2(
        context, source->else_expression, else_block, statement_index,
        depth + 1u);
    return else_block + else_count;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS ||
      source->kind == W_SEED_FRONTEND_EXPR_UNARY)
    return hir0_emit_expression_values_m2(context, source->left, current_block,
                                          statement_index, depth + 1u);
  if (source->kind == W_SEED_FRONTEND_EXPR_BINARY) {
    size_t block = hir0_emit_expression_values_m2(
        context, source->left, current_block, statement_index, depth + 1u);
    const w_seed_hir0_logical_operator logical =
        hir_logical_operator(source->operator_text);
    if (logical == W_SEED_HIR0_LOGICAL_NONE)
      block = hir0_emit_expression_values_m2(
          context, source->right, block, statement_index, depth + 1u);
    else {
      const size_t rhs_count = hir0_expression_block_count_m2(
          context, source->right, depth + 1u);
      const size_t true_block = block + 1u;
      const size_t false_block = logical == W_SEED_HIR0_LOGICAL_AND
                                     ? true_block + rhs_count
                                     : true_block + 1u;
      const size_t rhs_start = logical == W_SEED_HIR0_LOGICAL_AND
                                   ? true_block
                                   : false_block;
      const size_t join_block = logical == W_SEED_HIR0_LOGICAL_AND
                                    ? false_block + 1u
                                    : false_block + rhs_count;
      (void)hir0_emit_expression_values_m2(
          context, source->right, rhs_start, statement_index, depth + 1u);
      block = join_block;
    }
    return block;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING) {
    size_t block = current_block;
    for (size_t ordinal = 0u; ordinal < source->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_frontend_interpolation_segment *segment =
          &context->frontend->interpolation_segments[
              (size_t)source->first_interpolation_segment + ordinal];
      if (segment->kind == W_SEED_FRONTEND_INTERPOLATION_EXPRESSION)
        block = hir0_emit_expression_values_m2(
            context, segment->expression_index, block, statement_index,
            depth + 1u);
    }
    return block;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_CALL) {
    size_t block = current_block;
    for (size_t ordinal = 0u; ordinal < source->argument_count; ordinal += 1u) {
      const uint32_t argument_expression =
          context->frontend->arguments[(size_t)source->first_argument + ordinal]
              .expression_index;
      block = hir0_emit_expression_values_m2(
          context, argument_expression, block, statement_index, depth + 1u);
    }
    if (hir0_is_local_enum_constructor(context, source) ||
        hir0_is_external_enum_constructor(context, source))
      return block;
    const uint32_t call_index = hir0_find_call_m2(context, expression, block);
    if (call_index == W_SEED_HIR0_NONE) return block;
    size_t argument_block = current_block;
    for (size_t ordinal = 0u; ordinal < source->argument_count; ordinal += 1u) {
      const uint32_t argument_expression =
          context->frontend->arguments[(size_t)source->first_argument + ordinal]
              .expression_index;
      argument_block = hir0_expression_layout_end_m2(
          context, argument_expression, argument_block, depth + 1u);
      const uint32_t argument_index =
          context->output->calls[call_index].first_argument + (uint32_t)ordinal;
      context->output->arguments[argument_index].value_index =
          hir0_emit_value_m2(context, argument_expression,
                             W_SEED_HIR0_VALUE_OWNER_ARGUMENT, argument_index,
                             0u, argument_block, depth + 1u);
    }
    return block;
  }
  return current_block;
}

static void hir0_emit_chain_values_m2(hir0_emit_context *context,
                                      uint32_t first_statement,
                                      size_t current_block, size_t depth,
                                      size_t *binding_cursor) {
  uint32_t cursor = first_statement;
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < context->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        &context->frontend->statements[cursor];
    context->statement_index = cursor;
    if (statement->kind == W_SEED_FRONTEND_STMT_LET ||
        statement->kind == W_SEED_FRONTEND_STMT_VAR ||
        (statement->kind == W_SEED_FRONTEND_STMT_EXPRESSION &&
         context->frontend->expressions[statement->expression_index].kind ==
             W_SEED_FRONTEND_EXPR_ASSIGNMENT)) {
      const w_seed_frontend_expression *root =
          &context->frontend->expressions[statement->expression_index];
      const uint32_t initializer_expression =
          root->kind == W_SEED_FRONTEND_EXPR_ASSIGNMENT
              ? root->right
              : statement->expression_index;
      const size_t end = hir0_emit_expression_values_m2(
          context, initializer_expression, current_block, cursor, 0u);
      context->output->bindings[*binding_cursor].initializer_value =
          hir0_emit_value_m2(
              context, initializer_expression,
              W_SEED_HIR0_VALUE_OWNER_BINDING, (uint32_t)*binding_cursor, 0u,
              end, 0u);
      *binding_cursor += 1u;
      current_block = end;
    } else if (statement->kind == W_SEED_FRONTEND_STMT_EXPRESSION) {
      current_block = hir0_emit_expression_values_m2(
          context, statement->expression_index, current_block, cursor, 0u);
    } else if (statement->kind == W_SEED_FRONTEND_STMT_IF) {
      const size_t condition_end = hir0_emit_expression_values_m2(
          context, statement->condition_expression, current_block, cursor, 0u);
      const size_t then_block = condition_end + 1u;
      const size_t then_count = hir0_region_block_count(
          context, statement->first_child, depth + 1u);
      const size_t else_block = then_block + then_count;
      const size_t else_count = hir0_region_block_count(
          context, statement->else_child, depth + 1u);
      const bool terminal = hir0_terminal_if(context, statement, depth);
      const size_t join_block = terminal ? W_SEED_HIR0_NONE
                                         : else_block + else_count;
      hir0_branch_assignment_merge merge;
      if (frontend_branch_assignment_merge(
              context->frontend, context->frontend_result, context->function,
              cursor, &merge)) {
        for (size_t entry = 0u; entry < merge.count; entry += 1u)
          (void)hir0_emit_expression_values_m2(
              context, merge.entries[entry].then_rhs, then_block,
              merge.entries[entry].then_statement, 0u);
        for (size_t entry = 0u; entry < merge.count; entry += 1u)
          (void)hir0_emit_expression_values_m2(
              context, merge.entries[entry].else_rhs, else_block,
              merge.entries[entry].else_statement, 0u);
        const w_seed_hir0_block *join =
            &context->output->blocks[join_block];
        for (size_t entry = 0u; entry < merge.count; entry += 1u) {
          const uint32_t value_index = (uint32_t)*context->value_index;
          const uint32_t binding_index = (uint32_t)*binding_cursor;
          const uint32_t block_argument_index =
              join->first_block_argument + (uint32_t)entry;
          context->output->values[*context->value_index] =
              (w_seed_hir0_value){
                  .kind = W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ,
                  .owner_kind = W_SEED_HIR0_VALUE_OWNER_BINDING,
                  .owner_index = binding_index,
                  .owner_ordinal = 0u,
                  .type_index = context->output->bindings[binding_index]
                                    .type_index,
                  .binding_index = W_SEED_HIR0_NONE,
                  .parameter_index = W_SEED_HIR0_NONE,
                  .call_index = W_SEED_HIR0_NONE,
                  .left_value = W_SEED_HIR0_NONE,
                  .right_value = W_SEED_HIR0_NONE,
                  .first_interpolation_segment = W_SEED_HIR0_NONE,
                  .interpolation_segment_count = 0u,
                  .binary_operator = W_SEED_HIR0_BINARY_ADD,
                  .unary_operator = W_SEED_HIR0_UNARY_NOT,
                  .block_argument_index = block_argument_index,
                  .integer_value = 0,
                  .bool_value = false,
                  .byte_offset = 0u,
                  .byte_count = 0u,
                  .source_span = statement->span};
          context->output->bindings[binding_index].initializer_value =
              value_index;
          *context->value_index += 1u;
          *binding_cursor += 1u;
        }
      } else {
        hir0_emit_chain_values_m2(context, statement->first_child, then_block,
                                  depth + 1u, binding_cursor);
        hir0_emit_chain_values_m2(context, statement->else_child, else_block,
                                  depth + 1u, binding_cursor);
      }
      if (terminal) return;
      current_block = join_block;
    } else if (statement->kind == W_SEED_FRONTEND_STMT_WHILE ||
               statement->kind == W_SEED_FRONTEND_STMT_REPEAT) {
      const bool post_test =
          statement->kind == W_SEED_FRONTEND_STMT_REPEAT;
      const size_t header = current_block + 1u;
      const size_t body = header + 1u;
      const size_t exit = post_test ? body + 2u : body + 1u;
      if (!hir0_load_loop_roots(context, statement->first_child)) return;
      context->loop_active = true;
      context->loop_post_test = post_test;
      context->loop_header_block = header;
      context->loop_body_block = body;
      context->loop_exit_block = exit;
      if (!post_test) {
        (void)hir0_emit_expression_values_m2(
            context, statement->condition_expression, header, cursor, 0u);
        uint32_t assignment_cursor = statement->first_child;
        size_t assignment_guard = 0u;
        while (assignment_cursor != W_SEED_FRONTEND_NONE &&
               assignment_guard <
                   context->frontend_result->written.statements) {
          const w_seed_frontend_statement *body_statement =
              &context->frontend->statements[assignment_cursor];
          const w_seed_frontend_expression *assignment =
              &context->frontend->expressions[body_statement->expression_index];
          context->statement_index = assignment_cursor;
          (void)hir0_emit_expression_values_m2(
              context, assignment->right, body, assignment_cursor, 0u);
          context->output->bindings[*binding_cursor].initializer_value =
              hir0_emit_value_m2(
                  context, assignment->right,
                  W_SEED_HIR0_VALUE_OWNER_BINDING,
                  (uint32_t)*binding_cursor, 0u, body, 0u);
          *binding_cursor += 1u;
          assignment_cursor = body_statement->next_sibling;
          assignment_guard += 1u;
        }
      } else {
        /* A post-test body is the carrier block itself.  Its condition is
         * emitted in the following block and must see the body-written
         * versions, not the declarations in the preheader. */
        uint32_t assignment_cursor = statement->first_child;
        size_t assignment_guard = 0u;
        while (assignment_cursor != W_SEED_FRONTEND_NONE &&
               assignment_guard <
                   context->frontend_result->written.statements) {
          const w_seed_frontend_statement *body_statement =
              &context->frontend->statements[assignment_cursor];
          const w_seed_frontend_expression *assignment =
              &context->frontend->expressions[body_statement->expression_index];
          context->statement_index = assignment_cursor;
          (void)hir0_emit_expression_values_m2(
              context, assignment->right, header, assignment_cursor, 0u);
          context->output->bindings[*binding_cursor].initializer_value =
              hir0_emit_value_m2(
                  context, assignment->right,
                  W_SEED_HIR0_VALUE_OWNER_BINDING,
                  (uint32_t)*binding_cursor, 0u, header, 0u);
          *binding_cursor += 1u;
          assignment_cursor = body_statement->next_sibling;
          assignment_guard += 1u;
        }
        context->statement_index = statement->next_sibling !=
                                           W_SEED_FRONTEND_NONE
                                       ? statement->next_sibling
                                       : (uint32_t)context->frontend_result
                                             ->written.statements;
        (void)hir0_emit_expression_values_m2(
            context, statement->condition_expression, body, context->statement_index,
            0u);
      }
      current_block = exit;
    } else if (statement->kind == W_SEED_FRONTEND_STMT_RETURN) {
      const w_seed_frontend_expression *root =
          &context->frontend->expressions[statement->expression_index];
      if (root->kind == W_SEED_FRONTEND_EXPR_SWITCH) {
        /* The term pass owns the subject and arm result value records.  This
         * pass only descends into arm expressions to materialize nested call
         * argument values, so the dense value cursor is not consumed twice. */
        (void)hir0_emit_expression_values_m2(
            context, root->left, current_block, cursor, 0u);
        for (size_t ordinal = 0u; ordinal < root->switch_arm_count;
             ordinal += 1u) {
          const w_seed_frontend_switch_arm *arm =
              hir0_switch_arm_for_ordinal(context, root, ordinal);
          (void)hir0_emit_expression_values_m2(
              context, arm->result_expression,
              current_block + 1u + ordinal, cursor, 0u);
        }
        return;
      }
      current_block = hir0_emit_expression_values_m2(
          context, statement->expression_index, current_block, cursor, 0u);
      return;
    }
    cursor = statement->next_sibling;
    guard += 1u;
  }
}

static size_t hir0_emit_expression_terms_m2(hir0_emit_context *context,
                                            uint32_t expression,
                                            size_t current_block,
                                            size_t statement_index,
                                            size_t depth);

static size_t hir0_emit_expression_terms_m2(hir0_emit_context *context,
                                            uint32_t expression,
                                            size_t current_block,
                                            size_t statement_index,
                                            size_t depth) {
  if (context == NULL || expression == W_SEED_FRONTEND_NONE ||
      depth > W_SEED_HIR0_MAX_NESTING)
    return current_block;
  const w_seed_frontend_expression *source =
      &context->frontend->expressions[expression];
  if (source->kind == W_SEED_FRONTEND_EXPR_IF) {
    const size_t condition_end = hir0_emit_expression_terms_m2(
        context, source->left, current_block, statement_index, depth + 1u);
    const size_t then_block = condition_end + 1u;
    const size_t then_count = hir0_expression_block_count_m2(
        context, source->right, depth + 1u);
    const size_t else_block = then_block + then_count;
    const size_t else_count = hir0_expression_block_count_m2(
        context, source->else_expression, depth + 1u);
    context->output->terminators[condition_end].value_index =
        hir0_emit_value_m2(
            context, source->left, W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
            (uint32_t)condition_end, 0u, condition_end, depth + 1u);
    const size_t then_end = hir0_emit_expression_terms_m2(
        context, source->right, then_block, statement_index, depth + 1u);
    hir0_emit_edge_argument_m2(
        context, then_end, else_block + else_count,
        hir0_emit_value_m2(context, source->right,
                           W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
                           (uint32_t)then_end, 0u, then_end, depth + 1u), 0u);
    const size_t else_end = hir0_emit_expression_terms_m2(
        context, source->else_expression, else_block, statement_index,
        depth + 1u);
    hir0_emit_edge_argument_m2(
        context, else_end, else_block + else_count,
        hir0_emit_value_m2(context, source->else_expression,
                           W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
                           (uint32_t)else_end, 0u, else_end, depth + 1u), 0u);
    return else_block + else_count;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS ||
      source->kind == W_SEED_FRONTEND_EXPR_UNARY)
    return hir0_emit_expression_terms_m2(context, source->left, current_block,
                                         statement_index, depth + 1u);
  if (source->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING) {
    size_t block = current_block;
    for (size_t ordinal = 0u; ordinal < source->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_frontend_interpolation_segment *segment =
          &context->frontend->interpolation_segments[
              (size_t)source->first_interpolation_segment + ordinal];
      if (segment->kind == W_SEED_FRONTEND_INTERPOLATION_EXPRESSION)
        block = hir0_emit_expression_terms_m2(
            context, segment->expression_index, block, statement_index,
            depth + 1u);
    }
    return block;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_CALL) {
    size_t block = current_block;
    for (size_t ordinal = 0u; ordinal < source->argument_count; ordinal += 1u)
      block = hir0_emit_expression_terms_m2(
          context,
          context->frontend->arguments[(size_t)source->first_argument + ordinal]
              .expression_index,
          block, statement_index, depth + 1u);
    return block;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_BINARY) {
    size_t block = hir0_emit_expression_terms_m2(
        context, source->left, current_block, statement_index, depth + 1u);
    const w_seed_hir0_logical_operator logical =
        hir_logical_operator(source->operator_text);
    if (logical == W_SEED_HIR0_LOGICAL_NONE)
      return hir0_emit_expression_terms_m2(context, source->right, block,
                                           statement_index, depth + 1u);
    const size_t rhs_count = hir0_expression_block_count_m2(
        context, source->right, depth + 1u);
    const size_t true_block = block + 1u;
    const size_t false_block = logical == W_SEED_HIR0_LOGICAL_AND
                                   ? true_block + rhs_count
                                   : true_block + 1u;
    const size_t rhs_start = logical == W_SEED_HIR0_LOGICAL_AND
                                 ? true_block
                                 : false_block;
    const size_t join_block = logical == W_SEED_HIR0_LOGICAL_AND
                                  ? false_block + 1u
                                  : false_block + rhs_count;
    context->output->terminators[block].value_index = hir0_emit_value_m2(
        context, source->left, W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
        (uint32_t)block, 0u, block, depth + 1u);
    size_t rhs_end = rhs_start;
    if (logical == W_SEED_HIR0_LOGICAL_OR) {
      hir0_emit_edge_argument_m2(
          context, true_block, join_block,
          hir0_emit_bool_value_m2(context, true, source->span, true_block, 0u),
          0u);
      rhs_end = hir0_emit_expression_terms_m2(
          context, source->right, rhs_start, statement_index, depth + 1u);
      hir0_emit_edge_argument_m2(
          context, rhs_end, join_block,
          hir0_emit_value_m2(context, source->right,
                             W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
                             (uint32_t)rhs_end, 0u, rhs_end, depth + 1u), 0u);
    } else {
      rhs_end = hir0_emit_expression_terms_m2(
          context, source->right, rhs_start, statement_index, depth + 1u);
      hir0_emit_edge_argument_m2(
          context, rhs_end, join_block,
          hir0_emit_value_m2(context, source->right,
                             W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
                             (uint32_t)rhs_end, 0u, rhs_end, depth + 1u), 0u);
      hir0_emit_edge_argument_m2(
          context, false_block, join_block,
          hir0_emit_bool_value_m2(context, false, source->span, false_block, 0u),
          0u);
    }
    return join_block;
  }
  return current_block;
}

static void hir0_emit_chain_terms_m2(hir0_emit_context *context,
                                     uint32_t first_statement,
                                     size_t current_block, size_t depth) {
  uint32_t cursor = first_statement;
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < context->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        &context->frontend->statements[cursor];
    context->statement_index = cursor;
    if (statement->kind == W_SEED_FRONTEND_STMT_LET ||
        statement->kind == W_SEED_FRONTEND_STMT_VAR ||
        statement->kind == W_SEED_FRONTEND_STMT_EXPRESSION) {
      const w_seed_frontend_expression *root =
          &context->frontend->expressions[statement->expression_index];
      const uint32_t lowered_expression =
          root->kind == W_SEED_FRONTEND_EXPR_ASSIGNMENT
              ? root->right
              : statement->expression_index;
      current_block = hir0_emit_expression_terms_m2(
          context, lowered_expression, current_block, cursor, 0u);
    } else if (statement->kind == W_SEED_FRONTEND_STMT_IF) {
      const size_t condition_end = hir0_emit_expression_terms_m2(
          context, statement->condition_expression, current_block, cursor, 0u);
      context->output->terminators[condition_end].value_index =
          hir0_emit_value_m2(
              context, statement->condition_expression,
              W_SEED_HIR0_VALUE_OWNER_TERMINATOR, (uint32_t)condition_end, 0u,
              condition_end, 0u);
      const size_t then_block = condition_end + 1u;
      const size_t then_count = hir0_region_block_count(
          context, statement->first_child, depth + 1u);
      const size_t else_block = then_block + then_count;
      const size_t else_count = hir0_region_block_count(
          context, statement->else_child, depth + 1u);
      const bool terminal = hir0_terminal_if(context, statement, depth);
      const size_t join_block = terminal ? W_SEED_HIR0_NONE
                                         : else_block + else_count;
      hir0_branch_assignment_merge merge;
      if (frontend_branch_assignment_merge(
              context->frontend, context->frontend_result, context->function,
              cursor, &merge)) {
        size_t then_end = then_block;
        for (size_t entry = 0u; entry < merge.count; entry += 1u) {
          const hir0_branch_assignment *assignment = &merge.entries[entry];
          then_end = hir0_emit_expression_terms_m2(
              context, assignment->then_rhs, then_end,
              assignment->then_statement, 0u);
          const uint32_t value_index = hir0_emit_value_m2(
              context, assignment->then_rhs,
              W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
              (uint32_t)then_end, (uint32_t)entry, then_end, 0u);
          hir0_emit_edge_argument_m2(context, then_end, join_block, value_index,
                                     (uint32_t)entry);
        }
        size_t else_end = else_block;
        for (size_t entry = 0u; entry < merge.count; entry += 1u) {
          const hir0_branch_assignment *assignment = &merge.entries[entry];
          else_end = hir0_emit_expression_terms_m2(
              context, assignment->else_rhs, else_end,
              assignment->else_statement, 0u);
          const uint32_t value_index = hir0_emit_value_m2(
              context, assignment->else_rhs,
              W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
              (uint32_t)else_end, (uint32_t)entry, else_end, 0u);
          hir0_emit_edge_argument_m2(context, else_end, join_block, value_index,
                                     (uint32_t)entry);
        }
      } else {
        hir0_emit_chain_terms_m2(context, statement->first_child, then_block,
                                 depth + 1u);
        hir0_emit_chain_terms_m2(context, statement->else_child, else_block,
                                 depth + 1u);
      }
      if (terminal) return;
      current_block = join_block;
    } else if (statement->kind == W_SEED_FRONTEND_STMT_WHILE ||
               statement->kind == W_SEED_FRONTEND_STMT_REPEAT) {
      const bool post_test =
          statement->kind == W_SEED_FRONTEND_STMT_REPEAT;
      const size_t preheader = current_block;
      const size_t header = preheader + 1u;
      const size_t body = header + 1u;
      const size_t latch = body + 1u;
      const size_t exit = post_test ? latch + 1u : body + 1u;
      if (!hir0_load_loop_roots(context, statement->first_child)) return;
      context->loop_active = false;
      context->statement_index = cursor;
      for (size_t carrier_ordinal = 0u;
           carrier_ordinal < context->loop_root_count; carrier_ordinal += 1u) {
        uint32_t assignment_expression = W_SEED_FRONTEND_NONE;
        if (!hir0_loop_assignment_for_root(
                context, statement->first_child,
                context->loop_root_statements[carrier_ordinal],
                &assignment_expression))
          return;
        const w_seed_frontend_expression *assignment =
            &context->frontend->expressions[assignment_expression];
        const uint32_t initial = hir0_emit_value_m2(
            context, assignment->left, W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
            (uint32_t)preheader, (uint32_t)carrier_ordinal, preheader, 0u);
        hir0_emit_edge_argument_m2(context, preheader, header, initial,
                                   (uint32_t)carrier_ordinal);
      }
      context->loop_post_test = post_test;
      if (!post_test) {
        context->loop_active = true;
        context->loop_header_block = header;
        context->loop_body_block = body;
        context->loop_exit_block = exit;
        context->output->terminators[header].value_index =
            hir0_emit_value_m2(
                context, statement->condition_expression,
                W_SEED_HIR0_VALUE_OWNER_TERMINATOR, (uint32_t)header, 0u,
                header, 0u);
        context->loop_active = false;
        /* Backedge values must see the loop body's latest versions, but not a
         * later exit-side continuation assignment.  The first sibling after
         * the while is the correct source-order cut; with no sibling the
         * bounded function's terminal return remains the cut. */
        context->statement_index = statement->next_sibling !=
                                           W_SEED_FRONTEND_NONE
                                       ? statement->next_sibling
                                       : (uint32_t)context->frontend_result
                                             ->written.statements;
        for (size_t carrier_ordinal = 0u;
             carrier_ordinal < context->loop_root_count;
             carrier_ordinal += 1u) {
          uint32_t assignment_expression = W_SEED_FRONTEND_NONE;
          if (!hir0_loop_assignment_for_root(
                  context, statement->first_child,
                  context->loop_root_statements[carrier_ordinal],
                  &assignment_expression))
            return;
          const w_seed_frontend_expression *assignment =
              &context->frontend->expressions[assignment_expression];
          const uint32_t next = hir0_emit_value_m2(
              context, assignment->left, W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
              (uint32_t)body, (uint32_t)carrier_ordinal, body, 0u);
          hir0_emit_edge_argument_m2(context, body, header, next,
                                     (uint32_t)carrier_ordinal);
        }
        context->loop_active = true;
      } else {
        /* The repeat body receives the initial tuple in `header`, then
         * transfers to `body`, whose branch observes the body-written
         * versions.  A separate latch owns the updated tuple edge because
         * HIR branches deliberately do not carry edge arguments. */
        context->loop_header_block = header;
        context->loop_body_block = body;
        context->loop_exit_block = exit;
        context->statement_index = statement->next_sibling !=
                                           W_SEED_FRONTEND_NONE
                                       ? statement->next_sibling
                                       : (uint32_t)context->frontend_result
                                             ->written.statements;
        context->loop_active = true;
        context->output->terminators[body].value_index =
            hir0_emit_value_m2(
                context, statement->condition_expression,
                W_SEED_HIR0_VALUE_OWNER_TERMINATOR, (uint32_t)body, 0u, body,
                0u);
        for (size_t carrier_ordinal = 0u;
             carrier_ordinal < context->loop_root_count;
             carrier_ordinal += 1u) {
          uint32_t assignment_expression = W_SEED_FRONTEND_NONE;
          if (!hir0_loop_assignment_for_root(
                  context, statement->first_child,
                  context->loop_root_statements[carrier_ordinal],
                  &assignment_expression))
            return;
          const w_seed_frontend_expression *assignment =
              &context->frontend->expressions[assignment_expression];
          const uint32_t next = hir0_emit_value_m2(
              context, assignment->left, W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
              (uint32_t)latch, (uint32_t)carrier_ordinal, latch, 0u);
          hir0_emit_edge_argument_m2(context, latch, header, next,
                                     (uint32_t)carrier_ordinal);
        }
        context->loop_active = true;
      }
      current_block = exit;
    } else if (statement->kind == W_SEED_FRONTEND_STMT_RETURN) {
      const w_seed_frontend_expression *root =
          &context->frontend->expressions[statement->expression_index];
      if (root->kind == W_SEED_FRONTEND_EXPR_SWITCH) {
        const size_t dispatch = hir0_emit_expression_terms_m2(
            context, root->left, current_block, cursor, 0u);
        context->output->terminators[dispatch].value_index =
            hir0_emit_value_m2(
                context, root->left, W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
                (uint32_t)dispatch, 0u, dispatch, 0u);
        for (size_t ordinal = 0u; ordinal < root->switch_arm_count;
             ordinal += 1u) {
          const w_seed_frontend_switch_arm *arm =
              hir0_switch_arm_for_ordinal(context, root, ordinal);
          const size_t target_block = dispatch + 1u + ordinal;
          const size_t end = hir0_emit_expression_terms_m2(
              context, arm->result_expression, target_block, cursor, 0u);
          context->output->terminators[end].value_index = hir0_emit_value_m2(
              context, arm->result_expression,
              W_SEED_HIR0_VALUE_OWNER_TERMINATOR, (uint32_t)end, 0u, end,
              0u);
        }
        return;
      }
      const size_t end = hir0_emit_expression_terms_m2(
          context, statement->expression_index, current_block, cursor, 0u);
      context->output->terminators[end].value_index = hir0_emit_value_m2(
          context, statement->expression_index,
          W_SEED_HIR0_VALUE_OWNER_TERMINATOR, (uint32_t)end, 0u, end,
          0u);
      return;
    }
    cursor = statement->next_sibling;
    guard += 1u;
  }
}

static size_t hir0_expression_block_count_m2(const hir0_emit_context *context,
                                             uint32_t expression,
                                             size_t depth) {
  const size_t logical =
      hir0_expression_logical_count(context, expression, depth);
  return logical > (SIZE_MAX - 1u) / 3u ? 0u : 1u + logical * 3u;
}

static void hir0_begin_block_m2(hir0_emit_context *context,
                                size_t block_index) {
  w_seed_hir0_block *block = &context->output->blocks[block_index];
  if (block->first_instruction == W_SEED_HIR0_NONE)
    block->first_instruction = (uint32_t)*context->instruction_offset;
}

static void hir0_finish_block_m2(hir0_emit_context *context,
                                 size_t block_index) {
  w_seed_hir0_block *block = &context->output->blocks[block_index];
  hir0_begin_block_m2(context, block_index);
  block->instruction_count = (uint32_t)(
      *context->instruction_offset - block->first_instruction);
}

static void hir0_set_jump_m2(hir0_emit_context *context, size_t block_index,
                             size_t target_block, w_seed_span span) {
  hir0_finish_block_m2(context, block_index);
  const w_seed_hir0_block *block = &context->output->blocks[block_index];
  context->output->terminators[block_index] = (w_seed_hir0_terminator){
      .owner_block = (uint32_t)block_index,
      .kind = W_SEED_HIR0_TERMINATOR_JUMP,
      .ordinal = block->instruction_count,
      .value_index = W_SEED_HIR0_NONE,
      .result_type = 0u,
      .target_block = (uint32_t)target_block,
      .else_block = W_SEED_HIR0_NONE,
      .first_edge_argument = W_SEED_HIR0_NONE,
      .edge_argument_count = 0u,
      .logical_operator = W_SEED_HIR0_LOGICAL_NONE,
      .source_span = span};
}

static void hir0_set_return_unit_m2(hir0_emit_context *context,
                                    size_t block_index, w_seed_span span) {
  hir0_finish_block_m2(context, block_index);
  const w_seed_hir0_block *block = &context->output->blocks[block_index];
  context->output->terminators[block_index] = (w_seed_hir0_terminator){
      .owner_block = (uint32_t)block_index,
      .kind = W_SEED_HIR0_TERMINATOR_RETURN_UNIT,
      .ordinal = block->instruction_count,
      .value_index = W_SEED_HIR0_NONE,
      .result_type = 0u,
      .target_block = W_SEED_HIR0_NONE,
      .else_block = W_SEED_HIR0_NONE,
      .first_edge_argument = W_SEED_HIR0_NONE,
      .edge_argument_count = 0u,
      .logical_operator = W_SEED_HIR0_LOGICAL_NONE,
      .source_span = span};
}

static size_t hir0_emit_expression_layout_m2(hir0_emit_context *context,
                                             uint32_t expression,
                                             size_t current_block,
                                             size_t statement_index,
                                             size_t depth);

static size_t hir0_emit_call_layout_m2(hir0_emit_context *context,
                                       uint32_t expression,
                                       size_t current_block,
                                       size_t statement_index, size_t depth) {
  (void)statement_index;
  (void)depth;
  const w_seed_frontend_expression *source =
      &context->frontend->expressions[expression];
  const bool host_call = source->resolved_callee_kind ==
                         W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL;
  const uint32_t target_function_index = source->resolved_function_index;
  const uint32_t host_index = source->resolved_host_symbol_index;
  const w_seed_frontend_host_prelude_symbol *host =
      host_call ? &context->frontend_input->host_scope->symbols[host_index]
                : NULL;
  const w_seed_frontend_function *local =
      host_call ? NULL : &context->frontend->functions[target_function_index];
  const size_t first_argument = *context->argument_offset;
  *context->argument_offset += source->argument_count;
  for (size_t argument = 0u; argument < source->argument_count; argument += 1u) {
    const w_seed_frontend_argument *argument_source =
        &context->frontend->arguments[(size_t)source->first_argument + argument];
    w_seed_hir0_argument *target =
        &context->output->arguments[first_argument + argument];
    target->owner_call = W_SEED_HIR0_NONE;
    target->ordinal = (uint32_t)argument;
    target->parameter_ordinal = argument_source->resolved_parameter_ordinal;
    target->value_index = W_SEED_HIR0_NONE;
    if (host_call) {
      const w_seed_frontend_external_parameter *parameter =
          &host->parameters[target->parameter_ordinal];
      target->type_index = 1u;
      target->label_kind = hir_label_kind(parameter->label_kind);
    } else {
      const w_seed_frontend_parameter *parameter =
          &context->frontend->parameters[(size_t)local->first_parameter +
                                         target->parameter_ordinal];
      target->type_index = hir_type_from_frontend(
          context->frontend, context->frontend_result, parameter->type_index);
      target->label_kind = hir_label_kind(parameter->label_kind);
    }
    append_text_unchecked(argument_source->label, context->output->text_bytes,
                          context->text_offset, &target->label);
    target->source_span = argument_source->span;
  }
  size_t argument_block = current_block;
  for (size_t argument = 0u; argument < source->argument_count; argument += 1u)
    argument_block = hir0_emit_expression_layout_m2(
        context,
        context->frontend->arguments[(size_t)source->first_argument + argument]
            .expression_index,
        argument_block, statement_index, depth + 1u);
  current_block = argument_block;
  hir0_begin_block_m2(context, current_block);
  const uint32_t emitted_call = (uint32_t)*context->call_offset;
  const uint32_t emitted_instruction = (uint32_t)*context->instruction_offset;
  w_seed_hir0_block *block = &context->output->blocks[current_block];
  w_seed_hir0_instruction *instruction =
      &context->output->instructions[*context->instruction_offset];
  instruction->kind = W_SEED_HIR0_INSTRUCTION_CALL;
  instruction->owner_block = (uint32_t)current_block;
  instruction->ordinal = (uint32_t)(
      *context->instruction_offset - block->first_instruction);
  instruction->call_index = emitted_call;
  instruction->binding_index = W_SEED_HIR0_NONE;
  instruction->result_type = 0u;
  instruction->source_span = source->span;
  w_seed_hir0_call *call = &context->output->calls[*context->call_offset];
  call->owner_instruction = emitted_instruction;
  call->owner_block = (uint32_t)current_block;
  call->ordinal = instruction->ordinal;
  call->callee_identity = host_call
                              ? hir_host_identity_index(context->counts,
                                                        host_index)
                              : (uint32_t)(context->counts->modules +
                                            target_function_index);
  call->first_argument = (uint32_t)first_argument;
  call->argument_count = source->argument_count;
  const w_seed_hir0_identity *identity =
      &context->output->identities[call->callee_identity];
  call->first_requirement =
      host_call ? identity->first_requirement : W_SEED_HIR0_NONE;
  call->requirement_count = host_call ? identity->requirement_count : 0u;
  call->result_type = identity->return_type;
  call->execution_kind = W_SEED_HIR0_CALL_DIRECT;
  call->source_expression = expression;
  call->source_span = source->span;
  instruction->result_type = call->result_type;
  for (size_t argument = 0u; argument < source->argument_count; argument += 1u)
    context->output->arguments[first_argument + argument].owner_call =
        emitted_call;
  *context->instruction_offset += 1u;
  *context->call_offset += 1u;
  return current_block;
}

static size_t hir0_expression_layout_end_m2(const hir0_emit_context *context,
                                            uint32_t expression,
                                            size_t current_block, size_t depth) {
  if (context == NULL || expression == W_SEED_FRONTEND_NONE ||
      depth > W_SEED_HIR0_MAX_NESTING)
    return current_block;
  const w_seed_frontend_expression *source =
      &context->frontend->expressions[expression];
  if (source->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH)
    return hir0_expression_layout_end_m2(
        context, source->task_call_expression, current_block, depth + 1u);
  if (source->kind == W_SEED_FRONTEND_EXPR_AWAIT)
    return hir0_expression_layout_end_m2(context, source->left, current_block,
                                         depth + 1u);
  if (source->kind == W_SEED_FRONTEND_EXPR_IF) {
    const size_t branch = hir0_expression_layout_end_m2(
        context, source->left, current_block, depth + 1u);
    const size_t then_count = hir0_expression_block_count_m2(
        context, source->right, depth + 1u);
    const size_t else_count = hir0_expression_block_count_m2(
        context, source->else_expression, depth + 1u);
    return branch + 1u + then_count + else_count;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS ||
      source->kind == W_SEED_FRONTEND_EXPR_UNARY)
    return hir0_expression_layout_end_m2(context, source->left, current_block,
                                         depth + 1u);
  if (source->kind == W_SEED_FRONTEND_EXPR_CALL) {
    size_t block = current_block;
    for (size_t ordinal = 0u; ordinal < source->argument_count; ordinal += 1u)
      block = hir0_expression_layout_end_m2(
          context,
          context->frontend->arguments[(size_t)source->first_argument + ordinal]
              .expression_index,
          block, depth + 1u);
    return block;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING) {
    size_t block = current_block;
    for (size_t ordinal = 0u; ordinal < source->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_frontend_interpolation_segment *segment =
          &context->frontend->interpolation_segments[
              (size_t)source->first_interpolation_segment + ordinal];
      if (segment->kind == W_SEED_FRONTEND_INTERPOLATION_EXPRESSION)
        block = hir0_expression_layout_end_m2(
            context, segment->expression_index, block, depth + 1u);
    }
    return block;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_BINARY) {
    size_t block = hir0_expression_layout_end_m2(
        context, source->left, current_block, depth + 1u);
    const w_seed_hir0_logical_operator logical =
        hir_logical_operator(source->operator_text);
    if (logical == W_SEED_HIR0_LOGICAL_NONE)
      return hir0_expression_layout_end_m2(context, source->right, block,
                                           depth + 1u);
    const size_t rhs_count = hir0_expression_block_count_m2(
        context, source->right, depth + 1u);
    const size_t true_block = block + 1u;
    const size_t false_block = logical == W_SEED_HIR0_LOGICAL_AND
                                   ? true_block + rhs_count
                                   : true_block + 1u;
    return logical == W_SEED_HIR0_LOGICAL_AND
               ? false_block + 1u
               : false_block + rhs_count;
  }
  return current_block;
}

static void hir0_emit_binding_layout_m2(hir0_emit_context *context,
                                        uint32_t statement_index,
                                        size_t block_index) {
  const w_seed_frontend_statement *statement =
      &context->frontend->statements[statement_index];
  const bool assignment =
      statement->kind == W_SEED_FRONTEND_STMT_EXPRESSION &&
      context->frontend->expressions[statement->expression_index].kind ==
          W_SEED_FRONTEND_EXPR_ASSIGNMENT;
  uint32_t declaration_statement = statement_index;
  w_seed_frontend_text binding_name = statement->binding_name;
  uint32_t frontend_type = statement->effective_type;
  uint32_t source_binding = (uint32_t)*context->binding_offset;
  uint32_t previous_version = W_SEED_HIR0_NONE;
  if (assignment) {
    const w_seed_frontend_expression *root =
        &context->frontend->expressions[statement->expression_index];
    const w_seed_frontend_expression *target =
        &context->frontend->expressions[root->left];
    declaration_statement = target->resolved_binding_statement;
    binding_name = target->spelling;
    frontend_type = target->inferred_type;
    (void)root_binding_index_for_statement(
        context->frontend, context->frontend_result, context->function,
        declaration_statement, &source_binding);
    (void)binding_index_for_statement(
        context->frontend, context->frontend_result, context->function,
        statement_index, declaration_statement, &previous_version);
  }
  hir0_begin_block_m2(context, block_index);
  w_seed_hir0_block *block = &context->output->blocks[block_index];
  const uint32_t instruction_index = (uint32_t)*context->instruction_offset;
  const uint32_t binding_index = (uint32_t)*context->binding_offset;
  context->output->instructions[*context->instruction_offset] =
      (w_seed_hir0_instruction){
          .kind = W_SEED_HIR0_INSTRUCTION_BINDING,
          .owner_block = (uint32_t)block_index,
          .ordinal = (uint32_t)(*context->instruction_offset -
                                block->first_instruction),
          .call_index = W_SEED_HIR0_NONE,
          .binding_index = binding_index,
          .result_type = 0u,
          .source_span = statement->span};
  context->output->bindings[*context->binding_offset] =
      (w_seed_hir0_binding){
          .owner_instruction = instruction_index,
          .owner_block = (uint32_t)block_index,
          .ordinal = (uint32_t)(*context->instruction_offset -
                                block->first_instruction),
          .type_index = hir_type_from_frontend(
              context->frontend, context->frontend_result,
              frontend_type),
          .name = {0u, 0u},
          .is_mutable = statement->kind == W_SEED_FRONTEND_STMT_VAR ||
                        assignment,
          .task_role = W_SEED_HIR0_TASK_ROLE_NONE,
          .source_binding = source_binding,
          .previous_version = previous_version,
          .next_version = W_SEED_HIR0_NONE,
          .task_peer_binding = W_SEED_HIR0_NONE,
          .initializer_value = W_SEED_HIR0_NONE,
          .source_span = statement->span};
  if (!assignment && statement->expression_index != W_SEED_FRONTEND_NONE) {
    const w_seed_frontend_expression *initializer =
        &context->frontend->expressions[statement->expression_index];
    if (initializer->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH) {
      context->output->bindings[*context->binding_offset].task_role =
          W_SEED_HIR0_TASK_ROLE_LAUNCH;
    } else if (initializer->kind == W_SEED_FRONTEND_EXPR_AWAIT) {
      uint32_t launch_binding = 0u;
      context->output->bindings[*context->binding_offset].task_role =
          W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT;
      /* collect() already proved this exact source binding relation. */
      (void)binding_index_for_statement(
          context->frontend, context->frontend_result, context->function,
          statement_index, initializer->task_binding_statement,
          &launch_binding);
      context->output->bindings[*context->binding_offset].task_peer_binding =
          launch_binding;
      context->output->bindings[launch_binding].task_peer_binding =
          binding_index;
    }
  }
  if (previous_version != W_SEED_HIR0_NONE)
    context->output->bindings[previous_version].next_version =
        (uint32_t)*context->binding_offset;
  append_text_unchecked(binding_name, context->output->text_bytes,
                        context->text_offset,
                        &context->output->bindings[*context->binding_offset]
                             .name);
  *context->binding_offset += 1u;
  *context->instruction_offset += 1u;
}

static void hir0_emit_branch_merge_binding_layout_m2(
    hir0_emit_context *context, uint32_t if_statement,
    const hir0_branch_assignment_merge *merge, size_t join_block) {
  const w_seed_frontend_statement *branch =
      &context->frontend->statements[if_statement];
  hir0_begin_block_m2(context, join_block);
  w_seed_hir0_block *join = &context->output->blocks[join_block];
  join->first_block_argument = (uint32_t)*context->block_argument_index;
  join->block_argument_count = merge->count;
  for (size_t entry = 0u; entry < merge->count; entry += 1u) {
    const w_seed_frontend_statement *declaration =
        &context->frontend->statements[
            merge->entries[entry].declaration_statement];
    const uint32_t type_index = hir_type_from_frontend(
        context->frontend, context->frontend_result, declaration->effective_type);
    context->output->block_arguments[*context->block_argument_index] =
        (w_seed_hir0_block_argument){
            .owner_block = (uint32_t)join_block,
            .ordinal = (uint32_t)entry,
            .type_index = type_index,
            .source_span = branch->span};
    *context->block_argument_index += 1u;
  }
  for (size_t entry = 0u; entry < merge->count; entry += 1u) {
    const hir0_branch_assignment *assignment = &merge->entries[entry];
    const w_seed_frontend_statement *declaration =
        &context->frontend->statements[assignment->declaration_statement];
    const w_seed_frontend_expression *root =
        &context->frontend->expressions[
            context->frontend->statements[assignment->then_statement]
                .expression_index];
    const w_seed_frontend_expression *target =
        &context->frontend->expressions[root->left];
    uint32_t source_binding = W_SEED_HIR0_NONE;
    uint32_t previous_version = W_SEED_HIR0_NONE;
    (void)root_binding_index_for_statement(
        context->frontend, context->frontend_result, context->function,
        assignment->declaration_statement, &source_binding);
    (void)binding_index_for_statement(
        context->frontend, context->frontend_result, context->function,
        if_statement, assignment->declaration_statement, &previous_version);
    const uint32_t type_index = hir_type_from_frontend(
        context->frontend, context->frontend_result, declaration->effective_type);
    const uint32_t instruction_index = (uint32_t)*context->instruction_offset;
    const uint32_t binding_index = (uint32_t)*context->binding_offset;
    context->output->instructions[*context->instruction_offset] =
        (w_seed_hir0_instruction){
            .kind = W_SEED_HIR0_INSTRUCTION_BINDING,
            .owner_block = (uint32_t)join_block,
            .ordinal = (uint32_t)(*context->instruction_offset -
                                  join->first_instruction),
            .call_index = W_SEED_HIR0_NONE,
            .binding_index = binding_index,
            .result_type = 0u,
            .source_span = branch->span};
    context->output->bindings[*context->binding_offset] =
        (w_seed_hir0_binding){
            .owner_instruction = instruction_index,
            .owner_block = (uint32_t)join_block,
            .ordinal = (uint32_t)(*context->instruction_offset -
                                  join->first_instruction),
            .type_index = type_index,
            .name = {0u, 0u},
            .is_mutable = true,
            .task_role = W_SEED_HIR0_TASK_ROLE_NONE,
            .source_binding = source_binding,
            .previous_version = previous_version,
            .next_version = W_SEED_HIR0_NONE,
            .task_peer_binding = W_SEED_HIR0_NONE,
            .initializer_value = W_SEED_HIR0_NONE,
            .source_span = branch->span};
    if (previous_version != W_SEED_HIR0_NONE)
      context->output->bindings[previous_version].next_version = binding_index;
    append_text_unchecked(target->spelling, context->output->text_bytes,
                          context->text_offset,
                          &context->output->bindings[*context->binding_offset]
                               .name);
    *context->binding_offset += 1u;
    *context->instruction_offset += 1u;
  }
}

static const w_seed_frontend_switch_arm *hir0_switch_arm_for_ordinal(
    const hir0_emit_context *context,
    const w_seed_frontend_expression *root, size_t ordinal) {
  if (context == NULL || root == NULL ||
      root->first_switch_arm == W_SEED_FRONTEND_NONE ||
      ordinal >= root->switch_arm_count)
    return NULL;
  const uint32_t enum_index = root->enum_index;
  if (enum_index == W_SEED_FRONTEND_NONE ||
      (size_t)enum_index >= context->frontend_result->written.enums)
    return NULL;
  const w_seed_frontend_enum *decl = &context->frontend->enums[enum_index];
  uint32_t case_index = decl->first_case + (uint32_t)ordinal;
  if (root->left != W_SEED_FRONTEND_NONE &&
      (size_t)root->left < context->frontend_result->written.expressions) {
    const w_seed_frontend_expression *subject =
        &context->frontend->expressions[root->left];
    if (subject->inferred_type != W_SEED_FRONTEND_NONE &&
        (size_t)subject->inferred_type <
            context->frontend_result->written.types) {
      const w_seed_frontend_type *subject_type =
          &context->frontend->types[subject->inferred_type];
      if (subject_type->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET) {
        if (ordinal >= subject_type->subset_member_count ||
            subject_type->first_subset_member == W_SEED_FRONTEND_NONE ||
            context->frontend->enum_subset_members == NULL)
          return NULL;
        case_index = context->frontend->enum_subset_members[
            (size_t)subject_type->first_subset_member + ordinal]
                         .enum_case_index;
      }
    }
  }
  for (size_t index = 0u; index < root->switch_arm_count; index += 1u) {
    const w_seed_frontend_switch_arm *arm =
        &context->frontend->switch_arms[(size_t)root->first_switch_arm + index];
    if (arm->enum_case_index == case_index) return arm;
  }
  return NULL;
}

static void hir0_emit_switch_return_layout_m2(
    hir0_emit_context *context, const w_seed_frontend_statement *statement,
    size_t current_block) {
  const w_seed_frontend_expression *root =
      &context->frontend->expressions[statement->expression_index];
  const size_t dispatch = hir0_emit_expression_layout_m2(
      context, root->left, current_block, context->statement_index, 0u);
  const w_seed_frontend_enum *decl = &context->frontend->enums[root->enum_index];
  const w_seed_frontend_type *subject_type =
      &context->frontend->types[
          context->frontend->expressions[root->left].inferred_type];
  const bool subject_is_subset =
      subject_type->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  const size_t switch_case_count =
      subject_is_subset ? subject_type->subset_member_count : decl->case_count;
  hir0_finish_block_m2(context, dispatch);
  const uint32_t first_edge = (uint32_t)*context->switch_edge_index;
  context->output->terminators[dispatch] = (w_seed_hir0_terminator){
      .owner_block = (uint32_t)dispatch,
      .kind = W_SEED_HIR0_TERMINATOR_SWITCH_ENUM,
      .ordinal = context->output->blocks[dispatch].instruction_count,
      .value_index = W_SEED_HIR0_NONE,
      .result_type = context->output->functions[context->function].return_type,
      .target_block = W_SEED_HIR0_NONE,
      .else_block = W_SEED_HIR0_NONE,
      .first_edge_argument = W_SEED_HIR0_NONE,
      .edge_argument_count = 0u,
      .logical_operator = W_SEED_HIR0_LOGICAL_NONE,
      .switch_enum_index = root->enum_index,
      .first_switch_edge = first_edge,
      .switch_edge_count = (uint32_t)switch_case_count,
      /* A subset preserves the base enum representation and tags.  Its
       * carrier can never be compacted to the number of admitted cases. */
      .switch_carrier_width = hir0_enum_carrier_width(decl->case_count),
      .source_span = root->span};
  /* Switch edges are dense in the enum declaration's canonical case order,
   * even when source arms are written out of order.  Keep each arm's pattern
   * span on the edge so provenance still identifies its source occurrence. */
  for (size_t ordinal = 0u; ordinal < switch_case_count; ordinal += 1u) {
    const w_seed_frontend_switch_arm *arm =
        hir0_switch_arm_for_ordinal(context, root, ordinal);
    const size_t target_block = dispatch + 1u + ordinal;
    hir0_begin_block_m2(context, target_block);
    const size_t end = hir0_emit_expression_layout_m2(
        context, arm->result_expression, target_block,
        context->statement_index, 0u);
    hir0_finish_block_m2(context, end);
    context->output->terminators[end] = (w_seed_hir0_terminator){
        .owner_block = (uint32_t)end,
        .kind = W_SEED_HIR0_TERMINATOR_RETURN_VALUE,
        .ordinal = context->output->blocks[end].instruction_count,
        .value_index = W_SEED_HIR0_NONE,
        .result_type = context->output->functions[context->function].return_type,
        .target_block = W_SEED_HIR0_NONE,
        .else_block = W_SEED_HIR0_NONE,
        .first_edge_argument = W_SEED_HIR0_NONE,
        .edge_argument_count = 0u,
        .logical_operator = W_SEED_HIR0_LOGICAL_NONE,
        .source_span = arm->span};
    context->output->switch_edges[*context->switch_edge_index] =
        (w_seed_hir0_switch_edge){
            .owner_terminator = (uint32_t)dispatch,
            .ordinal = (uint32_t)ordinal,
            .enum_index = root->enum_index,
            .enum_case_index =
                subject_is_subset
                    ? context->frontend->enum_subset_members[
                          (size_t)subject_type->first_subset_member + ordinal]
                          .enum_case_index
                    : decl->first_case + (uint32_t)ordinal,
            .target_block = (uint32_t)end,
            .first_capture = (uint32_t)*context->switch_capture_index,
            .capture_count = arm->capture_count,
            .source_span = arm->pattern_span};
    for (size_t capture_ordinal = 0u; capture_ordinal < arm->capture_count;
         capture_ordinal += 1u) {
      const w_seed_frontend_pattern_capture *source =
          &context->frontend->pattern_captures[
              (size_t)arm->first_capture + capture_ordinal];
      w_seed_hir0_switch_capture *target =
          &context->output->switch_captures[*context->switch_capture_index];
      *target = (w_seed_hir0_switch_capture){
          .owner_switch_edge = (uint32_t)*context->switch_edge_index,
          .ordinal = (uint32_t)capture_ordinal,
          .parameter_ordinal = source->parameter_ordinal,
          .type_index = hir_type_from_frontend(
              context->frontend, context->frontend_result, source->type_index),
          .source_span = source->span};
      append_text_unchecked(source->name, context->output->text_bytes,
                            context->text_offset, &target->name);
      *context->switch_capture_index += 1u;
    }
    *context->switch_edge_index += 1u;
  }
}

static void hir0_emit_chain_layout_m2(hir0_emit_context *context,
                                      uint32_t first_statement,
                                      size_t current_block,
                                      uint32_t terminal_target, bool root,
                                      size_t depth) {
  hir0_begin_block_m2(context, current_block);
  uint32_t cursor = first_statement;
  size_t guard = 0u;
  while (cursor != W_SEED_FRONTEND_NONE &&
         guard < context->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        &context->frontend->statements[cursor];
    context->statement_index = cursor;
    if (statement->kind == W_SEED_FRONTEND_STMT_LET ||
        statement->kind == W_SEED_FRONTEND_STMT_VAR ||
        statement->kind == W_SEED_FRONTEND_STMT_EXPRESSION) {
      uint32_t lowered_expression = statement->expression_index;
      bool creates_binding = statement->kind == W_SEED_FRONTEND_STMT_LET ||
                             statement->kind == W_SEED_FRONTEND_STMT_VAR;
      if (statement->kind == W_SEED_FRONTEND_STMT_EXPRESSION) {
        const w_seed_frontend_expression *expression =
            &context->frontend->expressions[statement->expression_index];
        if (expression->kind == W_SEED_FRONTEND_EXPR_ASSIGNMENT) {
          lowered_expression = expression->right;
          creates_binding = true;
        }
      }
      current_block = hir0_emit_expression_layout_m2(
          context, lowered_expression, current_block, cursor, 0u);
      if (creates_binding)
        hir0_emit_binding_layout_m2(context, cursor, current_block);
    } else if (statement->kind == W_SEED_FRONTEND_STMT_IF) {
      current_block = hir0_emit_expression_layout_m2(
          context, statement->condition_expression, current_block, cursor, 0u);
      const size_t then_block = current_block + 1u;
      const size_t then_count = hir0_region_block_count(
          context, statement->first_child, depth + 1u);
      const size_t else_block = then_block + then_count;
      const size_t else_count = hir0_region_block_count(
          context, statement->else_child, depth + 1u);
      const bool terminal = hir0_terminal_if(context, statement, depth);
      const size_t join_block = terminal ? W_SEED_HIR0_NONE
                                         : else_block + else_count;
      hir0_branch_assignment_merge merge;
      const bool branch_merge = frontend_branch_assignment_merge(
          context->frontend, context->frontend_result, context->function,
          cursor, &merge);
      /* A statement-level mutation diamond has no scalar result. Its join
       * values are carried solely by destination block arguments and edge
       * arguments; result_type remains Unit. */
      const uint32_t branch_type = 0u;
      hir0_finish_block_m2(context, current_block);
      context->output->terminators[current_block] = (w_seed_hir0_terminator){
          .owner_block = (uint32_t)current_block,
          .kind = W_SEED_HIR0_TERMINATOR_BRANCH,
          .ordinal = context->output->blocks[current_block].instruction_count,
          .value_index = W_SEED_HIR0_NONE,
          .result_type = branch_type,
          .target_block = (uint32_t)then_block,
          .else_block = (uint32_t)else_block,
          .first_edge_argument = W_SEED_HIR0_NONE,
          .edge_argument_count = 0u,
          .logical_operator = W_SEED_HIR0_LOGICAL_NONE,
          .source_span = statement->span};
      if (branch_merge) {
        size_t then_end = then_block;
        uint32_t assignment_cursor = statement->first_child;
        size_t assignment_guard = 0u;
        while (assignment_cursor != W_SEED_FRONTEND_NONE &&
               assignment_guard <
                   context->frontend_result->written.statements) {
          const w_seed_frontend_statement *assignment =
              &context->frontend->statements[assignment_cursor];
          const w_seed_frontend_expression *rhs_root =
              &context->frontend->expressions[assignment->expression_index];
          then_end = hir0_emit_expression_layout_m2(
              context, rhs_root->right, then_end, assignment_cursor, 0u);
          assignment_cursor = assignment->next_sibling;
          assignment_guard += 1u;
        }
        hir0_set_jump_m2(context, then_end, join_block, statement->span);
        size_t else_end = else_block;
        assignment_cursor = statement->else_child;
        assignment_guard = 0u;
        while (assignment_cursor != W_SEED_FRONTEND_NONE &&
               assignment_guard <
                   context->frontend_result->written.statements) {
          const w_seed_frontend_statement *assignment =
              &context->frontend->statements[assignment_cursor];
          const w_seed_frontend_expression *rhs_root =
              &context->frontend->expressions[assignment->expression_index];
          else_end = hir0_emit_expression_layout_m2(
              context, rhs_root->right, else_end, assignment_cursor, 0u);
          assignment_cursor = assignment->next_sibling;
          assignment_guard += 1u;
        }
        hir0_set_jump_m2(context, else_end, join_block, statement->span);
        hir0_emit_branch_merge_binding_layout_m2(
            context, cursor, &merge, join_block);
      } else {
        hir0_emit_chain_layout_m2(context, statement->first_child, then_block,
                                  terminal ? W_SEED_HIR0_NONE
                                           : (uint32_t)join_block,
                                  false, depth + 1u);
        hir0_emit_chain_layout_m2(context, statement->else_child, else_block,
                                  terminal ? W_SEED_HIR0_NONE
                                           : (uint32_t)join_block,
                                  false, depth + 1u);
      }
      if (terminal) return;
      current_block = join_block;
      hir0_begin_block_m2(context, current_block);
    } else if (statement->kind == W_SEED_FRONTEND_STMT_WHILE ||
               statement->kind == W_SEED_FRONTEND_STMT_REPEAT) {
      const bool post_test =
          statement->kind == W_SEED_FRONTEND_STMT_REPEAT;
      const size_t preheader = current_block;
      const size_t header = preheader + 1u;
      const size_t body = header + 1u;
      const size_t latch = body + 1u;
      const size_t exit = post_test ? latch + 1u : body + 1u;
      if (!hir0_load_loop_roots(context, statement->first_child)) return;
      context->loop_active = true;
      context->loop_post_test = post_test;
      context->loop_header_block = header;
      context->loop_body_block = body;
      context->loop_exit_block = exit;
      hir0_set_jump_m2(context, preheader, header, statement->span);
      hir0_begin_block_m2(context, header);
      w_seed_hir0_block *header_block = &context->output->blocks[header];
      header_block->first_block_argument =
          (uint32_t)*context->block_argument_index;
      header_block->block_argument_count =
          (uint32_t)context->loop_root_count;
      for (size_t ordinal = 0u; ordinal < context->loop_root_count;
           ordinal += 1u) {
        context->output->block_arguments[*context->block_argument_index] =
            (w_seed_hir0_block_argument){
                .owner_block = (uint32_t)header,
                .ordinal = (uint32_t)ordinal,
                .type_index = W_SEED_HIR0_TYPE_I64,
                .source_span = statement->span};
        *context->block_argument_index += 1u;
      }
      if (!post_test) {
        hir0_finish_block_m2(context, header);
        context->output->terminators[header] = (w_seed_hir0_terminator){
            .owner_block = (uint32_t)header,
            .kind = W_SEED_HIR0_TERMINATOR_BRANCH,
            .ordinal = header_block->instruction_count,
            .value_index = W_SEED_HIR0_NONE,
            .result_type = 0u,
            .target_block = (uint32_t)body,
            .else_block = (uint32_t)exit,
            .first_edge_argument = W_SEED_HIR0_NONE,
            .edge_argument_count = 0u,
            .logical_operator = W_SEED_HIR0_LOGICAL_NONE,
            .source_span = statement->span};
        hir0_begin_block_m2(context, body);
        size_t body_end = body;
        uint32_t assignment_cursor = statement->first_child;
        size_t assignment_guard = 0u;
        while (assignment_cursor != W_SEED_FRONTEND_NONE &&
               assignment_guard <
                   context->frontend_result->written.statements) {
          const w_seed_frontend_statement *body_statement =
              &context->frontend->statements[assignment_cursor];
          const w_seed_frontend_expression *assignment =
              &context->frontend->expressions[body_statement->expression_index];
          body_end = hir0_emit_expression_layout_m2(
              context, assignment->right, body_end, assignment_cursor, 0u);
          hir0_emit_binding_layout_m2(context, assignment_cursor, body_end);
          assignment_cursor = body_statement->next_sibling;
          assignment_guard += 1u;
        }
        hir0_set_jump_m2(context, body_end, header, statement->span);
      } else {
        /* The carrier block is the post-test body.  It runs before the
         * condition block on every entry, including the first one. */
        size_t body_end = header;
        uint32_t assignment_cursor = statement->first_child;
        size_t assignment_guard = 0u;
        while (assignment_cursor != W_SEED_FRONTEND_NONE &&
               assignment_guard <
                   context->frontend_result->written.statements) {
          const w_seed_frontend_statement *body_statement =
              &context->frontend->statements[assignment_cursor];
          const w_seed_frontend_expression *assignment =
              &context->frontend->expressions[body_statement->expression_index];
          body_end = hir0_emit_expression_layout_m2(
              context, assignment->right, body_end, assignment_cursor, 0u);
          hir0_emit_binding_layout_m2(context, assignment_cursor, body_end);
          assignment_cursor = body_statement->next_sibling;
          assignment_guard += 1u;
        }
        hir0_set_jump_m2(context, body_end, body, statement->span);
        hir0_begin_block_m2(context, body);
        const size_t condition_end = hir0_emit_expression_layout_m2(
            context, statement->condition_expression, body,
            statement->next_sibling != W_SEED_FRONTEND_NONE
                ? statement->next_sibling
                : (uint32_t)context->frontend_result->written.statements,
            0u);
        hir0_finish_block_m2(context, condition_end);
        context->output->terminators[condition_end] =
            (w_seed_hir0_terminator){
                .owner_block = (uint32_t)condition_end,
                .kind = W_SEED_HIR0_TERMINATOR_BRANCH,
                .ordinal = context->output->blocks[condition_end]
                               .instruction_count,
                .value_index = W_SEED_HIR0_NONE,
                .result_type = 0u,
                .target_block = (uint32_t)latch,
                .else_block = (uint32_t)exit,
                .first_edge_argument = W_SEED_HIR0_NONE,
                .edge_argument_count = 0u,
                .logical_operator = W_SEED_HIR0_LOGICAL_NONE,
                .source_span = statement->span};
        hir0_begin_block_m2(context, latch);
        hir0_set_jump_m2(context, latch, header, statement->span);
      }
      current_block = exit;
      hir0_begin_block_m2(context, current_block);
    } else if (statement->kind == W_SEED_FRONTEND_STMT_RETURN) {
      if (context->frontend->expressions[statement->expression_index].kind ==
          W_SEED_FRONTEND_EXPR_SWITCH) {
        hir0_emit_switch_return_layout_m2(context, statement, current_block);
        return;
      }
      current_block = hir0_emit_expression_layout_m2(
          context, statement->expression_index, current_block, cursor, 0u);
      hir0_finish_block_m2(context, current_block);
      const w_seed_hir0_block *block = &context->output->blocks[current_block];
      context->output->terminators[current_block] = (w_seed_hir0_terminator){
          .owner_block = (uint32_t)current_block,
          .kind = W_SEED_HIR0_TERMINATOR_RETURN_VALUE,
          .ordinal = block->instruction_count,
          .value_index = W_SEED_HIR0_NONE,
          .result_type = context->output->functions[context->function]
                             .return_type,
          .target_block = W_SEED_HIR0_NONE,
          .else_block = W_SEED_HIR0_NONE,
          .first_edge_argument = W_SEED_HIR0_NONE,
          .edge_argument_count = 0u,
          .logical_operator = W_SEED_HIR0_LOGICAL_NONE,
          .source_span = statement->span};
      return;
    }
    cursor = statement->next_sibling;
    guard += 1u;
  }
  if (root) {
    hir0_set_return_unit_m2(
        context, current_block,
        first_statement == W_SEED_FRONTEND_NONE
            ? context->output->blocks[current_block].source_span
            : context->frontend->statements[first_statement].span);
  } else {
    hir0_set_jump_m2(
        context, current_block, terminal_target,
        first_statement == W_SEED_FRONTEND_NONE
            ? context->output->blocks[current_block].source_span
            : context->frontend->statements[first_statement].span);
  }
}

static size_t hir0_emit_expression_layout_m2(hir0_emit_context *context,
                                             uint32_t expression,
                                             size_t current_block,
                                             size_t statement_index,
                                             size_t depth) {
  if (expression == W_SEED_FRONTEND_NONE ||
      depth > W_SEED_HIR0_MAX_NESTING)
    return current_block;
  const w_seed_frontend_expression *source =
      &context->frontend->expressions[expression];
  if (source->kind == W_SEED_FRONTEND_EXPR_EXECUTION_YIELD) {
    hir0_begin_block_m2(context, current_block);
    w_seed_hir0_block *block = &context->output->blocks[current_block];
    context->output->instructions[*context->instruction_offset] =
        (w_seed_hir0_instruction){
            .kind = W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD,
            .owner_block = (uint32_t)current_block,
            .ordinal = (uint32_t)(*context->instruction_offset -
                                  block->first_instruction),
            .call_index = W_SEED_HIR0_NONE,
            .binding_index = W_SEED_HIR0_NONE,
            .result_type = 0u,
            .source_span = source->span};
    *context->instruction_offset += 1u;
    return current_block;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH) {
    const size_t block = hir0_emit_expression_layout_m2(
        context, source->task_call_expression, current_block, statement_index,
        depth + 1u);
    /* Call layout emits arguments first and the root call last. collect()
     * proved that this wrapper owns exactly one root local call. */
    const w_seed_frontend_expression *call =
        &context->frontend->expressions[source->task_call_expression];
    const bool static_yield =
        call->resolved_function_index != W_SEED_FRONTEND_NONE &&
        frontend_elision_function_static_yields(
            &(w_seed_hir0_input){
                .frontend_input = context->frontend_input,
                .frontend_output = context->frontend,
                .frontend_result = context->frontend_result},
            call->resolved_function_index);
    context->output->calls[*context->call_offset - 1u].execution_kind =
        static_yield
            ? W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED
            : W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED;
    return block;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_AWAIT)
    return hir0_emit_expression_layout_m2(context, source->left, current_block,
                                          statement_index, depth + 1u);
  if (source->kind == W_SEED_FRONTEND_EXPR_IF) {
    const size_t branch = hir0_emit_expression_layout_m2(
        context, source->left, current_block, statement_index, depth + 1u);
    const size_t then_block = branch + 1u;
    const size_t then_count = hir0_expression_block_count_m2(
        context, source->right, depth + 1u);
    const size_t else_block = then_block + then_count;
    const size_t else_count = hir0_expression_block_count_m2(
        context, source->else_expression, depth + 1u);
    const size_t join_block = else_block + else_count;
    const uint32_t result_type = hir_type_from_frontend(
        context->frontend, context->frontend_result, source->inferred_type);
    hir0_finish_block_m2(context, branch);
    context->output->terminators[branch] = (w_seed_hir0_terminator){
        .owner_block = (uint32_t)branch,
        .kind = W_SEED_HIR0_TERMINATOR_BRANCH,
        .ordinal = context->output->blocks[branch].instruction_count,
        .value_index = W_SEED_HIR0_NONE,
        .result_type = result_type,
        .target_block = (uint32_t)then_block,
        .else_block = (uint32_t)else_block,
        .first_edge_argument = W_SEED_HIR0_NONE,
        .edge_argument_count = 0u,
        .logical_operator = W_SEED_HIR0_LOGICAL_NONE,
        .source_span = source->span};
    hir0_begin_block_m2(context, then_block);
    const size_t then_end = hir0_emit_expression_layout_m2(
        context, source->right, then_block, statement_index, depth + 1u);
    hir0_set_jump_m2(context, then_end, join_block, source->span);
    hir0_begin_block_m2(context, else_block);
    const size_t else_end = hir0_emit_expression_layout_m2(
        context, source->else_expression, else_block, statement_index,
        depth + 1u);
    hir0_set_jump_m2(context, else_end, join_block, source->span);
    hir0_begin_block_m2(context, join_block);
    w_seed_hir0_block *join = &context->output->blocks[join_block];
    join->first_block_argument = (uint32_t)*context->block_argument_index;
    join->block_argument_count = 1u;
    context->output->block_arguments[*context->block_argument_index] =
        (w_seed_hir0_block_argument){
            .owner_block = (uint32_t)join_block,
            .ordinal = 0u,
            .type_index = result_type,
            .source_span = source->span};
    *context->block_argument_index += 1u;
    return join_block;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS ||
      source->kind == W_SEED_FRONTEND_EXPR_UNARY)
    return hir0_emit_expression_layout_m2(context, source->left, current_block,
                                          statement_index, depth + 1u);
  if (source->kind == W_SEED_FRONTEND_EXPR_CALL) {
    if (hir0_is_local_enum_constructor(context, source) ||
        hir0_is_external_enum_constructor(context, source)) {
      size_t block = current_block;
      for (size_t ordinal = 0u; ordinal < source->argument_count;
           ordinal += 1u)
        block = hir0_emit_expression_layout_m2(
            context,
            context->frontend
                ->arguments[(size_t)source->first_argument + ordinal]
                .expression_index,
            block, statement_index, depth + 1u);
      return block;
    }
    return hir0_emit_call_layout_m2(context, expression, current_block,
                                    statement_index, depth + 1u);
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING) {
    size_t block = current_block;
    for (size_t ordinal = 0u; ordinal < source->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_frontend_interpolation_segment *segment =
          &context->frontend->interpolation_segments[
              (size_t)source->first_interpolation_segment + ordinal];
      if (segment->kind == W_SEED_FRONTEND_INTERPOLATION_EXPRESSION)
        block = hir0_emit_expression_layout_m2(
            context, segment->expression_index, block, statement_index,
            depth + 1u);
    }
    return block;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_BINARY) {
    size_t block = hir0_emit_expression_layout_m2(
        context, source->left, current_block, statement_index, depth + 1u);
    const w_seed_hir0_logical_operator logical =
        hir_logical_operator(source->operator_text);
    if (logical == W_SEED_HIR0_LOGICAL_NONE)
      return hir0_emit_expression_layout_m2(context, source->right, block,
                                            statement_index, depth + 1u);
    const size_t rhs_count = hir0_expression_block_count_m2(
        context, source->right, depth + 1u);
    const size_t branch = block;
    const size_t true_block = branch + 1u;
    const size_t false_block = logical == W_SEED_HIR0_LOGICAL_AND
                                   ? true_block + rhs_count
                                   : true_block + 1u;
    const size_t rhs_start = logical == W_SEED_HIR0_LOGICAL_AND
                                 ? true_block
                                 : false_block;
    const size_t join_block = logical == W_SEED_HIR0_LOGICAL_AND
                                  ? false_block + 1u
                                  : false_block + rhs_count;
    hir0_finish_block_m2(context, branch);
    context->output->terminators[branch] = (w_seed_hir0_terminator){
        .owner_block = (uint32_t)branch,
        .kind = W_SEED_HIR0_TERMINATOR_BRANCH,
        .ordinal = context->output->blocks[branch].instruction_count,
        .value_index = W_SEED_HIR0_NONE,
        .result_type = 3u,
        .target_block = (uint32_t)true_block,
        .else_block = (uint32_t)false_block,
        .first_edge_argument = W_SEED_HIR0_NONE,
        .edge_argument_count = 0u,
        .logical_operator = logical,
        .source_span = source->span};
    hir0_begin_block_m2(context, true_block);
    if (logical == W_SEED_HIR0_LOGICAL_OR) {
      hir0_set_jump_m2(context, true_block, join_block, source->span);
      hir0_begin_block_m2(context, false_block);
    }
    const size_t rhs_end = hir0_emit_expression_layout_m2(
        context, source->right, rhs_start, statement_index, depth + 1u);
    if (logical == W_SEED_HIR0_LOGICAL_AND) {
      hir0_begin_block_m2(context, false_block);
      hir0_set_jump_m2(context, false_block, join_block, source->span);
      hir0_set_jump_m2(context, rhs_end, join_block, source->span);
    } else {
      hir0_set_jump_m2(context, rhs_end, join_block, source->span);
    }
    hir0_begin_block_m2(context, join_block);
    w_seed_hir0_block *join = &context->output->blocks[join_block];
    join->first_block_argument = (uint32_t)*context->block_argument_index;
    join->block_argument_count = 1u;
    context->output->block_arguments[*context->block_argument_index] =
        (w_seed_hir0_block_argument){
            .owner_block = (uint32_t)join_block,
            .ordinal = 0u,
            .type_index = 3u,
            .source_span = source->span};
    *context->block_argument_index += 1u;
    return join_block;
  }
  return current_block;
}

static uint32_t hir0_emit_value_m2(
    hir0_emit_context *context, uint32_t expression,
    w_seed_hir0_value_owner_kind owner_kind, uint32_t owner_index,
    uint32_t owner_ordinal, size_t current_block, size_t depth) {
  if (context == NULL || expression == W_SEED_FRONTEND_NONE ||
      depth > W_SEED_HIR0_MAX_NESTING)
    return W_SEED_HIR0_NONE;
  const w_seed_frontend_expression *source =
      &context->frontend->expressions[expression];
  if (source->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH)
    return hir0_emit_value_m2(context, source->task_call_expression, owner_kind,
                              owner_index, owner_ordinal, current_block,
                              depth + 1u);
  if (source->kind == W_SEED_FRONTEND_EXPR_AWAIT)
    return hir0_emit_value_m2(context, source->left, owner_kind, owner_index,
                              owner_ordinal, current_block, depth + 1u);
  if (context->loop_active &&
      source->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
      source->resolved_parameter_ordinal == W_SEED_FRONTEND_NONE &&
      hir0_loop_root_ordinal(context, source->resolved_binding_statement) !=
          SIZE_MAX &&
      (current_block == context->loop_header_block ||
       current_block == context->loop_body_block ||
       current_block == context->loop_exit_block)) {
    const w_seed_hir0_block *header =
        &context->output->blocks[context->loop_header_block];
    const size_t root_ordinal = hir0_loop_root_ordinal(
        context, source->resolved_binding_statement);
    uint32_t binding_index = W_SEED_HIR0_NONE;
    bool block_argument = current_block == context->loop_header_block &&
                          !context->loop_post_test;
    if (context->loop_post_test &&
        current_block == context->loop_header_block) {
      uint32_t source_binding = W_SEED_HIR0_NONE;
      if (!root_binding_index_for_statement(
              context->frontend, context->frontend_result, context->function,
              source->resolved_binding_statement, &source_binding) ||
          !binding_index_for_statement(
              context->frontend, context->frontend_result, context->function,
              context->statement_index, source->resolved_binding_statement,
              &binding_index))
        return W_SEED_HIR0_NONE;
      block_argument = binding_index == source_binding;
    }
    if (current_block == context->loop_body_block ||
        current_block == context->loop_exit_block) {
      uint32_t source_binding = W_SEED_HIR0_NONE;
      if (!root_binding_index_for_statement(
              context->frontend, context->frontend_result, context->function,
              source->resolved_binding_statement, &source_binding) ||
          !binding_index_for_statement(
              context->frontend, context->frontend_result, context->function,
              context->statement_index, source->resolved_binding_statement,
              &binding_index))
        return W_SEED_HIR0_NONE;
      if (current_block == context->loop_body_block) {
        block_argument = !context->loop_post_test &&
                         binding_index == source_binding;
      } else {
        if (binding_index >= context->counts->bindings)
          return W_SEED_HIR0_NONE;
        block_argument =
            binding_index == source_binding ||
            context->output->bindings[binding_index].owner_block ==
                context->loop_body_block;
      }
    }
    const uint32_t result = (uint32_t)*context->value_index;
    context->output->values[*context->value_index] = (w_seed_hir0_value){
        .kind = block_argument ? W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ
                               : W_SEED_HIR0_VALUE_BINDING_READ,
        .owner_kind = owner_kind,
        .owner_index = owner_index,
        .owner_ordinal = owner_ordinal,
        .type_index = W_SEED_HIR0_TYPE_I64,
        .binding_index = block_argument ? W_SEED_HIR0_NONE : binding_index,
        .parameter_index = W_SEED_HIR0_NONE,
        .call_index = W_SEED_HIR0_NONE,
        .left_value = W_SEED_HIR0_NONE,
        .right_value = W_SEED_HIR0_NONE,
        .first_interpolation_segment = W_SEED_HIR0_NONE,
        .interpolation_segment_count = 0u,
        .first_enum_payload = 0u,
        .enum_payload_count = 0u,
        .pattern_capture_index = W_SEED_HIR0_NONE,
        .binary_operator = W_SEED_HIR0_BINARY_ADD,
        .unary_operator = W_SEED_HIR0_UNARY_NOT,
        .block_argument_index = block_argument
                                    ? header->first_block_argument +
                                          (uint32_t)root_ordinal
                                    : W_SEED_HIR0_NONE,
        .integer_value = 0,
        .unsigned_integer_value = 0u,
        .bool_value = false,
        .byte_offset = 0u,
        .byte_count = 0u,
        .source_span = source->span,
        .external_module_index = W_SEED_HIR0_NONE,
        .external_symbol_index = W_SEED_HIR0_NONE,
        .member_name = {0u, 0u},
        .enum_index = W_SEED_HIR0_NONE,
        .enum_case_index = W_SEED_HIR0_NONE};
    *context->value_index += 1u;
    return result;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_IF) {
    const w_seed_hir0_block *block = &context->output->blocks[current_block];
    const uint32_t result = (uint32_t)*context->value_index;
    context->output->values[*context->value_index] = (w_seed_hir0_value){
        .kind = W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ,
        .owner_kind = owner_kind,
        .owner_index = owner_index,
        .owner_ordinal = owner_ordinal,
        .type_index = hir_type_from_frontend(
            context->frontend, context->frontend_result, source->inferred_type),
        .binding_index = W_SEED_HIR0_NONE,
        .parameter_index = W_SEED_HIR0_NONE,
        .call_index = W_SEED_HIR0_NONE,
        .left_value = W_SEED_HIR0_NONE,
        .right_value = W_SEED_HIR0_NONE,
        .first_interpolation_segment = W_SEED_HIR0_NONE,
        .interpolation_segment_count = 0u,
        .binary_operator = W_SEED_HIR0_BINARY_ADD,
        .unary_operator = W_SEED_HIR0_UNARY_NOT,
        .block_argument_index = block->block_argument_count == 1u
                                    ? block->first_block_argument
                                    : W_SEED_HIR0_NONE,
        .integer_value = 0,
        .bool_value = false,
        .byte_offset = 0u,
        .byte_count = 0u,
        .source_span = source->span};
    *context->value_index += 1u;
    return result;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS)
    return hir0_emit_value_m2(context, source->left, owner_kind, owner_index,
                              owner_ordinal, current_block, depth + 1u);

  if (source->kind == W_SEED_FRONTEND_EXPR_BINARY &&
      hir_logical_operator(source->operator_text) !=
          W_SEED_HIR0_LOGICAL_NONE) {
    const w_seed_hir0_block *block = &context->output->blocks[current_block];
    const uint32_t result = (uint32_t)*context->value_index;
    context->output->values[*context->value_index] = (w_seed_hir0_value){
        .kind = W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ,
        .owner_kind = owner_kind,
        .owner_index = owner_index,
        .owner_ordinal = owner_ordinal,
        .type_index = 3u,
        .binding_index = W_SEED_HIR0_NONE,
        .parameter_index = W_SEED_HIR0_NONE,
        .call_index = W_SEED_HIR0_NONE,
        .left_value = W_SEED_HIR0_NONE,
        .right_value = W_SEED_HIR0_NONE,
        .first_interpolation_segment = W_SEED_HIR0_NONE,
        .interpolation_segment_count = 0u,
        .binary_operator = W_SEED_HIR0_BINARY_ADD,
        .unary_operator = W_SEED_HIR0_UNARY_NOT,
        .block_argument_index = block->block_argument_count == 1u
                                    ? block->first_block_argument
                                    : W_SEED_HIR0_NONE,
        .integer_value = 0,
        .bool_value = false,
        .byte_offset = 0u,
        .byte_count = 0u,
        .source_span = source->span};
    *context->value_index += 1u;
    return result;
  }

  if (source->kind == W_SEED_FRONTEND_EXPR_CALL &&
      hir0_is_local_enum_constructor(context, source)) {
    const w_seed_frontend_expression *callee =
        &context->frontend->expressions[source->left];
    const uint32_t first_payload = (uint32_t)*context->enum_payload_index;
    size_t payload_block = current_block;
    for (size_t ordinal = 0u; ordinal < source->argument_count;
         ordinal += 1u) {
      const w_seed_frontend_argument *argument =
          &context->frontend
               ->arguments[(size_t)source->first_argument + ordinal];
      payload_block = hir0_expression_layout_end_m2(
          context, argument->expression_index, payload_block, depth + 1u);
      const uint32_t payload_index =
          (uint32_t)*context->enum_payload_index;
      w_seed_hir0_enum_payload *payload =
          &context->output->enum_payloads[*context->enum_payload_index];
      if (argument->expression_index >=
          context->frontend_result->written.expressions)
        return W_SEED_HIR0_NONE;
      const w_seed_frontend_expression *payload_expression =
          &context->frontend->expressions[argument->expression_index];
      *payload = (w_seed_hir0_enum_payload){
          .owner_value = W_SEED_HIR0_NONE,
          .ordinal = (uint32_t)ordinal,
          .parameter_ordinal = argument->resolved_parameter_ordinal,
          .value_index = W_SEED_HIR0_NONE,
          .type_index = hir_type_from_frontend(
              context->frontend, context->frontend_result,
              payload_expression->inferred_type),
          .source_span = argument->span};
      *context->enum_payload_index += 1u;
      payload->value_index = hir0_emit_value_m2(
          context, argument->expression_index,
          W_SEED_HIR0_VALUE_OWNER_ENUM_PAYLOAD, payload_index, 0u,
          payload_block, depth + 1u);
    }
    const uint32_t result = (uint32_t)*context->value_index;
    context->output->values[*context->value_index] = (w_seed_hir0_value){
        .kind = W_SEED_HIR0_VALUE_ENUM_CASE,
        .owner_kind = owner_kind,
        .owner_index = owner_index,
        .owner_ordinal = owner_ordinal,
        .type_index = hir_type_from_frontend(
            context->frontend, context->frontend_result, source->inferred_type),
        .binding_index = W_SEED_HIR0_NONE,
        .parameter_index = W_SEED_HIR0_NONE,
        .call_index = W_SEED_HIR0_NONE,
        .left_value = W_SEED_HIR0_NONE,
        .right_value = W_SEED_HIR0_NONE,
        .first_interpolation_segment = W_SEED_HIR0_NONE,
        .interpolation_segment_count = 0u,
        .first_enum_payload = first_payload,
        .enum_payload_count = source->argument_count,
        .binary_operator = W_SEED_HIR0_BINARY_ADD,
        .unary_operator = W_SEED_HIR0_UNARY_NOT,
        .block_argument_index = W_SEED_HIR0_NONE,
        .integer_value = 0,
        .bool_value = false,
        .byte_offset = 0u,
        .byte_count = 0u,
        .source_span = source->span,
        .external_module_index = W_SEED_HIR0_NONE,
        .external_symbol_index = W_SEED_HIR0_NONE,
        .member_name = {0u, 0u},
        .enum_index = callee->enum_index,
        .enum_case_index = callee->enum_case_index};
    for (size_t ordinal = 0u; ordinal < source->argument_count;
         ordinal += 1u)
      context->output->enum_payloads[(size_t)first_payload + ordinal]
          .owner_value = result;
    *context->value_index += 1u;
    return result;
  }

  if (source->kind == W_SEED_FRONTEND_EXPR_CALL &&
      hir0_is_external_enum_constructor(context, source)) {
    const w_seed_frontend_argument *argument =
        &context->frontend
             ->arguments[(size_t)source->first_argument];
    const size_t payload_block = hir0_expression_layout_end_m2(
        context, argument->expression_index, current_block, depth + 1u);
    const uint32_t payload = hir0_emit_value_m2(
        context, argument->expression_index,
        W_SEED_HIR0_VALUE_OWNER_EXTERNAL_ENUM_CASE, W_SEED_HIR0_NONE, 0u,
        payload_block, depth + 1u);
    const uint32_t result = (uint32_t)*context->value_index;
    w_seed_hir0_value *target = &context->output->values[*context->value_index];
    *target = (w_seed_hir0_value){
        .kind = W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE,
        .owner_kind = owner_kind,
        .owner_index = owner_index,
        .owner_ordinal = owner_ordinal,
        .type_index = hir_type_from_frontend(
            context->frontend, context->frontend_result, source->inferred_type),
        .binding_index = W_SEED_HIR0_NONE,
        .parameter_index = W_SEED_HIR0_NONE,
        .call_index = W_SEED_HIR0_NONE,
        .left_value = payload,
        .right_value = W_SEED_HIR0_NONE,
        .first_interpolation_segment = W_SEED_HIR0_NONE,
        .interpolation_segment_count = 0u,
        .binary_operator = W_SEED_HIR0_BINARY_ADD,
        .unary_operator = W_SEED_HIR0_UNARY_NOT,
        .block_argument_index = W_SEED_HIR0_NONE,
        .integer_value = 0,
        .bool_value = false,
        .byte_offset = 0u,
        .byte_count = 0u,
        .source_span = source->span,
        .external_module_index = 0u,
        .external_symbol_index = 5u,
        .member_name = context->output->external_symbols[5].name,
        .enum_index = W_SEED_HIR0_NONE,
        .enum_case_index = W_SEED_HIR0_NONE};
    if (payload != W_SEED_HIR0_NONE)
      context->output->values[payload].owner_index = result;
    *context->value_index += 1u;
    return result;
  }

  if (source->kind == W_SEED_FRONTEND_EXPR_CALL) {
    const uint32_t call_index =
        hir0_find_call_m2(context, expression, current_block);
    const uint32_t result = (uint32_t)*context->value_index;
    const uint32_t result_type = call_index == W_SEED_HIR0_NONE
                                     ? W_SEED_HIR0_NONE
                                     : context->output->calls[call_index]
                                           .result_type;
    context->output->values[*context->value_index] = (w_seed_hir0_value){
        .kind = W_SEED_HIR0_VALUE_CALL_RESULT,
        .owner_kind = owner_kind,
        .owner_index = owner_index,
        .owner_ordinal = owner_ordinal,
        .type_index = result_type,
        .binding_index = W_SEED_HIR0_NONE,
        .parameter_index = W_SEED_HIR0_NONE,
        .call_index = call_index,
        .left_value = W_SEED_HIR0_NONE,
        .right_value = W_SEED_HIR0_NONE,
        .first_interpolation_segment = W_SEED_HIR0_NONE,
        .interpolation_segment_count = 0u,
        .binary_operator = W_SEED_HIR0_BINARY_ADD,
        .unary_operator = W_SEED_HIR0_UNARY_NOT,
        .block_argument_index = W_SEED_HIR0_NONE,
        .integer_value = 0,
        .bool_value = false,
        .byte_offset = 0u,
        .byte_count = 0u,
        .source_span = source->span};
    *context->value_index += 1u;
    return result;
  }

  uint32_t left = W_SEED_HIR0_NONE;
  uint32_t right = W_SEED_HIR0_NONE;
  uint32_t first_segment = W_SEED_HIR0_NONE;
  if (source->kind == W_SEED_FRONTEND_EXPR_UNARY) {
    left = hir0_emit_value_m2(
        context, source->left, W_SEED_HIR0_VALUE_OWNER_UNARY,
        W_SEED_HIR0_NONE, 0u,
        current_block, depth + 1u);
    const uint32_t result = (uint32_t)*context->value_index;
    w_seed_hir0_value *target = &context->output->values[*context->value_index];
    const bool numeric_negate = text_is(source->operator_text, "-");
    *target = (w_seed_hir0_value){
        .kind = numeric_negate ? W_SEED_HIR0_VALUE_UNARY_I64
                               : W_SEED_HIR0_VALUE_UNARY_BOOL,
        .owner_kind = owner_kind,
        .owner_index = owner_index,
        .owner_ordinal = owner_ordinal,
        .type_index = numeric_negate ? 2u : 3u,
        .binding_index = W_SEED_HIR0_NONE,
        .parameter_index = W_SEED_HIR0_NONE,
        .call_index = W_SEED_HIR0_NONE,
        .left_value = left,
        .right_value = W_SEED_HIR0_NONE,
        .first_interpolation_segment = W_SEED_HIR0_NONE,
        .interpolation_segment_count = 0u,
        .binary_operator = W_SEED_HIR0_BINARY_ADD,
        .unary_operator = numeric_negate ? W_SEED_HIR0_UNARY_NEGATE
                                         : W_SEED_HIR0_UNARY_NOT,
        .block_argument_index = W_SEED_HIR0_NONE,
        .integer_value = 0,
        .bool_value = false,
        .byte_offset = 0u,
        .byte_count = 0u,
        .source_span = source->span};
    if (left != W_SEED_HIR0_NONE)
      context->output->values[left].owner_index = result;
    *context->value_index += 1u;
    return result;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_BINARY) {
    const bool usize_count_comparison =
        hir0_usize_count_comparison_source_ok(context, source);
    left = hir0_emit_value_m2(
        context, source->left, W_SEED_HIR0_VALUE_OWNER_BINARY,
        W_SEED_HIR0_NONE, 0u,
        current_block, depth + 1u);
    right = hir0_emit_value_m2(
        context, source->right, W_SEED_HIR0_VALUE_OWNER_BINARY,
        W_SEED_HIR0_NONE, 1u,
        current_block, depth + 1u);
    const uint32_t result = (uint32_t)*context->value_index;
    w_seed_hir0_value *target = &context->output->values[*context->value_index];
    *target = (w_seed_hir0_value){
        .kind = usize_count_comparison
                    ? W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON
                    : W_SEED_HIR0_VALUE_BINARY_I64,
        .owner_kind = owner_kind,
        .owner_index = owner_index,
        .owner_ordinal = owner_ordinal,
        .type_index = hir_type_from_frontend(
            context->frontend, context->frontend_result, source->inferred_type),
        .binding_index = W_SEED_HIR0_NONE,
        .parameter_index = W_SEED_HIR0_NONE,
        .call_index = W_SEED_HIR0_NONE,
        .left_value = left,
        .right_value = right,
        .first_interpolation_segment = W_SEED_HIR0_NONE,
        .interpolation_segment_count = 0u,
        .binary_operator = hir_binary_operator(source->operator_text),
        .unary_operator = W_SEED_HIR0_UNARY_NOT,
        .block_argument_index = W_SEED_HIR0_NONE,
        .integer_value = 0,
        .bool_value = false,
        .byte_offset = 0u,
        .byte_count = 0u,
        .source_span = source->span};
    if (left != W_SEED_HIR0_NONE) context->output->values[left].owner_index = result;
    if (right != W_SEED_HIR0_NONE)
      context->output->values[right].owner_index = result;
    *context->value_index += 1u;
    return result;
  }
  if (source->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING) {
    first_segment = (uint32_t)*context->interpolation_segment_index;
    size_t segment_block = current_block;
    for (size_t ordinal = 0u; ordinal < source->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_frontend_interpolation_segment *source_segment =
          &context->frontend->interpolation_segments[
              (size_t)source->first_interpolation_segment + ordinal];
      const uint32_t target_segment_index =
          (uint32_t)*context->interpolation_segment_index;
      w_seed_hir0_interpolation_segment *target_segment =
          &context->output->interpolation_segments[
              *context->interpolation_segment_index];
      *target_segment = (w_seed_hir0_interpolation_segment){
          .kind = source_segment->kind == W_SEED_FRONTEND_INTERPOLATION_TEXT
                      ? W_SEED_HIR0_INTERPOLATION_TEXT
                      : W_SEED_HIR0_INTERPOLATION_VALUE,
          .owner_value = W_SEED_HIR0_NONE,
          .ordinal = (uint32_t)ordinal,
          .value_index = W_SEED_HIR0_NONE,
          .byte_offset = 0u,
          .byte_count = 0u,
          .source_span = source_segment->span};
      *context->interpolation_segment_index += 1u;
      if (source_segment->kind == W_SEED_FRONTEND_INTERPOLATION_TEXT) {
        const uint8_t *bytes = source_segment->const_byte_count == 0u
                                   ? NULL
                                   : context->frontend->const_bytes +
                                         source_segment->const_byte_offset;
        append_bytes_unchecked(bytes, source_segment->const_byte_count,
                               context->output->value_bytes, context->value_offset,
                               &target_segment->byte_offset,
                               &target_segment->byte_count);
      } else {
        const size_t segment_end = hir0_expression_layout_end_m2(
            context, source_segment->expression_index, segment_block,
            depth + 1u);
        target_segment->value_index = hir0_emit_value_m2(
            context, source_segment->expression_index,
            W_SEED_HIR0_VALUE_OWNER_INTERPOLATION_SEGMENT,
            target_segment_index, 0u, segment_end, depth + 1u);
        segment_block = segment_end;
      }
    }
    const uint32_t result = (uint32_t)*context->value_index;
    w_seed_hir0_value *target = &context->output->values[*context->value_index];
    *target = (w_seed_hir0_value){
        .kind = W_SEED_HIR0_VALUE_INTERPOLATED_STRING,
        .owner_kind = owner_kind,
        .owner_index = owner_index,
        .owner_ordinal = owner_ordinal,
        .type_index = 1u,
        .binding_index = W_SEED_HIR0_NONE,
        .parameter_index = W_SEED_HIR0_NONE,
        .call_index = W_SEED_HIR0_NONE,
        .left_value = W_SEED_HIR0_NONE,
        .right_value = W_SEED_HIR0_NONE,
        .first_interpolation_segment = first_segment,
        .interpolation_segment_count = source->interpolation_segment_count,
        .binary_operator = W_SEED_HIR0_BINARY_ADD,
        .unary_operator = W_SEED_HIR0_UNARY_NOT,
        .block_argument_index = W_SEED_HIR0_NONE,
        .integer_value = 0,
        .bool_value = false,
        .byte_offset = 0u,
        .byte_count = 0u,
        .source_span = source->span};
    for (size_t ordinal = 0u; ordinal < source->interpolation_segment_count;
         ordinal += 1u)
      context->output->interpolation_segments[(size_t)first_segment + ordinal]
          .owner_value = result;
    *context->value_index += 1u;
    return result;
  }

  if (source->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE) {
    const uint32_t result = (uint32_t)*context->value_index;
    w_seed_hir0_value *target =
        &context->output->values[*context->value_index];
    const bool local_case = source->enum_index != W_SEED_FRONTEND_NONE;
    *target = (w_seed_hir0_value){
        .kind = local_case ? W_SEED_HIR0_VALUE_ENUM_CASE
                           : W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE,
        .owner_kind = owner_kind,
        .owner_index = owner_index,
        .owner_ordinal = owner_ordinal,
        .type_index = hir_type_from_frontend(
            context->frontend, context->frontend_result, source->inferred_type),
        .binding_index = W_SEED_HIR0_NONE,
        .parameter_index = W_SEED_HIR0_NONE,
        .call_index = W_SEED_HIR0_NONE,
        .left_value = W_SEED_HIR0_NONE,
        .right_value = W_SEED_HIR0_NONE,
        .first_interpolation_segment = W_SEED_HIR0_NONE,
        .interpolation_segment_count = 0u,
        .binary_operator = W_SEED_HIR0_BINARY_ADD,
        .unary_operator = W_SEED_HIR0_UNARY_NOT,
        .block_argument_index = W_SEED_HIR0_NONE,
        .integer_value = 0,
        .bool_value = false,
        .byte_offset = 0u,
        .byte_count = 0u,
        .source_span = source->span,
        .external_module_index = local_case
                                     ? W_SEED_HIR0_NONE
                                     : source->resolved_external_module_index,
        .external_symbol_index = local_case
                                    ? W_SEED_HIR0_NONE
                                    : source->resolved_external_symbol_index,
        .member_name = local_case
                           ? (w_seed_hir0_text){0u, 0u}
                           : context->output->external_symbols[
                                 source->resolved_external_symbol_index]
                                 .name,
        .enum_index = local_case ? source->enum_index : W_SEED_HIR0_NONE,
        .enum_case_index =
            local_case ? source->enum_case_index : W_SEED_HIR0_NONE};
    *context->value_index += 1u;
    return result;
  }

  if (source->kind == W_SEED_FRONTEND_EXPR_MEMBER) {
    const uint32_t receiver = hir0_emit_value_m2(
        context, source->left, W_SEED_HIR0_VALUE_OWNER_EXTERNAL_MEMBER,
        W_SEED_HIR0_NONE, 0u, current_block, depth + 1u);
    const uint32_t result = (uint32_t)*context->value_index;
    w_seed_hir0_value *target =
        &context->output->values[*context->value_index];
    *target = (w_seed_hir0_value){
        .kind = W_SEED_HIR0_VALUE_EXTERNAL_MEMBER,
        .owner_kind = owner_kind,
        .owner_index = owner_index,
        .owner_ordinal = owner_ordinal,
        .type_index = hir_type_from_frontend(
            context->frontend, context->frontend_result, source->inferred_type),
        .binding_index = W_SEED_HIR0_NONE,
        .parameter_index = W_SEED_HIR0_NONE,
        .call_index = W_SEED_HIR0_NONE,
        .left_value = receiver,
        .right_value = W_SEED_HIR0_NONE,
        .first_interpolation_segment = W_SEED_HIR0_NONE,
        .interpolation_segment_count = 0u,
        .binary_operator = W_SEED_HIR0_BINARY_ADD,
        .unary_operator = W_SEED_HIR0_UNARY_NOT,
        .block_argument_index = W_SEED_HIR0_NONE,
        .integer_value = 0,
        .bool_value = false,
        .byte_offset = 0u,
        .byte_count = 0u,
        .source_span = source->span,
        .external_module_index = source->resolved_external_module_index,
        .external_symbol_index = source->resolved_external_symbol_index,
        .member_name = context->output->external_symbols[
            source->resolved_external_symbol_index].name,
        .enum_index = W_SEED_HIR0_NONE,
        .enum_case_index = W_SEED_HIR0_NONE};
    if (receiver != W_SEED_HIR0_NONE)
      context->output->values[receiver].owner_index = result;
    *context->value_index += 1u;
    return result;
  }

  const uint32_t result = (uint32_t)*context->value_index;
  w_seed_hir0_value *target = &context->output->values[*context->value_index];
  *target = (w_seed_hir0_value){
      .kind = W_SEED_HIR0_VALUE_CONST_STRING,
      .owner_kind = owner_kind,
      .owner_index = owner_index,
      .owner_ordinal = owner_ordinal,
      .type_index = 1u,
      .binding_index = W_SEED_HIR0_NONE,
      .parameter_index = W_SEED_HIR0_NONE,
      .call_index = W_SEED_HIR0_NONE,
      .left_value = W_SEED_HIR0_NONE,
      .right_value = W_SEED_HIR0_NONE,
      .first_interpolation_segment = W_SEED_HIR0_NONE,
      .interpolation_segment_count = 0u,
      .binary_operator = W_SEED_HIR0_BINARY_ADD,
      .unary_operator = W_SEED_HIR0_UNARY_NOT,
      .block_argument_index = W_SEED_HIR0_NONE,
      .integer_value = 0,
      .bool_value = false,
      .byte_offset = 0u,
      .byte_count = 0u,
      .source_span = source->span};
  *context->value_index += 1u;
  if (source->kind == W_SEED_FRONTEND_EXPR_STRING) {
    const uint8_t *bytes = source->const_byte_count == 0u
                               ? NULL
                               : context->frontend->const_bytes +
                                     source->const_byte_offset;
    append_bytes_unchecked(bytes, source->const_byte_count,
                           context->output->value_bytes, context->value_offset,
                           &target->byte_offset, &target->byte_count);
  } else if (source->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER) {
    target->type_index = hir_type_from_frontend(
        context->frontend, context->frontend_result, source->inferred_type);
    if (source->resolved_pattern_capture != W_SEED_FRONTEND_NONE) {
      const w_seed_frontend_pattern_capture *capture =
          &context->frontend->pattern_captures[source->resolved_pattern_capture];
      target->kind = W_SEED_HIR0_VALUE_PATTERN_CAPTURE_READ;
      target->pattern_capture_index = W_SEED_HIR0_NONE;
      for (size_t edge = 0u; edge < *context->switch_edge_index; edge += 1u) {
        const w_seed_hir0_switch_edge *item = &context->output->switch_edges[edge];
        if (item->target_block == current_block) {
          target->pattern_capture_index = item->first_capture + capture->ordinal;
          break;
        }
      }
    } else if (source->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE) {
      target->kind = W_SEED_HIR0_VALUE_PARAMETER_READ;
      target->parameter_index =
          context->frontend->functions[context->function].first_parameter +
          source->resolved_parameter_ordinal;
    } else {
      target->kind = W_SEED_HIR0_VALUE_BINDING_READ;
      (void)binding_index_for_statement(
          context->frontend, context->frontend_result, context->function,
          context->statement_index,
          source->resolved_binding_statement, &target->binding_index);
    }
  } else if (source->kind == W_SEED_FRONTEND_EXPR_INTEGER) {
    if (frontend_expression_is_usize(context->frontend,
                                     context->frontend_result, source)) {
      target->kind = W_SEED_HIR0_VALUE_CONST_USIZE;
      target->type_index = hir_type_from_frontend(
          context->frontend, context->frontend_result, source->inferred_type);
      (void)frontend_integer_u64(source, &target->unsigned_integer_value);
    } else {
      target->kind = W_SEED_HIR0_VALUE_CONST_I64;
      target->type_index = 2u;
      (void)frontend_integer_i64(source, &target->integer_value);
    }
  } else if (source->kind == W_SEED_FRONTEND_EXPR_BOOL) {
    target->kind = W_SEED_HIR0_VALUE_CONST_BOOL;
    target->type_index = 3u;
    target->bool_value = source->bool_value;
  } else {
    target->kind = W_SEED_HIR0_VALUE_CONST_STRING;
  }
  return result;
}


static void emit_records(const w_seed_hir0_input *input,
                         const w_seed_hir0_counts *counts,
                         w_seed_hir0_output *output) {
  const w_seed_frontend_output *frontend = input->frontend_output;
  const w_seed_frontend_result *frontend_result = input->frontend_result;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  size_t text_offset = 0u;
  size_t value_offset = 0u;
  zero_bytes(output->modules, counts->modules * sizeof(*output->modules));
  zero_bytes(output->identities,
             counts->identities * sizeof(*output->identities));
  zero_bytes(output->types, counts->types * sizeof(*output->types));
  zero_bytes(output->enums, counts->enums * sizeof(*output->enums));
  zero_bytes(output->enum_cases,
             counts->enum_cases * sizeof(*output->enum_cases));
  zero_bytes(output->enum_case_parameters,
             counts->enum_case_parameters *
                 sizeof(*output->enum_case_parameters));
  zero_bytes(output->enum_subset_members,
             counts->enum_subset_members *
                 sizeof(*output->enum_subset_members));
  zero_bytes(output->functions,
             counts->functions * sizeof(*output->functions));
  zero_bytes(output->parameters,
             counts->parameters * sizeof(*output->parameters));
  zero_bytes(output->blocks, counts->blocks * sizeof(*output->blocks));
  zero_bytes(output->block_arguments,
             counts->block_arguments * sizeof(*output->block_arguments));
  zero_bytes(output->edge_arguments,
             counts->edge_arguments * sizeof(*output->edge_arguments));
  zero_bytes(output->switch_edges,
             counts->switch_edges * sizeof(*output->switch_edges));
  zero_bytes(output->switch_captures,
             counts->switch_captures * sizeof(*output->switch_captures));
  zero_bytes(output->instructions,
             counts->instructions * sizeof(*output->instructions));
  zero_bytes(output->bindings,
             counts->bindings * sizeof(*output->bindings));
  zero_bytes(output->calls, counts->calls * sizeof(*output->calls));
  zero_bytes(output->host_parameters,
             counts->host_parameters * sizeof(*output->host_parameters));
  zero_bytes(output->arguments,
             counts->arguments * sizeof(*output->arguments));
  zero_bytes(output->enum_payloads,
             counts->enum_payloads * sizeof(*output->enum_payloads));
  zero_bytes(output->requirements,
             counts->requirements * sizeof(*output->requirements));
  zero_bytes(output->values, counts->values * sizeof(*output->values));
  zero_bytes(output->interpolation_segments,
             counts->interpolation_segments *
                 sizeof(*output->interpolation_segments));
  zero_bytes(output->terminators,
             counts->terminators * sizeof(*output->terminators));
  zero_bytes(output->entries, counts->entries * sizeof(*output->entries));
  zero_bytes(output->external_modules,
             counts->external_modules * sizeof(*output->external_modules));
  zero_bytes(output->external_symbols,
             counts->external_symbols * sizeof(*output->external_symbols));
  zero_bytes(output->text_bytes, counts->text_bytes);
  zero_bytes(output->value_bytes, counts->value_bytes);
  zero_bytes(output->receipt, counts->receipt_bytes);
  for (size_t value = 0u; value < counts->values; value += 1u) {
    output->values[value].external_module_index = W_SEED_HIR0_NONE;
    output->values[value].external_symbol_index = W_SEED_HIR0_NONE;
    output->values[value].member_name = (w_seed_hir0_text){0u, 0u};
    output->values[value].enum_index = W_SEED_HIR0_NONE;
    output->values[value].enum_case_index = W_SEED_HIR0_NONE;
  }
  output->types[0] = (w_seed_hir0_type){
      .kind = W_SEED_HIR0_TYPE_UNIT,
      .owner_module = W_SEED_HIR0_NONE,
      .name = {0u, 2u},
      .external_module_index = W_SEED_HIR0_NONE,
      .external_symbol_index = W_SEED_HIR0_NONE,
      .enum_index = W_SEED_HIR0_NONE,
      .lifecycle = W_SEED_HIR0_LIFECYCLE_UNKNOWN,
      .release_contract = W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN};
  output->types[1] = (w_seed_hir0_type){
      .kind = W_SEED_HIR0_TYPE_STRING,
      .owner_module = W_SEED_HIR0_NONE,
      .name = {2u, 6u},
      .external_module_index = W_SEED_HIR0_NONE,
      .external_symbol_index = W_SEED_HIR0_NONE,
      .enum_index = W_SEED_HIR0_NONE,
      .lifecycle = W_SEED_HIR0_LIFECYCLE_UNKNOWN,
      .release_contract = W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN};
  output->types[2] = (w_seed_hir0_type){
      .kind = W_SEED_HIR0_TYPE_I64,
      .owner_module = W_SEED_HIR0_NONE,
      .name = {8u, 3u},
      .external_module_index = W_SEED_HIR0_NONE,
      .external_symbol_index = W_SEED_HIR0_NONE,
      .enum_index = W_SEED_HIR0_NONE,
      .lifecycle = W_SEED_HIR0_LIFECYCLE_UNKNOWN,
      .release_contract = W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN};
  output->types[3] = (w_seed_hir0_type){
      .kind = W_SEED_HIR0_TYPE_BOOL,
      .owner_module = W_SEED_HIR0_NONE,
      .name = {11u, 4u},
      .external_module_index = W_SEED_HIR0_NONE,
      .external_symbol_index = W_SEED_HIR0_NONE,
      .enum_index = W_SEED_HIR0_NONE,
      .lifecycle = W_SEED_HIR0_LIFECYCLE_UNKNOWN,
      .release_contract = W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN};
  for (size_t type = 0u; type < counts->types; type += 1u) {
    output->types[type].enum_index = W_SEED_HIR0_NONE;
    output->types[type].first_subset_member = W_SEED_HIR0_NONE;
    output->types[type].subset_member_count = 0u;
  }
  /* output_capacity_ok proves these storage preconditions. */
  (void)memcpy(output->text_bytes, HIR0_UNIT_NAME, 2u);
  (void)memcpy(output->text_bytes + 2u, HIR0_STRING_NAME, 6u);
  (void)memcpy(output->text_bytes + 8u, HIR0_I64_NAME, 3u);
  (void)memcpy(output->text_bytes + 11u, HIR0_BOOL_NAME, 4u);
  const bool has_public_process = counts->external_symbols == 7u;
  const size_t usize_type_index =
      7u + (has_public_process
                ? counts->enums + counts->enum_subsets
                : 0u);
  if (has_public_process) {
    (void)memcpy(output->text_bytes + 15u, HIR0_USIZE_NAME, 5u);
    output->types[usize_type_index] = (w_seed_hir0_type){
        .kind = W_SEED_HIR0_TYPE_USIZE,
        .owner_module = W_SEED_HIR0_NONE,
        .name = {15u, 5u},
        .external_module_index = W_SEED_HIR0_NONE,
        .external_symbol_index = W_SEED_HIR0_NONE,
        .enum_index = W_SEED_HIR0_NONE,
        .first_subset_member = W_SEED_HIR0_NONE,
        .subset_member_count = 0u,
        .lifecycle = W_SEED_HIR0_LIFECYCLE_UNKNOWN,
        .release_contract = W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN};
  }
  text_offset = has_public_process ? 20u : 15u;
  for (size_t module = 0u; module < counts->external_modules; module += 1u) {
    const w_seed_frontend_external_module *source =
        &frontend_input->external_modules[module];
    w_seed_hir0_external_module *target = &output->external_modules[module];
    target->module_index = (uint32_t)module;
    append_text_unchecked(source->module_id, output->text_bytes, &text_offset,
                          &target->module_id);
    target->first_symbol = 0u;
    target->symbol_count = (uint32_t)counts->external_symbols;
    for (size_t symbol = 0u; symbol < source->symbol_count; symbol += 1u) {
      const w_seed_frontend_external_symbol *symbol_source =
          &source->symbols[symbol];
      w_seed_hir0_external_symbol *symbol_target =
          &output->external_symbols[symbol];
      symbol_target->module_index = (uint32_t)module;
      symbol_target->ordinal = (uint32_t)symbol;
      append_text_unchecked(symbol_source->name, output->text_bytes,
                            &text_offset, &symbol_target->name);
      symbol_target->kind =
          symbol_source->kind == W_SEED_FRONTEND_EXTERNAL_TYPE
              ? W_SEED_HIR0_EXTERNAL_TYPE
              : W_SEED_HIR0_EXTERNAL_VALUE;
      symbol_target->exported = symbol_source->exported;
      symbol_target->is_const = symbol_source->is_const;
      symbol_target->parameter_count =
          symbol_source->parameter_count > (size_t)UINT32_MAX
              ? 0u
              : (uint32_t)symbol_source->parameter_count;
      symbol_target->parameter_abi =
          counts->external_symbols == 7u && symbol == 5u
              ? W_SEED_HIR0_EXTERNAL_PARAMETER_PROCESS_FAILURE_I64
              : W_SEED_HIR0_EXTERNAL_PARAMETER_NONE;
      symbol_target->receiver_type = (w_seed_hir0_text){0u, 0u};
      symbol_target->return_type = symbol_target->name;
      if (symbol == 3u) {
        symbol_target->receiver_type = output->external_symbols[2].name;
        symbol_target->return_type = output->external_symbols[2].name;
      } else if (symbol == 4u) {
        symbol_target->receiver_type = output->external_symbols[0].name;
        symbol_target->return_type = (w_seed_hir0_text){11u, 4u};
      } else if (symbol == 5u) {
        symbol_target->receiver_type = output->external_symbols[2].name;
        symbol_target->return_type = output->external_symbols[2].name;
      } else if (symbol == 6u) {
        symbol_target->receiver_type = output->external_symbols[0].name;
        symbol_target->return_type = output->types[usize_type_index].name;
      }
    }
  }
  if (counts->external_modules != 0u)
    for (size_t symbol = 0u; symbol < 3u; symbol += 1u) {
      output->types[4u + symbol] = (w_seed_hir0_type){
          .kind = W_SEED_HIR0_TYPE_NOMINAL,
          .owner_module = W_SEED_HIR0_NONE,
          .name = output->external_symbols[symbol].name,
          .external_module_index = 0u,
          .external_symbol_index = (uint32_t)symbol,
          .enum_index = W_SEED_HIR0_NONE,
          .first_subset_member = W_SEED_HIR0_NONE,
          .subset_member_count = 0u,
          .lifecycle = W_SEED_HIR0_LIFECYCLE_UNKNOWN,
          .release_contract = W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN};
    }
  const size_t local_enum_type_base =
      4u + (counts->external_modules == 0u ? 0u : 3u);
  for (size_t enum_index = 0u; enum_index < counts->enums; enum_index += 1u) {
    const w_seed_frontend_enum *source = &frontend->enums[enum_index];
    w_seed_hir0_enum *target = &output->enums[enum_index];
    target->module_index = source->module_index;
    target->type_index = (uint32_t)(local_enum_type_base + enum_index);
    append_text_unchecked(source->name, output->text_bytes, &text_offset,
                          &target->name);
    target->first_case = source->first_case;
    target->case_count = source->case_count;
    target->source_span = source->span;
    output->types[target->type_index] = (w_seed_hir0_type){
        .kind = W_SEED_HIR0_TYPE_ENUM,
        .owner_module = source->module_index,
        .name = target->name,
        .external_module_index = W_SEED_HIR0_NONE,
        .external_symbol_index = W_SEED_HIR0_NONE,
        .enum_index = (uint32_t)enum_index,
        .first_subset_member = W_SEED_HIR0_NONE,
        .subset_member_count = 0u,
        .lifecycle = W_SEED_HIR0_LIFECYCLE_VALUE_COPY,
        .release_contract = W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN};
    for (size_t ordinal = 0u; ordinal < source->case_count; ordinal += 1u) {
      const size_t case_index = (size_t)source->first_case + ordinal;
      const w_seed_frontend_enum_case *case_source =
          &frontend->enum_cases[case_index];
      w_seed_hir0_enum_case *case_target = &output->enum_cases[case_index];
      case_target->owner_enum = (uint32_t)enum_index;
      case_target->ordinal = (uint32_t)ordinal;
      case_target->tag = (uint32_t)ordinal;
      case_target->first_payload = case_source->first_payload;
      case_target->payload_count = case_source->payload_count;
      append_text_unchecked(case_source->name, output->text_bytes, &text_offset,
                            &case_target->name);
      case_target->source_span = case_source->span;
      for (size_t payload_ordinal = 0u;
           payload_ordinal < case_source->payload_count;
           payload_ordinal += 1u) {
        const size_t payload_index =
            (size_t)case_source->first_payload + payload_ordinal;
        const w_seed_frontend_enum_case_parameter *payload_source =
            &frontend->enum_case_parameters[payload_index];
        w_seed_hir0_enum_case_parameter *payload_target =
            &output->enum_case_parameters[payload_index];
        payload_target->owner_case = (uint32_t)case_index;
        payload_target->ordinal = (uint32_t)payload_ordinal;
        payload_target->type_index = hir_type_from_frontend(
            frontend, frontend_result, payload_source->type_index);
        append_text_unchecked(payload_source->label, output->text_bytes,
                              &text_offset, &payload_target->label);
        payload_target->has_label = payload_source->has_label;
        payload_target->source_span = payload_source->span;
      }
    }
  }
  size_t subset_member_offset = 0u;
  for (size_t frontend_type_index = 0u;
       frontend_type_index < frontend_result->written.types;
       frontend_type_index += 1u) {
    const w_seed_frontend_type *source =
        &frontend->types[frontend_type_index];
    if (source->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET) continue;
    size_t canonical_index = 0u;
    size_t canonical_ordinal = 0u;
    if (!frontend_enum_subset_canonical(
            frontend, frontend_result, frontend_type_index, &canonical_index,
            &canonical_ordinal) || canonical_index != frontend_type_index)
      continue;
    const uint32_t type_index = hir_type_from_frontend(
        frontend, frontend_result, (uint32_t)frontend_type_index);
    const w_seed_frontend_enum *base = &frontend->enums[source->enum_base_index];
    w_seed_hir0_type *target = &output->types[type_index];
    target->kind = W_SEED_HIR0_TYPE_ENUM_SUBSET;
    target->owner_module = base->module_index;
    target->name = output->enums[source->enum_base_index].name;
    target->external_module_index = W_SEED_HIR0_NONE;
    target->external_symbol_index = W_SEED_HIR0_NONE;
    target->enum_index = source->enum_base_index;
    target->first_subset_member = (uint32_t)subset_member_offset;
    target->subset_member_count = source->subset_member_count;
    target->lifecycle = W_SEED_HIR0_LIFECYCLE_VALUE_COPY;
    target->release_contract = W_SEED_HIR0_RELEASE_CONTRACT_NONE;
    for (size_t ordinal = 0u; ordinal < source->subset_member_count;
         ordinal += 1u) {
      const size_t source_index =
          (size_t)source->first_subset_member + ordinal;
      w_seed_hir0_enum_subset_member *member =
          &output->enum_subset_members[subset_member_offset + ordinal];
      const w_seed_frontend_enum_subset_member *source_member =
          &frontend->enum_subset_members[source_index];
      *member = (w_seed_hir0_enum_subset_member){
          .owner_type = type_index,
          .ordinal = (uint32_t)ordinal,
          .enum_index = source_member->enum_base_index,
          .enum_case_index = source_member->enum_case_index,
          .source_span = source_member->source_span};
    }
    subset_member_offset += source->subset_member_count;
    (void)canonical_ordinal;
  }
  /* Module, function, and entry identities have deterministic dense ranges. */
  for (size_t module = 0u; module < counts->modules; module += 1u) {
    const w_seed_frontend_module *source = &frontend->modules[module];
    const size_t document_index = source->document_index;
    const w_seed_frontend_document *document =
        &frontend_input->documents[document_index];
    w_seed_hir0_module *target = &output->modules[module];
    target->module_index = (uint32_t)module;
    target->identity_index = (uint32_t)module;
    append_text_unchecked(source->source_id, output->text_bytes, &text_offset,
                          &target->source_id);
    append_text_unchecked(source->module_id, output->text_bytes, &text_offset,
                          &target->module_id);
    append_text_unchecked(source->local_module_name, output->text_bytes,
                          &text_offset, &target->local_module_name);
    (void)source_digest(document, target->source_sha256);
    target->source_span = source->span;
    target->source_length = document->source->bytes.length;
    target->first_function = source->first_function;
    target->function_count = source->function_count;
    target->first_entry = source->first_entry;
    target->entry_count = source->entry_count;
    w_seed_hir0_identity *identity = &output->identities[module];
    *identity = (w_seed_hir0_identity){
        .kind = W_SEED_HIR0_IDENTITY_MODULE,
        .owner_module = W_SEED_HIR0_NONE,
        .target_index = (uint32_t)module,
        .name = target->module_id,
        .first_parameter = W_SEED_HIR0_NONE,
        .parameter_count = 0u,
        .first_requirement = W_SEED_HIR0_NONE,
        .requirement_count = 0u,
        .return_type = W_SEED_HIR0_NONE,
        .is_const = false,
        .profile = {0u, 0u}};
  }
  const size_t function_identity_base = counts->modules;
  for (size_t function = 0u; function < counts->functions; function += 1u) {
    const w_seed_frontend_function *source = &frontend->functions[function];
    w_seed_hir0_function *target = &output->functions[function];
    const size_t module = source->module_index;
    const w_seed_frontend_module *module_source = &frontend->modules[module];
    const size_t document_index = module_source->document_index;
    target->module_index = (uint32_t)module;
    target->identity_index = (uint32_t)(function_identity_base + function);
    append_text_unchecked(source->name, output->text_bytes, &text_offset,
                          &target->name);
    target->exported = source->exported;
    target->source_span = source->span;
    target->body_span = source->body_span;
    target->return_type = hir_type_from_frontend(
        frontend, frontend_result, source->return_type);
    target->first_parameter = source->first_parameter;
    target->parameter_count = source->parameter_count;
    target->first_block = (uint32_t)function;
    target->block_count = 1u;
    target->is_const = source->is_const;
    target->is_async = source->is_async;
    target->is_throws = source->is_throws;
    target->is_unsafe = source->is_unsafe;
    target->has_borrow_clause = source->has_borrow_clause;
    target->is_anonymous_entry = source->is_anonymous_entry;
    /* Start conservative; the whole-body proof publishes the final facts. */
    target->suspension = W_SEED_HIR0_SUSPENSION_MAY;
    target->direct_entry = W_SEED_HIR0_DIRECT_ENTRY_ABSENT;
    output->identities[function_identity_base + function] =
        (w_seed_hir0_identity){
            .kind = W_SEED_HIR0_IDENTITY_FUNCTION,
            .owner_module = (uint32_t)module,
            .target_index = (uint32_t)function,
            .name = target->name,
            .first_parameter = target->first_parameter,
            .parameter_count = target->parameter_count,
            .first_requirement = W_SEED_HIR0_NONE,
            .requirement_count = 0u,
            .return_type = target->return_type,
            .is_const = source->is_const,
            .profile = {0u, 0u}};
    (void)document_index;
  }
  size_t host_parameter_offset = 0u;
  size_t requirement_offset = 0u;
  const size_t host_identity_base = counts->modules + counts->functions +
                                    counts->entries;
  for (size_t host = 0u; host < frontend_input->host_scope->symbol_count;
       host += 1u) {
    const w_seed_frontend_host_prelude_symbol *source =
        &frontend_input->host_scope->symbols[host];
    const uint32_t identity_index =
        (uint32_t)(host_identity_base + host);
    w_seed_hir0_identity *identity = &output->identities[identity_index];
    append_text_unchecked(source->name, output->text_bytes, &text_offset,
                          &identity->name);
    append_text_unchecked(frontend_input->host_scope->profile,
                          output->text_bytes, &text_offset, &identity->profile);
    identity->kind = W_SEED_HIR0_IDENTITY_HOST_PRELUDE;
    identity->owner_module = W_SEED_HIR0_NONE;
    identity->target_index = (uint32_t)host;
    identity->first_parameter = (uint32_t)host_parameter_offset;
    identity->parameter_count = (uint32_t)source->parameter_count;
    identity->first_requirement = (uint32_t)requirement_offset;
    identity->requirement_count = (uint32_t)source->requirement_count;
    identity->return_type = 0u;
    identity->is_const = source->is_const;
    const size_t host_parameter_base = host_parameter_offset;
    for (size_t parameter = 0u; parameter < source->parameter_count;
         parameter += 1u) {
      const w_seed_frontend_external_parameter *value =
          &source->parameters[parameter];
      w_seed_hir0_host_parameter *target =
          &output->host_parameters[host_parameter_base + parameter];
      target->owner_identity = identity_index;
      target->ordinal = (uint32_t)parameter;
      target->type_index = 1u;
      target->label_kind = hir_label_kind(value->label_kind);
      append_text_unchecked(value->name, output->text_bytes, &text_offset,
                            &target->name);
      if (value->label_kind == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY)
        target->label = (w_seed_hir0_text){0u, 0u};
      else
        append_text_unchecked(value->name, output->text_bytes, &text_offset,
                              &target->label);
    }
    host_parameter_offset += source->parameter_count;
    for (size_t requirement = 0u; requirement < source->requirement_count;
         requirement += 1u) {
      w_seed_hir0_requirement *target =
          &output->requirements[requirement_offset + requirement];
      target->owner_kind = W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY;
      target->owner_index = identity_index;
      target->ordinal = (uint32_t)requirement;
      append_text_unchecked(source->requirements[requirement].name,
                            output->text_bytes, &text_offset, &target->name);
    }
    requirement_offset += source->requirement_count;
  }
  /* Parameters preserve function signature facts. */
  for (size_t parameter = 0u; parameter < counts->parameters; parameter += 1u) {
    const w_seed_frontend_parameter *source = &frontend->parameters[parameter];
    w_seed_hir0_parameter *target = &output->parameters[parameter];
    target->owner_function = source->owner_function;
    target->ordinal = (uint32_t)(parameter -
                                 frontend->functions[source->owner_function]
                                     .first_parameter);
    target->type_index = hir_type_from_frontend(frontend, frontend_result,
                                                source->type_index);
    target->label_kind = hir_label_kind(source->label_kind);
    append_text_unchecked(source->name, output->text_bytes, &text_offset,
                          &target->name);
    append_text_unchecked(source->label, output->text_bytes, &text_offset,
                          &target->label);
    target->source_span = source->span;
  }
  size_t instruction_offset = 0u;
  size_t binding_offset = 0u;
  size_t call_offset = 0u;
  size_t argument_offset = 0u;
  size_t enum_payload_index = 0u;
  size_t value_index = 0u;
  size_t interpolation_segment_index = 0u;
  size_t block_argument_index = 0u;
  size_t edge_argument_index = 0u;
  size_t switch_edge_index = 0u;
  size_t switch_capture_index = 0u;
  size_t block_cursor = 0u;
  for (size_t function = 0u; function < counts->functions; function += 1u) {
    const w_seed_frontend_function *source = &frontend->functions[function];
    hir0_emit_context layout = {
        .frontend = frontend,
        .frontend_result = frontend_result};
    const size_t function_block_count = hir0_region_block_count(
        &layout,
        source->statement_count == 0u ? W_SEED_FRONTEND_NONE
                                       : source->first_statement,
        0u);
    w_seed_hir0_function *target_function = &output->functions[function];
    target_function->first_block = (uint32_t)block_cursor;
    target_function->block_count = (uint32_t)function_block_count;
    block_cursor += function_block_count;
  }
  for (size_t function = 0u; function < counts->functions; function += 1u) {
    const w_seed_hir0_function *target_function = &output->functions[function];
    for (size_t offset = 0u; offset < target_function->block_count;
         offset += 1u) {
      w_seed_hir0_block *block =
          &output->blocks[(size_t)target_function->first_block + offset];
      block->owner_function = (uint32_t)function;
      block->ordinal = (uint32_t)offset;
      block->first_instruction = W_SEED_HIR0_NONE;
      block->instruction_count = 0u;
      block->terminator_index =
          (uint32_t)((size_t)target_function->first_block + offset);
      block->source_span = target_function->body_span;
      block->next_block = W_SEED_HIR0_NONE;
      block->first_block_argument = W_SEED_HIR0_NONE;
      block->block_argument_count = 0u;
    }
  }
  for (size_t function = 0u; function < counts->functions; function += 1u) {
    const w_seed_hir0_function *target_function = &output->functions[function];
    hir0_emit_context context = {
        .counts = counts,
        .output = output,
        .frontend = frontend,
        .frontend_result = frontend_result,
        .frontend_input = frontend_input,
        .function = function,
        .text_offset = &text_offset,
        .value_offset = &value_offset,
        .instruction_offset = &instruction_offset,
        .binding_offset = &binding_offset,
        .call_offset = &call_offset,
        .argument_offset = &argument_offset,
        .enum_payload_index = &enum_payload_index,
        .value_index = &value_index,
        .interpolation_segment_index = &interpolation_segment_index,
        .block_argument_index = &block_argument_index,
        .edge_argument_index = &edge_argument_index,
        .switch_edge_index = &switch_edge_index,
        .switch_capture_index = &switch_capture_index};
    const uint32_t first_statement =
        frontend->functions[function].statement_count == 0u
            ? W_SEED_FRONTEND_NONE
            : frontend->functions[function].first_statement;
    hir0_emit_chain_layout_m2(&context, first_statement,
                              target_function->first_block,
                              W_SEED_HIR0_NONE, true, 0u);
  }
  size_t binding_cursor = 0u;
  for (size_t function = 0u; function < counts->functions; function += 1u) {
    const w_seed_hir0_function *target_function = &output->functions[function];
    hir0_emit_context context = {
        .counts = counts,
        .output = output,
        .frontend = frontend,
        .frontend_result = frontend_result,
        .frontend_input = frontend_input,
        .function = function,
        .text_offset = &text_offset,
        .value_offset = &value_offset,
        .instruction_offset = &instruction_offset,
        .binding_offset = &binding_offset,
        .call_offset = &call_offset,
        .argument_offset = &argument_offset,
        .enum_payload_index = &enum_payload_index,
        .value_index = &value_index,
        .interpolation_segment_index = &interpolation_segment_index,
        .block_argument_index = &block_argument_index,
        .edge_argument_index = &edge_argument_index,
        .switch_edge_index = &switch_edge_index,
        .switch_capture_index = &switch_capture_index};
    const uint32_t first_statement =
        frontend->functions[function].statement_count == 0u
            ? W_SEED_FRONTEND_NONE
            : frontend->functions[function].first_statement;
    hir0_emit_chain_values_m2(&context, first_statement,
                              target_function->first_block, 0u,
                              &binding_cursor);
  }
  for (size_t function = 0u; function < counts->functions; function += 1u) {
    const w_seed_hir0_function *target_function = &output->functions[function];
    hir0_emit_context context = {
        .counts = counts,
        .output = output,
        .frontend = frontend,
        .frontend_result = frontend_result,
        .frontend_input = frontend_input,
        .function = function,
        .text_offset = &text_offset,
        .value_offset = &value_offset,
        .instruction_offset = &instruction_offset,
        .binding_offset = &binding_offset,
        .call_offset = &call_offset,
        .argument_offset = &argument_offset,
        .enum_payload_index = &enum_payload_index,
        .value_index = &value_index,
        .interpolation_segment_index = &interpolation_segment_index,
        .block_argument_index = &block_argument_index,
        .edge_argument_index = &edge_argument_index,
        .switch_edge_index = &switch_edge_index,
        .switch_capture_index = &switch_capture_index};
    const uint32_t first_statement =
        frontend->functions[function].statement_count == 0u
            ? W_SEED_FRONTEND_NONE
            : frontend->functions[function].first_statement;
    hir0_emit_chain_terms_m2(&context, first_statement,
                             target_function->first_block, 0u);
  }
  /* Entry identities and records are dense after functions. */
  const size_t entry_identity_base = counts->modules + counts->functions;
  for (size_t entry = 0u; entry < counts->entries; entry += 1u) {
    const w_seed_frontend_entry *source = &frontend->entries[entry];
    const size_t function = source->target_function;
    w_seed_hir0_entry *target = &output->entries[entry];
    target->module_index = source->module_index;
    target->identity_index = (uint32_t)(entry_identity_base + entry);
    target->target_function = (uint32_t)function;
    target->target_identity = output->functions[function].identity_index;
    append_text_unchecked(frontend->functions[function].name, output->text_bytes,
                          &text_offset,
                          &target->target_name);
    append_text_unchecked((w_seed_frontend_text){HIR0_SLOT_NAME,
                                                 sizeof(HIR0_SLOT_NAME) - 1u},
                          output->text_bytes, &text_offset, &target->slot);
    target->source_span = source->span;
    target->is_body = source->is_body;
    target->adapter_kind = counts->external_modules == 0u
                               ? W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT
                               : W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS;
    target->cleanup_obligation = W_SEED_HIR0_ENTRY_CLEANUP_NONE;
    target->first_cleanup_owner_parameter = W_SEED_HIR0_NONE;
    target->cleanup_owner_parameter_count = 0u;
    output->identities[entry_identity_base + entry] =
        (w_seed_hir0_identity){
            .kind = W_SEED_HIR0_IDENTITY_ENTRY,
            .owner_module = source->module_index,
            .target_index = (uint32_t)entry,
            .name = target->target_name,
            .first_parameter = W_SEED_HIR0_NONE,
            .parameter_count = 0u,
            .first_requirement = W_SEED_HIR0_NONE,
            .requirement_count = 0u,
            .return_type = W_SEED_HIR0_NONE,
            .is_const = false,
            .profile = {0u, 0u}};
  }
  /* Existing emitters use whole-struct literals. Normalize the append-only
   * external payload after all value emission so ordinary values retain the
   * atomic NONE identity regardless of which emitter path created them. */
  for (size_t value = 0u; value < counts->values; value += 1u) {
    if (output->values[value].kind !=
        W_SEED_HIR0_VALUE_PATTERN_CAPTURE_READ)
      output->values[value].pattern_capture_index = W_SEED_HIR0_NONE;
    if (output->values[value].kind == W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE ||
        output->values[value].kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER ||
        output->values[value].kind == W_SEED_HIR0_VALUE_ENUM_CASE)
      continue;
    output->values[value].external_module_index = W_SEED_HIR0_NONE;
    output->values[value].external_symbol_index = W_SEED_HIR0_NONE;
    output->values[value].member_name = (w_seed_hir0_text){0u, 0u};
    output->values[value].enum_index = W_SEED_HIR0_NONE;
    output->values[value].enum_case_index = W_SEED_HIR0_NONE;
  }
  for (size_t terminator = 0u; terminator < counts->terminators;
       terminator += 1u) {
    if (output->terminators[terminator].kind !=
        W_SEED_HIR0_TERMINATOR_SWITCH_ENUM) {
      output->terminators[terminator].switch_enum_index = W_SEED_HIR0_NONE;
      output->terminators[terminator].first_switch_edge = W_SEED_HIR0_NONE;
      output->terminators[terminator].switch_edge_count = 0u;
      output->terminators[terminator].switch_carrier_width = 0u;
    }
  }
  /* collect() proves these cursors equal the measured bounds. */
}

/* The digest encoding uses explicit big-endian scalar fields. It never hashes
 * C object representation or padding. */
static void digest_u32(w_seed_sha256_state *state, uint32_t value) {
  uint8_t bytes[4] = {(uint8_t)(value >> 24u), (uint8_t)(value >> 16u),
                      (uint8_t)(value >> 8u), (uint8_t)value};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void digest_u64(w_seed_sha256_state *state, uint64_t value) {
  uint8_t bytes[8] = {(uint8_t)(value >> 56u), (uint8_t)(value >> 48u),
                      (uint8_t)(value >> 40u), (uint8_t)(value >> 32u),
                      (uint8_t)(value >> 24u), (uint8_t)(value >> 16u),
                      (uint8_t)(value >> 8u), (uint8_t)value};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void digest_bool(w_seed_sha256_state *state, bool value) {
  const uint8_t byte = value ? 1u : 0u;
  w_seed_sha256_update(state, &byte, 1u);
}

static void digest_span(w_seed_sha256_state *state, w_seed_span span) {
  digest_u64(state, (uint64_t)span.start_byte);
  digest_u64(state, (uint64_t)span.end_byte);
}

static void digest_text(w_seed_sha256_state *state,
                        const w_seed_hir0_program *program,
                        w_seed_hir0_text text) {
  digest_u32(state, text.offset);
  digest_u32(state, text.count);
  if (text.count != 0u)
    w_seed_sha256_update(state, program->text_bytes + text.offset, text.count);
}

static void digest_bytes(w_seed_sha256_state *state,
                         const w_seed_hir0_program *program, uint32_t offset,
                         uint32_t count) {
  digest_u32(state, offset);
  digest_u32(state, count);
  if (count != 0u)
    w_seed_sha256_update(state, program->value_bytes + offset, count);
}

static void digest_counts(w_seed_sha256_state *state,
                          const w_seed_hir0_counts *counts) {
  digest_u64(state, counts->modules);
  digest_u64(state, counts->identities);
  digest_u64(state, counts->types);
  digest_u64(state, counts->functions);
  digest_u64(state, counts->parameters);
  digest_u64(state, counts->blocks);
  digest_u64(state, counts->block_arguments);
  digest_u64(state, counts->edge_arguments);
  digest_u64(state, counts->switch_edges);
  digest_u64(state, counts->switch_captures);
  digest_u64(state, counts->instructions);
  digest_u64(state, counts->bindings);
  digest_u64(state, counts->calls);
  digest_u64(state, counts->host_parameters);
  digest_u64(state, counts->arguments);
  digest_u64(state, counts->enum_payloads);
  digest_u64(state, counts->requirements);
  digest_u64(state, counts->values);
  digest_u64(state, counts->interpolation_segments);
  digest_u64(state, counts->terminators);
  digest_u64(state, counts->entries);
  digest_u64(state, counts->text_bytes);
  digest_u64(state, counts->value_bytes);
  digest_u64(state, counts->external_modules);
  digest_u64(state, counts->external_symbols);
  digest_u64(state, counts->enums);
  digest_u64(state, counts->enum_cases);
  digest_u64(state, counts->enum_case_parameters);
  digest_u64(state, counts->enum_subsets);
  digest_u64(state, counts->enum_subset_members);
}

static void digest_program(const w_seed_hir0_program *program,
                           const w_seed_hir0_counts *counts,
                           uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)W_SEED_HIR0_SCHEMA_VERSION,
                       sizeof(W_SEED_HIR0_SCHEMA_VERSION) - 1u);
  digest_counts(&state, counts);
#define HIR0_RECORD_TAG(value) digest_u32(&state, (uint32_t)(value))
  for (size_t index = 0u; index < counts->modules; index += 1u) {
    const w_seed_hir0_module *value = &program->modules[index];
    HIR0_RECORD_TAG(1u);
    digest_u32(&state, value->module_index);
    digest_u32(&state, value->identity_index);
    digest_text(&state, program, value->module_id);
    digest_text(&state, program, value->local_module_name);
    digest_u32(&state, value->first_function);
    digest_u32(&state, value->function_count);
    digest_u32(&state, value->first_entry);
    digest_u32(&state, value->entry_count);
  }
  for (size_t index = 0u; index < counts->identities; index += 1u) {
    const w_seed_hir0_identity *value = &program->identities[index];
    HIR0_RECORD_TAG(2u);
    digest_u32(&state, (uint32_t)value->kind);
    digest_u32(&state, value->owner_module);
    digest_u32(&state, value->target_index);
    digest_text(&state, program, value->name);
    digest_u32(&state, value->first_parameter);
    digest_u32(&state, value->parameter_count);
    digest_u32(&state, value->first_requirement);
    digest_u32(&state, value->requirement_count);
    digest_u32(&state, value->return_type);
    digest_bool(&state, value->is_const);
    digest_text(&state, program, value->profile);
  }
  for (size_t index = 0u; index < counts->types; index += 1u) {
    const w_seed_hir0_type *value = &program->types[index];
    HIR0_RECORD_TAG(3u);
    digest_u32(&state, (uint32_t)value->kind);
    digest_u32(&state, value->owner_module);
    digest_text(&state, program, value->name);
    digest_u32(&state, value->external_module_index);
    digest_u32(&state, value->external_symbol_index);
    digest_u32(&state, value->enum_index);
    digest_u32(&state, value->first_subset_member);
    digest_u32(&state, value->subset_member_count);
    digest_u32(&state, (uint32_t)value->lifecycle);
    digest_u32(&state, (uint32_t)value->release_contract);
    if (value->release_contract ==
        W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE) {
      digest_u32(&state, (uint32_t)(sizeof(HIR0_PROCESS_RELEASE_ABI) - 1u));
      w_seed_sha256_update(
          &state, (const uint8_t *)HIR0_PROCESS_RELEASE_ABI,
          sizeof(HIR0_PROCESS_RELEASE_ABI) - 1u);
    }
  }
  for (size_t index = 0u; index < counts->enums; index += 1u) {
    const w_seed_hir0_enum *value = &program->enums[index];
    HIR0_RECORD_TAG(21u);
    digest_u32(&state, value->module_index);
    digest_u32(&state, value->type_index);
    digest_text(&state, program, value->name);
    digest_u32(&state, value->first_case);
    digest_u32(&state, value->case_count);
  }
  for (size_t index = 0u; index < counts->enum_cases; index += 1u) {
    const w_seed_hir0_enum_case *value = &program->enum_cases[index];
    HIR0_RECORD_TAG(22u);
    digest_u32(&state, value->owner_enum);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->tag);
    digest_u32(&state, value->first_payload);
    digest_u32(&state, value->payload_count);
    digest_text(&state, program, value->name);
  }
  for (size_t index = 0u; index < counts->enum_case_parameters; index += 1u) {
    const w_seed_hir0_enum_case_parameter *value =
        &program->enum_case_parameters[index];
    HIR0_RECORD_TAG(23u);
    digest_u32(&state, value->owner_case);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->type_index);
    digest_text(&state, program, value->label);
    digest_bool(&state, value->has_label);
  }
  for (size_t index = 0u; index < counts->enum_subset_members; index += 1u) {
    const w_seed_hir0_enum_subset_member *value =
        &program->enum_subset_members[index];
    HIR0_RECORD_TAG(26u);
    digest_u32(&state, value->owner_type);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->enum_index);
    digest_u32(&state, value->enum_case_index);
  }
  for (size_t index = 0u; index < counts->functions; index += 1u) {
    const w_seed_hir0_function *value = &program->functions[index];
    HIR0_RECORD_TAG(4u);
    digest_u32(&state, value->module_index);
    digest_u32(&state, value->identity_index);
    digest_text(&state, program, value->name);
    digest_bool(&state, value->exported);
    digest_u32(&state, value->return_type);
    digest_u32(&state, value->first_parameter);
    digest_u32(&state, value->parameter_count);
    digest_u32(&state, value->first_block);
    digest_u32(&state, value->block_count);
    digest_bool(&state, value->is_const);
    digest_bool(&state, value->is_async);
    digest_bool(&state, value->is_throws);
    digest_bool(&state, value->is_unsafe);
    digest_bool(&state, value->has_borrow_clause);
    digest_bool(&state, value->is_anonymous_entry);
    digest_u32(&state, (uint32_t)value->suspension);
    digest_u32(&state, (uint32_t)value->direct_entry);
  }
  for (size_t index = 0u; index < counts->parameters; index += 1u) {
    const w_seed_hir0_parameter *value = &program->parameters[index];
    HIR0_RECORD_TAG(5u);
    digest_u32(&state, value->owner_function);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->type_index);
    digest_text(&state, program, value->name);
    digest_text(&state, program, value->label);
    digest_u32(&state, (uint32_t)value->label_kind);
  }
  for (size_t index = 0u; index < counts->blocks; index += 1u) {
    const w_seed_hir0_block *value = &program->blocks[index];
    HIR0_RECORD_TAG(6u);
    digest_u32(&state, value->owner_function);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->first_instruction);
    digest_u32(&state, value->instruction_count);
    digest_u32(&state, value->terminator_index);
    digest_u32(&state, value->next_block);
    digest_u32(&state, value->first_block_argument);
    digest_u32(&state, value->block_argument_count);
  }
  for (size_t index = 0u; index < counts->block_arguments; index += 1u) {
    const w_seed_hir0_block_argument *value = &program->block_arguments[index];
    HIR0_RECORD_TAG(17u);
    digest_u32(&state, value->owner_block);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->type_index);
  }
  for (size_t index = 0u; index < counts->edge_arguments; index += 1u) {
    const w_seed_hir0_edge_argument *value = &program->edge_arguments[index];
    HIR0_RECORD_TAG(20u);
    digest_u32(&state, value->owner_terminator);
    digest_u32(&state, value->owner_block);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->value_index);
    digest_u32(&state, value->type_index);
  }
  for (size_t index = 0u; index < counts->switch_edges; index += 1u) {
    const w_seed_hir0_switch_edge *value = &program->switch_edges[index];
    HIR0_RECORD_TAG(23u);
    digest_u32(&state, value->owner_terminator);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->enum_index);
    digest_u32(&state, value->enum_case_index);
    digest_u32(&state, value->target_block);
    digest_u32(&state, value->first_capture);
    digest_u32(&state, value->capture_count);
  }
  for (size_t index = 0u; index < counts->switch_captures; index += 1u) {
    const w_seed_hir0_switch_capture *value =
        &program->switch_captures[index];
    HIR0_RECORD_TAG(25u);
    digest_u32(&state, value->owner_switch_edge);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->parameter_ordinal);
    digest_u32(&state, value->type_index);
    digest_text(&state, program, value->name);
  }
  for (size_t index = 0u; index < counts->instructions; index += 1u) {
    const w_seed_hir0_instruction *value = &program->instructions[index];
    HIR0_RECORD_TAG(7u);
    digest_u32(&state, (uint32_t)value->kind);
    digest_u32(&state, value->owner_block);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->call_index);
    digest_u32(&state, value->binding_index);
    digest_u32(&state, value->result_type);
  }
  for (size_t index = 0u; index < counts->calls; index += 1u) {
    const w_seed_hir0_call *value = &program->calls[index];
    HIR0_RECORD_TAG(8u);
    digest_u32(&state, value->owner_instruction);
    digest_u32(&state, value->owner_block);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->callee_identity);
    digest_u32(&state, value->first_argument);
    digest_u32(&state, value->argument_count);
    digest_u32(&state, value->first_requirement);
    digest_u32(&state, value->requirement_count);
    digest_u32(&state, value->result_type);
    digest_u32(&state, (uint32_t)value->execution_kind);
  }
  for (size_t index = 0u; index < counts->host_parameters; index += 1u) {
    const w_seed_hir0_host_parameter *value = &program->host_parameters[index];
    HIR0_RECORD_TAG(9u);
    digest_u32(&state, value->owner_identity);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->type_index);
    digest_text(&state, program, value->name);
    digest_text(&state, program, value->label);
    digest_u32(&state, (uint32_t)value->label_kind);
  }
  for (size_t index = 0u; index < counts->arguments; index += 1u) {
    const w_seed_hir0_argument *value = &program->arguments[index];
    HIR0_RECORD_TAG(10u);
    digest_u32(&state, value->owner_call);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->parameter_ordinal);
    digest_u32(&state, value->value_index);
    digest_u32(&state, value->type_index);
    digest_text(&state, program, value->label);
    digest_u32(&state, (uint32_t)value->label_kind);
  }
  for (size_t index = 0u; index < counts->requirements; index += 1u) {
    const w_seed_hir0_requirement *value = &program->requirements[index];
    HIR0_RECORD_TAG(11u);
    digest_u32(&state, (uint32_t)value->owner_kind);
    digest_u32(&state, value->owner_index);
    digest_u32(&state, value->ordinal);
    digest_text(&state, program, value->name);
  }
  for (size_t index = 0u; index < counts->enum_payloads; index += 1u) {
    const w_seed_hir0_enum_payload *value = &program->enum_payloads[index];
    HIR0_RECORD_TAG(24u);
    digest_u32(&state, value->owner_value);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->parameter_ordinal);
    digest_u32(&state, value->value_index);
    digest_u32(&state, value->type_index);
  }
  for (size_t index = 0u; index < counts->values; index += 1u) {
    const w_seed_hir0_value *value = &program->values[index];
    HIR0_RECORD_TAG(12u);
    digest_u32(&state, (uint32_t)value->kind);
    digest_u32(&state, (uint32_t)value->owner_kind);
    digest_u32(&state, value->owner_index);
    digest_u32(&state, value->owner_ordinal);
    digest_u32(&state, value->type_index);
    digest_u32(&state, value->binding_index);
    digest_u32(&state, value->parameter_index);
    digest_u32(&state, value->call_index);
    digest_u32(&state, value->left_value);
    digest_u32(&state, value->right_value);
    digest_u32(&state, value->first_interpolation_segment);
    digest_u32(&state, value->interpolation_segment_count);
    digest_u32(&state, value->first_enum_payload);
    digest_u32(&state, value->enum_payload_count);
    digest_u32(&state, value->pattern_capture_index);
    digest_u32(&state, (uint32_t)value->binary_operator);
    digest_u32(&state, (uint32_t)value->unary_operator);
    digest_u32(&state, value->block_argument_index);
    digest_u64(&state, (uint64_t)value->integer_value);
    digest_u64(&state, value->unsigned_integer_value);
    digest_bool(&state, value->bool_value);
    digest_bytes(&state, program, value->byte_offset, value->byte_count);
    digest_u32(&state, value->external_module_index);
    digest_u32(&state, value->external_symbol_index);
    digest_text(&state, program, value->member_name);
    digest_u32(&state, value->enum_index);
    digest_u32(&state, value->enum_case_index);
  }
  for (size_t index = 0u; index < counts->interpolation_segments;
       index += 1u) {
    const w_seed_hir0_interpolation_segment *value =
        &program->interpolation_segments[index];
    HIR0_RECORD_TAG(16u);
    digest_u32(&state, (uint32_t)value->kind);
    digest_u32(&state, value->owner_value);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->value_index);
    digest_bytes(&state, program, value->byte_offset, value->byte_count);
  }
  for (size_t index = 0u; index < counts->terminators; index += 1u) {
    const w_seed_hir0_terminator *value = &program->terminators[index];
    HIR0_RECORD_TAG(13u);
    digest_u32(&state, value->owner_block);
    digest_u32(&state, (uint32_t)value->kind);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->value_index);
    digest_u32(&state, value->result_type);
    digest_u32(&state, value->target_block);
    digest_u32(&state, value->else_block);
    digest_u32(&state, value->first_edge_argument);
    digest_u32(&state, value->edge_argument_count);
    digest_u32(&state, (uint32_t)value->logical_operator);
    digest_u32(&state, value->switch_enum_index);
    digest_u32(&state, value->first_switch_edge);
    digest_u32(&state, value->switch_edge_count);
    digest_u32(&state, value->switch_carrier_width);
  }
  for (size_t index = 0u; index < counts->entries; index += 1u) {
    const w_seed_hir0_entry *value = &program->entries[index];
    HIR0_RECORD_TAG(14u);
    digest_u32(&state, value->module_index);
    digest_u32(&state, value->identity_index);
    digest_u32(&state, value->target_function);
    digest_u32(&state, value->target_identity);
    digest_text(&state, program, value->target_name);
    digest_text(&state, program, value->slot);
    digest_bool(&state, value->is_body);
    digest_u32(&state, (uint32_t)value->adapter_kind);
    digest_u32(&state, (uint32_t)value->cleanup_obligation);
    digest_u32(&state, value->first_cleanup_owner_parameter);
    digest_u32(&state, value->cleanup_owner_parameter_count);
  }
  for (size_t index = 0u; index < counts->bindings; index += 1u) {
    const w_seed_hir0_binding *value = &program->bindings[index];
    HIR0_RECORD_TAG(15u);
    digest_u32(&state, value->owner_instruction);
    digest_u32(&state, value->owner_block);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->type_index);
    digest_text(&state, program, value->name);
    digest_bool(&state, value->is_mutable);
    digest_u32(&state, value->task_role);
    digest_u32(&state, value->source_binding);
    digest_u32(&state, value->previous_version);
    digest_u32(&state, value->next_version);
    digest_u32(&state, value->task_peer_binding);
    digest_u32(&state, value->initializer_value);
  }
  for (size_t index = 0u; index < counts->external_modules; index += 1u) {
    const w_seed_hir0_external_module *value = &program->external_modules[index];
    HIR0_RECORD_TAG(18u);
    digest_u32(&state, value->module_index);
    digest_text(&state, program, value->module_id);
    digest_u32(&state, value->first_symbol);
    digest_u32(&state, value->symbol_count);
  }
  for (size_t index = 0u; index < counts->external_symbols; index += 1u) {
    const w_seed_hir0_external_symbol *value = &program->external_symbols[index];
    HIR0_RECORD_TAG(19u);
    digest_u32(&state, value->module_index);
    digest_u32(&state, value->ordinal);
    digest_text(&state, program, value->name);
    digest_u32(&state, (uint32_t)value->kind);
    digest_bool(&state, value->exported);
    digest_bool(&state, value->is_const);
    digest_text(&state, program, value->receiver_type);
    digest_text(&state, program, value->return_type);
    digest_u32(&state, value->parameter_count);
    /* Keep the private four-symbol process receipt byte-compatible with the
     * pre-ABI schema. The public seven-symbol catalog has one closed ABI fact,
     * so include parameter ABI details in that extension only. */
    if (counts->external_symbols == 7u)
      digest_u32(&state, (uint32_t)value->parameter_abi);
  }
#undef HIR0_RECORD_TAG
  w_seed_sha256_final(&state, digest);
}

static void digest_provenance(const w_seed_hir0_program *program,
                              const w_seed_hir0_counts *counts,
                              uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)"w-seed-hir0-provenance-1",
                       sizeof("w-seed-hir0-provenance-1") - 1u);
  digest_counts(&state, counts);
  for (size_t index = 0u; index < counts->modules; index += 1u) {
    const w_seed_hir0_module *value = &program->modules[index];
    digest_text(&state, program, value->source_id);
    digest_u64(&state, (uint64_t)value->source_length);
    digest_span(&state, value->source_span);
    w_seed_sha256_update(&state, value->source_sha256, 32u);
  }
  for (size_t index = 0u; index < counts->functions; index += 1u) {
    digest_span(&state, program->functions[index].source_span);
    digest_span(&state, program->functions[index].body_span);
  }
  for (size_t index = 0u; index < counts->parameters; index += 1u)
    digest_span(&state, program->parameters[index].source_span);
  for (size_t index = 0u; index < counts->enums; index += 1u)
    digest_span(&state, program->enums[index].source_span);
  for (size_t index = 0u; index < counts->enum_cases; index += 1u)
    digest_span(&state, program->enum_cases[index].source_span);
  for (size_t index = 0u; index < counts->enum_case_parameters; index += 1u)
    digest_span(&state, program->enum_case_parameters[index].source_span);
  for (size_t index = 0u; index < counts->enum_subset_members; index += 1u)
    digest_span(&state, program->enum_subset_members[index].source_span);
  for (size_t index = 0u; index < counts->blocks; index += 1u)
    digest_span(&state, program->blocks[index].source_span);
  for (size_t index = 0u; index < counts->block_arguments; index += 1u)
    digest_span(&state, program->block_arguments[index].source_span);
  for (size_t index = 0u; index < counts->edge_arguments; index += 1u)
    digest_span(&state, program->edge_arguments[index].source_span);
  for (size_t index = 0u; index < counts->switch_edges; index += 1u)
    digest_span(&state, program->switch_edges[index].source_span);
  for (size_t index = 0u; index < counts->switch_captures; index += 1u)
    digest_span(&state, program->switch_captures[index].source_span);
  for (size_t index = 0u; index < counts->instructions; index += 1u)
    digest_span(&state, program->instructions[index].source_span);
  for (size_t index = 0u; index < counts->calls; index += 1u) {
    digest_u32(&state, program->calls[index].source_expression);
    digest_span(&state, program->calls[index].source_span);
  }
  for (size_t index = 0u; index < counts->arguments; index += 1u)
    digest_span(&state, program->arguments[index].source_span);
  for (size_t index = 0u; index < counts->values; index += 1u)
    digest_span(&state, program->values[index].source_span);
  for (size_t index = 0u; index < counts->interpolation_segments;
       index += 1u)
    digest_span(&state, program->interpolation_segments[index].source_span);
  for (size_t index = 0u; index < counts->enum_payloads; index += 1u)
    digest_span(&state, program->enum_payloads[index].source_span);
  for (size_t index = 0u; index < counts->terminators; index += 1u)
    digest_span(&state, program->terminators[index].source_span);
  for (size_t index = 0u; index < counts->entries; index += 1u)
    digest_span(&state, program->entries[index].source_span);
  for (size_t index = 0u; index < counts->bindings; index += 1u)
    digest_span(&state, program->bindings[index].source_span);
  w_seed_sha256_final(&state, digest);
}

static void write_u64_be(uint8_t *buffer, size_t offset, uint64_t value) {
  buffer[offset] = (uint8_t)(value >> 56u);
  buffer[offset + 1u] = (uint8_t)(value >> 48u);
  buffer[offset + 2u] = (uint8_t)(value >> 40u);
  buffer[offset + 3u] = (uint8_t)(value >> 32u);
  buffer[offset + 4u] = (uint8_t)(value >> 24u);
  buffer[offset + 5u] = (uint8_t)(value >> 16u);
  buffer[offset + 6u] = (uint8_t)(value >> 8u);
  buffer[offset + 7u] = (uint8_t)value;
}

static void write_receipt_unchecked(uint8_t *buffer,
                                    const w_seed_hir0_counts *counts,
                                    const uint8_t semantic_digest[32],
                                    const uint8_t provenance_digest[32]) {
  (void)memset(buffer, 0, HIR0_RECEIPT_BYTES);
  (void)memcpy(buffer, W_SEED_HIR0_SCHEMA_VERSION,
               sizeof(W_SEED_HIR0_SCHEMA_VERSION) - 1u);
  const size_t fields[HIR0_RECEIPT_COUNT_FIELDS] = {
      counts->modules,       counts->identities, counts->types,
      counts->functions,      counts->parameters, counts->blocks,
      counts->block_arguments, counts->edge_arguments, counts->switch_edges,
      counts->switch_captures,
      counts->instructions,
      counts->bindings,
      counts->calls,
      counts->host_parameters,
      counts->arguments,      counts->enum_payloads, counts->requirements,
      counts->values,
      counts->interpolation_segments,
      counts->terminators,    counts->entries,     counts->text_bytes,
      counts->value_bytes,    counts->external_modules,
      counts->external_symbols,
      counts->enums,
      counts->enum_cases,
      counts->enum_case_parameters,
      counts->enum_subsets,
      counts->enum_subset_members};
  size_t offset = HIR0_RECEIPT_SCHEMA_BYTES;
  for (size_t index = 0u; index < HIR0_RECEIPT_COUNT_FIELDS; index += 1u) {
    write_u64_be(buffer, offset, (uint64_t)fields[index]);
    offset += 8u;
  }
  (void)memcpy(buffer + offset, semantic_digest, HIR0_DIGEST_BYTES);
  offset += HIR0_DIGEST_BYTES;
  (void)memcpy(buffer + offset, provenance_digest, HIR0_DIGEST_BYTES);
}

static bool basic_program_shape(const w_seed_hir0_program *program,
                                const w_seed_hir0_result *result) {
  if (program == NULL || result == NULL ||
      result->status != W_SEED_HIR0_OK ||
      result->schema[sizeof(result->schema) - 1u] != '\0' ||
      memcmp(result->schema, W_SEED_HIR0_SCHEMA_VERSION,
             sizeof(W_SEED_HIR0_SCHEMA_VERSION)) != 0 ||
      program->function_count > W_SEED_FRONTEND_MAX_CST_NODES)
    return false;
#define HIR0_PROGRAM(field, count_field, capacity_field, type)                 \
  if (program->count_field > program->capacity_field ||                       \
      (program->count_field != 0u && program->field == NULL) ||               \
      (program->count_field != 0u &&                                          \
       program->count_field > SIZE_MAX / sizeof(type))) return false
  HIR0_PROGRAM(modules, module_count, module_capacity, w_seed_hir0_module);
  HIR0_PROGRAM(identities, identity_count, identity_capacity,
               w_seed_hir0_identity);
  HIR0_PROGRAM(types, type_count, type_capacity, w_seed_hir0_type);
  HIR0_PROGRAM(enums, enum_count, enum_capacity, w_seed_hir0_enum);
  HIR0_PROGRAM(enum_cases, enum_case_count, enum_case_capacity,
               w_seed_hir0_enum_case);
  HIR0_PROGRAM(enum_case_parameters, enum_case_parameter_count,
               enum_case_parameter_capacity,
               w_seed_hir0_enum_case_parameter);
  HIR0_PROGRAM(enum_subset_members, enum_subset_member_count,
               enum_subset_member_capacity, w_seed_hir0_enum_subset_member);
  HIR0_PROGRAM(functions, function_count, function_capacity,
               w_seed_hir0_function);
  HIR0_PROGRAM(parameters, parameter_count, parameter_capacity,
               w_seed_hir0_parameter);
  HIR0_PROGRAM(blocks, block_count, block_capacity, w_seed_hir0_block);
  HIR0_PROGRAM(block_arguments, block_argument_count,
               block_argument_capacity, w_seed_hir0_block_argument);
  HIR0_PROGRAM(edge_arguments, edge_argument_count,
               edge_argument_capacity, w_seed_hir0_edge_argument);
  HIR0_PROGRAM(switch_edges, switch_edge_count, switch_edge_capacity,
               w_seed_hir0_switch_edge);
  HIR0_PROGRAM(switch_captures, switch_capture_count,
               switch_capture_capacity, w_seed_hir0_switch_capture);
  HIR0_PROGRAM(instructions, instruction_count, instruction_capacity,
               w_seed_hir0_instruction);
  HIR0_PROGRAM(bindings, binding_count, binding_capacity, w_seed_hir0_binding);
  HIR0_PROGRAM(calls, call_count, call_capacity, w_seed_hir0_call);
  HIR0_PROGRAM(host_parameters, host_parameter_count, host_parameter_capacity,
               w_seed_hir0_host_parameter);
  HIR0_PROGRAM(arguments, argument_count, argument_capacity,
               w_seed_hir0_argument);
  HIR0_PROGRAM(enum_payloads, enum_payload_count, enum_payload_capacity,
               w_seed_hir0_enum_payload);
  HIR0_PROGRAM(requirements, requirement_count, requirement_capacity,
               w_seed_hir0_requirement);
  HIR0_PROGRAM(values, value_count, value_capacity, w_seed_hir0_value);
  HIR0_PROGRAM(interpolation_segments, interpolation_segment_count,
               interpolation_segment_capacity,
               w_seed_hir0_interpolation_segment);
  HIR0_PROGRAM(terminators, terminator_count, terminator_capacity,
               w_seed_hir0_terminator);
  HIR0_PROGRAM(entries, entry_count, entry_capacity, w_seed_hir0_entry);
  HIR0_PROGRAM(external_modules, external_module_count,
               external_module_capacity, w_seed_hir0_external_module);
  HIR0_PROGRAM(external_symbols, external_symbol_count,
               external_symbol_capacity, w_seed_hir0_external_symbol);
#undef HIR0_PROGRAM
  if (program->text_byte_count > program->text_byte_capacity ||
      (program->text_byte_count != 0u && program->text_bytes == NULL) ||
      program->value_byte_count > program->value_byte_capacity ||
      (program->value_byte_count != 0u && program->value_bytes == NULL) ||
      program->receipt_count > program->receipt_capacity ||
      program->receipt_count != HIR0_RECEIPT_BYTES || program->receipt == NULL)
    return false;
  return true;
}

static bool program_aliases(const w_seed_hir0_program *program) {
  if (program == NULL) return true;
  hir0_memory_range ranges[37];
  size_t count = 0u;
  if (!program_range_table(program, ranges, &count)) return true;
  for (size_t first = 0u; first < count; first += 1u)
    for (size_t second = first + 1u; second < count; second += 1u)
      if (ranges_overlap(ranges[first].pointer, ranges[first].bytes,
                         ranges[second].pointer, ranges[second].bytes))
        return true;
  return false;
}

static bool verify_identity_records(const w_seed_hir0_program *program) {
  const size_t module_count = program->module_count;
  const size_t function_base = module_count;
  const size_t entry_base = module_count + program->function_count;
  const size_t host_base = entry_base + program->entry_count;
  if (program->identity_count < host_base)
    return false;
  size_t host_count = program->identity_count - host_base;
  for (size_t index = 0u; index < module_count; index += 1u) {
    const w_seed_hir0_identity *value = &program->identities[index];
    if (value->kind != W_SEED_HIR0_IDENTITY_MODULE ||
        value->owner_module != W_SEED_HIR0_NONE || value->target_index != index ||
        value->first_parameter != W_SEED_HIR0_NONE || value->parameter_count != 0u ||
        value->first_requirement != W_SEED_HIR0_NONE || value->requirement_count != 0u ||
        value->return_type != W_SEED_HIR0_NONE || value->is_const ||
        value->profile.count != 0u || !hir_text_valid(program, value->profile) ||
        !hir_text_equal(program, value->name, program->modules[index].module_id))
      return false;
  }
  for (size_t index = 0u; index < program->function_count; index += 1u) {
    const w_seed_hir0_identity *value = &program->identities[function_base + index];
    if (value->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
        value->target_index != index || value->owner_module >= module_count ||
        value->first_parameter != program->functions[index].first_parameter ||
        value->parameter_count != program->functions[index].parameter_count ||
        value->first_requirement != W_SEED_HIR0_NONE || value->requirement_count != 0u ||
        value->return_type != program->functions[index].return_type ||
        value->is_const != program->functions[index].is_const ||
        value->profile.count != 0u || !hir_text_valid(program, value->profile) ||
        !hir_text_equal(program, value->name, program->functions[index].name))
      return false;
  }
  for (size_t index = 0u; index < program->entry_count; index += 1u) {
    const w_seed_hir0_identity *value = &program->identities[entry_base + index];
    if (value->kind != W_SEED_HIR0_IDENTITY_ENTRY ||
        value->target_index != index || value->owner_module >= module_count ||
        value->first_parameter != W_SEED_HIR0_NONE || value->parameter_count != 0u ||
        value->first_requirement != W_SEED_HIR0_NONE || value->requirement_count != 0u ||
        value->return_type != W_SEED_HIR0_NONE || value->is_const ||
        value->profile.count != 0u || !hir_text_valid(program, value->profile) ||
        !hir_text_equal(program, value->name, program->entries[index].target_name))
      return false;
  }
  if (host_count == 0u) return false;
  size_t host_parameter_cursor = 0u;
  size_t requirement_cursor = 0u;
  for (size_t index = 0u; index < host_count; index += 1u) {
    const w_seed_hir0_identity *value = &program->identities[host_base + index];
    if (value->kind != W_SEED_HIR0_IDENTITY_HOST_PRELUDE ||
        value->owner_module != W_SEED_HIR0_NONE || value->target_index != index ||
        value->return_type != 0u || !hir_text_valid(program, value->profile) ||
        value->profile.count == 0u ||
        !range_valid(value->first_parameter, value->parameter_count,
                     program->host_parameter_count) ||
        !range_valid(value->first_requirement, value->requirement_count,
                     program->requirement_count) ||
        value->first_parameter != host_parameter_cursor ||
        value->first_requirement != requirement_cursor ||
        !hir_text_valid(program, value->name) || value->name.count == 0u ||
        (index != 0u &&
         !hir_text_equal(program, value->profile,
                         program->identities[host_base].profile)))
      return false;
    for (size_t parameter = 0u; parameter < value->parameter_count; parameter += 1u) {
      const w_seed_hir0_host_parameter *item =
          &program->host_parameters[(size_t)value->first_parameter + parameter];
      if (item->owner_identity != host_base + index || item->ordinal != parameter ||
          item->type_index != 1u || !hir_text_valid(program, item->name) ||
          !hir_text_valid(program, item->label) ||
          item->label_kind > W_SEED_HIR0_LABEL_REQUIRED ||
           !hir_host_label_matches(program, item->label_kind, item->name,
                                   item->label))
        return false;
    }
    for (size_t requirement = 0u; requirement < value->requirement_count;
         requirement += 1u) {
      const w_seed_hir0_requirement *item =
          &program->requirements[(size_t)value->first_requirement + requirement];
      if (item->owner_kind != W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY ||
          item->owner_index != host_base + index || item->ordinal != requirement ||
          !hir_text_valid(program, item->name) || item->name.count == 0u)
         return false;
    }
    host_parameter_cursor += value->parameter_count;
    requirement_cursor += value->requirement_count;
  }
  if (host_parameter_cursor != program->host_parameter_count ||
      requirement_cursor != program->requirement_count)
    return false;
  return true;
}

static bool verify_external_records(const w_seed_hir0_program *program) {
  if (program == NULL) return false;
  if (program->external_module_count == 0u) {
    return program->external_symbol_count == 0u;
  }
  if (program->external_module_count != 1u ||
      (program->external_symbol_count != 4u &&
       program->external_symbol_count != 7u) ||
      program->external_modules == NULL || program->external_symbols == NULL)
    return false;
  const w_seed_hir0_external_module *module = &program->external_modules[0];
  if (module->module_index != 0u ||
      !hir_text_is(program, module->module_id, HIR0_PROCESS_MODULE) ||
      module->first_symbol != 0u ||
      module->symbol_count != program->external_symbol_count)
    return false;
  static const char *const symbol_names[] = {
      HIR0_PROCESS_ARGUMENTS, HIR0_PROCESS_CONTEXT, HIR0_PROCESS_EXIT_CODE,
      HIR0_PROCESS_SUCCESS, HIR0_PROCESS_IS_EMPTY, HIR0_PROCESS_FAILURE,
      HIR0_PROCESS_COUNT};
  for (size_t index = 0u; index < program->external_symbol_count; index += 1u) {
    const w_seed_hir0_external_symbol *symbol =
        &program->external_symbols[index];
    if (symbol->module_index != 0u || symbol->ordinal != index ||
        !hir_text_is(program, symbol->name, symbol_names[index]) ||
        !symbol->exported ||
        (index != 5u && symbol->parameter_count != 0u) ||
        (index == 5u && symbol->parameter_count != 1u) ||
        symbol->parameter_abi >
            W_SEED_HIR0_EXTERNAL_PARAMETER_PROCESS_FAILURE_I64 ||
        (index != 5u &&
         symbol->parameter_abi != W_SEED_HIR0_EXTERNAL_PARAMETER_NONE) ||
        (index == 5u &&
         symbol->parameter_abi !=
             W_SEED_HIR0_EXTERNAL_PARAMETER_PROCESS_FAILURE_I64))
      return false;
    if (index < 3u) {
      if (symbol->kind != W_SEED_HIR0_EXTERNAL_TYPE || symbol->is_const ||
          symbol->receiver_type.count != 0u ||
          !hir_text_equal(program, symbol->return_type, symbol->name))
        return false;
    } else if (index == 3u) {
      if (symbol->kind != W_SEED_HIR0_EXTERNAL_VALUE || !symbol->is_const ||
          !hir_text_is(program, symbol->receiver_type,
                       HIR0_PROCESS_EXIT_CODE) ||
          !hir_text_is(program, symbol->return_type,
                       HIR0_PROCESS_EXIT_CODE))
        return false;
    } else if (index == 4u) {
      if (program->external_symbol_count != 7u ||
          symbol->kind != W_SEED_HIR0_EXTERNAL_VALUE || !symbol->is_const ||
          !hir_text_is(program, symbol->receiver_type,
                       HIR0_PROCESS_ARGUMENTS) ||
          !hir_text_is(program, symbol->return_type, HIR0_BOOL_NAME))
        return false;
    } else if (index == 5u) {
      if (program->external_symbol_count != 7u ||
          symbol->kind != W_SEED_HIR0_EXTERNAL_VALUE || !symbol->is_const ||
          !hir_text_is(program, symbol->receiver_type,
                       HIR0_PROCESS_EXIT_CODE) ||
          !hir_text_is(program, symbol->return_type,
                       HIR0_PROCESS_EXIT_CODE))
        return false;
    } else if (index == 6u) {
      if (program->external_symbol_count != 7u ||
          symbol->kind != W_SEED_HIR0_EXTERNAL_VALUE || !symbol->is_const ||
          !hir_text_is(program, symbol->receiver_type,
                       HIR0_PROCESS_ARGUMENTS) ||
          !hir_text_is(program, symbol->return_type, HIR0_USIZE_NAME))
        return false;
    } else {
      return false;
    }
    if (!hir_text_valid(program, symbol->receiver_type) ||
        !hir_text_valid(program, symbol->return_type))
      return false;
  }
  return true;
}

static bool hir_enum_subset_layout(const w_seed_hir0_program *program,
                                   size_t *subset_base,
                                   size_t *subset_count,
                                   uint32_t *usize_type_index) {
  if (program == NULL || subset_base == NULL || subset_count == NULL ||
      usize_type_index == NULL ||
      (program->external_module_count != 0u &&
       program->external_module_count != 1u) ||
      (program->external_module_count == 0u &&
       program->external_symbol_count != 0u) ||
      (program->external_module_count == 1u &&
       program->external_symbol_count != 4u &&
       program->external_symbol_count != 7u))
    return false;
  const size_t base =
      4u + (program->external_module_count == 0u ? 0u : 3u);
  if (program->enum_count > SIZE_MAX - base ||
      program->type_count < base + program->enum_count)
    return false;
  *subset_base = base + program->enum_count;
  *usize_type_index = W_SEED_HIR0_NONE;
  if (program->external_symbol_count == 7u) {
    if (program->type_count == *subset_base) return false;
    *usize_type_index = (uint32_t)(program->type_count - 1u);
    *subset_count = program->type_count - *subset_base - 1u;
  } else {
    *subset_count = program->type_count - *subset_base;
  }
  return true;
}

static bool hir_external_pair_valid(const w_seed_hir0_program *program,
                                    uint32_t module_index,
                                    uint32_t symbol_index,
                                    w_seed_hir0_external_kind kind) {
  if (program == NULL || program->external_module_count != 1u ||
      (program->external_symbol_count != 4u &&
       program->external_symbol_count != 7u) || module_index != 0u ||
      symbol_index >= program->external_symbol_count ||
      !verify_external_records(program))
    return false;
  const w_seed_hir0_external_symbol *symbol =
      &program->external_symbols[symbol_index];
  return symbol->kind == kind;
}

static bool verify_enum_records(const w_seed_hir0_program *program) {
  if (program == NULL) return false;
  size_t case_cursor = 0u;
  size_t payload_cursor = 0u;
  const size_t type_base =
      4u + (program->external_module_count == 0u ? 0u : 3u);
  for (size_t index = 0u; index < program->enum_count; index += 1u) {
    const w_seed_hir0_enum *decl = &program->enums[index];
    if (decl->module_index >= program->module_count ||
        decl->type_index != type_base + index ||
        decl->type_index >= program->type_count ||
        decl->first_case != case_cursor || decl->case_count == 0u ||
        !range_valid(decl->first_case, decl->case_count,
                     program->enum_case_count) ||
        !hir_text_valid(program, decl->name) || decl->name.count == 0u ||
        !span_valid(decl->source_span,
                    program->modules[decl->module_index].source_length))
      return false;
    const w_seed_hir0_type *type = &program->types[decl->type_index];
    if (type->kind != W_SEED_HIR0_TYPE_ENUM ||
        type->owner_module != decl->module_index ||
        type->enum_index != index ||
        type->external_module_index != W_SEED_HIR0_NONE ||
        type->external_symbol_index != W_SEED_HIR0_NONE ||
        type->first_subset_member != W_SEED_HIR0_NONE ||
        type->subset_member_count != 0u ||
        type->lifecycle != W_SEED_HIR0_LIFECYCLE_VALUE_COPY ||
        type->release_contract != W_SEED_HIR0_RELEASE_CONTRACT_NONE ||
        !hir_text_equal(program, type->name, decl->name))
      return false;
    for (size_t prior = 0u; prior < index; prior += 1u)
      if (program->enums[prior].module_index == decl->module_index &&
          hir_text_equal(program, program->enums[prior].name, decl->name))
        return false;
    for (size_t ordinal = 0u; ordinal < decl->case_count; ordinal += 1u) {
      const size_t case_index = case_cursor + ordinal;
      const w_seed_hir0_enum_case *value = &program->enum_cases[case_index];
      if (value->owner_enum != index || value->ordinal != ordinal ||
          value->tag != ordinal || value->first_payload != payload_cursor ||
          !range_valid(value->first_payload, value->payload_count,
                       program->enum_case_parameter_count) ||
          !hir_text_valid(program, value->name) || value->name.count == 0u ||
          !span_valid(value->source_span,
                      program->modules[decl->module_index].source_length))
        return false;
      for (size_t prior = 0u; prior < ordinal; prior += 1u)
        if (hir_text_equal(program, value->name,
                           program->enum_cases[case_cursor + prior].name))
          return false;
      for (size_t payload_ordinal = 0u;
           payload_ordinal < value->payload_count; payload_ordinal += 1u) {
        const size_t payload_index = payload_cursor + payload_ordinal;
        const w_seed_hir0_enum_case_parameter *payload =
            &program->enum_case_parameters[payload_index];
        if (payload->owner_case != case_index ||
            payload->ordinal != payload_ordinal ||
            (payload->type_index != W_SEED_HIR0_TYPE_I64 &&
             payload->type_index != W_SEED_HIR0_TYPE_BOOL) ||
            !hir_text_valid(program, payload->label) ||
            payload->has_label != (payload->label.count != 0u) ||
            !span_valid(payload->source_span,
                        program->modules[decl->module_index].source_length))
          return false;
        if (payload->has_label)
          for (size_t prior = 0u; prior < payload_ordinal; prior += 1u) {
            const w_seed_hir0_enum_case_parameter *previous =
                &program->enum_case_parameters[payload_cursor + prior];
            if (previous->has_label &&
                hir_text_equal(program, previous->label, payload->label))
              return false;
          }
      }
      if (!add_size(payload_cursor, value->payload_count, &payload_cursor))
        return false;
    }
    if (!add_size(case_cursor, decl->case_count, &case_cursor)) return false;
  }
  return case_cursor == program->enum_case_count &&
         payload_cursor == program->enum_case_parameter_count;
}

static bool verify_enum_subset_records(const w_seed_hir0_program *program) {
  size_t subset_base = 0u;
  size_t subset_count = 0u;
  uint32_t usize_type_index = W_SEED_HIR0_NONE;
  if (!hir_enum_subset_layout(program, &subset_base, &subset_count,
                              &usize_type_index))
    return false;
  (void)usize_type_index;
  size_t member_cursor = 0u;
  for (size_t ordinal = 0u; ordinal < subset_count; ordinal += 1u) {
    const size_t type_index = subset_base + ordinal;
    const w_seed_hir0_type *type = &program->types[type_index];
    if (type->kind != W_SEED_HIR0_TYPE_ENUM_SUBSET ||
        type->enum_index >= program->enum_count ||
        type->owner_module != program->enums[type->enum_index].module_index ||
        type->external_module_index != W_SEED_HIR0_NONE ||
        type->external_symbol_index != W_SEED_HIR0_NONE ||
        type->lifecycle != W_SEED_HIR0_LIFECYCLE_VALUE_COPY ||
        type->release_contract != W_SEED_HIR0_RELEASE_CONTRACT_NONE ||
        type->first_subset_member != member_cursor ||
        type->subset_member_count == 0u ||
        type->subset_member_count == program->enums[type->enum_index].case_count ||
        !range_valid(type->first_subset_member, type->subset_member_count,
                     program->enum_subset_member_count) ||
        !hir_text_equal(program, type->name,
                        program->enums[type->enum_index].name))
      return false;
    for (size_t prior = 0u; prior < ordinal; prior += 1u) {
      const w_seed_hir0_type *previous = &program->types[subset_base + prior];
      if (previous->enum_index != type->enum_index ||
          previous->subset_member_count != type->subset_member_count)
        continue;
      bool same_set = true;
      for (size_t member_ordinal = 0u;
           member_ordinal < type->subset_member_count; member_ordinal += 1u)
        if (program->enum_subset_members[
                (size_t)previous->first_subset_member + member_ordinal]
                .enum_case_index !=
            program->enum_subset_members[member_cursor + member_ordinal]
                .enum_case_index) {
          same_set = false;
          break;
        }
      if (same_set) return false;
    }
    const w_seed_hir0_enum *base = &program->enums[type->enum_index];
    for (size_t case_ordinal = 0u; case_ordinal < base->case_count;
         case_ordinal += 1u)
      if (program->enum_cases[(size_t)base->first_case + case_ordinal]
              .payload_count != 0u)
        return false;
    uint32_t previous_case = W_SEED_HIR0_NONE;
    for (size_t member_ordinal = 0u;
         member_ordinal < type->subset_member_count; member_ordinal += 1u) {
      const size_t member_index = member_cursor + member_ordinal;
      const w_seed_hir0_enum_subset_member *member =
          &program->enum_subset_members[member_index];
      if (member->owner_type != type_index ||
          member->ordinal != member_ordinal ||
          member->enum_index != type->enum_index ||
          member->enum_case_index >= program->enum_case_count ||
          member->enum_case_index < base->first_case ||
          member->enum_case_index >= base->first_case + base->case_count ||
          (previous_case != W_SEED_HIR0_NONE &&
           previous_case >= member->enum_case_index) ||
          program->enum_cases[member->enum_case_index].payload_count != 0u ||
          !span_valid(member->source_span,
                      program->modules[type->owner_module].source_length))
        return false;
      previous_case = member->enum_case_index;
    }
    if (!add_size(member_cursor, type->subset_member_count, &member_cursor))
      return false;
  }
  return member_cursor == program->enum_subset_member_count;
}

static bool hir_type_index_valid(const w_seed_hir0_program *program,
                                 uint32_t type_index) {
  if (program == NULL || type_index >= program->type_count) return false;
  const w_seed_hir0_type *type = &program->types[type_index];
  if (type_index < 4u) {
    const w_seed_hir0_type_kind expected[] = {
        W_SEED_HIR0_TYPE_UNIT, W_SEED_HIR0_TYPE_STRING,
        W_SEED_HIR0_TYPE_I64, W_SEED_HIR0_TYPE_BOOL};
    return type->kind == expected[type_index] &&
           type->owner_module == W_SEED_HIR0_NONE &&
           type->external_module_index == W_SEED_HIR0_NONE &&
           type->external_symbol_index == W_SEED_HIR0_NONE &&
           type->enum_index == W_SEED_HIR0_NONE &&
           type->first_subset_member == W_SEED_HIR0_NONE &&
           type->subset_member_count == 0u;
  }
  const size_t external_base =
      4u + (program->external_module_count == 0u ? 0u : 3u);
  if (type_index < external_base)
    return type->kind == W_SEED_HIR0_TYPE_NOMINAL &&
           type->owner_module == W_SEED_HIR0_NONE &&
           type->enum_index == W_SEED_HIR0_NONE &&
           type->first_subset_member == W_SEED_HIR0_NONE &&
           type->subset_member_count == 0u &&
           hir_external_pair_valid(program, type->external_module_index,
                                   type->external_symbol_index,
                                   W_SEED_HIR0_EXTERNAL_TYPE) &&
           type->external_symbol_index == type_index - 4u &&
           hir_text_equal(
               program, type->name,
               program->external_symbols[type->external_symbol_index].name);
  const size_t enum_index = type_index - external_base;
  if (enum_index < program->enum_count)
    return type->kind == W_SEED_HIR0_TYPE_ENUM &&
           type->enum_index == enum_index && type->owner_module ==
               program->enums[enum_index].module_index &&
           type->external_module_index == W_SEED_HIR0_NONE &&
           type->external_symbol_index == W_SEED_HIR0_NONE &&
           type->first_subset_member == W_SEED_HIR0_NONE &&
           type->subset_member_count == 0u;
  size_t subset_base = 0u;
  size_t subset_count = 0u;
  uint32_t usize_type_index = W_SEED_HIR0_NONE;
  if (!hir_enum_subset_layout(program, &subset_base, &subset_count,
                              &usize_type_index))
    return false;
  if (type_index >= subset_base &&
      type_index < subset_base + subset_count) {
    const size_t ordinal = type_index - subset_base;
    return type->kind == W_SEED_HIR0_TYPE_ENUM_SUBSET &&
           type->enum_index < program->enum_count &&
           type->first_subset_member != W_SEED_HIR0_NONE &&
           type->subset_member_count != 0u &&
           type->owner_module == program->enums[type->enum_index].module_index &&
           type->external_module_index == W_SEED_HIR0_NONE &&
           type->external_symbol_index == W_SEED_HIR0_NONE &&
           (size_t)type_index == subset_base + ordinal;
  }
  return usize_type_index != W_SEED_HIR0_NONE &&
         type_index == usize_type_index &&
         type->kind == W_SEED_HIR0_TYPE_USIZE &&
         type->owner_module == W_SEED_HIR0_NONE &&
         type->external_module_index == W_SEED_HIR0_NONE &&
         type->external_symbol_index == W_SEED_HIR0_NONE &&
         type->enum_index == W_SEED_HIR0_NONE &&
         type->first_subset_member == W_SEED_HIR0_NONE &&
         type->subset_member_count == 0u &&
          hir_text_is(program, type->name, HIR0_USIZE_NAME);
}

/* The HIR carries no conversion instruction for the bounded subset widening:
 * a subset value may flow directly to its payloadless base enum.  Keep this
 * relation directional and nominal; base-to-subset and cross-enum flows are
 * never assignable. */
static bool hir_type_assignable(const w_seed_hir0_program *program,
                                uint32_t from_type, uint32_t to_type) {
  if (program == NULL || !hir_type_index_valid(program, from_type) ||
      !hir_type_index_valid(program, to_type))
    return false;
  if (from_type == to_type) return true;
  const w_seed_hir0_type *from = &program->types[from_type];
  const w_seed_hir0_type *to = &program->types[to_type];
  return from->kind == W_SEED_HIR0_TYPE_ENUM_SUBSET &&
         to->kind == W_SEED_HIR0_TYPE_ENUM &&
         from->enum_index == to->enum_index;
}

static bool verify_block_argument_records(const w_seed_hir0_program *program) {
  if (program == NULL) return false;
  size_t cursor = 0u;
  for (size_t block_index = 0u; block_index < program->block_count;
       block_index += 1u) {
    const w_seed_hir0_block *block = &program->blocks[block_index];
    if (block->block_argument_count == 0u) {
      if (block->first_block_argument != W_SEED_HIR0_NONE) return false;
      continue;
    }
    if (block->first_block_argument != cursor ||
        !range_valid(block->first_block_argument, block->block_argument_count,
                     program->block_argument_count))
      return false;
    const size_t function = block->owner_function;
    if (function >= program->function_count) return false;
    const size_t module = program->functions[function].module_index;
    if (module >= program->module_count) return false;
    for (size_t ordinal = 0u; ordinal < block->block_argument_count;
         ordinal += 1u) {
      const w_seed_hir0_block_argument *argument =
          &program->block_arguments[cursor + ordinal];
      if (argument->owner_block != block_index ||
          argument->ordinal != ordinal ||
          (argument->type_index != 2u && argument->type_index != 3u) ||
          !span_valid(argument->source_span,
                      program->modules[module].source_length))
        return false;
    }
    cursor += block->block_argument_count;
  }
  return cursor == program->block_argument_count;
}

static bool verify_edge_argument_records(const w_seed_hir0_program *program) {
  if (program == NULL) return false;
  size_t cursor = 0u;
  for (size_t terminator_index = 0u;
       terminator_index < program->terminator_count; terminator_index += 1u) {
    const w_seed_hir0_terminator *terminator =
        &program->terminators[terminator_index];
    if (terminator->edge_argument_count == 0u) {
      if (terminator->first_edge_argument != W_SEED_HIR0_NONE) return false;
      continue;
    }
    if (terminator->first_edge_argument != cursor ||
        !range_valid(terminator->first_edge_argument,
                     terminator->edge_argument_count,
                     program->edge_argument_count) ||
        terminator->owner_block >= program->block_count)
      return false;
    const size_t function = program->blocks[terminator->owner_block].owner_function;
    if (function >= program->function_count ||
        program->functions[function].module_index >= program->module_count)
      return false;
    const size_t source_length =
        program->modules[program->functions[function].module_index].source_length;
    for (size_t ordinal = 0u; ordinal < terminator->edge_argument_count;
         ordinal += 1u) {
      const w_seed_hir0_edge_argument *edge =
          &program->edge_arguments[cursor + ordinal];
      if (edge->owner_terminator != terminator_index ||
          edge->owner_block != terminator->owner_block ||
          edge->ordinal != ordinal || edge->value_index >= program->value_count ||
          (edge->type_index != 2u && edge->type_index != 3u) ||
          !span_valid(edge->source_span, source_length) ||
          !span_equal(edge->source_span, terminator->source_span))
        return false;
    }
    cursor += terminator->edge_argument_count;
  }
  return cursor == program->edge_argument_count;
}

/* The bounded post-test carrier arguments dominate the condition and exit.
 * Keep this relation exact so ordinary cross-block block-argument reads
 * remain rejected while the explicit repeat CFG remains available. */
static bool post_test_loop_carrier_dominates(
    const w_seed_hir0_program *program, uint32_t carrier_index,
    uint32_t use_block) {
  if (program == NULL || carrier_index == 0u ||
      carrier_index + 3u >= program->block_count ||
      (use_block != carrier_index + 1u && use_block != carrier_index + 2u &&
       use_block != carrier_index + 3u))
    return false;
  const uint32_t preheader_index = carrier_index - 1u;
  const uint32_t condition_index = carrier_index + 1u;
  const uint32_t latch_index = carrier_index + 2u;
  const uint32_t exit_index = carrier_index + 3u;
  const w_seed_hir0_block *preheader = &program->blocks[preheader_index];
  const w_seed_hir0_block *carrier = &program->blocks[carrier_index];
  const w_seed_hir0_block *condition = &program->blocks[condition_index];
  const w_seed_hir0_block *latch = &program->blocks[latch_index];
  const w_seed_hir0_block *exit = &program->blocks[exit_index];
  if (preheader->owner_function != carrier->owner_function ||
      condition->owner_function != carrier->owner_function ||
      exit->owner_function != carrier->owner_function ||
      carrier->block_argument_count == 0u ||
      carrier->first_block_argument == W_SEED_HIR0_NONE ||
      !range_valid(carrier->first_block_argument,
                   carrier->block_argument_count,
                   program->block_argument_count) ||
      condition->block_argument_count != 0u ||
      latch->block_argument_count != 0u ||
      exit->block_argument_count != 0u ||
      preheader->terminator_index >= program->terminator_count ||
      carrier->terminator_index >= program->terminator_count ||
      condition->terminator_index >= program->terminator_count ||
      latch->terminator_index >= program->terminator_count)
    return false;
  const w_seed_hir0_terminator *preheader_term =
      &program->terminators[preheader->terminator_index];
  const w_seed_hir0_terminator *carrier_term =
      &program->terminators[carrier->terminator_index];
  const w_seed_hir0_terminator *condition_term =
      &program->terminators[condition->terminator_index];
  const w_seed_hir0_terminator *latch_term =
      &program->terminators[latch->terminator_index];
  return preheader_term->owner_block == preheader_index &&
         preheader_term->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
         preheader_term->target_block == carrier_index &&
         preheader_term->edge_argument_count ==
             carrier->block_argument_count &&
         carrier_term->owner_block == carrier_index &&
         carrier_term->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
         carrier_term->target_block == condition_index &&
         carrier_term->edge_argument_count == 0u &&
         condition_term->owner_block == condition_index &&
         condition_term->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
         condition_term->target_block == latch_index &&
         condition_term->else_block == exit_index &&
         condition_term->result_type == 0u &&
         condition_term->logical_operator == W_SEED_HIR0_LOGICAL_NONE &&
         condition_term->first_edge_argument == W_SEED_HIR0_NONE &&
         condition_term->edge_argument_count == 0u &&
         latch_term->owner_block == latch_index &&
         latch_term->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
         latch_term->target_block == carrier_index &&
         latch_term->edge_argument_count == carrier->block_argument_count;
}

/* The bounded natural-loop header arguments are SSA definitions that dominate
 * both their body and exit.  Keep this relation exact so ordinary
 * cross-block block-argument reads remain rejected. */
static bool natural_loop_header_dominates(
    const w_seed_hir0_program *program, uint32_t header_index,
    uint32_t use_block) {
  if (post_test_loop_carrier_dominates(program, header_index, use_block))
    return true;
  if (program == NULL || header_index == 0u ||
      header_index >= program->block_count || use_block >= program->block_count)
    return false;
  const uint32_t preheader_index = header_index - 1u;
  const uint32_t body_index = header_index + 1u;
  const uint32_t exit_index = header_index + 2u;
  if (exit_index >= program->block_count ||
      (use_block != body_index && use_block != exit_index))
    return false;
  const w_seed_hir0_block *preheader = &program->blocks[preheader_index];
  const w_seed_hir0_block *header = &program->blocks[header_index];
  const w_seed_hir0_block *body = &program->blocks[body_index];
  const w_seed_hir0_block *exit = &program->blocks[exit_index];
  if (preheader->owner_function != header->owner_function ||
      body->owner_function != header->owner_function ||
      exit->owner_function != header->owner_function ||
      header->block_argument_count == 0u ||
      header->first_block_argument == W_SEED_HIR0_NONE ||
      header->terminator_index >= program->terminator_count ||
      preheader->terminator_index >= program->terminator_count ||
      body->terminator_index >= program->terminator_count)
    return false;
  const w_seed_hir0_terminator *preheader_term =
      &program->terminators[preheader->terminator_index];
  const w_seed_hir0_terminator *header_term =
      &program->terminators[header->terminator_index];
  const w_seed_hir0_terminator *body_term =
      &program->terminators[body->terminator_index];
  return preheader_term->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
         preheader_term->target_block == header_index &&
         preheader_term->edge_argument_count == header->block_argument_count &&
         header_term->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
         header_term->target_block == body_index &&
         header_term->else_block == exit_index &&
         header_term->result_type == 0u &&
         header_term->logical_operator == W_SEED_HIR0_LOGICAL_NONE &&
         body_term->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
         body_term->target_block == header_index &&
         body_term->edge_argument_count == header->block_argument_count;
}

static bool natural_loop_exit_binding_available(
    const w_seed_hir0_program *program, uint32_t use_block,
    uint32_t binding_block) {
  if (program == NULL || use_block >= program->block_count ||
      binding_block >= program->block_count)
    return false;
  if (post_test_loop_carrier_dominates(program, binding_block, use_block))
    return true;
  if (use_block < 2u || binding_block + 1u != use_block) return false;
  const uint32_t header = use_block - 2u;
  return natural_loop_header_dominates(program, header, use_block);
}

static uint32_t hir0_external_type_index(const w_seed_hir0_program *program,
                                         uint32_t symbol_index) {
  if (program == NULL || program->external_module_count != 1u) return W_SEED_HIR0_NONE;
  for (size_t index = 4u; index < program->type_count; index += 1u) {
    const w_seed_hir0_type *type = &program->types[index];
    if (type->kind == W_SEED_HIR0_TYPE_NOMINAL &&
        type->external_module_index == 0u &&
        type->external_symbol_index == symbol_index)
      return (uint32_t)index;
  }
  return W_SEED_HIR0_NONE;
}

static bool hir0_usize_count_comparison_operands(
    const w_seed_hir0_program *program, const w_seed_hir0_value *value) {
  if (program == NULL || value == NULL ||
      value->left_value >= program->value_count ||
      value->right_value >= program->value_count ||
      program->external_symbol_count != 7u ||
      program->type_count == 0u || program->type_count - 1u > UINT32_MAX)
    return false;
  const uint32_t usize_type = (uint32_t)(program->type_count - 1u);
  const w_seed_hir0_value *left = &program->values[value->left_value];
  const w_seed_hir0_value *right = &program->values[value->right_value];
  const bool left_count =
      left->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
      left->external_module_index == 0u &&
      left->external_symbol_index == 6u && left->type_index == usize_type &&
      hir_text_is(program, left->member_name, HIR0_PROCESS_COUNT);
  const bool right_count =
      right->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
      right->external_module_index == 0u &&
      right->external_symbol_index == 6u && right->type_index == usize_type &&
      hir_text_is(program, right->member_name, HIR0_PROCESS_COUNT);
  const bool left_literal = left->kind == W_SEED_HIR0_VALUE_CONST_USIZE &&
                            left->type_index == usize_type;
  const bool right_literal = right->kind == W_SEED_HIR0_VALUE_CONST_USIZE &&
                             right->type_index == usize_type;
  return (left_count && right_literal) || (right_count && left_literal);
}

static bool hir_enum_subset_contains_case(
    const w_seed_hir0_program *program, uint32_t type_index,
    uint32_t enum_case_index) {
  if (program == NULL || type_index >= program->type_count ||
      enum_case_index >= program->enum_case_count)
    return false;
  const w_seed_hir0_type *type = &program->types[type_index];
  if (type->kind != W_SEED_HIR0_TYPE_ENUM_SUBSET ||
      type->first_subset_member == W_SEED_HIR0_NONE ||
      !range_valid(type->first_subset_member, type->subset_member_count,
                   program->enum_subset_member_count))
    return false;
  for (size_t ordinal = 0u; ordinal < type->subset_member_count; ordinal += 1u)
    if (program->enum_subset_members[(size_t)type->first_subset_member + ordinal]
            .enum_case_index == enum_case_index)
      return true;
  return false;
}

static bool hir_enum_subset_case_for_ordinal(
    const w_seed_hir0_program *program, uint32_t type_index, size_t ordinal,
    uint32_t *enum_case_index) {
  if (program == NULL || enum_case_index == NULL ||
      type_index >= program->type_count ||
      program->types[type_index].kind != W_SEED_HIR0_TYPE_ENUM_SUBSET ||
      ordinal >= program->types[type_index].subset_member_count ||
      program->types[type_index].first_subset_member == W_SEED_HIR0_NONE ||
      !range_valid(program->types[type_index].first_subset_member,
                   program->types[type_index].subset_member_count,
                   program->enum_subset_member_count))
    return false;
  *enum_case_index = program->enum_subset_members[
      (size_t)program->types[type_index].first_subset_member + ordinal]
                           .enum_case_index;
  return *enum_case_index < program->enum_case_count;
}

static bool verify_value_tree(
    const w_seed_hir0_program *program, uint32_t root_index,
    w_seed_hir0_value_owner_kind owner_kind, uint32_t owner_index,
    uint32_t owner_ordinal, uint32_t current_block,
    uint32_t current_instruction, size_t source_length, size_t depth,
    size_t *value_cursor, size_t *segment_cursor, size_t *byte_cursor) {
  if (program == NULL || value_cursor == NULL || segment_cursor == NULL ||
      byte_cursor == NULL || depth > 256u ||
      (size_t)root_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[root_index];
  if (value->owner_kind != owner_kind || value->owner_index != owner_index ||
      value->owner_ordinal != owner_ordinal ||
      !span_valid(value->source_span, source_length) ||
      (value->kind != W_SEED_HIR0_VALUE_CALL_RESULT &&
       value->call_index != W_SEED_HIR0_NONE) ||
      (value->kind != W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE &&
       value->kind != W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
       (value->external_module_index != W_SEED_HIR0_NONE ||
        value->external_symbol_index != W_SEED_HIR0_NONE ||
        value->member_name.count != 0u)) ||
      (value->kind != W_SEED_HIR0_VALUE_ENUM_CASE &&
       (value->enum_index != W_SEED_HIR0_NONE ||
        value->enum_case_index != W_SEED_HIR0_NONE)) ||
      (value->kind != W_SEED_HIR0_VALUE_ENUM_CASE &&
       (value->first_enum_payload != 0u || value->enum_payload_count != 0u)) ||
      (value->kind != W_SEED_HIR0_VALUE_PATTERN_CAPTURE_READ &&
       value->pattern_capture_index != W_SEED_HIR0_NONE) ||
      (value->kind != W_SEED_HIR0_VALUE_CONST_USIZE &&
       value->unsigned_integer_value != 0u))
    return false;

  if (value->kind == W_SEED_HIR0_VALUE_PATTERN_CAPTURE_READ) {
    if (value->pattern_capture_index >= program->switch_capture_count ||
        (value->type_index != W_SEED_HIR0_TYPE_I64 &&
         value->type_index != W_SEED_HIR0_TYPE_BOOL) ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->left_value != W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u ||
        root_index != *value_cursor)
      return false;
    const w_seed_hir0_switch_capture *capture =
        &program->switch_captures[value->pattern_capture_index];
    if (capture->type_index != value->type_index ||
        capture->owner_switch_edge >= program->switch_edge_count ||
        program->switch_edges[capture->owner_switch_edge].target_block !=
            current_block)
      return false;
    *value_cursor += 1u;
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER) {
    const uint32_t arguments_type = hir0_external_type_index(program, 0u);
    const bool is_empty = value->external_symbol_index == 4u &&
                          hir_text_is(program, value->member_name,
                                      HIR0_PROCESS_IS_EMPTY);
    const bool is_count = value->external_symbol_index == 6u &&
                          hir_text_is(program, value->member_name,
                                      HIR0_PROCESS_COUNT);
    const uint32_t usize_type = program->type_count == 0u
                                    ? W_SEED_HIR0_NONE
                                    : (uint32_t)(program->type_count - 1u);
    if ((is_empty && value->type_index != 3u) ||
        (is_count && (usize_type >= program->type_count ||
                      value->type_index != usize_type ||
                      program->types[usize_type].kind != W_SEED_HIR0_TYPE_USIZE)) ||
        (!is_empty && !is_count) ||
        arguments_type == W_SEED_HIR0_NONE ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u ||
        !hir_external_pair_valid(program, value->external_module_index,
                                 value->external_symbol_index,
                                 W_SEED_HIR0_EXTERNAL_VALUE) ||
        !hir_text_equal(
            program, value->member_name,
            program->external_symbols[value->external_symbol_index].name) ||
        !verify_value_tree(
            program, value->left_value,
            W_SEED_HIR0_VALUE_OWNER_EXTERNAL_MEMBER, root_index, 0u,
            current_block, current_instruction, source_length, depth + 1u,
            value_cursor, segment_cursor, byte_cursor))
      return false;
    const w_seed_hir0_value *receiver = &program->values[value->left_value];
    if ((size_t)root_index != *value_cursor ||
        receiver->kind != W_SEED_HIR0_VALUE_PARAMETER_READ ||
        receiver->type_index != arguments_type ||
        receiver->parameter_index >= program->parameter_count ||
        program->parameters[receiver->parameter_index].ordinal != 0u)
      return false;
    *value_cursor += 1u;
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE) {
    const uint32_t exit_code_type = hir0_external_type_index(program, 2u);
    if (value->type_index != exit_code_type || exit_code_type == W_SEED_HIR0_NONE ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u ||
        value->enum_index != W_SEED_HIR0_NONE ||
        value->enum_case_index != W_SEED_HIR0_NONE ||
        !hir_external_pair_valid(program, value->external_module_index,
                                 value->external_symbol_index,
                                 W_SEED_HIR0_EXTERNAL_VALUE) ||
        !hir_text_equal(
            program, value->member_name,
            program->external_symbols[value->external_symbol_index].name))
      return false;
    if (value->external_symbol_index == 3u) {
      if (value->left_value != W_SEED_HIR0_NONE ||
          !hir_text_is(program, value->member_name, HIR0_PROCESS_SUCCESS) ||
          program->external_symbols[3].parameter_abi !=
              W_SEED_HIR0_EXTERNAL_PARAMETER_NONE)
        return false;
    } else if (value->external_symbol_index == 5u) {
      if (value->left_value == W_SEED_HIR0_NONE ||
          !hir_text_is(program, value->member_name, HIR0_PROCESS_FAILURE) ||
          program->external_symbols[5].parameter_abi !=
              W_SEED_HIR0_EXTERNAL_PARAMETER_PROCESS_FAILURE_I64 ||
          !verify_value_tree(
              program, value->left_value,
              W_SEED_HIR0_VALUE_OWNER_EXTERNAL_ENUM_CASE, root_index, 0u,
              current_block, current_instruction, source_length, depth + 1u,
              value_cursor, segment_cursor, byte_cursor))
        return false;
      const w_seed_hir0_value *payload = &program->values[value->left_value];
      if (payload->kind != W_SEED_HIR0_VALUE_CONST_I64 ||
          payload->type_index != 2u || payload->integer_value < 1 ||
          payload->integer_value > 255)
        return false;
    } else {
      return false;
    }
    if ((size_t)root_index != *value_cursor) return false;
    *value_cursor += 1u;
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_ENUM_CASE) {
    if (value->type_index >= program->type_count ||
        !hir_type_index_valid(program, value->type_index) ||
        (program->types[value->type_index].kind != W_SEED_HIR0_TYPE_ENUM &&
         program->types[value->type_index].kind !=
             W_SEED_HIR0_TYPE_ENUM_SUBSET) ||
        value->enum_index >= program->enum_count ||
        program->types[value->type_index].enum_index != value->enum_index ||
        value->enum_case_index >= program->enum_case_count ||
        program->enum_cases[value->enum_case_index].owner_enum !=
            value->enum_index ||
        (program->types[value->type_index].kind ==
             W_SEED_HIR0_TYPE_ENUM_SUBSET &&
         !hir_enum_subset_contains_case(program, value->type_index,
                                        value->enum_case_index)) ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->left_value != W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        !range_valid(value->first_enum_payload, value->enum_payload_count,
                     program->enum_payload_count) ||
        program->enum_cases[value->enum_case_index].payload_count !=
            value->enum_payload_count ||
        (value->enum_payload_count == 0u && value->first_enum_payload != 0u) ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
    const w_seed_hir0_enum_case *enum_case =
        &program->enum_cases[value->enum_case_index];
    for (size_t ordinal = 0u; ordinal < value->enum_payload_count;
         ordinal += 1u) {
      const size_t payload_index =
          (size_t)value->first_enum_payload + ordinal;
      const w_seed_hir0_enum_payload *payload =
          &program->enum_payloads[payload_index];
      if (payload->owner_value != root_index || payload->ordinal != ordinal ||
          payload->parameter_ordinal >= enum_case->payload_count ||
          payload->value_index >= program->value_count ||
          (payload->type_index != W_SEED_HIR0_TYPE_I64 &&
           payload->type_index != W_SEED_HIR0_TYPE_BOOL) ||
          !span_valid(payload->source_span, source_length))
        return false;
      for (size_t prior = 0u; prior < ordinal; prior += 1u)
        if (program->enum_payloads[(size_t)value->first_enum_payload + prior]
                .parameter_ordinal == payload->parameter_ordinal)
          return false;
      const w_seed_hir0_enum_case_parameter *parameter =
          &program->enum_case_parameters[(size_t)enum_case->first_payload +
                                         payload->parameter_ordinal];
      if (parameter->owner_case != value->enum_case_index ||
          parameter->ordinal != payload->parameter_ordinal ||
          parameter->type_index != payload->type_index ||
          !verify_value_tree(
              program, payload->value_index,
              W_SEED_HIR0_VALUE_OWNER_ENUM_PAYLOAD, (uint32_t)payload_index,
              0u, current_block, current_instruction, source_length,
              depth + 1u, value_cursor, segment_cursor, byte_cursor) ||
          program->values[payload->value_index].type_index !=
              payload->type_index)
        return false;
    }
    if ((size_t)root_index != *value_cursor) return false;
    *value_cursor += 1u;
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON) {
    if (value->binary_operator < W_SEED_HIR0_BINARY_EQUAL ||
        value->binary_operator > W_SEED_HIR0_BINARY_GREATER_EQUAL ||
        !hir_type_index_valid(program, value->type_index) ||
        program->types[value->type_index].kind != W_SEED_HIR0_TYPE_BOOL ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->right_value == W_SEED_HIR0_NONE ||
        !hir0_usize_count_comparison_operands(program, value) ||
        !verify_value_tree(program, value->left_value,
                           W_SEED_HIR0_VALUE_OWNER_BINARY, root_index, 0u,
                           current_block, current_instruction, source_length,
                           depth + 1u, value_cursor, segment_cursor,
                           byte_cursor) ||
        !verify_value_tree(program, value->right_value,
                           W_SEED_HIR0_VALUE_OWNER_BINARY, root_index, 1u,
                           current_block, current_instruction, source_length,
                           depth + 1u, value_cursor, segment_cursor,
                           byte_cursor) ||
        (size_t)root_index != *value_cursor ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->binary_operator > W_SEED_HIR0_BINARY_GREATER_EQUAL ||
        value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
    *value_cursor += 1u;
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_BINARY_I64) {
    if ((uint32_t)value->binary_operator >
            (uint32_t)W_SEED_HIR0_BINARY_GREATER_EQUAL ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->right_value == W_SEED_HIR0_NONE ||
        !verify_value_tree(program, value->left_value,
                           W_SEED_HIR0_VALUE_OWNER_BINARY, root_index, 0u,
                           current_block, current_instruction, source_length,
                           depth + 1u, value_cursor, segment_cursor,
                           byte_cursor) ||
        !verify_value_tree(program, value->right_value,
                           W_SEED_HIR0_VALUE_OWNER_BINARY, root_index, 1u,
                           current_block, current_instruction, source_length,
                           depth + 1u, value_cursor, segment_cursor,
                           byte_cursor) ||
        (size_t)root_index != *value_cursor ||
        program->values[value->left_value].type_index != 2u ||
        program->values[value->right_value].type_index != 2u ||
        value->type_index !=
            (value->binary_operator >= W_SEED_HIR0_BINARY_EQUAL ? 3u : 2u) ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
    *value_cursor += 1u;
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_UNARY_BOOL) {
    if (value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
        value->type_index != 3u || value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u ||
        !verify_value_tree(
            program, value->left_value, W_SEED_HIR0_VALUE_OWNER_UNARY,
            root_index, 0u, current_block, current_instruction, source_length,
            depth + 1u, value_cursor, segment_cursor, byte_cursor) ||
        (size_t)root_index != *value_cursor ||
        program->values[value->left_value].type_index != 3u)
      return false;
    *value_cursor += 1u;
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64) {
    if (value->unary_operator != W_SEED_HIR0_UNARY_NEGATE ||
        value->type_index != 2u || value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u ||
        !verify_value_tree(
            program, value->left_value, W_SEED_HIR0_VALUE_OWNER_UNARY,
            root_index, 0u, current_block, current_instruction, source_length,
            depth + 1u, value_cursor, segment_cursor, byte_cursor) ||
        (size_t)root_index != *value_cursor ||
        program->values[value->left_value].type_index != 2u)
      return false;
    *value_cursor += 1u;
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ) {
    if ((size_t)root_index != *value_cursor ||
        (value->type_index != 2u && value->type_index != 3u) ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->left_value != W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
        value->block_argument_index == W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u ||
        current_block >= program->block_count)
      return false;
    const w_seed_hir0_block_argument *argument =
        (size_t)value->block_argument_index < program->block_argument_count
            ? &program->block_arguments[value->block_argument_index]
            : NULL;
    const bool local_argument =
        argument != NULL && argument->owner_block == current_block;
    const bool loop_argument =
        argument != NULL &&
        natural_loop_header_dominates(program, argument->owner_block,
                                      current_block);
    const bool loop_header_argument =
        argument != NULL && argument->owner_block + 1u < program->block_count &&
        natural_loop_header_dominates(program, argument->owner_block,
                                      argument->owner_block + 1u);
    const w_seed_hir0_block *argument_block =
        argument == NULL ? NULL : &program->blocks[argument->owner_block];
    if (argument == NULL || (!local_argument && !loop_argument) ||
        argument_block->first_block_argument == W_SEED_HIR0_NONE ||
        value->block_argument_index < argument_block->first_block_argument ||
        (size_t)value->block_argument_index >=
            (size_t)argument_block->first_block_argument +
                argument_block->block_argument_count ||
        argument->ordinal != value->block_argument_index -
                                 argument_block->first_block_argument ||
        argument->type_index != value->type_index ||
        (local_argument && !loop_header_argument &&
         !span_equal(value->source_span, argument->source_span)))
      return false;
    *value_cursor += 1u;
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
    if (value->type_index != 1u ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->left_value != W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != *segment_cursor ||
        value->interpolation_segment_count == 0u ||
        !range_valid(value->first_interpolation_segment,
                     value->interpolation_segment_count,
                     program->interpolation_segment_count) ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
    for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
         ordinal += 1u) {
      const size_t index = *segment_cursor;
      const w_seed_hir0_interpolation_segment *segment =
          &program->interpolation_segments[index];
      if (segment->owner_value != root_index || segment->ordinal != ordinal ||
          !span_valid(segment->source_span, source_length))
        return false;
      *segment_cursor += 1u;
      if (segment->kind == W_SEED_HIR0_INTERPOLATION_TEXT) {
        if (segment->value_index != W_SEED_HIR0_NONE ||
            (size_t)segment->byte_offset != *byte_cursor ||
            !byte_slice_valid(program, segment->byte_offset,
                              segment->byte_count) ||
            !add_size(*byte_cursor, segment->byte_count, byte_cursor))
          return false;
      } else if (segment->kind == W_SEED_HIR0_INTERPOLATION_VALUE) {
        if (segment->byte_offset != 0u || segment->byte_count != 0u ||
            !verify_value_tree(
                program, segment->value_index,
                W_SEED_HIR0_VALUE_OWNER_INTERPOLATION_SEGMENT,
                (uint32_t)index, 0u, current_block, current_instruction,
                source_length, depth + 1u, value_cursor, segment_cursor,
                byte_cursor))
          return false;
      } else {
        return false;
      }
    }
    if ((size_t)root_index != *value_cursor) return false;
    *value_cursor += 1u;
    return true;
  }

  if ((size_t)root_index != *value_cursor ||
      value->left_value != W_SEED_HIR0_NONE ||
      value->right_value != W_SEED_HIR0_NONE ||
      value->first_interpolation_segment != W_SEED_HIR0_NONE ||
      value->interpolation_segment_count != 0u ||
      value->binary_operator != W_SEED_HIR0_BINARY_ADD)
    return false;
  if (value->kind == W_SEED_HIR0_VALUE_CALL_RESULT) {
    if (!hir_type_index_valid(program, value->type_index) ||
        (value->type_index < 4u && value->type_index != 2u &&
         value->type_index != 3u) ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index == W_SEED_HIR0_NONE ||
        (size_t)value->call_index >= program->call_count ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
    const w_seed_hir0_call *call = &program->calls[value->call_index];
    if (call->owner_block != current_block ||
        call->owner_instruction >= current_instruction ||
        call->result_type != value->type_index ||
        program->identities[call->callee_identity].kind !=
            W_SEED_HIR0_IDENTITY_FUNCTION)
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_CONST_STRING) {
    if (value->type_index != 1u ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->bool_value ||
        (size_t)value->byte_offset != *byte_cursor ||
        !byte_slice_valid(program, value->byte_offset, value->byte_count) ||
        !add_size(*byte_cursor, value->byte_count, byte_cursor))
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
    if (!hir_type_index_valid(program, value->type_index) ||
        value->type_index == 0u ||
        value->binding_index == W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        (size_t)value->binding_index >= program->binding_count ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
    const w_seed_hir0_binding *binding =
        &program->bindings[value->binding_index];
    const w_seed_hir0_block *use_block = &program->blocks[current_block];
    const w_seed_hir0_function *function =
        &program->functions[use_block->owner_function];
    const bool available_in_block =
        binding->owner_block == current_block
            ? binding->owner_instruction < current_instruction
            : binding->owner_block == function->first_block ||
                  natural_loop_exit_binding_available(
                      program, (uint32_t)current_block,
                      binding->owner_block);
    if (!available_in_block ||
        program->blocks[binding->owner_block].owner_function !=
            use_block->owner_function ||
        binding->type_index != value->type_index)
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_PARAMETER_READ) {
    if (!hir_type_index_valid(program, value->type_index) ||
        value->type_index == 0u ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index == W_SEED_HIR0_NONE ||
        (size_t)value->parameter_index >= program->parameter_count ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
    const w_seed_hir0_parameter *parameter =
        &program->parameters[value->parameter_index];
    const bool external_owner_type =
        value->type_index < program->type_count &&
        program->types[value->type_index].kind == W_SEED_HIR0_TYPE_NOMINAL &&
        (value->type_index == hir0_external_type_index(program, 0u) ||
         value->type_index == hir0_external_type_index(program, 1u));
    if (current_block >= program->block_count ||
        parameter->owner_function !=
            program->blocks[current_block].owner_function ||
        parameter->type_index != value->type_index ||
        (external_owner_type &&
         (owner_kind != W_SEED_HIR0_VALUE_OWNER_EXTERNAL_MEMBER ||
          value->type_index != hir0_external_type_index(program, 0u) ||
          parameter->ordinal != 0u)))
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_CONST_I64) {
    if (value->type_index != 2u ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_CONST_USIZE) {
    if (!hir_type_index_valid(program, value->type_index) ||
        program->types[value->type_index].kind != W_SEED_HIR0_TYPE_USIZE ||
        value->owner_kind != W_SEED_HIR0_VALUE_OWNER_BINARY ||
        value->owner_index >= program->value_count ||
        program->values[value->owner_index].kind !=
            W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_CONST_BOOL) {
    if (value->type_index != 3u ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->byte_offset != 0u ||
        value->byte_count != 0u)
      return false;
  } else {
    return false;
  }
  *value_cursor += 1u;
  return true;
}

/* The block range is a structured layout proof. A branch starts its true arm
 * at the next block. The true arm ends before the false arm, and both terminal
 * jumps name one forward join. Recursion is limited by the HIR nesting bound. */
static bool verify_cfg_arm(const w_seed_hir0_program *program,
                           uint32_t function_index, size_t start,
                           size_t end, size_t depth, size_t *last_block,
                           size_t *join_block);

static bool verify_logical_jump_shape(
    const w_seed_hir0_program *program, const w_seed_hir0_terminator *branch,
    size_t branch_block, size_t jump_block, size_t join_block, bool logical_edge,
    bool skip, bool skip_value, size_t source_length) {
  if (program == NULL || branch == NULL || branch_block >= program->block_count ||
      jump_block >= program->block_count || join_block >= program->block_count ||
      branch->first_edge_argument != W_SEED_HIR0_NONE ||
      branch->edge_argument_count != 0u)
    return false;
  if (logical_edge && branch->logical_operator == W_SEED_HIR0_LOGICAL_NONE)
    return false;
  if (!logical_edge && branch->logical_operator != W_SEED_HIR0_LOGICAL_NONE)
    return false;
  const w_seed_hir0_terminator *jump = &program->terminators[jump_block];
  if (jump->owner_block != jump_block ||
      jump->kind != W_SEED_HIR0_TERMINATOR_JUMP ||
      jump->target_block != join_block ||
      jump->else_block != W_SEED_HIR0_NONE ||
      jump->value_index != W_SEED_HIR0_NONE || jump->result_type != 0u ||
      jump->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      !span_valid(jump->source_span, source_length) ||
      (logical_edge && !span_equal(jump->source_span, branch->source_span)))
    return false;
  if (!logical_edge)
    return jump->first_edge_argument == W_SEED_HIR0_NONE &&
           jump->edge_argument_count == 0u;
  if (jump->edge_argument_count != 1u ||
      jump->first_edge_argument == W_SEED_HIR0_NONE ||
      jump->first_edge_argument >= program->edge_argument_count)
    return false;
  const w_seed_hir0_edge_argument *edge =
      &program->edge_arguments[jump->first_edge_argument];
  if (edge->owner_terminator != (size_t)(jump - program->terminators) ||
      edge->ordinal != 0u || edge->value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *incoming =
      &program->values[edge->value_index];
  if (incoming->owner_kind != W_SEED_HIR0_VALUE_OWNER_TERMINATOR ||
      incoming->owner_index != jump_block || incoming->owner_ordinal != 0u ||
      incoming->type_index != 3u ||
      edge->type_index != incoming->type_index ||
      !span_valid(incoming->source_span, source_length))
    return false;
  if (skip && (incoming->kind != W_SEED_HIR0_VALUE_CONST_BOOL ||
               incoming->bool_value != skip_value ||
               !span_equal(incoming->source_span, branch->source_span)))
    return false;
  return !skip || (incoming->block_argument_index == W_SEED_HIR0_NONE &&
                   incoming->unary_operator == W_SEED_HIR0_UNARY_NOT);
}

static bool verify_logical_join_shape(const w_seed_hir0_program *program,
                                      const w_seed_hir0_terminator *branch,
                                      size_t join_block,
                                      size_t source_length) {
  if (program == NULL || branch == NULL || join_block >= program->block_count)
    return false;
  const w_seed_hir0_block *join = &program->blocks[join_block];
  if (join->block_argument_count != 1u ||
      join->first_block_argument == W_SEED_HIR0_NONE ||
      (size_t)join->first_block_argument >= program->block_argument_count)
    return false;
  const w_seed_hir0_block_argument *argument =
      &program->block_arguments[join->first_block_argument];
  return argument->owner_block == join_block && argument->ordinal == 0u &&
         argument->type_index == 3u &&
         span_valid(argument->source_span, source_length) &&
         span_equal(argument->source_span, branch->source_span);
}

static bool verify_scalar_jump_shape(
    const w_seed_hir0_program *program, const w_seed_hir0_terminator *branch,
    size_t jump_block, size_t join_block, size_t source_length) {
  if (program == NULL || branch == NULL || jump_block >= program->block_count ||
      join_block >= program->block_count || branch->logical_operator !=
                                                 W_SEED_HIR0_LOGICAL_NONE ||
      (branch->result_type != 0u && branch->result_type != 2u &&
       branch->result_type != 3u) ||
      program->blocks[join_block].block_argument_count != 1u)
    return false;
  const w_seed_hir0_terminator *jump = &program->terminators[jump_block];
  if (jump->owner_block != jump_block ||
      jump->kind != W_SEED_HIR0_TERMINATOR_JUMP ||
      jump->target_block != join_block ||
      jump->else_block != W_SEED_HIR0_NONE ||
      jump->value_index != W_SEED_HIR0_NONE || jump->result_type != 0u ||
      jump->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      !span_valid(jump->source_span, source_length) ||
      !span_equal(jump->source_span, branch->source_span) ||
      jump->edge_argument_count != 1u ||
      jump->first_edge_argument == W_SEED_HIR0_NONE ||
      jump->first_edge_argument >= program->edge_argument_count)
    return false;
  const w_seed_hir0_edge_argument *edge =
      &program->edge_arguments[jump->first_edge_argument];
  if (edge->owner_terminator != (size_t)(jump - program->terminators) ||
      edge->ordinal != 0u || edge->value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *incoming = &program->values[edge->value_index];
  const uint32_t expected_type =
      branch->result_type == 0u
          ? program->block_arguments[
                program->blocks[join_block].first_block_argument]
                .type_index
          : branch->result_type;
  return incoming->owner_kind == W_SEED_HIR0_VALUE_OWNER_TERMINATOR &&
         incoming->owner_index == jump_block &&
         incoming->owner_ordinal == 0u &&
         incoming->type_index == expected_type &&
         edge->type_index == incoming->type_index &&
         span_valid(incoming->source_span, source_length) &&
         incoming->source_span.start_byte >= branch->source_span.start_byte &&
         incoming->source_span.end_byte <= branch->source_span.end_byte;
}

/* A statement-level mutation diamond may carry more than one independently
 * typed value into its join.  Keep the scalar expression proof above narrow,
 * and verify the edge list against the destination block arguments here. */
static bool verify_unit_jump_shape(
    const w_seed_hir0_program *program, const w_seed_hir0_terminator *branch,
    size_t jump_block, size_t join_block, size_t source_length) {
  if (program == NULL || branch == NULL || jump_block >= program->block_count ||
      join_block >= program->block_count || branch->logical_operator !=
                                                 W_SEED_HIR0_LOGICAL_NONE ||
      branch->result_type != 0u)
    return false;
  const w_seed_hir0_block *join = &program->blocks[join_block];
  if (join->block_argument_count == 0u ||
      join->first_block_argument == W_SEED_HIR0_NONE)
    return false;
  const w_seed_hir0_terminator *jump = &program->terminators[jump_block];
  if (jump->owner_block != jump_block ||
      jump->kind != W_SEED_HIR0_TERMINATOR_JUMP ||
      jump->target_block != join_block ||
      jump->else_block != W_SEED_HIR0_NONE ||
      jump->value_index != W_SEED_HIR0_NONE || jump->result_type != 0u ||
      jump->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      !span_valid(jump->source_span, source_length) ||
      !span_equal(jump->source_span, branch->source_span) ||
      jump->edge_argument_count != join->block_argument_count ||
      jump->first_edge_argument == W_SEED_HIR0_NONE ||
      !range_valid(jump->first_edge_argument, jump->edge_argument_count,
                   program->edge_argument_count))
    return false;
  for (size_t ordinal = 0u; ordinal < jump->edge_argument_count;
       ordinal += 1u) {
    const w_seed_hir0_edge_argument *edge =
        &program->edge_arguments[(size_t)jump->first_edge_argument + ordinal];
    const w_seed_hir0_block_argument *argument =
        &program->block_arguments[(size_t)join->first_block_argument + ordinal];
    if (edge->type_index != argument->type_index ||
        edge->value_index >= program->value_count)
      return false;
    const w_seed_hir0_value *incoming = &program->values[edge->value_index];
    if (incoming->owner_kind != W_SEED_HIR0_VALUE_OWNER_TERMINATOR ||
        incoming->owner_index != jump_block ||
        incoming->owner_ordinal != ordinal ||
        incoming->type_index != edge->type_index ||
        !span_valid(incoming->source_span, source_length) ||
        incoming->source_span.start_byte < branch->source_span.start_byte ||
        incoming->source_span.end_byte > branch->source_span.end_byte)
      return false;
  }
  return true;
}

/* A statement-level mutation diamond carries its merged values through the
 * destination block arguments, then materializes a mutable binding version in
 * that block.  Expression diamonds also have a destination argument, but
 * their join binding (when present) is an immutable expression result.  Keep
 * this distinction independent from BRANCH.result_type so a forged scalar
 * tag cannot turn a statement merge into an expression diamond. */
static bool block_has_mutable_merge_binding(
    const w_seed_hir0_program *program, size_t block_index,
    w_seed_span branch_span) {
  if (program == NULL || block_index >= program->block_count) return false;
  const w_seed_hir0_block *block = &program->blocks[block_index];
  if (block->first_instruction == W_SEED_HIR0_NONE ||
      block->instruction_count == 0u ||
      !range_valid(block->first_instruction, block->instruction_count,
                   program->instruction_count))
    return false;
  for (size_t ordinal = 0u; ordinal < block->instruction_count;
       ordinal += 1u) {
    const w_seed_hir0_instruction *instruction =
        &program->instructions[(size_t)block->first_instruction + ordinal];
    if (instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
        instruction->binding_index == W_SEED_HIR0_NONE ||
        instruction->binding_index >= program->binding_count)
      continue;
    const w_seed_hir0_binding *binding =
        &program->bindings[instruction->binding_index];
    if (binding->is_mutable &&
        binding->source_binding != instruction->binding_index &&
        span_equal(binding->source_span, branch_span))
      return true;
  }
  return false;
}

static bool verify_unit_join_shape(const w_seed_hir0_program *program,
                                   const w_seed_hir0_terminator *branch,
                                   size_t join_block, size_t source_length) {
  if (program == NULL || branch == NULL || join_block >= program->block_count)
    return false;
  const w_seed_hir0_block *join = &program->blocks[join_block];
  if (join->block_argument_count == 0u ||
      join->first_block_argument == W_SEED_HIR0_NONE)
    return false;
  const size_t function = join->owner_function;
  if (function >= program->function_count ||
      program->functions[function].module_index >= program->module_count)
    return false;
  if (!block_has_mutable_merge_binding(program, join_block,
                                       branch->source_span))
    return false;
  for (size_t ordinal = 0u; ordinal < join->block_argument_count; ordinal += 1u) {
    const w_seed_hir0_block_argument *argument =
        &program->block_arguments[(size_t)join->first_block_argument + ordinal];
    if (argument->owner_block != join_block || argument->ordinal != ordinal ||
        (argument->type_index != 2u && argument->type_index != 3u) ||
        !span_valid(argument->source_span, source_length) ||
        !span_equal(argument->source_span, branch->source_span))
      return false;
  }
  return true;
}

static bool verify_join_shape(const w_seed_hir0_program *program,
                              const w_seed_hir0_terminator *branch,
                              size_t join_block, uint32_t expected_type,
                              size_t source_length) {
  if (program == NULL || branch == NULL || join_block >= program->block_count ||
      (expected_type != 2u && expected_type != 3u))
    return false;
  const w_seed_hir0_block *join = &program->blocks[join_block];
  if (join->block_argument_count != 1u ||
      join->first_block_argument == W_SEED_HIR0_NONE ||
      (size_t)join->first_block_argument >= program->block_argument_count)
    return false;
  const w_seed_hir0_block_argument *argument =
      &program->block_arguments[join->first_block_argument];
  return argument->owner_block == join_block && argument->ordinal == 0u &&
         argument->type_index == expected_type &&
         span_valid(argument->source_span, source_length) &&
         span_equal(argument->source_span, branch->source_span);
}

static uint32_t hir0_process_entry_function_index(
    const w_seed_hir0_program *program);

static bool verify_process_input0(const w_seed_hir0_program *program);

static bool verify_cfg_process_terminal_branch(
    const w_seed_hir0_program *program, size_t function_index);

static bool verify_cfg_branch(const w_seed_hir0_program *program,
                              uint32_t function_index, size_t branch_block,
                              size_t end, size_t depth, size_t *join_block) {
  if (program == NULL || join_block == NULL || depth >= W_SEED_HIR0_MAX_NESTING ||
      branch_block >= end || branch_block + 1u >= end)
    return false;
  const w_seed_hir0_terminator *branch =
      &program->terminators[branch_block];
  if (branch->kind != W_SEED_HIR0_TERMINATOR_BRANCH ||
      branch->target_block != branch_block + 1u ||
      branch->else_block <= branch->target_block ||
      branch->else_block >= end ||
      program->blocks[branch->target_block].owner_function != function_index ||
      program->blocks[branch->else_block].owner_function != function_index)
    return false;
  size_t then_last = 0u;
  size_t then_join = 0u;
  if (!verify_cfg_arm(program, function_index, branch->target_block, end,
                      depth + 1u, &then_last, &then_join) ||
      then_last + 1u != branch->else_block)
    return false;
  size_t else_last = 0u;
  size_t else_join = 0u;
  if (!verify_cfg_arm(program, function_index, branch->else_block, end,
                      depth + 1u, &else_last, &else_join) ||
      else_join != then_join || then_join != else_last + 1u ||
      then_join >= end || then_join <= branch_block)
    return false;
  if (branch->logical_operator > W_SEED_HIR0_LOGICAL_OR ||
      branch->first_edge_argument != W_SEED_HIR0_NONE ||
      branch->edge_argument_count != 0u)
    return false;
  const size_t source_length =
      program->modules[program->functions[function_index].module_index]
          .source_length;
  if (branch->logical_operator == W_SEED_HIR0_LOGICAL_NONE) {
    const bool mutable_merge = block_has_mutable_merge_binding(
        program, then_join, branch->source_span);
    if (branch->result_type == 0u) {
      if (program->blocks[then_join].block_argument_count == 0u) {
        if (mutable_merge) return false;
        if (!verify_logical_jump_shape(
                program, branch, branch_block, then_last, then_join, false,
                false, false, source_length) ||
            !verify_logical_jump_shape(program, branch, branch_block, else_last,
                                       else_join, false, false, false,
                                       source_length))
          return false;
      } else if (!verify_unit_jump_shape(program, branch, then_last,
                                         then_join, source_length) ||
                 !verify_unit_jump_shape(program, branch, else_last,
                                         else_join, source_length) ||
                 !verify_unit_join_shape(program, branch, then_join,
                                         source_length)) {
        return false;
      }
    } else if (branch->result_type == 2u || branch->result_type == 3u) {
      if (mutable_merge) return false;
      if (!verify_scalar_jump_shape(program, branch, then_last, then_join,
                                    source_length) ||
          !verify_scalar_jump_shape(program, branch, else_last, else_join,
                                    source_length) ||
          !verify_join_shape(program, branch, then_join, branch->result_type,
                             source_length))
        return false;
    } else {
      return false;
    }
  } else {
    if (branch->result_type != 3u ||
        !verify_logical_join_shape(program, branch, then_join, source_length))
      return false;
    const bool and_operator =
        branch->logical_operator == W_SEED_HIR0_LOGICAL_AND;
    const size_t rhs_last = and_operator ? then_last : else_last;
    const size_t skip_last = and_operator ? else_last : then_last;
    if (!verify_logical_jump_shape(program, branch, branch_block, rhs_last,
                                   then_join, true, false, false,
                                   source_length) ||
        !verify_logical_jump_shape(program, branch, branch_block, skip_last,
                                   then_join, true, true, !and_operator,
                                   source_length))
      return false;
  }
  *join_block = then_join;
  return true;
}

static bool verify_cfg_arm(const w_seed_hir0_program *program,
                           uint32_t function_index, size_t start,
                           size_t end, size_t depth, size_t *last_block,
                           size_t *join_block) {
  if (program == NULL || last_block == NULL || join_block == NULL ||
      start >= end || depth > W_SEED_HIR0_MAX_NESTING)
    return false;
  size_t current = start;
  size_t guard = 0u;
  while (current < end && guard < end) {
    const w_seed_hir0_terminator *term = &program->terminators[current];
    if (term->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
      size_t nested_join = 0u;
      if (!verify_cfg_branch(program, function_index, current, end, depth,
                             &nested_join) ||
          nested_join <= current || nested_join >= end)
        return false;
      current = nested_join;
      guard += 1u;
      continue;
    }
    if (term->kind != W_SEED_HIR0_TERMINATOR_JUMP ||
        term->target_block == W_SEED_HIR0_NONE ||
        term->target_block <= current || term->target_block >= end ||
        term->else_block != W_SEED_HIR0_NONE ||
        program->blocks[term->target_block].owner_function != function_index)
      return false;
    *last_block = current;
    *join_block = term->target_block;
    return true;
  }
  return false;
}

static bool value_tree_contains_any_block_argument(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t first_block_argument, size_t block_argument_count, size_t depth) {
  if (program == NULL || value_index >= program->value_count ||
      block_argument_count == 0u || depth > 256u)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
      value->block_argument_index >= first_block_argument &&
      (size_t)value->block_argument_index <
          (size_t)first_block_argument + block_argument_count)
    return true;
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
    if (value->binding_index == W_SEED_HIR0_NONE ||
        value->binding_index >= program->binding_count)
      return false;
    const w_seed_hir0_binding *binding =
        &program->bindings[value->binding_index];
    return binding->initializer_value < program->value_count &&
           value_tree_contains_any_block_argument(
               program, binding->initializer_value, first_block_argument,
               block_argument_count, depth + 1u);
  }
  return (value->left_value != W_SEED_HIR0_NONE &&
          value_tree_contains_any_block_argument(
              program, value->left_value, first_block_argument,
              block_argument_count, depth + 1u)) ||
         (value->right_value != W_SEED_HIR0_NONE &&
          value_tree_contains_any_block_argument(
              program, value->right_value, first_block_argument,
              block_argument_count, depth + 1u));
}

/* The exit-side continuation is intentionally narrower than the ordinary HIR
 * value language.  This independent proof keeps calls, effects, control
 * values, and unrelated bindings out of the one bounded post-loop operation;
 * only scalar i64 arithmetic over loop results, parameters, and constants is
 * admitted. */
static bool hir0_natural_loop_continuation_value_ok(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t function_index, uint32_t header_index,
    const uint32_t *result_bindings, size_t carrier_count,
    bool *uses_result, bool *result_lanes, size_t depth) {
  if (program == NULL || result_bindings == NULL || uses_result == NULL ||
      result_lanes == NULL || carrier_count == 0u ||
      carrier_count > HIR0_MAX_BRANCH_ASSIGNMENTS ||
      depth > W_SEED_HIR0_MAX_NESTING || value_index >= program->value_count ||
      header_index >= program->block_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index != W_SEED_HIR0_TYPE_I64 ||
      value->first_interpolation_segment != W_SEED_HIR0_NONE ||
      value->interpolation_segment_count != 0u ||
      value->first_enum_payload != 0u || value->enum_payload_count != 0u ||
      value->pattern_capture_index != W_SEED_HIR0_NONE ||
      value->unsigned_integer_value != 0u || value->byte_offset != 0u ||
      value->byte_count != 0u ||
      value->external_module_index != W_SEED_HIR0_NONE ||
      value->external_symbol_index != W_SEED_HIR0_NONE ||
      value->member_name.offset != 0u || value->member_name.count != 0u ||
      value->enum_index != W_SEED_HIR0_NONE ||
      value->enum_case_index != W_SEED_HIR0_NONE)
    return false;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_I64) {
    return value->binding_index == W_SEED_HIR0_NONE &&
           value->parameter_index == W_SEED_HIR0_NONE &&
           value->call_index == W_SEED_HIR0_NONE &&
           value->left_value == W_SEED_HIR0_NONE &&
           value->right_value == W_SEED_HIR0_NONE &&
           value->block_argument_index == W_SEED_HIR0_NONE &&
           value->binary_operator == W_SEED_HIR0_BINARY_ADD &&
           value->unary_operator == W_SEED_HIR0_UNARY_NOT &&
           !value->bool_value;
  }
  if (value->kind == W_SEED_HIR0_VALUE_PARAMETER_READ) {
    if (value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index >= program->parameter_count ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->left_value != W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
        value->integer_value != 0 || value->bool_value)
      return false;
    const w_seed_hir0_parameter *parameter =
        &program->parameters[value->parameter_index];
    return parameter->owner_function == function_index &&
           parameter->type_index == W_SEED_HIR0_TYPE_I64;
  }
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
    if (value->binding_index >= program->binding_count ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->left_value != W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
        value->integer_value != 0 || value->bool_value)
      return false;
    for (size_t lane = 0u; lane < carrier_count; lane += 1u)
      if (result_bindings[lane] == value->binding_index) {
        *uses_result = true;
        result_lanes[lane] = true;
        return true;
      }
    return false;
  }
  if (value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ) {
    if (value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->left_value != W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->block_argument_index == W_SEED_HIR0_NONE ||
        value->block_argument_index >= program->block_argument_count ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
        value->integer_value != 0 || value->bool_value)
      return false;
    const w_seed_hir0_block *header = &program->blocks[header_index];
    if (header->first_block_argument == W_SEED_HIR0_NONE ||
        !range_valid(header->first_block_argument, carrier_count,
                     program->block_argument_count) ||
        value->block_argument_index < header->first_block_argument ||
        (size_t)value->block_argument_index >=
            (size_t)header->first_block_argument + carrier_count)
      return false;
    const size_t lane = (size_t)value->block_argument_index -
                        header->first_block_argument;
    const w_seed_hir0_block_argument *argument =
        &program->block_arguments[value->block_argument_index];
    if (argument->owner_block != header_index || argument->ordinal != lane ||
        argument->type_index != W_SEED_HIR0_TYPE_I64)
      return false;
    *uses_result = true;
    result_lanes[lane] = true;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64)
    return value->unary_operator == W_SEED_HIR0_UNARY_NEGATE &&
           value->binary_operator == W_SEED_HIR0_BINARY_ADD &&
           value->binding_index == W_SEED_HIR0_NONE &&
           value->parameter_index == W_SEED_HIR0_NONE &&
           value->call_index == W_SEED_HIR0_NONE &&
           value->block_argument_index == W_SEED_HIR0_NONE &&
           value->integer_value == 0 && !value->bool_value &&
           value->left_value != W_SEED_HIR0_NONE &&
           value->right_value == W_SEED_HIR0_NONE &&
           hir0_natural_loop_continuation_value_ok(
               program, value->left_value, function_index, header_index,
               result_bindings, carrier_count, uses_result, result_lanes,
               depth + 1u);
  if (value->kind != W_SEED_HIR0_VALUE_BINARY_I64 ||
      value->binary_operator > W_SEED_HIR0_BINARY_REMAINDER ||
      value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
      value->binding_index != W_SEED_HIR0_NONE ||
      value->parameter_index != W_SEED_HIR0_NONE ||
      value->call_index != W_SEED_HIR0_NONE ||
      value->block_argument_index != W_SEED_HIR0_NONE ||
      value->integer_value != 0 || value->bool_value ||
      value->left_value == W_SEED_HIR0_NONE ||
      value->right_value == W_SEED_HIR0_NONE)
    return false;
  return hir0_natural_loop_continuation_value_ok(
             program, value->left_value, function_index, header_index,
             result_bindings, carrier_count, uses_result, result_lanes,
             depth + 1u) &&
         hir0_natural_loop_continuation_value_ok(
             program, value->right_value, function_index, header_index,
             result_bindings, carrier_count, uses_result, result_lanes,
             depth + 1u);
}

static bool value_tree_contains_binding_read(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t binding_index, size_t depth) {
  if (program == NULL || depth > W_SEED_HIR0_MAX_NESTING ||
      value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ)
    return value->binding_index == binding_index;
  return (value->left_value != W_SEED_HIR0_NONE &&
          value_tree_contains_binding_read(program, value->left_value,
                                           binding_index, depth + 1u)) ||
         (value->right_value != W_SEED_HIR0_NONE &&
          value_tree_contains_binding_read(program, value->right_value,
                                           binding_index, depth + 1u));
}

/* Recognize exactly the first bounded loop CFG.  This is deliberately
 * separate from the forward-only diamond verifier: arbitrary cycles and a
 * second backward edge remain invalid. */
static bool verify_cfg_natural_loop(const w_seed_hir0_program *program,
                                    size_t function_index) {
  if (program == NULL || function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->block_count != 4u || function->first_block + 3u >=
                                          program->block_count)
    return false;
  const size_t preheader_index = function->first_block;
  const size_t header_index = preheader_index + 1u;
  const size_t body_index = header_index + 1u;
  const size_t exit_index = body_index + 1u;
  const w_seed_hir0_block *header = &program->blocks[header_index];
  const w_seed_hir0_block *body = &program->blocks[body_index];
  const w_seed_hir0_block *exit = &program->blocks[exit_index];
  const size_t carrier_count = header->block_argument_count;
  if (carrier_count == 0u || carrier_count > UINT32_MAX ||
      carrier_count > HIR0_MAX_BRANCH_ASSIGNMENTS ||
      header->first_block_argument == W_SEED_HIR0_NONE ||
      !range_valid(header->first_block_argument, carrier_count,
                   program->block_argument_count))
    return false;
  if (header->block_argument_count == 0u ||
      header->first_block_argument == W_SEED_HIR0_NONE ||
      body->block_argument_count != 0u || exit->block_argument_count != 0u ||
       body->instruction_count != carrier_count ||
       body->first_instruction == W_SEED_HIR0_NONE ||
       !range_valid(body->first_instruction, body->instruction_count,
                    program->instruction_count) ||
       exit->instruction_count > 1u ||
       (exit->instruction_count != 0u &&
        (exit->first_instruction == W_SEED_HIR0_NONE ||
         !range_valid(exit->first_instruction, exit->instruction_count,
                      program->instruction_count))) ||
       !natural_loop_header_dominates(program, (uint32_t)header_index,
                                     (uint32_t)body_index) ||
      !natural_loop_header_dominates(program, (uint32_t)header_index,
                                     (uint32_t)exit_index))
    return false;
  const w_seed_hir0_terminator *preheader_term =
      &program->terminators[preheader_index];
  const w_seed_hir0_terminator *header_term =
      &program->terminators[header_index];
  const w_seed_hir0_terminator *body_term = &program->terminators[body_index];
  const w_seed_hir0_terminator *exit_term = &program->terminators[exit_index];
  if (preheader_term->edge_argument_count != carrier_count ||
      body_term->edge_argument_count != carrier_count ||
      preheader_term->first_edge_argument == W_SEED_HIR0_NONE ||
      body_term->first_edge_argument == W_SEED_HIR0_NONE ||
      header_term->value_index == W_SEED_HIR0_NONE ||
      header_term->value_index >= program->value_count ||
      !value_tree_contains_any_block_argument(
          program, header_term->value_index, header->first_block_argument,
          carrier_count, 0u) ||
       (exit_term->kind != W_SEED_HIR0_TERMINATOR_RETURN_UNIT &&
        exit_term->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE))
    return false;
  uint32_t initial_bindings[HIR0_MAX_BRANCH_ASSIGNMENTS] = {0u};
  uint32_t result_bindings[HIR0_MAX_BRANCH_ASSIGNMENTS] = {0u};
  for (size_t ordinal = 0u; ordinal < carrier_count; ordinal += 1u) {
    const w_seed_hir0_block_argument *argument =
        &program->block_arguments[(size_t)header->first_block_argument +
                                  ordinal];
    if (argument->type_index != W_SEED_HIR0_TYPE_I64 ||
        argument->ordinal != ordinal || argument->owner_block != header_index ||
        !span_equal(argument->source_span, header_term->source_span))
      return false;
    const w_seed_hir0_edge_argument *initial_edge =
        &program->edge_arguments[(size_t)preheader_term->first_edge_argument +
                                 ordinal];
    if (initial_edge->type_index != W_SEED_HIR0_TYPE_I64 ||
        initial_edge->value_index >= program->value_count)
      return false;
    const w_seed_hir0_value *initial =
        &program->values[initial_edge->value_index];
    if (initial->kind != W_SEED_HIR0_VALUE_BINDING_READ ||
        initial->binding_index == W_SEED_HIR0_NONE ||
        initial->binding_index >= program->binding_count)
      return false;
    const uint32_t source_index = initial->binding_index;
    if (ordinal != 0u) {
      const w_seed_hir0_edge_argument *previous_edge =
          &program->edge_arguments[
              (size_t)preheader_term->first_edge_argument + ordinal - 1u];
      if (previous_edge->value_index >= program->value_count ||
          program->values[previous_edge->value_index].kind !=
              W_SEED_HIR0_VALUE_BINDING_READ ||
          program->values[previous_edge->value_index].binding_index >=
              source_index)
        return false;
    }
    const w_seed_hir0_binding *source = &program->bindings[source_index];
    if (source->owner_block != preheader_index || !source->is_mutable ||
        source->type_index != W_SEED_HIR0_TYPE_I64 ||
        source->source_binding != source_index ||
        source->previous_version != W_SEED_HIR0_NONE ||
        source->owner_instruction == W_SEED_HIR0_NONE ||
        source->owner_instruction >= program->instruction_count)
      return false;
    const w_seed_hir0_instruction *source_instruction =
        &program->instructions[source->owner_instruction];
    if (source_instruction->owner_block != preheader_index ||
        source_instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
        source_instruction->binding_index != source_index)
      return false;
    size_t matching_updates = 0u;
    uint32_t update_index = W_SEED_HIR0_NONE;
    for (size_t instruction_ordinal = 0u;
         instruction_ordinal < body->instruction_count; instruction_ordinal += 1u) {
      const w_seed_hir0_instruction *body_instruction =
          &program->instructions[(size_t)body->first_instruction +
                                 instruction_ordinal];
      if (body_instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
          body_instruction->binding_index == W_SEED_HIR0_NONE ||
          body_instruction->binding_index >= program->binding_count)
        return false;
      const w_seed_hir0_binding *candidate =
          &program->bindings[body_instruction->binding_index];
      if (candidate->source_binding == source_index) {
        matching_updates += 1u;
        update_index = body_instruction->binding_index;
      }
    }
    if (matching_updates != 1u || update_index == W_SEED_HIR0_NONE)
      return false;
    const w_seed_hir0_binding *update = &program->bindings[update_index];
    if (!update->is_mutable || update->type_index != W_SEED_HIR0_TYPE_I64 ||
        update->previous_version != source_index ||
        source->next_version != update_index ||
        update->initializer_value >= program->value_count ||
        !value_tree_contains_any_block_argument(
            program, update->initializer_value,
            header->first_block_argument, carrier_count, 0u))
      return false;
    const w_seed_hir0_edge_argument *back_edge =
        &program->edge_arguments[(size_t)body_term->first_edge_argument +
                                 ordinal];
    if (back_edge->type_index != W_SEED_HIR0_TYPE_I64 ||
        back_edge->value_index >= program->value_count)
      return false;
    const w_seed_hir0_value *next = &program->values[back_edge->value_index];
    if (next->kind != W_SEED_HIR0_VALUE_BINDING_READ ||
        next->binding_index != update_index)
      return false;
    initial_bindings[ordinal] = source_index;
    result_bindings[ordinal] = update_index;
  }
  uint32_t continuation_binding = W_SEED_HIR0_NONE;
  size_t continuation_lane = carrier_count;
  if (exit->instruction_count == 1u) {
    const w_seed_hir0_instruction *instruction =
        &program->instructions[exit->first_instruction];
    if (exit_term->ordinal != 1u ||
        exit_term->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
        exit_term->result_type != W_SEED_HIR0_TYPE_I64 ||
        instruction->owner_block != exit_index || instruction->ordinal != 0u ||
        instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
        instruction->result_type != 0u ||
        instruction->binding_index >= program->binding_count)
      return false;
    const w_seed_hir0_binding *continuation =
        &program->bindings[instruction->binding_index];
    for (size_t lane = 0u; lane < carrier_count; lane += 1u)
      if (continuation->source_binding == initial_bindings[lane]) {
        if (continuation_lane != carrier_count) return false;
        continuation_lane = lane;
      }
    if (continuation_lane == carrier_count ||
        continuation->owner_instruction != exit->first_instruction ||
        continuation->owner_block != exit_index || !continuation->is_mutable ||
        continuation->source_binding != initial_bindings[continuation_lane] ||
        continuation->previous_version != result_bindings[continuation_lane] ||
        continuation->next_version != W_SEED_HIR0_NONE ||
        continuation->type_index != W_SEED_HIR0_TYPE_I64 ||
        continuation->initializer_value >= program->value_count)
      return false;
    bool uses_result = false;
    bool result_lanes[HIR0_MAX_BRANCH_ASSIGNMENTS] = {false};
    if (!hir0_natural_loop_continuation_value_ok(
            program, continuation->initializer_value,
            (uint32_t)function_index, (uint32_t)header_index,
            result_bindings, carrier_count, &uses_result, result_lanes, 0u) ||
        !uses_result ||
        !value_tree_contains_binding_read(
            program, exit_term->value_index, instruction->binding_index, 0u))
      return false;
    continuation_binding = instruction->binding_index;
  }
  for (size_t lane = 0u; lane < carrier_count; lane += 1u)
    if (program->bindings[result_bindings[lane]].next_version !=
        (lane == continuation_lane ? continuation_binding
                                    : W_SEED_HIR0_NONE))
      return false;
  if (exit_term->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
      (exit_term->value_index == W_SEED_HIR0_NONE ||
       exit_term->value_index >= program->value_count))
    return false;
  return true;
}

/* Recognize the bounded post-test loop independently from the natural-loop
 * verifier.  HIR branches intentionally carry no edge arguments, so repeat
 * uses a dedicated latch: preheader -> carrier body -> condition -> latch |
 * exit, with the latch carrying the updated tuple back to the body. */
static bool verify_cfg_post_test_loop(const w_seed_hir0_program *program,
                                      size_t function_index) {
  if (program == NULL || function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->block_count != 5u || function->first_block + 4u >=
                                      program->block_count)
    return false;
  const size_t preheader_index = function->first_block;
  const size_t carrier_index = preheader_index + 1u;
  const size_t condition_index = carrier_index + 1u;
  const size_t latch_index = carrier_index + 2u;
  const size_t exit_index = carrier_index + 3u;
  const w_seed_hir0_block *carrier = &program->blocks[carrier_index];
  const w_seed_hir0_block *condition = &program->blocks[condition_index];
  const w_seed_hir0_block *latch = &program->blocks[latch_index];
  const w_seed_hir0_block *exit = &program->blocks[exit_index];
  const size_t carrier_count = carrier->block_argument_count;
  if (carrier_count == 0u || carrier_count > UINT32_MAX ||
      carrier_count > HIR0_MAX_BRANCH_ASSIGNMENTS ||
      carrier->first_block_argument == W_SEED_HIR0_NONE ||
      !range_valid(carrier->first_block_argument, carrier_count,
                   program->block_argument_count) ||
      condition->block_argument_count != 0u ||
      latch->block_argument_count != 0u || exit->block_argument_count != 0u ||
      carrier->instruction_count != carrier_count ||
      carrier->first_instruction == W_SEED_HIR0_NONE ||
      !range_valid(carrier->first_instruction, carrier->instruction_count,
                   program->instruction_count) ||
      exit->instruction_count > 1u ||
      (exit->instruction_count != 0u &&
        (exit->first_instruction == W_SEED_HIR0_NONE ||
         !range_valid(exit->first_instruction, exit->instruction_count,
                      program->instruction_count))) ||
      !post_test_loop_carrier_dominates(program, (uint32_t)carrier_index,
                                        (uint32_t)condition_index) ||
      !post_test_loop_carrier_dominates(program, (uint32_t)carrier_index,
                                        (uint32_t)latch_index) ||
      !post_test_loop_carrier_dominates(program, (uint32_t)carrier_index,
                                        (uint32_t)exit_index))
    return false;
  const w_seed_hir0_terminator *preheader_term =
      &program->terminators[preheader_index];
  const w_seed_hir0_terminator *carrier_term =
      &program->terminators[carrier_index];
  const w_seed_hir0_terminator *condition_term =
      &program->terminators[condition_index];
  const w_seed_hir0_terminator *latch_term =
      &program->terminators[latch_index];
  const w_seed_hir0_terminator *exit_term =
      &program->terminators[exit_index];
  if (preheader_term->edge_argument_count != carrier_count ||
      latch_term->edge_argument_count != carrier_count ||
      preheader_term->first_edge_argument == W_SEED_HIR0_NONE ||
      latch_term->first_edge_argument == W_SEED_HIR0_NONE ||
      condition_term->value_index == W_SEED_HIR0_NONE ||
      condition_term->value_index >= program->value_count ||
      program->values[condition_term->value_index].type_index !=
          W_SEED_HIR0_TYPE_BOOL ||
      (exit_term->kind != W_SEED_HIR0_TERMINATOR_RETURN_UNIT &&
       exit_term->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE))
    return false;
  uint32_t initial_bindings[HIR0_MAX_BRANCH_ASSIGNMENTS] = {0u};
  uint32_t result_bindings[HIR0_MAX_BRANCH_ASSIGNMENTS] = {0u};
  bool condition_uses_result = false;
  for (size_t ordinal = 0u; ordinal < carrier_count; ordinal += 1u) {
    const w_seed_hir0_block_argument *argument =
        &program->block_arguments[(size_t)carrier->first_block_argument +
                                  ordinal];
    if (argument->type_index != W_SEED_HIR0_TYPE_I64 ||
        argument->ordinal != ordinal || argument->owner_block != carrier_index ||
        !span_equal(argument->source_span, carrier_term->source_span))
      return false;
    const w_seed_hir0_edge_argument *initial_edge =
        &program->edge_arguments[(size_t)preheader_term->first_edge_argument +
                                 ordinal];
    if (initial_edge->type_index != W_SEED_HIR0_TYPE_I64 ||
        initial_edge->value_index >= program->value_count)
      return false;
    const w_seed_hir0_value *initial =
        &program->values[initial_edge->value_index];
    if (initial->kind != W_SEED_HIR0_VALUE_BINDING_READ ||
        initial->binding_index == W_SEED_HIR0_NONE ||
        initial->binding_index >= program->binding_count)
      return false;
    const uint32_t source_index = initial->binding_index;
    if (ordinal != 0u) {
      const w_seed_hir0_edge_argument *previous_edge =
          &program->edge_arguments[
              (size_t)preheader_term->first_edge_argument + ordinal - 1u];
      if (previous_edge->value_index >= program->value_count ||
          program->values[previous_edge->value_index].kind !=
              W_SEED_HIR0_VALUE_BINDING_READ ||
          program->values[previous_edge->value_index].binding_index >=
              source_index)
        return false;
    }
    const w_seed_hir0_binding *source = &program->bindings[source_index];
    if (source->owner_block != preheader_index || !source->is_mutable ||
        source->type_index != W_SEED_HIR0_TYPE_I64 ||
        source->source_binding != source_index ||
        source->previous_version != W_SEED_HIR0_NONE ||
        source->owner_instruction == W_SEED_HIR0_NONE ||
        source->owner_instruction >= program->instruction_count)
      return false;
    const w_seed_hir0_instruction *source_instruction =
        &program->instructions[source->owner_instruction];
    if (source_instruction->owner_block != preheader_index ||
        source_instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
        source_instruction->binding_index != source_index)
      return false;
    size_t matching_updates = 0u;
    uint32_t update_index = W_SEED_HIR0_NONE;
    for (size_t instruction_ordinal = 0u;
         instruction_ordinal < carrier->instruction_count;
         instruction_ordinal += 1u) {
      const w_seed_hir0_instruction *body_instruction =
          &program->instructions[(size_t)carrier->first_instruction +
                                 instruction_ordinal];
      if (body_instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
          body_instruction->binding_index == W_SEED_HIR0_NONE ||
          body_instruction->binding_index >= program->binding_count)
        return false;
      const w_seed_hir0_binding *candidate =
          &program->bindings[body_instruction->binding_index];
      if (candidate->source_binding == source_index) {
        matching_updates += 1u;
        update_index = body_instruction->binding_index;
      }
    }
    if (matching_updates != 1u || update_index == W_SEED_HIR0_NONE)
      return false;
    const w_seed_hir0_binding *update = &program->bindings[update_index];
    if (!update->is_mutable || update->type_index != W_SEED_HIR0_TYPE_I64 ||
        update->previous_version != source_index ||
        source->next_version != update_index ||
        update->initializer_value >= program->value_count ||
        !value_tree_contains_any_block_argument(
            program, update->initializer_value, carrier->first_block_argument,
            carrier_count, 0u))
      return false;
    const w_seed_hir0_edge_argument *back_edge =
        &program->edge_arguments[(size_t)latch_term->first_edge_argument +
                                 ordinal];
    if (back_edge->type_index != W_SEED_HIR0_TYPE_I64 ||
        back_edge->value_index >= program->value_count)
      return false;
    const w_seed_hir0_value *next = &program->values[back_edge->value_index];
    if (next->kind != W_SEED_HIR0_VALUE_BINDING_READ ||
        next->binding_index != update_index)
      return false;
    if (value_tree_contains_binding_read(
            program, condition_term->value_index, update_index, 0u))
      condition_uses_result = true;
    initial_bindings[ordinal] = source_index;
    result_bindings[ordinal] = update_index;
  }
  if (!condition_uses_result) return false;
  uint32_t continuation_binding = W_SEED_HIR0_NONE;
  size_t continuation_lane = carrier_count;
  if (exit->instruction_count == 1u) {
    const w_seed_hir0_instruction *instruction =
        &program->instructions[exit->first_instruction];
    if (exit_term->ordinal != 1u ||
        exit_term->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
        exit_term->result_type != W_SEED_HIR0_TYPE_I64 ||
        instruction->owner_block != exit_index || instruction->ordinal != 0u ||
        instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
        instruction->result_type != 0u ||
        instruction->binding_index >= program->binding_count)
      return false;
    const w_seed_hir0_binding *continuation =
        &program->bindings[instruction->binding_index];
    for (size_t lane = 0u; lane < carrier_count; lane += 1u)
      if (continuation->source_binding == initial_bindings[lane]) {
        if (continuation_lane != carrier_count) return false;
        continuation_lane = lane;
      }
    if (continuation_lane == carrier_count ||
        continuation->owner_instruction != exit->first_instruction ||
        continuation->owner_block != exit_index || !continuation->is_mutable ||
        continuation->source_binding != initial_bindings[continuation_lane] ||
        continuation->previous_version != result_bindings[continuation_lane] ||
        continuation->next_version != W_SEED_HIR0_NONE ||
        continuation->type_index != W_SEED_HIR0_TYPE_I64 ||
        continuation->initializer_value >= program->value_count)
      return false;
    bool uses_result = false;
    bool result_lanes[HIR0_MAX_BRANCH_ASSIGNMENTS] = {false};
    if (!hir0_natural_loop_continuation_value_ok(
            program, continuation->initializer_value,
            (uint32_t)function_index, (uint32_t)carrier_index,
            result_bindings, carrier_count, &uses_result, result_lanes, 0u) ||
        !uses_result ||
        !value_tree_contains_binding_read(
            program, exit_term->value_index, instruction->binding_index, 0u))
      return false;
    continuation_binding = instruction->binding_index;
  }
  for (size_t lane = 0u; lane < carrier_count; lane += 1u)
    if (program->bindings[result_bindings[lane]].next_version !=
        (lane == continuation_lane ? continuation_binding
                                    : W_SEED_HIR0_NONE))
      return false;
  if (exit_term->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
      (exit_term->value_index == W_SEED_HIR0_NONE ||
       exit_term->value_index >= program->value_count))
    return false;
  return true;
}

/* The bounded enum-switch lowering is a single dispatch block followed by one
 * direct-return block for every lexical enum case.  There is no HIR default
 * edge: invalid carrier patterns are introduced only by the MLIR backend and
 * terminate there.  Keep this CFG proof separate from the forward-diamond
 * proof so a switch arm cannot be mistaken for a branch arm or a join. */
static bool verify_cfg_switch(const w_seed_hir0_program *program,
                              size_t function_index) {
  if (program == NULL || function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->first_block >= program->block_count ||
      function->block_count < 2u)
    return false;
  const size_t dispatch_index = function->first_block;
  const w_seed_hir0_terminator *dispatch =
      &program->terminators[dispatch_index];
  if (dispatch->kind != W_SEED_HIR0_TERMINATOR_SWITCH_ENUM ||
      dispatch->owner_block != dispatch_index ||
      dispatch->switch_edge_count + 1u != function->block_count ||
      dispatch->first_switch_edge == W_SEED_HIR0_NONE ||
      !range_valid(dispatch->first_switch_edge, dispatch->switch_edge_count,
                   program->switch_edge_count))
    return false;
  const size_t end = function->first_block + function->block_count;
  if (end > program->block_count) return false;
  for (size_t ordinal = 0u; ordinal < dispatch->switch_edge_count;
       ordinal += 1u) {
    const w_seed_hir0_switch_edge *edge =
        &program->switch_edges[(size_t)dispatch->first_switch_edge + ordinal];
    const size_t target = edge->target_block;
    if (target != dispatch_index + 1u + ordinal || target >= end ||
        program->blocks[target].owner_function != function_index ||
        program->blocks[target].block_argument_count != 0u)
      return false;
    const w_seed_hir0_terminator *return_term = &program->terminators[target];
    if (return_term->owner_block != target ||
        return_term->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
        return_term->target_block != W_SEED_HIR0_NONE ||
        return_term->else_block != W_SEED_HIR0_NONE ||
        return_term->first_edge_argument != W_SEED_HIR0_NONE ||
        return_term->edge_argument_count != 0u ||
        return_term->value_index == W_SEED_HIR0_NONE ||
        return_term->result_type != function->return_type)
      return false;
  }
  return true;
}

/* Resolve the native process descriptor through its declared target and
 * identity.  Function zero is not a process ABI fact: helper functions may
 * precede the entry target in the caller-owned ranges. */
static uint32_t hir0_process_entry_function_index(
    const w_seed_hir0_program *program) {
  if (program == NULL || program->external_module_count != 1u ||
      (program->external_symbol_count != 4u &&
       program->external_symbol_count != 7u) ||
      program->entry_count != 1u || program->function_count == 0u ||
      program->entries == NULL || program->functions == NULL)
    return W_SEED_HIR0_NONE;
  const w_seed_hir0_entry *entry = &program->entries[0];
  if (entry->adapter_kind != W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS ||
      entry->is_body || entry->target_function >= program->function_count)
    return W_SEED_HIR0_NONE;
  const uint32_t target = entry->target_function;
  const w_seed_hir0_function *function = &program->functions[target];
  if (entry->module_index != function->module_index ||
      entry->target_identity != function->identity_index)
    return W_SEED_HIR0_NONE;
  return target;
}

/* The public process adapter is a normal function body contract.  This
 * checks only the resolved ABI and entry descriptor; body reachability is
 * checked by verify_cfg_process_terminal_branch after all records are closed.
 * Parameter names are intentionally opaque so aliases and renamed ABI
 * parameters remain valid. */
static bool verify_process_input0(const w_seed_hir0_program *program) {
  if (program == NULL || program->external_module_count != 1u ||
      program->external_symbol_count != 7u || program->module_count != 1u ||
      program->entry_count != 1u || program->function_count == 0u ||
      !verify_external_records(program))
    return false;
  const uint32_t target = hir0_process_entry_function_index(program);
  if (target == W_SEED_HIR0_NONE ||
      target >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[target];
  const w_seed_hir0_entry *entry = &program->entries[0];
  const uint32_t arguments_type = hir0_external_type_index(program, 0u);
  const uint32_t context_type = hir0_external_type_index(program, 1u);
  const uint32_t exit_code_type = hir0_external_type_index(program, 2u);
  if (arguments_type == W_SEED_HIR0_NONE ||
      context_type == W_SEED_HIR0_NONE ||
      exit_code_type == W_SEED_HIR0_NONE ||
      !hir_type_index_valid(program, arguments_type) ||
      !hir_type_index_valid(program, context_type) ||
      !hir_type_index_valid(program, exit_code_type) ||
      function->module_index != 0u || !function->is_async ||
      function->is_const || function->is_throws || function->is_unsafe ||
      function->has_borrow_clause || function->is_anonymous_entry ||
      function->return_type != exit_code_type ||
      !range_valid(function->first_parameter, 2u, program->parameter_count) ||
      function->parameter_count != 2u ||
      entry->module_index != function->module_index || entry->is_body ||
      entry->adapter_kind != W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS ||
      !hir_text_valid(program, entry->target_name) ||
      !hir_text_equal(program, entry->target_name, function->name) ||
      !hir_text_is(program, entry->slot, HIR0_SLOT_NAME))
    return false;
  const size_t host_base = program->module_count + program->function_count +
                           program->entry_count;
  if (host_base >= program->identity_count ||
      !hir_text_is(program, program->identities[host_base].profile,
                   HIR0_PROCESS_PROFILE))
    return false;
  const uint32_t expected_types[] = {arguments_type, context_type};
  for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
    const w_seed_hir0_parameter *parameter =
        &program->parameters[(size_t)function->first_parameter + ordinal];
    if (parameter->owner_function != target ||
        parameter->ordinal != ordinal ||
        parameter->type_index != expected_types[ordinal] ||
        parameter->label_kind != W_SEED_HIR0_LABEL_REQUIRED ||
        !hir_text_valid(program, parameter->name) ||
        !hir_text_valid(program, parameter->label) ||
        !hir_text_equal(program, parameter->label, parameter->name))
      return false;
  }
  return true;
}

/* Initial public-process CFG admission is deliberately bounded to a single
 * forward terminal diamond.  This accepts ordinary pre-branch instructions
 * in the entry block while rejecting entry loops/cycles until a path-aware
 * native cost proof exists. */
static bool verify_cfg_process_terminal_branch(
    const w_seed_hir0_program *program, size_t function_index) {
  if (program == NULL || function_index >= program->function_count ||
      hir0_process_entry_function_index(program) != function_index ||
      !verify_process_input0(program))
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  const uint32_t exit_code_type = hir0_external_type_index(program, 2u);
  if (exit_code_type == W_SEED_HIR0_NONE || function->first_block >=
                                                program->block_count)
    return false;
  if (function->block_count == 1u) {
    const size_t block_index = function->first_block;
    const w_seed_hir0_block *block = &program->blocks[block_index];
    const w_seed_hir0_terminator *term = &program->terminators[block_index];
    return block->owner_function == function_index &&
           block->block_argument_count == 0u &&
           term->owner_block == block_index &&
           term->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
           term->value_index != W_SEED_HIR0_NONE &&
           term->result_type == exit_code_type &&
           term->target_block == W_SEED_HIR0_NONE &&
           term->else_block == W_SEED_HIR0_NONE &&
           term->first_edge_argument == W_SEED_HIR0_NONE &&
           term->edge_argument_count == 0u;
  }
  if (function->block_count != 3u || program->block_count < 3u ||
      function->first_block > program->block_count - 3u)
    return false;
  const size_t branch_index = function->first_block;
  const size_t then_index = branch_index + 1u;
  const size_t else_index = branch_index + 2u;
  const w_seed_hir0_block *branch_block = &program->blocks[branch_index];
  const w_seed_hir0_block *then_block = &program->blocks[then_index];
  const w_seed_hir0_block *else_block = &program->blocks[else_index];
  const w_seed_hir0_terminator *branch =
      &program->terminators[branch_index];
  const w_seed_hir0_terminator *then_term =
      &program->terminators[then_index];
  const w_seed_hir0_terminator *else_term =
      &program->terminators[else_index];
  if (branch_block->owner_function != function_index ||
      then_block->owner_function != function_index ||
      else_block->owner_function != function_index ||
      branch->owner_block != branch_index ||
      branch->kind != W_SEED_HIR0_TERMINATOR_BRANCH ||
      branch->target_block != then_index || branch->else_block != else_index ||
      branch->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      branch->result_type != 0u || branch->value_index == W_SEED_HIR0_NONE ||
      branch->first_edge_argument != W_SEED_HIR0_NONE ||
      branch->edge_argument_count != 0u ||
      then_block->block_argument_count != 0u ||
      else_block->block_argument_count != 0u ||
      then_term->owner_block != then_index ||
      else_term->owner_block != else_index ||
      then_term->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      else_term->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      then_term->value_index == W_SEED_HIR0_NONE ||
      else_term->value_index == W_SEED_HIR0_NONE ||
      then_term->result_type != exit_code_type ||
      else_term->result_type != exit_code_type ||
      then_term->target_block != W_SEED_HIR0_NONE ||
      then_term->else_block != W_SEED_HIR0_NONE ||
      else_term->target_block != W_SEED_HIR0_NONE ||
      else_term->else_block != W_SEED_HIR0_NONE ||
      then_term->first_edge_argument != W_SEED_HIR0_NONE ||
      then_term->edge_argument_count != 0u ||
      else_term->first_edge_argument != W_SEED_HIR0_NONE ||
      else_term->edge_argument_count != 0u)
    return false;
  return true;
}

static bool verify_cfg_function(const w_seed_hir0_program *program,
                                size_t function_index) {
  if (program == NULL || function_index >= program->function_count) return false;
  /* The public process witness is a terminal branch: each arm returns its
   * ExitCode directly and therefore has no synthetic join block. Its complete
   * record contract is checked separately above. */
  if (program->external_symbol_count == 7u &&
      hir0_process_entry_function_index(program) == function_index)
    return verify_cfg_process_terminal_branch(program, function_index);
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->first_block < program->block_count &&
      program->terminators[function->first_block].kind ==
          W_SEED_HIR0_TERMINATOR_SWITCH_ENUM)
    return verify_cfg_switch(program, function_index);
  if (function->block_count == 5u &&
      verify_cfg_post_test_loop(program, function_index))
    return true;
  if (function->block_count == 4u &&
      verify_cfg_natural_loop(program, function_index))
    return true;
  const size_t start = function->first_block;
  const size_t end = start + function->block_count;
  size_t current = start;
  size_t guard = 0u;
  while (current < end && guard < function->block_count) {
    const w_seed_hir0_terminator *term = &program->terminators[current];
    if (term->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
      size_t join = 0u;
      if (!verify_cfg_branch(program, (uint32_t)function_index, current, end,
                             0u, &join) ||
          join <= current || join >= end)
        return false;
      current = join;
      guard += 1u;
      continue;
    }
    if (term->kind != W_SEED_HIR0_TERMINATOR_RETURN_UNIT &&
        term->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE)
      return false;
    return current + 1u == end;
  }
  return false;
}

static bool verify_logical_join_membership(
    const w_seed_hir0_program *program, size_t function_index) {
  if (program == NULL || function_index >= program->function_count) return false;
  if (program->external_symbol_count == 7u &&
      hir0_process_entry_function_index(program) == function_index)
    return verify_cfg_process_terminal_branch(program, function_index);
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->first_block < program->block_count &&
      program->terminators[function->first_block].kind ==
          W_SEED_HIR0_TERMINATOR_SWITCH_ENUM)
    return verify_cfg_switch(program, function_index);
  if (function->block_count == 5u &&
      verify_cfg_post_test_loop(program, function_index))
    return true;
  if (function->block_count == 4u &&
      verify_cfg_natural_loop(program, function_index))
    return true;
  const size_t start = function->first_block;
  const size_t end = start + function->block_count;
  for (size_t block_index = start; block_index < end; block_index += 1u) {
    size_t matches = 0u;
    for (size_t branch_index = start; branch_index < end; branch_index += 1u) {
      const w_seed_hir0_terminator *branch =
          &program->terminators[branch_index];
      if (branch->kind != W_SEED_HIR0_TERMINATOR_BRANCH)
        continue;
      size_t join = 0u;
      if (!verify_cfg_branch(program, (uint32_t)function_index, branch_index,
                             end, 0u, &join))
        return false;
      if (join == block_index &&
          program->blocks[block_index].block_argument_count != 0u)
        matches += 1u;
    }
    if ((program->blocks[block_index].block_argument_count != 0u) !=
        (matches == 1u))
      return false;
  }
  return true;
}

/* Recheck the consumer-facing process adapter from caller-owned HIR only.
 * This binds the finite native-process profile but does not infer product
 * profile selection or a directEntry proof. */
static bool verify_process_handler_private(const w_seed_hir0_program *program) {
  if (program == NULL) return false;
  if (program->external_module_count == 0u) {
    for (size_t entry = 0u; entry < program->entry_count; entry += 1u)
      if (program->entries[entry].adapter_kind !=
          W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT)
        return false;
    return true;
  }
  if (program->external_module_count != 1u ||
      program->external_symbol_count != 4u ||
      program->function_count != 1u ||
      program->entry_count != 1u || program->parameter_count != 2u ||
      program->block_count != 1u || program->instruction_count != 0u ||
      program->binding_count != 0u || program->call_count != 0u ||
      program->argument_count != 0u || program->value_count != 1u ||
      program->interpolation_segment_count != 0u ||
      program->terminator_count != 1u)
    return false;
  const size_t host_base = program->module_count + program->function_count +
                           program->entry_count;
  if (host_base >= program->identity_count ||
      !hir_text_is(program, program->identities[host_base].profile,
                   HIR0_PROCESS_PROFILE))
    return false;
  const w_seed_hir0_function *function = &program->functions[0];
  const w_seed_hir0_entry *entry = &program->entries[0];
  if (!function->is_async || function->is_const || function->is_throws ||
      function->is_unsafe || function->has_borrow_clause ||
      function->is_anonymous_entry || function->return_type != 6u ||
      function->first_parameter != 0u || function->parameter_count != 2u ||
      entry->adapter_kind != W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS ||
      entry->is_body)
    return false;
  for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
    const w_seed_hir0_parameter *parameter = &program->parameters[ordinal];
    if (parameter->owner_function != 0u || parameter->ordinal != ordinal ||
        parameter->type_index != 4u + ordinal ||
        parameter->label_kind != W_SEED_HIR0_LABEL_REQUIRED ||
        !hir_text_is(program, parameter->name, ordinal == 0u ? "args" : "ctx") ||
        !hir_text_valid(program, parameter->label) ||
        !hir_text_equal(program, parameter->label, parameter->name))
      return false;
  }
  const w_seed_hir0_terminator *terminator = &program->terminators[0];
  const w_seed_hir0_value *value = &program->values[0];
  return terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
         terminator->value_index == 0u && terminator->result_type == 6u &&
         value->kind == W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE &&
         value->type_index == 6u && value->external_module_index == 0u &&
         value->external_symbol_index == 3u &&
         hir_text_is(program, value->member_name, HIR0_PROCESS_SUCCESS);
}

static bool verify_process_handler(const w_seed_hir0_program *program) {
  if (program == NULL) return false;
  if (program->external_symbol_count == 7u)
    return verify_process_input0(program);
  return verify_process_handler_private(program);
}

/* Recompute the typed lifecycle classification from the already closed
 * external identity records.  Published lifecycle fields are deliberately
 * not consulted here: the same relation is used by emission and by the
 * independent verifier. */
static bool hir0_expected_type_lifecycle(
    const w_seed_hir0_program *program, uint32_t type_index,
    w_seed_hir0_lifecycle_kind *lifecycle,
    w_seed_hir0_release_contract_kind *release_contract) {
  if (program == NULL || lifecycle == NULL || release_contract == NULL ||
      type_index >= program->type_count)
    return false;
  *lifecycle = W_SEED_HIR0_LIFECYCLE_UNKNOWN;
  *release_contract = W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN;
  const w_seed_hir0_type *type = &program->types[type_index];
  switch (type->kind) {
    case W_SEED_HIR0_TYPE_UNIT:
    case W_SEED_HIR0_TYPE_I64:
    case W_SEED_HIR0_TYPE_BOOL:
    case W_SEED_HIR0_TYPE_USIZE:
    case W_SEED_HIR0_TYPE_ENUM:
    case W_SEED_HIR0_TYPE_ENUM_SUBSET:
      *lifecycle = W_SEED_HIR0_LIFECYCLE_VALUE_COPY;
      *release_contract = W_SEED_HIR0_RELEASE_CONTRACT_NONE;
      return true;
    case W_SEED_HIR0_TYPE_STRING:
      /* String ownership is outside this bounded proof. */
      return true;
    case W_SEED_HIR0_TYPE_NOMINAL:
      if (!hir_type_index_valid(program, type_index) ||
          type->external_module_index != 0u ||
          type->external_symbol_index > 2u)
        return true;
      if (type->external_symbol_index == 0u ||
          type->external_symbol_index == 1u) {
        *lifecycle = W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER;
        *release_contract =
            W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE;
      } else if (type->external_symbol_index == 2u) {
        *lifecycle = W_SEED_HIR0_LIFECYCLE_VALUE_COPY;
        *release_contract = W_SEED_HIR0_RELEASE_CONTRACT_NONE;
      }
      return true;
    default:
      return true;
  }
}

/* Derive the one supported process cleanup obligation from the complete
 * handler shape.  This is an exact parameter range in the caller-owned HIR;
 * no cleanup list is synthesized from liveness or adapter spelling. */
static bool hir0_expected_entry_cleanup(
    const w_seed_hir0_program *program, size_t entry_index,
    w_seed_hir0_entry_cleanup_kind *cleanup_obligation,
    uint32_t *first_cleanup_owner_parameter,
    uint32_t *cleanup_owner_parameter_count) {
  if (cleanup_obligation == NULL || first_cleanup_owner_parameter == NULL ||
      cleanup_owner_parameter_count == NULL || program == NULL ||
      entry_index >= program->entry_count)
    return false;
  *cleanup_obligation = W_SEED_HIR0_ENTRY_CLEANUP_NONE;
  *first_cleanup_owner_parameter = W_SEED_HIR0_NONE;
  *cleanup_owner_parameter_count = 0u;
  if (program->external_module_count == 0u) return true;
  if (!verify_process_handler(program) || entry_index != 0u) return false;
  const uint32_t target = hir0_process_entry_function_index(program);
  if (target == W_SEED_HIR0_NONE || target >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[target];
  *cleanup_obligation = W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS;
  *first_cleanup_owner_parameter = function->first_parameter;
  *cleanup_owner_parameter_count = 2u;
  return true;
}

/* This is the complete, independently recomputed process-owner witness used
 * by direct-entry analysis.  The root is not drained or reclaimed here; the
 * obligation covers exactly one normal-return wrapper release per handler
 * owner. */
static bool hir0_process_entry_lifecycle_ready(
    const w_seed_hir0_program *program) {
  if (program == NULL || program->external_module_count != 1u ||
      program->type_count < 7u ||
      !verify_process_handler(program))
    return false;
  const uint32_t arguments_type = hir0_external_type_index(program, 0u);
  const uint32_t context_type = hir0_external_type_index(program, 1u);
  const uint32_t exit_code_type = hir0_external_type_index(program, 2u);
  if (arguments_type == W_SEED_HIR0_NONE ||
      context_type == W_SEED_HIR0_NONE ||
      exit_code_type == W_SEED_HIR0_NONE)
    return false;
  const uint32_t expected_types[] = {arguments_type, context_type,
                                     exit_code_type};
  for (size_t ordinal = 0u; ordinal < 3u; ordinal += 1u) {
    const uint32_t type = expected_types[ordinal];
    w_seed_hir0_lifecycle_kind lifecycle;
    w_seed_hir0_release_contract_kind release_contract;
    if (!hir0_expected_type_lifecycle(program, type, &lifecycle,
                                      &release_contract) ||
        ((ordinal < 2u) &&
         (lifecycle != W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER ||
          release_contract !=
              W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE)) ||
        (ordinal == 2u &&
         (lifecycle != W_SEED_HIR0_LIFECYCLE_VALUE_COPY ||
          release_contract != W_SEED_HIR0_RELEASE_CONTRACT_NONE)))
      return false;
  }
  const uint32_t target = hir0_process_entry_function_index(program);
  if (target == W_SEED_HIR0_NONE || target >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[target];
  w_seed_hir0_entry_cleanup_kind cleanup_obligation;
  uint32_t first_cleanup_owner_parameter;
  uint32_t cleanup_owner_parameter_count;
  if (!hir0_expected_entry_cleanup(
          program, 0u, &cleanup_obligation, &first_cleanup_owner_parameter,
          &cleanup_owner_parameter_count) ||
      cleanup_obligation != W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS ||
      first_cleanup_owner_parameter != function->first_parameter ||
      cleanup_owner_parameter_count != 2u)
    return false;
  for (size_t ordinal = 0u; ordinal < cleanup_owner_parameter_count;
       ordinal += 1u) {
    const size_t parameter_index =
        (size_t)first_cleanup_owner_parameter + ordinal;
    if (parameter_index >= program->parameter_count) return false;
    const w_seed_hir0_parameter *parameter =
        &program->parameters[parameter_index];
    w_seed_hir0_lifecycle_kind lifecycle;
    w_seed_hir0_release_contract_kind release_contract;
    if (parameter->owner_function != target || parameter->ordinal != ordinal ||
        !hir0_expected_type_lifecycle(program, parameter->type_index,
                                      &lifecycle, &release_contract) ||
        lifecycle != W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER ||
        release_contract !=
            W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE)
      return false;
  }
  return true;
}

/* Publish fields only after all records exist.  There is no fallible path
 * after the first output write; invalid internal relations remain UNKNOWN and
 * therefore cannot publish a direct-entry proof. */
static void hir0_publish_process_lifecycle_facts(
    const w_seed_hir0_program *program, w_seed_hir0_output *output) {
  if (program == NULL || output == NULL) return;
  for (size_t type_index = 0u; type_index < program->type_count;
       type_index += 1u) {
    w_seed_hir0_lifecycle_kind lifecycle = W_SEED_HIR0_LIFECYCLE_UNKNOWN;
    w_seed_hir0_release_contract_kind release_contract =
        W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN;
    (void)hir0_expected_type_lifecycle(program, (uint32_t)type_index,
                                       &lifecycle, &release_contract);
    output->types[type_index].lifecycle = lifecycle;
    output->types[type_index].release_contract = release_contract;
  }
  for (size_t entry_index = 0u; entry_index < program->entry_count;
       entry_index += 1u) {
    w_seed_hir0_entry_cleanup_kind cleanup_obligation =
        W_SEED_HIR0_ENTRY_CLEANUP_NONE;
    uint32_t first_cleanup_owner_parameter = W_SEED_HIR0_NONE;
    uint32_t cleanup_owner_parameter_count = 0u;
    (void)hir0_expected_entry_cleanup(
        program, entry_index, &cleanup_obligation,
        &first_cleanup_owner_parameter, &cleanup_owner_parameter_count);
    output->entries[entry_index].cleanup_obligation = cleanup_obligation;
    output->entries[entry_index].first_cleanup_owner_parameter =
        first_cleanup_owner_parameter;
    output->entries[entry_index].cleanup_owner_parameter_count =
        cleanup_owner_parameter_count;
  }
}

/* Validate published lifecycle fields without using them as proof inputs.
 * This runs after verify_records has established all ranges and identities. */
static bool verify_process_lifecycle_facts(
    const w_seed_hir0_program *program) {
  if (program == NULL) return false;
  for (size_t type_index = 0u; type_index < program->type_count;
       type_index += 1u) {
    w_seed_hir0_lifecycle_kind lifecycle;
    w_seed_hir0_release_contract_kind release_contract;
    if (program->types[type_index].lifecycle >
            W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER ||
        program->types[type_index].release_contract >
            W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE ||
        !hir0_expected_type_lifecycle(program, (uint32_t)type_index,
                                      &lifecycle, &release_contract) ||
        program->types[type_index].lifecycle != lifecycle ||
        program->types[type_index].release_contract != release_contract)
      return false;
  }
  for (size_t entry_index = 0u; entry_index < program->entry_count;
       entry_index += 1u) {
    w_seed_hir0_entry_cleanup_kind cleanup_obligation;
    uint32_t first_cleanup_owner_parameter;
    uint32_t cleanup_owner_parameter_count;
    if (!hir0_expected_entry_cleanup(
            program, entry_index, &cleanup_obligation,
            &first_cleanup_owner_parameter, &cleanup_owner_parameter_count) ||
        program->entries[entry_index].cleanup_obligation !=
            cleanup_obligation ||
        program->entries[entry_index].first_cleanup_owner_parameter !=
            first_cleanup_owner_parameter ||
        program->entries[entry_index].cleanup_owner_parameter_count !=
            cleanup_owner_parameter_count)
      return false;
  }
  if (program->external_module_count == 0u) return true;
  if (!hir0_process_entry_lifecycle_ready(program)) return false;
  const w_seed_hir0_entry *entry = &program->entries[0];
  if (entry->cleanup_obligation !=
          W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS ||
      entry->cleanup_owner_parameter_count != 2u ||
      entry->first_cleanup_owner_parameter !=
          program->functions[entry->target_function].first_parameter)
    return false;
  return true;
}

static bool hir0_function_bit_get(const uint8_t *bits, size_t function) {
  return bits != NULL &&
         (bits[function / 8u] & (uint8_t)(1u << (function % 8u))) != 0u;
}

static void hir0_function_bit_set(uint8_t *bits, size_t function, bool value) {
  if (bits == NULL) return;
  const uint8_t mask = (uint8_t)(1u << (function % 8u));
  if (value)
    bits[function / 8u] |= mask;
  else
    bits[function / 8u] &= (uint8_t)~mask;
}

static bool hir0_value_kind_is_closed(w_seed_hir0_value_kind kind) {
  switch (kind) {
    case W_SEED_HIR0_VALUE_CONST_STRING:
    case W_SEED_HIR0_VALUE_BINDING_READ:
    case W_SEED_HIR0_VALUE_PARAMETER_READ:
    case W_SEED_HIR0_VALUE_CONST_I64:
    case W_SEED_HIR0_VALUE_CONST_USIZE:
    case W_SEED_HIR0_VALUE_CONST_BOOL:
    case W_SEED_HIR0_VALUE_BINARY_I64:
    case W_SEED_HIR0_VALUE_INTERPOLATED_STRING:
    case W_SEED_HIR0_VALUE_CALL_RESULT:
    case W_SEED_HIR0_VALUE_UNARY_BOOL:
    case W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ:
    case W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE:
    case W_SEED_HIR0_VALUE_EXTERNAL_MEMBER:
    case W_SEED_HIR0_VALUE_UNARY_I64:
    case W_SEED_HIR0_VALUE_ENUM_CASE:
    case W_SEED_HIR0_VALUE_PATTERN_CAPTURE_READ:
    case W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON:
      return true;
    default:
      /* A future value kind needs an explicit suspension/effect review before
       * it can participate in a direct-entry proof. */
      return false;
  }
}

static bool hir0_instruction_kind_is_closed(w_seed_hir0_instruction_kind kind) {
  switch (kind) {
    case W_SEED_HIR0_INSTRUCTION_CALL:
    case W_SEED_HIR0_INSTRUCTION_BINDING:
    case W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD:
      return true;
    default:
      return false;
  }
}

static bool hir0_call_execution_kind_is_closed(
    w_seed_hir0_call_execution_kind kind) {
  switch (kind) {
    case W_SEED_HIR0_CALL_DIRECT:
    case W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED:
    case W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED:
      return true;
    default:
      return false;
  }
}

static bool hir0_terminator_kind_is_closed(w_seed_hir0_terminator_kind kind) {
  switch (kind) {
    case W_SEED_HIR0_TERMINATOR_RETURN_UNIT:
    case W_SEED_HIR0_TERMINATOR_RETURN_VALUE:
    case W_SEED_HIR0_TERMINATOR_BRANCH:
    case W_SEED_HIR0_TERMINATOR_JUMP:
    case W_SEED_HIR0_TERMINATOR_SWITCH_ENUM:
      return true;
    default:
      return false;
  }
}

/* String and unknown nominal values still have ownership/lifecycle obligations
 * that the bounded HIR16 witness cannot establish. An invalid or future type
 * is blocked conservatively too; verifier shape checks reject it before this
 * helper is used on input HIR. */
static bool hir0_type_is_blocked(const w_seed_hir0_program *program,
                                 uint32_t type_index) {
  if (program == NULL || type_index == W_SEED_HIR0_NONE ||
      (size_t)type_index >= program->type_count)
    return true;
  const w_seed_hir0_type *type = &program->types[type_index];
  switch (type->kind) {
    case W_SEED_HIR0_TYPE_UNIT:
    case W_SEED_HIR0_TYPE_I64:
    case W_SEED_HIR0_TYPE_BOOL:
    case W_SEED_HIR0_TYPE_USIZE:
    case W_SEED_HIR0_TYPE_ENUM:
    case W_SEED_HIR0_TYPE_ENUM_SUBSET:
      return false;
    case W_SEED_HIR0_TYPE_STRING:
    case W_SEED_HIR0_TYPE_NOMINAL:
    default:
      return true;
  }
}

/* Process owner types are admitted only for the one complete bounded handler
 * whose exact release obligation has been recomputed.  This does not relax
 * the generic String/nominal barrier or admit any unknown call/effect. */
static bool hir0_type_is_blocked_for_function(
    const w_seed_hir0_program *program, size_t function_index,
    uint32_t type_index) {
  if (!hir0_type_is_blocked(program, type_index)) return false;
  const uint32_t process_target =
      hir0_process_entry_function_index(program);
  if (program == NULL || function_index >= program->function_count ||
      !hir0_process_entry_lifecycle_ready(program) ||
      process_target == W_SEED_HIR0_NONE || function_index != process_target)
    return true;
  w_seed_hir0_lifecycle_kind lifecycle;
  w_seed_hir0_release_contract_kind release_contract;
  if (!hir0_expected_type_lifecycle(program, type_index, &lifecycle,
                                    &release_contract))
    return true;
  return lifecycle != W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
         lifecycle != W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER;
}

/* Return the declaration owning a call without trusting the duplicated block
 * relation.  Shape verification has already checked that the two relations
 * agree; the conservative NONE result is used by the emitter if an internal
 * preflight invariant is ever broken. */
static uint32_t hir0_call_owner_function(const w_seed_hir0_program *program,
                                         size_t call_index) {
  if (program == NULL || call_index >= program->call_count) return W_SEED_HIR0_NONE;
  const w_seed_hir0_call *call = &program->calls[call_index];
  if (call->owner_instruction >= program->instruction_count ||
      call->owner_block >= program->block_count)
    return W_SEED_HIR0_NONE;
  const w_seed_hir0_instruction *instruction =
      &program->instructions[call->owner_instruction];
  if (instruction->owner_block != call->owner_block) return W_SEED_HIR0_NONE;
  const w_seed_hir0_block *block = &program->blocks[call->owner_block];
  return block->owner_function < program->function_count
             ? block->owner_function
             : W_SEED_HIR0_NONE;
}

/* Values are emitted in preorder.  Their owner relation points either to a
 * declaration anchor or to an earlier parent value/segment.  Walk that finite
 * chain iteratively so direct-entry analysis never recurses on caller data. */
static uint32_t hir0_value_owner_function(const w_seed_hir0_program *program,
                                          uint32_t value_index) {
  if (program == NULL || value_index == W_SEED_HIR0_NONE ||
      (size_t)value_index >= program->value_count)
    return W_SEED_HIR0_NONE;
  uint32_t current = value_index;
  for (size_t depth = 0u; depth <= W_SEED_HIR0_MAX_NESTING; depth += 1u) {
    if ((size_t)current >= program->value_count) return W_SEED_HIR0_NONE;
    const w_seed_hir0_value *value = &program->values[current];
    if (value->owner_kind == W_SEED_HIR0_VALUE_OWNER_ARGUMENT) {
      if (value->owner_index >= program->argument_count) return W_SEED_HIR0_NONE;
      const w_seed_hir0_argument *argument =
          &program->arguments[value->owner_index];
      return hir0_call_owner_function(program,
                                      argument->owner_call);
    }
    if (value->owner_kind == W_SEED_HIR0_VALUE_OWNER_BINDING) {
      if (value->owner_index >= program->binding_count) return W_SEED_HIR0_NONE;
      const w_seed_hir0_binding *binding =
          &program->bindings[value->owner_index];
      if (binding->owner_block >= program->block_count) return W_SEED_HIR0_NONE;
      const w_seed_hir0_block *block = &program->blocks[binding->owner_block];
      return block->owner_function < program->function_count
                 ? block->owner_function
                 : W_SEED_HIR0_NONE;
    }
    if (value->owner_kind == W_SEED_HIR0_VALUE_OWNER_TERMINATOR) {
      if (value->owner_index >= program->terminator_count)
        return W_SEED_HIR0_NONE;
      const w_seed_hir0_terminator *terminator =
          &program->terminators[value->owner_index];
      if (terminator->owner_block >= program->block_count)
        return W_SEED_HIR0_NONE;
      const w_seed_hir0_block *block =
          &program->blocks[terminator->owner_block];
      return block->owner_function < program->function_count
                 ? block->owner_function
                 : W_SEED_HIR0_NONE;
    }
    if (value->owner_kind == W_SEED_HIR0_VALUE_OWNER_BINARY ||
        value->owner_kind == W_SEED_HIR0_VALUE_OWNER_UNARY ||
        value->owner_kind == W_SEED_HIR0_VALUE_OWNER_EXTERNAL_MEMBER ||
        value->owner_kind == W_SEED_HIR0_VALUE_OWNER_EXTERNAL_ENUM_CASE) {
      if (value->owner_index == W_SEED_HIR0_NONE ||
          (size_t)value->owner_index >= program->value_count)
        return W_SEED_HIR0_NONE;
      current = value->owner_index;
      continue;
    }
    if (value->owner_kind ==
        W_SEED_HIR0_VALUE_OWNER_INTERPOLATION_SEGMENT) {
      if (value->owner_index >= program->interpolation_segment_count)
        return W_SEED_HIR0_NONE;
      const w_seed_hir0_interpolation_segment *segment =
          &program->interpolation_segments[value->owner_index];
      if (segment->owner_value == W_SEED_HIR0_NONE ||
          (size_t)segment->owner_value >= program->value_count)
        return W_SEED_HIR0_NONE;
      current = segment->owner_value;
      continue;
    }
    if (value->owner_kind == W_SEED_HIR0_VALUE_OWNER_ENUM_PAYLOAD) {
      if (value->owner_index >= program->enum_payload_count)
        return W_SEED_HIR0_NONE;
      const w_seed_hir0_enum_payload *payload =
          &program->enum_payloads[value->owner_index];
      if (payload->owner_value == W_SEED_HIR0_NONE ||
          payload->owner_value >= program->value_count)
        return W_SEED_HIR0_NONE;
      current = payload->owner_value;
      continue;
    }
    return W_SEED_HIR0_NONE;
  }
  return W_SEED_HIR0_NONE;
}

/* A String binding is normally an ownership barrier.  The one closed
 * exception is an immutable binding whose initializer resolves to a
 * module-owned constant string.  Follow only immutable binding reads, with
 * the same finite nesting bound used by value-tree verification; malformed or
 * cyclic caller-owned chains remain blocked. */
static bool hir0_binding_is_module_const_string(
    const w_seed_hir0_program *program, uint32_t binding_index,
    uint32_t owner_function,
    size_t depth) {
  if (program == NULL || depth > W_SEED_HIR0_MAX_NESTING ||
      binding_index == W_SEED_HIR0_NONE ||
      owner_function >= program->function_count ||
      (size_t)binding_index >= program->binding_count)
    return false;
  const w_seed_hir0_binding *binding = &program->bindings[binding_index];
  if (binding->owner_block >= program->block_count ||
      program->blocks[binding->owner_block].owner_function != owner_function ||
      binding->type_index != W_SEED_HIR0_TYPE_STRING || binding->is_mutable ||
      binding->source_binding != binding_index ||
      binding->previous_version != W_SEED_HIR0_NONE ||
      binding->next_version != W_SEED_HIR0_NONE ||
      binding->initializer_value == W_SEED_HIR0_NONE ||
      (size_t)binding->initializer_value >= program->value_count)
    return false;
  const w_seed_hir0_value *initializer =
      &program->values[binding->initializer_value];
  if (initializer->owner_kind != W_SEED_HIR0_VALUE_OWNER_BINDING ||
      initializer->owner_index != binding_index ||
      initializer->owner_ordinal != 0u ||
      initializer->type_index != W_SEED_HIR0_TYPE_STRING)
    return false;
  if (initializer->kind == W_SEED_HIR0_VALUE_CONST_STRING)
    return byte_slice_valid(program, initializer->byte_offset,
                            initializer->byte_count);
  if (initializer->kind != W_SEED_HIR0_VALUE_BINDING_READ ||
      initializer->binding_index == W_SEED_HIR0_NONE ||
      initializer->binding_index == binding_index ||
      (size_t)initializer->binding_index >= program->binding_count)
    return false;
  const w_seed_hir0_binding *source =
      &program->bindings[initializer->binding_index];
  const w_seed_hir0_function *function = &program->functions[owner_function];
  if (source->owner_block >= program->block_count ||
      program->blocks[source->owner_block].owner_function != owner_function ||
      (source->owner_block == binding->owner_block
           ? source->owner_instruction >= binding->owner_instruction
           : source->owner_block != function->first_block))
    return false;
  return hir0_binding_is_module_const_string(
      program, initializer->binding_index, owner_function, depth + 1u);
}

/* The seed process profile defines `print(String): ()` as a synchronous host
 * effect. It can block the current thread, but it cannot suspend a W task.
 * This proof uses only verified HIR records. Other host calls remain MAY. */
static bool hir0_host_call_never_suspends(
    const w_seed_hir0_program *program,
    const w_seed_hir0_identity *identity) {
  if (program == NULL || identity == NULL ||
      identity->kind != W_SEED_HIR0_IDENTITY_HOST_PRELUDE ||
      identity->return_type != 0u || identity->is_const ||
      identity->parameter_count != 1u || identity->requirement_count != 1u ||
      !hir_text_is(program, identity->name, "print") ||
      !hir_text_is(program, identity->profile, HIR0_PROCESS_PROFILE) ||
      !range_valid(identity->first_parameter, identity->parameter_count,
                   program->host_parameter_count) ||
      !range_valid(identity->first_requirement, identity->requirement_count,
                   program->requirement_count))
    return false;
  const uint32_t identity_index =
      (uint32_t)(identity - program->identities);
  const w_seed_hir0_host_parameter *parameter =
      &program->host_parameters[identity->first_parameter];
  const w_seed_hir0_requirement *requirement =
      &program->requirements[identity->first_requirement];
  return parameter->owner_identity == identity_index &&
         parameter->ordinal == 0u && parameter->type_index == 1u &&
         parameter->label_kind == W_SEED_HIR0_LABEL_POSITIONAL_ONLY &&
         parameter->label.count == 0u &&
         hir_text_is(program, parameter->name, "message") &&
         requirement->owner_kind == W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY &&
         requirement->owner_index == identity_index &&
         requirement->ordinal == 0u &&
          hir_text_is(program, requirement->name, "Console");
}

/* Interpolation normally produces an owned String and remains blocked.  A
 * direct print argument is the closed lowering exception: text segments are
 * already module-owned byte slices, and value segments are scalar-only. */
static bool hir0_interpolated_string_is_direct_print_argument(
    const w_seed_hir0_program *program, uint32_t value_index) {
  if (program == NULL || value_index == W_SEED_HIR0_NONE ||
      (size_t)value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->kind != W_SEED_HIR0_VALUE_INTERPOLATED_STRING ||
      value->type_index != W_SEED_HIR0_TYPE_STRING ||
      value->owner_kind != W_SEED_HIR0_VALUE_OWNER_ARGUMENT ||
      value->owner_index == W_SEED_HIR0_NONE ||
      (size_t)value->owner_index >= program->argument_count ||
      value->first_interpolation_segment == W_SEED_HIR0_NONE ||
      value->interpolation_segment_count == 0u ||
      !range_valid(value->first_interpolation_segment,
                   value->interpolation_segment_count,
                   program->interpolation_segment_count))
    return false;
  const w_seed_hir0_argument *argument =
      &program->arguments[value->owner_index];
  if (argument->owner_call == W_SEED_HIR0_NONE ||
      (size_t)argument->owner_call >= program->call_count ||
      argument->value_index != value_index || argument->ordinal != 0u ||
      argument->type_index != W_SEED_HIR0_TYPE_STRING)
    return false;
  const w_seed_hir0_call *call = &program->calls[argument->owner_call];
  if (call->first_argument != value->owner_index ||
      call->argument_count != 1u || call->result_type != W_SEED_HIR0_TYPE_UNIT ||
      call->callee_identity == W_SEED_HIR0_NONE ||
      (size_t)call->callee_identity >= program->identity_count ||
      !hir0_host_call_never_suspends(
          program, &program->identities[call->callee_identity]) ||
      call->owner_instruction == W_SEED_HIR0_NONE ||
      (size_t)call->owner_instruction >= program->instruction_count ||
      call->owner_block == W_SEED_HIR0_NONE ||
      (size_t)call->owner_block >= program->block_count)
    return false;
  const w_seed_hir0_instruction *instruction =
      &program->instructions[call->owner_instruction];
  if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
      instruction->call_index != argument->owner_call ||
      instruction->owner_block != call->owner_block ||
      instruction->result_type != call->result_type)
    return false;
  for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
       ordinal += 1u) {
    const size_t segment_index =
        (size_t)value->first_interpolation_segment + ordinal;
    const w_seed_hir0_interpolation_segment *segment =
        &program->interpolation_segments[segment_index];
    if (segment->owner_value != value_index || segment->ordinal != ordinal)
      return false;
    if (segment->kind == W_SEED_HIR0_INTERPOLATION_TEXT) {
      if (segment->value_index != W_SEED_HIR0_NONE ||
          !byte_slice_valid(program, segment->byte_offset,
                            segment->byte_count))
        return false;
    } else if (segment->kind == W_SEED_HIR0_INTERPOLATION_VALUE) {
      if (segment->byte_offset != 0u || segment->byte_count != 0u ||
          segment->value_index == W_SEED_HIR0_NONE ||
          (size_t)segment->value_index >= program->value_count)
        return false;
      const w_seed_hir0_value *child = &program->values[segment->value_index];
      if (child->type_index >= program->type_count ||
          (program->types[child->type_index].kind != W_SEED_HIR0_TYPE_I64 &&
           program->types[child->type_index].kind != W_SEED_HIR0_TYPE_BOOL &&
           program->types[child->type_index].kind != W_SEED_HIR0_TYPE_USIZE))
        return false;
      if (child->owner_kind !=
              W_SEED_HIR0_VALUE_OWNER_INTERPOLATION_SEGMENT ||
          child->owner_index != segment_index || child->owner_ordinal != 0u)
        return false;
    } else {
      return false;
    }
  }
  return true;
}

/* Compute the greatest fixed point of bodies that are proven never to
 * suspend.  The bitset is scratch only; no published HIR field participates
 * in this calculation.  Base classification scans F functions, V values,
 * I instructions, T terminators, and C calls; value-owner walks are bounded
 * by W_SEED_HIR0_MAX_NESTING.  The local-ordinary dependency relation is then
 * propagated in at most F passes, for O(F*(F+E)) worst-case propagation where
 * E is the call count. */
static void hir0_compute_body_never(const w_seed_hir0_program *program,
                                    uint8_t *body_never) {
  (void)memset(body_never, 0xff, HIR0_DIRECT_FUNCTION_BITSET_BYTES);
  for (size_t function = 0u; function < program->function_count; function += 1u) {
    const w_seed_hir0_function *value = &program->functions[function];
    if (hir0_type_is_blocked_for_function(program, function,
                                         value->return_type))
      hir0_function_bit_set(body_never, function, false);
    for (size_t parameter = 0u; parameter < value->parameter_count;
         parameter += 1u) {
      const size_t parameter_index = (size_t)value->first_parameter + parameter;
      if (parameter_index >= program->parameter_count ||
          hir0_type_is_blocked_for_function(
              program, function, program->parameters[parameter_index].type_index))
        hir0_function_bit_set(body_never, function, false);
    }
  }

  /* An opaque value or result is a lifecycle/provider boundary.  It is not a
   * proof that suspension occurs; it is information insufficient for this
   * seed's direct-entry witness, so the body is conservatively MAY. */
  for (size_t value_index = 0u; value_index < program->value_count;
       value_index += 1u) {
    const w_seed_hir0_value *value = &program->values[value_index];
    if (!hir0_value_kind_is_closed(value->kind)) {
      (void)memset(body_never, 0, HIR0_DIRECT_FUNCTION_BITSET_BYTES);
      continue;
    }
    /* A constant string is a module-owned byte slice in this HIR. Reading it
     * does not allocate, release, or suspend. Other String values keep the
     * conservative lifecycle barrier. */
    if (value->kind == W_SEED_HIR0_VALUE_CONST_STRING) continue;
    /* Keep the scalar fast path: lifecycle ownership only matters once the
     * value crosses the existing blocked-type boundary. */
    if (!hir0_type_is_blocked(program, value->type_index)) continue;
    if (value->kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING &&
        hir0_interpolated_string_is_direct_print_argument(
            program, (uint32_t)value_index))
      continue;
    const uint32_t owner =
        hir0_value_owner_function(program, (uint32_t)value_index);
    if (owner == W_SEED_HIR0_NONE) {
      (void)memset(body_never, 0, HIR0_DIRECT_FUNCTION_BITSET_BYTES);
      continue;
    }
    if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        value->type_index == W_SEED_HIR0_TYPE_STRING &&
        hir0_binding_is_module_const_string(program, value->binding_index,
                                            owner, 0u))
      continue;
    if (!hir0_type_is_blocked_for_function(program, owner, value->type_index))
      continue;
    hir0_function_bit_set(body_never, owner, false);
  }
  for (size_t block = 0u; block < program->block_count; block += 1u) {
    const w_seed_hir0_block *value = &program->blocks[block];
    if (value->owner_function >= program->function_count) continue;
    for (size_t argument = 0u; argument < value->block_argument_count;
         argument += 1u) {
      const size_t index = (size_t)value->first_block_argument + argument;
      if (index < program->block_argument_count &&
          hir0_type_is_blocked_for_function(
              program, value->owner_function,
              program->block_arguments[index].type_index))
        hir0_function_bit_set(body_never, value->owner_function, false);
    }
  }
  for (size_t instruction = 0u; instruction < program->instruction_count;
       instruction += 1u) {
    const w_seed_hir0_instruction *value = &program->instructions[instruction];
    if (!hir0_instruction_kind_is_closed(value->kind)) {
      (void)memset(body_never, 0, HIR0_DIRECT_FUNCTION_BITSET_BYTES);
      continue;
    }
    if (value->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD &&
        value->owner_block < program->block_count) {
      const uint32_t owner = program->blocks[value->owner_block].owner_function;
      if (owner < program->function_count)
        hir0_function_bit_set(body_never, owner, false);
      continue;
    }
    if (hir0_type_is_blocked_for_function(
            program, value->owner_block < program->block_count
                         ? program->blocks[value->owner_block].owner_function
                         : W_SEED_HIR0_NONE,
            value->result_type) &&
        value->owner_block < program->block_count) {
      const uint32_t owner = program->blocks[value->owner_block].owner_function;
      if (owner < program->function_count)
        hir0_function_bit_set(body_never, owner, false);
    }
  }
  for (size_t terminator = 0u; terminator < program->terminator_count;
       terminator += 1u) {
    const w_seed_hir0_terminator *value = &program->terminators[terminator];
    if (!hir0_terminator_kind_is_closed(value->kind)) {
      (void)memset(body_never, 0, HIR0_DIRECT_FUNCTION_BITSET_BYTES);
      continue;
    }
    if (hir0_type_is_blocked_for_function(
            program, value->owner_block < program->block_count
                         ? program->blocks[value->owner_block].owner_function
                         : W_SEED_HIR0_NONE,
            value->result_type) &&
        value->owner_block < program->block_count) {
      const uint32_t owner = program->blocks[value->owner_block].owner_function;
      if (owner < program->function_count)
        hir0_function_bit_set(body_never, owner, false);
    }
  }

  /* Host identities use their closed HIR0 profile witness. Direct local calls
   * inherit the target proof. A structured async-elided call is admissible
   * for a local never-suspending target, including an explicit async target
   * reached through its direct-entry facet, and adds no suspension edge or
   * runtime carrier of its own. */
  for (size_t call_index = 0u; call_index < program->call_count; call_index += 1u) {
    const uint32_t owner = hir0_call_owner_function(program, call_index);
    if (owner == W_SEED_HIR0_NONE) continue;
    const w_seed_hir0_call *call = &program->calls[call_index];
    if (!hir0_call_execution_kind_is_closed(call->execution_kind) ||
        call->callee_identity >= program->identity_count) {
      hir0_function_bit_set(body_never, owner, false);
      continue;
    }
    const w_seed_hir0_identity *identity =
        &program->identities[call->callee_identity];
    if (identity->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE &&
        hir0_host_call_never_suspends(program, identity))
      continue;
    if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
        identity->target_index >= program->function_count) {
      hir0_function_bit_set(body_never, owner, false);
      continue;
    }
    const w_seed_hir0_function *target =
        &program->functions[identity->target_index];
    if (target->is_anonymous_entry ||
        (target->is_async &&
         call->execution_kind != W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED))
      hir0_function_bit_set(body_never, owner, false);
  }

  for (size_t pass = 0u; pass < program->function_count; pass += 1u) {
    bool changed = false;
    for (size_t call_index = 0u; call_index < program->call_count;
         call_index += 1u) {
      const uint32_t owner = hir0_call_owner_function(program, call_index);
      if (owner == W_SEED_HIR0_NONE || !hir0_function_bit_get(body_never, owner))
        continue;
      const w_seed_hir0_call *call = &program->calls[call_index];
      if (call->callee_identity >= program->identity_count) continue;
      const w_seed_hir0_identity *identity =
          &program->identities[call->callee_identity];
      if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
          identity->target_index >= program->function_count)
        continue;
      const size_t target = identity->target_index;
      const bool ordinary_or_structured_async =
          !program->functions[target].is_anonymous_entry &&
          (!program->functions[target].is_async ||
           call->execution_kind == W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED);
      if (ordinary_or_structured_async &&
          !hir0_function_bit_get(body_never, target)) {
        hir0_function_bit_set(body_never, owner, false);
        changed = true;
      }
    }
    if (!changed) break;
  }
}

static void hir0_publish_direct_entry_facts(
    const w_seed_hir0_program *program, w_seed_hir0_output *output) {
  uint8_t body_never[HIR0_DIRECT_FUNCTION_BITSET_BYTES];
  hir0_compute_body_never(program, body_never);
  const uint32_t process_target =
      hir0_process_entry_function_index(program);
  const bool process_entry_ready =
      process_target != W_SEED_HIR0_NONE &&
      hir0_process_entry_lifecycle_ready(program);
  for (size_t function = 0u; function < program->function_count; function += 1u) {
    const w_seed_hir0_function *value = &program->functions[function];
    const bool never = hir0_function_bit_get(body_never, function);
    output->functions[function].suspension =
        value->is_async || !never ? W_SEED_HIR0_SUSPENSION_MAY
                                  : W_SEED_HIR0_SUSPENSION_NEVER;
    output->functions[function].direct_entry =
        value->is_async && !value->is_const && !value->is_anonymous_entry &&
                never &&
                (program->external_module_count == 0u ||
                 (process_entry_ready && process_target == function))
            ? W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE
            : W_SEED_HIR0_DIRECT_ENTRY_ABSENT;
  }
}

static bool verify_direct_entry_facts(const w_seed_hir0_program *program) {
  if (program == NULL) return false;
  uint8_t body_never[HIR0_DIRECT_FUNCTION_BITSET_BYTES];
  hir0_compute_body_never(program, body_never);
  const uint32_t process_target =
      hir0_process_entry_function_index(program);
  const bool process_entry_ready =
      process_target != W_SEED_HIR0_NONE &&
      hir0_process_entry_lifecycle_ready(program);
  for (size_t function = 0u; function < program->function_count; function += 1u) {
    const w_seed_hir0_function *value = &program->functions[function];
    const bool never = hir0_function_bit_get(body_never, function);
    const w_seed_hir0_suspension_kind expected_suspension =
        value->is_async || !never ? W_SEED_HIR0_SUSPENSION_MAY
                                  : W_SEED_HIR0_SUSPENSION_NEVER;
    const w_seed_hir0_direct_entry_kind expected_direct_entry =
        value->is_async && !value->is_const && !value->is_anonymous_entry &&
                never &&
                (program->external_module_count == 0u ||
                 (process_entry_ready && process_target == function))
            ? W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE
            : W_SEED_HIR0_DIRECT_ENTRY_ABSENT;
    if (value->suspension != expected_suspension ||
        value->direct_entry != expected_direct_entry)
      return false;
  }
  return true;
}

/* Independent HIR proof for the virtual suspension slice. It accepts one
 * async scalar function, one block, one or more yield instructions, and no
 * nested call. The yields remain explicit in verified HIR but a closed
 * launch/join consumer may choose a legal serial schedule and erase them. */
static bool hir0_function_static_yields(
    const w_seed_hir0_program *program, uint32_t function_index) {
  if (program == NULL || function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function =
      &program->functions[function_index];
  if (!function->is_async || function->is_const || function->is_throws ||
      function->is_unsafe || function->has_borrow_clause ||
      function->is_anonymous_entry ||
      function->direct_entry != W_SEED_HIR0_DIRECT_ENTRY_ABSENT ||
      function->block_count != 1u ||
      function->return_type >= program->type_count ||
      (program->types[function->return_type].kind != W_SEED_HIR0_TYPE_I64 &&
       program->types[function->return_type].kind != W_SEED_HIR0_TYPE_BOOL) ||
      !range_valid(function->first_parameter, function->parameter_count,
                   program->parameter_count) ||
      !range_valid(function->first_block, function->block_count,
                   program->block_count))
    return false;
  for (size_t ordinal = 0u; ordinal < function->parameter_count;
       ordinal += 1u) {
    const uint32_t type_index =
        program->parameters[(size_t)function->first_parameter + ordinal]
            .type_index;
    if (type_index >= program->type_count ||
        (program->types[type_index].kind != W_SEED_HIR0_TYPE_I64 &&
         program->types[type_index].kind != W_SEED_HIR0_TYPE_BOOL))
      return false;
  }
  const w_seed_hir0_block *block = &program->blocks[function->first_block];
  if (block->owner_function != function_index ||
      !range_valid(block->first_instruction, block->instruction_count,
                   program->instruction_count) ||
      block->terminator_index >= program->terminator_count)
    return false;
  size_t yields = 0u;
  for (size_t ordinal = 0u; ordinal < block->instruction_count; ordinal += 1u) {
    const w_seed_hir0_instruction *instruction =
        &program->instructions[(size_t)block->first_instruction + ordinal];
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) {
      yields += 1u;
    } else if (instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
               instruction->binding_index >= program->binding_count ||
               program->bindings[instruction->binding_index].type_index >=
                   program->type_count ||
               (program->types[program->bindings[instruction->binding_index]
                                   .type_index]
                        .kind != W_SEED_HIR0_TYPE_I64 &&
                program->types[program->bindings[instruction->binding_index]
                                   .type_index]
                        .kind != W_SEED_HIR0_TYPE_BOOL)) {
      return false;
    }
  }
  const w_seed_hir0_terminator *terminator =
      &program->terminators[block->terminator_index];
  return yields != 0u &&
         (terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT ||
          terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE);
}

static bool verify_records(const w_seed_hir0_program *program) {
  const bool has_external_process =
      program != NULL && program->external_module_count != 0u;
  if (program->module_count == 0u || program->function_count == 0u ||
      program->entry_count != 1u ||
      program->type_count < (has_external_process ? 7u : 4u) +
                                program->enum_count +
                                (program->external_symbol_count == 7u ? 1u : 0u) ||
      program->block_count == 0u ||
      program->terminator_count != program->block_count ||
      program->argument_count > program->value_count ||
      program->enum_payload_count > program->value_count ||
      !hir_text_is(program, program->types[0].name, HIR0_UNIT_NAME) ||
      !hir_text_is(program, program->types[1].name, HIR0_STRING_NAME) ||
      !hir_text_is(program, program->types[2].name, HIR0_I64_NAME) ||
      !hir_text_is(program, program->types[3].name, HIR0_BOOL_NAME) ||
      program->types[0].kind != W_SEED_HIR0_TYPE_UNIT ||
      program->types[1].kind != W_SEED_HIR0_TYPE_STRING ||
      program->types[2].kind != W_SEED_HIR0_TYPE_I64 ||
      program->types[3].kind != W_SEED_HIR0_TYPE_BOOL ||
      program->types[0].owner_module != W_SEED_HIR0_NONE ||
      program->types[1].owner_module != W_SEED_HIR0_NONE ||
      program->types[2].owner_module != W_SEED_HIR0_NONE ||
      program->types[3].owner_module != W_SEED_HIR0_NONE ||
      program->types[0].enum_index != W_SEED_HIR0_NONE ||
      program->types[1].enum_index != W_SEED_HIR0_NONE ||
      program->types[2].enum_index != W_SEED_HIR0_NONE ||
      program->types[3].enum_index != W_SEED_HIR0_NONE ||
      program->types[0].first_subset_member != W_SEED_HIR0_NONE ||
      program->types[1].first_subset_member != W_SEED_HIR0_NONE ||
      program->types[2].first_subset_member != W_SEED_HIR0_NONE ||
      program->types[3].first_subset_member != W_SEED_HIR0_NONE ||
      program->types[0].subset_member_count != 0u ||
      program->types[1].subset_member_count != 0u ||
      program->types[2].subset_member_count != 0u ||
      program->types[3].subset_member_count != 0u ||
      program->types[0].external_module_index != W_SEED_HIR0_NONE ||
      program->types[0].external_symbol_index != W_SEED_HIR0_NONE ||
      program->types[1].external_module_index != W_SEED_HIR0_NONE ||
      program->types[1].external_symbol_index != W_SEED_HIR0_NONE ||
      program->types[2].external_module_index != W_SEED_HIR0_NONE ||
      program->types[2].external_symbol_index != W_SEED_HIR0_NONE ||
      program->types[3].external_module_index != W_SEED_HIR0_NONE ||
      program->types[3].external_symbol_index != W_SEED_HIR0_NONE ||
      !verify_external_records(program) || !verify_enum_records(program) ||
      !verify_enum_subset_records(program) ||
      !verify_identity_records(program) ||
      !verify_process_handler(program))
    return false;
  if (has_external_process) {
    for (size_t type = 4u; type < 7u; type += 1u)
      if (!hir_type_index_valid(program, (uint32_t)type)) return false;
    if (program->external_symbol_count == 7u &&
        !hir_type_index_valid(program,
                              (uint32_t)(program->type_count - 1u)))
      return false;
  }
  size_t module_function_cursor = 0u;
  size_t module_entry_cursor = 0u;
  for (size_t module = 0u; module < program->module_count; module += 1u) {
    const w_seed_hir0_module *value = &program->modules[module];
    if (value->module_index != module || value->identity_index != module ||
        !hir_text_valid(program, value->source_id) ||
        !hir_text_valid(program, value->module_id) ||
        !hir_text_valid(program, value->local_module_name) ||
         value->source_length == 0u ||
         !span_valid(value->source_span, value->source_length) ||
         value->first_function != module_function_cursor ||
         value->first_entry != module_entry_cursor ||
         !range_valid(value->first_function, value->function_count,
                     program->function_count) ||
        !range_valid(value->first_entry, value->entry_count,
                     program->entry_count))
      return false;
    for (size_t prior = 0u; prior < module; prior += 1u)
      if (hir_text_equal(program, value->module_id,
                          program->modules[prior].module_id))
        return false;
    module_function_cursor += value->function_count;
    module_entry_cursor += value->entry_count;
  }
  if (module_function_cursor != program->function_count ||
      module_entry_cursor != program->entry_count)
    return false;
  size_t function_parameter_cursor = 0u;
  size_t function_block_cursor = 0u;
  for (size_t function = 0u; function < program->function_count; function += 1u) {
    const w_seed_hir0_function *value = &program->functions[function];
    if (value->module_index >= program->module_count ||
        value->identity_index != program->module_count + function ||
        value->suspension > W_SEED_HIR0_SUSPENSION_MAY ||
        value->direct_entry > W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE ||
        !hir_text_valid(program, value->name) || value->name.count == 0u ||
        (value->return_type != 0u &&
         (!hir_type_index_valid(program, value->return_type) ||
          (value->return_type < 4u && value->return_type != 2u &&
           value->return_type != 3u))) ||
        !span_valid(value->source_span,
                    program->modules[value->module_index].source_length) ||
         !span_valid(value->body_span,
                     program->modules[value->module_index].source_length) ||
         value->first_parameter != function_parameter_cursor ||
         !range_valid(value->first_parameter, value->parameter_count,
                     program->parameter_count) ||
        value->first_block != function_block_cursor || value->block_count == 0u ||
        !range_valid(value->first_block, value->block_count,
                     program->block_count))
      return false;
    if (value->is_anonymous_entry &&
        (value->exported || value->parameter_count != 0u ||
         value->return_type != 0u))
      return false;
    for (size_t parameter = 0u; parameter < value->parameter_count; parameter += 1u) {
      const w_seed_hir0_parameter *item =
          &program->parameters[(size_t)value->first_parameter + parameter];
      if (item->owner_function != function || item->ordinal != parameter ||
          !hir_type_index_valid(program, item->type_index) ||
          item->type_index == 0u ||
          !hir_text_valid(program, item->name) || item->name.count == 0u ||
          !hir_text_valid(program, item->label) ||
          item->label_kind > W_SEED_HIR0_LABEL_REQUIRED ||
          !hir_label_valid(item->label_kind, item->label, true) ||
         !span_valid(item->source_span,
                      program->modules[value->module_index].source_length))
         return false;
    }
    for (size_t prior = 0u; prior < function; prior += 1u)
      if (program->functions[prior].module_index == value->module_index &&
          hir_text_equal(program, program->functions[prior].name, value->name))
        return false;
    function_parameter_cursor += value->parameter_count;
    function_block_cursor += value->block_count;
  }
  if (function_parameter_cursor != program->parameter_count ||
      function_block_cursor != program->block_count)
    return false;
  size_t block_instruction_cursor = 0u;
  for (size_t block = 0u; block < program->block_count; block += 1u) {
    const w_seed_hir0_block *value = &program->blocks[block];
    if (value->owner_function >= program->function_count ||
        block < program->functions[value->owner_function].first_block ||
        block >= (size_t)program->functions[value->owner_function].first_block +
                     program->functions[value->owner_function].block_count ||
        value->ordinal !=
            block - program->functions[value->owner_function].first_block ||
         value->next_block != W_SEED_HIR0_NONE ||
         value->first_instruction != block_instruction_cursor ||
        !range_valid(value->first_instruction, value->instruction_count,
                     program->instruction_count) ||
        value->terminator_index != block ||
        !span_valid(value->source_span,
                    program->modules[program->functions[value->owner_function]
                                         .module_index]
                        .source_length))
      return false;
    block_instruction_cursor += value->instruction_count;
  }
  if (block_instruction_cursor != program->instruction_count)
    return false;
  if (!verify_block_argument_records(program) ||
      !verify_edge_argument_records(program))
    return false;
  size_t call_instruction_cursor = 0u;
  size_t binding_instruction_cursor = 0u;
  size_t yield_instruction_count = 0u;
  for (size_t instruction = 0u; instruction < program->instruction_count;
       instruction += 1u) {
    const w_seed_hir0_instruction *value = &program->instructions[instruction];
    if (value->owner_block >= program->block_count ||
        value->result_type >= program->type_count)
      return false;
    const w_seed_hir0_block *block = &program->blocks[value->owner_block];
    if (instruction < block->first_instruction ||
        instruction >= (size_t)block->first_instruction + block->instruction_count ||
        value->ordinal != instruction - block->first_instruction ||
        !span_valid(value->source_span,
                    program->modules[program->functions[block->owner_function]
                                         .module_index]
                        .source_length))
      return false;
    if (value->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (value->call_index == W_SEED_HIR0_NONE ||
          (size_t)value->call_index >= program->call_count ||
          value->call_index != call_instruction_cursor ||
          value->binding_index != W_SEED_HIR0_NONE ||
          program->calls[value->call_index].owner_instruction != instruction)
        return false;
      call_instruction_cursor += 1u;
    } else if (value->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (value->binding_index == W_SEED_HIR0_NONE ||
          (size_t)value->binding_index >= program->binding_count ||
          value->binding_index != binding_instruction_cursor ||
          value->result_type != 0u ||
          value->call_index != W_SEED_HIR0_NONE)
        return false;
      const w_seed_hir0_binding *binding =
          &program->bindings[value->binding_index];
      if (binding->owner_instruction != instruction ||
          binding->owner_block != value->owner_block ||
          binding->ordinal != value->ordinal ||
          !hir_type_index_valid(program, binding->type_index) ||
          binding->type_index == 0u ||
          !hir_text_valid(program, binding->name) || binding->name.count == 0u ||
          binding->source_binding > value->binding_index ||
          binding->task_role > W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT ||
          ((binding->task_role == W_SEED_HIR0_TASK_ROLE_NONE) !=
           (binding->task_peer_binding == W_SEED_HIR0_NONE)) ||
          (binding->next_version != W_SEED_HIR0_NONE &&
           (binding->next_version <= value->binding_index ||
             binding->next_version >= program->binding_count)) ||
          (binding->task_peer_binding != W_SEED_HIR0_NONE &&
           (binding->task_peer_binding >= program->binding_count ||
            binding->task_peer_binding == value->binding_index)) ||
          binding->initializer_value >= program->value_count ||
          !span_valid(binding->source_span,
                      program->modules[program->functions[block->owner_function]
                                           .module_index]
                          .source_length))
        return false;
      if (binding->source_binding == value->binding_index) {
        if (binding->previous_version != W_SEED_HIR0_NONE) return false;
      } else {
        const w_seed_hir0_binding *source =
            &program->bindings[binding->source_binding];
        if (binding->previous_version == W_SEED_HIR0_NONE ||
            binding->previous_version >= value->binding_index)
          return false;
        const w_seed_hir0_binding *previous =
            &program->bindings[binding->previous_version];
        const w_seed_hir0_function *owner_function =
            &program->functions[block->owner_function];
        const bool source_available =
            source->owner_block == binding->owner_block
                ? source->owner_instruction < binding->owner_instruction
                : source->owner_block == owner_function->first_block ||
                      natural_loop_exit_binding_available(
                          program, binding->owner_block, source->owner_block);
        const bool previous_available =
            previous->owner_block == binding->owner_block
                ? previous->owner_instruction < binding->owner_instruction
                : previous->owner_block == owner_function->first_block ||
                      natural_loop_exit_binding_available(
                          program, binding->owner_block,
                          previous->owner_block);
        if (!binding->is_mutable || !source->is_mutable ||
            source->source_binding != binding->source_binding ||
            !source_available ||
            source->type_index != binding->type_index ||
            !hir_text_equal(program, source->name, binding->name) ||
            !previous->is_mutable ||
            previous->source_binding != binding->source_binding ||
            !previous_available ||
            previous->type_index != binding->type_index ||
            !hir_text_equal(program, previous->name, binding->name))
          return false;
      }
      if (binding->previous_version != W_SEED_HIR0_NONE &&
          program->bindings[binding->previous_version].next_version !=
              value->binding_index)
        return false;
      if (binding->next_version != W_SEED_HIR0_NONE) {
        const w_seed_hir0_binding *next =
            &program->bindings[binding->next_version];
        if (next->previous_version != value->binding_index ||
            next->source_binding != binding->source_binding)
          return false;
      }
      if (binding->task_peer_binding != W_SEED_HIR0_NONE) {
        const uint32_t peer_index = binding->task_peer_binding;
        const w_seed_hir0_binding *peer = &program->bindings[peer_index];
        if (peer->task_peer_binding != value->binding_index ||
            (binding->task_role == W_SEED_HIR0_TASK_ROLE_LAUNCH &&
             peer->task_role != W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT) ||
            (binding->task_role == W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT &&
             peer->task_role != W_SEED_HIR0_TASK_ROLE_LAUNCH) ||
            binding->task_role == W_SEED_HIR0_TASK_ROLE_NONE ||
            binding->is_mutable || peer->is_mutable ||
            binding->source_binding != value->binding_index ||
            peer->source_binding != peer_index ||
            binding->previous_version != W_SEED_HIR0_NONE ||
            binding->next_version != W_SEED_HIR0_NONE ||
            peer->previous_version != W_SEED_HIR0_NONE ||
            peer->next_version != W_SEED_HIR0_NONE ||
            binding->owner_block != peer->owner_block ||
            binding->type_index != peer->type_index ||
            peer->initializer_value >= program->value_count)
          return false;
        const bool is_launch =
            binding->task_role == W_SEED_HIR0_TASK_ROLE_LAUNCH;
        const w_seed_hir0_binding *launch = is_launch ? binding : peer;
        const w_seed_hir0_binding *joined = is_launch ? peer : binding;
        const uint32_t launch_index =
            is_launch ? value->binding_index : peer_index;
        const w_seed_hir0_value *launch_value =
            &program->values[launch->initializer_value];
        const w_seed_hir0_value *joined_value =
            &program->values[joined->initializer_value];
        if (launch_index >= (is_launch ? peer_index : value->binding_index) ||
            launch->owner_instruction >= joined->owner_instruction ||
            launch_value->kind != W_SEED_HIR0_VALUE_CALL_RESULT ||
            launch_value->call_index >= program->call_count ||
            (program->calls[launch_value->call_index].execution_kind !=
                 W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED &&
             program->calls[launch_value->call_index].execution_kind !=
                 W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED) ||
            joined_value->kind != W_SEED_HIR0_VALUE_BINDING_READ ||
            joined_value->binding_index != launch_index)
          return false;
      }
      binding_instruction_cursor += 1u;
    } else if (value->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) {
      if (value->call_index != W_SEED_HIR0_NONE ||
          value->binding_index != W_SEED_HIR0_NONE ||
          value->result_type != 0u ||
          !program->functions[block->owner_function].is_async)
        return false;
      yield_instruction_count += 1u;
    } else {
      return false;
    }
  }
  if (call_instruction_cursor != program->call_count ||
      binding_instruction_cursor != program->binding_count ||
      call_instruction_cursor + binding_instruction_cursor +
              yield_instruction_count !=
          program->instruction_count)
    return false;
  const size_t host_base = program->module_count + program->function_count +
                           program->entry_count;
  size_t call_argument_cursor = 0u;
  for (size_t call = 0u; call < program->call_count; call += 1u) {
    const w_seed_hir0_call *value = &program->calls[call];
    if (value->owner_instruction >= program->instruction_count ||
        value->owner_block >= program->block_count ||
        value->callee_identity >= program->identity_count ||
        (value->result_type != 0u &&
         (!hir_type_index_valid(program, value->result_type) ||
           (value->result_type < 4u && value->result_type != 2u &&
            value->result_type != 3u))) ||
         !hir0_call_execution_kind_is_closed(value->execution_kind) ||
         value->first_argument != call_argument_cursor ||
         !range_valid(value->first_argument, value->argument_count,
                     program->argument_count) ||
        !span_valid(value->source_span,
                    program->modules[program->functions[
                                         program->blocks[value->owner_block]
                                             .owner_function]
                                         .module_index]
                        .source_length))
      return false;
    const w_seed_hir0_instruction *instruction =
        &program->instructions[value->owner_instruction];
    const w_seed_hir0_identity *identity = &program->identities[value->callee_identity];
    const bool host_call =
        identity->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE;
    const bool local_call = identity->kind == W_SEED_HIR0_IDENTITY_FUNCTION;
    if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
        instruction->call_index != call ||
        instruction->binding_index != W_SEED_HIR0_NONE ||
        instruction->owner_block != value->owner_block ||
        instruction->ordinal != value->ordinal ||
        instruction->result_type != value->result_type ||
        (!host_call && !local_call) ||
        identity->parameter_count != value->argument_count ||
        identity->return_type != value->result_type)
      return false;
    if (host_call) {
      if (value->callee_identity < host_base ||
          identity->first_requirement != value->first_requirement ||
          identity->requirement_count != value->requirement_count ||
          !range_valid(value->first_requirement, value->requirement_count,
                       program->requirement_count))
        return false;
    } else if (value->callee_identity !=
                   program->module_count + identity->target_index ||
               identity->target_index >= program->function_count ||
               value->first_requirement != W_SEED_HIR0_NONE ||
               value->requirement_count != 0u) {
      return false;
    }
    if (value->execution_kind == W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED ||
        value->execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED) {
      const w_seed_hir0_function *target =
          local_call && identity->target_index < program->function_count
              ? &program->functions[identity->target_index]
              : NULL;
      const bool static_yield =
          value->execution_kind ==
          W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED;
      const bool target_non_suspending =
          !static_yield && target != NULL &&
          (target->suspension == W_SEED_HIR0_SUSPENSION_NEVER ||
           (target->is_async &&
            target->direct_entry == W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE));
      if (!local_call || identity->target_index >= program->function_count ||
          target == NULL || target->is_throws ||
          (!target_non_suspending &&
           !(static_yield && hir0_function_static_yields(
                                 program, identity->target_index))) ||
          value->owner_instruction + 1u >= program->instruction_count)
        return false;
      const w_seed_hir0_instruction *join_instruction =
          &program->instructions[value->owner_instruction + 1u];
      if (join_instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
          join_instruction->owner_block != value->owner_block ||
          join_instruction->binding_index >= program->binding_count)
        return false;
      const w_seed_hir0_binding *launch =
          &program->bindings[join_instruction->binding_index];
      if (launch->task_peer_binding == W_SEED_HIR0_NONE ||
          launch->initializer_value >= program->value_count)
        return false;
      const w_seed_hir0_value *result_value =
          &program->values[launch->initializer_value];
      if (result_value->kind != W_SEED_HIR0_VALUE_CALL_RESULT ||
          result_value->call_index != call)
        return false;
    }
    for (size_t argument = 0u; argument < value->argument_count; argument += 1u) {
      const w_seed_hir0_argument *item =
          &program->arguments[(size_t)value->first_argument + argument];
      if (item->owner_call != call || item->ordinal != argument ||
          item->parameter_ordinal >= identity->parameter_count ||
           item->value_index >= program->value_count ||
          !hir_text_valid(program, item->label) ||
          item->label_kind > W_SEED_HIR0_LABEL_REQUIRED ||
          !hir_label_valid(item->label_kind, item->label, true) ||
          !span_valid(item->source_span,
                      program->modules[program->functions[
                                           program->blocks[value->owner_block]
                                               .owner_function]
                                           .module_index]
                          .source_length))
        return false;
      for (size_t prior = 0u; prior < argument; prior += 1u) {
        const w_seed_hir0_argument *prior_item =
            &program->arguments[(size_t)value->first_argument + prior];
        if (prior_item->parameter_ordinal == item->parameter_ordinal)
          return false;
      }
      if (host_call) {
        const w_seed_hir0_host_parameter *parameter =
            &program->host_parameters[(size_t)identity->first_parameter +
                                      item->parameter_ordinal];
        if (item->type_index != parameter->type_index ||
            item->type_index != 1u ||
            item->label_kind != parameter->label_kind ||
            !hir_text_equal(program, item->label, parameter->label))
          return false;
      } else {
        const w_seed_hir0_parameter *parameter =
            &program->parameters[(size_t)identity->first_parameter +
                                 item->parameter_ordinal];
        if (parameter->owner_function != identity->target_index ||
            item->type_index != parameter->type_index ||
            item->label_kind != parameter->label_kind ||
            !hir_text_equal(program, item->label, parameter->label))
          return false;
      }
      call_argument_cursor += 1u;
    }
    for (size_t requirement = 0u; requirement < value->requirement_count;
         requirement += 1u) {
      const w_seed_hir0_requirement *item =
          &program->requirements[(size_t)value->first_requirement + requirement];
      if (item->owner_kind != W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY ||
          item->owner_index != value->callee_identity || item->ordinal != requirement ||
          !hir_text_valid(program, item->name) || item->name.count == 0u)
        return false;
    }
  }
  if (call_argument_cursor != program->argument_count)
    return false;
  for (size_t payload_index = 0u;
       payload_index < program->enum_payload_count; payload_index += 1u) {
    const w_seed_hir0_enum_payload *payload =
        &program->enum_payloads[payload_index];
    if (payload->owner_value >= program->value_count ||
        payload->value_index >= program->value_count)
      return false;
    const w_seed_hir0_value *owner = &program->values[payload->owner_value];
    if (owner->kind != W_SEED_HIR0_VALUE_ENUM_CASE ||
        !range_valid(owner->first_enum_payload, owner->enum_payload_count,
                     program->enum_payload_count) ||
        payload_index < owner->first_enum_payload ||
        payload_index >=
            (size_t)owner->first_enum_payload + owner->enum_payload_count)
      return false;
  }
  size_t value_byte_cursor = 0u;
  size_t value_cursor = 0u;
  size_t interpolation_segment_cursor = 0u;
  size_t switch_edge_cursor = 0u;
  size_t switch_capture_cursor = 0u;
  for (size_t instruction = 0u; instruction < program->instruction_count;
       instruction += 1u) {
    const w_seed_hir0_instruction *item = &program->instructions[instruction];
    if (item->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      const w_seed_hir0_binding *binding =
          &program->bindings[item->binding_index];
      const size_t function = program->blocks[item->owner_block].owner_function;
      const size_t module = program->functions[function].module_index;
      if (!verify_value_tree(
              program, binding->initializer_value,
              W_SEED_HIR0_VALUE_OWNER_BINDING, item->binding_index, 0u,
              item->owner_block, (uint32_t)instruction,
              program->modules[module].source_length, 0u, &value_cursor,
              &interpolation_segment_cursor, &value_byte_cursor))
        return false;
      const w_seed_hir0_value *initializer =
          &program->values[binding->initializer_value];
      if (!hir_type_assignable(program, initializer->type_index,
                               binding->type_index))
        return false;
      continue;
    }
    if (item->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) continue;
    const w_seed_hir0_call *call = &program->calls[item->call_index];
    for (size_t argument = 0u; argument < call->argument_count;
         argument += 1u) {
      const w_seed_hir0_argument *call_argument =
          &program->arguments[(size_t)call->first_argument + argument];
      const size_t function = program->blocks[item->owner_block].owner_function;
      const size_t module = program->functions[function].module_index;
      if (!verify_value_tree(
              program, call_argument->value_index,
              W_SEED_HIR0_VALUE_OWNER_ARGUMENT,
              (uint32_t)((size_t)call->first_argument + argument), 0u,
              item->owner_block, (uint32_t)instruction,
              program->modules[module].source_length, 0u, &value_cursor,
              &interpolation_segment_cursor, &value_byte_cursor))
        return false;
      const w_seed_hir0_value *root =
          &program->values[call_argument->value_index];
      if (!hir_type_assignable(program, root->type_index,
                               call_argument->type_index))
        return false;
      if (root->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
        const w_seed_hir0_binding *binding =
            &program->bindings[root->binding_index];
        const w_seed_hir0_function *owner_function =
            &program->functions[program->blocks[item->owner_block]
                                    .owner_function];
        const bool available_in_block =
            binding->owner_block == item->owner_block
                ? binding->owner_instruction < call->owner_instruction
                : binding->owner_block == owner_function->first_block ||
                      natural_loop_exit_binding_available(
                          program, item->owner_block,
                          binding->owner_block);
        if (!available_in_block ||
            program->blocks[binding->owner_block].owner_function !=
                program->blocks[item->owner_block].owner_function ||
            binding->type_index != root->type_index)
          return false;
      }
    }
  }
  for (size_t terminator = 0u; terminator < program->terminator_count;
       terminator += 1u) {
    const w_seed_hir0_terminator *value = &program->terminators[terminator];
    const w_seed_hir0_block *block = &program->blocks[terminator];
    const uint32_t function = block->owner_function;
    const size_t source_length =
        program->modules[program->functions[function].module_index]
            .source_length;
    if (value->owner_block != terminator ||
        value->ordinal != block->instruction_count ||
        !span_valid(value->source_span, source_length))
      return false;
    if ((uint32_t)value->logical_operator >
            (uint32_t)W_SEED_HIR0_LOGICAL_OR ||
        (value->kind != W_SEED_HIR0_TERMINATOR_BRANCH &&
         value->logical_operator != W_SEED_HIR0_LOGICAL_NONE) ||
        (value->kind != W_SEED_HIR0_TERMINATOR_JUMP &&
         (value->first_edge_argument != W_SEED_HIR0_NONE ||
          value->edge_argument_count != 0u)) ||
        (value->kind != W_SEED_HIR0_TERMINATOR_SWITCH_ENUM &&
         (value->switch_enum_index != W_SEED_HIR0_NONE ||
          value->first_switch_edge != W_SEED_HIR0_NONE ||
          value->switch_edge_count != 0u ||
          value->switch_carrier_width != 0u)))
      return false;
    if (value->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
       if ((value->logical_operator == W_SEED_HIR0_LOGICAL_NONE
               ? (value->result_type != 0u && value->result_type != 2u &&
                  value->result_type != 3u)
               : value->result_type != 3u) ||
          value->value_index == W_SEED_HIR0_NONE ||
          value->value_index >= program->value_count ||
          value->target_block == W_SEED_HIR0_NONE ||
          value->else_block == W_SEED_HIR0_NONE ||
          value->target_block >= program->block_count ||
          value->else_block >= program->block_count ||
          value->first_edge_argument != W_SEED_HIR0_NONE ||
          value->edge_argument_count != 0u ||
          program->blocks[value->target_block].owner_function != function ||
          program->blocks[value->else_block].owner_function != function ||
          value->target_block == value->else_block ||
          !verify_value_tree(
              program, value->value_index,
              W_SEED_HIR0_VALUE_OWNER_TERMINATOR, (uint32_t)terminator, 0u,
              (uint32_t)terminator,
              (uint32_t)((size_t)block->first_instruction +
                         block->instruction_count),
              source_length, 0u, &value_cursor,
              &interpolation_segment_cursor, &value_byte_cursor) ||
          program->values[value->value_index].type_index != 3u)
        return false;
      continue;
    }
    if (value->kind == W_SEED_HIR0_TERMINATOR_SWITCH_ENUM) {
      if (value->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
          value->value_index == W_SEED_HIR0_NONE ||
          value->value_index >= program->value_count ||
          value->result_type != program->functions[function].return_type ||
          value->target_block != W_SEED_HIR0_NONE ||
          value->else_block != W_SEED_HIR0_NONE ||
          value->first_edge_argument != W_SEED_HIR0_NONE ||
          value->edge_argument_count != 0u ||
          value->switch_enum_index >= program->enum_count ||
          value->first_switch_edge == W_SEED_HIR0_NONE ||
          !range_valid(value->first_switch_edge, value->switch_edge_count,
                       program->switch_edge_count))
        return false;
      const w_seed_hir0_enum *decl =
          &program->enums[value->switch_enum_index];
      const uint32_t enum_type = decl->type_index;
      if (value->first_switch_edge != switch_edge_cursor ||
          !hir_type_index_valid(program, enum_type) ||
          program->types[enum_type].kind != W_SEED_HIR0_TYPE_ENUM ||
          program->types[enum_type].enum_index != value->switch_enum_index ||
          !verify_value_tree(
              program, value->value_index,
              W_SEED_HIR0_VALUE_OWNER_TERMINATOR, (uint32_t)terminator, 0u,
              (uint32_t)terminator,
              (uint32_t)((size_t)block->first_instruction +
                         block->instruction_count),
              source_length, 0u, &value_cursor,
              &interpolation_segment_cursor, &value_byte_cursor) ||
          program->values[value->value_index].type_index >= program->type_count)
        return false;
      const uint32_t subject_type_index =
          program->values[value->value_index].type_index;
      const w_seed_hir0_type *subject_type =
          &program->types[subject_type_index];
      const bool subject_is_base =
          subject_type_index == enum_type &&
          subject_type->kind == W_SEED_HIR0_TYPE_ENUM;
      const bool subject_is_subset =
          subject_type->kind == W_SEED_HIR0_TYPE_ENUM_SUBSET &&
          subject_type->enum_index == value->switch_enum_index &&
          hir_type_index_valid(program, subject_type_index);
      const size_t expected_switch_count =
          subject_is_subset ? subject_type->subset_member_count
                            : decl->case_count;
      if ((!subject_is_base && !subject_is_subset) ||
          expected_switch_count == 0u ||
          value->switch_edge_count != expected_switch_count ||
          value->switch_carrier_width !=
              hir0_enum_carrier_width(decl->case_count))
        return false;
      const size_t function_end =
          (size_t)program->functions[function].first_block +
          program->functions[function].block_count;
      for (size_t ordinal = 0u; ordinal < value->switch_edge_count;
           ordinal += 1u) {
        const size_t edge_index =
            (size_t)value->first_switch_edge + ordinal;
        const w_seed_hir0_switch_edge *edge =
            &program->switch_edges[edge_index];
        uint32_t selected_case = W_SEED_HIR0_NONE;
        if (subject_is_subset) {
          if (!hir_enum_subset_case_for_ordinal(
                  program, subject_type_index, ordinal, &selected_case))
            return false;
        } else {
          selected_case = decl->first_case + (uint32_t)ordinal;
        }
        const size_t case_index = selected_case;
        const size_t base_ordinal = case_index - decl->first_case;
        if (edge->owner_terminator != terminator ||
            edge->ordinal != ordinal || edge->enum_index != value->switch_enum_index ||
            edge->enum_case_index != case_index ||
            edge->target_block >= program->block_count ||
            edge->target_block <= terminator ||
            edge->target_block >= function_end ||
            program->blocks[edge->target_block].owner_function != function ||
            !span_valid(edge->source_span, source_length) ||
            case_index >= program->enum_case_count ||
            program->enum_cases[case_index].owner_enum !=
                value->switch_enum_index ||
            program->enum_cases[case_index].ordinal != base_ordinal ||
            program->enum_cases[case_index].tag != base_ordinal ||
            edge->first_capture != switch_capture_cursor ||
            !range_valid(edge->first_capture, edge->capture_count,
                         program->switch_capture_count) ||
            edge->capture_count > program->enum_cases[case_index].payload_count)
          return false;
        for (size_t capture_ordinal = 0u;
             capture_ordinal < edge->capture_count; capture_ordinal += 1u) {
          const w_seed_hir0_switch_capture *capture =
              &program->switch_captures[switch_capture_cursor + capture_ordinal];
          const w_seed_hir0_enum_case *enum_case = &program->enum_cases[case_index];
          if (capture->owner_switch_edge != edge_index ||
              capture->ordinal != capture_ordinal ||
              capture->parameter_ordinal >= enum_case->payload_count ||
              (capture->type_index != W_SEED_HIR0_TYPE_I64 &&
               capture->type_index != W_SEED_HIR0_TYPE_BOOL) ||
              !hir_text_valid(program, capture->name) || capture->name.count == 0u ||
              !span_valid(capture->source_span, source_length) ||
              program->enum_case_parameters[(size_t)enum_case->first_payload +
                  capture->parameter_ordinal].type_index != capture->type_index)
            return false;
          for (size_t prior = 0u; prior < capture_ordinal; prior += 1u)
            if (program->switch_captures[switch_capture_cursor + prior]
                    .parameter_ordinal == capture->parameter_ordinal ||
                hir_text_equal(program,
                    program->switch_captures[switch_capture_cursor + prior].name,
                    capture->name))
              return false;
        }
        switch_capture_cursor += edge->capture_count;
        const w_seed_hir0_terminator *arm =
            &program->terminators[edge->target_block];
        if (arm->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
            arm->owner_block != edge->target_block ||
            arm->result_type != program->functions[function].return_type ||
            arm->value_index == W_SEED_HIR0_NONE ||
            arm->target_block != W_SEED_HIR0_NONE ||
            arm->else_block != W_SEED_HIR0_NONE ||
            arm->first_edge_argument != W_SEED_HIR0_NONE ||
            arm->edge_argument_count != 0u)
          return false;
      }
      switch_edge_cursor += value->switch_edge_count;
      continue;
    }
    if (value->kind == W_SEED_HIR0_TERMINATOR_JUMP) {
      if (value->value_index != W_SEED_HIR0_NONE || value->result_type != 0u ||
          value->target_block == W_SEED_HIR0_NONE ||
          value->target_block >= program->block_count ||
          value->else_block != W_SEED_HIR0_NONE ||
          program->blocks[value->target_block].owner_function != function)
        return false;
      const w_seed_hir0_block *target = &program->blocks[value->target_block];
      if ((target->block_argument_count == 0u) !=
          (value->edge_argument_count == 0u) ||
          target->block_argument_count != value->edge_argument_count ||
          (target->block_argument_count != 0u &&
           value->first_edge_argument == W_SEED_HIR0_NONE))
        return false;
      for (size_t ordinal = 0u; ordinal < value->edge_argument_count;
           ordinal += 1u) {
        const w_seed_hir0_edge_argument *edge =
            &program->edge_arguments[(size_t)value->first_edge_argument +
                                     ordinal];
        if (edge->type_index !=
                program->block_arguments[(size_t)target->first_block_argument +
                                         ordinal]
                    .type_index ||
            !verify_value_tree(
                program, edge->value_index,
                W_SEED_HIR0_VALUE_OWNER_TERMINATOR, (uint32_t)terminator,
                (uint32_t)ordinal, (uint32_t)terminator,
                (uint32_t)((size_t)block->first_instruction +
                           block->instruction_count),
                source_length, 0u, &value_cursor,
                &interpolation_segment_cursor, &value_byte_cursor) ||
            program->values[edge->value_index].type_index != edge->type_index)
          return false;
      }
      continue;
    }
    if (value->kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT) {
      if (program->functions[function].return_type != 0u ||
          value->value_index != W_SEED_HIR0_NONE || value->result_type != 0u ||
          value->target_block != W_SEED_HIR0_NONE ||
          value->else_block != W_SEED_HIR0_NONE ||
          value->first_edge_argument != W_SEED_HIR0_NONE ||
          value->edge_argument_count != 0u)
        return false;
      continue;
    }
    if (value->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
        program->functions[function].return_type == 0u ||
        !hir_type_index_valid(program,
                              program->functions[function].return_type) ||
        (program->functions[function].return_type < 4u &&
         program->functions[function].return_type != 2u &&
         program->functions[function].return_type != 3u) ||
        value->value_index == W_SEED_HIR0_NONE ||
        value->value_index >= program->value_count ||
        value->result_type != program->functions[function].return_type ||
        value->target_block != W_SEED_HIR0_NONE ||
        value->else_block != W_SEED_HIR0_NONE ||
        value->first_edge_argument != W_SEED_HIR0_NONE ||
        value->edge_argument_count != 0u ||
        !verify_value_tree(
            program, value->value_index,
            W_SEED_HIR0_VALUE_OWNER_TERMINATOR, (uint32_t)terminator, 0u,
            (uint32_t)terminator,
            (uint32_t)((size_t)block->first_instruction +
                       block->instruction_count),
            source_length, 0u, &value_cursor,
            &interpolation_segment_cursor, &value_byte_cursor) ||
         !hir_type_assignable(program,
                              program->values[value->value_index].type_index,
                              value->result_type))
      return false;
  }
  /* The bounded producer proves exact reachability from each function entry.
   * Contiguous arm ranges and forward joins prove acyclicity and common
   * postdominators outside the one exact natural-loop shape; that shape proves
   * one header-dominated body/exit and one unique backedge separately. */
  for (size_t function = 0u; function < program->function_count; function += 1u)
    if (!verify_cfg_function(program, function) ||
        !verify_logical_join_membership(program, function))
      return false;
  if (value_cursor != program->value_count ||
      interpolation_segment_cursor != program->interpolation_segment_count ||
      value_byte_cursor != program->value_byte_count ||
      switch_edge_cursor != program->switch_edge_count ||
      switch_capture_cursor != program->switch_capture_count)
    return false;
  for (size_t entry = 0u; entry < program->entry_count; entry += 1u) {
    const w_seed_hir0_entry *value = &program->entries[entry];
    if (value->module_index >= program->module_count ||
        value->identity_index != program->module_count + program->function_count + entry ||
        value->target_function >= program->function_count ||
        value->target_identity != program->functions[value->target_function].identity_index ||
        program->functions[value->target_function].module_index != value->module_index ||
        !hir_text_valid(program, value->target_name) || value->target_name.count == 0u ||
        !hir_text_equal(program, value->target_name,
                        program->functions[value->target_function].name) ||
        value->is_body !=
            program->functions[value->target_function].is_anonymous_entry ||
        !hir_text_is(program, value->slot, HIR0_SLOT_NAME) ||
        !span_valid(value->source_span,
                    program->modules[value->module_index].source_length))
      return false;
    if (!value->is_body &&
        program->functions[value->target_function].is_anonymous_entry)
      return false;
    const w_seed_hir0_module *module = &program->modules[value->module_index];
    if (entry < module->first_entry ||
        entry >= (size_t)module->first_entry + module->entry_count)
      return false;
    if (value->module_index != 0u ||
        (program->module_count > 1u && value->is_body))
      return false;
  }
  return true;
}

bool w_seed_hir0_program_from_output(const w_seed_hir0_output *output,
                                     const w_seed_hir0_result *result,
                                     w_seed_hir0_program *program) {
  if (output == NULL || result == NULL || program == NULL ||
      result->status != W_SEED_HIR0_OK ||
      memcmp(result->schema, W_SEED_HIR0_SCHEMA_VERSION,
             sizeof(result->schema)) != 0)
    return false;
  const w_seed_hir0_counts counts = result->written;
  const w_seed_hir0_counts required = result->required;
  if (!hir_counts_equal(&counts, &required)) return false;
  hir0_memory_range output_ranges[37];
  size_t output_range_count = 0u;
  if (!output_range_table(output, output_ranges, &output_range_count) ||
      ranges_overlap(program, sizeof(*program), output, sizeof(*output)) ||
      ranges_overlap(program, sizeof(*program), result, sizeof(*result)) ||
      output_overlaps_memory(output_ranges, output_range_count, program,
                             sizeof(*program)))
    return false;
  w_seed_hir0_program candidate = {
      .modules = output->modules,
      .module_count = counts.modules,
      .module_capacity = output->module_capacity,
      .identities = output->identities,
      .identity_count = counts.identities,
      .identity_capacity = output->identity_capacity,
      .types = output->types,
      .type_count = counts.types,
      .type_capacity = output->type_capacity,
      .enums = output->enums,
      .enum_count = counts.enums,
      .enum_capacity = output->enum_capacity,
      .enum_cases = output->enum_cases,
      .enum_case_count = counts.enum_cases,
      .enum_case_capacity = output->enum_case_capacity,
      .enum_case_parameters = output->enum_case_parameters,
      .enum_case_parameter_count = counts.enum_case_parameters,
      .enum_case_parameter_capacity = output->enum_case_parameter_capacity,
      .enum_subset_members = output->enum_subset_members,
      .enum_subset_member_count = counts.enum_subset_members,
      .enum_subset_member_capacity = output->enum_subset_member_capacity,
      .functions = output->functions,
      .function_count = counts.functions,
      .function_capacity = output->function_capacity,
      .parameters = output->parameters,
      .parameter_count = counts.parameters,
      .parameter_capacity = output->parameter_capacity,
      .blocks = output->blocks,
      .block_count = counts.blocks,
      .block_capacity = output->block_capacity,
      .block_arguments = output->block_arguments,
      .block_argument_count = counts.block_arguments,
      .block_argument_capacity = output->block_argument_capacity,
      .edge_arguments = output->edge_arguments,
      .edge_argument_count = counts.edge_arguments,
      .edge_argument_capacity = output->edge_argument_capacity,
      .switch_edges = output->switch_edges,
      .switch_edge_count = counts.switch_edges,
      .switch_edge_capacity = output->switch_edge_capacity,
      .switch_captures = output->switch_captures,
      .switch_capture_count = counts.switch_captures,
      .switch_capture_capacity = output->switch_capture_capacity,
      .instructions = output->instructions,
      .instruction_count = counts.instructions,
      .instruction_capacity = output->instruction_capacity,
      .bindings = output->bindings,
      .binding_count = counts.bindings,
      .binding_capacity = output->binding_capacity,
      .calls = output->calls,
      .call_count = counts.calls,
      .call_capacity = output->call_capacity,
      .host_parameters = output->host_parameters,
      .host_parameter_count = counts.host_parameters,
      .host_parameter_capacity = output->host_parameter_capacity,
      .arguments = output->arguments,
      .argument_count = counts.arguments,
      .argument_capacity = output->argument_capacity,
      .enum_payloads = output->enum_payloads,
      .enum_payload_count = counts.enum_payloads,
      .enum_payload_capacity = output->enum_payload_capacity,
      .requirements = output->requirements,
      .requirement_count = counts.requirements,
      .requirement_capacity = output->requirement_capacity,
      .values = output->values,
      .value_count = counts.values,
      .value_capacity = output->value_capacity,
      .interpolation_segments = output->interpolation_segments,
      .interpolation_segment_count = counts.interpolation_segments,
      .interpolation_segment_capacity = output->interpolation_segment_capacity,
      .terminators = output->terminators,
      .terminator_count = counts.terminators,
      .terminator_capacity = output->terminator_capacity,
      .entries = output->entries,
      .entry_count = counts.entries,
      .entry_capacity = output->entry_capacity,
      .text_bytes = output->text_bytes,
      .text_byte_count = counts.text_bytes,
      .text_byte_capacity = output->text_byte_capacity,
      .value_bytes = output->value_bytes,
      .value_byte_count = counts.value_bytes,
      .value_byte_capacity = output->value_byte_capacity,
      .receipt = output->receipt,
      .receipt_count = counts.receipt_bytes,
      .receipt_capacity = output->receipt_capacity,
      .external_modules = output->external_modules,
      .external_module_count = counts.external_modules,
      .external_module_capacity = output->external_module_capacity,
      .external_symbols = output->external_symbols,
      .external_symbol_count = counts.external_symbols,
      .external_symbol_capacity = output->external_symbol_capacity};
  if (!basic_program_shape(&candidate, result) || program_aliases(&candidate))
    return false;
  *program = candidate;
  return true;
}

bool w_seed_hir0_verify(const w_seed_hir0_program *program,
                        const w_seed_hir0_result *result) {
  if (!basic_program_shape(program, result) || program_aliases(program))
    return false;
  size_t subset_base = 0u;
  size_t subset_count = 0u;
  uint32_t usize_type_index = W_SEED_HIR0_NONE;
  if (!hir_enum_subset_layout(program, &subset_base, &subset_count,
                              &usize_type_index))
    return false;
  (void)subset_base;
  (void)usize_type_index;
  w_seed_hir0_counts counts = {
      .modules = program->module_count,
      .identities = program->identity_count,
      .types = program->type_count,
      .functions = program->function_count,
      .parameters = program->parameter_count,
      .blocks = program->block_count,
      .block_arguments = program->block_argument_count,
      .edge_arguments = program->edge_argument_count,
      .switch_edges = program->switch_edge_count,
      .switch_captures = program->switch_capture_count,
      .instructions = program->instruction_count,
      .bindings = program->binding_count,
      .calls = program->call_count,
      .host_parameters = program->host_parameter_count,
      .arguments = program->argument_count,
      .enum_payloads = program->enum_payload_count,
      .requirements = program->requirement_count,
      .values = program->value_count,
      .interpolation_segments = program->interpolation_segment_count,
      .terminators = program->terminator_count,
      .entries = program->entry_count,
      .text_bytes = program->text_byte_count,
      .value_bytes = program->value_byte_count,
      .receipt_bytes = program->receipt_count,
      .external_modules = program->external_module_count,
      .external_symbols = program->external_symbol_count,
      .enums = program->enum_count,
      .enum_cases = program->enum_case_count,
      .enum_case_parameters = program->enum_case_parameter_count,
      .enum_subsets = subset_count,
      .enum_subset_members = program->enum_subset_member_count};
  if (result->required.modules != counts.modules ||
      result->required.identities != counts.identities ||
      result->required.types != counts.types ||
      result->required.functions != counts.functions ||
      result->required.parameters != counts.parameters ||
      result->required.blocks != counts.blocks ||
      result->required.block_arguments != counts.block_arguments ||
      result->required.edge_arguments != counts.edge_arguments ||
      result->required.switch_edges != counts.switch_edges ||
      result->required.switch_captures != counts.switch_captures ||
      result->required.instructions != counts.instructions ||
      result->required.bindings != counts.bindings ||
      result->required.calls != counts.calls ||
      result->required.host_parameters != counts.host_parameters ||
      result->required.arguments != counts.arguments ||
      result->required.enum_payloads != counts.enum_payloads ||
      result->required.requirements != counts.requirements ||
      result->required.values != counts.values ||
      result->required.interpolation_segments != counts.interpolation_segments ||
      result->required.terminators != counts.terminators ||
      result->required.entries != counts.entries ||
      result->required.text_bytes != counts.text_bytes ||
      result->required.value_bytes != counts.value_bytes ||
      result->required.receipt_bytes != counts.receipt_bytes ||
      result->written.modules != counts.modules ||
      result->written.identities != counts.identities ||
      result->written.types != counts.types ||
      result->written.functions != counts.functions ||
      result->written.parameters != counts.parameters ||
      result->written.blocks != counts.blocks ||
      result->written.block_arguments != counts.block_arguments ||
      result->written.edge_arguments != counts.edge_arguments ||
      result->written.switch_edges != counts.switch_edges ||
      result->written.switch_captures != counts.switch_captures ||
      result->written.instructions != counts.instructions ||
      result->written.bindings != counts.bindings ||
      result->written.calls != counts.calls ||
      result->written.host_parameters != counts.host_parameters ||
      result->written.arguments != counts.arguments ||
      result->written.enum_payloads != counts.enum_payloads ||
      result->written.requirements != counts.requirements ||
      result->written.values != counts.values ||
      result->written.interpolation_segments != counts.interpolation_segments ||
      result->written.terminators != counts.terminators ||
      result->written.entries != counts.entries ||
      result->written.text_bytes != counts.text_bytes ||
      result->written.value_bytes != counts.value_bytes ||
      result->written.receipt_bytes != counts.receipt_bytes ||
      result->required.external_modules != counts.external_modules ||
      result->required.external_symbols != counts.external_symbols ||
      result->written.external_modules != counts.external_modules ||
      result->written.external_symbols != counts.external_symbols ||
      result->required.enums != counts.enums ||
      result->required.enum_cases != counts.enum_cases ||
      result->required.enum_case_parameters != counts.enum_case_parameters ||
      result->required.enum_subsets != counts.enum_subsets ||
      result->required.enum_subset_members != counts.enum_subset_members ||
      result->written.enums != counts.enums ||
      result->written.enum_cases != counts.enum_cases ||
      result->written.enum_case_parameters != counts.enum_case_parameters ||
      result->written.enum_subsets != counts.enum_subsets ||
      result->written.enum_subset_members != counts.enum_subset_members ||
      !verify_records(program))
    return false;
  if (!verify_process_lifecycle_facts(program)) return false;
  if (!verify_direct_entry_facts(program)) return false;
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  digest_program(program, &counts, semantic_digest);
  digest_provenance(program, &counts, provenance_digest);
  if (memcmp(semantic_digest, result->semantic_digest,
             sizeof(semantic_digest)) != 0 ||
      memcmp(provenance_digest, result->provenance_digest,
             sizeof(provenance_digest)) != 0)
    return false;
  uint8_t expected_receipt[HIR0_RECEIPT_BYTES];
  write_receipt_unchecked(expected_receipt, &counts, semantic_digest,
                          provenance_digest);
  if (memcmp(expected_receipt, program->receipt, HIR0_RECEIPT_BYTES) != 0)
    return false;
  return true;
}

w_seed_hir0_status w_seed_hir0_measure(const w_seed_hir0_input *input,
                                       w_seed_hir0_counts *counts,
                                       w_seed_hir0_result *result) {
  if (counts == NULL || result == NULL ||
      ranges_overlap(counts, sizeof(*counts), result, sizeof(*result)))
    return W_SEED_HIR0_INVALID;
  w_seed_hir0_counts candidate_counts;
  w_seed_hir0_result candidate_result;
  (void)memset(&candidate_counts, 0, sizeof(candidate_counts));
  (void)memset(&candidate_result, 0, sizeof(candidate_result));
  const hir0_prepare_status prepared = collect(input, &candidate_counts);
  if (prepared != HIR0_PREPARE_READY) {
    return prepared == HIR0_PREPARE_FRONTEND
               ? W_SEED_HIR0_FRONTEND
               : prepared == HIR0_PREPARE_UNSUPPORTED ? W_SEED_HIR0_UNSUPPORTED
                                                       : W_SEED_HIR0_INVALID;
  }
  (void)memcpy(candidate_result.schema, W_SEED_HIR0_SCHEMA_VERSION,
               sizeof(candidate_result.schema));
  candidate_result.required = candidate_counts;
  candidate_result.status = W_SEED_HIR0_OK;
  *counts = candidate_counts;
  *result = candidate_result;
  return W_SEED_HIR0_OK;
}

w_seed_hir0_status w_seed_hir0_run(const w_seed_hir0_input *input,
                                   w_seed_hir0_output *output,
                                   w_seed_hir0_result *result) {
  if (result == NULL) return W_SEED_HIR0_INVALID;
  w_seed_hir0_counts counts;
  w_seed_hir0_result measured;
  const w_seed_hir0_status measured_status =
      w_seed_hir0_measure(input, &counts, &measured);
  if (measured_status != W_SEED_HIR0_OK) {
    return measured_status;
  }
  if (output == NULL || !output_capacity_ok(output, &counts)) {
    return W_SEED_HIR0_CAPACITY;
  }
  if (output_aliases(output, &counts) ||
      output_overlaps_input(input, output, result)) {
    return W_SEED_HIR0_INVALID;
  }
  w_seed_hir0_program program;
  w_seed_hir0_result candidate_result = measured;
  candidate_result.written = counts;
  (void)memcpy(candidate_result.schema, W_SEED_HIR0_SCHEMA_VERSION,
               sizeof(candidate_result.schema));
  /* The bridge is structural here: records are still unwritten, but all
   * pointers, capacities, counts, and alias relations are already fixed. */
  if (!w_seed_hir0_program_from_output(output, &candidate_result, &program)) {
    return W_SEED_HIR0_INVALID;
  }
  /* All branches of emission are proven by collect() and the alias/capacity
   * preflight above. From this first write onward the commit is infallible. */
  emit_records(input, &counts, output);
  /* collect() and the capacity/alias preflight prove the helper domains over
   * this now-complete caller-owned HIR. Its bounded scratch and those finite
   * relations keep this post-emission step infallible. */
  hir0_publish_process_lifecycle_facts(&program, output);
  hir0_publish_direct_entry_facts(&program, output);
  digest_program(&program, &counts, candidate_result.semantic_digest);
  digest_provenance(&program, &counts, candidate_result.provenance_digest);
  write_receipt_unchecked(output->receipt, &counts,
                          candidate_result.semantic_digest,
                          candidate_result.provenance_digest);
  candidate_result.status = W_SEED_HIR0_OK;
  *result = candidate_result;
  return W_SEED_HIR0_OK;
}
