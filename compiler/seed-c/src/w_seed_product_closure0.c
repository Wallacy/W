#include "w_seed_product_closure0.h"

#include <limits.h>
#include <string.h>

#include "w_seed_sha256.h"

typedef struct {
  const void *address;
  size_t bytes;
} closure0_range;

typedef struct {
  bool modules[W_SEED_PRODUCT_CLOSURE0_MAX_MODULES];
  bool functions[W_SEED_PRODUCT_CLOSURE0_MAX_FUNCTIONS];
  bool identities[W_SEED_PRODUCT_CLOSURE0_MAX_IDENTITIES];
  bool types[W_SEED_PRODUCT_CLOSURE0_MAX_TYPES];
  bool values[W_SEED_PRODUCT_CLOSURE0_MAX_VALUES];
  bool requirements[W_SEED_PRODUCT_CLOSURE0_MAX_REQUIREMENTS];
  bool external_modules[W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_MODULES];
  bool external_symbols[W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_SYMBOLS];
  uint8_t identity_state[W_SEED_PRODUCT_CLOSURE0_MAX_IDENTITIES];
  uint8_t function_state[W_SEED_PRODUCT_CLOSURE0_MAX_FUNCTIONS];
  uint8_t value_state[W_SEED_PRODUCT_CLOSURE0_MAX_VALUES];
  uint32_t module_remap[W_SEED_PRODUCT_CLOSURE0_MAX_MODULES];
  uint32_t function_remap[W_SEED_PRODUCT_CLOSURE0_MAX_FUNCTIONS];
  uint32_t identity_remap[W_SEED_PRODUCT_CLOSURE0_MAX_IDENTITIES];
  uint32_t type_remap[W_SEED_PRODUCT_CLOSURE0_MAX_TYPES];
  uint32_t value_remap[W_SEED_PRODUCT_CLOSURE0_MAX_VALUES];
  uint32_t requirement_remap[W_SEED_PRODUCT_CLOSURE0_MAX_REQUIREMENTS];
  uint32_t external_module_remap[
      W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_MODULES];
  uint32_t external_symbol_remap[W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_SYMBOLS];
  w_seed_product_closure0_counts counts;
  w_seed_product_closure0_root root;
  uint8_t digest[W_SEED_PRODUCT_CLOSURE0_DIGEST_BYTES];
} closure0_plan;

static bool size_mul(size_t left, size_t right, size_t *out) {
  if (out == NULL || (right != 0u && left > SIZE_MAX / right)) return false;
  *out = left * right;
  return true;
}

static bool range_overlap(const void *left, size_t left_bytes,
                          const void *right, size_t right_bytes) {
  if (left == NULL || right == NULL || left_bytes == 0u || right_bytes == 0u)
    return false;
  const uintptr_t left_address = (uintptr_t)left;
  const uintptr_t right_address = (uintptr_t)right;
  if (left_address > UINTPTR_MAX - left_bytes ||
      right_address > UINTPTR_MAX - right_bytes)
    return true;
  return left_address < right_address + right_bytes &&
         right_address < left_address + left_bytes;
}

static bool add_range(closure0_range *ranges, size_t capacity,
                      size_t *count, const void *address, size_t elements,
                      size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity ||
      !size_mul(elements, element_size, &elements))
    return false;
  ranges[*count] = (closure0_range){address, elements};
  *count += 1u;
  return true;
}

static bool text_valid(const w_seed_hir0_program *program,
                       w_seed_hir0_text text) {
  return program != NULL && text.offset <= program->text_byte_count &&
         text.count <= program->text_byte_count - text.offset &&
         (text.count == 0u || program->text_bytes != NULL);
}

static bool text_is(const w_seed_hir0_program *program, w_seed_hir0_text text,
                    const char *literal) {
  if (literal == NULL) return false;
  const size_t length = strlen(literal);
  return text.count == length && text_valid(program, text) &&
         (length == 0u ||
          memcmp(program->text_bytes + text.offset, literal, length) == 0);
}

static bool host_print_identity_valid(const w_seed_hir0_program *program,
                                      uint32_t identity_index) {
  if (program == NULL || identity_index >= program->identity_count) return false;
  const size_t host_base = (size_t)program->module_count +
                           program->function_count + program->entry_count;
  if (identity_index < host_base) return false;
  const w_seed_hir0_identity *identity = &program->identities[identity_index];
  if (identity->kind != W_SEED_HIR0_IDENTITY_HOST_PRELUDE ||
      !text_is(program, identity->name, "print") ||
      !text_is(program, identity->profile, "native-process@1") ||
      identity->return_type != W_SEED_HIR0_TYPE_UNIT || identity->is_const ||
      identity->parameter_count != 1u || identity->requirement_count != 1u ||
      identity->first_parameter >= program->host_parameter_count ||
      identity->first_requirement >= program->requirement_count)
    return false;
  const w_seed_hir0_host_parameter *parameter =
      &program->host_parameters[identity->first_parameter];
  const w_seed_hir0_requirement *requirement =
      &program->requirements[identity->first_requirement];
  return parameter->owner_identity == identity_index &&
         parameter->ordinal == 0u && parameter->type_index ==
             W_SEED_HIR0_TYPE_STRING &&
         parameter->label_kind == W_SEED_HIR0_LABEL_POSITIONAL_ONLY &&
         parameter->label.count == 0u &&
         text_is(program, parameter->name, "message") &&
         requirement->owner_kind == W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY &&
         requirement->owner_index == identity_index && requirement->ordinal == 0u &&
         text_is(program, requirement->name, "Console");
}

static bool product_value_kind_supported(w_seed_hir0_value_kind kind) {
  switch (kind) {
    case W_SEED_HIR0_VALUE_CONST_STRING:
    case W_SEED_HIR0_VALUE_BINDING_READ:
    case W_SEED_HIR0_VALUE_PARAMETER_READ:
    case W_SEED_HIR0_VALUE_CONST_I64:
    case W_SEED_HIR0_VALUE_CONST_BOOL:
    case W_SEED_HIR0_VALUE_BINARY_I64:
    case W_SEED_HIR0_VALUE_INTERPOLATED_STRING:
    case W_SEED_HIR0_VALUE_CALL_RESULT:
    case W_SEED_HIR0_VALUE_UNARY_BOOL:
    case W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ:
    case W_SEED_HIR0_VALUE_UNARY_I64:
    case W_SEED_HIR0_VALUE_INTEGER_WIDEN:
      return true;
    case W_SEED_HIR0_VALUE_CONST_USIZE:
    case W_SEED_HIR0_VALUE_CONST_U64:
    case W_SEED_HIR0_VALUE_CONST_FLOAT:
    case W_SEED_HIR0_VALUE_BINARY_FLOAT:
    case W_SEED_HIR0_VALUE_UNARY_FLOAT:
    case W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE:
    case W_SEED_HIR0_VALUE_EXTERNAL_MEMBER:
    case W_SEED_HIR0_VALUE_ENUM_CASE:
    case W_SEED_HIR0_VALUE_PATTERN_CAPTURE_READ:
    case W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON:
    case W_SEED_HIR0_VALUE_BINARY_U64:
    case W_SEED_HIR0_VALUE_UNARY_U64:
    case W_SEED_HIR0_VALUE_TUPLE_ELEMENT:
      return false;
    case W_SEED_HIR0_VALUE_BINARY_INTEGER_COMPARISON:
      return true;
  }
  return false;
}

static bool product_terminator_kind_supported(w_seed_hir0_terminator_kind kind) {
  switch (kind) {
    case W_SEED_HIR0_TERMINATOR_RETURN_UNIT:
    case W_SEED_HIR0_TERMINATOR_RETURN_VALUE:
    case W_SEED_HIR0_TERMINATOR_BRANCH:
    case W_SEED_HIR0_TERMINATOR_JUMP:
      return true;
    case W_SEED_HIR0_TERMINATOR_SWITCH_ENUM:
    case W_SEED_HIR0_TERMINATOR_THROW:
    case W_SEED_HIR0_TERMINATOR_INVOKE:
    case W_SEED_HIR0_TERMINATOR_PANIC:
      return false;
  }
  return false;
}

/* HIR0 is intentionally broader than this first product projection.  Keep
 * the projection honest by rejecting every family for which the closure does
 * not publish a complete semantic relation.  This check runs after HIR0's
 * full verifier, including dead records, so the reachability walk never has
 * to guess about a future/opaque family. */
static bool product_shape_supported(const w_seed_hir0_program *program) {
  if (program == NULL || program->type_count != 4u ||
      program->external_module_count != 0u ||
      program->external_symbol_count != 0u || program->enum_count != 0u ||
      program->enum_case_count != 0u ||
      program->enum_case_parameter_count != 0u ||
      program->enum_subset_member_count != 0u ||
      program->enum_payload_count != 0u || program->switch_edge_count != 0u ||
      program->switch_capture_count != 0u ||
      program->cleanup_count != 0u ||
      program->parameter_count > W_SEED_PRODUCT_CLOSURE0_MAX_PARAMETERS ||
      program->block_count > W_SEED_PRODUCT_CLOSURE0_MAX_BLOCKS ||
      program->block_argument_count >
          W_SEED_PRODUCT_CLOSURE0_MAX_BLOCK_ARGUMENTS ||
      program->edge_argument_count > W_SEED_PRODUCT_CLOSURE0_MAX_EDGE_ARGUMENTS ||
      program->instruction_count > W_SEED_PRODUCT_CLOSURE0_MAX_INSTRUCTIONS ||
      program->binding_count > W_SEED_PRODUCT_CLOSURE0_MAX_BINDINGS ||
      program->call_count > W_SEED_PRODUCT_CLOSURE0_MAX_CALLS ||
      program->host_parameter_count >
          W_SEED_PRODUCT_CLOSURE0_MAX_HOST_PARAMETERS ||
      program->argument_count > W_SEED_PRODUCT_CLOSURE0_MAX_ARGUMENTS ||
      program->interpolation_segment_count >
          W_SEED_PRODUCT_CLOSURE0_MAX_INTERPOLATION_SEGMENTS ||
      program->terminator_count > W_SEED_PRODUCT_CLOSURE0_MAX_TERMINATORS)
    return false;
  for (size_t type = 0u; type < program->type_count; type += 1u) {
    const w_seed_hir0_type *item = &program->types[type];
    const w_seed_hir0_type_kind expected[] = {
        W_SEED_HIR0_TYPE_UNIT, W_SEED_HIR0_TYPE_STRING,
        W_SEED_HIR0_TYPE_I64, W_SEED_HIR0_TYPE_BOOL};
    if (item->kind != expected[type] || item->owner_module != W_SEED_HIR0_NONE)
      return false;
  }
  for (size_t function = 0u; function < program->function_count; function += 1u) {
    const w_seed_hir0_function *item = &program->functions[function];
    if (item->is_async || item->is_throws || item->is_unsafe ||
        item->has_borrow_clause || item->return_type >= program->type_count ||
        item->block_count == 0u)
      return false;
  }
  for (size_t instruction = 0u; instruction < program->instruction_count;
       instruction += 1u) {
    const w_seed_hir0_instruction *item = &program->instructions[instruction];
    if (item->kind != W_SEED_HIR0_INSTRUCTION_CALL &&
        item->kind != W_SEED_HIR0_INSTRUCTION_BINDING)
      return false;
  }
  for (size_t terminator = 0u; terminator < program->terminator_count;
       terminator += 1u)
    if (!product_terminator_kind_supported(
            program->terminators[terminator].kind))
      return false;
  for (size_t value = 0u; value < program->value_count; value += 1u) {
    const w_seed_hir0_value *item = &program->values[value];
    if (!product_value_kind_supported(item->kind)) return false;
    if (item->kind == W_SEED_HIR0_VALUE_BINARY_INTEGER_COMPARISON) {
      if (item->binary_operator < W_SEED_HIR0_BINARY_EQUAL ||
          item->binary_operator > W_SEED_HIR0_BINARY_GREATER_EQUAL ||
          item->type_index >= program->type_count ||
          program->types[item->type_index].kind != W_SEED_HIR0_TYPE_BOOL ||
          item->left_value >= program->value_count ||
          item->right_value >= program->value_count ||
          program->values[item->left_value].type_index >= program->type_count ||
          program->values[item->right_value].type_index >= program->type_count ||
          program->types[program->values[item->left_value].type_index].kind !=
              W_SEED_HIR0_TYPE_I64 ||
          program->types[program->values[item->right_value].type_index].kind !=
              W_SEED_HIR0_TYPE_I64)
        return false;
    }
  }
  for (size_t call = 0u; call < program->call_count; call += 1u) {
    const w_seed_hir0_call *item = &program->calls[call];
    if (item->callee_identity >= program->identity_count) return false;
    const w_seed_hir0_identity *identity =
        &program->identities[item->callee_identity];
    if (identity->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE) {
      if (!host_print_identity_valid(program, item->callee_identity)) return false;
    } else if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION) {
      return false;
    }
  }
  for (size_t requirement = 0u; requirement < program->requirement_count;
       requirement += 1u) {
    const w_seed_hir0_requirement *item = &program->requirements[requirement];
    if (item->owner_index >= program->identity_count ||
        !host_print_identity_valid(program, item->owner_index))
      return false;
  }
  return true;
}

static void result_init(w_seed_product_closure0_result *result) {
  if (result == NULL) return;
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_PRODUCT_CLOSURE0_INVALID;
  result->failure = W_SEED_PRODUCT_CLOSURE0_FAILURE_NONE;
  result->root.entry_index = W_SEED_PRODUCT_CLOSURE0_NONE;
  result->root.module_index = W_SEED_PRODUCT_CLOSURE0_NONE;
  result->root.function_index = W_SEED_PRODUCT_CLOSURE0_NONE;
  result->root.identity_index = W_SEED_PRODUCT_CLOSURE0_NONE;
}

static void set_failure(w_seed_product_closure0_result *result,
                        w_seed_product_closure0_status status,
                        w_seed_product_closure0_failure failure) {
  if (result == NULL) return;
  result_init(result);
  result->status = status;
  result->failure = failure;
}

static bool input_valid(const w_seed_product_closure0_input *input,
                        w_seed_product_closure0_result *result) {
  if (input == NULL || input->program == NULL || input->hir_result == NULL) {
    set_failure(result, W_SEED_PRODUCT_CLOSURE0_INVALID,
                W_SEED_PRODUCT_CLOSURE0_FAILURE_POINTER);
    return false;
  }
  /* This is deliberately first: no reachability walk is allowed to inspect a
   * record before the complete HIR has passed its independent verifier. */
  if (!w_seed_hir0_verify(input->program, input->hir_result)) {
    set_failure(result, W_SEED_PRODUCT_CLOSURE0_INVALID,
                W_SEED_PRODUCT_CLOSURE0_FAILURE_HIR);
    return false;
  }
  const w_seed_hir0_program *program = input->program;
  if (program->module_count == 0u ||
      program->module_count > W_SEED_PRODUCT_CLOSURE0_MAX_MODULES ||
      program->function_count == 0u ||
      program->function_count > W_SEED_PRODUCT_CLOSURE0_MAX_FUNCTIONS ||
      program->identity_count > W_SEED_PRODUCT_CLOSURE0_MAX_IDENTITIES ||
      program->type_count > W_SEED_PRODUCT_CLOSURE0_MAX_TYPES ||
      program->value_count > W_SEED_PRODUCT_CLOSURE0_MAX_VALUES ||
      program->requirement_count > W_SEED_PRODUCT_CLOSURE0_MAX_REQUIREMENTS ||
      program->external_module_count >
          W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_MODULES ||
      program->external_symbol_count >
          W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_SYMBOLS) {
    set_failure(result, W_SEED_PRODUCT_CLOSURE0_UNSUPPORTED,
                W_SEED_PRODUCT_CLOSURE0_FAILURE_LIMIT);
    return false;
  }
  if (!product_shape_supported(program)) {
    set_failure(result, W_SEED_PRODUCT_CLOSURE0_UNSUPPORTED,
                W_SEED_PRODUCT_CLOSURE0_FAILURE_UNSUPPORTED);
    return false;
  }
  if (program->entry_count != 1u || program->entries == NULL ||
      program->entries[0].module_index != 0u ||
      program->entries[0].identity_index !=
          program->module_count + program->function_count ||
      program->entries[0].target_function >= program->function_count ||
      program->entries[0].target_identity !=
          program->functions[program->entries[0].target_function].identity_index ||
      program->functions[program->entries[0].target_function].module_index !=
          0u ||
      !text_is(program, program->entries[0].slot, ".default") ||
      program->entries[0].adapter_kind !=
          W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT ||
      program->entries[0].cleanup_obligation !=
          W_SEED_HIR0_ENTRY_CLEANUP_NONE ||
      program->entries[0].first_cleanup_owner_parameter !=
          W_SEED_HIR0_NONE ||
      program->entries[0].cleanup_owner_parameter_count != 0u ||
      program->entries[0].is_body !=
          program->functions[program->entries[0].target_function]
              .is_anonymous_entry) {
    set_failure(result, W_SEED_PRODUCT_CLOSURE0_UNSUPPORTED,
                W_SEED_PRODUCT_CLOSURE0_FAILURE_ROOT);
    return false;
  }
  return true;
}

static bool mark_type(closure0_plan *plan,
                      const w_seed_hir0_program *program,
                      uint32_t type_index);
static bool mark_value(closure0_plan *plan,
                       const w_seed_hir0_program *program,
                       uint32_t value_index, size_t depth);
static bool mark_function(closure0_plan *plan,
                          const w_seed_hir0_program *program,
                          uint32_t function_index, size_t depth);

static bool mark_identity(closure0_plan *plan,
                          const w_seed_hir0_program *program,
                          uint32_t identity_index) {
  if (plan == NULL || program == NULL || identity_index >= program->identity_count ||
      identity_index >= W_SEED_PRODUCT_CLOSURE0_MAX_IDENTITIES)
    return false;
  const w_seed_hir0_identity *identity = &program->identities[identity_index];
  if (identity->kind == W_SEED_HIR0_IDENTITY_MODULE) {
    if (identity_index >= program->module_count || identity->target_index != identity_index ||
        identity->owner_module != W_SEED_HIR0_NONE)
      return false;
    plan->identities[identity_index] = true;
    return true;
  }
  if (identity->kind == W_SEED_HIR0_IDENTITY_FUNCTION) {
    if (identity->target_index >= program->function_count ||
        identity_index != program->module_count + identity->target_index ||
        plan->identity_state[identity_index] == 1u)
      return false;
    if (plan->identity_state[identity_index] == 2u) return true;
    plan->identity_state[identity_index] = 1u;
    plan->identities[identity_index] = true;
    const bool marked = mark_function(plan, program, identity->target_index, 0u);
    if (!marked) return false;
    plan->identity_state[identity_index] = 2u;
    return true;
  }
  if (identity->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE) {
    if (!host_print_identity_valid(program, identity_index)) return false;
    plan->identities[identity_index] = true;
    return true;
  }
  if (identity->kind == W_SEED_HIR0_IDENTITY_ENTRY) {
    if (identity_index != program->module_count + program->function_count ||
        identity->target_index != 0u || identity->owner_module != 0u)
      return false;
    plan->identities[identity_index] = true;
    return true;
  }
  return false;
}

static bool mark_requirement(closure0_plan *plan,
                            const w_seed_hir0_program *program,
                            uint32_t requirement_index) {
  if (plan == NULL || program == NULL ||
      requirement_index >= program->requirement_count ||
      requirement_index >= W_SEED_PRODUCT_CLOSURE0_MAX_REQUIREMENTS)
    return false;
  plan->requirements[requirement_index] = true;
  return true;
}

static bool mark_call(closure0_plan *plan,
                      const w_seed_hir0_program *program,
                      uint32_t call_index, size_t depth) {
  if (plan == NULL || program == NULL || depth > W_SEED_PRODUCT_CLOSURE0_MAX_DEPTH ||
      call_index >= program->call_count)
    return false;
  const w_seed_hir0_call *call = &program->calls[call_index];
  if (!mark_identity(plan, program, call->callee_identity) ||
      !mark_type(plan, program, call->result_type))
    return false;
  const w_seed_hir0_identity *callee = &program->identities[call->callee_identity];
  if (callee->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE) {
    if (call->first_requirement == W_SEED_HIR0_NONE &&
        call->requirement_count != 0u)
      return false;
    for (size_t ordinal = 0u; ordinal < call->requirement_count; ordinal += 1u)
      if (!mark_requirement(plan, program,
                            (uint32_t)((size_t)call->first_requirement + ordinal)))
        return false;
  } else if (callee->kind != W_SEED_HIR0_IDENTITY_FUNCTION) {
    return false;
  }
  for (size_t ordinal = 0u; ordinal < call->argument_count; ordinal += 1u) {
    const w_seed_hir0_argument *argument =
        &program->arguments[(size_t)call->first_argument + ordinal];
    if (!mark_type(plan, program, argument->type_index) ||
        !mark_value(plan, program, argument->value_index, depth + 1u))
      return false;
  }
  return true;
}

static bool mark_type(closure0_plan *plan,
                      const w_seed_hir0_program *program,
                      uint32_t type_index) {
  if (plan == NULL || program == NULL || type_index >= program->type_count ||
      type_index >= W_SEED_PRODUCT_CLOSURE0_MAX_TYPES)
    return false;
  if (plan->types[type_index]) return true;
  const w_seed_hir0_type *type = &program->types[type_index];
  plan->types[type_index] = true;
  if (type->kind == W_SEED_HIR0_TYPE_NOMINAL) {
    if (type->external_module_index >= program->external_module_count ||
        type->external_symbol_index >= program->external_symbol_count ||
        type->external_module_index >=
            W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_MODULES ||
        type->external_symbol_index >=
            W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_SYMBOLS)
      return false;
    plan->external_modules[type->external_module_index] = true;
    plan->external_symbols[type->external_symbol_index] = true;
  }
  return true;
}

static bool mark_value(closure0_plan *plan,
                       const w_seed_hir0_program *program,
                       uint32_t value_index, size_t depth) {
  if (plan == NULL || program == NULL || depth > W_SEED_PRODUCT_CLOSURE0_MAX_DEPTH ||
      value_index >= program->value_count ||
      value_index >= W_SEED_PRODUCT_CLOSURE0_MAX_VALUES)
    return false;
  if (plan->value_state[value_index] == 2u) return true;
  if (plan->value_state[value_index] == 1u) return false;
  plan->value_state[value_index] = 1u;
  plan->values[value_index] = true;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (!mark_type(plan, program, value->type_index)) return false;
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
    if (value->binding_index >= program->binding_count ||
        !mark_value(plan, program,
                   program->bindings[value->binding_index].initializer_value,
                   depth + 1u))
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_CALL_RESULT) {
    if (!mark_call(plan, program, value->call_index, depth + 1u)) return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_BINARY_U64 ||
             value->kind == W_SEED_HIR0_VALUE_UNARY_U64) {
    return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
             value->kind == W_SEED_HIR0_VALUE_UNARY_BOOL ||
             value->kind == W_SEED_HIR0_VALUE_UNARY_I64 ||
             value->kind == W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON ||
             value->kind == W_SEED_HIR0_VALUE_BINARY_INTEGER_COMPARISON) {
    if (value->left_value != W_SEED_HIR0_NONE &&
        !mark_value(plan, program, value->left_value, depth + 1u))
      return false;
    if (value->right_value != W_SEED_HIR0_NONE &&
        !mark_value(plan, program, value->right_value, depth + 1u))
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
    if (value->first_interpolation_segment >
            program->interpolation_segment_count ||
        value->interpolation_segment_count >
            program->interpolation_segment_count -
                value->first_interpolation_segment)
      return false;
    for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_hir0_interpolation_segment *segment =
          &program->interpolation_segments[
              (size_t)value->first_interpolation_segment + ordinal];
      if (segment->kind == W_SEED_HIR0_INTERPOLATION_VALUE &&
          !mark_value(plan, program, segment->value_index, depth + 1u))
        return false;
    }
  } else if (value->kind == W_SEED_HIR0_VALUE_ENUM_CASE) {
    if (value->first_enum_payload > program->enum_payload_count ||
        value->enum_payload_count >
            program->enum_payload_count - value->first_enum_payload)
      return false;
    for (size_t ordinal = 0u; ordinal < value->enum_payload_count; ordinal += 1u)
      if (!mark_value(plan, program,
                      program->enum_payloads[(size_t)value->first_enum_payload +
                                             ordinal]
                          .value_index,
                      depth + 1u))
        return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER ||
             value->kind == W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE) {
    if (value->external_module_index >= program->external_module_count ||
        value->external_symbol_index >= program->external_symbol_count ||
        value->external_module_index >=
            W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_MODULES ||
        value->external_symbol_index >=
            W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_SYMBOLS) return false;
    plan->external_modules[value->external_module_index] = true;
    plan->external_symbols[value->external_symbol_index] = true;
    if (value->left_value != W_SEED_HIR0_NONE &&
        !mark_value(plan, program, value->left_value, depth + 1u))
      return false;
  }
  plan->value_state[value_index] = 2u;
  return true;
}

static bool mark_terminator(closure0_plan *plan,
                            const w_seed_hir0_program *program,
                            const w_seed_hir0_terminator *terminator,
                            size_t depth) {
  if (plan == NULL || program == NULL || terminator == NULL ||
      depth > W_SEED_PRODUCT_CLOSURE0_MAX_DEPTH)
    return false;
  if (!mark_type(plan, program, terminator->result_type)) return false;
  if (terminator->value_index != W_SEED_HIR0_NONE &&
      !mark_value(plan, program, terminator->value_index, depth + 1u))
    return false;
  for (size_t ordinal = 0u; ordinal < terminator->edge_argument_count;
       ordinal += 1u) {
    if (terminator->first_edge_argument == W_SEED_HIR0_NONE ||
        (size_t)terminator->first_edge_argument + ordinal >=
            program->edge_argument_count ||
        !mark_type(plan, program,
                   program->edge_arguments[(size_t)terminator->first_edge_argument +
                                           ordinal]
                       .type_index) ||
        !mark_value(plan, program,
                    program->edge_arguments[(size_t)terminator->first_edge_argument +
                                            ordinal]
                        .value_index,
                    depth + 1u))
      return false;
  }
  if (terminator->kind == W_SEED_HIR0_TERMINATOR_SWITCH_ENUM) {
    if (terminator->first_switch_edge == W_SEED_HIR0_NONE ||
        terminator->first_switch_edge > program->switch_edge_count ||
        terminator->switch_edge_count >
            program->switch_edge_count - terminator->first_switch_edge)
      return false;
    for (size_t ordinal = 0u; ordinal < terminator->switch_edge_count;
         ordinal += 1u) {
      const w_seed_hir0_switch_edge *edge =
          &program->switch_edges[(size_t)terminator->first_switch_edge + ordinal];
      if (edge->target_block >= program->block_count ||
          edge->target_block == terminator->owner_block ||
          !mark_terminator(plan, program,
                           &program->terminators[edge->target_block], depth + 1u))
        return false;
    }
  }
  return true;
}

static bool mark_function(closure0_plan *plan,
                          const w_seed_hir0_program *program,
                          uint32_t function_index, size_t depth) {
  if (plan == NULL || program == NULL || depth > program->function_count ||
      function_index >= program->function_count ||
      function_index >= W_SEED_PRODUCT_CLOSURE0_MAX_FUNCTIONS)
    return false;
  if (plan->function_state[function_index] == 2u) return true;
  if (plan->function_state[function_index] == 1u) return false;
  plan->function_state[function_index] = 1u;
  plan->functions[function_index] = true;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->module_index >= program->module_count ||
      function->identity_index >= program->identity_count ||
      function->identity_index != program->module_count + function_index ||
      program->identities[function->identity_index].kind !=
          W_SEED_HIR0_IDENTITY_FUNCTION ||
      program->identities[function->identity_index].target_index !=
          function_index ||
      !mark_type(plan, program, function->return_type))
    return false;
  plan->identities[function->identity_index] = true;
  if (function->is_async || function->is_throws || function->is_unsafe ||
      function->has_borrow_clause)
    return false;
  for (size_t ordinal = 0u; ordinal < function->parameter_count; ordinal += 1u) {
    const w_seed_hir0_parameter *parameter =
        &program->parameters[(size_t)function->first_parameter + ordinal];
    if (!mark_type(plan, program, parameter->type_index)) return false;
  }
  for (size_t block_ordinal = 0u; block_ordinal < function->block_count;
       block_ordinal += 1u) {
    const size_t block_index = (size_t)function->first_block + block_ordinal;
    const w_seed_hir0_block *block = &program->blocks[block_index];
    for (size_t argument = 0u; argument < block->block_argument_count;
         argument += 1u) {
      const w_seed_hir0_block_argument *item =
          &program->block_arguments[(size_t)block->first_block_argument +
                                    argument];
      if (!mark_type(plan, program, item->type_index)) return false;
    }
    for (size_t instruction_ordinal = 0u;
         instruction_ordinal < block->instruction_count; instruction_ordinal += 1u) {
      const w_seed_hir0_instruction *instruction =
          &program->instructions[(size_t)block->first_instruction +
                                 instruction_ordinal];
      if (!mark_type(plan, program, instruction->result_type)) return false;
      if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
        if (!mark_call(plan, program, instruction->call_index, depth + 1u))
          return false;
      } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
        if (instruction->binding_index >= program->binding_count ||
            !mark_type(plan, program,
                       program->bindings[instruction->binding_index].type_index) ||
            !mark_value(plan, program,
                        program->bindings[instruction->binding_index]
                            .initializer_value,
                        depth + 1u))
          return false;
      } else {
        return false;
      }
    }
    if (block->terminator_index >= program->terminator_count ||
        !mark_terminator(plan, program,
                         &program->terminators[block->terminator_index], depth + 1u))
      return false;
  }
  plan->function_state[function_index] = 2u;
  return true;
}

static bool build_plan(const w_seed_product_closure0_input *input,
                       closure0_plan *plan,
                       w_seed_product_closure0_result *failure_result) {
  if (plan == NULL || !input_valid(input, failure_result)) return false;
  const w_seed_hir0_program *program = input->program;
  (void)memset(plan, 0, sizeof(*plan));
  for (size_t index = 0u; index < W_SEED_PRODUCT_CLOSURE0_MAX_MODULES; index += 1u)
    plan->module_remap[index] = W_SEED_PRODUCT_CLOSURE0_NONE;
  for (size_t index = 0u; index < W_SEED_PRODUCT_CLOSURE0_MAX_FUNCTIONS; index += 1u)
    plan->function_remap[index] = W_SEED_PRODUCT_CLOSURE0_NONE;
  for (size_t index = 0u; index < W_SEED_PRODUCT_CLOSURE0_MAX_IDENTITIES; index += 1u)
    plan->identity_remap[index] = W_SEED_PRODUCT_CLOSURE0_NONE;
  for (size_t index = 0u; index < W_SEED_PRODUCT_CLOSURE0_MAX_TYPES; index += 1u)
    plan->type_remap[index] = W_SEED_PRODUCT_CLOSURE0_NONE;
  for (size_t index = 0u; index < W_SEED_PRODUCT_CLOSURE0_MAX_VALUES; index += 1u)
    plan->value_remap[index] = W_SEED_PRODUCT_CLOSURE0_NONE;
  for (size_t index = 0u;
       index < W_SEED_PRODUCT_CLOSURE0_MAX_REQUIREMENTS; index += 1u)
    plan->requirement_remap[index] = W_SEED_PRODUCT_CLOSURE0_NONE;
  for (size_t index = 0u;
       index < W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_MODULES; index += 1u)
    plan->external_module_remap[index] = W_SEED_PRODUCT_CLOSURE0_NONE;
  for (size_t index = 0u;
       index < W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_SYMBOLS; index += 1u)
    plan->external_symbol_remap[index] = W_SEED_PRODUCT_CLOSURE0_NONE;

  const w_seed_hir0_entry *entry = &program->entries[0];
  plan->root = (w_seed_product_closure0_root){
      0u, entry->module_index, entry->target_function, entry->identity_index};
  plan->modules[entry->module_index] = true;
  if (!mark_identity(plan, program, entry->module_index) ||
      !mark_identity(plan, program, entry->identity_index)) {
    set_failure(failure_result, W_SEED_PRODUCT_CLOSURE0_UNSUPPORTED,
                W_SEED_PRODUCT_CLOSURE0_FAILURE_ROOT);
    return false;
  }
  if (!mark_function(plan, program, entry->target_function, 0u)) {
    set_failure(failure_result, W_SEED_PRODUCT_CLOSURE0_UNSUPPORTED,
                W_SEED_PRODUCT_CLOSURE0_FAILURE_UNSUPPORTED);
    return false;
  }
  for (size_t function = 0u; function < program->function_count; function += 1u)
    if (plan->functions[function]) {
      const uint32_t module = program->functions[function].module_index;
      if (module >= program->module_count) {
        set_failure(failure_result, W_SEED_PRODUCT_CLOSURE0_INVALID,
                    W_SEED_PRODUCT_CLOSURE0_FAILURE_OWNERSHIP);
        return false;
      }
      plan->modules[module] = true;
    }
  for (size_t module = 0u; module < program->module_count; module += 1u)
    if (plan->modules[module] && !mark_identity(plan, program, (uint32_t)module)) {
      set_failure(failure_result, W_SEED_PRODUCT_CLOSURE0_INVALID,
                  W_SEED_PRODUCT_CLOSURE0_FAILURE_OWNERSHIP);
      return false;
    }
  for (size_t index = 0u; index < program->module_count; index += 1u)
    if (plan->modules[index])
      plan->module_remap[index] = (uint32_t)plan->counts.reachable_modules++;
    else
      plan->counts.omitted_modules += 1u;
  for (size_t index = 0u; index < program->function_count; index += 1u)
    if (plan->functions[index])
      plan->function_remap[index] =
          (uint32_t)plan->counts.reachable_functions++;
    else
      plan->counts.omitted_functions += 1u;
  for (size_t index = 0u; index < program->identity_count; index += 1u)
    if (plan->identities[index])
      plan->identity_remap[index] =
          (uint32_t)plan->counts.reachable_identities++;
  for (size_t index = 0u; index < program->type_count; index += 1u)
    if (plan->types[index])
      plan->type_remap[index] = (uint32_t)plan->counts.reachable_types++;
  for (size_t index = 0u; index < program->value_count; index += 1u)
    if (plan->values[index])
      plan->value_remap[index] = (uint32_t)plan->counts.reachable_values++;
  for (size_t index = 0u; index < program->requirement_count; index += 1u)
    if (plan->requirements[index])
      plan->requirement_remap[index] =
          (uint32_t)plan->counts.reachable_requirements++;
  for (size_t index = 0u; index < program->external_module_count; index += 1u)
    if (plan->external_modules[index])
      plan->external_module_remap[index] =
          (uint32_t)plan->counts.reachable_external_modules++;
  for (size_t index = 0u; index < program->external_symbol_count; index += 1u)
    if (plan->external_symbols[index])
      plan->external_symbol_remap[index] =
          (uint32_t)plan->counts.reachable_external_symbols++;

  plan->counts.modules = program->module_count;
  plan->counts.functions = program->function_count;
  plan->counts.identities = program->identity_count;
  plan->counts.types = program->type_count;
  plan->counts.values = program->value_count;
  plan->counts.requirements = program->requirement_count;
  plan->counts.external_modules = program->external_module_count;
  plan->counts.external_symbols = program->external_symbol_count;
  plan->counts.module_remap = program->module_count;
  plan->counts.function_remap = program->function_count;
  plan->counts.identity_remap = program->identity_count;
  plan->counts.type_remap = program->type_count;
  plan->counts.value_remap = program->value_count;
  plan->counts.requirement_remap = program->requirement_count;
  plan->counts.external_module_remap = program->external_module_count;
  plan->counts.external_symbol_remap = program->external_symbol_count;
  return true;
}

static void digest_u32(w_seed_sha256_state *state, uint32_t value) {
  uint8_t bytes[4];
  bytes[0] = (uint8_t)(value >> 24u);
  bytes[1] = (uint8_t)(value >> 16u);
  bytes[2] = (uint8_t)(value >> 8u);
  bytes[3] = (uint8_t)value;
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void digest_u64(w_seed_sha256_state *state, uint64_t value) {
  uint8_t bytes[8];
  for (size_t index = 0u; index < sizeof(bytes); index += 1u)
    bytes[sizeof(bytes) - 1u - index] = (uint8_t)(value >> (index * 8u));
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void digest_bool(w_seed_sha256_state *state, bool value) {
  const uint8_t byte = value ? 1u : 0u;
  w_seed_sha256_update(state, &byte, sizeof(byte));
}

static void digest_bytes(w_seed_sha256_state *state, const uint8_t *bytes,
                         size_t count) {
  digest_u64(state, (uint64_t)count);
  if (count != 0u && bytes != NULL) w_seed_sha256_update(state, bytes, count);
}

static void digest_text(w_seed_sha256_state *state,
                        const w_seed_hir0_program *program,
                        w_seed_hir0_text text) {
  if (program == NULL || !text_valid(program, text)) {
    digest_u64(state, UINT64_MAX);
    return;
  }
  digest_bytes(state, program->text_bytes + text.offset, text.count);
}

static uint32_t function_block_ordinal(const w_seed_hir0_program *program,
                                       uint32_t block_index) {
  if (program == NULL || block_index >= program->block_count) return UINT32_MAX;
  const uint32_t function = program->blocks[block_index].owner_function;
  if (function >= program->function_count) return UINT32_MAX;
  const w_seed_hir0_function *owner = &program->functions[function];
  if (block_index < owner->first_block ||
      block_index >= (size_t)owner->first_block + owner->block_count)
    return UINT32_MAX;
  return block_index - owner->first_block;
}

static uint32_t value_owner_function(const w_seed_hir0_program *program,
                                     uint32_t value_index) {
  if (program == NULL || value_index >= program->value_count) return UINT32_MAX;
  uint32_t current = value_index;
  for (size_t depth = 0u; depth <= W_SEED_HIR0_MAX_NESTING; depth += 1u) {
    if (current >= program->value_count) return UINT32_MAX;
    const w_seed_hir0_value *value = &program->values[current];
    if (value->owner_kind == W_SEED_HIR0_VALUE_OWNER_ARGUMENT) {
      if (value->owner_index >= program->argument_count) return UINT32_MAX;
      const uint32_t call = program->arguments[value->owner_index].owner_call;
      if (call >= program->call_count) return UINT32_MAX;
      const uint32_t block = program->calls[call].owner_block;
      return block < program->block_count
                 ? program->blocks[block].owner_function
                 : UINT32_MAX;
    }
    if (value->owner_kind == W_SEED_HIR0_VALUE_OWNER_BINDING) {
      if (value->owner_index >= program->binding_count) return UINT32_MAX;
      const uint32_t block = program->bindings[value->owner_index].owner_block;
      return block < program->block_count
                 ? program->blocks[block].owner_function
                 : UINT32_MAX;
    }
    if (value->owner_kind == W_SEED_HIR0_VALUE_OWNER_TERMINATOR) {
      if (value->owner_index >= program->terminator_count) return UINT32_MAX;
      const uint32_t block =
          program->terminators[value->owner_index].owner_block;
      return block < program->block_count
                 ? program->blocks[block].owner_function
                 : UINT32_MAX;
    }
    if (value->owner_kind == W_SEED_HIR0_VALUE_OWNER_BINARY ||
        value->owner_kind == W_SEED_HIR0_VALUE_OWNER_UNARY ||
        value->owner_kind == W_SEED_HIR0_VALUE_OWNER_EXTERNAL_MEMBER ||
        value->owner_kind == W_SEED_HIR0_VALUE_OWNER_EXTERNAL_ENUM_CASE) {
      if (value->owner_index == W_SEED_HIR0_NONE) return UINT32_MAX;
      current = value->owner_index;
      continue;
    }
    if (value->owner_kind == W_SEED_HIR0_VALUE_OWNER_INTERPOLATION_SEGMENT) {
      if (value->owner_index >= program->interpolation_segment_count)
        return UINT32_MAX;
      current = program->interpolation_segments[value->owner_index].owner_value;
      continue;
    }
    if (value->owner_kind == W_SEED_HIR0_VALUE_OWNER_ENUM_PAYLOAD) {
      if (value->owner_index >= program->enum_payload_count) return UINT32_MAX;
      current = program->enum_payloads[value->owner_index].owner_value;
      continue;
    }
    return UINT32_MAX;
  }
  return UINT32_MAX;
}

static void digest_function_location(w_seed_sha256_state *state,
                                     const w_seed_hir0_program *program,
                                     uint32_t function_index,
                                     uint32_t block_index,
                                     uint32_t instruction_ordinal) {
  if (program == NULL || function_index >= program->function_count ||
      block_index >= program->block_count ||
      program->blocks[block_index].owner_function != function_index) {
    digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    return;
  }
  const w_seed_hir0_function *function = &program->functions[function_index];
  digest_text(state, program,
              program->modules[function->module_index].module_id);
  digest_text(state, program, function->name);
  digest_u32(state, function_block_ordinal(program, block_index));
  digest_u32(state, instruction_ordinal);
}

static void digest_binding_reference(w_seed_sha256_state *state,
                                     const w_seed_hir0_program *program,
                                     uint32_t binding_index) {
  if (program == NULL || binding_index >= program->binding_count) {
    digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    return;
  }
  const w_seed_hir0_binding *binding = &program->bindings[binding_index];
  if (binding->owner_block >= program->block_count) {
    digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    return;
  }
  const uint32_t function_index =
      program->blocks[binding->owner_block].owner_function;
  digest_function_location(state, program, function_index, binding->owner_block,
                           binding->ordinal);
  digest_text(state, program, binding->name);
  digest_bool(state, binding->is_mutable);
  if (binding->source_binding >= program->binding_count) {
    digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    return;
  }
  const w_seed_hir0_binding *source =
      &program->bindings[binding->source_binding];
  if (source->owner_block >= program->block_count) {
    digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    return;
  }
  const uint32_t source_function =
      program->blocks[source->owner_block].owner_function;
  digest_function_location(state, program, source_function, source->owner_block,
                           source->ordinal);
  digest_text(state, program, source->name);
  /* Version identity is a semantic predecessor count, never the source
   * binding array index. */
  size_t version = 0u;
  uint32_t cursor = binding->source_binding;
  while (cursor != binding_index && cursor < program->binding_count &&
         version <= program->binding_count) {
    const uint32_t next = program->bindings[cursor].next_version;
    if (next == W_SEED_HIR0_NONE) break;
    cursor = next;
    version += 1u;
  }
  digest_u64(state, (uint64_t)version);
}

static void digest_parameter_reference(w_seed_sha256_state *state,
                                       const w_seed_hir0_program *program,
                                       uint32_t parameter_index) {
  if (program == NULL || parameter_index >= program->parameter_count) {
    digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    return;
  }
  const w_seed_hir0_parameter *parameter = &program->parameters[parameter_index];
  if (parameter->owner_function >= program->function_count) {
    digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    return;
  }
  const w_seed_hir0_function *function =
      &program->functions[parameter->owner_function];
  digest_text(state, program,
              program->modules[function->module_index].module_id);
  digest_text(state, program, function->name);
  digest_u32(state, parameter->ordinal);
  digest_text(state, program, parameter->name);
  digest_text(state, program, parameter->label);
}

static void digest_block_argument_reference(w_seed_sha256_state *state,
                                            const w_seed_hir0_program *program,
                                            uint32_t argument_index) {
  if (program == NULL || argument_index >= program->block_argument_count) {
    digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    return;
  }
  const w_seed_hir0_block_argument *argument =
      &program->block_arguments[argument_index];
  if (argument->owner_block >= program->block_count) {
    digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    return;
  }
  const uint32_t function_index =
      program->blocks[argument->owner_block].owner_function;
  digest_function_location(state, program, function_index, argument->owner_block,
                           argument->ordinal);
}

static void digest_type(w_seed_sha256_state *state,
                        const w_seed_hir0_program *program, uint32_t type_index,
                        const closure0_plan *plan) {
  (void)plan;
  if (program == NULL || plan == NULL || type_index >= program->type_count) {
    digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    return;
  }
  const w_seed_hir0_type *type = &program->types[type_index];
  digest_u32(state, (uint32_t)type->kind);
  digest_text(state, program, type->name);
  if (type->kind == W_SEED_HIR0_TYPE_NOMINAL) {
    if (type->external_module_index < program->external_module_count)
      digest_text(state,
                  program,
                  program->external_modules[type->external_module_index]
                      .module_id);
    else
      digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    if (type->external_symbol_index < program->external_symbol_count)
      digest_text(state, program,
                  program->external_symbols[type->external_symbol_index].name);
    else
      digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
  }
}

static void digest_identity(w_seed_sha256_state *state,
                            const w_seed_hir0_program *program,
                            uint32_t identity_index) {
  if (program == NULL || identity_index >= program->identity_count) {
    digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    return;
  }
  const w_seed_hir0_identity *identity = &program->identities[identity_index];
  digest_u32(state, (uint32_t)identity->kind);
  digest_text(state, program, identity->name);
  digest_text(state, program, identity->profile);
  if (identity->kind == W_SEED_HIR0_IDENTITY_FUNCTION &&
      identity->target_index < program->function_count) {
    const w_seed_hir0_function *function =
        &program->functions[identity->target_index];
    digest_text(state, program, function->name);
    if (function->module_index < program->module_count)
      digest_text(state, program,
                  program->modules[function->module_index].module_id);
  }
}

static void digest_value(w_seed_sha256_state *state,
                         const w_seed_hir0_program *program,
                         const closure0_plan *plan, uint32_t value_index) {
  if (program == NULL || plan == NULL || value_index >= program->value_count ||
      !plan->values[value_index]) {
    digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
    return;
  }
  const w_seed_hir0_value *value = &program->values[value_index];
  digest_u32(state, (uint32_t)value->kind);
  digest_type(state, program, value->type_index, plan);
  digest_u32(state, (uint32_t)value->owner_kind);
  digest_u32(state, value->owner_ordinal);
  switch (value->kind) {
    case W_SEED_HIR0_VALUE_BINDING_READ:
      digest_binding_reference(state, program, value->binding_index);
      break;
    case W_SEED_HIR0_VALUE_PARAMETER_READ:
      digest_parameter_reference(state, program, value->parameter_index);
      break;
    case W_SEED_HIR0_VALUE_CALL_RESULT:
      if (value->call_index < program->call_count)
        digest_identity(state, program,
                        program->calls[value->call_index].callee_identity);
      else
        digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
      break;
    case W_SEED_HIR0_VALUE_BINARY_I64:
      digest_u32(state, (uint32_t)value->binary_operator);
      digest_u32(state, value->left_value < program->value_count
                             ? plan->value_remap[value->left_value]
                             : W_SEED_PRODUCT_CLOSURE0_NONE);
      digest_u32(state, value->right_value < program->value_count
                             ? plan->value_remap[value->right_value]
                             : W_SEED_PRODUCT_CLOSURE0_NONE);
      break;
    case W_SEED_HIR0_VALUE_BINARY_INTEGER_COMPARISON:
      digest_u32(state, (uint32_t)value->binary_operator);
      digest_u32(state, value->left_value < program->value_count
                             ? plan->value_remap[value->left_value]
                             : W_SEED_PRODUCT_CLOSURE0_NONE);
      digest_u32(state, value->right_value < program->value_count
                             ? plan->value_remap[value->right_value]
                             : W_SEED_PRODUCT_CLOSURE0_NONE);
      break;
    case W_SEED_HIR0_VALUE_UNARY_BOOL:
    case W_SEED_HIR0_VALUE_UNARY_I64:
      digest_u32(state, (uint32_t)value->unary_operator);
      digest_u32(state, value->left_value < program->value_count
                             ? plan->value_remap[value->left_value]
                             : W_SEED_PRODUCT_CLOSURE0_NONE);
      break;
    case W_SEED_HIR0_VALUE_UNARY_U64:
      /* ProductClosure0 rejects this kind before digest publication. */
      break;
    case W_SEED_HIR0_VALUE_INTERPOLATED_STRING:
      digest_u32(state, value->interpolation_segment_count);
      for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
           ordinal += 1u) {
        const size_t index = (size_t)value->first_interpolation_segment + ordinal;
        if (index >= program->interpolation_segment_count) {
          digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
          continue;
        }
        const w_seed_hir0_interpolation_segment *segment =
            &program->interpolation_segments[index];
        digest_u32(state, (uint32_t)segment->kind);
        if (segment->kind == W_SEED_HIR0_INTERPOLATION_TEXT) {
          if (segment->byte_offset <= program->value_byte_count &&
              segment->byte_count <=
                  program->value_byte_count - segment->byte_offset)
            digest_bytes(state, program->value_bytes + segment->byte_offset,
                         segment->byte_count);
          else
            digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
        } else {
          digest_u32(state, segment->value_index < program->value_count
                                 ? plan->value_remap[segment->value_index]
                                 : W_SEED_PRODUCT_CLOSURE0_NONE);
        }
      }
      break;
    case W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ:
      digest_block_argument_reference(state, program, value->block_argument_index);
      break;
    case W_SEED_HIR0_VALUE_CONST_STRING:
      if (value->byte_offset <= program->value_byte_count &&
          value->byte_count <= program->value_byte_count - value->byte_offset)
        digest_bytes(state, program->value_bytes + value->byte_offset,
                     value->byte_count);
      else
        digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
      break;
    case W_SEED_HIR0_VALUE_CONST_I64:
      digest_u64(state, (uint64_t)value->integer_value);
      break;
    case W_SEED_HIR0_VALUE_CONST_BOOL:
      digest_bool(state, value->bool_value);
      break;
    default:
      /* product_shape_supported() prevents this arm for a published plan. */
      digest_u32(state, W_SEED_PRODUCT_CLOSURE0_NONE);
      break;
  }
}

static void compute_digest(const w_seed_hir0_program *program,
                           const closure0_plan *plan,
                           uint8_t digest[W_SEED_PRODUCT_CLOSURE0_DIGEST_BYTES]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  static const uint8_t tag[] = "w-seed-product-closure0-semantic\0";
  w_seed_sha256_update(&state, tag, sizeof(tag) - 1u);
  const w_seed_hir0_entry *root_entry = &program->entries[plan->root.entry_index];
  digest_text(&state, program,
              program->modules[plan->root.module_index].module_id);
  digest_text(&state, program, root_entry->target_name);
  digest_text(&state, program, root_entry->slot);
  digest_bool(&state, root_entry->is_body);
  digest_u32(&state, (uint32_t)plan->counts.reachable_modules);
  for (size_t module = 0u; module < program->module_count; module += 1u)
    if (plan->modules[module]) {
      digest_text(&state, program, program->modules[module].module_id);
      digest_text(&state, program, program->modules[module].local_module_name);
    }
  digest_u32(&state, (uint32_t)plan->counts.reachable_identities);
  for (size_t identity = 0u; identity < program->identity_count; identity += 1u)
    if (plan->identities[identity])
      digest_identity(&state, program, (uint32_t)identity);
  digest_u32(&state, (uint32_t)plan->counts.reachable_functions);
  for (size_t function_index = 0u; function_index < program->function_count;
       function_index += 1u)
    if (plan->functions[function_index]) {
      const w_seed_hir0_function *function = &program->functions[function_index];
      digest_text(&state, program, function->name);
      digest_bool(&state, function->exported);
      digest_bool(&state, function->is_const);
      digest_bool(&state, function->is_async);
      digest_bool(&state, function->is_throws);
      digest_bool(&state, function->is_unsafe);
      digest_bool(&state, function->has_borrow_clause);
      digest_type(&state, program, function->return_type, plan);
      digest_u32(&state, function->parameter_count);
      for (size_t parameter = 0u; parameter < function->parameter_count;
           parameter += 1u) {
        const w_seed_hir0_parameter *item =
            &program->parameters[(size_t)function->first_parameter + parameter];
        digest_text(&state, program, item->name);
        digest_text(&state, program, item->label);
        digest_u32(&state, (uint32_t)item->label_kind);
        digest_type(&state, program, item->type_index, plan);
      }
      digest_u32(&state, function->block_count);
      for (size_t block_ordinal = 0u; block_ordinal < function->block_count;
           block_ordinal += 1u) {
        const w_seed_hir0_block *block =
            &program->blocks[(size_t)function->first_block + block_ordinal];
        digest_u32(&state, block->instruction_count);
        for (size_t instruction_ordinal = 0u;
             instruction_ordinal < block->instruction_count;
             instruction_ordinal += 1u) {
          const w_seed_hir0_instruction *instruction =
              &program->instructions[(size_t)block->first_instruction +
                                     instruction_ordinal];
          digest_u32(&state, (uint32_t)instruction->kind);
          digest_type(&state, program, instruction->result_type, plan);
          if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
            const w_seed_hir0_binding *binding =
                &program->bindings[instruction->binding_index];
            digest_binding_reference(&state, program, instruction->binding_index);
            digest_u32(&state, plan->value_remap[binding->initializer_value]);
          } else {
            const w_seed_hir0_call *call =
                &program->calls[instruction->call_index];
            digest_identity(&state, program, call->callee_identity);
            digest_u32(&state, call->argument_count);
            for (size_t argument = 0u; argument < call->argument_count;
                 argument += 1u) {
              const w_seed_hir0_argument *item =
                  &program->arguments[(size_t)call->first_argument + argument];
              digest_u32(&state, item->parameter_ordinal);
              digest_text(&state, program, item->label);
              digest_u32(&state, (uint32_t)item->label_kind);
              digest_type(&state, program, item->type_index, plan);
              digest_u32(&state, plan->value_remap[item->value_index]);
            }
          }
        }
        const w_seed_hir0_terminator *terminator =
            &program->terminators[block->terminator_index];
        digest_u32(&state, (uint32_t)terminator->kind);
        digest_type(&state, program, terminator->result_type, plan);
        digest_u32(&state, (uint32_t)terminator->logical_operator);
        digest_u32(&state, terminator->value_index < program->value_count
                               ? plan->value_remap[terminator->value_index]
                               : W_SEED_PRODUCT_CLOSURE0_NONE);
        digest_u32(&state, function_block_ordinal(program,
                                                   terminator->target_block));
        digest_u32(&state, function_block_ordinal(program,
                                                   terminator->else_block));
        digest_u32(&state, terminator->edge_argument_count);
        for (size_t edge = 0u; edge < terminator->edge_argument_count; edge += 1u)
          digest_u32(&state, plan->value_remap[
                                 program->edge_arguments[(size_t)terminator
                                                              ->first_edge_argument +
                                                          edge]
                                     .value_index]);
      }
    }
  digest_u32(&state, (uint32_t)plan->counts.reachable_types);
  for (size_t type = 0u; type < program->type_count; type += 1u)
    if (plan->types[type]) digest_type(&state, program, (uint32_t)type, plan);
  digest_u32(&state, (uint32_t)plan->counts.reachable_requirements);
  for (size_t requirement = 0u; requirement < program->requirement_count;
       requirement += 1u)
    if (plan->requirements[requirement]) {
      const w_seed_hir0_requirement *item = &program->requirements[requirement];
      digest_u32(&state, (uint32_t)item->owner_kind);
      digest_identity(&state, program, item->owner_index);
      digest_text(&state, program, item->name);
    }
  digest_u32(&state, (uint32_t)plan->counts.reachable_values);
  for (size_t value = 0u; value < program->value_count; value += 1u)
    if (plan->values[value]) digest_value(&state, program, plan, (uint32_t)value);
  w_seed_sha256_final(&state, digest);
}

static void finish_plan_result(const closure0_plan *plan,
                              w_seed_product_closure0_result *result) {
  if (plan == NULL || result == NULL) return;
  result_init(result);
  result->status = W_SEED_PRODUCT_CLOSURE0_OK;
  result->required = plan->counts;
  result->written = plan->counts;
  result->failure = W_SEED_PRODUCT_CLOSURE0_FAILURE_NONE;
  result->root = plan->root;
  (void)memcpy(result->reachable_semantic_digest, plan->digest,
               sizeof(result->reachable_semantic_digest));
}

static bool output_capacity_valid(const w_seed_product_closure0_output *output,
                                  const w_seed_product_closure0_counts *counts) {
  if (output == NULL || counts == NULL) return false;
#define REQUIRE_ARRAY(pointer, capacity, amount)                              \
  ((amount) == 0u || ((pointer) != NULL && (capacity) >= (amount)))
  if (!REQUIRE_ARRAY(output->reachable_modules,
                    output->reachable_module_capacity,
                    counts->reachable_modules) ||
      !REQUIRE_ARRAY(output->omitted_modules, output->omitted_module_capacity,
                     counts->omitted_modules) ||
      !REQUIRE_ARRAY(output->reachable_functions,
                     output->reachable_function_capacity,
                     counts->reachable_functions) ||
      !REQUIRE_ARRAY(output->omitted_functions,
                     output->omitted_function_capacity,
                     counts->omitted_functions) ||
      !REQUIRE_ARRAY(output->reachable_identities,
                     output->reachable_identity_capacity,
                     counts->reachable_identities) ||
      !REQUIRE_ARRAY(output->reachable_types, output->reachable_type_capacity,
                     counts->reachable_types) ||
      !REQUIRE_ARRAY(output->reachable_values,
                     output->reachable_value_capacity,
                     counts->reachable_values) ||
      !REQUIRE_ARRAY(output->reachable_requirements,
                     output->reachable_requirement_capacity,
                     counts->reachable_requirements) ||
      !REQUIRE_ARRAY(output->reachable_external_modules,
                     output->reachable_external_module_capacity,
                     counts->reachable_external_modules) ||
      !REQUIRE_ARRAY(output->reachable_external_symbols,
                     output->reachable_external_symbol_capacity,
                     counts->reachable_external_symbols) ||
      !REQUIRE_ARRAY(output->module_remap, output->module_remap_capacity,
                     counts->module_remap) ||
      !REQUIRE_ARRAY(output->function_remap, output->function_remap_capacity,
                     counts->function_remap) ||
      !REQUIRE_ARRAY(output->identity_remap, output->identity_remap_capacity,
                     counts->identity_remap) ||
      !REQUIRE_ARRAY(output->type_remap, output->type_remap_capacity,
                     counts->type_remap) ||
      !REQUIRE_ARRAY(output->value_remap, output->value_remap_capacity,
                     counts->value_remap) ||
      !REQUIRE_ARRAY(output->requirement_remap,
                     output->requirement_remap_capacity,
                     counts->requirement_remap) ||
      !REQUIRE_ARRAY(output->external_module_remap,
                     output->external_module_remap_capacity,
                     counts->external_module_remap) ||
      !REQUIRE_ARRAY(output->external_symbol_remap,
                     output->external_symbol_remap_capacity,
                     counts->external_symbol_remap)) {
#undef REQUIRE_ARRAY
    return false;
  }
#undef REQUIRE_ARRAY
  /* Fact arrays are optional projections.  When supplied they are still
   * bounded transactionally. */
  const bool values_ok =
      (output->value_facts == NULL && output->value_fact_capacity == 0u) ||
      (output->value_facts != NULL &&
       output->value_fact_capacity >= counts->reachable_values);
  const bool types_ok =
      (output->type_facts == NULL && output->type_fact_capacity == 0u) ||
      (output->type_facts != NULL &&
       output->type_fact_capacity >= counts->reachable_types);
  const bool requirements_ok =
      (output->requirement_facts == NULL &&
       output->requirement_fact_capacity == 0u) ||
      (output->requirement_facts != NULL &&
       output->requirement_fact_capacity >= counts->reachable_requirements);
  return values_ok && types_ok && requirements_ok;
}

static bool input_ranges_build(const w_seed_product_closure0_input *input,
                               closure0_range *ranges, size_t *range_count) {
  if (input == NULL || input->program == NULL || input->hir_result == NULL ||
      ranges == NULL || range_count == NULL)
    return false;
  *range_count = 0u;
#define ADD_RANGE(address, count, element_size)                               \
  (!add_range(ranges, 64u, range_count, (address), (count), (element_size)))
  if (ADD_RANGE(input, 1u, sizeof(*input)) ||
      ADD_RANGE(input->program, 1u, sizeof(*input->program)) ||
      ADD_RANGE(input->hir_result, 1u, sizeof(*input->hir_result))) {
#undef ADD_RANGE
    return false;
  }
  const w_seed_hir0_program *program = input->program;
#define ADD_HIR(address, count, element_size)                                 \
  (!add_range(ranges, 64u, range_count, (address), (count), (element_size)))
  const bool added =
      !ADD_HIR(program->modules, program->module_capacity,
               sizeof(*program->modules)) &&
      !ADD_HIR(program->functions, program->function_capacity,
               sizeof(*program->functions)) &&
      !ADD_HIR(program->identities, program->identity_capacity,
               sizeof(*program->identities)) &&
      !ADD_HIR(program->types, program->type_capacity, sizeof(*program->types)) &&
      !ADD_HIR(program->values, program->value_capacity,
               sizeof(*program->values)) &&
      !ADD_HIR(program->requirements, program->requirement_capacity,
               sizeof(*program->requirements)) &&
      !ADD_HIR(program->external_modules, program->external_module_capacity,
               sizeof(*program->external_modules)) &&
      !ADD_HIR(program->external_symbols, program->external_symbol_capacity,
               sizeof(*program->external_symbols)) &&
      !ADD_HIR(program->parameters, program->parameter_capacity,
               sizeof(*program->parameters)) &&
      !ADD_HIR(program->blocks, program->block_capacity, sizeof(*program->blocks)) &&
      !ADD_HIR(program->block_arguments, program->block_argument_capacity,
               sizeof(*program->block_arguments)) &&
      !ADD_HIR(program->edge_arguments, program->edge_argument_capacity,
               sizeof(*program->edge_arguments)) &&
      !ADD_HIR(program->switch_edges, program->switch_edge_capacity,
               sizeof(*program->switch_edges)) &&
      !ADD_HIR(program->switch_captures, program->switch_capture_capacity,
               sizeof(*program->switch_captures)) &&
      !ADD_HIR(program->instructions, program->instruction_capacity,
               sizeof(*program->instructions)) &&
      !ADD_HIR(program->bindings, program->binding_capacity,
               sizeof(*program->bindings)) &&
      !ADD_HIR(program->calls, program->call_capacity, sizeof(*program->calls)) &&
      !ADD_HIR(program->host_parameters, program->host_parameter_capacity,
               sizeof(*program->host_parameters)) &&
      !ADD_HIR(program->arguments, program->argument_capacity,
               sizeof(*program->arguments)) &&
      !ADD_HIR(program->enum_payloads, program->enum_payload_capacity,
               sizeof(*program->enum_payloads)) &&
      !ADD_HIR(program->interpolation_segments,
               program->interpolation_segment_capacity,
               sizeof(*program->interpolation_segments)) &&
      !ADD_HIR(program->terminators, program->terminator_capacity,
               sizeof(*program->terminators)) &&
      !ADD_HIR(program->entries, program->entry_capacity, sizeof(*program->entries)) &&
      !ADD_HIR(program->enums, program->enum_capacity, sizeof(*program->enums)) &&
      !ADD_HIR(program->enum_cases, program->enum_case_capacity,
               sizeof(*program->enum_cases)) &&
      !ADD_HIR(program->enum_case_parameters,
               program->enum_case_parameter_capacity,
               sizeof(*program->enum_case_parameters)) &&
      !ADD_HIR(program->enum_subset_members, program->enum_subset_member_capacity,
               sizeof(*program->enum_subset_members)) &&
      !ADD_HIR(program->text_bytes, program->text_byte_capacity, sizeof(uint8_t)) &&
      !ADD_HIR(program->value_bytes, program->value_byte_capacity, sizeof(uint8_t)) &&
      !ADD_HIR(program->receipt, program->receipt_capacity, sizeof(uint8_t));
#undef ADD_HIR
#undef ADD_RANGE
  return added;
}

static bool ranges_overlap_any(const closure0_range *ranges, size_t count) {
  if (ranges == NULL) return true;
  for (size_t first = 0u; first < count; first += 1u)
    for (size_t second = first + 1u; second < count; second += 1u)
      if (range_overlap(ranges[first].address, ranges[first].bytes,
                        ranges[second].address, ranges[second].bytes))
        return true;
  return false;
}

static bool measure_aliases_input(const w_seed_product_closure0_input *input,
                                  const w_seed_product_closure0_counts *counts,
                                  const w_seed_product_closure0_result *result) {
  closure0_range ranges[64];
  size_t range_count = 0u;
  if (!input_ranges_build(input, ranges, &range_count) || counts == NULL ||
      result == NULL ||
      !add_range(ranges, 64u, &range_count, counts, 1u, sizeof(*counts)) ||
      !add_range(ranges, 64u, &range_count, result, 1u, sizeof(*result)))
    return true;
  return ranges_overlap_any(ranges, range_count);
}

static bool output_aliases_input(
    const w_seed_product_closure0_input *input,
    const w_seed_product_closure0_output *output,
    const w_seed_product_closure0_result *result) {
  closure0_range ranges[64];
  size_t range_count = 0u;
  if (!input_ranges_build(input, ranges, &range_count) || output == NULL ||
      result == NULL ||
      !add_range(ranges, 64u, &range_count, output, 1u, sizeof(*output)) ||
      !add_range(ranges, 64u, &range_count, result, 1u, sizeof(*result)))
    return true;
#define ADD_OUT(address, count, element_size)                                 \
  (!add_range(ranges, 64u, &range_count, (address), (count), (element_size)))
  if (ADD_OUT(output->reachable_modules, output->reachable_module_capacity,
              sizeof(*output->reachable_modules)) ||
      ADD_OUT(output->omitted_modules, output->omitted_module_capacity,
              sizeof(*output->omitted_modules)) ||
      ADD_OUT(output->reachable_functions, output->reachable_function_capacity,
              sizeof(*output->reachable_functions)) ||
      ADD_OUT(output->omitted_functions, output->omitted_function_capacity,
              sizeof(*output->omitted_functions)) ||
      ADD_OUT(output->reachable_identities, output->reachable_identity_capacity,
              sizeof(*output->reachable_identities)) ||
      ADD_OUT(output->reachable_types, output->reachable_type_capacity,
              sizeof(*output->reachable_types)) ||
      ADD_OUT(output->reachable_values, output->reachable_value_capacity,
              sizeof(*output->reachable_values)) ||
      ADD_OUT(output->reachable_requirements,
              output->reachable_requirement_capacity,
              sizeof(*output->reachable_requirements)) ||
      ADD_OUT(output->reachable_external_modules,
              output->reachable_external_module_capacity,
              sizeof(*output->reachable_external_modules)) ||
      ADD_OUT(output->reachable_external_symbols,
              output->reachable_external_symbol_capacity,
              sizeof(*output->reachable_external_symbols)) ||
      ADD_OUT(output->module_remap, output->module_remap_capacity,
              sizeof(*output->module_remap)) ||
      ADD_OUT(output->function_remap, output->function_remap_capacity,
              sizeof(*output->function_remap)) ||
      ADD_OUT(output->identity_remap, output->identity_remap_capacity,
              sizeof(*output->identity_remap)) ||
      ADD_OUT(output->type_remap, output->type_remap_capacity,
              sizeof(*output->type_remap)) ||
      ADD_OUT(output->value_remap, output->value_remap_capacity,
              sizeof(*output->value_remap)) ||
      ADD_OUT(output->requirement_remap, output->requirement_remap_capacity,
              sizeof(*output->requirement_remap)) ||
      ADD_OUT(output->external_module_remap,
              output->external_module_remap_capacity,
              sizeof(*output->external_module_remap)) ||
      ADD_OUT(output->external_symbol_remap,
              output->external_symbol_remap_capacity,
              sizeof(*output->external_symbol_remap)) ||
      ADD_OUT(output->value_facts, output->value_fact_capacity,
              sizeof(*output->value_facts)) ||
      ADD_OUT(output->type_facts, output->type_fact_capacity,
              sizeof(*output->type_facts)) ||
      ADD_OUT(output->requirement_facts, output->requirement_fact_capacity,
              sizeof(*output->requirement_facts))) {
#undef ADD_OUT
    return true;
  }
#undef ADD_OUT
  return ranges_overlap_any(ranges, range_count);
}

static uint32_t value_owner_function_index(const w_seed_hir0_program *program,
                                           uint32_t value_index) {
  return value_owner_function(program, value_index);
}

static void copy_plan_to_output(const w_seed_hir0_program *program,
                                const closure0_plan *plan,
                                const w_seed_product_closure0_output *output) {
  size_t reachable_module = 0u;
  size_t omitted_module = 0u;
  size_t reachable_function = 0u;
  size_t omitted_function = 0u;
  size_t reachable_identity = 0u;
  size_t reachable_type = 0u;
  size_t reachable_value = 0u;
  size_t reachable_requirement = 0u;
  size_t reachable_external_module = 0u;
  size_t reachable_external_symbol = 0u;
  for (size_t index = 0u; index < program->module_count; index += 1u) {
    if (plan->modules[index])
      output->reachable_modules[reachable_module++] = (uint32_t)index;
    else
      output->omitted_modules[omitted_module++] = (uint32_t)index;
    output->module_remap[index] = plan->module_remap[index];
  }
  for (size_t index = 0u; index < program->function_count; index += 1u) {
    if (plan->functions[index])
      output->reachable_functions[reachable_function++] = (uint32_t)index;
    else
      output->omitted_functions[omitted_function++] = (uint32_t)index;
    output->function_remap[index] = plan->function_remap[index];
  }
  for (size_t index = 0u; index < program->identity_count; index += 1u) {
    if (plan->identities[index])
      output->reachable_identities[reachable_identity++] = (uint32_t)index;
    output->identity_remap[index] = plan->identity_remap[index];
  }
  for (size_t index = 0u; index < program->type_count; index += 1u) {
    if (plan->types[index]) {
      output->reachable_types[reachable_type++] = (uint32_t)index;
      if (output->type_facts != NULL)
        output->type_facts[reachable_type - 1u] =
            (w_seed_product_closure0_type_fact){
                (uint32_t)index, plan->type_remap[index],
                program->types[index].owner_module, program->types[index].kind};
    }
    output->type_remap[index] = plan->type_remap[index];
  }
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    if (plan->values[index]) {
      output->reachable_values[reachable_value++] = (uint32_t)index;
      if (output->value_facts != NULL)
        output->value_facts[reachable_value - 1u] =
            (w_seed_product_closure0_value_fact){
                (uint32_t)index, plan->value_remap[index],
                value_owner_function_index(program, (uint32_t)index),
                program->values[index].type_index, program->values[index].kind};
    }
    output->value_remap[index] = plan->value_remap[index];
  }
  for (size_t index = 0u; index < program->requirement_count; index += 1u) {
    if (plan->requirements[index]) {
      output->reachable_requirements[reachable_requirement++] = (uint32_t)index;
      if (output->requirement_facts != NULL)
        output->requirement_facts[reachable_requirement - 1u] =
            (w_seed_product_closure0_requirement_fact){
                (uint32_t)index, plan->requirement_remap[index],
                program->requirements[index].owner_index};
    }
    output->requirement_remap[index] = plan->requirement_remap[index];
  }
  for (size_t index = 0u; index < program->external_module_count; index += 1u) {
    if (plan->external_modules[index])
      output->reachable_external_modules[reachable_external_module++] =
          (uint32_t)index;
    output->external_module_remap[index] = plan->external_module_remap[index];
  }
  for (size_t index = 0u; index < program->external_symbol_count; index += 1u) {
    if (plan->external_symbols[index])
      output->reachable_external_symbols[reachable_external_symbol++] =
          (uint32_t)index;
    output->external_symbol_remap[index] = plan->external_symbol_remap[index];
  }
}

static bool output_matches_plan(const w_seed_hir0_program *program,
                                const closure0_plan *plan,
                                const w_seed_product_closure0_output *output,
                                const w_seed_product_closure0_result *result) {
  if (program == NULL || plan == NULL || output == NULL || result == NULL ||
      result->status != W_SEED_PRODUCT_CLOSURE0_OK ||
      result->failure != W_SEED_PRODUCT_CLOSURE0_FAILURE_NONE ||
      memcmp(&result->required, &plan->counts, sizeof(plan->counts)) != 0 ||
      memcmp(&result->written, &plan->counts, sizeof(plan->counts)) != 0 ||
      memcmp(&result->root, &plan->root, sizeof(plan->root)) != 0 ||
      memcmp(result->reachable_semantic_digest, plan->digest,
             sizeof(plan->digest)) != 0 || !output_capacity_valid(output, &plan->counts))
    return false;
  for (size_t index = 0u; index < program->module_count; index += 1u)
    if (output->module_remap[index] != plan->module_remap[index]) return false;
  for (size_t index = 0u; index < program->function_count; index += 1u)
    if (output->function_remap[index] != plan->function_remap[index]) return false;
  for (size_t index = 0u; index < program->identity_count; index += 1u)
    if (output->identity_remap[index] != plan->identity_remap[index]) return false;
  for (size_t index = 0u; index < program->type_count; index += 1u)
    if (output->type_remap[index] != plan->type_remap[index]) return false;
  for (size_t index = 0u; index < program->value_count; index += 1u)
    if (output->value_remap[index] != plan->value_remap[index]) return false;
  for (size_t index = 0u; index < program->requirement_count; index += 1u)
    if (output->requirement_remap[index] != plan->requirement_remap[index]) return false;
  for (size_t index = 0u; index < program->external_module_count; index += 1u)
    if (output->external_module_remap[index] != plan->external_module_remap[index])
      return false;
  for (size_t index = 0u; index < program->external_symbol_count; index += 1u)
    if (output->external_symbol_remap[index] != plan->external_symbol_remap[index])
      return false;
  size_t reachable_module = 0u;
  size_t omitted_module = 0u;
  for (size_t index = 0u; index < program->module_count; index += 1u) {
    if (plan->modules[index]) {
      if (output->reachable_modules[reachable_module++] != index) return false;
    } else if (output->omitted_modules[omitted_module++] != index) {
      return false;
    }
  }
  size_t reachable_function = 0u;
  size_t omitted_function = 0u;
  for (size_t index = 0u; index < program->function_count; index += 1u) {
    if (plan->functions[index]) {
      if (output->reachable_functions[reachable_function++] != index) return false;
    } else if (output->omitted_functions[omitted_function++] != index) {
      return false;
    }
  }
  size_t reachable_identity = 0u;
  for (size_t index = 0u; index < program->identity_count; index += 1u)
    if (plan->identities[index] &&
        output->reachable_identities[reachable_identity++] != index)
      return false;
  size_t reachable_type = 0u;
  for (size_t index = 0u; index < program->type_count; index += 1u)
    if (plan->types[index]) {
      if (output->reachable_types[reachable_type] != index) return false;
      if (output->type_facts != NULL &&
          (output->type_facts[reachable_type].source_index != index ||
           output->type_facts[reachable_type].closure_index !=
               plan->type_remap[index] ||
           output->type_facts[reachable_type].owner_module !=
               program->types[index].owner_module ||
           output->type_facts[reachable_type].kind !=
               program->types[index].kind))
        return false;
      reachable_type += 1u;
    }
  size_t reachable_value = 0u;
  for (size_t index = 0u; index < program->value_count; index += 1u)
    if (plan->values[index]) {
      if (output->reachable_values[reachable_value] != index) return false;
      if (output->value_facts != NULL &&
          (output->value_facts[reachable_value].source_index != index ||
           output->value_facts[reachable_value].closure_index !=
               plan->value_remap[index] ||
           output->value_facts[reachable_value].owner_function !=
               value_owner_function_index(program, (uint32_t)index) ||
           output->value_facts[reachable_value].type_index !=
               program->values[index].type_index ||
           output->value_facts[reachable_value].kind !=
               program->values[index].kind))
        return false;
      reachable_value += 1u;
    }
  size_t reachable_requirement = 0u;
  for (size_t index = 0u; index < program->requirement_count; index += 1u)
    if (plan->requirements[index]) {
      if (output->reachable_requirements[reachable_requirement] != index)
        return false;
      if (output->requirement_facts != NULL &&
          (output->requirement_facts[reachable_requirement].source_index !=
               index ||
           output->requirement_facts[reachable_requirement].closure_index !=
               plan->requirement_remap[index] ||
           output->requirement_facts[reachable_requirement].owner_identity !=
               program->requirements[index].owner_index))
        return false;
      reachable_requirement += 1u;
    }
  size_t reachable_external_module = 0u;
  for (size_t index = 0u; index < program->external_module_count; index += 1u)
    if (plan->external_modules[index] &&
        output->reachable_external_modules[reachable_external_module++] != index)
      return false;
  size_t reachable_external_symbol = 0u;
  for (size_t index = 0u; index < program->external_symbol_count; index += 1u)
    if (plan->external_symbols[index] &&
        output->reachable_external_symbols[reachable_external_symbol++] != index)
      return false;
  return true;
}

w_seed_product_closure0_status w_seed_product_closure0_measure(
    const w_seed_product_closure0_input *input,
    w_seed_product_closure0_counts *counts,
    w_seed_product_closure0_result *result) {
  if (counts == NULL || result == NULL) return W_SEED_PRODUCT_CLOSURE0_INVALID;
  closure0_plan plan;
  w_seed_product_closure0_result error;
  result_init(&error);
  if (!build_plan(input, &plan, &error)) return error.status;
  /* HIR verification and the complete bounded plan precede every read of
   * caller-supplied capacities used by the alias table.  Publication still
   * happens only after the alias barrier. */
  if (measure_aliases_input(input, counts, result))
    return W_SEED_PRODUCT_CLOSURE0_INVALID;
  compute_digest(input->program, &plan, plan.digest);
  w_seed_product_closure0_result candidate;
  finish_plan_result(&plan, &candidate);
  *counts = plan.counts;
  *result = candidate;
  return W_SEED_PRODUCT_CLOSURE0_OK;
}

w_seed_product_closure0_status w_seed_product_closure0_run(
    const w_seed_product_closure0_input *input,
    const w_seed_product_closure0_output *output,
    w_seed_product_closure0_result *result) {
  if (result == NULL || output == NULL) return W_SEED_PRODUCT_CLOSURE0_INVALID;
  closure0_plan plan;
  w_seed_product_closure0_result error;
  result_init(&error);
  if (!build_plan(input, &plan, &error)) return error.status;
  compute_digest(input->program, &plan, plan.digest);
  if (!output_capacity_valid(output, &plan.counts))
    return W_SEED_PRODUCT_CLOSURE0_CAPACITY;
  if (output_aliases_input(input, output, result))
    return W_SEED_PRODUCT_CLOSURE0_INVALID;
  copy_plan_to_output(input->program, &plan, output);
  w_seed_product_closure0_result candidate;
  finish_plan_result(&plan, &candidate);
  *result = candidate;
  return W_SEED_PRODUCT_CLOSURE0_OK;
}

bool w_seed_product_closure0_verify(
    const w_seed_product_closure0_input *input,
    const w_seed_product_closure0_output *output,
    const w_seed_product_closure0_result *result) {
  if (output == NULL || result == NULL) return false;
  closure0_plan plan;
  w_seed_product_closure0_result error;
  result_init(&error);
  if (!build_plan(input, &plan, &error)) return false;
  compute_digest(input->program, &plan, plan.digest);
  if (output_aliases_input(input, output, result)) return false;
  return output_matches_plan(input->program, &plan, output, result);
}

bool w_seed_product_closure0_cross_check_functions(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const bool *expected_reachable_functions,
    size_t expected_capacity) {
  if (program == NULL || hir_result == NULL || expected_reachable_functions == NULL ||
      expected_capacity == 0u)
    return false;
  w_seed_product_closure0_input input = {program, hir_result};
  closure0_plan plan;
  w_seed_product_closure0_result error;
  result_init(&error);
  if (!build_plan(&input, &plan, &error)) return false;
  if (expected_capacity < plan.counts.functions) return false;
  for (size_t index = 0u; index < program->function_count; index += 1u)
    if (expected_reachable_functions[index] != plan.functions[index]) return false;
  return true;
}
