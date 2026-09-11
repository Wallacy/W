#include "w_seed_native_subset0.h"

#include <limits.h>
#include <string.h>

static const uint8_t NATIVE_SUBSET0_PROFILE[] = "native-process@1";
static const uint8_t NATIVE_SUBSET0_SLOT[] = ".default";
static const uint8_t NATIVE_SUBSET0_CALLEE[] = "print";
static const uint8_t NATIVE_SUBSET0_REQUIREMENT[] = "Console";

static bool text_is(const w_seed_hir0_program *program,
                    w_seed_hir0_text text, const uint8_t *literal,
                    size_t literal_bytes) {
  if (program == NULL || literal == NULL || text.offset > program->text_byte_count ||
      text.count != literal_bytes ||
      text.count > program->text_byte_count - text.offset ||
      (text.count != 0u && program->text_bytes == NULL))
    return false;
  return text.count == 0u ||
         memcmp(program->text_bytes + text.offset, literal, text.count) == 0;
}

static bool text_equal(const w_seed_hir0_program *program,
                       w_seed_hir0_text left, w_seed_hir0_text right) {
  if (program == NULL || left.offset > program->text_byte_count ||
      right.offset > program->text_byte_count ||
      left.count > program->text_byte_count - left.offset ||
      right.count > program->text_byte_count - right.offset ||
      left.count != right.count ||
      (left.count != 0u && program->text_bytes == NULL))
    return false;
  return left.count == 0u ||
         memcmp(program->text_bytes + left.offset,
                program->text_bytes + right.offset, left.count) == 0;
}

w_seed_native_subset0_status w_seed_native_subset0_select(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_selection *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return W_SEED_NATIVE_SUBSET0_INVALID;

  const bool direct_shape = program->binding_count == 0u &&
                            program->instruction_count == 1u;
  const bool binding_shape = program->binding_count == 1u &&
                             program->instruction_count == 2u;
  if (program->module_count != 1u || program->function_count != 1u ||
      program->parameter_count != 0u || program->block_count != 1u ||
      program->call_count != 1u || program->argument_count != 1u ||
      program->requirement_count != 1u ||
      program->value_count != (binding_shape ? 2u : 1u) ||
      program->terminator_count != 1u || program->entry_count != 1u ||
      (!direct_shape && !binding_shape))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  (void)memset(selection, 0, sizeof(*selection));
  selection->entry = &program->entries[0];
  if (selection->entry->target_function >= program->function_count ||
      selection->entry->target_name.count == 0u ||
      !text_is(program, selection->entry->slot, NATIVE_SUBSET0_SLOT,
               sizeof(NATIVE_SUBSET0_SLOT) - 1u))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  selection->function = &program->functions[selection->entry->target_function];
  if (!text_equal(program, selection->function->name,
                  selection->entry->target_name) ||
      selection->function->name.count == 0u ||
      selection->function->parameter_count != 0u ||
      selection->function->return_type >= program->type_count ||
      program->types[selection->function->return_type].kind !=
          W_SEED_HIR0_TYPE_UNIT ||
      selection->function->is_async || selection->function->is_throws ||
      selection->function->is_unsafe ||
      selection->function->has_borrow_clause ||
      selection->function->block_count != 1u ||
      selection->function->first_block >= program->block_count)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  selection->block = &program->blocks[selection->function->first_block];
  const size_t expected_instructions = binding_shape ? 2u : 1u;
  if (selection->block->instruction_count != expected_instructions ||
      selection->block->first_instruction >
          program->instruction_count - expected_instructions ||
      selection->block->terminator_index >= program->terminator_count)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  const size_t first_instruction = selection->block->first_instruction;
  if (binding_shape) {
    const w_seed_hir0_instruction *binding_instruction =
        &program->instructions[first_instruction];
    selection->binding = &program->bindings[0];
    if (binding_instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
        binding_instruction->binding_index != 0u ||
        binding_instruction->call_index != W_SEED_HIR0_NONE ||
        binding_instruction->owner_block != selection->function->first_block ||
        binding_instruction->ordinal != 0u ||
        binding_instruction->result_type >= program->type_count ||
        program->types[binding_instruction->result_type].kind !=
            W_SEED_HIR0_TYPE_UNIT ||
        selection->binding->owner_instruction != first_instruction ||
        selection->binding->owner_block != selection->function->first_block ||
        selection->binding->ordinal != 0u || selection->binding->is_mutable ||
        selection->binding->type_index >= program->type_count ||
        program->types[selection->binding->type_index].kind !=
            W_SEED_HIR0_TYPE_STRING ||
        selection->binding->name.count == 0u ||
        selection->binding->initializer_value >= program->value_count)
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
    const w_seed_hir0_value *initializer =
        &program->values[selection->binding->initializer_value];
    if (initializer->kind != W_SEED_HIR0_VALUE_CONST_STRING ||
        initializer->owner_kind != W_SEED_HIR0_VALUE_OWNER_BINDING ||
        initializer->owner_index != 0u || initializer->owner_ordinal != 0u ||
        initializer->type_index != selection->binding->type_index ||
        initializer->byte_count > W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD)
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  }

  selection->instruction = &program->instructions[
      first_instruction + (binding_shape ? 1u : 0u)];
  if (selection->instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
      selection->instruction->call_index >= program->call_count ||
      selection->instruction->binding_index != W_SEED_HIR0_NONE ||
      selection->instruction->result_type >= program->type_count ||
      program->types[selection->instruction->result_type].kind !=
          W_SEED_HIR0_TYPE_UNIT)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  selection->call = &program->calls[selection->instruction->call_index];
  if (selection->call->owner_instruction !=
          first_instruction + (binding_shape ? 1u : 0u) ||
      selection->call->argument_count != 1u ||
      selection->call->requirement_count != 1u ||
      selection->call->callee_identity >= program->identity_count ||
      selection->call->first_argument >= program->argument_count ||
      selection->call->first_requirement >= program->requirement_count ||
      selection->call->result_type >= program->type_count ||
      program->types[selection->call->result_type].kind !=
          W_SEED_HIR0_TYPE_UNIT)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  selection->callee = &program->identities[selection->call->callee_identity];
  selection->requirement =
      &program->requirements[selection->call->first_requirement];
  if (selection->callee->kind != W_SEED_HIR0_IDENTITY_HOST_PRELUDE ||
      !text_is(program, selection->callee->name, NATIVE_SUBSET0_CALLEE,
               sizeof(NATIVE_SUBSET0_CALLEE) - 1u) ||
      !text_is(program, selection->callee->profile, NATIVE_SUBSET0_PROFILE,
               sizeof(NATIVE_SUBSET0_PROFILE) - 1u) ||
      selection->requirement->owner_kind !=
               W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY ||
      selection->requirement->owner_index != selection->call->callee_identity ||
      !text_is(program, selection->requirement->name,
               NATIVE_SUBSET0_REQUIREMENT,
               sizeof(NATIVE_SUBSET0_REQUIREMENT) - 1u))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  selection->argument = &program->arguments[selection->call->first_argument];
  if (selection->argument->owner_call != selection->instruction->call_index ||
      selection->argument->type_index >= program->type_count ||
      program->types[selection->argument->type_index].kind !=
          W_SEED_HIR0_TYPE_STRING ||
      selection->argument->label_kind != W_SEED_HIR0_LABEL_POSITIONAL_ONLY ||
      selection->argument->label.count != 0u ||
      selection->argument->value_index >= program->value_count)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  selection->value = &program->values[selection->argument->value_index];
  if (selection->value->owner_kind != W_SEED_HIR0_VALUE_OWNER_ARGUMENT ||
      selection->value->owner_index != selection->call->first_argument ||
      selection->value->owner_ordinal != 0u ||
      selection->value->type_index >= program->type_count ||
      program->types[selection->value->type_index].kind !=
          W_SEED_HIR0_TYPE_STRING)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  if (direct_shape) {
    if (selection->value->kind != W_SEED_HIR0_VALUE_CONST_STRING ||
        selection->value->binding_index != W_SEED_HIR0_NONE ||
        selection->value->byte_count > W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD)
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
    selection->payload_bytes = selection->value->byte_count;
    selection->payload = selection->payload_bytes == 0u
                            ? NULL
                            : program->value_bytes +
                                  selection->value->byte_offset;
  } else {
    if (selection->value->kind != W_SEED_HIR0_VALUE_BINDING_READ ||
        selection->value->binding_index != 0u ||
        selection->value->byte_offset != 0u ||
        selection->value->byte_count != 0u)
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
    const w_seed_hir0_value *initializer =
        &program->values[selection->binding->initializer_value];
    selection->payload_bytes = initializer->byte_count;
    selection->payload = selection->payload_bytes == 0u
                            ? NULL
                            : program->value_bytes +
                                  initializer->byte_offset;
  }
  return W_SEED_NATIVE_SUBSET0_OK;
}

static bool sequence_stdout_add(size_t current, size_t payload_bytes,
                                size_t *next) {
  if (next == NULL || payload_bytes > SIZE_MAX - 1u) return false;
  const size_t line_bytes = payload_bytes + 1u;
  if (current > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES ||
      line_bytes > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES - current)
    return false;
  *next = current + line_bytes;
  return true;
}

static bool checked_i64_add(int64_t left, int64_t right, int64_t *result) {
  if (result == NULL || (right > 0 && left > INT64_MAX - right) ||
      (right < 0 && left < INT64_MIN - right))
    return false;
  *result = left + right;
  return true;
}

static bool checked_i64_subtract(int64_t left, int64_t right,
                                 int64_t *result) {
  if (result == NULL || (right < 0 && left > INT64_MAX + right) ||
      (right > 0 && left < INT64_MIN + right))
    return false;
  *result = left - right;
  return true;
}

static bool checked_i64_multiply(int64_t left, int64_t right,
                                 int64_t *result) {
  if (result == NULL) return false;
  if (left == 0 || right == 0) {
    *result = 0;
    return true;
  }
  if ((left == -1 && right == INT64_MIN) ||
      (right == -1 && left == INT64_MIN))
    return false;
  if ((left > 0 && right > 0 && left > INT64_MAX / right) ||
      (left > 0 && right < 0 && right < INT64_MIN / left) ||
      (left < 0 && right > 0 && left < INT64_MIN / right) ||
      (left < 0 && right < 0 && left < INT64_MAX / right))
    return false;
  *result = left * right;
  return true;
}

static bool evaluate_i64(const w_seed_hir0_program *program,
                         uint32_t value_index, size_t depth,
                         int64_t *result) {
  if (program == NULL || result == NULL || depth > 256u ||
      value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index >= program->type_count ||
      program->types[value->type_index].kind != W_SEED_HIR0_TYPE_I64)
    return false;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_I64) {
    *result = value->integer_value;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64) {
    int64_t operand = 0;
    if (value->unary_operator != W_SEED_HIR0_UNARY_NEGATE ||
        !evaluate_i64(program, value->left_value, depth + 1u, &operand) ||
        operand == INT64_MIN)
      return false;
    *result = -operand;
    return true;
  }
  if (value->kind != W_SEED_HIR0_VALUE_BINARY_I64) return false;
  int64_t left = 0;
  int64_t right = 0;
  if (!evaluate_i64(program, value->left_value, depth + 1u, &left) ||
      !evaluate_i64(program, value->right_value, depth + 1u, &right))
    return false;
  switch (value->binary_operator) {
    case W_SEED_HIR0_BINARY_ADD:
      return checked_i64_add(left, right, result);
    case W_SEED_HIR0_BINARY_SUBTRACT:
      return checked_i64_subtract(left, right, result);
    case W_SEED_HIR0_BINARY_MULTIPLY:
      return checked_i64_multiply(left, right, result);
    case W_SEED_HIR0_BINARY_DIVIDE:
      if (right == 0 || (left == INT64_MIN && right == -1)) return false;
      *result = left / right;
      return true;
    case W_SEED_HIR0_BINARY_REMAINDER:
      if (right == 0) return false;
      *result = left == INT64_MIN && right == -1 ? 0 : left % right;
      return true;
    default:
      return false;
  }
}

/* A constant arithmetic tree is still checked here so the native selector
 * cannot turn a constant overflow into a runtime program. Binding reads are
 * intentionally not considered constant by this syntactic predicate; their
 * initializers are checked independently when the binding is selected. */
static bool program_value_is_constant_i64(
    const w_seed_hir0_program *program, uint32_t value_index, size_t depth) {
  if (program == NULL || depth > 256u || value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index >= program->type_count ||
      program->types[value->type_index].kind != W_SEED_HIR0_TYPE_I64)
    return false;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_I64) return true;
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64)
    return value->unary_operator == W_SEED_HIR0_UNARY_NEGATE &&
           value->left_value != W_SEED_HIR0_NONE &&
           program_value_is_constant_i64(program, value->left_value,
                                         depth + 1u);
  if (value->kind != W_SEED_HIR0_VALUE_BINARY_I64 ||
      value->binary_operator > W_SEED_HIR0_BINARY_REMAINDER)
    return false;
  return program_value_is_constant_i64(program, value->left_value,
                                       depth + 1u) &&
         program_value_is_constant_i64(program, value->right_value,
                                       depth + 1u);
}

static bool resolve_binding_value(
    const w_seed_hir0_program *program, const w_seed_hir0_value *read,
    const w_seed_hir0_binding *const *bindings, size_t binding_count,
    size_t current_instruction, size_t *binding_reads,
    const w_seed_hir0_value **initializer) {
  if (program == NULL || read == NULL || bindings == NULL ||
      binding_reads == NULL || initializer == NULL ||
      read->kind != W_SEED_HIR0_VALUE_BINDING_READ ||
      read->binding_index == W_SEED_HIR0_NONE ||
      (size_t)read->binding_index >= binding_count ||
      read->byte_offset != 0u || read->byte_count != 0u)
    return false;
  const w_seed_hir0_binding *binding = bindings[read->binding_index];
  if (binding == NULL || binding->owner_instruction >= current_instruction ||
      binding->type_index != read->type_index ||
      binding->initializer_value >= program->value_count ||
      binding_reads[read->binding_index] == SIZE_MAX)
    return false;
  const w_seed_hir0_value *value =
      &program->values[binding->initializer_value];
  if (value->owner_kind != W_SEED_HIR0_VALUE_OWNER_BINDING ||
      value->owner_index != read->binding_index ||
      value->owner_ordinal != 0u || value->type_index != binding->type_index)
    return false;
  binding_reads[read->binding_index] += 1u;
  *initializer = value;
  return true;
}

static bool interpolation_string_bytes(
    const w_seed_hir0_program *program, const w_seed_hir0_value *value,
    const w_seed_hir0_binding *const *bindings, size_t binding_count,
    size_t current_instruction, size_t *binding_reads, size_t *bytes) {
  if (program == NULL || value == NULL || bindings == NULL ||
      binding_reads == NULL || bytes == NULL ||
      value->type_index >= program->type_count ||
      program->types[value->type_index].kind != W_SEED_HIR0_TYPE_STRING)
    return false;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_STRING) {
    if (value->binding_index != W_SEED_HIR0_NONE ||
        value->byte_count > W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD)
      return false;
    *bytes = value->byte_count;
    return true;
  }
  const w_seed_hir0_value *initializer = NULL;
  if (!resolve_binding_value(program, value, bindings, binding_count,
                             current_instruction, binding_reads,
                             &initializer) ||
      initializer->kind != W_SEED_HIR0_VALUE_CONST_STRING ||
      initializer->byte_count > W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD)
    return false;
  *bytes = initializer->byte_count;
  return true;
}

static bool program_value_lowerable(const w_seed_hir0_program *program,
                                    uint32_t value_index,
                                    uint32_t owner_function,
                                    bool allow_string, size_t depth);

static bool interpolation_maximum_bytes(
    const w_seed_hir0_program *program, const w_seed_hir0_value *value,
    const w_seed_hir0_binding *const *bindings, size_t binding_count,
    size_t current_instruction, size_t *binding_reads,
    size_t *maximum_bytes) {
  if (program == NULL || value == NULL || maximum_bytes == NULL ||
      value->kind != W_SEED_HIR0_VALUE_INTERPOLATED_STRING ||
      value->first_interpolation_segment >
          program->interpolation_segment_count ||
      value->interpolation_segment_count >
          program->interpolation_segment_count -
              value->first_interpolation_segment)
    return false;
  size_t total = 0u;
  for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
       ordinal += 1u) {
    const w_seed_hir0_interpolation_segment *segment =
        &program->interpolation_segments[value->first_interpolation_segment +
                                         ordinal];
    size_t bytes = 0u;
    if (segment->kind == W_SEED_HIR0_INTERPOLATION_TEXT) {
      bytes = segment->byte_count;
    } else if (segment->kind == W_SEED_HIR0_INTERPOLATION_VALUE &&
               segment->value_index < program->value_count) {
      const w_seed_hir0_value *embedded =
          &program->values[segment->value_index];
      if (embedded->type_index >= program->type_count) return false;
      const w_seed_hir0_value *effective = embedded;
      const bool parameter_read =
          embedded->kind == W_SEED_HIR0_VALUE_PARAMETER_READ;
      if (embedded->kind == W_SEED_HIR0_VALUE_BINDING_READ &&
          !resolve_binding_value(program, embedded, bindings, binding_count,
                                 current_instruction, binding_reads,
                                 &effective))
        return false;
      const bool runtime_value =
          parameter_read || effective->kind == W_SEED_HIR0_VALUE_PARAMETER_READ ||
          effective->kind == W_SEED_HIR0_VALUE_CALL_RESULT ||
          effective->kind == W_SEED_HIR0_VALUE_UNARY_I64 ||
          effective->kind == W_SEED_HIR0_VALUE_BINARY_I64;
      switch (program->types[embedded->type_index].kind) {
        case W_SEED_HIR0_TYPE_I64: {
          if (!runtime_value) {
            int64_t ignored = 0;
            if (!evaluate_i64(
                    program, (uint32_t)(effective - program->values), 0u,
                    &ignored))
              return false;
          } else if (!program_value_lowerable(
                         program, (uint32_t)(effective - program->values),
                         program->blocks[program->instructions[current_instruction]
                                            .owner_block]
                             .owner_function,
                         false, 0u)) {
            return false;
          }
          bytes = 20u;
          break;
        }
        case W_SEED_HIR0_TYPE_BOOL:
          if (runtime_value && !program_value_lowerable(
                  program, (uint32_t)(effective - program->values),
                  program->blocks[program->instructions[current_instruction]
                                      .owner_block].owner_function,
                  false, 0u))
            return false;
          if (!runtime_value &&
              effective->kind != W_SEED_HIR0_VALUE_CONST_BOOL)
            return false;
          bytes = runtime_value ? 5u : (effective->bool_value ? 4u : 5u);
          break;
        case W_SEED_HIR0_TYPE_STRING:
          if (embedded->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
            if (effective->kind != W_SEED_HIR0_VALUE_CONST_STRING ||
                effective->byte_count > W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD)
              return false;
            bytes = effective->byte_count;
          } else if (!interpolation_string_bytes(
                         program, embedded, bindings, binding_count,
                         current_instruction, binding_reads, &bytes)) {
            return false;
          }
          break;
        default:
          return false;
      }
    } else {
      return false;
    }
    if (total > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES ||
        bytes > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES - total)
      return false;
    total += bytes;
  }
  *maximum_bytes = total;
  return true;
}

w_seed_native_subset0_status w_seed_native_subset0_select_sequence(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_sequence *sequence) {
  if (program == NULL || hir_result == NULL || sequence == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return W_SEED_NATIVE_SUBSET0_INVALID;

  if (program->module_count != 1u || program->function_count != 1u ||
      program->parameter_count != 0u || program->block_count != 1u ||
      program->instruction_count == 0u ||
      program->instruction_count > W_SEED_NATIVE_SUBSET0_MAX_INSTRUCTIONS ||
      program->call_count == 0u ||
      program->call_count > W_SEED_NATIVE_SUBSET0_MAX_CALLS ||
      program->binding_count > W_SEED_NATIVE_SUBSET0_MAX_BINDINGS ||
      program->binding_count > program->instruction_count ||
      program->call_count > program->instruction_count ||
      program->instruction_count - program->call_count !=
          program->binding_count ||
      program->argument_count != program->call_count ||
      program->value_count == 0u ||
      program->value_count > W_SEED_NATIVE_SUBSET0_MAX_VALUES ||
      program->interpolation_segment_count >
          W_SEED_NATIVE_SUBSET0_MAX_INTERPOLATION_SEGMENTS ||
      program->requirement_count != 1u || program->terminator_count != 1u ||
      program->entry_count != 1u)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  w_seed_native_subset0_sequence candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  candidate.entry = &program->entries[0];
  if (candidate.entry->target_function >= program->function_count ||
      candidate.entry->target_name.count == 0u ||
      !text_is(program, candidate.entry->slot, NATIVE_SUBSET0_SLOT,
               sizeof(NATIVE_SUBSET0_SLOT) - 1u))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  candidate.function =
      &program->functions[candidate.entry->target_function];
  if (!text_equal(program, candidate.function->name,
                  candidate.entry->target_name) ||
      candidate.function->name.count == 0u ||
      candidate.function->parameter_count != 0u ||
      candidate.function->return_type >= program->type_count ||
      program->types[candidate.function->return_type].kind !=
          W_SEED_HIR0_TYPE_UNIT ||
      candidate.function->is_async || candidate.function->is_throws ||
      candidate.function->is_unsafe || candidate.function->has_borrow_clause ||
      candidate.function->block_count != 1u ||
      candidate.function->first_block >= program->block_count)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  candidate.block = &program->blocks[candidate.function->first_block];
  if (candidate.block->instruction_count != program->instruction_count ||
      candidate.block->first_instruction >
          program->instruction_count - candidate.block->instruction_count ||
      candidate.block->terminator_index >= program->terminator_count)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  size_t binding_cursor = 0u;
  size_t call_cursor = 0u;
  size_t binding_reads[W_SEED_NATIVE_SUBSET0_MAX_BINDINGS] = {0u};
  for (size_t ordinal = 0u; ordinal < program->instruction_count;
       ordinal += 1u) {
    const size_t instruction_index =
        (size_t)candidate.block->first_instruction + ordinal;
    const w_seed_hir0_instruction *instruction =
        &program->instructions[instruction_index];
    if (instruction->owner_block != candidate.function->first_block ||
        instruction->ordinal != ordinal || instruction->result_type >=
                                               program->type_count ||
        program->types[instruction->result_type].kind !=
            W_SEED_HIR0_TYPE_UNIT)
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (binding_cursor >= program->binding_count ||
          instruction->binding_index != binding_cursor ||
          instruction->call_index != W_SEED_HIR0_NONE)
        return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
      const w_seed_hir0_binding *binding = &program->bindings[binding_cursor];
      if (binding->owner_instruction != instruction_index ||
          binding->owner_block != candidate.function->first_block ||
          binding->ordinal != ordinal || binding->is_mutable ||
          binding->type_index >= program->type_count ||
          program->types[binding->type_index].kind ==
              W_SEED_HIR0_TYPE_UNIT ||
          binding->name.count == 0u ||
          binding->initializer_value >= program->value_count)
        return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
      const w_seed_hir0_value *initializer =
          &program->values[binding->initializer_value];
      if (initializer->owner_kind != W_SEED_HIR0_VALUE_OWNER_BINDING ||
          initializer->owner_index != binding_cursor ||
          initializer->owner_ordinal != 0u ||
          initializer->type_index != binding->type_index)
        return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
      candidate.bindings[binding_cursor] = binding;
      binding_cursor += 1u;
      continue;
    }

    if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
        call_cursor >= program->call_count ||
        instruction->call_index != call_cursor ||
        instruction->binding_index != W_SEED_HIR0_NONE)
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
    const w_seed_hir0_call *call = &program->calls[call_cursor];
    if (call->owner_instruction != instruction_index ||
        call->owner_block != candidate.function->first_block ||
        call->ordinal != ordinal || call->argument_count != 1u ||
        call->requirement_count != 1u ||
        call->callee_identity >= program->identity_count ||
        call->first_argument >= program->argument_count ||
        call->first_requirement >= program->requirement_count ||
        call->result_type >= program->type_count ||
        program->types[call->result_type].kind != W_SEED_HIR0_TYPE_UNIT)
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

    const w_seed_hir0_identity *callee =
        &program->identities[call->callee_identity];
    const w_seed_hir0_requirement *requirement =
        &program->requirements[call->first_requirement];
    if (callee->kind != W_SEED_HIR0_IDENTITY_HOST_PRELUDE ||
        !text_is(program, callee->name, NATIVE_SUBSET0_CALLEE,
                 sizeof(NATIVE_SUBSET0_CALLEE) - 1u) ||
        !text_is(program, callee->profile, NATIVE_SUBSET0_PROFILE,
                 sizeof(NATIVE_SUBSET0_PROFILE) - 1u) ||
        requirement->owner_kind != W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY ||
        requirement->owner_index != call->callee_identity ||
        !text_is(program, requirement->name, NATIVE_SUBSET0_REQUIREMENT,
                 sizeof(NATIVE_SUBSET0_REQUIREMENT) - 1u))
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

    const w_seed_hir0_argument *argument =
        &program->arguments[call->first_argument];
    if (argument->owner_call != call_cursor || argument->ordinal != 0u ||
        argument->type_index >= program->type_count ||
        program->types[argument->type_index].kind !=
            W_SEED_HIR0_TYPE_STRING ||
        argument->label_kind != W_SEED_HIR0_LABEL_POSITIONAL_ONLY ||
        argument->label.count != 0u ||
        argument->value_index >= program->value_count)
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
    const w_seed_hir0_value *value = &program->values[argument->value_index];
    if (value->owner_kind != W_SEED_HIR0_VALUE_OWNER_ARGUMENT ||
        value->owner_index != call->first_argument ||
        value->owner_ordinal != 0u ||
        value->type_index >= program->type_count ||
        program->types[value->type_index].kind != W_SEED_HIR0_TYPE_STRING)
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

    const w_seed_hir0_binding *binding = NULL;
    const uint8_t *payload = NULL;
    size_t payload_bytes = 0u;
    if (value->kind == W_SEED_HIR0_VALUE_CONST_STRING) {
      if (value->binding_index != W_SEED_HIR0_NONE ||
          value->byte_count > W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD)
        return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
      payload_bytes = value->byte_count;
      payload = payload_bytes == 0u
                    ? NULL
                    : program->value_bytes + value->byte_offset;
    } else if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
      if (value->binding_index == W_SEED_HIR0_NONE ||
          (size_t)value->binding_index >= binding_cursor ||
          value->byte_offset != 0u || value->byte_count != 0u)
        return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
      binding = candidate.bindings[value->binding_index];
      if (binding == NULL || binding->owner_instruction >= instruction_index ||
          binding->owner_block != candidate.function->first_block ||
          binding->type_index != value->type_index ||
          binding->initializer_value >= program->value_count ||
          binding_reads[value->binding_index] == SIZE_MAX)
        return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
      const w_seed_hir0_value *initializer =
          &program->values[binding->initializer_value];
      if (initializer->kind != W_SEED_HIR0_VALUE_CONST_STRING ||
          initializer->type_index != binding->type_index ||
          initializer->byte_count > W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD)
        return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
      binding_reads[value->binding_index] += 1u;
      payload_bytes = initializer->byte_count;
      payload = payload_bytes == 0u
                    ? NULL
                    : program->value_bytes + initializer->byte_offset;
    } else if (value->kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
      size_t maximum_payload_bytes = 0u;
      if (!interpolation_maximum_bytes(
              program, value, candidate.bindings, binding_cursor,
              instruction_index, binding_reads, &maximum_payload_bytes))
        return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
      size_t maximum_stdout_bytes = 0u;
      if (!sequence_stdout_add(candidate.maximum_stdout_bytes,
                               maximum_payload_bytes,
                               &maximum_stdout_bytes))
        return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
      candidate.maximum_stdout_bytes = maximum_stdout_bytes;
      candidate.has_interpolation = true;
    } else {
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
    }
    size_t stdout_bytes = 0u;
    if (!candidate.has_interpolation &&
        !sequence_stdout_add(candidate.stdout_bytes, payload_bytes,
                             &stdout_bytes))
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
    candidate.calls[call_cursor] = (w_seed_native_subset0_call_selection){
        .instruction = instruction,
        .call = call,
        .callee = callee,
        .requirement = requirement,
        .argument = argument,
        .value = value,
        .payload = payload,
        .payload_bytes = payload_bytes,
        .is_interpolated =
            value->kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING};
    if (!candidate.has_interpolation) candidate.stdout_bytes = stdout_bytes;
    if (!candidate.has_interpolation)
      candidate.maximum_stdout_bytes = candidate.stdout_bytes;
    else if (value->kind != W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
      size_t maximum_stdout_bytes = 0u;
      if (!sequence_stdout_add(candidate.maximum_stdout_bytes, payload_bytes,
                               &maximum_stdout_bytes))
        return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
      candidate.maximum_stdout_bytes = maximum_stdout_bytes;
    }
    call_cursor += 1u;
  }

  if (binding_cursor != program->binding_count ||
      call_cursor != program->call_count)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    const w_seed_hir0_value *value = &program->values[index];
    if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        value->owner_kind == W_SEED_HIR0_VALUE_OWNER_BINARY &&
        value->binding_index < binding_cursor)
      binding_reads[value->binding_index] += 1u;
  }
  for (size_t binding = 0u; binding < binding_cursor; binding += 1u)
    if (binding_reads[binding] == 0u) return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  candidate.instruction_count = program->instruction_count;
  candidate.binding_count = binding_cursor;
  candidate.call_count = call_cursor;
  *sequence = candidate;
  return W_SEED_NATIVE_SUBSET0_OK;
}

static bool program_value_lowerable(const w_seed_hir0_program *program,
                                    uint32_t value_index,
                                    uint32_t owner_function,
                                    bool allow_string, size_t depth) {
  if (program == NULL || depth > 256u ||
      value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index >= program->type_count) return false;
  const w_seed_hir0_type_kind type = program->types[value->type_index].kind;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_I64)
    return type == W_SEED_HIR0_TYPE_I64;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_BOOL)
    return type == W_SEED_HIR0_TYPE_BOOL;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_STRING)
    return allow_string && type == W_SEED_HIR0_TYPE_STRING;
  if (value->kind == W_SEED_HIR0_VALUE_PARAMETER_READ) {
    return (type == W_SEED_HIR0_TYPE_I64 || type == W_SEED_HIR0_TYPE_BOOL) &&
           value->parameter_index < program->parameter_count &&
           program->parameters[value->parameter_index].owner_function ==
               owner_function;
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_BOOL) {
    return type == W_SEED_HIR0_TYPE_BOOL &&
           value->unary_operator == W_SEED_HIR0_UNARY_NOT &&
           value->left_value != W_SEED_HIR0_NONE &&
           value->right_value == W_SEED_HIR0_NONE &&
           value->binding_index == W_SEED_HIR0_NONE &&
           value->parameter_index == W_SEED_HIR0_NONE &&
           value->call_index == W_SEED_HIR0_NONE &&
           value->first_interpolation_segment == W_SEED_HIR0_NONE &&
           value->interpolation_segment_count == 0u &&
           value->binary_operator == W_SEED_HIR0_BINARY_ADD &&
           value->block_argument_index == W_SEED_HIR0_NONE &&
           program_value_lowerable(program, value->left_value,
                                   owner_function, false, depth + 1u);
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64) {
    if (type != W_SEED_HIR0_TYPE_I64 ||
        value->unary_operator != W_SEED_HIR0_UNARY_NEGATE ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        !program_value_lowerable(program, value->left_value, owner_function,
                                 false, depth + 1u))
      return false;
    if (program_value_is_constant_i64(program, value_index, 0u)) {
      int64_t ignored = 0;
      if (!evaluate_i64(program, value_index, 0u, &ignored)) return false;
    }
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ) {
    if ((type != W_SEED_HIR0_TYPE_I64 && type != W_SEED_HIR0_TYPE_BOOL) ||
        value->block_argument_index == W_SEED_HIR0_NONE ||
        value->block_argument_index >= program->block_argument_count)
      return false;
    const w_seed_hir0_block_argument *argument =
        &program->block_arguments[value->block_argument_index];
    if (argument->owner_block >= program->block_count ||
        argument->type_index != value->type_index ||
        argument->ordinal >= program->blocks[argument->owner_block]
                                  .block_argument_count ||
        program->blocks[argument->owner_block].first_block_argument ==
            W_SEED_HIR0_NONE ||
        (size_t)program->blocks[argument->owner_block].first_block_argument +
                argument->ordinal !=
            value->block_argument_index)
      return false;
    const w_seed_hir0_block *block = &program->blocks[argument->owner_block];
    return block->owner_function == owner_function &&
           block->block_argument_count != 0u;
  }
  if (value->kind == W_SEED_HIR0_VALUE_CALL_RESULT) {
    if ((type != W_SEED_HIR0_TYPE_I64 && type != W_SEED_HIR0_TYPE_BOOL) ||
        value->call_index >= program->call_count)
      return false;
    const w_seed_hir0_call *call = &program->calls[value->call_index];
    return call->owner_block < program->block_count &&
           program->blocks[call->owner_block].owner_function ==
               owner_function &&
           call->result_type == value->type_index &&
           call->callee_identity < program->identity_count &&
           program->identities[call->callee_identity].kind ==
               W_SEED_HIR0_IDENTITY_FUNCTION;
  }
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
    if (value->binding_index >= program->binding_count) return false;
    const w_seed_hir0_binding *binding =
        &program->bindings[value->binding_index];
    if (binding->owner_block >= program->block_count ||
        program->blocks[binding->owner_block].owner_function !=
            owner_function ||
        binding->initializer_value >= program->value_count)
      return false;
    return program_value_lowerable(program, binding->initializer_value,
                                   owner_function, allow_string, depth + 1u);
  }
  if (value->kind == W_SEED_HIR0_VALUE_BINARY_I64) {
    if (value->left_value >= program->value_count ||
        value->right_value >= program->value_count ||
        program->values[value->left_value].type_index >= program->type_count ||
        program->values[value->right_value].type_index >= program->type_count ||
        program->types[program->values[value->left_value].type_index].kind !=
            W_SEED_HIR0_TYPE_I64 ||
        program->types[program->values[value->right_value].type_index].kind !=
            W_SEED_HIR0_TYPE_I64)
      return false;
    if (value->binary_operator >= W_SEED_HIR0_BINARY_EQUAL &&
        value->binary_operator <= W_SEED_HIR0_BINARY_GREATER_EQUAL)
      return type == W_SEED_HIR0_TYPE_BOOL &&
             program_value_lowerable(program, value->left_value,
                                      owner_function, false, depth + 1u) &&
             program_value_lowerable(program, value->right_value,
                                      owner_function, false, depth + 1u);
    if (value->binary_operator > W_SEED_HIR0_BINARY_REMAINDER ||
        type != W_SEED_HIR0_TYPE_I64 ||
        !program_value_lowerable(program, value->left_value, owner_function,
                                 false, depth + 1u) ||
        !program_value_lowerable(program, value->right_value, owner_function,
                                 false, depth + 1u))
      return false;
    if (program_value_is_constant_i64(program, value_index, 0u)) {
      int64_t ignored = 0;
      if (!evaluate_i64(program, value_index, 0u, &ignored)) return false;
    }
    return true;
  }
  return false;
}

/* HIR0 verification proves each branch is a structured diamond. Scalar
 * functions may use the same forward-only value-if diamonds, including a
 * nested diamond in either arm. Logical diamonds keep their existing Bool-only
 * shape. Keep this local structural proof because the maximum-output walk
 * below is deliberately a less precise reverse dynamic program. */
static bool program_scalar_cfg_arm(
    const w_seed_hir0_program *program, uint32_t function_index, size_t start,
    size_t end, size_t depth, bool require_empty, size_t *last_block,
    size_t *join_block);

static bool program_scalar_cfg_branch(
    const w_seed_hir0_program *program, uint32_t function_index,
    size_t branch_block, size_t end, size_t depth, size_t *join_block);

static bool program_scalar_cfg_join(
    const w_seed_hir0_program *program, const w_seed_hir0_terminator *branch,
    size_t join_block, uint32_t expected_type) {
  if (program == NULL || branch == NULL || join_block >= program->block_count ||
      (expected_type != W_SEED_HIR0_TYPE_I64 &&
       expected_type != W_SEED_HIR0_TYPE_BOOL))
    return false;
  const w_seed_hir0_block *join = &program->blocks[join_block];
  if (join->block_argument_count != 1u ||
      join->first_block_argument == W_SEED_HIR0_NONE ||
      join->first_block_argument >= program->block_argument_count)
    return false;
  const w_seed_hir0_block_argument *argument =
      &program->block_arguments[join->first_block_argument];
  return argument->owner_block == join_block && argument->ordinal == 0u &&
         argument->type_index == expected_type;
}

static bool program_scalar_cfg_scalar_jump(
    const w_seed_hir0_program *program, const w_seed_hir0_terminator *branch,
    size_t jump_block, size_t join_block) {
  if (program == NULL || branch == NULL || jump_block >= program->block_count ||
      join_block >= program->block_count ||
      branch->logical_operator != W_SEED_HIR0_LOGICAL_NONE)
    return false;
  uint32_t expected_type = branch->result_type;
  if (expected_type == 0u) {
    const w_seed_hir0_block *join = &program->blocks[join_block];
    if (join->block_argument_count != 1u ||
        join->first_block_argument == W_SEED_HIR0_NONE ||
        join->first_block_argument >= program->block_argument_count)
      return false;
    expected_type =
        program->block_arguments[join->first_block_argument].type_index;
  }
  if (expected_type != W_SEED_HIR0_TYPE_I64 &&
      expected_type != W_SEED_HIR0_TYPE_BOOL)
    return false;
  const w_seed_hir0_terminator *jump = &program->terminators[jump_block];
  if (jump->owner_block != jump_block ||
      jump->kind != W_SEED_HIR0_TERMINATOR_JUMP ||
      jump->target_block != join_block ||
      jump->else_block != W_SEED_HIR0_NONE ||
      jump->value_index != W_SEED_HIR0_NONE || jump->result_type != 0u ||
      jump->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      jump->edge_argument_count != 1u ||
      jump->first_edge_argument == W_SEED_HIR0_NONE ||
      jump->first_edge_argument >= program->edge_argument_count)
    return false;
  const w_seed_hir0_edge_argument *edge =
      &program->edge_arguments[jump->first_edge_argument];
  return edge->owner_terminator == jump_block && edge->ordinal == 0u &&
         edge->value_index < program->value_count &&
         edge->type_index == expected_type &&
         program->values[edge->value_index].type_index == expected_type;
}

static bool program_scalar_cfg_unit_join(
    const w_seed_hir0_program *program, size_t join_block) {
  if (program == NULL || join_block >= program->block_count) return false;
  const w_seed_hir0_block *join = &program->blocks[join_block];
  if (join->block_argument_count == 0u ||
      join->first_block_argument == W_SEED_HIR0_NONE ||
      join->block_argument_count > program->block_argument_count ||
      (size_t)join->first_block_argument >
          program->block_argument_count - join->block_argument_count)
    return false;
  for (size_t ordinal = 0u; ordinal < join->block_argument_count;
       ordinal += 1u) {
    const w_seed_hir0_block_argument *argument =
        &program->block_arguments[(size_t)join->first_block_argument + ordinal];
    if (argument->owner_block != join_block || argument->ordinal != ordinal ||
        (argument->type_index != W_SEED_HIR0_TYPE_I64 &&
         argument->type_index != W_SEED_HIR0_TYPE_BOOL))
      return false;
  }
  return true;
}

static bool program_scalar_cfg_unit_jump(
    const w_seed_hir0_program *program, const w_seed_hir0_terminator *branch,
    size_t jump_block, size_t join_block) {
  if (program == NULL || branch == NULL || jump_block >= program->block_count ||
      join_block >= program->block_count ||
      branch->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      branch->result_type != 0u || !program_scalar_cfg_unit_join(program, join_block))
    return false;
  const w_seed_hir0_block *join = &program->blocks[join_block];
  const w_seed_hir0_terminator *jump = &program->terminators[jump_block];
  if (jump->owner_block != jump_block ||
      jump->kind != W_SEED_HIR0_TERMINATOR_JUMP ||
      jump->target_block != join_block ||
      jump->else_block != W_SEED_HIR0_NONE ||
      jump->value_index != W_SEED_HIR0_NONE || jump->result_type != 0u ||
      jump->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      jump->edge_argument_count != join->block_argument_count ||
      jump->first_edge_argument == W_SEED_HIR0_NONE ||
      jump->edge_argument_count > program->edge_argument_count ||
      (size_t)jump->first_edge_argument >
          program->edge_argument_count - jump->edge_argument_count)
    return false;
  for (size_t ordinal = 0u; ordinal < jump->edge_argument_count;
       ordinal += 1u) {
    const w_seed_hir0_edge_argument *edge =
        &program->edge_arguments[(size_t)jump->first_edge_argument + ordinal];
    const w_seed_hir0_block_argument *argument =
        &program->block_arguments[(size_t)join->first_block_argument + ordinal];
    if (edge->owner_terminator != jump_block || edge->ordinal != ordinal ||
        edge->value_index >= program->value_count ||
        edge->type_index != argument->type_index ||
        program->values[edge->value_index].type_index != edge->type_index)
      return false;
  }
  return true;
}

static bool program_scalar_cfg_logical_jump(
    const w_seed_hir0_program *program, const w_seed_hir0_terminator *branch,
    size_t jump_block, size_t join_block) {
  if (program == NULL || branch == NULL || jump_block >= program->block_count ||
      join_block >= program->block_count ||
      (branch->logical_operator != W_SEED_HIR0_LOGICAL_AND &&
       branch->logical_operator != W_SEED_HIR0_LOGICAL_OR) ||
      branch->result_type != W_SEED_HIR0_TYPE_BOOL)
    return false;
  const w_seed_hir0_terminator *jump = &program->terminators[jump_block];
  if (jump->owner_block != jump_block ||
      jump->kind != W_SEED_HIR0_TERMINATOR_JUMP ||
      jump->target_block != join_block ||
      jump->else_block != W_SEED_HIR0_NONE ||
      jump->value_index != W_SEED_HIR0_NONE || jump->result_type != 0u ||
      jump->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      jump->edge_argument_count != 1u ||
      jump->first_edge_argument == W_SEED_HIR0_NONE ||
      jump->first_edge_argument >= program->edge_argument_count)
    return false;
  const w_seed_hir0_edge_argument *edge =
      &program->edge_arguments[jump->first_edge_argument];
  return edge->owner_terminator == jump_block && edge->ordinal == 0u &&
         edge->value_index < program->value_count && edge->type_index ==
             W_SEED_HIR0_TYPE_BOOL &&
         program->values[edge->value_index].type_index ==
             W_SEED_HIR0_TYPE_BOOL;
}

static bool program_scalar_cfg_branch(
    const w_seed_hir0_program *program, uint32_t function_index,
    size_t branch_block, size_t end, size_t depth, size_t *join_block) {
  if (program == NULL || join_block == NULL ||
      depth >= W_SEED_HIR0_MAX_NESTING || branch_block >= end ||
      branch_block + 1u >= end || branch_block >= program->block_count)
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
  const bool scalar_arms =
      branch->logical_operator == W_SEED_HIR0_LOGICAL_NONE;
  if (!program_scalar_cfg_arm(program, function_index,
                              branch->target_block, end, depth + 1u,
                              scalar_arms, &then_last, &then_join) ||
      then_last + 1u != branch->else_block)
    return false;
  size_t else_last = 0u;
  size_t else_join = 0u;
  if (!program_scalar_cfg_arm(program, function_index, branch->else_block,
                              end, depth + 1u, scalar_arms, &else_last,
                              &else_join) ||
      else_join != then_join || then_join != else_last + 1u ||
      then_join >= end || then_join <= branch_block)
    return false;

  if (branch->value_index == W_SEED_HIR0_NONE ||
      branch->value_index >= program->value_count ||
      program->values[branch->value_index].type_index !=
          W_SEED_HIR0_TYPE_BOOL)
    return false;
  if (branch->logical_operator == W_SEED_HIR0_LOGICAL_NONE) {
    uint32_t expected_type = branch->result_type;
    if (expected_type == 0u) {
      const w_seed_hir0_block *join = &program->blocks[then_join];
      if (join->block_argument_count == 0u ||
          join->first_block_argument == W_SEED_HIR0_NONE ||
          join->first_block_argument >= program->block_argument_count)
        return false;
      if (join->block_argument_count != 1u) {
        if (!program_scalar_cfg_unit_jump(program, branch, then_last,
                                          then_join) ||
            !program_scalar_cfg_unit_jump(program, branch, else_last,
                                          else_join))
          return false;
        *join_block = then_join;
        return true;
      }
      expected_type =
          program->block_arguments[join->first_block_argument].type_index;
    }
    if ((expected_type != W_SEED_HIR0_TYPE_I64 &&
         expected_type != W_SEED_HIR0_TYPE_BOOL) ||
        !program_scalar_cfg_scalar_jump(program, branch, then_last,
                                        then_join) ||
        !program_scalar_cfg_scalar_jump(program, branch, else_last,
                                        else_join) ||
        !program_scalar_cfg_join(program, branch, then_join,
                                 expected_type))
      return false;
  } else if ((branch->logical_operator != W_SEED_HIR0_LOGICAL_AND &&
              branch->logical_operator != W_SEED_HIR0_LOGICAL_OR) ||
             branch->result_type != W_SEED_HIR0_TYPE_BOOL ||
             !program_scalar_cfg_logical_jump(program, branch, then_last,
                                              then_join) ||
             !program_scalar_cfg_logical_jump(program, branch, else_last,
                                              else_join) ||
             !program_scalar_cfg_join(program, branch, then_join,
                                      W_SEED_HIR0_TYPE_BOOL))
    return false;
  *join_block = then_join;
  return true;
}

static bool program_scalar_cfg_arm(
    const w_seed_hir0_program *program, uint32_t function_index, size_t start,
    size_t end, size_t depth, bool require_empty, size_t *last_block,
    size_t *join_block) {
  if (program == NULL || last_block == NULL || join_block == NULL ||
      start >= end || end > program->block_count ||
      depth > W_SEED_HIR0_MAX_NESTING)
    return false;
  size_t current = start;
  size_t guard = 0u;
  while (current < end && guard < end - start) {
    if (program->blocks[current].owner_function != function_index ||
        (require_empty && program->blocks[current].instruction_count != 0u) ||
        program->blocks[current].terminator_index >=
            program->terminator_count)
      return false;
    const w_seed_hir0_terminator *term =
        &program->terminators[program->blocks[current].terminator_index];
    if (term->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
      size_t nested_join = 0u;
      if (!program_scalar_cfg_branch(program, function_index, current, end,
                                     depth, &nested_join) ||
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

static bool program_scalar_cfg_is_supported(
    const w_seed_hir0_program *program, size_t function_index) {
  if (program == NULL || function_index >= program->function_count) return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->block_count <= 1u) return true;
  if (function->first_block >= program->block_count ||
      function->block_count > program->block_count - function->first_block)
    return false;
  const size_t start = function->first_block;
  const size_t end = start + function->block_count;
  size_t current = start;
  size_t guard = 0u;
  bool has_branch = false;
  while (current < end && guard < function->block_count) {
    if (program->blocks[current].owner_function != function_index ||
        program->blocks[current].terminator_index >=
            program->terminator_count)
      return false;
    const w_seed_hir0_terminator *term =
        &program->terminators[program->blocks[current].terminator_index];
    if (term->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
      size_t join = 0u;
      if (!program_scalar_cfg_branch(program, (uint32_t)function_index,
                                     current, end, 0u, &join) ||
          join <= current || join >= end)
        return false;
      current = join;
      has_branch = true;
      guard += 1u;
      continue;
    }
    if (term->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        current + 1u == end)
      return has_branch;
    return false;
  }
  return false;
}

static bool program_host_print_maximum(
    const w_seed_hir0_program *program, const w_seed_hir0_call *call,
    const w_seed_hir0_binding *const *bindings, size_t *binding_reads,
    size_t *maximum, bool *has_interpolation) {
  if (program == NULL || call == NULL || bindings == NULL ||
      binding_reads == NULL || maximum == NULL || has_interpolation == NULL ||
      call->callee_identity >= program->identity_count ||
      call->owner_instruction >= program->instruction_count ||
      call->argument_count != 1u || call->first_argument >= program->argument_count ||
      call->requirement_count != 1u ||
      call->first_requirement >= program->requirement_count)
    return false;
  const w_seed_hir0_identity *callee =
      &program->identities[call->callee_identity];
  const w_seed_hir0_requirement *requirement =
      &program->requirements[call->first_requirement];
  const w_seed_hir0_argument *argument =
      &program->arguments[call->first_argument];
  if (callee->kind != W_SEED_HIR0_IDENTITY_HOST_PRELUDE ||
      !text_is(program, callee->name, NATIVE_SUBSET0_CALLEE,
               sizeof(NATIVE_SUBSET0_CALLEE) - 1u) ||
      !text_is(program, callee->profile, NATIVE_SUBSET0_PROFILE,
               sizeof(NATIVE_SUBSET0_PROFILE) - 1u) ||
      requirement->owner_kind != W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY ||
      requirement->owner_index != call->callee_identity ||
      !text_is(program, requirement->name, NATIVE_SUBSET0_REQUIREMENT,
               sizeof(NATIVE_SUBSET0_REQUIREMENT) - 1u) ||
      argument->parameter_ordinal != 0u ||
      argument->type_index >= program->type_count ||
      program->types[argument->type_index].kind != W_SEED_HIR0_TYPE_STRING ||
      argument->value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *root = &program->values[argument->value_index];
  size_t payload = 0u;
  if (root->kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
    if (!interpolation_maximum_bytes(
            program, root, bindings, program->binding_count,
            call->owner_instruction, binding_reads, &payload))
      return false;
    *has_interpolation = true;
  } else if (!interpolation_string_bytes(
                 program, root, bindings, program->binding_count,
                 call->owner_instruction, binding_reads, &payload)) {
    return false;
  }
  return sequence_stdout_add(0u, payload, maximum);
}


/* HIR0 verification proves that each function has a dense, forward-only
 * block order. Reverse topological dynamic programming computes each block
 * exactly once. A shared continuation is therefore read from the cache once,
 * while a branch combines mutually-exclusive arm maxima with max(). */
static bool program_function_maximum(
    const w_seed_hir0_program *program, size_t function_index,
    const w_seed_hir0_binding *const *bindings, size_t *binding_reads,
    uint8_t *state, size_t *cached, bool *has_interpolation,
    bool *has_local_calls) {
  if (program == NULL || bindings == NULL || binding_reads == NULL ||
      state == NULL || cached == NULL || has_interpolation == NULL ||
      has_local_calls == NULL || function_index >= program->function_count)
    return false;
  if (state[function_index] == 2u) return true;
  /* Preserve call-cycle detection for the function graph. */
  if (state[function_index] == 1u) return false;
  state[function_index] = 1u;

  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->return_type >= program->type_count ||
      (program->types[function->return_type].kind != W_SEED_HIR0_TYPE_UNIT &&
       program->types[function->return_type].kind != W_SEED_HIR0_TYPE_I64 &&
       program->types[function->return_type].kind != W_SEED_HIR0_TYPE_BOOL) ||
      function->is_async || function->is_throws || function->is_unsafe ||
      function->has_borrow_clause || function->block_count == 0u ||
      function->block_count > W_SEED_NATIVE_SUBSET0_MAX_BLOCKS ||
      function->first_block >= program->block_count ||
      function->block_count > program->block_count - function->first_block)
    return false;
  if (program->types[function->return_type].kind != W_SEED_HIR0_TYPE_UNIT &&
      function->block_count > 1u &&
      !program_scalar_cfg_is_supported(program, function_index))
    return false;
  for (size_t parameter = 0u; parameter < function->parameter_count;
       parameter += 1u) {
    const size_t parameter_index =
        (size_t)function->first_parameter + parameter;
    if (parameter_index >= program->parameter_count) return false;
    const uint32_t type_index = program->parameters[parameter_index].type_index;
    if (type_index >= program->type_count ||
        (program->types[type_index].kind != W_SEED_HIR0_TYPE_I64 &&
         program->types[type_index].kind != W_SEED_HIR0_TYPE_BOOL))
      return false;
  }

  const size_t start = function->first_block;
  const size_t end = start + function->block_count;
  size_t block_maximum[W_SEED_NATIVE_SUBSET0_MAX_BLOCKS] = {0u};
  for (size_t offset = function->block_count; offset != 0u; offset -= 1u) {
    const size_t local_block = offset - 1u;
    const size_t block_index = start + local_block;
    const w_seed_hir0_block *block = &program->blocks[block_index];
    if (block->owner_function != function_index ||
        block->terminator_index >= program->terminator_count ||
        (size_t)block->first_instruction > program->instruction_count ||
        block->instruction_count >
            program->instruction_count - block->first_instruction)
      return false;

    size_t total = 0u;
    for (size_t ordinal = 0u; ordinal < block->instruction_count;
         ordinal += 1u) {
      const size_t instruction_index =
          (size_t)block->first_instruction + ordinal;
      const w_seed_hir0_instruction *instruction =
          &program->instructions[instruction_index];
      if (instruction->owner_block != block_index ||
          instruction->ordinal != ordinal)
        return false;
      if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
        if (instruction->binding_index >= program->binding_count ||
            bindings[instruction->binding_index] == NULL ||
            !program_value_lowerable(
                program, program->bindings[instruction->binding_index]
                           .initializer_value,
                (uint32_t)function_index, true, 0u))
          return false;
        continue;
      }
      if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
          instruction->call_index >= program->call_count)
        return false;
      const w_seed_hir0_call *call = &program->calls[instruction->call_index];
      if (call->owner_block != block_index ||
          call->owner_instruction != instruction_index ||
          call->callee_identity >= program->identity_count ||
          call->first_argument > program->argument_count ||
          call->argument_count >
              program->argument_count - call->first_argument)
        return false;
      const w_seed_hir0_identity *callee =
          &program->identities[call->callee_identity];
      size_t addition = 0u;
      if (callee->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE) {
        if (!program_host_print_maximum(program, call, bindings, binding_reads,
                                        &addition, has_interpolation))
          return false;
      } else if (callee->kind == W_SEED_HIR0_IDENTITY_FUNCTION) {
        if (callee->target_index >= program->function_count ||
            call->argument_count != callee->parameter_count)
          return false;
        for (size_t argument = 0u; argument < call->argument_count;
             argument += 1u) {
          const w_seed_hir0_argument *item =
              &program->arguments[(size_t)call->first_argument + argument];
          if (!program_value_lowerable(program, item->value_index,
                                       (uint32_t)function_index, false, 0u))
            return false;
        }
        if (!program_function_maximum(
                program, callee->target_index, bindings, binding_reads, state,
                cached, has_interpolation, has_local_calls))
          return false;
        addition = cached[callee->target_index];
        *has_local_calls = true;
      } else {
        return false;
      }
      if (total > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES ||
          addition > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES - total)
        return false;
      total += addition;
    }

    const w_seed_hir0_terminator *terminator =
        &program->terminators[block->terminator_index];
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT) {
      if (function->return_type != 0u ||
          terminator->target_block != W_SEED_HIR0_NONE ||
          terminator->else_block != W_SEED_HIR0_NONE)
        return false;
      block_maximum[local_block] = total;
      continue;
    }
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE) {
      if (function->return_type == 0u ||
          (terminator->value_index < program->value_count &&
           program->values[terminator->value_index].kind ==
               W_SEED_HIR0_VALUE_CALL_RESULT) ||
          !program_value_lowerable(program, terminator->value_index,
                                    (uint32_t)function_index, false, 0u)) {
        return false;
      }
      block_maximum[local_block] = total;
      continue;
    }
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_JUMP) {
      if (terminator->target_block == W_SEED_HIR0_NONE ||
          terminator->target_block < start ||
          terminator->target_block >= end ||
          terminator->target_block <= block_index ||
          terminator->else_block != W_SEED_HIR0_NONE)
        return false;
      const size_t continuation =
          block_maximum[(size_t)terminator->target_block - start];
      if (continuation > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES ||
          total > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES - continuation)
        return false;
      block_maximum[local_block] = total + continuation;
      continue;
    }
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
      if (terminator->target_block == W_SEED_HIR0_NONE ||
          terminator->else_block == W_SEED_HIR0_NONE ||
          terminator->target_block < start ||
          terminator->target_block >= end ||
          terminator->else_block < start ||
          terminator->else_block >= end ||
          terminator->target_block <= block_index ||
          terminator->else_block <= block_index ||
          terminator->value_index == W_SEED_HIR0_NONE ||
          terminator->value_index >= program->value_count ||
          program->values[terminator->value_index].type_index >=
              program->type_count ||
          program->types[program->values[terminator->value_index].type_index]
                  .kind != W_SEED_HIR0_TYPE_BOOL ||
          !program_value_lowerable(program, terminator->value_index,
                                   (uint32_t)function_index, false, 0u))
        return false;
      const size_t then_maximum =
          block_maximum[(size_t)terminator->target_block - start];
      const size_t else_maximum =
          block_maximum[(size_t)terminator->else_block - start];
      const size_t branch_maximum =
          then_maximum > else_maximum ? then_maximum : else_maximum;
      if (branch_maximum > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES ||
          total > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES - branch_maximum)
        return false;
      block_maximum[local_block] = total + branch_maximum;
      continue;
    }
    return false;
  }

  cached[function_index] = block_maximum[0u];
  state[function_index] = 2u;
  return true;
}

w_seed_native_subset0_status w_seed_native_subset0_select_program(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_program *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return W_SEED_NATIVE_SUBSET0_INVALID;
  if (program->module_count != 1u || program->function_count == 0u ||
      program->function_count > W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS ||
      program->parameter_count > W_SEED_NATIVE_SUBSET0_MAX_PARAMETERS ||
      program->block_count == 0u ||
      program->block_count > W_SEED_NATIVE_SUBSET0_MAX_BLOCKS ||
      program->instruction_count == 0u ||
      program->instruction_count > W_SEED_NATIVE_SUBSET0_MAX_INSTRUCTIONS ||
      program->call_count == 0u ||
      program->call_count > W_SEED_NATIVE_SUBSET0_MAX_CALLS ||
      program->binding_count > W_SEED_NATIVE_SUBSET0_MAX_BINDINGS ||
      program->value_count == 0u ||
      program->value_count > W_SEED_NATIVE_SUBSET0_MAX_VALUES ||
      program->interpolation_segment_count >
          W_SEED_NATIVE_SUBSET0_MAX_INTERPOLATION_SEGMENTS ||
      program->terminator_count != program->block_count ||
      program->entry_count != 1u)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  const w_seed_hir0_entry *entry = &program->entries[0];
  if (entry->target_function >= program->function_count ||
      program->functions[entry->target_function].parameter_count != 0u ||
      program->functions[entry->target_function].return_type >=
          program->type_count ||
      program->types[program->functions[entry->target_function].return_type]
              .kind != W_SEED_HIR0_TYPE_UNIT ||
      !text_is(program, entry->slot, NATIVE_SUBSET0_SLOT,
               sizeof(NATIVE_SUBSET0_SLOT) - 1u))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  const w_seed_hir0_binding *bindings[W_SEED_NATIVE_SUBSET0_MAX_BINDINGS] =
      {NULL};
  size_t binding_reads[W_SEED_NATIVE_SUBSET0_MAX_BINDINGS] = {0u};
  for (size_t index = 0u; index < program->binding_count; index += 1u)
    bindings[index] = &program->bindings[index];
  uint8_t state[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS] = {0u};
  size_t cached[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS] = {0u};
  bool has_interpolation = false;
  bool has_local_calls = false;
  for (size_t function = 0u; function < program->function_count;
       function += 1u)
    if (!program_function_maximum(
            program, function, bindings, binding_reads, state, cached,
            &has_interpolation, &has_local_calls))
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  if (cached[entry->target_function] == 0u)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  bool has_cfg = false;
  for (size_t function = 0u; function < program->function_count;
       function += 1u)
    if (program->functions[function].block_count > 1u) has_cfg = true;
  bool has_bool = false;
  for (size_t value = 0u; value < program->value_count; value += 1u)
    if (program->values[value].type_index < program->type_count &&
        program->types[program->values[value].type_index].kind ==
            W_SEED_HIR0_TYPE_BOOL)
      has_bool = true;
  bool has_mutable_bindings = false;
  for (size_t binding = 0u; binding < program->binding_count; binding += 1u)
    if (program->bindings[binding].is_mutable ||
        program->bindings[binding].source_binding != binding) {
      has_mutable_bindings = true;
      break;
    }
  *selection = (w_seed_native_subset0_program){
      .entry = entry,
      .function_count = program->function_count,
      .parameter_count = program->parameter_count,
      .instruction_count = program->instruction_count,
      .binding_count = program->binding_count,
      .call_count = program->call_count,
      .maximum_stdout_bytes = cached[entry->target_function],
      .has_interpolation = has_interpolation,
      .has_bool = has_bool,
      .has_local_calls = has_local_calls,
      .has_cfg = has_cfg,
      .has_mutable_bindings = has_mutable_bindings};
  return W_SEED_NATIVE_SUBSET0_OK;
}

w_seed_native_subset0_status w_seed_native_subset0_select_process(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_process *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return W_SEED_NATIVE_SUBSET0_INVALID;

  /* HIR verification has already rederived the external identity, lifecycle,
   * direct-entry, and complete-body facts. Keep this selector conservative and
   * require the exact HIR16 process handler shape before emitting an ABI. */
  if (program->module_count != 1u || program->external_module_count != 1u ||
      program->external_symbol_count != 4u || program->function_count != 1u ||
      program->parameter_count != 2u || program->block_count != 1u ||
      program->instruction_count != 0u || program->binding_count != 0u ||
      program->call_count != 0u || program->argument_count != 0u ||
      program->value_count != 1u ||
      program->interpolation_segment_count != 0u ||
      program->terminator_count != 1u || program->entry_count != 1u)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  const w_seed_hir0_external_module *external_module =
      &program->external_modules[0];
  if (external_module->module_index != 0u ||
      external_module->first_symbol != 0u ||
      external_module->symbol_count != 4u ||
      !text_is(program, external_module->module_id,
               (const uint8_t *)"std.process", 11u))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  size_t arguments_symbol = SIZE_MAX;
  size_t context_symbol = SIZE_MAX;
  size_t exit_code_symbol = SIZE_MAX;
  size_t success_symbol = SIZE_MAX;
  for (size_t symbol_index = 0u;
       symbol_index < program->external_symbol_count; symbol_index += 1u) {
    const w_seed_hir0_external_symbol *symbol =
        &program->external_symbols[symbol_index];
    if (symbol->module_index != 0u || symbol->parameter_count != 0u)
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
    if (symbol->kind == W_SEED_HIR0_EXTERNAL_TYPE &&
        text_is(program, symbol->name, (const uint8_t *)"Arguments", 9u))
      arguments_symbol = symbol_index;
    else if (symbol->kind == W_SEED_HIR0_EXTERNAL_TYPE &&
             text_is(program, symbol->name, (const uint8_t *)"Context", 7u))
      context_symbol = symbol_index;
    else if (symbol->kind == W_SEED_HIR0_EXTERNAL_TYPE &&
             text_is(program, symbol->name, (const uint8_t *)"ExitCode", 8u))
      exit_code_symbol = symbol_index;
    else if (symbol->kind == W_SEED_HIR0_EXTERNAL_VALUE && symbol->is_const &&
             text_is(program, symbol->name, (const uint8_t *)"success", 7u))
      success_symbol = symbol_index;
  }
  if (arguments_symbol == SIZE_MAX || context_symbol == SIZE_MAX ||
      exit_code_symbol == SIZE_MAX || success_symbol == SIZE_MAX)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  const w_seed_hir0_entry *entry = &program->entries[0];
  if (entry->target_function >= program->function_count ||
      entry->adapter_kind != W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS ||
      entry->cleanup_obligation !=
          W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS ||
      entry->cleanup_owner_parameter_count != 2u)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  const w_seed_hir0_function *function =
      &program->functions[entry->target_function];
  if (function->first_parameter >= program->parameter_count ||
      function->parameter_count != 2u ||
      function->first_parameter >
          program->parameter_count - function->parameter_count ||
      !function->is_async || function->is_const || function->is_throws ||
      function->is_unsafe || function->has_borrow_clause ||
      function->is_anonymous_entry ||
      function->suspension != W_SEED_HIR0_SUSPENSION_MAY ||
      function->direct_entry != W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE ||
      function->return_type >= program->type_count)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  const w_seed_hir0_type *return_type = &program->types[function->return_type];
  if (return_type->kind != W_SEED_HIR0_TYPE_NOMINAL ||
      return_type->external_module_index != 0u ||
      return_type->external_symbol_index != (uint32_t)exit_code_symbol ||
      return_type->lifecycle != W_SEED_HIR0_LIFECYCLE_VALUE_COPY ||
      return_type->release_contract != W_SEED_HIR0_RELEASE_CONTRACT_NONE)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  const uint32_t first_parameter = function->first_parameter;
  const w_seed_hir0_parameter *arguments_parameter =
      &program->parameters[first_parameter];
  const w_seed_hir0_parameter *context_parameter =
      &program->parameters[(size_t)first_parameter + 1u];
  if (entry->first_cleanup_owner_parameter != first_parameter ||
      arguments_parameter->owner_function != entry->target_function ||
      context_parameter->owner_function != entry->target_function ||
      arguments_parameter->ordinal != 0u || context_parameter->ordinal != 1u)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  const w_seed_hir0_type *arguments_type =
      &program->types[arguments_parameter->type_index];
  const w_seed_hir0_type *context_type =
      &program->types[context_parameter->type_index];
  if (arguments_type->kind != W_SEED_HIR0_TYPE_NOMINAL ||
      arguments_type->external_module_index != 0u ||
      arguments_type->external_symbol_index != (uint32_t)arguments_symbol ||
      arguments_type->lifecycle != W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER ||
      arguments_type->release_contract !=
          W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE ||
      context_type->kind != W_SEED_HIR0_TYPE_NOMINAL ||
      context_type->external_module_index != 0u ||
      context_type->external_symbol_index != (uint32_t)context_symbol ||
      context_type->lifecycle != W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER ||
      context_type->release_contract !=
          W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  (void)memset(selection, 0, sizeof(*selection));
  selection->entry = entry;
  selection->function = function;
  selection->arguments_parameter = arguments_parameter;
  selection->context_parameter = context_parameter;
  selection->arguments_parameter_ordinal = 0u;
  selection->context_parameter_ordinal = 1u;
  return W_SEED_NATIVE_SUBSET0_OK;
}

w_seed_native_subset0_status
w_seed_native_subset0_select_process_executable(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_process *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return W_SEED_NATIVE_SUBSET0_INVALID;

  /* HIR verification has already rederived the exact public process-input
   * witness. Keep this consumer boundary explicit so a private four-symbol
   * handler cannot be emitted as a public executable by accident. */
  if (program->module_count != 1u || program->external_module_count != 1u ||
      program->external_symbol_count != 6u || program->function_count != 1u ||
      program->parameter_count != 2u || program->block_count != 3u ||
      program->instruction_count != 2u || program->binding_count != 0u ||
      program->call_count != 2u || program->argument_count != 2u ||
      program->value_count != 7u || program->terminator_count != 3u ||
      program->entry_count != 1u)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  const w_seed_hir0_entry *entry = &program->entries[0];
  if (entry->target_function != 0u ||
      entry->adapter_kind != W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS ||
      entry->cleanup_obligation !=
          W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS ||
      entry->first_cleanup_owner_parameter != 0u ||
      entry->cleanup_owner_parameter_count != 2u)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  const w_seed_hir0_function *function = &program->functions[0];
  if (function->first_parameter != 0u || function->parameter_count != 2u ||
      function->first_block != 0u || function->block_count != 3u ||
      !function->is_async || function->is_const || function->is_throws ||
      function->is_unsafe || function->has_borrow_clause ||
      function->is_anonymous_entry ||
      function->suspension != W_SEED_HIR0_SUSPENSION_MAY ||
      function->direct_entry != W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  const w_seed_hir0_parameter *arguments_parameter = &program->parameters[0];
  const w_seed_hir0_parameter *context_parameter = &program->parameters[1];
  if (arguments_parameter->owner_function != 0u ||
      arguments_parameter->ordinal != 0u ||
      context_parameter->owner_function != 0u ||
      context_parameter->ordinal != 1u)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  (void)memset(selection, 0, sizeof(*selection));
  selection->entry = entry;
  selection->function = function;
  selection->arguments_parameter = arguments_parameter;
  selection->context_parameter = context_parameter;
  selection->arguments_parameter_ordinal = 0u;
  selection->context_parameter_ordinal = 1u;
  return W_SEED_NATIVE_SUBSET0_OK;
}
