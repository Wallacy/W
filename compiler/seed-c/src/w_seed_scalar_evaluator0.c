#include "w_seed_scalar_evaluator0.h"

#include <limits.h>

bool w_seed_scalar_evaluator0_checked_binary(
    w_seed_hir0_binary_operator operation, int64_t left, int64_t right,
    int64_t *result) {
  if (result == NULL) return false;
  switch (operation) {
    case W_SEED_HIR0_BINARY_ADD:
      if ((right > 0 && left > INT64_MAX - right) ||
          (right < 0 && left < INT64_MIN - right))
        return false;
      *result = left + right;
      return true;
    case W_SEED_HIR0_BINARY_SUBTRACT:
      if ((right < 0 && left > INT64_MAX + right) ||
          (right > 0 && left < INT64_MIN + right))
        return false;
      *result = left - right;
      return true;
    case W_SEED_HIR0_BINARY_MULTIPLY:
      if (left == 0 || right == 0) {
        *result = 0;
        return true;
      }
      if (left == -1 || right == -1) {
        const int64_t other = left == -1 ? right : left;
        if (other == INT64_MIN) return false;
        *result = -other;
        return true;
      }
      if ((left > 0 && right > 0 && left > INT64_MAX / right) ||
          (left > 0 && right < 0 && right < INT64_MIN / left) ||
          (left < 0 && right > 0 && left < INT64_MIN / right) ||
          (left < 0 && right < 0 && left < INT64_MAX / right))
        return false;
      *result = left * right;
      return true;
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

static bool scalar_i64_type(const w_seed_hir0_program *program,
                            uint32_t type_index) {
  return program != NULL && type_index < program->type_count &&
         program->types[type_index].kind == W_SEED_HIR0_TYPE_I64;
}

static bool scalar_range(uint32_t first, uint32_t count, size_t total) {
  return first != W_SEED_HIR0_NONE && first <= total && count <= total - first;
}

/* Scalar evaluation is pure. Resolve a callee parameter through the verified
 * HIR argument relation instead of materializing an arity-sized stack array.
 * The frame size is constant and only call depth consumes stack space. */
typedef struct scalar_parameter_frame {
  const struct scalar_parameter_frame *caller;
  uint32_t call_index;
  size_t parameter_count;
} scalar_parameter_frame;

static bool scalar_evaluate_function(const w_seed_hir0_program *program,
                                     uint32_t function_index,
                                     const scalar_parameter_frame *parameters,
                                     size_t parameter_count, size_t depth,
                                     size_t *budget, int64_t *result);

static bool scalar_evaluate_value(const w_seed_hir0_program *program,
                                  uint32_t value_index,
                                  const scalar_parameter_frame *parameters,
                                  size_t parameter_count, size_t depth,
                                  size_t *budget, int64_t *result);

static const w_seed_hir0_argument *scalar_argument_for_ordinal(
    const w_seed_hir0_program *program,
    const scalar_parameter_frame *parameters, size_t ordinal) {
  if (program == NULL || parameters == NULL ||
      parameters->call_index >= program->call_count ||
      ordinal >= parameters->parameter_count)
    return NULL;
  const w_seed_hir0_call *call = &program->calls[parameters->call_index];
  if (call->argument_count != parameters->parameter_count ||
      !scalar_range(call->first_argument, call->argument_count,
                    program->argument_count))
    return NULL;
  const w_seed_hir0_argument *match = NULL;
  for (size_t index = 0u; index < call->argument_count; index += 1u) {
    const w_seed_hir0_argument *argument =
        &program->arguments[(size_t)call->first_argument + index];
    if (argument->owner_call != parameters->call_index ||
        argument->parameter_ordinal != ordinal)
      continue;
    if (match != NULL) return NULL;
    match = argument;
  }
  return match;
}

static bool scalar_evaluate_call(const w_seed_hir0_program *program,
                                 uint32_t call_index,
                                 const scalar_parameter_frame *caller_parameters,
                                 size_t caller_parameter_count,
                                 bool root_dispatch, size_t depth,
                                 size_t *budget, int64_t *result) {
  if (program == NULL || result == NULL || budget == NULL || *budget == 0u ||
      depth > W_SEED_HIR0_MAX_NESTING || call_index >= program->call_count)
    return false;
  *budget -= 1u;
  const w_seed_hir0_call *call = &program->calls[call_index];
  const bool dispatch =
      call->execution_kind == W_SEED_HIR0_CALL_DIRECT ||
      (root_dispatch &&
       (call->execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_COOPERATIVE_TRACE ||
        call->execution_kind == W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH ||
        call->execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH));
  if (!dispatch || call->callee_identity >= program->identity_count ||
      !scalar_range(call->first_argument, call->argument_count,
                    program->argument_count))
    return false;
  const w_seed_hir0_identity *identity =
      &program->identities[call->callee_identity];
  if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      identity->target_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function =
      &program->functions[identity->target_index];
  if (function->parameter_count != call->argument_count ||
      !scalar_i64_type(program, function->return_type))
    return false;
  for (size_t index = 0u; index < call->argument_count; index += 1u) {
    const w_seed_hir0_argument *argument =
        &program->arguments[(size_t)call->first_argument + index];
    if (argument->owner_call != call_index ||
        argument->parameter_ordinal >= call->argument_count)
      return false;
    for (size_t previous = 0u; previous < index; previous += 1u) {
      const w_seed_hir0_argument *prior =
          &program->arguments[(size_t)call->first_argument + previous];
      if (prior->parameter_ordinal == argument->parameter_ordinal) return false;
    }
    int64_t ignored = 0;
    if (!scalar_evaluate_value(program, argument->value_index,
                               caller_parameters, caller_parameter_count,
                               depth + 1u, budget, &ignored))
      return false;
  }
  const scalar_parameter_frame parameters = {
      caller_parameters, call_index, call->argument_count};
  return scalar_evaluate_function(program, identity->target_index, &parameters,
                                  call->argument_count, depth + 1u, budget,
                                  result);
}

static bool scalar_evaluate_value(const w_seed_hir0_program *program,
                                  uint32_t value_index,
                                  const scalar_parameter_frame *parameters,
                                  size_t parameter_count, size_t depth,
                                  size_t *budget, int64_t *result) {
  if (program == NULL || result == NULL || budget == NULL || *budget == 0u ||
      depth > W_SEED_HIR0_MAX_NESTING || value_index >= program->value_count)
    return false;
  *budget -= 1u;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (!scalar_i64_type(program, value->type_index)) return false;
  switch (value->kind) {
    case W_SEED_HIR0_VALUE_CONST_I64:
      *result = value->integer_value;
      return true;
    case W_SEED_HIR0_VALUE_PARAMETER_READ: {
      if (value->parameter_index >= program->parameter_count) return false;
      const size_t ordinal = program->parameters[value->parameter_index].ordinal;
      const w_seed_hir0_argument *argument =
          scalar_argument_for_ordinal(program, parameters, ordinal);
      return argument != NULL && parameter_count == parameters->parameter_count &&
             scalar_evaluate_value(program, argument->value_index,
                                   parameters->caller,
                                   parameters->caller == NULL
                                       ? 0u
                                       : parameters->caller->parameter_count,
                                   depth + 1u, budget, result);
    }
    case W_SEED_HIR0_VALUE_BINDING_READ:
      return value->binding_index < program->binding_count &&
             scalar_evaluate_value(
                 program,
                 program->bindings[value->binding_index].initializer_value,
                 parameters, parameter_count, depth + 1u, budget, result);
    case W_SEED_HIR0_VALUE_CALL_RESULT:
      return scalar_evaluate_call(program, value->call_index, parameters,
                                  parameter_count, false, depth + 1u, budget,
                                  result);
    case W_SEED_HIR0_VALUE_UNARY_I64: {
      int64_t operand = 0;
      if (value->unary_operator != W_SEED_HIR0_UNARY_NEGATE ||
          !scalar_evaluate_value(program, value->left_value, parameters,
                                 parameter_count, depth + 1u, budget,
                                 &operand) ||
          operand == INT64_MIN)
        return false;
      *result = -operand;
      return true;
    }
    case W_SEED_HIR0_VALUE_BINARY_I64: {
      int64_t left = 0;
      int64_t right = 0;
      return scalar_evaluate_value(program, value->left_value, parameters,
                                   parameter_count, depth + 1u, budget,
                                   &left) &&
             scalar_evaluate_value(program, value->right_value, parameters,
                                   parameter_count, depth + 1u, budget,
                                   &right) &&
             w_seed_scalar_evaluator0_checked_binary(
                 value->binary_operator, left, right, result);
    }
    default:
      return false;
  }
}

static bool scalar_evaluate_function(const w_seed_hir0_program *program,
                                     uint32_t function_index,
                                     const scalar_parameter_frame *parameters,
                                     size_t parameter_count, size_t depth,
                                     size_t *budget, int64_t *result) {
  if (program == NULL || result == NULL || budget == NULL || *budget == 0u ||
      depth > W_SEED_HIR0_MAX_NESTING ||
      function_index >= program->function_count)
    return false;
  *budget -= 1u;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->parameter_count != parameter_count ||
      function->block_count != 1u ||
      function->first_block >= program->block_count ||
      !scalar_i64_type(program, function->return_type))
    return false;
  const w_seed_hir0_block *block = &program->blocks[function->first_block];
  if (!scalar_range(block->first_instruction, block->instruction_count,
                    program->instruction_count) ||
      block->terminator_index >= program->terminator_count)
    return false;
  for (size_t ordinal = 0u; ordinal < block->instruction_count; ordinal += 1u) {
    const w_seed_hir0_instruction *instruction =
        &program->instructions[(size_t)block->first_instruction + ordinal];
    int64_t ignored = 0;
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD)
      continue;
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= program->binding_count ||
          !scalar_evaluate_value(
              program,
              program->bindings[instruction->binding_index].initializer_value,
              parameters, parameter_count, depth + 1u, budget, &ignored))
        return false;
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (!scalar_evaluate_call(program, instruction->call_index, parameters,
                                parameter_count, false, depth + 1u, budget,
                                &ignored))
        return false;
    } else {
      return false;
    }
  }
  const w_seed_hir0_terminator *terminator =
      &program->terminators[block->terminator_index];
  return terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
         scalar_evaluate_value(program, terminator->value_index, parameters,
                               parameter_count, depth + 1u, budget, result);
}

bool w_seed_scalar_evaluator0_evaluate_call(
    const w_seed_hir0_program *program, uint32_t call_index, size_t *budget,
    int64_t *result) {
  return scalar_evaluate_call(program, call_index, NULL, 0u, true, 0u, budget,
                              result);
}
