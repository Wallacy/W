#include "w_seed_native_subset0.h"

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
      program->requirement_count != 1u || program->value_count != 1u ||
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
        selection->binding->byte_count > W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD)
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
    selection->payload_bytes = selection->binding->byte_count;
    selection->payload = selection->payload_bytes == 0u
                            ? NULL
                            : program->value_bytes +
                                  selection->binding->byte_offset;
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
      program->value_count != program->call_count ||
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
          program->types[binding->type_index].kind !=
              W_SEED_HIR0_TYPE_STRING ||
          binding->name.count == 0u ||
          binding->byte_count > W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD)
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
          binding_reads[value->binding_index] == SIZE_MAX)
        return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
      binding_reads[value->binding_index] += 1u;
      payload_bytes = binding->byte_count;
      payload = payload_bytes == 0u
                    ? NULL
                    : program->value_bytes + binding->byte_offset;
    } else {
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
    }
    size_t stdout_bytes = 0u;
    if (!sequence_stdout_add(candidate.stdout_bytes, payload_bytes,
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
        .payload_bytes = payload_bytes};
    candidate.stdout_bytes = stdout_bytes;
    call_cursor += 1u;
  }

  if (binding_cursor != program->binding_count ||
      call_cursor != program->call_count)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  for (size_t binding = 0u; binding < binding_cursor; binding += 1u)
    if (binding_reads[binding] == 0u) return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  candidate.instruction_count = program->instruction_count;
  candidate.binding_count = binding_cursor;
  candidate.call_count = call_cursor;
  *sequence = candidate;
  return W_SEED_NATIVE_SUBSET0_OK;
}
