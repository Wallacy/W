#include "w_seed_cooperative0.h"

#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

typedef struct {
  const w_seed_hir0_program *program;
  w_seed_cooperative0_plan *plan;
  w_seed_cooperative0_trace_event *events;
  size_t event_count;
  uint32_t queue[W_SEED_COOPERATIVE0_MAX_QUEUE];
  size_t queue_count;
  uint8_t *stdout_bytes;
  size_t stdout_capacity;
  size_t stdout_count;
} cooperative_run;

_Static_assert(W_SEED_COOPERATIVE0_MAX_FUNCTIONS ==
                   W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS,
               "COOP0 helper state shares the HIR cooperative function bound");

static bool range_end(uintptr_t start, size_t length, uintptr_t *end) {
  if (end == NULL || length > UINTPTR_MAX - start) return false;
  *end = start + (uintptr_t)length;
  return true;
}

static bool range_valid(size_t first, size_t count, size_t total) {
  return first <= total && count <= total - first;
}

/* Program counters are serialized as u32 in the fixed frame/trace ABI.  A
 * block whose one-past-the-end PC would wrap cannot be represented by that
 * ABI, even when a wider host size_t could index the backing array. */
static bool instruction_span_end(uint32_t first, uint32_t count,
                                 uint32_t *end) {
  if (end == NULL || count > UINT32_MAX - first) return false;
  *end = first + count;
  return true;
}

static bool ranges_overlap(const void *left, size_t left_length,
                           const void *right, size_t right_length) {
  if (left == NULL || right == NULL || left_length == 0u ||
      right_length == 0u)
    return false;
  const uintptr_t left_start = (uintptr_t)left;
  const uintptr_t right_start = (uintptr_t)right;
  uintptr_t left_end = 0u;
  uintptr_t right_end = 0u;
  if (!range_end(left_start, left_length, &left_end) ||
      !range_end(right_start, right_length, &right_end))
    return true;
  return left_start < right_end && right_start < left_end;
}

static bool text_is(const w_seed_hir0_program *program, w_seed_hir0_text text,
                    const char *literal) {
  if (program == NULL || literal == NULL || program->text_bytes == NULL)
    return false;
  const size_t length = strlen(literal);
  return text.count == length && text.offset <= program->text_byte_count &&
         length <= program->text_byte_count - text.offset &&
         (length == 0u ||
          memcmp(program->text_bytes + text.offset, literal, length) == 0);
}

static bool type_is_scalar(const w_seed_hir0_program *program,
                           uint32_t type_index) {
  if (program == NULL || type_index >= program->type_count) return false;
  return program->types[type_index].kind == W_SEED_HIR0_TYPE_I64 ||
         program->types[type_index].kind == W_SEED_HIR0_TYPE_BOOL;
}

static bool type_is_unit(const w_seed_hir0_program *program,
                         uint32_t type_index) {
  return program != NULL && type_index < program->type_count &&
         program->types[type_index].kind == W_SEED_HIR0_TYPE_UNIT;
}

static bool value_bytes_valid(const w_seed_hir0_program *program,
                              uint32_t offset, uint32_t count) {
  return program != NULL && offset <= program->value_byte_count &&
         (size_t)count <= program->value_byte_count - offset;
}

static bool program_has_physical_calls(const w_seed_hir0_program *program) {
  if (program == NULL || program->calls == NULL) return false;
  for (size_t index = 0u; index < program->call_count; index += 1u)
    if (program->calls[index].execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_COOPERATIVE_TRACE ||
        program->calls[index].execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH)
      return true;
  return false;
}

static bool function_instruction_span(const w_seed_hir0_program *program,
                                      uint32_t function_index,
                                      uint32_t *first, uint32_t *count) {
  if (program == NULL || first == NULL || count == NULL ||
      function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->block_count != 1u || function->first_block >= program->block_count)
    return false;
  const w_seed_hir0_block *block = &program->blocks[function->first_block];
  uint32_t end = 0u;
  if (block->owner_function != function_index ||
      block->first_instruction > program->instruction_count ||
      block->instruction_count >
          program->instruction_count - block->first_instruction ||
      !instruction_span_end(block->first_instruction,
                            block->instruction_count, &end))
    return false;
  *first = block->first_instruction;
  *count = block->instruction_count;
  return true;
}

static bool checked_add_i64(int64_t left, int64_t right, int64_t *out) {
  if (out == NULL || (right > 0 && left > INT64_MAX - right) ||
      (right < 0 && left < INT64_MIN - right))
    return false;
  *out = left + right;
  return true;
}

static bool checked_sub_i64(int64_t left, int64_t right, int64_t *out) {
  if (out == NULL || (right < 0 && left > INT64_MAX + right) ||
      (right > 0 && left < INT64_MIN + right))
    return false;
  *out = left - right;
  return true;
}

static bool checked_mul_i64(int64_t left, int64_t right, int64_t *out) {
  if (out == NULL) return false;
  if (left == 0 || right == 0) {
    *out = 0;
    return true;
  }
  if (left == -1) {
    if (right == INT64_MIN) return false;
    *out = -right;
    return true;
  }
  if (right == -1) {
    if (left == INT64_MIN) return false;
    *out = -left;
    return true;
  }
  if (left > 0) {
    if (right > 0 && left > INT64_MAX / right) return false;
    if (right < 0 && right < INT64_MIN / left) return false;
  } else {
    if (right > 0 && left < INT64_MIN / right) return false;
    if (right < 0 && left < INT64_MAX / right) return false;
  }
  *out = left * right;
  return true;
}

static bool append_bytes(uint8_t *buffer, size_t capacity, size_t *offset,
                         const uint8_t *bytes, size_t count) {
  if (offset == NULL || *offset > capacity || count > capacity - *offset ||
      (count != 0u && bytes == NULL))
    return false;
  if (count != 0u) (void)memcpy(buffer + *offset, bytes, count);
  *offset += count;
  return true;
}

static bool append_literal(uint8_t *buffer, size_t capacity, size_t *offset,
                           const char *literal) {
  if (literal == NULL) return false;
  return append_bytes(buffer, capacity, offset, (const uint8_t *)literal,
                      strlen(literal));
}

static bool append_u32(uint8_t *buffer, size_t capacity, size_t *offset,
                       uint32_t value) {
  uint8_t digits[10];
  size_t count = 0u;
  do {
    digits[count] = (uint8_t)('0' + value % 10u);
    value /= 10u;
    count += 1u;
  } while (value != 0u && count < sizeof(digits));
  if (value != 0u) return false;
  while (count != 0u) {
    count -= 1u;
    if (!append_bytes(buffer, capacity, offset, &digits[count], 1u))
      return false;
  }
  return true;
}

static bool append_i64(uint8_t *buffer, size_t capacity, size_t *offset,
                       int64_t value) {
  if (value < 0 && !append_literal(buffer, capacity, offset, "-"))
    return false;
  uint64_t magnitude = value < 0
                           ? (uint64_t)(-(value + 1)) + 1u
                           : (uint64_t)value;
  uint8_t digits[20];
  size_t count = 0u;
  do {
    digits[count] = (uint8_t)('0' + magnitude % 10u);
    magnitude /= 10u;
    count += 1u;
  } while (magnitude != 0u && count < sizeof(digits));
  if (magnitude != 0u) return false;
  while (count != 0u) {
    count -= 1u;
    if (!append_bytes(buffer, capacity, offset, &digits[count], 1u))
      return false;
  }
  return true;
}

static bool append_hex(uint8_t *buffer, size_t capacity, size_t *offset,
                       const uint8_t *bytes, size_t count) {
  static const char digits[] = "0123456789abcdef";
  if (bytes == NULL && count != 0u) return false;
  for (size_t index = 0u; index < count; index += 1u) {
    const uint8_t value = bytes[index];
    if (!append_bytes(buffer, capacity, offset,
                      (const uint8_t *)&digits[value >> 4], 1u) ||
        !append_bytes(buffer, capacity, offset,
                      (const uint8_t *)&digits[value & 0x0fu], 1u))
      return false;
  }
  return true;
}

static bool queue_contains(const cooperative_run *run, uint32_t task_id) {
  if (run == NULL) return false;
  for (size_t index = 0u; index < run->queue_count; index += 1u)
    if (run->queue[index] == task_id) return true;
  return false;
}

static bool queue_push(cooperative_run *run, uint32_t task_id) {
  if (run == NULL || task_id >= W_SEED_COOPERATIVE0_MAX_TASKS ||
      run->queue_count >= W_SEED_COOPERATIVE0_MAX_QUEUE ||
      queue_contains(run, task_id))
    return false;
  run->queue[run->queue_count] = task_id;
  run->queue_count += 1u;
  return true;
}

static bool queue_pop(cooperative_run *run, uint32_t *task_id,
                      uint32_t before[W_SEED_COOPERATIVE0_MAX_QUEUE],
                      size_t *before_count) {
  if (run == NULL || task_id == NULL || before == NULL || before_count == NULL ||
      run->queue_count == 0u)
    return false;
  *before_count = run->queue_count;
  for (size_t index = 0u; index < run->queue_count; index += 1u)
    before[index] = run->queue[index];
  *task_id = run->queue[0];
  for (size_t index = 1u; index < run->queue_count; index += 1u)
    run->queue[index - 1u] = run->queue[index];
  run->queue_count -= 1u;
  return true;
}

static void digest_u32(w_seed_sha256_state *state, uint32_t value) {
  uint8_t bytes[4] = {(uint8_t)(value >> 24), (uint8_t)(value >> 16),
                      (uint8_t)(value >> 8), (uint8_t)value};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void digest_i64(w_seed_sha256_state *state, int64_t value) {
  const uint64_t bits = (uint64_t)value;
  uint8_t bytes[8] = {(uint8_t)(bits >> 56), (uint8_t)(bits >> 48),
                      (uint8_t)(bits >> 40), (uint8_t)(bits >> 32),
                      (uint8_t)(bits >> 24), (uint8_t)(bits >> 16),
                      (uint8_t)(bits >> 8), (uint8_t)bits};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static bool digest_value_parts(const w_seed_hir0_program *program,
                               const w_seed_cooperative0_value *value,
                               w_seed_sha256_state *state) {
  if (program == NULL || value == NULL || state == NULL) return false;
  digest_u32(state, (uint32_t)value->kind);
  switch (value->kind) {
    case W_SEED_COOPERATIVE0_VALUE_I64:
      digest_i64(state, value->integer);
      return true;
    case W_SEED_COOPERATIVE0_VALUE_BOOL:
      digest_u32(state, value->boolean ? 1u : 0u);
      return true;
    case W_SEED_COOPERATIVE0_VALUE_TEXT:
      if (!value_bytes_valid(program, value->byte_offset, value->byte_count) ||
          (value->byte_count != 0u && program->value_bytes == NULL))
        return false;
      digest_u32(state, value->byte_count);
      if (value->byte_count != 0u)
        w_seed_sha256_update(state, program->value_bytes + value->byte_offset,
                             value->byte_count);
      return true;
    case W_SEED_COOPERATIVE0_VALUE_NONE:
      return true;
    default:
      return false;
  }
}

static bool value_digest(const w_seed_hir0_program *program,
                         const w_seed_cooperative0_value *value,
                         uint8_t digest[32]) {
  if (program == NULL || value == NULL || digest == NULL) return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  if (!digest_value_parts(program, value, &state)) return false;
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool frame_digest(const w_seed_hir0_program *program,
                         const w_seed_cooperative0_frame *frame,
                         uint8_t digest[32]) {
  if (program == NULL || frame == NULL || digest == NULL ||
      frame->parameter_count > W_SEED_COOPERATIVE0_MAX_FRAME_PARAMETERS ||
      frame->binding_count > W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS)
    return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  digest_u32(&state, frame->function_index);
  digest_u32(&state, frame->next_instruction);
  digest_u32(&state, frame->first_instruction);
  digest_u32(&state, frame->instruction_count);
  digest_u32(&state, frame->parameter_count);
  digest_u32(&state, frame->binding_count);
  digest_u32(&state, frame->yield_count);
  digest_u32(&state, frame->settled ? 1u : 0u);
  digest_u32(&state, frame->cleaned ? 1u : 0u);
  digest_u32(&state, frame->outcome_committed ? 1u : 0u);
  for (size_t index = 0u; index < W_SEED_COOPERATIVE0_MAX_FRAME_PARAMETERS;
       index += 1u)
    if (!digest_value_parts(program, &frame->parameters[index], &state))
      return false;
  for (size_t index = 0u; index < W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS;
       index += 1u) {
    digest_u32(&state, frame->binding_initialized[index] ? 1u : 0u);
    if (!digest_value_parts(program, &frame->bindings[index], &state))
      return false;
  }
  if (!digest_value_parts(program, &frame->result, &state)) return false;
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool trace_push(
    cooperative_run *run, w_seed_cooperative0_event_kind kind,
    uint32_t task_id, const uint32_t before[W_SEED_COOPERATIVE0_MAX_QUEUE],
    size_t before_count, uint32_t frame_slot, uint32_t pc,
    uint32_t next_pc, uint32_t yield_ordinal,
    const w_seed_cooperative0_value *outcome) {
  if (run == NULL || run->plan == NULL || run->events == NULL || before == NULL ||
      before_count > W_SEED_COOPERATIVE0_MAX_QUEUE ||
      run->queue_count > W_SEED_COOPERATIVE0_MAX_QUEUE ||
      run->event_count >= W_SEED_COOPERATIVE0_MAX_TRACE_EVENTS)
    return false;
  w_seed_cooperative0_trace_event *event = &run->events[run->event_count];
  *event = (w_seed_cooperative0_trace_event){
      .sequence = (uint32_t)(run->event_count + 1u),
      .kind = kind,
      .task_id = task_id,
      .frame_slot = frame_slot,
      .pc = pc,
      .next_pc = next_pc,
      .yield_ordinal = yield_ordinal,
      .queue_before_count = (uint32_t)before_count,
      .queue_after_count = (uint32_t)run->queue_count};
  for (size_t index = 0u; index < before_count; index += 1u)
    event->queue_before[index] = before[index];
  for (size_t index = 0u; index < run->queue_count; index += 1u)
    event->queue_after[index] = run->queue[index];
  if (frame_slot >= W_SEED_COOPERATIVE0_MAX_TASKS ||
      !frame_digest(run->program, &run->plan->frames[frame_slot],
                    event->frame_digest))
    return false;
  if (outcome != NULL && !value_digest(run->program, outcome,
                                       event->outcome_digest))
    return false;
  run->event_count += 1u;
  return true;
}

static bool value_from_i64(int64_t value, w_seed_cooperative0_value *out) {
  if (out == NULL) return false;
  *out = (w_seed_cooperative0_value){
      .kind = W_SEED_COOPERATIVE0_VALUE_I64, .integer = value};
  return true;
}

static bool value_from_bool(bool value, w_seed_cooperative0_value *out) {
  if (out == NULL) return false;
  *out = (w_seed_cooperative0_value){
      .kind = W_SEED_COOPERATIVE0_VALUE_BOOL, .boolean = value};
  return true;
}

static bool frame_parameter_ordinal(const w_seed_hir0_program *program,
                                    const w_seed_cooperative0_frame *frame,
                                    uint32_t parameter_index,
                                    uint32_t *ordinal) {
  if (program == NULL || frame == NULL || ordinal == NULL ||
      frame->function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function =
      &program->functions[frame->function_index];
  if (parameter_index < function->first_parameter ||
      parameter_index - function->first_parameter >= frame->parameter_count)
    return false;
  *ordinal = parameter_index - function->first_parameter;
  return true;
}

static bool evaluate_value(const w_seed_hir0_program *program, uint32_t value_index,
                           const w_seed_cooperative0_frame *frame,
                           w_seed_cooperative0_value *out, size_t depth);

static bool execute_sync_function(const w_seed_hir0_program *program,
                                  uint32_t function_index,
                                  const w_seed_cooperative0_value *parameters,
                                  size_t parameter_count,
                                  w_seed_cooperative0_value *out,
                                  size_t depth);

static bool evaluate_call(const w_seed_hir0_program *program, uint32_t call_index,
                          const w_seed_cooperative0_frame *caller,
                          w_seed_cooperative0_value *out, size_t depth) {
  if (program == NULL || caller == NULL || out == NULL || depth > 64u ||
      call_index >= program->call_count)
    return false;
  const w_seed_hir0_call *call = &program->calls[call_index];
  if (call->callee_identity >= program->identity_count) return false;
  const w_seed_hir0_identity *identity =
      &program->identities[call->callee_identity];
  if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      identity->target_index >= program->function_count ||
      identity->target_index == caller->function_index ||
      call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
      call->argument_count > W_SEED_COOPERATIVE0_MAX_FRAME_PARAMETERS)
    return false;
  w_seed_cooperative0_value parameters[W_SEED_COOPERATIVE0_MAX_FRAME_PARAMETERS];
  (void)memset(parameters, 0, sizeof(parameters));
  for (size_t ordinal = 0u; ordinal < call->argument_count; ordinal += 1u) {
    const w_seed_hir0_argument *argument =
        &program->arguments[(size_t)call->first_argument + ordinal];
    if (argument->parameter_ordinal >=
            W_SEED_COOPERATIVE0_MAX_FRAME_PARAMETERS ||
        !evaluate_value(program, argument->value_index, caller,
                        &parameters[argument->parameter_ordinal], depth + 1u))
      return false;
  }
  return execute_sync_function(program, identity->target_index, parameters,
                               identity->parameter_count, out, depth + 1u);
}

static bool evaluate_value(const w_seed_hir0_program *program, uint32_t value_index,
                           const w_seed_cooperative0_frame *frame,
                           w_seed_cooperative0_value *out, size_t depth) {
  if (program == NULL || frame == NULL || out == NULL || depth > 64u ||
      value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  switch (value->kind) {
    case W_SEED_HIR0_VALUE_CONST_I64:
      return value_from_i64(value->integer_value, out);
    case W_SEED_HIR0_VALUE_CONST_BOOL:
      return value_from_bool(value->bool_value, out);
    case W_SEED_HIR0_VALUE_CONST_STRING:
      if (!value_bytes_valid(program, value->byte_offset, value->byte_count))
        return false;
      *out = (w_seed_cooperative0_value){
          .kind = W_SEED_COOPERATIVE0_VALUE_TEXT,
          .byte_offset = value->byte_offset,
          .byte_count = value->byte_count};
      return true;
    case W_SEED_HIR0_VALUE_BINDING_READ:
      return value->binding_index < W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS &&
             frame->binding_initialized[value->binding_index] &&
             (*out = frame->bindings[value->binding_index], true);
    case W_SEED_HIR0_VALUE_PARAMETER_READ: {
      uint32_t ordinal = 0u;
      return frame_parameter_ordinal(program, frame, value->parameter_index,
                                    &ordinal) &&
             ordinal < W_SEED_COOPERATIVE0_MAX_FRAME_PARAMETERS &&
             (*out = frame->parameters[ordinal], true);
    }
    case W_SEED_HIR0_VALUE_CALL_RESULT:
      return evaluate_call(program, value->call_index, frame, out, depth + 1u);
    case W_SEED_HIR0_VALUE_UNARY_I64: {
      w_seed_cooperative0_value child;
      if (!evaluate_value(program, value->left_value, frame, &child,
                          depth + 1u) ||
          child.kind != W_SEED_COOPERATIVE0_VALUE_I64 ||
          (value->unary_operator != W_SEED_HIR0_UNARY_NEGATE &&
           value->unary_operator != W_SEED_HIR0_UNARY_BIT_NOT))
        return false;
      if (value->unary_operator == W_SEED_HIR0_UNARY_NEGATE)
        return child.integer != INT64_MIN &&
               value_from_i64(-child.integer, out);
      return value_from_i64(~child.integer, out);
    }
    case W_SEED_HIR0_VALUE_UNARY_BOOL: {
      w_seed_cooperative0_value child;
      if (!evaluate_value(program, value->left_value, frame, &child,
                          depth + 1u) ||
          child.kind != W_SEED_COOPERATIVE0_VALUE_BOOL)
        return false;
      return value_from_bool(!child.boolean, out);
    }
    case W_SEED_HIR0_VALUE_BINARY_I64: {
      w_seed_cooperative0_value left;
      w_seed_cooperative0_value right;
      if (!evaluate_value(program, value->left_value, frame, &left,
                          depth + 1u) ||
          !evaluate_value(program, value->right_value, frame, &right,
                          depth + 1u) ||
          left.kind != W_SEED_COOPERATIVE0_VALUE_I64 ||
          right.kind != W_SEED_COOPERATIVE0_VALUE_I64)
        return false;
      int64_t result = 0;
      bool boolean = false;
      switch (value->binary_operator) {
        case W_SEED_HIR0_BINARY_ADD:
          return checked_add_i64(left.integer, right.integer, &result) &&
                 value_from_i64(result, out);
        case W_SEED_HIR0_BINARY_SUBTRACT:
          return checked_sub_i64(left.integer, right.integer, &result) &&
                 value_from_i64(result, out);
        case W_SEED_HIR0_BINARY_MULTIPLY:
          return checked_mul_i64(left.integer, right.integer, &result) &&
                 value_from_i64(result, out);
        case W_SEED_HIR0_BINARY_DIVIDE:
          if (right.integer == 0 ||
              (left.integer == INT64_MIN && right.integer == -1))
            return false;
          return value_from_i64(left.integer / right.integer, out);
        case W_SEED_HIR0_BINARY_REMAINDER:
          if (right.integer == 0 ||
              (left.integer == INT64_MIN && right.integer == -1))
            return false;
          return value_from_i64(left.integer % right.integer, out);
        case W_SEED_HIR0_BINARY_BIT_AND:
          return value_from_i64(left.integer & right.integer, out);
        case W_SEED_HIR0_BINARY_BIT_OR:
          return value_from_i64(left.integer | right.integer, out);
        case W_SEED_HIR0_BINARY_BIT_XOR:
          return value_from_i64(left.integer ^ right.integer, out);
        case W_SEED_HIR0_BINARY_EQUAL:
          boolean = left.integer == right.integer;
          break;
        case W_SEED_HIR0_BINARY_NOT_EQUAL:
          boolean = left.integer != right.integer;
          break;
        case W_SEED_HIR0_BINARY_LESS:
          boolean = left.integer < right.integer;
          break;
        case W_SEED_HIR0_BINARY_LESS_EQUAL:
          boolean = left.integer <= right.integer;
          break;
        case W_SEED_HIR0_BINARY_GREATER:
          boolean = left.integer > right.integer;
          break;
        case W_SEED_HIR0_BINARY_GREATER_EQUAL:
          boolean = left.integer >= right.integer;
          break;
        default:
          return false;
      }
      return value_from_bool(boolean, out);
    }
    default:
      return false;
  }
}

static bool append_value_text(const w_seed_hir0_program *program,
                              uint32_t value_index,
                              const w_seed_cooperative0_frame *frame,
                              uint8_t *buffer, size_t capacity, size_t *offset,
                              size_t depth) {
  if (program == NULL || frame == NULL || buffer == NULL || offset == NULL ||
      depth > 64u || value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
    if (value->first_interpolation_segment == W_SEED_HIR0_NONE ||
        value->first_interpolation_segment >
            program->interpolation_segment_count ||
        value->interpolation_segment_count >
            program->interpolation_segment_count -
                value->first_interpolation_segment)
      return false;
    for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_hir0_interpolation_segment *segment =
          &program->interpolation_segments[(size_t)value->first_interpolation_segment +
                                           ordinal];
      if (segment->kind == W_SEED_HIR0_INTERPOLATION_TEXT) {
        if (!value_bytes_valid(program, segment->byte_offset,
                               segment->byte_count) ||
            !append_bytes(buffer, capacity, offset,
                          program->value_bytes + segment->byte_offset,
                          segment->byte_count))
          return false;
      } else if (segment->kind == W_SEED_HIR0_INTERPOLATION_VALUE) {
        if (!append_value_text(program, segment->value_index, frame, buffer,
                               capacity, offset, depth + 1u))
          return false;
      } else {
        return false;
      }
    }
    return true;
  }
  w_seed_cooperative0_value evaluated;
  if (!evaluate_value(program, value_index, frame, &evaluated, depth + 1u))
    return false;
  switch (evaluated.kind) {
    case W_SEED_COOPERATIVE0_VALUE_TEXT:
      return value_bytes_valid(program, evaluated.byte_offset,
                               evaluated.byte_count) &&
             append_bytes(buffer, capacity, offset,
                          program->value_bytes + evaluated.byte_offset,
                          evaluated.byte_count);
    case W_SEED_COOPERATIVE0_VALUE_I64:
      return append_i64(buffer, capacity, offset, evaluated.integer);
    case W_SEED_COOPERATIVE0_VALUE_BOOL:
      return evaluated.boolean
                 ? append_literal(buffer, capacity, offset, "true")
                 : append_literal(buffer, capacity, offset, "false");
    default:
      return false;
  }
}

static bool host_print_call(const w_seed_hir0_program *program,
                            const w_seed_hir0_call *call) {
  if (program == NULL || call == NULL || call->callee_identity >= program->identity_count)
    return false;
  const w_seed_hir0_identity *identity =
      &program->identities[call->callee_identity];
  return identity->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE &&
         text_is(program, identity->name, "print") && call->argument_count == 1u &&
         type_is_unit(program, call->result_type);
}

static bool append_print_call(const w_seed_hir0_program *program,
                              const w_seed_hir0_call *call,
                              const w_seed_cooperative0_frame *frame,
                              uint8_t *buffer, size_t capacity, size_t *offset) {
  if (!host_print_call(program, call) || call->first_argument >= program->argument_count)
    return false;
  const w_seed_hir0_argument *argument = &program->arguments[call->first_argument];
  return append_value_text(program, argument->value_index, frame, buffer,
                           capacity, offset, 0u) &&
         append_bytes(buffer, capacity, offset, (const uint8_t *)"\n", 1u);
}

static bool execute_sync_function(const w_seed_hir0_program *program,
                                  uint32_t function_index,
                                  const w_seed_cooperative0_value *parameters,
                                  size_t parameter_count,
                                  w_seed_cooperative0_value *out,
                                  size_t depth) {
  if (program == NULL || parameters == NULL || out == NULL || depth > 64u ||
      function_index >= program->function_count ||
      parameter_count > W_SEED_COOPERATIVE0_MAX_FRAME_PARAMETERS)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->is_async || parameter_count != function->parameter_count ||
      function->block_count != 1u || function->first_block >= program->block_count)
    return false;
  w_seed_cooperative0_frame frame;
  (void)memset(&frame, 0, sizeof(frame));
  frame.function_index = function_index;
  frame.parameter_count = (uint32_t)parameter_count;
  for (size_t index = 0u; index < parameter_count; index += 1u)
    frame.parameters[index] = parameters[index];
  uint32_t first = 0u;
  uint32_t instruction_count_u32 = 0u;
  if (!function_instruction_span(program, function_index, &first,
                                 &instruction_count_u32))
    return false;
  const size_t instruction_count = (size_t)instruction_count_u32;
  for (size_t ordinal = 0u; ordinal < instruction_count; ordinal += 1u) {
    const w_seed_hir0_instruction *instruction = &program->instructions[first + ordinal];
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS ||
          instruction->binding_index >= program->binding_count ||
          !evaluate_value(program,
                          program->bindings[instruction->binding_index]
                              .initializer_value,
                          &frame, &frame.bindings[instruction->binding_index],
                          depth + 1u))
        return false;
      frame.binding_initialized[instruction->binding_index] = true;
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (instruction->call_index >= program->call_count ||
          !host_print_call(program, &program->calls[instruction->call_index])) {
        if (instruction->call_index >= program->call_count) return false;
        w_seed_cooperative0_value ignored;
        if (!evaluate_call(program, instruction->call_index, &frame, &ignored,
                           depth + 1u))
          return false;
      } else {
        return false;
      }
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) {
      return false;
    } else {
      return false;
    }
  }
  const w_seed_hir0_block *block = &program->blocks[function->first_block];
  if (block->terminator_index >= program->terminator_count) return false;
  const w_seed_hir0_terminator *terminator =
      &program->terminators[block->terminator_index];
  if (terminator->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      terminator->value_index >= program->value_count)
    return false;
  return evaluate_value(program, terminator->value_index, &frame, out,
                        depth + 1u);
}

static bool cooperative_sync_function_shape(
    const w_seed_hir0_program *program, uint32_t function_index,
    uint32_t module_index, uint8_t state[W_SEED_COOPERATIVE0_MAX_FUNCTIONS],
    size_t depth) {
  if (program == NULL || state == NULL || function_index >= program->function_count ||
      depth > W_SEED_HIR0_MAX_NESTING)
    return false;
  /* The fixed state array is deliberately larger than the two task limit so
   * ordinary same-module helper calls can be checked without allocation. */
  if (function_index >= W_SEED_COOPERATIVE0_MAX_FUNCTIONS) return false;
  if (state[function_index] == 2u) return true;
  if (state[function_index] == 1u) return false;
  state[function_index] = 1u;
  const w_seed_hir0_function *function = &program->functions[function_index];
  bool valid = function->module_index == module_index && !function->is_async &&
               !function->is_const && !function->is_throws &&
               !function->is_unsafe && !function->has_borrow_clause &&
               !function->is_anonymous_entry &&
               type_is_scalar(program, function->return_type) &&
               function->parameter_count <=
                   W_SEED_COOPERATIVE0_MAX_FRAME_PARAMETERS;
  for (size_t parameter = 0u; valid && parameter < function->parameter_count;
       parameter += 1u) {
    const size_t index = (size_t)function->first_parameter + parameter;
    valid = index < program->parameter_count &&
            type_is_scalar(program, program->parameters[index].type_index);
  }
  uint32_t first = 0u;
  uint32_t instructions_u32 = 0u;
  const bool instruction_span_ok = function_instruction_span(
      program, function_index, &first, &instructions_u32);
  const size_t instructions = (size_t)instructions_u32;
  if (!instruction_span_ok) {
    state[function_index] = 3u;
    return false;
  }
  for (size_t ordinal = 0u; valid && ordinal < instructions; ordinal += 1u) {
    const w_seed_hir0_instruction *instruction = &program->instructions[first + ordinal];
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) {
      valid = false;
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= program->binding_count ||
          instruction->binding_index >= W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS ||
          program->bindings[instruction->binding_index].task_role !=
              W_SEED_HIR0_TASK_ROLE_NONE ||
          !type_is_scalar(program,
                          program->bindings[instruction->binding_index].type_index))
        valid = false;
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (instruction->call_index >= program->call_count) {
        valid = false;
      } else {
        const w_seed_hir0_call *call = &program->calls[instruction->call_index];
        if (call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
            call->callee_identity >= program->identity_count) {
          valid = false;
        } else {
          const w_seed_hir0_identity *identity =
              &program->identities[call->callee_identity];
          valid = identity->kind == W_SEED_HIR0_IDENTITY_FUNCTION &&
                  identity->target_index < program->function_count &&
                  !program->functions[identity->target_index].is_async &&
                  type_is_scalar(program, call->result_type) &&
                  cooperative_sync_function_shape(
                      program, identity->target_index, module_index, state,
                      depth + 1u);
        }
      }
    } else {
      valid = false;
    }
  }
  /* This helper is used only for ordinary pure callees. */
  const w_seed_hir0_block *block =
      function->first_block < program->block_count
          ? &program->blocks[function->first_block]
          : NULL;
  if (block == NULL || block->terminator_index >= program->terminator_count ||
      block->terminator_index >= program->block_count) {
    valid = false;
  } else {
    const w_seed_hir0_terminator *terminator =
        &program->terminators[block->terminator_index];
    valid = valid &&
            terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
            terminator->value_index < program->value_count &&
            type_is_scalar(program, terminator->result_type);
  }
  state[function_index] = valid ? 2u : 3u;
  return valid;
}

static bool cooperative_async_function_shape(const w_seed_hir0_program *program,
                                             uint32_t function_index,
                                             uint32_t module_index,
                                             uint32_t *yield_count) {
  if (program == NULL || yield_count == NULL || function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (!function->is_async || function->is_const || function->is_throws ||
      function->is_unsafe || function->has_borrow_clause ||
      function->is_anonymous_entry || function->module_index != module_index ||
      !type_is_scalar(program, function->return_type) ||
      function->parameter_count > W_SEED_COOPERATIVE0_MAX_FRAME_PARAMETERS)
    return false;
  for (size_t parameter = 0u; parameter < function->parameter_count; parameter += 1u) {
    const size_t index = (size_t)function->first_parameter + parameter;
    if (index >= program->parameter_count ||
        !type_is_scalar(program, program->parameters[index].type_index))
      return false;
  }
  uint32_t first = 0u;
  uint32_t instructions_u32 = 0u;
  if (!function_instruction_span(program, function_index, &first,
                                 &instructions_u32))
    return false;
  const size_t instructions = (size_t)instructions_u32;
  if (instructions == 0u) return false;
  uint32_t yields = 0u;
  uint8_t helper_state[W_SEED_COOPERATIVE0_MAX_FUNCTIONS] = {0u};
  for (size_t ordinal = 0u; ordinal < instructions; ordinal += 1u) {
    const w_seed_hir0_instruction *instruction = &program->instructions[first + ordinal];
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) {
      yields += 1u;
      if (yields > W_SEED_COOPERATIVE0_MAX_YIELDS_PER_TASK) return false;
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= program->binding_count ||
          instruction->binding_index >= W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS ||
          program->bindings[instruction->binding_index].task_role !=
              W_SEED_HIR0_TASK_ROLE_NONE ||
          !type_is_scalar(program,
                          program->bindings[instruction->binding_index].type_index))
        return false;
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (instruction->call_index >= program->call_count) return false;
      const w_seed_hir0_call *call = &program->calls[instruction->call_index];
      if (call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
          call->callee_identity >= program->identity_count ||
          !type_is_scalar(program, call->result_type))
        return false;
      const w_seed_hir0_identity *identity =
          &program->identities[call->callee_identity];
      if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
          identity->target_index >= program->function_count ||
          !cooperative_sync_function_shape(
              program, identity->target_index, module_index, helper_state, 0u))
        return false;
    } else {
      return false;
    }
  }
  if (yields == 0u) return false;
  const w_seed_hir0_block *block = &program->blocks[function->first_block];
  if (block->terminator_index >= program->terminator_count) return false;
  const w_seed_hir0_terminator *terminator =
      &program->terminators[block->terminator_index];
  if (terminator->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      terminator->value_index >= program->value_count ||
      !type_is_scalar(program, terminator->result_type))
    return false;
  *yield_count = yields;
  return true;
}

static bool plan_from_program(const w_seed_hir0_program *program,
                              const w_seed_hir0_result *hir_result,
                              w_seed_cooperative0_plan *plan) {
  if (program == NULL || hir_result == NULL || plan == NULL ||
      !program_has_physical_calls(program) ||
      program->entry_count != 1u || program->entries == NULL ||
      program->entries[0].target_function >= program->function_count)
    return false;
  (void)memset(plan, 0, sizeof(*plan));
  (void)memcpy(plan->schema, W_SEED_COOPERATIVE0_SCHEMA_VERSION,
              sizeof(plan->schema));
  plan->phase = W_SEED_COOPERATIVE0_PLAN_INITIAL;
  (void)memcpy(plan->hir_semantic_digest, hir_result->semantic_digest,
               sizeof(plan->hir_semantic_digest));
  plan->execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL;
  const uint32_t root_index = program->entries[0].target_function;
  const w_seed_hir0_function *root = &program->functions[root_index];
  if (!root->is_anonymous_entry || root->block_count != 1u ||
      root->first_block >= program->block_count || program->binding_count >
          W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS)
    return false;
  const w_seed_hir0_block *root_block = &program->blocks[root->first_block];
  if (root_block->owner_function != root_index ||
      root_block->terminator_index >= program->terminator_count ||
      !type_is_unit(program, root->return_type) ||
      program->terminators[root_block->terminator_index].kind !=
          W_SEED_HIR0_TERMINATOR_RETURN_UNIT ||
      !range_valid(root_block->first_instruction,
                   root_block->instruction_count,
                   program->instruction_count))
    return false;
  size_t physical_count = 0u;
  uint32_t physical_calls[W_SEED_COOPERATIVE0_MAX_TASKS] = {
      W_SEED_COOPERATIVE0_NONE, W_SEED_COOPERATIVE0_NONE};
  for (size_t call_index = 0u; call_index < program->call_count; call_index += 1u) {
    const w_seed_hir0_call *call = &program->calls[call_index];
    const bool physical =
        call->execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_COOPERATIVE_TRACE ||
        call->execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH;
    const bool structured =
        call->execution_kind == W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED ||
        call->execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED || physical;
    if (structured && !physical) return false;
    if (!physical) continue;
    if (physical_count >= W_SEED_COOPERATIVE0_MAX_TASKS ||
        call->owner_block != root->first_block ||
        call->owner_instruction >= program->instruction_count ||
        call->callee_identity >= program->identity_count)
      return false;
    const w_seed_hir0_execution_profile call_profile =
        call->execution_kind ==
                W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH
            ? W_SEED_HIR0_EXECUTION_PROFILE_MAIN_SERIAL
            : W_SEED_HIR0_EXECUTION_PROFILE_COOPERATIVE_TRACE;
    if (physical_count == 0u)
      plan->execution_profile = call_profile;
    else if (plan->execution_profile != call_profile)
      return false;
    if (physical_count != 0u &&
        call->owner_instruction <=
            program->calls[physical_calls[physical_count - 1u]]
                .owner_instruction)
      return false;
    const w_seed_hir0_identity *identity = &program->identities[call->callee_identity];
    if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
        identity->target_index >= program->function_count ||
        !cooperative_async_function_shape(program, identity->target_index,
                                          root->module_index,
                                          &plan->tasks[physical_count].yield_count))
      return false;
    physical_calls[physical_count] = (uint32_t)call_index;
    physical_count += 1u;
  }
  if (physical_count != W_SEED_COOPERATIVE0_MAX_TASKS) return false;
  size_t root_launch_bindings = 0u;
  size_t root_join_bindings = 0u;
  size_t root_physical_calls = 0u;
  size_t last_join_instruction = 0u;
  for (size_t ordinal = 0u; ordinal < root_block->instruction_count; ordinal += 1u) {
    const size_t instruction_index = (size_t)root_block->first_instruction + ordinal;
    if (instruction_index >= program->instruction_count) return false;
    const w_seed_hir0_instruction *instruction = &program->instructions[instruction_index];
    if (instruction->owner_block != root->first_block ||
        instruction->ordinal != ordinal)
      return false;
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) return false;
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= program->binding_count) return false;
      const w_seed_hir0_binding *binding = &program->bindings[instruction->binding_index];
      if (binding->owner_block != root->first_block ||
          binding->owner_instruction != instruction_index)
        return false;
      if (binding->task_role == W_SEED_HIR0_TASK_ROLE_LAUNCH) {
        if (root_join_bindings != 0u ||
            root_launch_bindings >= W_SEED_COOPERATIVE0_MAX_TASKS)
          return false;
        root_launch_bindings += 1u;
      } else if (binding->task_role == W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT) {
        if (root_launch_bindings != W_SEED_COOPERATIVE0_MAX_TASKS ||
            root_join_bindings >= W_SEED_COOPERATIVE0_MAX_TASKS)
          return false;
        root_join_bindings += 1u;
        last_join_instruction = instruction_index;
      } else {
        return false;
      }
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (instruction->call_index >= program->call_count) return false;
      const w_seed_hir0_call *call = &program->calls[instruction->call_index];
      if (call->execution_kind ==
              W_SEED_HIR0_CALL_STRUCTURED_ASYNC_COOPERATIVE_TRACE ||
          call->execution_kind ==
              W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH) {
        if (root_join_bindings != 0u ||
            root_physical_calls >= W_SEED_COOPERATIVE0_MAX_TASKS ||
            instruction->call_index != physical_calls[root_physical_calls] ||
            instruction_index + 1u >= program->instruction_count ||
            program->instructions[instruction_index + 1u].kind !=
                W_SEED_HIR0_INSTRUCTION_BINDING)
          return false;
        root_physical_calls += 1u;
      } else if (!host_print_call(program, call) ||
                 root_join_bindings != W_SEED_COOPERATIVE0_MAX_TASKS) {
        return false;
      }
    } else {
      return false;
    }
  }
  if (root_launch_bindings != W_SEED_COOPERATIVE0_MAX_TASKS ||
      root_join_bindings != W_SEED_COOPERATIVE0_MAX_TASKS ||
      root_physical_calls != W_SEED_COOPERATIVE0_MAX_TASKS)
    return false;
  size_t launch_bindings_seen = 0u;
  size_t join_bindings_seen = 0u;
  for (size_t call_ordinal = 0u; call_ordinal < 2u; call_ordinal += 1u) {
    const uint32_t call_index = physical_calls[call_ordinal];
    const w_seed_hir0_call *call = &program->calls[call_index];
    if (call->owner_instruction == UINT32_MAX ||
        (size_t)call->owner_instruction + 1u >= program->instruction_count)
      return false;
    const w_seed_hir0_instruction *launch_instruction =
        &program->instructions[call->owner_instruction + 1u];
    if (launch_instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
        launch_instruction->binding_index >= program->binding_count)
      return false;
    const uint32_t launch_binding_index = launch_instruction->binding_index;
    const w_seed_hir0_binding *launch = &program->bindings[launch_binding_index];
    if (launch->task_role != W_SEED_HIR0_TASK_ROLE_LAUNCH ||
        launch->task_peer_binding >= program->binding_count ||
        launch->owner_block != root->first_block ||
        launch->owner_instruction <= call->owner_instruction)
      return false;
    const w_seed_hir0_binding *join = &program->bindings[launch->task_peer_binding];
    if (join->task_role != W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT ||
        join->task_peer_binding != launch_binding_index ||
        join->owner_block != root->first_block ||
        join->owner_instruction <= launch->owner_instruction ||
        join->owner_instruction <= call->owner_instruction)
      return false;
    if (call_ordinal != 0u &&
        (launch->owner_instruction <=
             program->bindings[plan->tasks[call_ordinal - 1u]
                                   .launch_binding]
                 .owner_instruction ||
         join->owner_instruction <=
             program->bindings[plan->tasks[call_ordinal - 1u].join_binding]
                 .owner_instruction))
      return false;
    plan->tasks[call_ordinal] = (w_seed_cooperative0_task_proof){
        .task_id = (uint32_t)call_ordinal,
        .call_index = call_index,
        .function_index = call->callee_identity < program->identity_count
                              ? program->identities[call->callee_identity].target_index
                              : W_SEED_COOPERATIVE0_NONE,
        .launch_binding = launch_binding_index,
        .join_binding = launch->task_peer_binding,
        .frame_index = (uint32_t)call_ordinal,
        .yield_count = plan->tasks[call_ordinal].yield_count,
        .lexical_ordinal = (uint32_t)call_ordinal};
    w_seed_cooperative0_frame *frame = &plan->frames[call_ordinal];
    const w_seed_hir0_function *function =
        &program->functions[plan->tasks[call_ordinal].function_index];
    uint32_t first = 0u;
    uint32_t instruction_count_u32 = 0u;
    if (!function_instruction_span(
            program, plan->tasks[call_ordinal].function_index, &first,
            &instruction_count_u32))
      return false;
    const size_t instruction_count = (size_t)instruction_count_u32;
    frame->function_index = plan->tasks[call_ordinal].function_index;
    frame->first_instruction = first;
    frame->next_instruction = first;
    frame->instruction_count = instruction_count_u32;
    frame->parameter_count = function->parameter_count;
    size_t binding_count = 0u;
    for (size_t instruction = 0u; instruction < instruction_count; instruction += 1u) {
      const w_seed_hir0_instruction *item = &program->instructions[first + instruction];
      if (item->kind == W_SEED_HIR0_INSTRUCTION_BINDING) binding_count += 1u;
    }
    frame->binding_count = (uint32_t)binding_count;
    plan->yield_count += plan->tasks[call_ordinal].yield_count;
    launch_bindings_seen += 1u;
    join_bindings_seen += 1u;
  }
  if (launch_bindings_seen != 2u || join_bindings_seen != 2u ||
      plan->tasks[0].join_binding >= plan->tasks[1].join_binding ||
      program->bindings[plan->tasks[0].join_binding].owner_instruction >=
          program->bindings[plan->tasks[1].join_binding].owner_instruction ||
      last_join_instruction !=
          program->bindings[plan->tasks[1].join_binding].owner_instruction)
    return false;
  plan->task_count = W_SEED_COOPERATIVE0_MAX_TASKS;
  plan->frame_count = W_SEED_COOPERATIVE0_MAX_TASKS;
  plan->trace_event_count = 18u + 3u * plan->yield_count;
  return true;
}

static bool evaluate_task_arguments(const w_seed_hir0_program *program,
                                    const w_seed_cooperative0_task_proof *task,
                                    w_seed_cooperative0_frame *frame) {
  if (program == NULL || task == NULL || frame == NULL ||
      task->call_index >= program->call_count)
    return false;
  const w_seed_hir0_call *call = &program->calls[task->call_index];
  w_seed_cooperative0_frame root;
  (void)memset(&root, 0, sizeof(root));
  root.function_index = program->blocks[call->owner_block].owner_function;
  root.parameter_count = 0u;
  for (size_t ordinal = 0u; ordinal < call->argument_count; ordinal += 1u) {
    const w_seed_hir0_argument *argument =
        &program->arguments[(size_t)call->first_argument + ordinal];
    if (argument->parameter_ordinal >= W_SEED_COOPERATIVE0_MAX_FRAME_PARAMETERS ||
        !evaluate_value(program, argument->value_index, &root,
                        &frame->parameters[argument->parameter_ordinal], 0u))
      return false;
  }
  return true;
}

static bool run_task_slice(cooperative_run *run, uint32_t task_id) {
  if (run == NULL || run->plan == NULL || task_id >= run->plan->task_count)
    return false;
  w_seed_cooperative0_frame *frame = &run->plan->frames[task_id];
  const w_seed_hir0_program *program = run->program;
  uint32_t end_u32 = 0u;
  if (!instruction_span_end(frame->first_instruction, frame->instruction_count,
                            &end_u32))
    return false;
  const size_t end = (size_t)end_u32;
  while (frame->next_instruction < end) {
    const w_seed_hir0_instruction *instruction =
        &program->instructions[frame->next_instruction];
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS ||
          instruction->binding_index >= program->binding_count ||
          !evaluate_value(program,
                          program->bindings[instruction->binding_index]
                              .initializer_value,
                          frame, &frame->bindings[instruction->binding_index],
                          0u))
        return false;
      frame->binding_initialized[instruction->binding_index] = true;
      frame->next_instruction += 1u;
      continue;
    }
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (instruction->call_index >= program->call_count) return false;
      const w_seed_hir0_call *call = &program->calls[instruction->call_index];
      if (host_print_call(program, call)) {
        if (!append_print_call(program, call, frame, run->stdout_bytes,
                               run->stdout_capacity, &run->stdout_count))
          return false;
      } else {
        w_seed_cooperative0_value ignored;
        if (!evaluate_call(program, instruction->call_index, frame, &ignored,
                           0u))
          return false;
      }
      frame->next_instruction += 1u;
      continue;
    }
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) {
      const uint32_t pc = frame->next_instruction;
      frame->next_instruction += 1u;
      frame->yield_count += 1u;
      const uint32_t before[W_SEED_COOPERATIVE0_MAX_QUEUE] = {
          run->queue_count > 0u ? run->queue[0] : W_SEED_COOPERATIVE0_NONE,
          run->queue_count > 1u ? run->queue[1] : W_SEED_COOPERATIVE0_NONE};
      if (!queue_push(run, task_id) ||
          !trace_push(run, W_SEED_COOPERATIVE0_EVENT_YIELD, task_id, before,
                      run->queue_count - 1u, task_id, pc,
                      frame->next_instruction, frame->yield_count, NULL))
        return false;
      return true;
    }
    return false;
  }
  const w_seed_hir0_function *function = &program->functions[frame->function_index];
  const w_seed_hir0_block *block = &program->blocks[function->first_block];
  if (block->terminator_index >= program->terminator_count) return false;
  const w_seed_hir0_terminator *terminator =
      &program->terminators[block->terminator_index];
  if (terminator->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      terminator->value_index >= program->value_count ||
      !evaluate_value(program, terminator->value_index, frame, &frame->result,
                      0u))
    return false;
  frame->settled = true;
  const uint32_t before[W_SEED_COOPERATIVE0_MAX_QUEUE] = {
      run->queue_count > 0u ? run->queue[0] : W_SEED_COOPERATIVE0_NONE,
      run->queue_count > 1u ? run->queue[1] : W_SEED_COOPERATIVE0_NONE};
  if (!trace_push(run, W_SEED_COOPERATIVE0_EVENT_SETTLE, task_id, before,
                  run->queue_count, task_id, frame->next_instruction,
                  frame->next_instruction, frame->yield_count, &frame->result))
    return false;
  frame->cleaned = true;
  if (!trace_push(run, W_SEED_COOPERATIVE0_EVENT_CLEANUP, task_id, before,
                  run->queue_count, task_id, frame->next_instruction,
                  frame->next_instruction, frame->yield_count, &frame->result))
    return false;
  frame->outcome_committed = true;
  return trace_push(run, W_SEED_COOPERATIVE0_EVENT_OUTCOME_COMMIT, task_id,
                    before, run->queue_count, task_id, frame->next_instruction,
                    frame->next_instruction, frame->yield_count,
                    &frame->result);
}

static bool execute_plan(const w_seed_hir0_program *program,
                          const w_seed_cooperative0_plan *plan,
                          uint8_t *stdout_bytes, size_t stdout_capacity,
                          w_seed_cooperative0_trace_event *events,
                          size_t *stdout_count, size_t *event_count,
                          w_seed_cooperative0_execution_state *final_state) {
  if (program == NULL || plan == NULL || stdout_bytes == NULL || events == NULL ||
      stdout_count == NULL || event_count == NULL || final_state == NULL)
    return false;
  /* Keep the published proof immutable. Runtime state belongs to this fixed
   * stack copy; result.plan remains a pristine independently verifiable
   * admission record while the trace carries the actual transitions. */
  w_seed_cooperative0_plan execution_plan = *plan;
  cooperative_run run = {.program = program,
                          .plan = &execution_plan,
                          .events = events,
                          .stdout_bytes = stdout_bytes,
                          .stdout_capacity = stdout_capacity};
  for (size_t task = 0u; task < plan->task_count; task += 1u) {
    uint32_t before[W_SEED_COOPERATIVE0_MAX_QUEUE] = {
        run.queue_count > 0u ? run.queue[0] : W_SEED_COOPERATIVE0_NONE,
        run.queue_count > 1u ? run.queue[1] : W_SEED_COOPERATIVE0_NONE};
    if (!trace_push(&run, W_SEED_COOPERATIVE0_EVENT_RESERVE, (uint32_t)task,
                    before, run.queue_count, (uint32_t)task,
                    execution_plan.frames[task].first_instruction,
                    execution_plan.frames[task].next_instruction, 0u, NULL)) {
      return false;
    }
  }
  for (size_t task = 0u; task < plan->task_count; task += 1u) {
    if (!evaluate_task_arguments(program, &execution_plan.tasks[task],
                                 &execution_plan.frames[task])) {
      return false;
    }
    uint32_t before[W_SEED_COOPERATIVE0_MAX_QUEUE] = {
        run.queue_count > 0u ? run.queue[0] : W_SEED_COOPERATIVE0_NONE,
        run.queue_count > 1u ? run.queue[1] : W_SEED_COOPERATIVE0_NONE};
    if (!queue_push(&run, (uint32_t)task)) {
      return false;
    }
    if (!trace_push(&run, W_SEED_COOPERATIVE0_EVENT_PUBLISH, (uint32_t)task,
                    before, run.queue_count - 1u, (uint32_t)task,
                    execution_plan.frames[task].first_instruction,
                    execution_plan.frames[task].next_instruction, 0u, NULL)) {
      return false;
    }
  }
  for (size_t join = 0u; join < execution_plan.task_count; join += 1u) {
    const uint32_t task_id = execution_plan.tasks[join].task_id;
    while (!execution_plan.frames[task_id].settled) {
      uint32_t before[W_SEED_COOPERATIVE0_MAX_QUEUE];
      size_t before_count = 0u;
      uint32_t selected = W_SEED_COOPERATIVE0_NONE;
      if (!queue_pop(&run, &selected, before, &before_count) ||
          selected >= plan->task_count) {
        return false;
      }
      if (!trace_push(&run, W_SEED_COOPERATIVE0_EVENT_DISPATCH, selected,
                      before, before_count, selected,
                      execution_plan.frames[selected].next_instruction,
                      execution_plan.frames[selected].next_instruction,
                      execution_plan.frames[selected].yield_count, NULL)) {
        return false;
      }
      const uint32_t resume_before[W_SEED_COOPERATIVE0_MAX_QUEUE] = {
          run.queue_count > 0u ? run.queue[0] : W_SEED_COOPERATIVE0_NONE,
          run.queue_count > 1u ? run.queue[1] : W_SEED_COOPERATIVE0_NONE};
      if (!trace_push(&run, W_SEED_COOPERATIVE0_EVENT_RESUME, selected,
                      resume_before, run.queue_count, selected,
                      execution_plan.frames[selected].next_instruction,
                      execution_plan.frames[selected].next_instruction,
                      execution_plan.frames[selected].yield_count, NULL)) {
        return false;
      }
      if (!run_task_slice(&run, selected)) {
        return false;
      }
    }
    const uint32_t before[W_SEED_COOPERATIVE0_MAX_QUEUE] = {
        run.queue_count > 0u ? run.queue[0] : W_SEED_COOPERATIVE0_NONE,
        run.queue_count > 1u ? run.queue[1] : W_SEED_COOPERATIVE0_NONE};
    if (!trace_push(&run, W_SEED_COOPERATIVE0_EVENT_JOIN, task_id, before,
                    run.queue_count, task_id,
                    execution_plan.frames[task_id].next_instruction,
                    execution_plan.frames[task_id].next_instruction,
                    execution_plan.frames[task_id].yield_count,
                    &execution_plan.frames[task_id].result)) {
      return false;
    }
    if (!trace_push(&run, W_SEED_COOPERATIVE0_EVENT_RELEASE, task_id, before,
                    run.queue_count, task_id,
                    execution_plan.frames[task_id].next_instruction,
                    execution_plan.frames[task_id].next_instruction,
                    execution_plan.frames[task_id].yield_count,
                    &execution_plan.frames[task_id].result)) {
      return false;
    }
  }
  /* Root joins are the only values made visible to the final root print. */
  w_seed_cooperative0_frame root;
  (void)memset(&root, 0, sizeof(root));
  root.function_index = program->entries[0].target_function;
  for (size_t task = 0u; task < execution_plan.task_count; task += 1u) {
    const uint32_t binding = execution_plan.tasks[task].join_binding;
    if (binding >= W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS) return false;
    root.binding_initialized[binding] = true;
    root.bindings[binding] = execution_plan.frames[task].result;
  }
  const w_seed_hir0_function *root_function = &program->functions[root.function_index];
  const w_seed_hir0_block *root_block = &program->blocks[root_function->first_block];
  for (size_t ordinal = 0u; ordinal < root_block->instruction_count; ordinal += 1u) {
    const w_seed_hir0_instruction *instruction =
        &program->instructions[(size_t)root_block->first_instruction + ordinal];
    if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL) continue;
    if (instruction->call_index >= program->call_count) return false;
    const w_seed_hir0_call *call = &program->calls[instruction->call_index];
    if (call->execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_COOPERATIVE_TRACE ||
        call->execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH)
      continue;
    if (!append_print_call(program, call, &root, stdout_bytes, stdout_capacity,
                           &run.stdout_count)) {
      return false;
    }
  }
  *stdout_count = run.stdout_count;
  *event_count = run.event_count;
  (void)memset(final_state, 0, sizeof(*final_state));
  (void)memcpy(final_state->schema, W_SEED_COOPERATIVE0_SCHEMA_VERSION,
               sizeof(final_state->schema));
  final_state->phase = W_SEED_COOPERATIVE0_STATE_EXECUTED;
  final_state->task_count = execution_plan.task_count;
  final_state->frame_count = execution_plan.frame_count;
  final_state->trace_event_count = (uint32_t)run.event_count;
  (void)memcpy(final_state->hir_semantic_digest,
               execution_plan.hir_semantic_digest,
               sizeof(final_state->hir_semantic_digest));
  final_state->execution_profile = execution_plan.execution_profile;
  (void)memcpy(final_state->frames, execution_plan.frames,
               sizeof(final_state->frames));
  return true;
}

static bool replay_frame_to(const w_seed_hir0_program *program,
                            const w_seed_cooperative0_plan *plan,
                            uint32_t task_id, uint32_t target_pc,
                            uint32_t target_yield_count, bool settled,
                            bool cleaned, bool outcome_committed,
                            w_seed_cooperative0_frame *frame) {
  if (program == NULL || plan == NULL || frame == NULL ||
      task_id >= W_SEED_COOPERATIVE0_MAX_TASKS ||
      target_yield_count > W_SEED_COOPERATIVE0_MAX_YIELDS_PER_TASK)
    return false;
  *frame = plan->frames[task_id];
  uint32_t frame_end = 0u;
  if (!evaluate_task_arguments(program, &plan->tasks[task_id], frame) ||
      !instruction_span_end(frame->first_instruction, frame->instruction_count,
                            &frame_end) ||
      target_pc < frame->first_instruction || target_pc > frame_end)
    return false;
  while (frame->next_instruction < target_pc) {
    const w_seed_hir0_program *hir = program;
    const w_seed_hir0_instruction *instruction =
        &hir->instructions[frame->next_instruction];
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= program->binding_count ||
          instruction->binding_index >= W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS ||
          !evaluate_value(program,
                          program->bindings[instruction->binding_index]
                              .initializer_value,
                          frame, &frame->bindings[instruction->binding_index],
                          0u))
        return false;
      frame->binding_initialized[instruction->binding_index] = true;
      frame->next_instruction += 1u;
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (instruction->call_index >= program->call_count ||
          host_print_call(program, &program->calls[instruction->call_index]))
        return false;
      w_seed_cooperative0_value ignored;
      if (!evaluate_call(program, instruction->call_index, frame, &ignored, 0u))
        return false;
      frame->next_instruction += 1u;
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) {
      frame->next_instruction += 1u;
      frame->yield_count += 1u;
      if (frame->yield_count > W_SEED_COOPERATIVE0_MAX_YIELDS_PER_TASK)
        return false;
    } else {
      return false;
    }
  }
  if (frame->next_instruction != target_pc ||
      frame->yield_count != target_yield_count)
    return false;
  if (settled || cleaned || outcome_committed) {
    if (!settled || (outcome_committed && !cleaned) ||
        target_pc != frame_end)
      return false;
    const w_seed_hir0_function *function =
        &program->functions[frame->function_index];
    if (function->first_block >= program->block_count)
      return false;
    const w_seed_hir0_block *block = &program->blocks[function->first_block];
    if (block->terminator_index >= program->terminator_count ||
        program->terminators[block->terminator_index].kind !=
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
        !evaluate_value(program,
                        program->terminators[block->terminator_index].value_index,
                        frame, &frame->result, 0u))
      return false;
  }
  frame->settled = settled;
  frame->cleaned = cleaned;
  frame->outcome_committed = outcome_committed;
  return true;
}

static bool queue_equal(const uint32_t *left, size_t left_count,
                        const uint32_t *right, size_t right_count) {
  return left != NULL && right != NULL &&
         left_count <= W_SEED_COOPERATIVE0_MAX_QUEUE &&
         right_count <= W_SEED_COOPERATIVE0_MAX_QUEUE &&
         left_count == right_count &&
         (left_count == 0u || memcmp(left, right, left_count * sizeof(*left)) == 0);
}

static bool digest_zero(const uint8_t digest[32]) {
  if (digest == NULL) return false;
  for (size_t index = 0u; index < 32u; index += 1u)
    if (digest[index] != 0u) return false;
  return true;
}

static bool event_has_outcome(w_seed_cooperative0_event_kind kind) {
  return kind == W_SEED_COOPERATIVE0_EVENT_SETTLE ||
         kind == W_SEED_COOPERATIVE0_EVENT_CLEANUP ||
         kind == W_SEED_COOPERATIVE0_EVENT_OUTCOME_COMMIT ||
         kind == W_SEED_COOPERATIVE0_EVENT_JOIN ||
         kind == W_SEED_COOPERATIVE0_EVENT_RELEASE;
}

static bool verify_trace_event(
    const w_seed_hir0_program *program,
    const w_seed_cooperative0_trace_event *event, uint32_t sequence,
    w_seed_cooperative0_event_kind kind, uint32_t task_id, uint32_t pc,
    uint32_t next_pc, uint32_t yield_ordinal,
    const uint32_t before[W_SEED_COOPERATIVE0_MAX_QUEUE], size_t before_count,
    const uint32_t after[W_SEED_COOPERATIVE0_MAX_QUEUE], size_t after_count,
    const w_seed_cooperative0_frame *frame) {
  if (program == NULL || event == NULL || before == NULL || after == NULL ||
      frame == NULL || before_count > W_SEED_COOPERATIVE0_MAX_QUEUE ||
      after_count > W_SEED_COOPERATIVE0_MAX_QUEUE ||
      event->sequence != sequence || event->kind != kind ||
      event->task_id != task_id || event->frame_slot != task_id ||
      event->pc != pc || event->next_pc != next_pc ||
      event->yield_ordinal != yield_ordinal ||
      !queue_equal(event->queue_before, event->queue_before_count, before,
                   before_count) ||
      !queue_equal(event->queue_after, event->queue_after_count, after,
                   after_count))
    return false;
  for (size_t index = 0u; index < after_count; index += 1u)
    for (size_t other = index + 1u; other < after_count; other += 1u)
      if (after[index] == after[other]) return false;
  for (size_t index = 0u; index < before_count; index += 1u)
    if (before[index] >= W_SEED_COOPERATIVE0_MAX_TASKS) return false;
  for (size_t index = 0u; index < after_count; index += 1u)
    if (after[index] >= W_SEED_COOPERATIVE0_MAX_TASKS) return false;
  uint8_t expected_frame_digest[32];
  if (!frame_digest(program, frame, expected_frame_digest) ||
      memcmp(event->frame_digest, expected_frame_digest,
             sizeof(expected_frame_digest)) != 0)
    return false;
  if (!event_has_outcome(kind)) return digest_zero(event->outcome_digest);
  uint8_t expected_outcome_digest[32];
  if (!value_digest(program, &frame->result, expected_outcome_digest) ||
      memcmp(event->outcome_digest, expected_outcome_digest,
             sizeof(expected_outcome_digest)) != 0)
    return false;
  return true;
}

static bool frame_equal(const w_seed_cooperative0_frame *left,
                        const w_seed_cooperative0_frame *right) {
  if (left == NULL || right == NULL ||
      left->function_index != right->function_index ||
      left->next_instruction != right->next_instruction ||
      left->first_instruction != right->first_instruction ||
      left->instruction_count != right->instruction_count ||
      left->parameter_count != right->parameter_count ||
      left->binding_count != right->binding_count ||
      left->yield_count != right->yield_count || left->settled != right->settled ||
      left->cleaned != right->cleaned ||
      left->outcome_committed != right->outcome_committed ||
      memcmp(left->reserved, right->reserved, sizeof(left->reserved)) != 0 ||
      memcmp(left->parameters, right->parameters, sizeof(left->parameters)) != 0 ||
      memcmp(left->bindings, right->bindings, sizeof(left->bindings)) != 0 ||
      memcmp(left->binding_initialized, right->binding_initialized,
             sizeof(left->binding_initialized)) != 0 ||
      memcmp(&left->result, &right->result, sizeof(left->result)) != 0)
    return false;
  return true;
}

bool w_seed_cooperative0_verify_trace(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative0_plan *plan,
    const w_seed_cooperative0_trace_event *trace, size_t trace_count) {
  if (program == NULL || hir_result == NULL || plan == NULL || trace == NULL ||
      trace_count > W_SEED_COOPERATIVE0_MAX_TRACE_EVENTS ||
      !w_seed_cooperative0_verify_plan(program, hir_result, plan) ||
      trace_count != plan->trace_event_count)
    return false;
  uint32_t yield_pc[W_SEED_COOPERATIVE0_MAX_TASKS]
                    [W_SEED_COOPERATIVE0_MAX_YIELDS_PER_TASK];
  uint32_t yield_next[W_SEED_COOPERATIVE0_MAX_TASKS]
                       [W_SEED_COOPERATIVE0_MAX_YIELDS_PER_TASK];
  uint32_t final_pc[W_SEED_COOPERATIVE0_MAX_TASKS];
  for (size_t task = 0u; task < W_SEED_COOPERATIVE0_MAX_TASKS; task += 1u) {
    const w_seed_cooperative0_frame *frame = &plan->frames[task];
    if (frame->first_instruction > program->instruction_count ||
        frame->instruction_count >
            program->instruction_count - frame->first_instruction ||
        !instruction_span_end(frame->first_instruction,
                              frame->instruction_count, &final_pc[task]))
      return false;
    size_t yields = 0u;
    for (size_t ordinal = 0u; ordinal < frame->instruction_count; ordinal += 1u) {
      const uint32_t pc = frame->first_instruction + (uint32_t)ordinal;
      if (program->instructions[pc].kind ==
          W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) {
        if (yields >= W_SEED_COOPERATIVE0_MAX_YIELDS_PER_TASK) return false;
        yield_pc[task][yields] = pc;
        yield_next[task][yields] = pc + 1u;
        yields += 1u;
      }
    }
    if (yields != plan->tasks[task].yield_count) return false;
  }

  uint32_t queue[W_SEED_COOPERATIVE0_MAX_QUEUE] = {0u, 0u};
  size_t queue_count = 0u;
  bool settled[W_SEED_COOPERATIVE0_MAX_TASKS] = {false, false};
  uint32_t progress[W_SEED_COOPERATIVE0_MAX_TASKS] = {0u, 0u};
  size_t event_index = 0u;
  uint32_t sequence = 1u;
  for (size_t task = 0u; task < W_SEED_COOPERATIVE0_MAX_TASKS; task += 1u) {
    uint32_t before[W_SEED_COOPERATIVE0_MAX_QUEUE] = {0u, 0u};
    uint32_t after[W_SEED_COOPERATIVE0_MAX_QUEUE] = {0u, 0u};
    (void)memcpy(before, queue, sizeof(before));
    (void)memcpy(after, queue, sizeof(after));
    const w_seed_cooperative0_frame *expected = &plan->frames[task];
    if (event_index >= trace_count ||
        !verify_trace_event(program, &trace[event_index++], sequence++,
                            W_SEED_COOPERATIVE0_EVENT_RESERVE, (uint32_t)task,
                            plan->frames[task].first_instruction,
                            plan->frames[task].first_instruction, 0u, before,
                            queue_count, after, queue_count, expected))
      return false;
  }
  for (size_t task = 0u; task < W_SEED_COOPERATIVE0_MAX_TASKS; task += 1u) {
    uint32_t before[W_SEED_COOPERATIVE0_MAX_QUEUE] = {0u, 0u};
    uint32_t after[W_SEED_COOPERATIVE0_MAX_QUEUE] = {0u, 0u};
    (void)memcpy(before, queue, sizeof(before));
    if (queue_count >= W_SEED_COOPERATIVE0_MAX_QUEUE) return false;
    queue[queue_count] = (uint32_t)task;
    queue_count += 1u;
    (void)memcpy(after, queue, sizeof(after));
    w_seed_cooperative0_frame expected;
    if (event_index >= trace_count ||
        !replay_frame_to(program, plan, (uint32_t)task,
                         plan->frames[task].first_instruction, 0u, false, false,
                         false, &expected) ||
        !verify_trace_event(program, &trace[event_index++], sequence++,
                            W_SEED_COOPERATIVE0_EVENT_PUBLISH, (uint32_t)task,
                            plan->frames[task].first_instruction,
                            plan->frames[task].first_instruction, 0u, before,
                            queue_count - 1u, after, queue_count, &expected))
      return false;
  }

  for (size_t join = 0u; join < W_SEED_COOPERATIVE0_MAX_TASKS; join += 1u) {
    const uint32_t join_task = (uint32_t)join;
    while (!settled[join]) {
      if (queue_count == 0u) return false;
      uint32_t before[W_SEED_COOPERATIVE0_MAX_QUEUE] = {0u, 0u};
      uint32_t after[W_SEED_COOPERATIVE0_MAX_QUEUE] = {0u, 0u};
      (void)memcpy(before, queue, sizeof(before));
      const uint32_t task = queue[0];
      for (size_t index = 1u; index < queue_count; index += 1u)
        queue[index - 1u] = queue[index];
      queue_count -= 1u;
      (void)memcpy(after, queue, sizeof(after));
      if (task >= W_SEED_COOPERATIVE0_MAX_TASKS || settled[task]) return false;
      const uint32_t pc = progress[task] == 0u
                              ? plan->frames[task].first_instruction
                              : progress[task] <=
                                        W_SEED_COOPERATIVE0_MAX_YIELDS_PER_TASK
                                    ? yield_next[task][progress[task] - 1u]
                                    : final_pc[task];
      w_seed_cooperative0_frame expected;
      if (event_index >= trace_count ||
          !replay_frame_to(program, plan, task, pc, progress[task], false,
                           false, false, &expected) ||
          !verify_trace_event(program, &trace[event_index++], sequence++,
                              W_SEED_COOPERATIVE0_EVENT_DISPATCH, task, pc, pc,
                              progress[task], before, queue_count + 1u,
                              after, queue_count, &expected))
        return false;
      /* The dispatch queue-before is the queue before the pop. Keep its count
       * explicit because the copied array is intentionally fixed-width. */

      uint32_t resume_before[W_SEED_COOPERATIVE0_MAX_QUEUE] = {0u, 0u};
      (void)memcpy(resume_before, queue, sizeof(resume_before));
      if (event_index >= trace_count ||
          !verify_trace_event(program, &trace[event_index++], sequence++,
                              W_SEED_COOPERATIVE0_EVENT_RESUME, task, pc, pc,
                              progress[task], resume_before, queue_count,
                              resume_before, queue_count, &expected))
        return false;
      if (progress[task] < plan->tasks[task].yield_count) {
        const uint32_t ordinal = progress[task];
        const uint32_t yield_pc_value = yield_pc[task][ordinal];
        const uint32_t next_pc_value = yield_next[task][ordinal];
        if (!replay_frame_to(program, plan, task, next_pc_value, ordinal + 1u,
                             false, false, false, &expected))
          return false;
        if (queue_count >= W_SEED_COOPERATIVE0_MAX_QUEUE) return false;
        (void)memcpy(before, queue, sizeof(before));
        queue[queue_count] = task;
        queue_count += 1u;
        (void)memcpy(after, queue, sizeof(after));
        if (event_index >= trace_count ||
            !verify_trace_event(program, &trace[event_index++], sequence++,
                                W_SEED_COOPERATIVE0_EVENT_YIELD, task,
                                yield_pc_value, next_pc_value, ordinal + 1u,
                                before, queue_count - 1u, after, queue_count,
                                &expected))
          return false;
        progress[task] += 1u;
      } else {
        if (!replay_frame_to(program, plan, task, final_pc[task],
                             progress[task], true, false, false, &expected) ||
            event_index >= trace_count ||
            !verify_trace_event(program, &trace[event_index++], sequence++,
                                W_SEED_COOPERATIVE0_EVENT_SETTLE, task,
                                final_pc[task], final_pc[task], progress[task],
                                queue, queue_count, queue, queue_count,
                                &expected) ||
            !replay_frame_to(program, plan, task, final_pc[task],
                             progress[task], true, true, false, &expected) ||
            event_index >= trace_count ||
            !verify_trace_event(program, &trace[event_index++], sequence++,
                                W_SEED_COOPERATIVE0_EVENT_CLEANUP, task,
                                final_pc[task], final_pc[task], progress[task],
                                queue, queue_count, queue, queue_count,
                                &expected) ||
            !replay_frame_to(program, plan, task, final_pc[task],
                             progress[task], true, true, true, &expected) ||
            event_index >= trace_count ||
            !verify_trace_event(program, &trace[event_index++], sequence++,
                                W_SEED_COOPERATIVE0_EVENT_OUTCOME_COMMIT, task,
                                final_pc[task], final_pc[task], progress[task],
                                queue, queue_count, queue, queue_count,
                                &expected))
          return false;
        settled[task] = true;
      }
    }
    w_seed_cooperative0_frame expected;
    if (!replay_frame_to(program, plan, join_task, final_pc[join_task],
                         plan->tasks[join_task].yield_count, true, true, true,
                         &expected) ||
        event_index >= trace_count ||
        !verify_trace_event(program, &trace[event_index++], sequence++,
                            W_SEED_COOPERATIVE0_EVENT_JOIN, join_task,
                            final_pc[join_task], final_pc[join_task],
                            plan->tasks[join_task].yield_count, queue, queue_count,
                            queue, queue_count, &expected) ||
        event_index >= trace_count ||
        !verify_trace_event(program, &trace[event_index++], sequence++,
                            W_SEED_COOPERATIVE0_EVENT_RELEASE, join_task,
                            final_pc[join_task], final_pc[join_task],
                            plan->tasks[join_task].yield_count, queue, queue_count,
                            queue, queue_count, &expected))
      return false;
  }
  return event_index == trace_count && queue_count == 0u && settled[0] &&
         settled[1];
}

bool w_seed_cooperative0_verify_execution(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative0_plan *plan,
    const w_seed_cooperative0_trace_event *trace, size_t trace_count,
    const w_seed_cooperative0_execution_state *final_state) {
  if (program == NULL || hir_result == NULL || plan == NULL ||
      final_state == NULL || !w_seed_cooperative0_verify_trace(
                                 program, hir_result, plan, trace, trace_count) ||
      memcmp(final_state->schema, W_SEED_COOPERATIVE0_SCHEMA_VERSION,
             sizeof(final_state->schema)) != 0 ||
      final_state->phase != W_SEED_COOPERATIVE0_STATE_EXECUTED ||
      final_state->task_count != W_SEED_COOPERATIVE0_MAX_TASKS ||
      final_state->frame_count != W_SEED_COOPERATIVE0_MAX_TASKS ||
      final_state->trace_event_count != trace_count ||
      memcmp(final_state->hir_semantic_digest, hir_result->semantic_digest,
             sizeof(final_state->hir_semantic_digest)) != 0 ||
      final_state->execution_profile != plan->execution_profile ||
      memcmp(final_state->reserved, "\0\0\0", sizeof(final_state->reserved)) !=
          0)
    return false;
  for (size_t task = 0u; task < W_SEED_COOPERATIVE0_MAX_TASKS; task += 1u) {
    w_seed_cooperative0_frame expected;
    const w_seed_cooperative0_frame *actual = &final_state->frames[task];
    uint32_t end = 0u;
    if (!instruction_span_end(plan->frames[task].first_instruction,
                              plan->frames[task].instruction_count, &end))
      return false;
    if (!replay_frame_to(program, plan, (uint32_t)task, end,
                         plan->tasks[task].yield_count, true, true, true,
                         &expected) ||
        !frame_equal(actual, &expected))
      return false;
  }
  return true;
}

/* Reconstruct the root's post-join host output from the verified final
 * scalar values.  This is deliberately separate from execute_plan(): the
 * public output verifier must not accept a caller-forged stdout buffer merely
 * because its digest was changed to match. */
static bool reconstruct_root_stdout(
    const w_seed_hir0_program *program,
    const w_seed_cooperative0_plan *plan,
    const w_seed_cooperative0_execution_state *final_state, uint8_t *buffer,
    size_t capacity, size_t *written) {
  if (program == NULL || plan == NULL || final_state == NULL || buffer == NULL ||
      written == NULL || program->entry_count != 1u || program->entries == NULL ||
      program->functions == NULL || program->blocks == NULL ||
      program->instructions == NULL || program->calls == NULL)
    return false;
  if (program->entries[0].target_function >= program->function_count)
    return false;
  w_seed_cooperative0_frame root;
  (void)memset(&root, 0, sizeof(root));
  root.function_index = program->entries[0].target_function;
  for (size_t task = 0u; task < W_SEED_COOPERATIVE0_MAX_TASKS; task += 1u) {
    const uint32_t binding = plan->tasks[task].join_binding;
    if (binding >= W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS)
      return false;
    root.binding_initialized[binding] = true;
    root.bindings[binding] = final_state->frames[task].result;
  }
  const w_seed_hir0_function *function = &program->functions[root.function_index];
  if (function->block_count != 1u || function->first_block >= program->block_count)
    return false;
  const w_seed_hir0_block *block = &program->blocks[function->first_block];
  if (!range_valid(block->first_instruction, block->instruction_count,
                   program->instruction_count))
    return false;
  size_t offset = 0u;
  for (size_t ordinal = 0u; ordinal < block->instruction_count; ordinal += 1u) {
    const w_seed_hir0_instruction *instruction =
        &program->instructions[(size_t)block->first_instruction + ordinal];
    if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
        instruction->call_index >= program->call_count)
      continue;
    const w_seed_hir0_call *call = &program->calls[instruction->call_index];
    if (call->execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_COOPERATIVE_TRACE ||
        call->execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH)
      continue;
    if (!append_print_call(program, call, &root, buffer, capacity, &offset))
      return false;
  }
  *written = offset;
  return true;
}

static bool emit_artifact(const w_seed_cooperative0_plan *plan,
                          const w_seed_cooperative0_trace_event *events,
                          size_t event_count, uint8_t *artifact, size_t capacity,
                          size_t *written);
static bool output_shape_valid(const w_seed_cooperative0_output *output,
                               size_t *trace_bytes);
static bool output_aliases(const w_seed_cooperative0_output *output,
                           const w_seed_cooperative0_result *result);
static bool result_aliases_inputs(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const w_seed_cooperative0_output *output,
    const w_seed_cooperative0_result *result);
static bool output_overlaps_hir(const w_seed_hir0_program *program,
                                const w_seed_hir0_result *hir_result,
                                const w_seed_cooperative0_output *output);

bool w_seed_cooperative0_verify_output(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative0_output *output,
    const w_seed_cooperative0_result *result) {
  if (program == NULL || hir_result == NULL || output == NULL || result == NULL ||
      result->status != W_SEED_COOPERATIVE0_OK ||
      memcmp(result->schema, W_SEED_COOPERATIVE0_SCHEMA_VERSION,
             sizeof(result->schema)) != 0 ||
      memcmp(result->hir_semantic_digest, hir_result->semantic_digest,
             sizeof(result->hir_semantic_digest)) != 0 ||
      result->execution_profile != result->plan.execution_profile ||
      memcmp(result->reserved, "\0\0\0", sizeof(result->reserved)) != 0 ||
      result->written.artifact_bytes != result->required.artifact_bytes ||
      result->written.stdout_bytes != result->required.stdout_bytes ||
      result->written.trace_events != result->required.trace_events ||
      result->required.artifact_bytes > W_SEED_COOPERATIVE0_MAX_ARTIFACT_BYTES ||
      result->required.stdout_bytes > W_SEED_COOPERATIVE0_MAX_STDOUT_BYTES ||
      result->required.trace_events > W_SEED_COOPERATIVE0_MAX_TRACE_EVENTS)
    return false;
  size_t trace_bytes = 0u;
  if (!output_shape_valid(output, &trace_bytes) ||
      output->artifact_capacity < result->required.artifact_bytes ||
      output->stdout_capacity < result->required.stdout_bytes ||
      output->trace_capacity < result->required.trace_events ||
      (result->required.artifact_bytes != 0u && output->artifact == NULL) ||
      (result->required.stdout_bytes != 0u && output->stdout_bytes == NULL) ||
      (result->required.trace_events != 0u && output->trace == NULL) ||
      output_aliases(output, result) ||
      result_aliases_inputs(program, hir_result, output, result) ||
      output_overlaps_hir(program, hir_result, output) ||
      result->plan.trace_event_count != result->required.trace_events ||
      result->final_state.trace_event_count != result->required.trace_events ||
      !w_seed_cooperative0_verify_plan(program, hir_result, &result->plan) ||
      !w_seed_cooperative0_verify_execution(
          program, hir_result, &result->plan, output->trace,
          result->required.trace_events, &result->final_state))
    return false;

  uint8_t expected_artifact[W_SEED_COOPERATIVE0_MAX_ARTIFACT_BYTES];
  size_t expected_artifact_bytes = 0u;
  if (!emit_artifact(&result->plan, output->trace,
                     result->required.trace_events, expected_artifact,
                     sizeof(expected_artifact), &expected_artifact_bytes) ||
      expected_artifact_bytes != result->required.artifact_bytes ||
      memcmp(expected_artifact, output->artifact, expected_artifact_bytes) != 0)
    return false;

  uint8_t expected_stdout[W_SEED_COOPERATIVE0_MAX_STDOUT_BYTES];
  size_t expected_stdout_bytes = 0u;
  if (!reconstruct_root_stdout(
          program, &result->plan, &result->final_state, expected_stdout,
          sizeof(expected_stdout), &expected_stdout_bytes) ||
      expected_stdout_bytes != result->required.stdout_bytes ||
      memcmp(expected_stdout, output->stdout_bytes, expected_stdout_bytes) != 0)
    return false;

  uint8_t digest[32];
  w_seed_sha256_state hash;
  w_seed_sha256_init(&hash);
  w_seed_sha256_update(&hash, output->artifact,
                       result->required.artifact_bytes);
  w_seed_sha256_final(&hash, digest);
  if (memcmp(digest, result->artifact_digest, sizeof(digest)) != 0) return false;
  w_seed_sha256_init(&hash);
  w_seed_sha256_update(&hash, output->stdout_bytes,
                       result->required.stdout_bytes);
  w_seed_sha256_final(&hash, digest);
  return memcmp(digest, result->stdout_digest, sizeof(digest)) == 0;
}

static bool append_event_queue(uint8_t *buffer, size_t capacity, size_t *offset,
                               const uint32_t *queue, uint32_t count) {
  if (!append_literal(buffer, capacity, offset, "[")) return false;
  for (uint32_t index = 0u; index < count; index += 1u) {
    if ((index != 0u && !append_literal(buffer, capacity, offset, ",")) ||
        !append_u32(buffer, capacity, offset, queue[index]))
      return false;
  }
  return append_literal(buffer, capacity, offset, "]");
}

static const char *event_name(w_seed_cooperative0_event_kind kind) {
  switch (kind) {
    case W_SEED_COOPERATIVE0_EVENT_RESERVE:
      return "reserve";
    case W_SEED_COOPERATIVE0_EVENT_PUBLISH:
      return "publish";
    case W_SEED_COOPERATIVE0_EVENT_DISPATCH:
      return "dispatch";
    case W_SEED_COOPERATIVE0_EVENT_RESUME:
      return "resume";
    case W_SEED_COOPERATIVE0_EVENT_YIELD:
      return "yield";
    case W_SEED_COOPERATIVE0_EVENT_SETTLE:
      return "settle";
    case W_SEED_COOPERATIVE0_EVENT_CLEANUP:
      return "cleanup";
    case W_SEED_COOPERATIVE0_EVENT_OUTCOME_COMMIT:
      return "outcome_commit";
    case W_SEED_COOPERATIVE0_EVENT_JOIN:
      return "join";
    case W_SEED_COOPERATIVE0_EVENT_RELEASE:
      return "release";
    default:
      return NULL;
  }
}

static bool emit_artifact(const w_seed_cooperative0_plan *plan,
                          const w_seed_cooperative0_trace_event *events,
                          size_t event_count, uint8_t *artifact, size_t capacity,
                          size_t *written) {
  if (plan == NULL || events == NULL || artifact == NULL || written == NULL ||
      event_count > W_SEED_COOPERATIVE0_MAX_TRACE_EVENTS)
    return false;
  size_t offset = 0u;
  if (!append_literal(artifact, capacity, &offset,
                      W_SEED_COOPERATIVE0_SCHEMA_VERSION) ||
      !append_literal(artifact, capacity,
                      &offset, "\nexecution=compiler-oracle\nmode=single-thread-fifo-evidence\n") ||
      !append_literal(artifact, capacity, &offset, "hir_semantic_digest=") ||
      !append_hex(artifact, capacity, &offset, plan->hir_semantic_digest,
                  sizeof(plan->hir_semantic_digest)) ||
      !append_literal(artifact, capacity, &offset, " profile=") ||
      !append_u32(artifact, capacity, &offset,
                  (uint32_t)plan->execution_profile) ||
      !append_literal(artifact, capacity, &offset, "\n") ||
      !append_literal(artifact, capacity, &offset, "tasks=") ||
      !append_u32(artifact, capacity, &offset, plan->task_count) ||
      !append_literal(artifact, capacity, &offset, " frames=") ||
      !append_u32(artifact, capacity, &offset, plan->frame_count) ||
      !append_literal(artifact, capacity, &offset, " yields=") ||
      !append_u32(artifact, capacity, &offset, plan->yield_count) ||
      !append_literal(artifact, capacity, &offset,
                      " heap=forbidden os_threads=forbidden crt=forbidden\n"))
    return false;
  for (size_t index = 0u; index < event_count; index += 1u) {
    const char *name = event_name(events[index].kind);
    if (name == NULL || !append_literal(artifact, capacity, &offset, "seq=") ||
        !append_u32(artifact, capacity, &offset, events[index].sequence) ||
        !append_literal(artifact, capacity, &offset, " event=") ||
        !append_literal(artifact, capacity, &offset, name) ||
        !append_literal(artifact, capacity, &offset, " task=") ||
        !append_u32(artifact, capacity, &offset, events[index].task_id) ||
        !append_literal(artifact, capacity, &offset, " frame=") ||
        !append_u32(artifact, capacity, &offset, events[index].frame_slot) ||
        !append_literal(artifact, capacity, &offset, " pc=") ||
        !append_u32(artifact, capacity, &offset, events[index].pc) ||
        !append_literal(artifact, capacity, &offset, " next_pc=") ||
        !append_u32(artifact, capacity, &offset, events[index].next_pc) ||
        !append_literal(artifact, capacity, &offset, " yield=") ||
        !append_u32(artifact, capacity, &offset, events[index].yield_ordinal) ||
        !append_literal(artifact, capacity, &offset, " before=") ||
        !append_event_queue(artifact, capacity, &offset,
                            events[index].queue_before,
                            events[index].queue_before_count) ||
        !append_literal(artifact, capacity, &offset, " after=") ||
        !append_event_queue(artifact, capacity, &offset,
                            events[index].queue_after,
                            events[index].queue_after_count) ||
        !append_literal(artifact, capacity, &offset, " outcome=") ||
        !append_hex(artifact, capacity, &offset, events[index].outcome_digest,
                    sizeof(events[index].outcome_digest)) ||
        !append_literal(artifact, capacity, &offset, "\n"))
      return false;
  }
  *written = offset;
  return true;
}

static bool output_shape_valid(const w_seed_cooperative0_output *output,
                              size_t *trace_bytes) {
  if (output == NULL || trace_bytes == NULL ||
      output->artifact_capacity > W_SEED_COOPERATIVE0_MAX_ARTIFACT_BYTES ||
      output->stdout_capacity > W_SEED_COOPERATIVE0_MAX_STDOUT_BYTES ||
      output->trace_capacity > W_SEED_COOPERATIVE0_MAX_TRACE_EVENTS ||
      (output->artifact_capacity != 0u && output->artifact == NULL) ||
      (output->stdout_capacity != 0u && output->stdout_bytes == NULL) ||
      (output->trace_capacity != 0u && output->trace == NULL) ||
      output->trace_capacity > SIZE_MAX / sizeof(*output->trace))
    return false;
  *trace_bytes = output->trace_capacity * sizeof(*output->trace);
  return true;
}

static bool output_aliases(const w_seed_cooperative0_output *output,
                           const w_seed_cooperative0_result *result) {
  if (output == NULL || result == NULL) return true;
  size_t trace_bytes = 0u;
  if (!output_shape_valid(output, &trace_bytes)) return true;
  if (ranges_overlap(result, sizeof(*result), output->artifact,
                     output->artifact_capacity) ||
      ranges_overlap(result, sizeof(*result), output->stdout_bytes,
                     output->stdout_capacity) ||
      ranges_overlap(result, sizeof(*result), output->trace, trace_bytes) ||
      ranges_overlap(output->artifact, output->artifact_capacity,
                     output->stdout_bytes, output->stdout_capacity) ||
      ranges_overlap(output->artifact, output->artifact_capacity, output->trace,
                     trace_bytes) ||
      ranges_overlap(output->stdout_bytes, output->stdout_capacity, output->trace,
                     trace_bytes) ||
      ranges_overlap(output->artifact, output->artifact_capacity, output,
                     sizeof(*output)) ||
      ranges_overlap(output->stdout_bytes, output->stdout_capacity, output,
                     sizeof(*output)) ||
      ranges_overlap(output->trace, trace_bytes, output, sizeof(*output)))
    return true;
  return false;
}

static bool hir_ranges_overlap(const w_seed_hir0_program *program,
                               const void *address, size_t bytes);

static bool result_aliases_inputs(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const w_seed_cooperative0_output *output,
    const w_seed_cooperative0_result *result) {
  if (program == NULL || hir_result == NULL || output == NULL || result == NULL)
    return true;
  return hir_ranges_overlap(program, hir_result, sizeof(*hir_result)) ||
         hir_ranges_overlap(program, output, sizeof(*output)) ||
         hir_ranges_overlap(program, result, sizeof(*result)) ||
         ranges_overlap(output, sizeof(*output), hir_result,
                        sizeof(*hir_result)) ||
         ranges_overlap(result, sizeof(*result), hir_result,
                        sizeof(*hir_result)) ||
         ranges_overlap(result, sizeof(*result), output, sizeof(*output));
}

static bool measure_aliases_inputs(const w_seed_hir0_program *program,
                                   const w_seed_hir0_result *hir_result,
                                   const w_seed_cooperative0_counts *counts,
                                   const w_seed_cooperative0_result *result) {
  if (program == NULL || hir_result == NULL || counts == NULL || result == NULL)
    return true;
  return hir_ranges_overlap(program, hir_result, sizeof(*hir_result)) ||
         hir_ranges_overlap(program, counts, sizeof(*counts)) ||
         hir_ranges_overlap(program, result, sizeof(*result)) ||
         ranges_overlap(counts, sizeof(*counts), result, sizeof(*result)) ||
         ranges_overlap(counts, sizeof(*counts), hir_result,
                        sizeof(*hir_result)) ||
         ranges_overlap(result, sizeof(*result), hir_result,
                        sizeof(*hir_result));
}

/* Reusable complete HIR range table.  All callers use the written counts,
 * not capacities, and multiplication/endpoint overflow fails closed. */
static bool hir_ranges_overlap(const w_seed_hir0_program *program,
                               const void *address, size_t bytes) {
  if (program == NULL) return true;
  if (address == NULL || bytes == 0u) return false;
  if (ranges_overlap(address, bytes, program, sizeof(*program))) return true;
#define COOPERATIVE_HIR_RANGE(field, count_field, type)                       \
  do {                                                                        \
    if (program->count_field > SIZE_MAX / sizeof(type)) return true;          \
    if (ranges_overlap(address, bytes, program->field,                       \
                       program->count_field * sizeof(type)))                 \
      return true;                                                            \
  } while (false)
  COOPERATIVE_HIR_RANGE(modules, module_count, w_seed_hir0_module);
  COOPERATIVE_HIR_RANGE(identities, identity_count, w_seed_hir0_identity);
  COOPERATIVE_HIR_RANGE(types, type_count, w_seed_hir0_type);
  COOPERATIVE_HIR_RANGE(enums, enum_count, w_seed_hir0_enum);
  COOPERATIVE_HIR_RANGE(enum_cases, enum_case_count, w_seed_hir0_enum_case);
  COOPERATIVE_HIR_RANGE(enum_case_parameters, enum_case_parameter_count,
                        w_seed_hir0_enum_case_parameter);
  COOPERATIVE_HIR_RANGE(enum_subset_members, enum_subset_member_count,
                        w_seed_hir0_enum_subset_member);
  COOPERATIVE_HIR_RANGE(functions, function_count, w_seed_hir0_function);
  COOPERATIVE_HIR_RANGE(parameters, parameter_count, w_seed_hir0_parameter);
  COOPERATIVE_HIR_RANGE(blocks, block_count, w_seed_hir0_block);
  COOPERATIVE_HIR_RANGE(block_arguments, block_argument_count,
                        w_seed_hir0_block_argument);
  COOPERATIVE_HIR_RANGE(edge_arguments, edge_argument_count,
                        w_seed_hir0_edge_argument);
  COOPERATIVE_HIR_RANGE(switch_edges, switch_edge_count,
                        w_seed_hir0_switch_edge);
  COOPERATIVE_HIR_RANGE(switch_captures, switch_capture_count,
                        w_seed_hir0_switch_capture);
  COOPERATIVE_HIR_RANGE(instructions, instruction_count,
                        w_seed_hir0_instruction);
  COOPERATIVE_HIR_RANGE(bindings, binding_count, w_seed_hir0_binding);
  COOPERATIVE_HIR_RANGE(calls, call_count, w_seed_hir0_call);
  COOPERATIVE_HIR_RANGE(host_parameters, host_parameter_count,
                        w_seed_hir0_host_parameter);
  COOPERATIVE_HIR_RANGE(arguments, argument_count, w_seed_hir0_argument);
  COOPERATIVE_HIR_RANGE(enum_payloads, enum_payload_count,
                        w_seed_hir0_enum_payload);
  COOPERATIVE_HIR_RANGE(requirements, requirement_count,
                        w_seed_hir0_requirement);
  COOPERATIVE_HIR_RANGE(values, value_count, w_seed_hir0_value);
  COOPERATIVE_HIR_RANGE(interpolation_segments, interpolation_segment_count,
                        w_seed_hir0_interpolation_segment);
  COOPERATIVE_HIR_RANGE(terminators, terminator_count,
                        w_seed_hir0_terminator);
  COOPERATIVE_HIR_RANGE(entries, entry_count, w_seed_hir0_entry);
  COOPERATIVE_HIR_RANGE(external_modules, external_module_count,
                        w_seed_hir0_external_module);
  COOPERATIVE_HIR_RANGE(external_symbols, external_symbol_count,
                        w_seed_hir0_external_symbol);
#undef COOPERATIVE_HIR_RANGE
  return ranges_overlap(address, bytes, program->text_bytes,
                        program->text_byte_count) ||
         ranges_overlap(address, bytes, program->value_bytes,
                        program->value_byte_count) ||
         ranges_overlap(address, bytes, program->receipt,
                        program->receipt_count);
}

static bool output_overlaps_hir(const w_seed_hir0_program *program,
                                const w_seed_hir0_result *hir_result,
                                const w_seed_cooperative0_output *output) {
  size_t trace_bytes = 0u;
  if (program == NULL || hir_result == NULL || output == NULL ||
      !output_shape_valid(output, &trace_bytes))
    return true;
  return hir_ranges_overlap(program, hir_result, sizeof(*hir_result)) ||
         hir_ranges_overlap(program, output, sizeof(*output)) ||
         hir_ranges_overlap(program, output->artifact,
                            output->artifact_capacity) ||
         hir_ranges_overlap(program, output->stdout_bytes,
                            output->stdout_capacity) ||
         hir_ranges_overlap(program, output->trace, trace_bytes) ||
         ranges_overlap(output->artifact, output->artifact_capacity,
                        hir_result, sizeof(*hir_result)) ||
         ranges_overlap(output->stdout_bytes, output->stdout_capacity,
                        hir_result, sizeof(*hir_result)) ||
         ranges_overlap(output->trace, trace_bytes, hir_result,
                        sizeof(*hir_result));
}

bool w_seed_cooperative0_verify_plan(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative0_plan *plan) {
  if (program == NULL || hir_result == NULL || plan == NULL ||
      !w_seed_hir0_verify(program, hir_result) ||
      memcmp(plan->schema, W_SEED_COOPERATIVE0_SCHEMA_VERSION,
             sizeof(plan->schema)) != 0 ||
      plan->phase != W_SEED_COOPERATIVE0_PLAN_INITIAL ||
      plan->task_count != W_SEED_COOPERATIVE0_MAX_TASKS ||
      plan->frame_count != W_SEED_COOPERATIVE0_MAX_TASKS ||
      plan->yield_count == 0u || plan->yield_count > 4u ||
      memcmp(plan->hir_semantic_digest, hir_result->semantic_digest,
             sizeof(plan->hir_semantic_digest)) != 0 ||
      (plan->execution_profile !=
           W_SEED_HIR0_EXECUTION_PROFILE_COOPERATIVE_TRACE &&
       plan->execution_profile != W_SEED_HIR0_EXECUTION_PROFILE_MAIN_SERIAL))
    return false;
  w_seed_cooperative0_plan expected;
  if (!plan_from_program(program, hir_result, &expected) ||
      expected.task_count != plan->task_count ||
      expected.frame_count != plan->frame_count ||
      expected.yield_count != plan->yield_count ||
      expected.trace_event_count != plan->trace_event_count ||
      memcmp(expected.hir_semantic_digest, plan->hir_semantic_digest,
             sizeof(expected.hir_semantic_digest)) != 0 ||
      expected.execution_profile != plan->execution_profile ||
      memcmp(expected.reserved, plan->reserved, sizeof(expected.reserved)) != 0)
    return false;
  for (size_t task = 0u; task < W_SEED_COOPERATIVE0_MAX_TASKS; task += 1u) {
    const w_seed_cooperative0_task_proof *actual = &plan->tasks[task];
    const w_seed_cooperative0_task_proof *wanted = &expected.tasks[task];
    if (memcmp(actual, wanted, sizeof(*actual)) != 0)
      return false;
    const w_seed_cooperative0_frame *frame = &plan->frames[task];
    const w_seed_cooperative0_frame *expected_frame = &expected.frames[task];
    if (frame->function_index != expected_frame->function_index ||
        frame->first_instruction != expected_frame->first_instruction ||
        frame->instruction_count != expected_frame->instruction_count ||
        frame->parameter_count != expected_frame->parameter_count ||
        frame->binding_count != expected_frame->binding_count ||
        frame->yield_count != 0u || frame->settled || frame->cleaned ||
        frame->outcome_committed || frame->next_instruction != frame->first_instruction ||
        memcmp(frame->reserved, expected_frame->reserved,
               sizeof(frame->reserved)) != 0 ||
        memcmp(frame->parameters, expected_frame->parameters,
               sizeof(frame->parameters)) != 0 ||
        memcmp(frame->bindings, expected_frame->bindings,
               sizeof(frame->bindings)) != 0 ||
        memcmp(frame->binding_initialized, expected_frame->binding_initialized,
               sizeof(frame->binding_initialized)) != 0 ||
        memcmp(&frame->result, &expected_frame->result,
               sizeof(frame->result)) != 0)
      return false;
  }
  return true;
}

w_seed_cooperative0_status w_seed_cooperative0_measure(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    w_seed_cooperative0_counts *counts, w_seed_cooperative0_result *result) {
  if (program == NULL || counts == NULL || result == NULL || hir_result == NULL)
    return W_SEED_COOPERATIVE0_INVALID;
  if (measure_aliases_inputs(program, hir_result, counts, result))
    return W_SEED_COOPERATIVE0_ALIAS;
  if (!w_seed_hir0_verify(program, hir_result)) return W_SEED_COOPERATIVE0_INVALID;
  w_seed_cooperative0_plan plan;
  if (!plan_from_program(program, hir_result, &plan))
    return W_SEED_COOPERATIVE0_UNSUPPORTED;
  uint8_t stdout_stage[W_SEED_COOPERATIVE0_MAX_STDOUT_BYTES];
  uint8_t artifact_stage[W_SEED_COOPERATIVE0_MAX_ARTIFACT_BYTES];
  w_seed_cooperative0_trace_event events[W_SEED_COOPERATIVE0_MAX_TRACE_EVENTS];
  w_seed_cooperative0_execution_state final_state;
  size_t stdout_count = 0u;
  size_t event_count = 0u;
  size_t artifact_count = 0u;
  if (!execute_plan(program, &plan, stdout_stage, sizeof(stdout_stage), events,
                    &stdout_count, &event_count, &final_state))
    return W_SEED_COOPERATIVE0_UNSUPPORTED;
  if (!emit_artifact(&plan, events, event_count, artifact_stage,
                     sizeof(artifact_stage), &artifact_count))
    return W_SEED_COOPERATIVE0_UNSUPPORTED;
  if (!w_seed_cooperative0_verify_execution(program, hir_result, &plan, events,
                                            event_count, &final_state))
    return W_SEED_COOPERATIVE0_UNSUPPORTED;
  *counts = (w_seed_cooperative0_counts){.artifact_bytes = artifact_count,
                                        .stdout_bytes = stdout_count,
                                        .trace_events = event_count};
  plan.trace_event_count = (uint32_t)event_count;
  w_seed_cooperative0_result candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  candidate.status = W_SEED_COOPERATIVE0_OK;
  candidate.required = *counts;
  candidate.plan = plan;
  candidate.final_state = final_state;
  (void)memcpy(candidate.hir_semantic_digest, hir_result->semantic_digest,
               sizeof(candidate.hir_semantic_digest));
  candidate.execution_profile = plan.execution_profile;
  (void)memcpy(candidate.schema, W_SEED_COOPERATIVE0_SCHEMA_VERSION,
              sizeof(candidate.schema));
  *result = candidate;
  return W_SEED_COOPERATIVE0_OK;
}

w_seed_cooperative0_status w_seed_cooperative0_run(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative0_output *output,
    w_seed_cooperative0_result *result) {
  if (program == NULL || output == NULL || result == NULL ||
      hir_result == NULL || !w_seed_hir0_verify(program, hir_result))
    return W_SEED_COOPERATIVE0_INVALID;
  size_t trace_bytes = 0u;
  if (!output_shape_valid(output, &trace_bytes))
    return W_SEED_COOPERATIVE0_CAPACITY;
  (void)trace_bytes;
  if (output_aliases(output, result) ||
      result_aliases_inputs(program, hir_result, output, result))
    return W_SEED_COOPERATIVE0_ALIAS;
  if (output_overlaps_hir(program, hir_result, output))
    return W_SEED_COOPERATIVE0_ALIAS;
  w_seed_cooperative0_plan plan;
  if (!plan_from_program(program, hir_result, &plan))
    return W_SEED_COOPERATIVE0_UNSUPPORTED;
  uint8_t stdout_stage[W_SEED_COOPERATIVE0_MAX_STDOUT_BYTES];
  uint8_t artifact_stage[W_SEED_COOPERATIVE0_MAX_ARTIFACT_BYTES];
  w_seed_cooperative0_trace_event events[W_SEED_COOPERATIVE0_MAX_TRACE_EVENTS];
  w_seed_cooperative0_execution_state final_state;
  size_t stdout_count = 0u;
  size_t event_count = 0u;
  if (!execute_plan(program, &plan, stdout_stage, sizeof(stdout_stage), events,
                    &stdout_count, &event_count, &final_state))
    return W_SEED_COOPERATIVE0_UNSUPPORTED;
  if (!w_seed_cooperative0_verify_execution(program, hir_result, &plan, events,
                                            event_count, &final_state))
    return W_SEED_COOPERATIVE0_UNSUPPORTED;
  size_t artifact_count = 0u;
  if (!emit_artifact(&plan, events, event_count, artifact_stage,
                     sizeof(artifact_stage), &artifact_count))
    return W_SEED_COOPERATIVE0_CAPACITY;
  if (output->artifact_capacity < artifact_count ||
      output->stdout_capacity < stdout_count ||
      output->trace_capacity < event_count)
    return W_SEED_COOPERATIVE0_CAPACITY;
  if (artifact_count != 0u && output->artifact == NULL) return W_SEED_COOPERATIVE0_CAPACITY;
  if (stdout_count != 0u && output->stdout_bytes == NULL) return W_SEED_COOPERATIVE0_CAPACITY;
  if (event_count != 0u && output->trace == NULL) return W_SEED_COOPERATIVE0_CAPACITY;
  if (artifact_count != 0u) (void)memcpy(output->artifact, artifact_stage, artifact_count);
  if (stdout_count != 0u) (void)memcpy(output->stdout_bytes, stdout_stage, stdout_count);
  if (event_count != 0u)
    (void)memcpy(output->trace, events,
                 event_count * sizeof(w_seed_cooperative0_trace_event));
  w_seed_cooperative0_result candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  candidate.status = W_SEED_COOPERATIVE0_OK;
  candidate.required = (w_seed_cooperative0_counts){
      .artifact_bytes = artifact_count,
      .stdout_bytes = stdout_count,
      .trace_events = event_count};
  candidate.written = candidate.required;
  (void)memcpy(candidate.schema, W_SEED_COOPERATIVE0_SCHEMA_VERSION,
              sizeof(candidate.schema));
  candidate.plan = plan;
  candidate.final_state = final_state;
  (void)memcpy(candidate.hir_semantic_digest, hir_result->semantic_digest,
               sizeof(candidate.hir_semantic_digest));
  candidate.execution_profile = plan.execution_profile;
  w_seed_sha256_state hash;
  w_seed_sha256_init(&hash);
  w_seed_sha256_update(&hash, artifact_stage, artifact_count);
  w_seed_sha256_final(&hash, candidate.artifact_digest);
  w_seed_sha256_init(&hash);
  w_seed_sha256_update(&hash, stdout_stage, stdout_count);
  w_seed_sha256_final(&hash, candidate.stdout_digest);
  *result = candidate;
  return W_SEED_COOPERATIVE0_OK;
}
