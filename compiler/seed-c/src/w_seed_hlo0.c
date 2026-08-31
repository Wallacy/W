#include "w_seed_hlo0.h"

#include <limits.h>
#include <string.h>

#include "w_seed_sha256.h"

_Static_assert(CHAR_BIT == 8, "w-seed HLO0 requires 8-bit bytes");

enum {
  HLO0_RECEIPT_CAPACITY = 2048,
  HLO0_DIGEST_BYTES = 32,
  HLO0_NEWLINE_BYTES = 1,
};

static const char HLO0_PROFILE[] = "native-process@1";
static const char HLO0_SLOT[] = ".default";
static const char HLO0_CALLEE[] = "print";
static const char HLO0_REQUIREMENT[] = "Console";

typedef struct {
  w_seed_hlo0_plan plan;
  uint8_t receipt[HLO0_RECEIPT_CAPACITY];
  size_t receipt_bytes;
} hlo0_candidate;

typedef struct {
  const w_seed_hir0_entry *entry;
  const w_seed_hir0_function *function;
  const w_seed_hir0_block *block;
  const w_seed_hir0_instruction *instruction;
  const w_seed_hir0_call *call;
  const w_seed_hir0_identity *callee;
  const w_seed_hir0_requirement *requirement;
  const w_seed_hir0_argument *argument;
  const w_seed_hir0_value *value;
} hlo0_selection;

static bool add_size(size_t left, size_t right, size_t *result) {
  if (result == NULL || right > SIZE_MAX - left) return false;
  *result = left + right;
  return true;
}

static bool ranges_overlap(const void *left, size_t left_bytes,
                           const void *right, size_t right_bytes) {
  if (left == NULL || right == NULL || left_bytes == 0u || right_bytes == 0u)
    return false;
  const uintptr_t left_start = (uintptr_t)left;
  const uintptr_t right_start = (uintptr_t)right;
  if (left_bytes > UINTPTR_MAX - left_start ||
      right_bytes > UINTPTR_MAX - right_start)
    return true;
  const uintptr_t left_end = left_start + (uintptr_t)left_bytes;
  const uintptr_t right_end = right_start + (uintptr_t)right_bytes;
  return left_start < right_end && right_start < left_end;
}

static bool hir_text_valid(const w_seed_hir0_program *program,
                           w_seed_hir0_text text) {
  return program != NULL && text.offset <= program->text_byte_count &&
         text.count <= program->text_byte_count - text.offset &&
         (text.count == 0u || program->text_bytes != NULL);
}

static bool hir_text_is(const w_seed_hir0_program *program,
                        w_seed_hir0_text text, const char *literal) {
  if (!hir_text_valid(program, text) || literal == NULL) return false;
  const size_t length = strlen(literal);
  return text.count == length &&
         (length == 0u ||
          memcmp(program->text_bytes + text.offset, literal, length) == 0);
}

static bool hir_texts_equal(const w_seed_hir0_program *program,
                            w_seed_hir0_text left, w_seed_hir0_text right) {
  if (!hir_text_valid(program, left) || !hir_text_valid(program, right) ||
      left.count != right.count)
    return false;
  return left.count == 0u ||
         memcmp(program->text_bytes + left.offset,
                program->text_bytes + right.offset, left.count) == 0;
}

static bool copy_literal(char *destination, size_t capacity,
                         const char *literal) {
  if (destination == NULL || literal == NULL) return false;
  const size_t length = strlen(literal);
  if (length >= capacity) return false;
  (void)memcpy(destination, literal, length + 1u);
  return true;
}

static bool copy_hir_text(char *destination, size_t capacity,
                          const w_seed_hir0_program *program,
                          w_seed_hir0_text text) {
  if (!hir_text_valid(program, text) || destination == NULL ||
      text.count >= capacity)
    return false;
  if (text.count != 0u)
    (void)memcpy(destination, program->text_bytes + text.offset, text.count);
  destination[text.count] = '\0';
  return true;
}

static bool bounded_length(const char *text, size_t capacity,
                           size_t *length) {
  if (text == NULL || length == NULL) return false;
  for (size_t index = 0u; index < capacity; index += 1u) {
    if (text[index] == '\0') {
      *length = index;
      return true;
    }
  }
  return false;
}

static bool plan_text_is(const char *text, size_t capacity,
                         const char *expected) {
  if (expected == NULL) return false;
  size_t length = 0u;
  if (!bounded_length(text, capacity, &length)) return false;
  const size_t expected_length = strlen(expected);
  if (length != expected_length ||
      memcmp(text, expected, expected_length) != 0)
    return false;
  for (size_t index = length + 1u; index < capacity; index += 1u)
    if (text[index] != '\0') return false;
  return true;
}

static bool plan_text_pair_is_equal_nonempty(const char *left,
                                             size_t left_capacity,
                                             const char *right,
                                             size_t right_capacity) {
  size_t left_length = 0u;
  size_t right_length = 0u;
  if (!bounded_length(left, left_capacity, &left_length) ||
      !bounded_length(right, right_capacity, &right_length) ||
      left_length == 0u || left_length != right_length ||
      memcmp(left, right, left_length) != 0)
    return false;
  for (size_t index = left_length + 1u; index < left_capacity; index += 1u)
    if (left[index] != '\0') return false;
  for (size_t index = right_length + 1u; index < right_capacity; index += 1u)
    if (right[index] != '\0') return false;
  return true;
}

static bool digest_equal(const uint8_t *left, const uint8_t *right,
                         size_t length) {
  return left != NULL && right != NULL && memcmp(left, right, length) == 0;
}

static bool bytes_are_zero(const uint8_t *bytes, size_t length) {
  if (bytes == NULL) return false;
  for (size_t index = 0u; index < length; index += 1u)
    if (bytes[index] != 0u) return false;
  return true;
}

bool w_seed_hlo0_verify_plan(const w_seed_hlo0_plan *plan) {
  size_t expected_stdout_bytes = 0u;
  if (plan == NULL ||
      !plan_text_is(plan->schema, sizeof(plan->schema),
                    W_SEED_HLO0_SCHEMA_VERSION) ||
      !plan_text_is(plan->profile, sizeof(plan->profile), HLO0_PROFILE) ||
      !plan_text_is(plan->slot, sizeof(plan->slot), HLO0_SLOT) ||
      !plan_text_pair_is_equal_nonempty(
          plan->entry_target, sizeof(plan->entry_target), plan->handler,
          sizeof(plan->handler)) ||
      !plan_text_is(plan->callee, sizeof(plan->callee), HLO0_CALLEE) ||
      !plan_text_is(plan->requirement, sizeof(plan->requirement),
                    HLO0_REQUIREMENT) ||
      plan->is_async || plan->is_throws || plan->is_unsafe ||
      plan->has_borrow_clause || !plan->zero_parameters || !plan->unit_return ||
      plan->newline_policy != W_SEED_HLO0_NEWLINE_ADD_LF ||
      plan->payload_bytes > sizeof(plan->payload) ||
      !bytes_are_zero(plan->payload + plan->payload_bytes,
                      sizeof(plan->payload) - plan->payload_bytes) ||
      !add_size(plan->payload_bytes, HLO0_NEWLINE_BYTES,
                &expected_stdout_bytes) ||
      plan->stdout_bytes != expected_stdout_bytes || !plan->exit_success) {
    return false;
  }

  w_seed_sha256_state state;
  uint8_t expected_digest[HLO0_DIGEST_BYTES];
  static const uint8_t line_feed = 0x0au;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, plan->payload, plan->payload_bytes);
  w_seed_sha256_update(&state, &line_feed, HLO0_NEWLINE_BYTES);
  w_seed_sha256_final(&state, expected_digest);
  return digest_equal(plan->stdout_sha256, expected_digest, HLO0_DIGEST_BYTES);
}

static bool select_print_literal(const w_seed_hir0_program *program,
                                 hlo0_selection *selection) {
  if (program == NULL || selection == NULL || program->module_count != 1u ||
      program->function_count != 1u || program->parameter_count != 0u ||
      program->block_count != 1u || program->instruction_count != 1u ||
      program->call_count != 1u || program->argument_count != 1u ||
      program->requirement_count != 1u || program->value_count != 1u ||
      program->terminator_count != 1u || program->entry_count != 1u)
    return false;
  (void)memset(selection, 0, sizeof(*selection));
  size_t matching_entries = 0u;
  for (size_t index = 0u; index < program->entry_count; index += 1u) {
    const w_seed_hir0_entry *entry = &program->entries[index];
    if (!hir_text_valid(program, entry->target_name) ||
        entry->target_name.count == 0u ||
        !hir_text_is(program, entry->slot, HLO0_SLOT))
      continue;
    matching_entries += 1u;
    selection->entry = entry;
  }
  if (matching_entries != 1u || selection->entry == NULL ||
      selection->entry->target_function >= program->function_count)
    return false;
  selection->function = &program->functions[selection->entry->target_function];
  if (!hir_text_valid(program, selection->function->name) ||
      selection->function->name.count == 0u ||
      !hir_texts_equal(program, selection->function->name,
                       selection->entry->target_name) ||
      selection->function->parameter_count != 0u ||
      selection->function->return_type >= program->type_count ||
      program->types[selection->function->return_type].kind !=
          W_SEED_HIR0_TYPE_UNIT ||
      selection->function->is_async || selection->function->is_throws ||
      selection->function->is_unsafe || selection->function->has_borrow_clause ||
      selection->function->block_count != 1u ||
      selection->function->first_block >= program->block_count)
    return false;
  selection->block = &program->blocks[selection->function->first_block];
  if (selection->block->instruction_count != 1u ||
      selection->block->first_instruction >= program->instruction_count)
    return false;
  selection->instruction =
      &program->instructions[selection->block->first_instruction];
  if (selection->instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
      selection->instruction->call_index >= program->call_count ||
      selection->instruction->result_type >= program->type_count ||
      program->types[selection->instruction->result_type].kind !=
          W_SEED_HIR0_TYPE_UNIT)
    return false;
  selection->call = &program->calls[selection->instruction->call_index];
  if (selection->call->argument_count != 1u ||
      selection->call->requirement_count != 1u ||
      selection->call->callee_identity >= program->identity_count ||
      selection->call->first_argument >= program->argument_count ||
      selection->call->first_requirement >= program->requirement_count ||
      selection->call->result_type >= program->type_count ||
      program->types[selection->call->result_type].kind !=
          W_SEED_HIR0_TYPE_UNIT)
    return false;
  selection->callee = &program->identities[selection->call->callee_identity];
  selection->requirement =
      &program->requirements[selection->call->first_requirement];
  if (selection->callee->kind != W_SEED_HIR0_IDENTITY_HOST_PRELUDE ||
      !hir_text_is(program, selection->callee->name, HLO0_CALLEE) ||
      !hir_text_is(program, selection->callee->profile, HLO0_PROFILE) ||
      !hir_text_is(program, selection->requirement->name, HLO0_REQUIREMENT))
    return false;
  selection->argument = &program->arguments[selection->call->first_argument];
  if (selection->argument->type_index >= program->type_count ||
      program->types[selection->argument->type_index].kind !=
          W_SEED_HIR0_TYPE_STRING ||
      selection->argument->label_kind != W_SEED_HIR0_LABEL_POSITIONAL_ONLY ||
      selection->argument->label.count != 0u ||
      selection->argument->value_index >= program->value_count)
    return false;
  selection->value = &program->values[selection->argument->value_index];
  return selection->value->kind == W_SEED_HIR0_VALUE_CONST_STRING &&
         selection->value->type_index < program->type_count &&
         program->types[selection->value->type_index].kind ==
             W_SEED_HIR0_TYPE_STRING &&
         selection->value->byte_count <= W_SEED_HLO0_MAX_PAYLOAD &&
         selection->value->byte_offset <= program->value_byte_count &&
         selection->value->byte_count <=
             program->value_byte_count - selection->value->byte_offset;
}

static bool append_bytes(uint8_t *buffer, size_t capacity, size_t *offset,
                         const void *bytes, size_t length) {
  if (buffer == NULL || offset == NULL ||
      (length != 0u && bytes == NULL) || *offset > capacity ||
      length > capacity - *offset)
    return false;
  if (length != 0u) (void)memcpy(buffer + *offset, bytes, length);
  *offset += length;
  return true;
}

static bool append_literal(uint8_t *buffer, size_t capacity, size_t *offset,
                           const char *literal) {
  return literal != NULL &&
         append_bytes(buffer, capacity, offset, literal, strlen(literal));
}

static bool append_size(uint8_t *buffer, size_t capacity, size_t *offset,
                        size_t value) {
  char digits[3u * sizeof(size_t) + 1u];
  size_t length = 0u;
  do {
    digits[length] = (char)('0' + value % 10u);
    value /= 10u;
    length += 1u;
  } while (value != 0u);
  for (size_t index = 0u; index < length / 2u; index += 1u) {
    const char swap = digits[index];
    digits[index] = digits[length - index - 1u];
    digits[length - index - 1u] = swap;
  }
  return append_bytes(buffer, capacity, offset, digits, length);
}

static const char HLO0_HEX[] = "0123456789abcdef";

static bool append_hex(uint8_t *buffer, size_t capacity, size_t *offset,
                       const uint8_t *bytes, size_t length) {
  if ((bytes == NULL && length != 0u) || *offset > capacity ||
      length > (capacity - *offset) / 2u)
    return false;
  for (size_t index = 0u; index < length; index += 1u) {
    buffer[*offset] = (uint8_t)HLO0_HEX[bytes[index] >> 4u];
    *offset += 1u;
    buffer[*offset] = (uint8_t)HLO0_HEX[bytes[index] & 0x0fu];
    *offset += 1u;
  }
  return true;
}

static bool write_receipt(const w_seed_hlo0_plan *plan, uint8_t *buffer,
                          size_t capacity, size_t *written) {
  if (plan == NULL || buffer == NULL || written == NULL) return false;
  size_t offset = 0u;
  if (!append_literal(buffer, capacity, &offset, "schema=") ||
      !append_bytes(buffer, capacity, &offset, plan->schema,
                    strlen(plan->schema)) ||
      !append_literal(buffer, capacity, &offset, "\nprofile=") ||
      !append_bytes(buffer, capacity, &offset, plan->profile,
                    strlen(plan->profile)) ||
      !append_literal(buffer, capacity, &offset, "\nslot=") ||
      !append_bytes(buffer, capacity, &offset, plan->slot, strlen(plan->slot)) ||
      !append_literal(buffer, capacity, &offset, "\nentry=") ||
      !append_bytes(buffer, capacity, &offset, plan->entry_target,
                    strlen(plan->entry_target)) ||
      !append_literal(buffer, capacity, &offset, "\nhandler=") ||
      !append_bytes(buffer, capacity, &offset, plan->handler,
                    strlen(plan->handler)) ||
      !append_literal(buffer, capacity, &offset, "\ncallee=") ||
      !append_bytes(buffer, capacity, &offset, plan->callee,
                    strlen(plan->callee)) ||
      !append_literal(buffer, capacity, &offset, "\nrequirement=") ||
      !append_bytes(buffer, capacity, &offset, plan->requirement,
                    strlen(plan->requirement)) ||
      !append_literal(buffer, capacity, &offset,
                      "\neffects=sync,no-throws,no-unsafe,no-borrows\n") ||
      !append_literal(buffer, capacity, &offset,
                      "signature=zero-params,unit\n") ||
      !append_literal(buffer, capacity, &offset, "payload=") ||
      !append_size(buffer, capacity, &offset, plan->payload_bytes) ||
      !append_literal(buffer, capacity, &offset, ":") ||
      !append_hex(buffer, capacity, &offset, plan->payload, plan->payload_bytes) ||
      !append_literal(buffer, capacity, &offset, "\nnewline=add-lf\nstdout=") ||
      !append_size(buffer, capacity, &offset, plan->stdout_bytes) ||
      !append_literal(buffer, capacity, &offset, "\nsha256=") ||
      !append_hex(buffer, capacity, &offset, plan->stdout_sha256,
                  HLO0_DIGEST_BYTES) ||
      !append_literal(buffer, capacity, &offset, "\nexit=success\n"))
    return false;
  *written = offset;
  return true;
}

typedef enum {
  HLO0_PREPARE_READY = 0,
  HLO0_PREPARE_UNSUPPORTED,
  HLO0_PREPARE_INVALID,
} hlo0_prepare_status;

static hlo0_prepare_status prepare_candidate(const w_seed_hlo0_input *input,
                                             hlo0_candidate *candidate) {
  if (input == NULL || input->program == NULL || input->hir_result == NULL ||
      candidate == NULL ||
      !w_seed_hir0_verify(input->program, input->hir_result))
    return HLO0_PREPARE_INVALID;
  hlo0_selection selection;
  if (!select_print_literal(input->program, &selection))
    return HLO0_PREPARE_UNSUPPORTED;
  (void)memset(candidate, 0, sizeof(*candidate));
  w_seed_hlo0_plan *plan = &candidate->plan;
  if (!copy_literal(plan->schema, sizeof(plan->schema),
                    W_SEED_HLO0_SCHEMA_VERSION) ||
      !copy_hir_text(plan->profile, sizeof(plan->profile), input->program,
                     selection.callee->profile) ||
      !copy_hir_text(plan->slot, sizeof(plan->slot), input->program,
                     selection.entry->slot) ||
      !copy_hir_text(plan->entry_target, sizeof(plan->entry_target),
                     input->program, selection.entry->target_name) ||
      !copy_hir_text(plan->handler, sizeof(plan->handler), input->program,
                     selection.function->name) ||
      !copy_hir_text(plan->callee, sizeof(plan->callee), input->program,
                     selection.callee->name) ||
      !copy_hir_text(plan->requirement, sizeof(plan->requirement),
                     input->program, selection.requirement->name))
    return HLO0_PREPARE_INVALID;
  plan->is_async = selection.function->is_async;
  plan->is_throws = selection.function->is_throws;
  plan->is_unsafe = selection.function->is_unsafe;
  plan->has_borrow_clause = selection.function->has_borrow_clause;
  plan->zero_parameters = selection.function->parameter_count == 0u;
  plan->unit_return = selection.function->return_type == W_SEED_HIR0_TYPE_UNIT;
  plan->payload_bytes = selection.value->byte_count;
  if (plan->payload_bytes != 0u && input->program->value_bytes == NULL)
    return HLO0_PREPARE_INVALID;
  if (plan->payload_bytes != 0u)
    (void)memcpy(plan->payload,
                 input->program->value_bytes + selection.value->byte_offset,
                 plan->payload_bytes);
  plan->newline_policy = W_SEED_HLO0_NEWLINE_ADD_LF;
  if (!add_size(plan->payload_bytes, HLO0_NEWLINE_BYTES, &plan->stdout_bytes))
    return HLO0_PREPARE_INVALID;
  w_seed_sha256_state sha;
  w_seed_sha256_init(&sha);
  w_seed_sha256_update(&sha, plan->payload, plan->payload_bytes);
  static const uint8_t line_feed = 0x0au;
  w_seed_sha256_update(&sha, &line_feed, HLO0_NEWLINE_BYTES);
  w_seed_sha256_final(&sha, plan->stdout_sha256);
  plan->exit_success = true;
  if (!write_receipt(plan, candidate->receipt, sizeof(candidate->receipt),
                     &candidate->receipt_bytes))
    return HLO0_PREPARE_INVALID;
  return HLO0_PREPARE_READY;
}

typedef struct {
  const void *address;
  size_t count;
  size_t element_size;
} hlo0_input_range;

static bool hlo0_range_add(hlo0_input_range *ranges, size_t capacity,
                           size_t *range_count, const void *address,
                           size_t count, size_t element_size) {
  if (ranges == NULL || range_count == NULL || *range_count >= capacity ||
      element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  ranges[*range_count] = (hlo0_input_range){address, count, element_size};
  *range_count += 1u;
  return true;
}

static bool hlo0_range_pair_overlaps(const hlo0_input_range *left,
                                     const hlo0_input_range *right) {
  if (left == NULL || right == NULL || left->element_size == 0u ||
      right->element_size == 0u)
    return true;
  if (left->count > SIZE_MAX / left->element_size ||
      right->count > SIZE_MAX / right->element_size)
    return true;
  return ranges_overlap(left->address, left->count * left->element_size,
                        right->address, right->count * right->element_size);
}

static bool output_aliases(const w_seed_hlo0_output *output) {
  if (output == NULL ||
      output->plan_capacity > SIZE_MAX / sizeof(w_seed_hlo0_plan))
    return true;
  const hlo0_input_range ranges[] = {
      {output, 1u, sizeof(*output)},
      {output->plans, output->plan_capacity, sizeof(w_seed_hlo0_plan)},
      {output->receipt, output->receipt_capacity, sizeof(uint8_t)},
  };
  for (size_t first = 0u; first < sizeof(ranges) / sizeof(ranges[0]);
       first += 1u)
    for (size_t second = first + 1u;
         second < sizeof(ranges) / sizeof(ranges[0]); second += 1u)
      if (hlo0_range_pair_overlaps(&ranges[first], &ranges[second])) return true;
  return false;
}

static bool input_aliases_outputs(const w_seed_hlo0_input *input,
                                  const w_seed_hlo0_output *output,
                                  const w_seed_hlo0_counts *counts,
                                  const w_seed_hlo0_result *result) {
  if (input == NULL || input->program == NULL || input->hir_result == NULL)
    return true;
  hlo0_input_range ranges[32];
  size_t range_count = 0u;
  const size_t range_capacity = sizeof(ranges) / sizeof(ranges[0]);
  if (!hlo0_range_add(ranges, range_capacity, &range_count, input, 1u,
                      sizeof(*input)))
    return true;
  if (counts != NULL &&
      !hlo0_range_add(ranges, range_capacity, &range_count, counts, 1u,
                      sizeof(*counts)))
    return true;
  if (result != NULL &&
      !hlo0_range_add(ranges, range_capacity, &range_count, result, 1u,
                      sizeof(*result)))
    return true;
  if (output != NULL) {
    if (!hlo0_range_add(ranges, range_capacity, &range_count, output, 1u,
                        sizeof(*output)) ||
        !hlo0_range_add(ranges, range_capacity, &range_count, output->plans,
                        output->plan_capacity, sizeof(w_seed_hlo0_plan)) ||
        !hlo0_range_add(ranges, range_capacity, &range_count, output->receipt,
                        output->receipt_capacity, sizeof(uint8_t)))
      return true;
  }
  const hlo0_input_range input_ranges[] = {
      {input->program, 1u, sizeof(*input->program)},
      {input->hir_result, 1u, sizeof(*input->hir_result)},
      {input->program->modules, input->program->module_capacity,
       sizeof(*input->program->modules)},
      {input->program->identities, input->program->identity_capacity,
       sizeof(*input->program->identities)},
      {input->program->types, input->program->type_capacity,
       sizeof(*input->program->types)},
      {input->program->functions, input->program->function_capacity,
       sizeof(*input->program->functions)},
      {input->program->parameters, input->program->parameter_capacity,
       sizeof(*input->program->parameters)},
      {input->program->blocks, input->program->block_capacity,
       sizeof(*input->program->blocks)},
      {input->program->instructions, input->program->instruction_capacity,
       sizeof(*input->program->instructions)},
      {input->program->calls, input->program->call_capacity,
       sizeof(*input->program->calls)},
      {input->program->host_parameters,
       input->program->host_parameter_capacity,
       sizeof(*input->program->host_parameters)},
      {input->program->arguments, input->program->argument_capacity,
       sizeof(*input->program->arguments)},
      {input->program->requirements, input->program->requirement_capacity,
       sizeof(*input->program->requirements)},
      {input->program->values, input->program->value_capacity,
       sizeof(*input->program->values)},
      {input->program->terminators, input->program->terminator_capacity,
       sizeof(*input->program->terminators)},
      {input->program->entries, input->program->entry_capacity,
       sizeof(*input->program->entries)},
      {input->program->text_bytes, input->program->text_byte_capacity,
       sizeof(uint8_t)},
      {input->program->value_bytes, input->program->value_byte_capacity,
       sizeof(uint8_t)},
      {input->program->receipt, input->program->receipt_capacity,
       sizeof(uint8_t)},
  };
  for (size_t index = 0u;
       index < sizeof(input_ranges) / sizeof(input_ranges[0]); index += 1u) {
    const hlo0_input_range *range = &input_ranges[index];
    if (!hlo0_range_add(ranges, range_capacity, &range_count, range->address,
                        range->count, range->element_size))
      return true;
  }
  for (size_t first = 0u; first < range_count; first += 1u)
    for (size_t second = first + 1u; second < range_count; second += 1u)
      if (hlo0_range_pair_overlaps(&ranges[first], &ranges[second])) return true;
  return false;
}

static bool counts_equal(const w_seed_hlo0_counts *left,
                         const w_seed_hlo0_counts *right) {
  return left != NULL && right != NULL && left->plans == right->plans &&
         left->payload_bytes == right->payload_bytes &&
         left->receipt_bytes == right->receipt_bytes;
}

w_seed_hlo0_status w_seed_hlo0_measure(const w_seed_hlo0_input *input,
                                       w_seed_hlo0_counts *counts,
                                       w_seed_hlo0_result *result) {
  if (counts == NULL || result == NULL ||
      ranges_overlap(counts, sizeof(*counts), result, sizeof(*result)))
    return W_SEED_HLO0_INVALID;
  if (input_aliases_outputs(input, NULL, counts, result))
    return W_SEED_HLO0_INVALID;
  hlo0_candidate candidate;
  const hlo0_prepare_status prepared = prepare_candidate(input, &candidate);
  if (prepared != HLO0_PREPARE_READY)
    return prepared == HLO0_PREPARE_UNSUPPORTED ? W_SEED_HLO0_UNSUPPORTED
                                                : W_SEED_HLO0_INVALID;
  w_seed_hlo0_counts candidate_counts = {
      1u, candidate.plan.payload_bytes, candidate.receipt_bytes};
  w_seed_hlo0_result candidate_result = {
      W_SEED_HLO0_OK, candidate_counts, {0u, 0u, 0u}};
  *counts = candidate_counts;
  *result = candidate_result;
  return W_SEED_HLO0_OK;
}

w_seed_hlo0_status w_seed_hlo0_run(const w_seed_hlo0_input *input,
                                   w_seed_hlo0_output *output,
                                   w_seed_hlo0_result *result) {
  if (result == NULL) return W_SEED_HLO0_INVALID;
  w_seed_hlo0_counts counts = {0u, 0u, 0u};
  w_seed_hlo0_result measured = {0};
  const w_seed_hlo0_status measured_status =
      w_seed_hlo0_measure(input, &counts, &measured);
  if (measured_status != W_SEED_HLO0_OK) return measured_status;
  if (!counts_equal(&measured.required, &counts)) return W_SEED_HLO0_INVALID;
  if (output == NULL || output->plan_capacity < counts.plans ||
      output->plans == NULL || output->receipt == NULL ||
      output->receipt_capacity < counts.receipt_bytes)
    return W_SEED_HLO0_CAPACITY;
  if (output_aliases(output)) return W_SEED_HLO0_INVALID;
  if (input_aliases_outputs(input, output, &counts, result))
    return W_SEED_HLO0_INVALID;
  hlo0_candidate candidate;
  if (prepare_candidate(input, &candidate) != HLO0_PREPARE_READY)
    return W_SEED_HLO0_INVALID;
  (void)memcpy(output->plans, &candidate.plan, sizeof(candidate.plan));
  (void)memcpy(output->receipt, candidate.receipt, candidate.receipt_bytes);
  w_seed_hlo0_result candidate_result = measured;
  candidate_result.written = counts;
  *result = candidate_result;
  return W_SEED_HLO0_OK;
}
