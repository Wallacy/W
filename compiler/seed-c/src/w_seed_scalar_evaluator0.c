#include "w_seed_scalar_evaluator0.h"

#include <limits.h>
#include <string.h>

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
    case W_SEED_HIR0_BINARY_BIT_AND:
      *result = left & right;
      return true;
    case W_SEED_HIR0_BINARY_BIT_OR:
      *result = left | right;
      return true;
    case W_SEED_HIR0_BINARY_BIT_XOR:
      *result = left ^ right;
      return true;
    default:
      return false;
  }
}

static uint64_t scalar_width_mask(uint16_t bit_width) {
  return bit_width == 64u
             ? UINT64_MAX
             : (UINT64_C(1) << bit_width) - UINT64_C(1);
}

static uint64_t scalar_sign_extend_bits(uint64_t bits, uint16_t bit_width) {
  if (bit_width == 64u) return bits;
  const uint64_t mask = scalar_width_mask(bit_width);
  const uint64_t normalized = bits & mask;
  const uint64_t sign_bit = UINT64_C(1) << (bit_width - 1u);
  return (normalized & sign_bit) != 0u ? normalized | ~mask : normalized;
}

/* Shift in the logical integer domain, never in a signed C type. The result
 * is the normalized N-bit pattern; the caller performs signed extension for
 * signed values or keeps the zero-extended unsigned pattern. */
static uint64_t scalar_arithmetic_shift_right_bits(uint64_t bits,
                                                   uint16_t bit_width,
                                                   uint64_t count) {
  const uint64_t mask = scalar_width_mask(bit_width);
  const uint64_t normalized = bits & mask;
  uint64_t shifted = normalized >> count;
  const uint64_t sign_bit = UINT64_C(1) << (bit_width - 1u);
  if (count != 0u && (normalized & sign_bit) != 0u)
    shifted |= mask & ~(mask >> count);
  return shifted & mask;
}

static bool scalar_checked_integer_shift(
    w_seed_hir0_binary_operator operation, bool is_signed,
    uint16_t bit_width, uint64_t left_bits, uint64_t count,
    uint64_t *result_bits) {
  if (result_bits == NULL ||
      (bit_width != 8u && bit_width != 16u && bit_width != 32u &&
       bit_width != 64u) ||
      (operation != W_SEED_HIR0_BINARY_SHIFT_LEFT &&
       operation != W_SEED_HIR0_BINARY_SHIFT_RIGHT))
    return false;

  const uint64_t mask = scalar_width_mask(bit_width);
  const uint64_t left = left_bits & mask;
  const uint64_t carrier = is_signed ? scalar_sign_extend_bits(left, bit_width)
                                     : left;
  if (left_bits != carrier || count >= bit_width) return false;

  uint64_t candidate = 0u;
  if (operation == W_SEED_HIR0_BINARY_SHIFT_LEFT) {
    candidate = (left << count) & mask;
    if (is_signed) {
      if (scalar_arithmetic_shift_right_bits(candidate, bit_width, count) !=
          left)
        return false;
    } else if ((candidate >> count) != left) {
      return false;
    }
  } else if (is_signed) {
    candidate = scalar_arithmetic_shift_right_bits(left, bit_width, count);
  } else {
    candidate = left >> count;
  }

  *result_bits = candidate;
  return true;
}

bool w_seed_scalar_evaluator0_checked_integer_arithmetic(
    w_seed_hir0_binary_operator operation, bool is_signed,
    uint16_t bit_width, uint64_t left_bits, uint64_t right_bits,
    uint64_t *result_bits) {
  if (result_bits == NULL ||
      (bit_width != 8u && bit_width != 16u && bit_width != 32u &&
       bit_width != 64u) ||
      (operation != W_SEED_HIR0_BINARY_ADD &&
       operation != W_SEED_HIR0_BINARY_SUBTRACT &&
       operation != W_SEED_HIR0_BINARY_MULTIPLY &&
       operation != W_SEED_HIR0_BINARY_DIVIDE &&
       operation != W_SEED_HIR0_BINARY_REMAINDER))
    return false;

  const uint64_t mask = scalar_width_mask(bit_width);
  if (is_signed) {
    if (left_bits != scalar_sign_extend_bits(left_bits, bit_width) ||
        right_bits != scalar_sign_extend_bits(right_bits, bit_width))
      return false;
    int64_t left = 0;
    int64_t right = 0;
    int64_t candidate = 0;
    (void)memcpy(&left, &left_bits, sizeof(left));
    (void)memcpy(&right, &right_bits, sizeof(right));
    if (!w_seed_scalar_evaluator0_checked_binary(
            operation, left, right, &candidate))
      return false;
    if (bit_width < 64u) {
      const int64_t magnitude = INT64_C(1) << (bit_width - 1u);
      if (candidate < -magnitude || candidate >= magnitude) return false;
    }
    const uint64_t result = (uint64_t)candidate;
    *result_bits = result;
    return true;
  }

  if (left_bits > mask || right_bits > mask) return false;
  uint64_t candidate = 0u;
  switch (operation) {
    case W_SEED_HIR0_BINARY_ADD:
      if (right_bits > mask - left_bits) return false;
      candidate = left_bits + right_bits;
      break;
    case W_SEED_HIR0_BINARY_SUBTRACT:
      if (left_bits < right_bits) return false;
      candidate = left_bits - right_bits;
      break;
    case W_SEED_HIR0_BINARY_MULTIPLY:
      if (left_bits != 0u && right_bits > mask / left_bits) return false;
      candidate = left_bits * right_bits;
      break;
    case W_SEED_HIR0_BINARY_DIVIDE:
      if (right_bits == 0u) return false;
      candidate = left_bits / right_bits;
      break;
    case W_SEED_HIR0_BINARY_REMAINDER:
      if (right_bits == 0u) return false;
      candidate = left_bits % right_bits;
      break;
    default:
      return false;
  }
  *result_bits = candidate;
  return true;
}

static bool scalar_i64_type(const w_seed_hir0_program *program,
                            uint32_t type_index) {
  return program != NULL && type_index < program->type_count &&
         program->types[type_index].kind == W_SEED_HIR0_TYPE_I64;
}

typedef struct {
  bool is_signed;
  uint16_t bit_width;
} scalar_integer_facts;

static bool scalar_integer_type_facts(const w_seed_hir0_program *program,
                                      uint32_t type_index,
                                      scalar_integer_facts *facts) {
  if (program == NULL || facts == NULL || type_index >= program->type_count)
    return false;
  const w_seed_hir0_type *type = &program->types[type_index];
  if (type->kind == W_SEED_HIR0_TYPE_I64) {
    if (!type->integer_is_signed || type->integer_bit_width != 64u)
      return false;
  } else if (type->kind == W_SEED_HIR0_TYPE_U64) {
    if (type->integer_is_signed || type->integer_bit_width != 64u)
      return false;
  } else if (type->kind == W_SEED_HIR0_TYPE_INTEGER) {
    if (type->integer_bit_width != 8u && type->integer_bit_width != 16u &&
        type->integer_bit_width != 32u)
      return false;
  } else {
    return false;
  }
  facts->is_signed = type->integer_is_signed;
  facts->bit_width = type->integer_bit_width;
  return true;
}

static int64_t scalar_signed_integer_bits(uint64_t bits,
                                         uint16_t bit_width) {
  if (bit_width == 64u) {
    int64_t value = 0;
    (void)memcpy(&value, &bits, sizeof(value));
    return value;
  }
  const uint64_t mask = (UINT64_C(1) << bit_width) - UINT64_C(1);
  bits &= mask;
  const uint64_t sign_bit = UINT64_C(1) << (bit_width - 1u);
  if ((bits & sign_bit) == 0u) return (int64_t)bits;
  const uint64_t magnitude = ((~bits) & mask) + UINT64_C(1);
  return -(int64_t)magnitude;
}

static bool scalar_integer_bitwise(w_seed_hir0_binary_operator operation,
                                  uint16_t bit_width, uint64_t left_bits,
                                  uint64_t right_bits,
                                  uint64_t *result_bits) {
  if (result_bits == NULL ||
      (bit_width != 8u && bit_width != 16u && bit_width != 32u &&
       bit_width != 64u))
    return false;
  const uint64_t mask = bit_width == 64u
                            ? UINT64_MAX
                            : (UINT64_C(1) << bit_width) - UINT64_C(1);
  left_bits &= mask;
  right_bits &= mask;
  switch (operation) {
    case W_SEED_HIR0_BINARY_BIT_AND:
      *result_bits = left_bits & right_bits;
      break;
    case W_SEED_HIR0_BINARY_BIT_OR:
      *result_bits = left_bits | right_bits;
      break;
    case W_SEED_HIR0_BINARY_BIT_XOR:
      *result_bits = left_bits ^ right_bits;
      break;
    default:
      return false;
  }
  *result_bits &= mask;
  return true;
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
  scalar_integer_facts return_facts;
  if (function->parameter_count != call->argument_count ||
      !scalar_integer_type_facts(program, function->return_type,
                                 &return_facts))
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
  const bool bool_comparison =
      value->kind == W_SEED_HIR0_VALUE_BINARY_INTEGER_COMPARISON &&
      value->type_index < program->type_count &&
      program->types[value->type_index].kind == W_SEED_HIR0_TYPE_BOOL;
  scalar_integer_facts value_facts = {0};
  if (!bool_comparison &&
      !scalar_integer_type_facts(program, value->type_index, &value_facts))
    return false;
  switch (value->kind) {
    case W_SEED_HIR0_VALUE_CONST_I64:
      if (!scalar_integer_type_facts(program, value->type_index,
                                     &value_facts) ||
          !value_facts.is_signed)
        return false;
      *result = value->integer_value;
      return true;
    case W_SEED_HIR0_VALUE_CONST_U64:
      if (!scalar_integer_type_facts(program, value->type_index,
                                     &value_facts) ||
          value_facts.is_signed)
        return false;
      (void)memcpy(result, &value->unsigned_integer_value, sizeof(*result));
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
    case W_SEED_HIR0_VALUE_INTEGER_TRUNCATING_BITS: {
      scalar_integer_facts source_facts;
      scalar_integer_facts destination_facts;
      scalar_integer_facts child_facts;
      int64_t source_value = 0;
      if (value->source_type >= program->type_count ||
          value->left_value == W_SEED_HIR0_NONE ||
          value->left_value >= program->value_count ||
          value->right_value != W_SEED_HIR0_NONE ||
          !scalar_integer_type_facts(program, value->source_type,
                                     &source_facts) ||
          !scalar_integer_type_facts(program, value->type_index,
                                     &destination_facts) ||
          !scalar_integer_type_facts(program, value->type_index,
                                     &value_facts) ||
          destination_facts.is_signed != value_facts.is_signed ||
          destination_facts.bit_width != value_facts.bit_width ||
          !scalar_integer_type_facts(
              program, program->values[value->left_value].type_index,
              &child_facts) ||
          child_facts.is_signed != source_facts.is_signed ||
          child_facts.bit_width != source_facts.bit_width ||
          program->values[value->left_value].type_index != value->source_type ||
          program->values[value->left_value].owner_kind !=
              W_SEED_HIR0_VALUE_OWNER_INTEGER_TRUNCATING_BITS ||
          program->values[value->left_value].owner_index != value_index ||
          program->values[value->left_value].owner_ordinal != 0u ||
          !scalar_evaluate_value(program, value->left_value, parameters,
                                 parameter_count, depth + 1u, budget,
                                 &source_value))
        return false;
      const uint64_t destination_mask =
          destination_facts.bit_width == 64u
              ? UINT64_MAX
              : (UINT64_C(1) << destination_facts.bit_width) - UINT64_C(1);
      const uint64_t destination_bits =
          (uint64_t)source_value & destination_mask;
      if (destination_facts.is_signed) {
        *result = scalar_signed_integer_bits(destination_bits,
                                             destination_facts.bit_width);
      } else {
        (void)memcpy(result, &destination_bits, sizeof(*result));
      }
      return true;
    }
    case W_SEED_HIR0_VALUE_UNARY_I64: {
      int64_t operand = 0;
      scalar_integer_facts operand_facts;
      if (!scalar_integer_type_facts(program, value->type_index,
                                     &value_facts) ||
          !value_facts.is_signed ||
          (value->unary_operator != W_SEED_HIR0_UNARY_NEGATE &&
           value->unary_operator != W_SEED_HIR0_UNARY_BIT_NOT) ||
          value->left_value >= program->value_count ||
          !scalar_integer_type_facts(
              program, program->values[value->left_value].type_index,
              &operand_facts) ||
          operand_facts.is_signed != value_facts.is_signed ||
          operand_facts.bit_width != value_facts.bit_width ||
          !scalar_evaluate_value(program, value->left_value, parameters,
                                 parameter_count, depth + 1u, budget, &operand))
        return false;
      const uint64_t mask = value_facts.bit_width == 64u
                                ? UINT64_MAX
                                : (UINT64_C(1) << value_facts.bit_width) - 1u;
      const uint64_t operand_bits = (uint64_t)operand & mask;
      uint64_t result_bits = 0u;
      if (value->unary_operator == W_SEED_HIR0_UNARY_NEGATE) {
        const uint64_t minimum =
            UINT64_C(1) << (value_facts.bit_width - 1u);
        if (operand_bits == minimum) return false;
        result_bits = (UINT64_C(0) - operand_bits) & mask;
      } else {
        result_bits = (~operand_bits) & mask;
      }
      *result = scalar_signed_integer_bits(result_bits,
                                           value_facts.bit_width);
      return true;
    }
    case W_SEED_HIR0_VALUE_UNARY_U64: {
      int64_t operand = 0;
      scalar_integer_facts operand_facts;
      if (!scalar_integer_type_facts(program, value->type_index,
                                     &value_facts) ||
          value_facts.is_signed ||
          value->unary_operator != W_SEED_HIR0_UNARY_BIT_NOT ||
          value->left_value >= program->value_count ||
          !scalar_integer_type_facts(
              program, program->values[value->left_value].type_index,
              &operand_facts) ||
          operand_facts.is_signed ||
          operand_facts.bit_width != value_facts.bit_width ||
          !scalar_evaluate_value(program, value->left_value, parameters,
                                 parameter_count, depth + 1u, budget,
                                 &operand))
        return false;
      const uint64_t mask = value_facts.bit_width == 64u
                                ? UINT64_MAX
                                : (UINT64_C(1) << value_facts.bit_width) - 1u;
      const uint64_t result_bits = (~(uint64_t)operand) & mask;
      (void)memcpy(result, &result_bits, sizeof(*result));
      return true;
    }
    case W_SEED_HIR0_VALUE_BINARY_I64: {
      int64_t left = 0;
      int64_t right = 0;
      const bool checked_arithmetic =
          value->binary_operator == W_SEED_HIR0_BINARY_ADD ||
          value->binary_operator == W_SEED_HIR0_BINARY_SUBTRACT ||
          value->binary_operator == W_SEED_HIR0_BINARY_MULTIPLY ||
          value->binary_operator == W_SEED_HIR0_BINARY_DIVIDE ||
          value->binary_operator == W_SEED_HIR0_BINARY_REMAINDER;
      const bool checked_shift =
          value->binary_operator == W_SEED_HIR0_BINARY_SHIFT_LEFT ||
          value->binary_operator == W_SEED_HIR0_BINARY_SHIFT_RIGHT;
      const bool bitwise =
          value->binary_operator >= W_SEED_HIR0_BINARY_BIT_AND &&
          value->binary_operator <= W_SEED_HIR0_BINARY_BIT_XOR;
      scalar_integer_facts value_type;
      scalar_integer_facts left_type;
      scalar_integer_facts right_type;
      if (!scalar_integer_type_facts(program, value->type_index,
                                     &value_type) ||
          !value_type.is_signed || value->left_value >= program->value_count ||
          value->right_value >= program->value_count ||
          !scalar_integer_type_facts(
              program, program->values[value->left_value].type_index,
              &left_type) ||
          !scalar_integer_type_facts(
              program, program->values[value->right_value].type_index,
              &right_type) ||
          left_type.is_signed != value_type.is_signed ||
          left_type.bit_width != value_type.bit_width ||
          (checked_shift
               ? (right_type.is_signed || right_type.bit_width != 64u ||
                  program->values[value->left_value].type_index !=
                      value->type_index)
               : (right_type.is_signed != value_type.is_signed ||
                  right_type.bit_width != value_type.bit_width)) ||
          (bitwise &&
           (program->values[value->left_value].type_index !=
                value->type_index ||
            program->values[value->right_value].type_index !=
                value->type_index)))
        return false;
      if (!scalar_evaluate_value(program, value->left_value, parameters,
                                 parameter_count, depth + 1u, budget, &left) ||
          !scalar_evaluate_value(program, value->right_value, parameters,
                                 parameter_count, depth + 1u, budget, &right))
        return false;
      if (bitwise) {
        uint64_t result_bits = 0u;
        if (!scalar_integer_bitwise(value->binary_operator,
                                    value_type.bit_width, (uint64_t)left,
                                    (uint64_t)right, &result_bits))
          return false;
        *result = scalar_signed_integer_bits(result_bits,
                                             value_type.bit_width);
        return true;
      }
      if (checked_shift) {
        uint64_t result_bits = 0u;
        if (!scalar_checked_integer_shift(
                value->binary_operator, true, value_type.bit_width,
                (uint64_t)left, (uint64_t)right, &result_bits))
          return false;
        *result = scalar_signed_integer_bits(result_bits,
                                             value_type.bit_width);
        return true;
      }
      if (checked_arithmetic) {
        uint64_t result_bits = 0u;
        const uint64_t left_bits = (uint64_t)left;
        const uint64_t right_bits = (uint64_t)right;
        if (!w_seed_scalar_evaluator0_checked_integer_arithmetic(
                value->binary_operator, value_type.is_signed,
                value_type.bit_width, left_bits, right_bits, &result_bits))
          return false;
        (void)memcpy(result, &result_bits, sizeof(*result));
        return true;
      }
      return scalar_i64_type(program, value->type_index) &&
             w_seed_scalar_evaluator0_checked_binary(
                 value->binary_operator, left, right, result);
    }
    case W_SEED_HIR0_VALUE_BINARY_U64: {
      const bool checked_shift =
          value->binary_operator == W_SEED_HIR0_BINARY_SHIFT_LEFT ||
          value->binary_operator == W_SEED_HIR0_BINARY_SHIFT_RIGHT;
      const bool bitwise =
          value->binary_operator >= W_SEED_HIR0_BINARY_BIT_AND &&
          value->binary_operator <= W_SEED_HIR0_BINARY_BIT_XOR;
      scalar_integer_facts value_type;
      scalar_integer_facts left_type;
      scalar_integer_facts right_type;
      if ((!bitwise && !checked_shift &&
           value->binary_operator != W_SEED_HIR0_BINARY_ADD &&
           value->binary_operator != W_SEED_HIR0_BINARY_SUBTRACT &&
           value->binary_operator != W_SEED_HIR0_BINARY_MULTIPLY &&
           value->binary_operator != W_SEED_HIR0_BINARY_DIVIDE &&
           value->binary_operator != W_SEED_HIR0_BINARY_REMAINDER) ||
          !scalar_integer_type_facts(program, value->type_index,
                                     &value_type) ||
          value_type.is_signed || value->left_value >= program->value_count ||
          value->right_value >= program->value_count ||
          !scalar_integer_type_facts(
              program, program->values[value->left_value].type_index,
              &left_type) ||
          !scalar_integer_type_facts(
              program, program->values[value->right_value].type_index,
              &right_type) ||
          left_type.is_signed || right_type.is_signed ||
          left_type.bit_width != value_type.bit_width ||
          (checked_shift
               ? (right_type.bit_width != 64u ||
                  program->values[value->left_value].type_index !=
                      value->type_index)
               : right_type.bit_width != value_type.bit_width) ||
          (bitwise &&
           (program->values[value->left_value].type_index !=
                value->type_index ||
            program->values[value->right_value].type_index !=
                value->type_index)))
        return false;
      int64_t left = 0;
      int64_t right = 0;
      uint64_t result_bits = 0u;
      if (!scalar_evaluate_value(program, value->left_value, parameters,
                                 parameter_count, depth + 1u, budget, &left) ||
          !scalar_evaluate_value(program, value->right_value, parameters,
                                 parameter_count, depth + 1u, budget, &right))
        return false;
      if (bitwise) {
        if (!scalar_integer_bitwise(value->binary_operator,
                                    value_type.bit_width, (uint64_t)left,
                                    (uint64_t)right, &result_bits))
          return false;
        (void)memcpy(result, &result_bits, sizeof(*result));
        return true;
      }
      if (checked_shift) {
        if (!scalar_checked_integer_shift(
                value->binary_operator, false, value_type.bit_width,
                (uint64_t)left, (uint64_t)right, &result_bits))
          return false;
        (void)memcpy(result, &result_bits, sizeof(*result));
        return true;
      }
      if (!w_seed_scalar_evaluator0_checked_integer_arithmetic(
              value->binary_operator, false, value_type.bit_width,
              (uint64_t)left, (uint64_t)right, &result_bits))
        return false;
      (void)memcpy(result, &result_bits, sizeof(*result));
      return true;
    }
    case W_SEED_HIR0_VALUE_BINARY_INTEGER_COMPARISON: {
      scalar_integer_facts left_facts;
      scalar_integer_facts right_facts;
      if (!bool_comparison ||
          value->binary_operator < W_SEED_HIR0_BINARY_EQUAL ||
          value->binary_operator > W_SEED_HIR0_BINARY_GREATER_EQUAL ||
          value->left_value >= program->value_count ||
          value->right_value >= program->value_count ||
          !scalar_integer_type_facts(
              program, program->values[value->left_value].type_index,
              &left_facts) ||
          !scalar_integer_type_facts(
              program, program->values[value->right_value].type_index,
              &right_facts) ||
          left_facts.is_signed != right_facts.is_signed ||
          left_facts.bit_width != right_facts.bit_width)
        return false;
      int64_t left = 0;
      int64_t right = 0;
      if (!scalar_evaluate_value(program, value->left_value, parameters,
                                 parameter_count, depth + 1u, budget, &left) ||
          !scalar_evaluate_value(program, value->right_value, parameters,
                                 parameter_count, depth + 1u, budget, &right))
        return false;
      uint64_t left_bits = (uint64_t)left;
      uint64_t right_bits = (uint64_t)right;
      if (left_facts.bit_width != 64u) {
        const uint64_t mask =
            (UINT64_C(1) << left_facts.bit_width) - UINT64_C(1);
        left_bits &= mask;
        right_bits &= mask;
      }
      const int64_t left_signed =
          scalar_signed_integer_bits(left_bits, left_facts.bit_width);
      const int64_t right_signed =
          scalar_signed_integer_bits(right_bits, right_facts.bit_width);
      bool comparison = false;
      switch (value->binary_operator) {
        case W_SEED_HIR0_BINARY_EQUAL:
          comparison = left_bits == right_bits;
          break;
        case W_SEED_HIR0_BINARY_NOT_EQUAL:
          comparison = left_bits != right_bits;
          break;
        case W_SEED_HIR0_BINARY_LESS:
          comparison = left_facts.is_signed ? left_signed < right_signed
                                            : left_bits < right_bits;
          break;
        case W_SEED_HIR0_BINARY_LESS_EQUAL:
          comparison = left_facts.is_signed ? left_signed <= right_signed
                                            : left_bits <= right_bits;
          break;
        case W_SEED_HIR0_BINARY_GREATER:
          comparison = left_facts.is_signed ? left_signed > right_signed
                                            : left_bits > right_bits;
          break;
        case W_SEED_HIR0_BINARY_GREATER_EQUAL:
          comparison = left_facts.is_signed ? left_signed >= right_signed
                                            : left_bits >= right_bits;
          break;
        default:
          return false;
      }
      *result = comparison ? 1 : 0;
      return true;
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
  scalar_integer_facts return_facts;
  if (function->parameter_count != parameter_count ||
      function->block_count != 1u ||
      function->first_block >= program->block_count ||
      !scalar_integer_type_facts(program, function->return_type,
                                 &return_facts))
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
