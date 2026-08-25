#include "w_seed_constir.h"
#include "w_seed_constir_session.h"

#include <limits.h>
#include <string.h>

_Static_assert(sizeof(size_t) * CHAR_BIT >= W_SEED_FRONTEND_TARGET_USIZE_BITS,
               "w-seed D1 requires a host that can represent target usize");

typedef struct {
  const w_seed_constir_input *input;
  const w_seed_frontend_output *frontend;
  const w_seed_frontend_result *frontend_result;
  w_seed_constir_counts counts;
  bool emit;
  w_seed_constir_output *output;
  size_t current_function;
  bool failed;
  w_seed_span failure_span;
  uint32_t failure_expression;
  /* Digest-only dependency stack.  It prevents a source cycle from making
   * lowering fail while still incorporating each acyclic target body into its
   * parent's structural digest. */
  uint32_t digest_const_stack[W_SEED_FRONTEND_MAX_NESTING];
  size_t digest_const_stack_count;
} constir_lower_context;

typedef struct {
  uint8_t bytes[W_SEED_CONSTIR_INTEGER_BYTES];
} constir_bits;

typedef struct {
  w_seed_constir_value value;
  bool valid;
} constir_value_result;

static const uint8_t CONSTIR_RECEIPT_SCHEMA[] = "w-seed-constir-6";

static bool add_size(size_t left, size_t right, size_t *out) {
  if (out == NULL || right > SIZE_MAX - left) return false;
  *out = left + right;
  return true;
}

static bool mul_size(size_t left, size_t right, size_t *out) {
  if (out == NULL || (left != 0 && right > SIZE_MAX / left)) return false;
  *out = left * right;
  return true;
}

static bool u32_from_size(size_t value, uint32_t *out) {
  if (out == NULL || value >= (size_t)UINT32_MAX) return false;
  *out = (uint32_t)value;
  return true;
}

static bool count_fits_u32(size_t value) { return value < (size_t)UINT32_MAX; }

static bool span_valid(w_seed_span span) { return span.start_byte <= span.end_byte; }

static bool span_contains(w_seed_span outer, w_seed_span inner) {
  return span_valid(outer) && span_valid(inner) &&
         outer.start_byte <= inner.start_byte && inner.end_byte <= outer.end_byte;
}

static bool range_valid(size_t start, size_t count, size_t total);

static bool text_is(w_seed_frontend_text text, const char *literal) {
  if (literal == NULL) return false;
  const size_t length = strlen(literal);
  return text.length == length && text.data != NULL &&
         memcmp(text.data, literal, length) == 0;
}

static bool frontend_text_equal(w_seed_frontend_text left,
                                w_seed_frontend_text right) {
  return left.length == right.length &&
         (left.length == 0u ||
          (left.data != NULL && right.data != NULL &&
           memcmp(left.data, right.data, left.length) == 0));
}

static const w_seed_frontend_function *frontend_function_at(
    const constir_lower_context *context, uint32_t index) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->functions == NULL ||
      (size_t)index >= context->frontend_result->written.functions) {
    return NULL;
  }
  return &context->frontend->functions[index];
}

static const w_seed_frontend_const_declaration *frontend_const_declaration_at(
    const constir_lower_context *context, uint32_t index) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || index == W_SEED_FRONTEND_NONE ||
      (size_t)index >= context->frontend_result->written.const_declarations)
    return NULL;
  if (context->frontend->const_declarations == NULL) return NULL;
  return &context->frontend->const_declarations[index];
}

static const w_seed_frontend_typed_const_expression *
frontend_typed_const_expression_at(const constir_lower_context *context,
                                   uint32_t index) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL ||
      context->frontend->typed_const_expressions == NULL ||
      index == W_SEED_FRONTEND_NONE ||
      (size_t)index >= context->frontend_result->written.typed_const_expressions)
    return NULL;
  return &context->frontend->typed_const_expressions[index];
}

/* Only a clean pending application may become an executable synthetic
 * function.  Typed records attached to an INVALID/UNSUPPORTED application
 * remain audit data and lower to a non-executable diagnostic record. */
static bool typed_const_expression_application_pending(
    const constir_lower_context *context, uint32_t typed_index,
    const w_seed_frontend_typed_const_expression *typed) {
  if (context == NULL || typed == NULL || context->frontend == NULL ||
      context->frontend_result == NULL ||
      context->frontend->generic_applications == NULL ||
      context->frontend->generic_arguments == NULL ||
      typed->owner_application == W_SEED_FRONTEND_NONE ||
      (size_t)typed->owner_application >=
          context->frontend_result->written.generic_applications)
    return false;
  const w_seed_frontend_generic_application *application =
      &context->frontend->generic_applications[typed->owner_application];
  if (application->binding_status !=
          W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST ||
      !application->requires_const_evaluation ||
      typed->module_index != application->module_index ||
      typed->argument_ordinal >= application->argument_count ||
      !range_valid(application->first_argument, application->argument_count,
                   context->frontend_result->written.generic_arguments))
    return false;
  const w_seed_frontend_generic_argument *argument =
      &context->frontend
           ->generic_arguments[application->first_argument +
                               typed->argument_ordinal];
  return argument->owner_application == typed->owner_application &&
         argument->source_ordinal == typed->argument_ordinal &&
         argument->binding_status ==
             W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST &&
         argument->typed_const_expression_index == typed_index;
}

static const w_seed_frontend_expression *frontend_expression_at(
    const constir_lower_context *context, uint32_t index) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->expressions == NULL ||
      (size_t)index >= context->frontend_result->written.expressions) {
    return NULL;
  }
  return &context->frontend->expressions[index];
}

static const w_seed_frontend_type *frontend_type_at(
    const constir_lower_context *context, uint32_t index) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->types == NULL ||
      index == W_SEED_FRONTEND_NONE ||
      (size_t)index >= context->frontend_result->written.types) {
    return NULL;
  }
  return &context->frontend->types[index];
}

static bool frontend_string_slice_valid(const constir_lower_context *context,
                                        uint32_t offset, uint32_t count) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || offset == W_SEED_FRONTEND_NONE ||
      count > W_SEED_CONSTIR_MAX_STRING_BYTES ||
      offset > context->frontend_result->written.const_bytes ||
      count > context->frontend_result->written.const_bytes - offset)
    return false;
  if (context->frontend->const_bytes_capacity <
      context->frontend_result->written.const_bytes)
    return false;
  return count == 0u || context->frontend->const_bytes != NULL;
}

static bool constir_result_type_supported(w_seed_frontend_type_kind kind) {
  return kind == W_SEED_FRONTEND_TYPE_BOOL ||
         kind == W_SEED_FRONTEND_TYPE_INTEGER ||
         kind == W_SEED_FRONTEND_TYPE_ENUM ||
         kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
}

static const w_seed_frontend_parameter *frontend_parameter_at(
    const constir_lower_context *context, uint32_t index) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->parameters == NULL ||
      (size_t)index >= context->frontend_result->written.parameters) {
    return NULL;
  }
  return &context->frontend->parameters[index];
}

static const w_seed_frontend_statement *frontend_statement_at(
    const constir_lower_context *context, uint32_t index) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->statements == NULL ||
      (size_t)index >= context->frontend_result->written.statements) {
    return NULL;
  }
  return &context->frontend->statements[index];
}

static const w_seed_frontend_argument *frontend_argument_at(
    const constir_lower_context *context, uint32_t index) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->arguments == NULL ||
      (size_t)index >= context->frontend_result->written.arguments) {
    return NULL;
  }
  return &context->frontend->arguments[index];
}

static const w_seed_frontend_switch_arm *frontend_switch_arm_at(
    const constir_lower_context *context, uint32_t index) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->switch_arms == NULL ||
      (size_t)index >= context->frontend_result->written.switch_arms) {
    return NULL;
  }
  return &context->frontend->switch_arms[index];
}

static const w_seed_frontend_enum_membership_case *frontend_membership_at(
    const constir_lower_context *context, uint32_t index) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL ||
      context->frontend->enum_membership_cases == NULL ||
      (size_t)index >= context->frontend_result->written.enum_membership_cases) {
    return NULL;
  }
  return &context->frontend->enum_membership_cases[index];
}

static bool function_is_const(const constir_lower_context *context,
                              uint32_t index) {
  const w_seed_frontend_function *function = frontend_function_at(context, index);
  return function != NULL && function->is_const;
}

static bool function_base_supported(const constir_lower_context *context,
                                    uint32_t index) {
  const w_seed_frontend_function *function = frontend_function_at(context, index);
  if (function == NULL || !function->is_const || !function->const_body_supported ||
      function->first_statement == W_SEED_FRONTEND_NONE) {
    return false;
  }
  const w_seed_frontend_type *return_type =
      frontend_type_at(context, function->return_type);
  if (return_type == NULL || !constir_result_type_supported(return_type->kind))
    return false;
  for (uint32_t offset = 0; offset < function->statement_count; offset += 1u) {
    const w_seed_frontend_statement *statement = frontend_statement_at(
        context, function->first_statement + offset);
    if (statement == NULL ||
        (statement->kind == W_SEED_FRONTEND_STMT_UNSUPPORTED)) return false;
  }
  const w_seed_frontend_statement *statement =
      frontend_statement_at(context, function->first_statement);
  return statement != NULL &&
         (statement->kind == W_SEED_FRONTEND_STMT_RETURN ||
          statement->kind == W_SEED_FRONTEND_STMT_GUARD ||
          statement->kind == W_SEED_FRONTEND_STMT_IF ||
          statement->kind == W_SEED_FRONTEND_STMT_FOR);
}

static bool node_count_increment(constir_lower_context *context) {
  return context != NULL && add_size(context->counts.nodes, 1u,
                                     &context->counts.nodes) &&
         count_fits_u32(context->counts.nodes);
}

static void mark_failure(constir_lower_context *context, w_seed_span span,
                        uint32_t expression) {
  if (context == NULL || context->failed) return;
  context->failed = true;
  context->failure_span = span;
  context->failure_expression = expression;
}

static void bits_zero(constir_bits *value) {
  if (value != NULL) (void)memset(value->bytes, 0, sizeof(value->bytes));
}

static bool bits_is_zero(const constir_bits *value) {
  if (value == NULL) return false;
  for (size_t index = 0; index < sizeof(value->bytes); index += 1) {
    if (value->bytes[index] != 0u) return false;
  }
  return true;
}

static int bits_compare_unsigned(const constir_bits *left,
                                 const constir_bits *right) {
  if (left == NULL || right == NULL) return 0;
  for (size_t index = sizeof(left->bytes); index > 0; index -= 1) {
    if (left->bytes[index - 1u] < right->bytes[index - 1u]) return -1;
    if (left->bytes[index - 1u] > right->bytes[index - 1u]) return 1;
  }
  return 0;
}

static bool bits_add(const constir_bits *left, const constir_bits *right,
                     constir_bits *out, bool *carry_out) {
  if (left == NULL || right == NULL || out == NULL) return false;
  unsigned int carry = 0;
  for (size_t index = 0; index < sizeof(out->bytes); index += 1) {
    const unsigned int sum = (unsigned int)left->bytes[index] +
                             (unsigned int)right->bytes[index] + carry;
    out->bytes[index] = (uint8_t)sum;
    carry = sum > 255u ? 1u : 0u;
  }
  if (carry_out != NULL) *carry_out = carry != 0u;
  return true;
}

static bool bits_subtract(const constir_bits *left, const constir_bits *right,
                          constir_bits *out, bool *borrow_out) {
  if (left == NULL || right == NULL || out == NULL) return false;
  unsigned int borrow = 0;
  for (size_t index = 0; index < sizeof(out->bytes); index += 1) {
    const unsigned int left_value = left->bytes[index];
    const unsigned int right_value = (unsigned int)right->bytes[index] + borrow;
    out->bytes[index] = (uint8_t)(left_value - right_value);
    borrow = left_value < right_value ? 1u : 0u;
  }
  if (borrow_out != NULL) *borrow_out = borrow != 0u;
  return true;
}

static void bits_negate(constir_bits *value) {
  if (value == NULL) return;
  for (size_t index = 0; index < sizeof(value->bytes); index += 1)
    value->bytes[index] = (uint8_t)~value->bytes[index];
  constir_bits one;
  bits_zero(&one);
  one.bytes[0] = 1u;
  (void)bits_add(value, &one, value, NULL);
}

static bool bits_negative(const constir_bits *value, uint16_t width) {
  if (value == NULL || width == 0u || width > 128u) return false;
  const size_t bit = (size_t)width - 1u;
  return (value->bytes[bit / 8u] & (uint8_t)(1u << (bit % 8u))) != 0u;
}

static void bits_mask(constir_bits *value, uint16_t width, bool signed_value) {
  if (value == NULL || width == 0u || width > 128u) return;
  const size_t bytes = ((size_t)width + 7u) / 8u;
  const unsigned int remainder = (unsigned int)width % 8u;
  if (remainder != 0u) {
    value->bytes[bytes - 1u] &= (uint8_t)((1u << remainder) - 1u);
  }
  for (size_t index = bytes; index < sizeof(value->bytes); index += 1)
    value->bytes[index] = 0u;
  if (signed_value && bits_negative(value, width)) {
    if (remainder != 0u) {
      value->bytes[bytes - 1u] |= (uint8_t)(0xffu << remainder);
    }
    for (size_t index = bytes; index < sizeof(value->bytes); index += 1)
      value->bytes[index] = 0xffu;
  }
}

static bool bits_fit(constir_bits value, bool signed_value, uint16_t width) {
  if (width == 0u || width > 128u) return false;
  const size_t bytes = ((size_t)width + 7u) / 8u;
  const unsigned int remainder = (unsigned int)width % 8u;
  const bool negative = signed_value && bits_negative(&value, width);
  if (remainder != 0u) {
    const uint8_t high = value.bytes[bytes - 1u];
    const uint8_t mask = (uint8_t)(0xffu << remainder);
    if (signed_value) {
      if (negative) {
        if ((high & mask) != mask) return false;
      } else if ((high & mask) != 0u) {
        return false;
      }
    } else if ((high & mask) != 0u) {
      return false;
    }
  }
  for (size_t index = bytes; index < sizeof(value.bytes); index += 1) {
    if (signed_value) {
      if ((negative && value.bytes[index] != 0xffu) ||
          (!negative && value.bytes[index] != 0u)) return false;
    } else if (value.bytes[index] != 0u) {
      return false;
    }
  }
  return true;
}

static void bits_limit(uint16_t width, bool negative, constir_bits *out) {
  bits_zero(out);
  if (width == 0u || width > 128u) return;
  const size_t bit = negative ? (size_t)width - 1u : (size_t)width - 1u;
  out->bytes[bit / 8u] = (uint8_t)(1u << (bit % 8u));
  if (!negative) {
    for (size_t index = 0; index < bit / 8u; index += 1) out->bytes[index] = 0xffu;
    const unsigned int rem = (unsigned int)(bit % 8u);
    out->bytes[bit / 8u] = rem == 0u ? 0u : (uint8_t)((1u << rem) - 1u);
  }
}

static void bits_limit_unsigned(uint16_t width, constir_bits *out) {
  bits_zero(out);
  if (out == NULL || width == 0u || width > 128u) return;
  const size_t bytes = ((size_t)width + 7u) / 8u;
  const unsigned int remainder = (unsigned int)width % 8u;
  for (size_t index = 0; index < bytes; index += 1u) out->bytes[index] = 0xffu;
  if (remainder != 0u) out->bytes[bytes - 1u] =
      (uint8_t)((1u << remainder) - 1u);
}

static void bits_magnitude(constir_bits value, bool signed_value,
                           uint16_t width, constir_bits *magnitude,
                           bool *negative) {
  if (magnitude == NULL) return;
  *magnitude = value;
  const bool is_negative = signed_value && bits_negative(&value, width);
  if (negative != NULL) *negative = is_negative;
  if (is_negative) bits_negate(magnitude);
  bits_mask(magnitude, width, false);
}

static bool bits_multiply(const constir_bits *left, const constir_bits *right,
                          constir_bits *out, bool *high_nonzero) {
  if (left == NULL || right == NULL || out == NULL) return false;
  uint8_t product[32];
  (void)memset(product, 0, sizeof(product));
  for (size_t i = 0; i < 16u; i += 1) {
    unsigned int carry = 0;
    for (size_t j = 0; j < 16u; j += 1) {
      const size_t position = i + j;
      const unsigned int value = (unsigned int)product[position] +
                                 (unsigned int)left->bytes[i] *
                                     (unsigned int)right->bytes[j] + carry;
      product[position] = (uint8_t)value;
      carry = value >> 8;
    }
    size_t position = i + 16u;
    while (carry != 0u && position < sizeof(product)) {
      const unsigned int value = (unsigned int)product[position] + carry;
      product[position] = (uint8_t)value;
      carry = value >> 8;
      position += 1;
    }
  }
  (void)memcpy(out->bytes, product, sizeof(out->bytes));
  if (high_nonzero != NULL) {
    *high_nonzero = false;
    for (size_t index = sizeof(out->bytes); index < sizeof(product); index += 1)
      if (product[index] != 0u) *high_nonzero = true;
  }
  return true;
}

static bool bits_divide_unsigned(constir_bits dividend, constir_bits divisor,
                                 constir_bits *quotient, constir_bits *remainder) {
  if (quotient == NULL || remainder == NULL || bits_is_zero(&divisor))
    return false;
  bits_zero(quotient);
  bits_zero(remainder);
  for (size_t step = 128u; step > 0; step -= 1) {
    const size_t bit = step - 1u;
    constir_bits shifted = *remainder;
    for (size_t index = sizeof(shifted.bytes); index > 0; index -= 1) {
      const uint8_t carry = index > 1u ? shifted.bytes[index - 2u] : 0u;
      shifted.bytes[index - 1u] =
          (uint8_t)((shifted.bytes[index - 1u] << 1) |
                    ((carry & 0x80u) != 0u ? 1u : 0u));
    }
    shifted.bytes[0] = (uint8_t)(shifted.bytes[0] |
                                 ((dividend.bytes[bit / 8u] >> (bit % 8u)) & 1u));
    *remainder = shifted;
    if (bits_compare_unsigned(remainder, &divisor) >= 0) {
      (void)bits_subtract(remainder, &divisor, remainder, NULL);
      quotient->bytes[bit / 8u] |= (uint8_t)(1u << (bit % 8u));
    }
  }
  return true;
}

static bool type_metadata(const constir_lower_context *context, uint32_t type_index,
                          w_seed_frontend_type_kind *kind, bool *is_signed,
                          uint16_t *width, uint32_t *enum_base) {
  const w_seed_frontend_type *type = frontend_type_at(context, type_index);
  if (type == NULL || kind == NULL || is_signed == NULL || width == NULL ||
      enum_base == NULL) return false;
  *kind = type->kind;
  *is_signed = type->is_signed;
  *width = type->bit_width;
  *enum_base = type->enum_base_index;
  if (type->kind == W_SEED_FRONTEND_TYPE_INTEGER)
    return type->bit_width != 0u && type->bit_width <= 128u;
  return type->kind == W_SEED_FRONTEND_TYPE_BOOL ||
         type->kind == W_SEED_FRONTEND_TYPE_STRING ||
         type->kind == W_SEED_FRONTEND_TYPE_ENUM ||
         type->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET ||
         type->kind == W_SEED_FRONTEND_TYPE_STATIC_LIST ||
         type->kind == W_SEED_FRONTEND_TYPE_RANGE;
}

static bool integer_value_from_expression(const constir_lower_context *context,
                                          const w_seed_frontend_expression *expression,
                                          constir_bits *out) {
  if (context == NULL || expression == NULL || out == NULL ||
      !expression->has_integer_value) return false;
  (void)memcpy(out->bytes, expression->integer_value, sizeof(out->bytes));
  w_seed_frontend_type_kind kind;
  bool is_signed;
  uint16_t width;
  uint32_t enum_base;
  if (!type_metadata(context, expression->inferred_type, &kind, &is_signed,
                     &width, &enum_base) ||
      kind != W_SEED_FRONTEND_TYPE_INTEGER) return false;
  (void)enum_base;
  return bits_fit(*out, is_signed, width);
}

static w_seed_constir_operator operator_for_text(w_seed_frontend_text text) {
  if (text_is(text, "!")) return W_SEED_CONSTIR_OPERATOR_NOT;
  if (text_is(text, "+")) return W_SEED_CONSTIR_OPERATOR_ADD;
  if (text_is(text, "-")) return W_SEED_CONSTIR_OPERATOR_SUBTRACT;
  if (text_is(text, "*")) return W_SEED_CONSTIR_OPERATOR_MULTIPLY;
  if (text_is(text, "/")) return W_SEED_CONSTIR_OPERATOR_DIVIDE;
  if (text_is(text, "%")) return W_SEED_CONSTIR_OPERATOR_REMAINDER;
  if (text_is(text, "==")) return W_SEED_CONSTIR_OPERATOR_EQUAL;
  if (text_is(text, "!=")) return W_SEED_CONSTIR_OPERATOR_NOT_EQUAL;
  if (text_is(text, "<")) return W_SEED_CONSTIR_OPERATOR_LESS;
  if (text_is(text, "<=")) return W_SEED_CONSTIR_OPERATOR_LESS_EQUAL;
  if (text_is(text, ">")) return W_SEED_CONSTIR_OPERATOR_GREATER;
  if (text_is(text, ">=")) return W_SEED_CONSTIR_OPERATOR_GREATER_EQUAL;
  if (text_is(text, "&&")) return W_SEED_CONSTIR_OPERATOR_AND;
  if (text_is(text, "||")) return W_SEED_CONSTIR_OPERATOR_OR;
  if (text_is(text, "<<")) return W_SEED_CONSTIR_OPERATOR_SHIFT_LEFT;
  if (text_is(text, ">>")) return W_SEED_CONSTIR_OPERATOR_SHIFT_RIGHT;
  if (text_is(text, "&")) return W_SEED_CONSTIR_OPERATOR_BIT_AND;
  if (text_is(text, "|")) return W_SEED_CONSTIR_OPERATOR_BIT_OR;
  if (text_is(text, "^")) return W_SEED_CONSTIR_OPERATOR_BIT_XOR;
  if (text_is(text, "**")) return W_SEED_CONSTIR_OPERATOR_POWER;
  return W_SEED_CONSTIR_OPERATOR_INVALID;
}

static w_seed_constir_operator unary_operator_for_text(
    w_seed_frontend_text text) {
  if (text_is(text, "!")) return W_SEED_CONSTIR_OPERATOR_NOT;
  if (text_is(text, "-")) return W_SEED_CONSTIR_OPERATOR_NEGATE;
  return W_SEED_CONSTIR_OPERATOR_INVALID;
}

static bool operator_is_supported(w_seed_constir_operator operator,
                                   w_seed_frontend_type_kind type_kind) {
  if (operator == W_SEED_CONSTIR_OPERATOR_NOT)
    return type_kind == W_SEED_FRONTEND_TYPE_BOOL;
  if (operator == W_SEED_CONSTIR_OPERATOR_NEGATE)
    return type_kind == W_SEED_FRONTEND_TYPE_INTEGER;
  if (operator == W_SEED_CONSTIR_OPERATOR_AND ||
      operator == W_SEED_CONSTIR_OPERATOR_OR)
    return type_kind == W_SEED_FRONTEND_TYPE_BOOL;
  if (operator == W_SEED_CONSTIR_OPERATOR_EQUAL ||
      operator == W_SEED_CONSTIR_OPERATOR_NOT_EQUAL)
    return type_kind == W_SEED_FRONTEND_TYPE_BOOL ||
           type_kind == W_SEED_FRONTEND_TYPE_INTEGER ||
           type_kind == W_SEED_FRONTEND_TYPE_ENUM;
  return type_kind == W_SEED_FRONTEND_TYPE_INTEGER;
}

static bool operator_is_comparison(w_seed_constir_operator operator) {
  return operator == W_SEED_CONSTIR_OPERATOR_EQUAL ||
         operator == W_SEED_CONSTIR_OPERATOR_NOT_EQUAL ||
         operator == W_SEED_CONSTIR_OPERATOR_LESS ||
         operator == W_SEED_CONSTIR_OPERATOR_LESS_EQUAL ||
         operator == W_SEED_CONSTIR_OPERATOR_GREATER ||
         operator == W_SEED_CONSTIR_OPERATOR_GREATER_EQUAL;
}

static bool operator_is_ordered_comparison(w_seed_constir_operator operator) {
  return operator == W_SEED_CONSTIR_OPERATOR_LESS ||
         operator == W_SEED_CONSTIR_OPERATOR_LESS_EQUAL ||
         operator == W_SEED_CONSTIR_OPERATOR_GREATER ||
         operator == W_SEED_CONSTIR_OPERATOR_GREATER_EQUAL;
}

static bool comparison_operand_types_compatible(
    w_seed_frontend_type_kind left_kind, bool left_signed, uint32_t left_enum,
    w_seed_frontend_type_kind right_kind, bool right_signed,
    uint32_t right_enum) {
  const bool left_enum_kind = left_kind == W_SEED_FRONTEND_TYPE_ENUM ||
                              left_kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  const bool right_enum_kind = right_kind == W_SEED_FRONTEND_TYPE_ENUM ||
                               right_kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  if (left_enum_kind || right_enum_kind) {
    return left_enum_kind && right_enum_kind && left_enum == right_enum;
  }
  if (left_kind != right_kind) return false;
  if (left_kind == W_SEED_FRONTEND_TYPE_INTEGER)
    return left_signed == right_signed;
  return left_kind == W_SEED_FRONTEND_TYPE_BOOL ||
         left_kind == W_SEED_FRONTEND_TYPE_STRING;
}

static bool binary_operator_supported(const constir_lower_context *context,
                                      uint32_t expression_index,
                                      w_seed_constir_operator operator,
                                      w_seed_frontend_type_kind result_kind) {
  if (context == NULL) return false;
  if (!operator_is_comparison(operator))
    return operator_is_supported(operator, result_kind);
  if (result_kind != W_SEED_FRONTEND_TYPE_BOOL) return false;
  const w_seed_frontend_expression *expression =
      frontend_expression_at(context, expression_index);
  if (expression == NULL || expression->left == W_SEED_FRONTEND_NONE ||
      expression->right == W_SEED_FRONTEND_NONE)
    return false;
  const w_seed_frontend_expression *left =
      frontend_expression_at(context, expression->left);
  const w_seed_frontend_expression *right =
      frontend_expression_at(context, expression->right);
  if (left == NULL || right == NULL) return false;
  w_seed_frontend_type_kind left_kind;
  w_seed_frontend_type_kind right_kind;
  bool left_signed;
  bool right_signed;
  uint16_t left_width;
  uint16_t right_width;
  uint32_t left_enum;
  uint32_t right_enum;
  if (!type_metadata(context, left->inferred_type, &left_kind, &left_signed,
                     &left_width, &left_enum) ||
      !type_metadata(context, right->inferred_type, &right_kind, &right_signed,
                     &right_width, &right_enum) ||
      !comparison_operand_types_compatible(
          left_kind, left_signed, left_enum, right_kind, right_signed,
          right_enum))
    return false;
  if (operator_is_ordered_comparison(operator))
    return left_kind == W_SEED_FRONTEND_TYPE_INTEGER;
  return true;
}

static bool append_node(constir_lower_context *context,
                        const w_seed_constir_node *node, uint32_t *index) {
  if (context == NULL || node == NULL || index == NULL || !node_count_increment(context))
    return false;
  if (!context->emit) {
    *index = W_SEED_CONSTIR_NONE;
    return true;
  }
  const size_t offset = context->counts.nodes - 1u;
  if (context->output == NULL || context->output->nodes == NULL ||
      offset >= context->output->node_capacity || !u32_from_size(offset, index))
    return false;
  context->output->nodes[offset] = *node;
  return true;
}

static bool append_call_argument(constir_lower_context *context,
                                 const w_seed_constir_call_argument *argument,
                                 uint32_t *index) {
  if (context == NULL || argument == NULL || index == NULL ||
      !add_size(context->counts.call_arguments, 1u,
                &context->counts.call_arguments) ||
      !count_fits_u32(context->counts.call_arguments)) return false;
  if (!context->emit) {
    *index = W_SEED_CONSTIR_NONE;
    return true;
  }
  const size_t offset = context->counts.call_arguments - 1u;
  if (context->output == NULL || context->output->call_arguments == NULL ||
      offset >= context->output->call_argument_capacity ||
      !u32_from_size(offset, index)) return false;
  context->output->call_arguments[offset] = *argument;
  return true;
}

static bool append_parameter(constir_lower_context *context,
                             const w_seed_constir_parameter *parameter,
                             uint32_t *index) {
  if (context == NULL || parameter == NULL || index == NULL ||
      !add_size(context->counts.parameters, 1u, &context->counts.parameters) ||
      !count_fits_u32(context->counts.parameters)) return false;
  if (!context->emit) {
    *index = W_SEED_CONSTIR_NONE;
    return true;
  }
  const size_t offset = context->counts.parameters - 1u;
  if (context->output == NULL || context->output->parameters == NULL ||
      offset >= context->output->parameter_capacity || !u32_from_size(offset, index))
    return false;
  context->output->parameters[offset] = *parameter;
  return true;
}

static bool append_switch_arm(constir_lower_context *context,
                              const w_seed_constir_switch_arm *arm,
                              uint32_t *index) {
  if (context == NULL || arm == NULL || index == NULL ||
      !add_size(context->counts.switch_arms, 1u, &context->counts.switch_arms) ||
      !count_fits_u32(context->counts.switch_arms)) return false;
  if (!context->emit) {
    *index = W_SEED_CONSTIR_NONE;
    return true;
  }
  const size_t offset = context->counts.switch_arms - 1u;
  if (context->output == NULL || context->output->switch_arms == NULL ||
      offset >= context->output->switch_arm_capacity ||
      !u32_from_size(offset, index)) return false;
  context->output->switch_arms[offset] = *arm;
  return true;
}

static bool append_membership_case(constir_lower_context *context,
                                   const w_seed_constir_membership_case *item,
                                   uint32_t *index) {
  if (context == NULL || item == NULL || index == NULL ||
      !add_size(context->counts.membership_cases, 1u,
                &context->counts.membership_cases) ||
      !count_fits_u32(context->counts.membership_cases)) return false;
  if (!context->emit) {
    *index = W_SEED_CONSTIR_NONE;
    return true;
  }
  const size_t offset = context->counts.membership_cases - 1u;
  if (context->output == NULL || context->output->membership_cases == NULL ||
      offset >= context->output->membership_case_capacity ||
      !u32_from_size(offset, index)) return false;
  context->output->membership_cases[offset] = *item;
  return true;
}

static bool append_statement(constir_lower_context *context,
                             const w_seed_constir_statement *statement,
                             uint32_t *index) {
  if (context == NULL || statement == NULL || index == NULL ||
      !add_size(context->counts.statements, 1u, &context->counts.statements) ||
      !count_fits_u32(context->counts.statements)) return false;
  if (!context->emit) {
    *index = W_SEED_CONSTIR_NONE;
    return true;
  }
  const size_t offset = context->counts.statements - 1u;
  if (context->output == NULL || context->output->statements == NULL ||
      offset >= context->output->statement_capacity ||
      !u32_from_size(offset, index)) return false;
  context->output->statements[offset] = *statement;
  return true;
}

static bool append_local(constir_lower_context *context,
                         const w_seed_constir_local *local,
                         uint32_t *index) {
  if (context == NULL || local == NULL || index == NULL ||
      !add_size(context->counts.locals, 1u, &context->counts.locals) ||
      !count_fits_u32(context->counts.locals)) return false;
  if (!context->emit) {
    *index = W_SEED_CONSTIR_NONE;
    return true;
  }
  const size_t offset = context->counts.locals - 1u;
  if (context->output == NULL || context->output->locals == NULL ||
      offset >= context->output->local_capacity || !u32_from_size(offset, index))
    return false;
  context->output->locals[offset] = *local;
  return true;
}

static bool lower_expression(constir_lower_context *context,
                             uint32_t function_index, uint32_t expression_index,
                             uint32_t *ir_index, size_t depth);

static bool lower_statement_tree(constir_lower_context *context,
                                 uint32_t function_index,
                                 uint32_t frontend_statement_index,
                                 uint32_t *ir_index, size_t depth);

static bool lower_statement_chain(constir_lower_context *context,
                                  uint32_t function_index,
                                  uint32_t frontend_statement_index,
                                  uint32_t *first_ir, uint32_t *count,
                                  size_t depth) {
  if (first_ir == NULL || count == NULL ||
      depth > W_SEED_FRONTEND_MAX_NESTING) return false;
  *first_ir = W_SEED_CONSTIR_NONE;
  *count = 0u;
  uint32_t previous_ir = W_SEED_CONSTIR_NONE;
  uint32_t current = frontend_statement_index;
  size_t guard = 0u;
  while (current != W_SEED_FRONTEND_NONE &&
         guard <= context->frontend_result->written.statements) {
    uint32_t child_ir = W_SEED_CONSTIR_NONE;
    if (!lower_statement_tree(context, function_index, current, &child_ir,
                              depth + 1u)) return false;
    if (*first_ir == W_SEED_CONSTIR_NONE) *first_ir = child_ir;
    if (context->emit && context->output != NULL &&
        previous_ir != W_SEED_CONSTIR_NONE && child_ir != W_SEED_CONSTIR_NONE &&
        previous_ir < context->output->statement_capacity)
      context->output->statements[previous_ir].next_sibling = child_ir;
    previous_ir = child_ir;
    *count += 1u;
    const w_seed_frontend_statement *source =
        frontend_statement_at(context, current);
    if (source == NULL) return false;
    current = source->next_sibling;
    guard += 1u;
  }
  return current == W_SEED_FRONTEND_NONE;
}

static bool lower_call_arguments(constir_lower_context *context,
                                uint32_t function_index,
                                const w_seed_frontend_expression *expression,
                                uint32_t *first, uint32_t *argument_count,
                                size_t depth) {
  if (first == NULL || argument_count == NULL || expression == NULL ||
      expression->argument_count > W_SEED_CONSTIR_MAX_PARAMETERS) return false;
  *first = W_SEED_CONSTIR_NONE;
  *argument_count = expression->argument_count;
  if (expression->argument_count == 0u) return true;
  for (uint32_t offset = 0; offset < expression->argument_count; offset += 1) {
    const uint32_t frontend_argument_index = expression->first_argument + offset;
    const w_seed_frontend_argument *argument =
        frontend_argument_at(context, frontend_argument_index);
    if (argument == NULL || argument->owner_expression != expression->left)
      return false;
    const uint32_t parameter_ordinal = argument->resolved_parameter_ordinal;
    if (parameter_ordinal == W_SEED_FRONTEND_NONE) return false;
    uint32_t child = W_SEED_CONSTIR_NONE;
    if (!lower_expression(context, function_index, argument->expression_index,
                          &child, depth + 1u)) return false;
    w_seed_constir_call_argument item;
    item.owner_node = W_SEED_CONSTIR_NONE;
    item.parameter_ordinal = parameter_ordinal;
    item.node_index = child;
    item.source_span = argument->span;
    uint32_t item_index = W_SEED_CONSTIR_NONE;
    if (!append_call_argument(context, &item, &item_index)) return false;
    if (offset == 0u) *first = item_index;
  }
  return true;
}

static bool lower_statement_tree(constir_lower_context *context,
                                 uint32_t function_index,
                                 uint32_t frontend_statement_index,
                                 uint32_t *ir_index, size_t depth) {
  if (context == NULL || ir_index == NULL ||
      depth > W_SEED_FRONTEND_MAX_NESTING) return false;
  const w_seed_frontend_statement *source =
      frontend_statement_at(context, frontend_statement_index);
  if (source == NULL || source->owner_function != function_index ||
      source->kind == W_SEED_FRONTEND_STMT_UNSUPPORTED) return false;
  w_seed_constir_statement record;
  (void)memset(&record, 0, sizeof(record));
  record.kind = W_SEED_CONSTIR_STATEMENT_INVALID;
  record.owner_function = function_index;
  record.source_span = source->span;
  record.expression_node = W_SEED_CONSTIR_NONE;
  record.condition_node = W_SEED_CONSTIR_NONE;
  record.first_child = W_SEED_CONSTIR_NONE;
  record.child_count = 0u;
  record.else_child = W_SEED_CONSTIR_NONE;
  record.next_sibling = W_SEED_CONSTIR_NONE;
  record.lower_node = W_SEED_CONSTIR_NONE;
  record.upper_node = W_SEED_CONSTIR_NONE;
  record.local_ordinal = W_SEED_CONSTIR_NONE;
  record.local_type_index = W_SEED_CONSTIR_NONE;
  record.local_type_kind = W_SEED_FRONTEND_TYPE_UNKNOWN;
  record.local_type_is_signed = false;
  record.local_type_bit_width = 0u;
  record.half_open = 0u;
  switch (source->kind) {
    case W_SEED_FRONTEND_STMT_RETURN:
      if (source->expression_index == W_SEED_FRONTEND_NONE ||
          !lower_expression(context, function_index, source->expression_index,
                            &record.expression_node, depth + 1u)) return false;
      record.kind = W_SEED_CONSTIR_STATEMENT_RETURN;
      break;
    case W_SEED_FRONTEND_STMT_GUARD:
      if (source->condition_expression == W_SEED_FRONTEND_NONE ||
          !lower_expression(context, function_index, source->condition_expression,
                            &record.condition_node, depth + 1u) ||
          source->else_child == W_SEED_FRONTEND_NONE)
        return false;
      record.kind = W_SEED_CONSTIR_STATEMENT_GUARD;
      break;
    case W_SEED_FRONTEND_STMT_IF:
      if (source->condition_expression == W_SEED_FRONTEND_NONE ||
          !lower_expression(context, function_index, source->condition_expression,
                            &record.condition_node, depth + 1u)) return false;
      record.kind = W_SEED_CONSTIR_STATEMENT_IF;
      break;
    case W_SEED_FRONTEND_STMT_FOR: {
      if (source->range_lower_expression == W_SEED_FRONTEND_NONE ||
          source->range_upper_expression == W_SEED_FRONTEND_NONE ||
          source->first_child == W_SEED_FRONTEND_NONE) return false;
      if (!lower_expression(context, function_index, source->range_lower_expression,
                            &record.lower_node, depth + 1u) ||
          !lower_expression(context, function_index, source->range_upper_expression,
                            &record.upper_node, depth + 1u)) return false;
      const w_seed_frontend_expression *lower = frontend_expression_at(
          context, source->range_lower_expression);
      const w_seed_frontend_expression *upper = frontend_expression_at(
          context, source->range_upper_expression);
      if (lower == NULL || upper == NULL || lower->inferred_type == W_SEED_FRONTEND_NONE ||
          upper->inferred_type == W_SEED_FRONTEND_NONE) return false;
      const w_seed_frontend_type *lower_type = frontend_type_at(context, lower->inferred_type);
      const w_seed_frontend_type *upper_type = frontend_type_at(context, upper->inferred_type);
      if (lower_type == NULL || upper_type == NULL ||
          lower_type->kind != W_SEED_FRONTEND_TYPE_INTEGER ||
          upper_type->kind != W_SEED_FRONTEND_TYPE_INTEGER ||
          lower_type->is_signed || upper_type->is_signed ||
          lower_type->bit_width != upper_type->bit_width) return false;
      record.kind = W_SEED_CONSTIR_STATEMENT_FOR_RANGE;
      record.local_ordinal = source->loop_local_ordinal;
      record.local_type_index = lower->inferred_type;
      record.local_type_kind = lower_type->kind;
      record.local_type_is_signed = lower_type->is_signed;
      record.local_type_bit_width = lower_type->bit_width;
      record.half_open = 1u;
      break;
    }
    default:
      return false;
  }
  if (!append_statement(context, &record, ir_index)) return false;
  if (source->kind == W_SEED_FRONTEND_STMT_FOR) {
    w_seed_constir_local local;
    (void)memset(&local, 0, sizeof(local));
    local.owner_function = function_index;
    local.ordinal = source->loop_local_ordinal;
    local.type_index = record.local_type_index;
    local.type_kind = record.local_type_kind;
    local.type_is_signed = record.local_type_is_signed;
    local.type_bit_width = record.local_type_bit_width;
    local.element_type_index = W_SEED_CONSTIR_NONE;
    local.source_span = source->span;
    uint32_t ignored_local = W_SEED_CONSTIR_NONE;
    if (!append_local(context, &local, &ignored_local)) return false;
  }
  w_seed_constir_statement *written = NULL;
  if (context->emit && context->output != NULL &&
      *ir_index < context->output->statement_capacity)
    written = &context->output->statements[*ir_index];
  if (source->first_child != W_SEED_FRONTEND_NONE) {
    uint32_t child_first = W_SEED_CONSTIR_NONE;
    uint32_t child_count = 0u;
    if (!lower_statement_chain(context, function_index, source->first_child,
                               &child_first, &child_count, depth + 1u))
      return false;
    if (written != NULL) {
      written->first_child = child_first;
      written->child_count = child_count;
    }
  }
  if (source->else_child != W_SEED_FRONTEND_NONE) {
    uint32_t else_ir = W_SEED_CONSTIR_NONE;
    uint32_t else_count = 0u;
    if (!lower_statement_chain(context, function_index, source->else_child,
                               &else_ir, &else_count, depth + 1u)) return false;
    if (written != NULL) written->else_child = else_ir;
  }
  return true;
}

static void digest_u8(w_seed_sha256_state *state, uint8_t value) {
  w_seed_sha256_update(state, &value, 1u);
}

static void digest_u16(w_seed_sha256_state *state, uint16_t value) {
  uint8_t bytes[2];
  bytes[0] = (uint8_t)(value >> 8);
  bytes[1] = (uint8_t)value;
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void digest_u32(w_seed_sha256_state *state, uint32_t value) {
  uint8_t bytes[4];
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void digest_text(w_seed_sha256_state *state,
                        w_seed_frontend_text text) {
  uint32_t length = text.length > (size_t)UINT32_MAX ? UINT32_MAX
                                                      : (uint32_t)text.length;
  digest_u32(state, length);
  if (length != 0u && text.data != NULL)
    w_seed_sha256_update(state, (const uint8_t *)text.data, length);
}

static bool enum_identity(const constir_lower_context *context,
                          uint32_t enum_index, w_seed_sha256_state *state) {
  if (enum_index == W_SEED_FRONTEND_NONE || context == NULL || state == NULL ||
      context->frontend == NULL || context->frontend_result == NULL ||
      context->frontend->enums == NULL ||
      (size_t)enum_index >= context->frontend_result->written.enums) return false;
  const w_seed_frontend_enum *enumeration = &context->frontend->enums[enum_index];
  digest_text(state, enumeration->name);
  return true;
}

static bool enum_case_identity(const constir_lower_context *context,
                               uint32_t enum_index, uint32_t case_index,
                               w_seed_sha256_state *state) {
  if (!enum_identity(context, enum_index, state) || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->enum_cases == NULL ||
      (size_t)case_index >= context->frontend_result->written.enum_cases)
    return false;
  const w_seed_frontend_enum_case *item = &context->frontend->enum_cases[case_index];
  digest_text(state, item->name);
  return true;
}

static bool digest_type(const constir_lower_context *context, uint32_t type_index,
                        w_seed_sha256_state *state) {
  w_seed_frontend_type_kind kind;
  bool is_signed;
  uint16_t width;
  uint32_t enum_base;
  if (!type_metadata(context, type_index, &kind, &is_signed, &width,
                     &enum_base)) return false;
  digest_u8(state, (uint8_t)kind);
  digest_u8(state, is_signed ? 1u : 0u);
  digest_u16(state, width);
  if (kind == W_SEED_FRONTEND_TYPE_ENUM ||
      kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET) {
    if (!enum_identity(context, enum_base, state)) return false;
    const w_seed_frontend_type *type = frontend_type_at(context, type_index);
    if (type == NULL) return false;
    if (kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET) {
      digest_u32(state, type->subset_member_count);
      for (uint32_t offset = 0; offset < type->subset_member_count; offset += 1) {
        if (context->frontend->enum_subset_members == NULL) return false;
        const w_seed_frontend_enum_subset_member *member =
            &context->frontend->enum_subset_members[type->first_subset_member + offset];
        if (!enum_case_identity(context, member->enum_base_index,
                                member->enum_case_index, state)) return false;
      }
    }
  }
  return true;
}

static bool digest_expression(const constir_lower_context *context,
                              uint32_t function_index, uint32_t expression_index,
                              w_seed_sha256_state *state, size_t depth);

static bool digest_const_stack_contains(const constir_lower_context *context,
                                        uint32_t target_const) {
  if (context == NULL) return false;
  for (size_t index = 0u; index < context->digest_const_stack_count; index += 1u)
    if (context->digest_const_stack[index] == target_const) return true;
  return false;
}

static bool digest_call_target(const constir_lower_context *context,
                               uint32_t target_function,
                               w_seed_sha256_state *state) {
  const w_seed_frontend_function *target =
      frontend_function_at(context, target_function);
  if (context == NULL || state == NULL || target == NULL ||
      context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->modules == NULL ||
      (size_t)target->module_index >= context->frontend_result->written.modules)
    return false;
  digest_text(state, context->frontend->modules[target->module_index].module_id);
  digest_text(state, target->name);
  return true;
}

static bool digest_const_target(const constir_lower_context *context,
                                uint32_t target_const,
                                w_seed_sha256_state *state, size_t depth) {
  const w_seed_frontend_const_declaration *target =
      frontend_const_declaration_at(context, target_const);
  if (context == NULL || state == NULL || target == NULL ||
      context->frontend == NULL || context->frontend_result == NULL ||
      context->frontend->modules == NULL ||
      (size_t)target->module_index >= context->frontend_result->written.modules)
    return false;
  digest_u8(state, 0x6du);
  digest_text(state, context->frontend->modules[target->module_index].module_id);
  digest_text(state, target->name);
  /* ConstIR-6 used a constant target tag in this position.  It is semantic
   * type framing, not annotation presence: retain it for D4-D6 body-digest
   * compatibility and use it for D7 inferred declarations as well. */
  digest_u8(state, 1u);
  if (target->effective_type == W_SEED_FRONTEND_NONE ||
      !digest_type(context, target->effective_type, state)) {
    return false;
  }
  if (target->initializer_expression == W_SEED_FRONTEND_NONE) return false;
  if (digest_const_stack_contains(context, target_const)) {
    /* The identity was already framed above.  This marker closes a cycle and
     * keeps the declaration lowerable for generic graph preflight. */
    digest_u8(state, 0xc1u);
    return true;
  }
  if (depth > W_SEED_FRONTEND_MAX_NESTING ||
      context->digest_const_stack_count >= W_SEED_FRONTEND_MAX_NESTING) {
    /* A bounded identity marker preserves a deterministic digest at the
     * source ceiling.  The generic validator owns the separate 256-edge
     * dependency ceiling. */
    digest_u8(state, 0xc2u);
    return true;
  }
  constir_lower_context *mutable_context = (constir_lower_context *)context;
  mutable_context->digest_const_stack[
      mutable_context->digest_const_stack_count++] = target_const;
  const bool digested = digest_expression(
      context, W_SEED_FRONTEND_NONE, target->initializer_expression, state, depth);
  mutable_context->digest_const_stack_count -= 1u;
  if (!digested) return false;
  return true;
}

static bool digest_expression(const constir_lower_context *context,
                              uint32_t function_index, uint32_t expression_index,
                              w_seed_sha256_state *state, size_t depth) {
  if (context == NULL || state == NULL || depth > W_SEED_FRONTEND_MAX_NESTING)
    return false;
  const w_seed_frontend_expression *expression =
      frontend_expression_at(context, expression_index);
  if (expression == NULL || expression->owner_function != function_index ||
      !expression->supported || !span_valid(expression->span)) return false;
  /* Parentheses are source provenance only.  They do not become public IR
   * nodes and are absent from the semantic digest. */
  if (expression->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS) {
    if (expression->left == W_SEED_FRONTEND_NONE) return false;
    return digest_expression(context, function_index, expression->left, state,
                             depth + 1u);
  }
  w_seed_frontend_type_kind kind;
  bool is_signed;
  uint16_t width;
  uint32_t enum_base;
  if (!type_metadata(context, expression->inferred_type, &kind, &is_signed,
                     &width, &enum_base) ||
      !digest_type(context, expression->inferred_type, state)) return false;
  digest_u8(state, (uint8_t)expression->kind);
  switch (expression->kind) {
    case W_SEED_FRONTEND_EXPR_BOOL: {
      if (!expression->has_bool_value) return false;
      digest_u8(state, expression->bool_value ? 1u : 0u);
      break;
    }
    case W_SEED_FRONTEND_EXPR_INTEGER: {
      constir_bits value;
      if (!integer_value_from_expression(context, expression, &value)) return false;
      w_seed_sha256_update(state, value.bytes, sizeof(value.bytes));
      break;
    }
    case W_SEED_FRONTEND_EXPR_STRING:
      if (!frontend_string_slice_valid(context, expression->const_byte_offset,
                                       expression->const_byte_count))
        return false;
      /* Stable String framing excludes source spelling, spans, and trivia. */
      digest_u8(state, 0x53u);
      digest_u32(state, expression->const_byte_count);
      if (expression->const_byte_count != 0u)
        w_seed_sha256_update(
            state, context->frontend->const_bytes +
                       expression->const_byte_offset,
            expression->const_byte_count);
      break;
    case W_SEED_FRONTEND_EXPR_ENUM_CASE:
      if (!enum_case_identity(context, expression->enum_index,
                              expression->enum_case_index, state)) return false;
      break;
    case W_SEED_FRONTEND_EXPR_IDENTIFIER: {
      if (expression->resolved_local_ordinal != W_SEED_FRONTEND_NONE) {
        digest_u8(state, 0x6cu);
        digest_u32(state, expression->resolved_local_ordinal);
        break;
      }
      if (expression->resolved_const_declaration != W_SEED_FRONTEND_NONE) {
        const uint32_t target = expression->resolved_const_declaration;
        return digest_const_target(context, target, state, depth);
      }
      const uint32_t ordinal = expression->resolved_parameter_ordinal;
      if (ordinal == W_SEED_FRONTEND_NONE) return false;
      digest_u32(state, ordinal);
      break;
    }
    case W_SEED_FRONTEND_EXPR_MEMBER:
      if (expression->left == W_SEED_FRONTEND_NONE ||
          !text_is(expression->member_name, "count") ||
          !digest_expression(context, function_index, expression->left, state,
                             depth + 1u))
        return false;
      digest_u8(state, 0x63u); /* StaticList.count */
      break;
    case W_SEED_FRONTEND_EXPR_INDEX: {
      if (expression->left == W_SEED_FRONTEND_NONE ||
          expression->right == W_SEED_FRONTEND_NONE) return false;
      const w_seed_frontend_expression *receiver = frontend_expression_at(
          context, expression->left);
      if (receiver == NULL || receiver->inferred_type == W_SEED_FRONTEND_NONE)
        return false;
      const w_seed_frontend_type *receiver_type =
          frontend_type_at(context, receiver->inferred_type);
      if (receiver_type == NULL ||
          receiver_type->kind != W_SEED_FRONTEND_TYPE_STATIC_LIST ||
          receiver_type->element_type == W_SEED_FRONTEND_NONE)
        return false;
      digest_u8(state, 0x69u); /* StaticList[index] */
      if (!digest_type(context, receiver->inferred_type, state) ||
          !digest_expression(context, function_index, expression->left, state,
                             depth + 1u) ||
          !digest_expression(context, function_index, expression->right, state,
                             depth + 1u))
        return false;
      break;
    }
    case W_SEED_FRONTEND_EXPR_RANGE:
      if (expression->left == W_SEED_FRONTEND_NONE ||
          expression->right == W_SEED_FRONTEND_NONE ||
          !digest_expression(context, function_index, expression->left, state,
                             depth + 1u) ||
          !digest_expression(context, function_index, expression->right, state,
                             depth + 1u))
        return false;
      digest_u8(state, 0x72u); /* half-open range */
      break;
    case W_SEED_FRONTEND_EXPR_UNARY: {
      const w_seed_constir_operator operator =
          unary_operator_for_text(expression->operator_text);
      if (operator == W_SEED_CONSTIR_OPERATOR_INVALID ||
          !operator_is_supported(operator, kind)) return false;
      digest_u8(state, (uint8_t)operator);
      return digest_expression(context, function_index, expression->left, state,
                               depth + 1u);
    }
    case W_SEED_FRONTEND_EXPR_BINARY: {
      const w_seed_constir_operator operator =
          operator_for_text(expression->operator_text);
      if (operator == W_SEED_CONSTIR_OPERATOR_INVALID ||
          !binary_operator_supported(context, expression_index, operator, kind) ||
          !digest_expression(context, function_index, expression->left, state,
                             depth + 1u) ||
          !digest_expression(context, function_index, expression->right, state,
                             depth + 1u)) return false;
      /* The operator follows operands in this framing to keep node order
       * explicit while preserving short-circuit branch structure. */
      digest_u8(state, (uint8_t)operator);
      return true;
    }
    case W_SEED_FRONTEND_EXPR_CALL: {
      const w_seed_frontend_expression *callee =
          frontend_expression_at(context, expression->left);
      if (callee == NULL || callee->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER)
        return false;
      const uint32_t target = callee->resolved_function_index;
      if (target == W_SEED_FRONTEND_NONE || !function_is_const(context, target) ||
          !function_base_supported(context, target) ||
          !digest_call_target(context, target, state) ||
          expression->argument_count > W_SEED_CONSTIR_MAX_PARAMETERS)
        return false;
      digest_u32(state, expression->argument_count);
      for (uint32_t offset = 0; offset < expression->argument_count; offset += 1) {
        const w_seed_frontend_argument *argument = frontend_argument_at(
            context, expression->first_argument + offset);
        const uint32_t ordinal = argument == NULL
                                     ? W_SEED_CONSTIR_NONE
                                     : argument->resolved_parameter_ordinal;
        if (argument == NULL || argument->owner_expression != expression->left ||
            ordinal == W_SEED_FRONTEND_NONE ||
            !digest_expression(context, function_index, argument->expression_index,
                               state, depth + 1u)) return false;
        digest_u32(state, ordinal);
      }
      return true;
    }
    case W_SEED_FRONTEND_EXPR_ENUM_MEMBERSHIP:
      if (expression->left == W_SEED_FRONTEND_NONE ||
          expression->membership_case_count == 0u ||
          !digest_expression(context, function_index, expression->left, state,
                             depth + 1u)) return false;
      digest_u32(state, expression->membership_case_count);
      for (uint32_t offset = 0; offset < expression->membership_case_count; offset += 1) {
        const w_seed_frontend_enum_membership_case *item = frontend_membership_at(
            context, expression->first_membership_case + offset);
        if (item == NULL || item->owner_expression != expression_index ||
            !enum_case_identity(context, item->enum_base_index,
                                item->enum_case_index, state)) return false;
      }
      return true;
    case W_SEED_FRONTEND_EXPR_SWITCH:
      if (expression->left == W_SEED_FRONTEND_NONE ||
          expression->switch_arm_count == 0u ||
          !digest_expression(context, function_index, expression->left, state,
                             depth + 1u)) return false;
      digest_u32(state, expression->switch_arm_count);
      for (uint32_t offset = 0; offset < expression->switch_arm_count; offset += 1) {
        const w_seed_frontend_switch_arm *arm = frontend_switch_arm_at(
            context, expression->first_switch_arm + offset);
        if (arm == NULL || !arm->supported ||
            !digest_expression(context, function_index, arm->result_expression,
                               state, depth + 1u)) return false;
        digest_u8(state, (uint8_t)arm->pattern_kind);
        if (arm->enum_case_index != W_SEED_FRONTEND_NONE &&
            !enum_case_identity(context, arm->enum_index, arm->enum_case_index,
                                state)) return false;
      }
      return true;
    default:
      return false;
  }
  return true;
}

static bool digest_statement_chain(const constir_lower_context *context,
                                   uint32_t function_index,
                                   uint32_t statement_index,
                                   w_seed_sha256_state *state, size_t depth);

static bool digest_statement_tree(const constir_lower_context *context,
                                  uint32_t function_index,
                                  uint32_t statement_index,
                                  w_seed_sha256_state *state, size_t depth) {
  if (context == NULL || state == NULL || depth > W_SEED_FRONTEND_MAX_NESTING)
    return false;
  const w_seed_frontend_statement *statement =
      frontend_statement_at(context, statement_index);
  if (statement == NULL || statement->owner_function != function_index ||
      statement->kind == W_SEED_FRONTEND_STMT_UNSUPPORTED) {
    return false;
  }
  /* Statement kind is semantic.  Child-chain framing below records control
   * shape without exposing frontend ordinals, spans, names, or pointers. */
  digest_u8(state, 0x70u);
  digest_u8(state, (uint8_t)statement->kind);
  switch (statement->kind) {
    case W_SEED_FRONTEND_STMT_RETURN:
      if (statement->expression_index == W_SEED_FRONTEND_NONE ||
          !digest_expression(context, function_index,
                             statement->expression_index, state, depth + 1u))
        return false;
      break;
    case W_SEED_FRONTEND_STMT_GUARD:
      if (statement->condition_expression == W_SEED_FRONTEND_NONE ||
          statement->else_child == W_SEED_FRONTEND_NONE ||
          !digest_expression(context, function_index,
                             statement->condition_expression, state,
                             depth + 1u))
        return false;
      digest_u8(state, 0x71u);
      if (!digest_statement_chain(context, function_index, statement->else_child,
                                  state, depth + 1u))
        return false;
      break;
    case W_SEED_FRONTEND_STMT_IF:
      if (statement->condition_expression == W_SEED_FRONTEND_NONE ||
          !digest_expression(context, function_index,
                             statement->condition_expression, state,
                             depth + 1u))
        return false;
      digest_u8(state, (uint8_t)(statement->first_child ==
                                         W_SEED_FRONTEND_NONE
                                     ? 0u
                                     : 1u));
      if (statement->first_child != W_SEED_FRONTEND_NONE &&
          !digest_statement_chain(context, function_index,
                                  statement->first_child, state, depth + 1u))
        return false;
      digest_u8(state, (uint8_t)(statement->else_child ==
                                         W_SEED_FRONTEND_NONE
                                     ? 0u
                                     : 1u));
      if (statement->else_child != W_SEED_FRONTEND_NONE &&
          !digest_statement_chain(context, function_index,
                                  statement->else_child, state, depth + 1u))
        return false;
      break;
    case W_SEED_FRONTEND_STMT_FOR:
      if (statement->range_lower_expression == W_SEED_FRONTEND_NONE ||
          statement->range_upper_expression == W_SEED_FRONTEND_NONE ||
          statement->first_child == W_SEED_FRONTEND_NONE ||
          statement->loop_local_ordinal == W_SEED_FRONTEND_NONE ||
          !digest_expression(context, function_index,
                             statement->range_lower_expression, state,
                             depth + 1u) ||
          !digest_expression(context, function_index,
                             statement->range_upper_expression, state,
                             depth + 1u))
        return false;
      digest_u32(state, statement->loop_local_ordinal);
      digest_u8(state, 1u); /* `..<` is the only accepted range boundary. */
      if (!digest_statement_chain(context, function_index, statement->first_child,
                                  state, depth + 1u))
        return false;
      break;
    default:
      return false;
  }
  return true;
}

static bool digest_statement_chain(const constir_lower_context *context,
                                   uint32_t function_index,
                                   uint32_t statement_index,
                                   w_seed_sha256_state *state, size_t depth) {
  if (context == NULL || state == NULL || depth > W_SEED_FRONTEND_MAX_NESTING ||
      context->frontend_result == NULL)
    return false;
  digest_u8(state, 0x72u);
  uint32_t current = statement_index;
  size_t guard = 0u;
  while (current != W_SEED_FRONTEND_NONE &&
         guard <= context->frontend_result->written.statements) {
    const w_seed_frontend_statement *statement =
        frontend_statement_at(context, current);
    if (statement == NULL || statement->owner_function != function_index ||
        !digest_statement_tree(context, function_index, current, state,
                               depth + 1u))
      return false;
    current = statement->next_sibling;
    guard += 1u;
  }
  if (current != W_SEED_FRONTEND_NONE) return false;
  digest_u8(state, 0x73u);
  return true;
}

static size_t receipt_node_bytes(void) { return 111u; }
static size_t receipt_function_bytes(void) { return 94u; }
static size_t receipt_parameter_bytes(void) { return 40u; }
static size_t receipt_call_argument_bytes(void) { return 28u; }
static size_t receipt_switch_arm_bytes(void) { return 49u; }
static size_t receipt_membership_bytes(void) { return 28u; }
/* statement = kind(1) + owner(4) + span(16) + nine u32 fields(36) +
 * half-open marker(1); local = three u32 fields(12) + kind/signed/width(4)
 * + element type(4) + span(16). */
static size_t receipt_statement_bytes(void) { return 58u; }
static size_t receipt_local_bytes(void) { return 36u; }
static size_t receipt_diagnostic_bytes(void) { return 25u; }

static bool receipt_size_for_counts(const w_seed_constir_counts *counts,
                                   size_t *bytes) {
  if (counts == NULL || bytes == NULL) return false;
  size_t value = sizeof(CONSTIR_RECEIPT_SCHEMA) - 1u;
  size_t part = 0;
  if (!mul_size(counts->functions, receipt_function_bytes(), &part) ||
      !add_size(value, part, &value) ||
      !mul_size(counts->parameters, receipt_parameter_bytes(), &part) ||
      !add_size(value, part, &value) ||
      !mul_size(counts->nodes, receipt_node_bytes(), &part) ||
      !add_size(value, part, &value) ||
      !mul_size(counts->call_arguments, receipt_call_argument_bytes(), &part) ||
      !add_size(value, part, &value) ||
      !mul_size(counts->switch_arms, receipt_switch_arm_bytes(), &part) ||
      !add_size(value, part, &value) ||
      !mul_size(counts->membership_cases, receipt_membership_bytes(), &part) ||
      !add_size(value, part, &value) ||
      !mul_size(counts->statements, receipt_statement_bytes(), &part) ||
      !add_size(value, part, &value) ||
      !mul_size(counts->locals, receipt_local_bytes(), &part) ||
      !add_size(value, part, &value) ||
      !mul_size(counts->diagnostics, receipt_diagnostic_bytes(), &part) ||
      !add_size(value, part, &value)) return false;
  *bytes = value;
  return true;
}

static bool append_diagnostic(constir_lower_context *context,
                             uint32_t owner_function, uint32_t expression,
                             w_seed_span span, uint32_t *index) {
  if (context == NULL || index == NULL ||
      !add_size(context->counts.diagnostics, 1u, &context->counts.diagnostics) ||
      !count_fits_u32(context->counts.diagnostics)) return false;
  if (!context->emit) {
    *index = W_SEED_CONSTIR_NONE;
    return true;
  }
  const size_t offset = context->counts.diagnostics - 1u;
  if (context->output == NULL || context->output->diagnostics == NULL ||
      offset >= context->output->diagnostic_capacity || !u32_from_size(offset, index))
    return false;
  context->output->diagnostics[offset] = (w_seed_constir_diagnostic){
      W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0001, owner_function, expression, span};
  return true;
}

/* D3 synthetic functions are intentionally narrower than ordinary const
 * functions.  The frontend has already typed the tree; this pass enforces
 * that the tree is closed and contains only scalar literals, grouping, unary
 * and binary operators before it becomes ConstIR. */
static bool typed_const_expression_closed(constir_lower_context *context,
                                          uint32_t expression_index,
                                          size_t depth) {
  if (context == NULL || depth == 0u ||
      depth > W_SEED_FRONTEND_MAX_NESTING)
    return false;
  const w_seed_frontend_expression *expression =
      frontend_expression_at(context, expression_index);
  if (expression == NULL || expression->owner_function != W_SEED_FRONTEND_NONE ||
      !expression->supported || !span_valid(expression->span))
    return false;
  switch (expression->kind) {
    case W_SEED_FRONTEND_EXPR_BOOL:
      return expression->has_bool_value &&
             expression->inferred_type != W_SEED_FRONTEND_NONE;
    case W_SEED_FRONTEND_EXPR_INTEGER: {
      w_seed_frontend_type_kind kind;
      bool is_signed = false;
      uint16_t width = 0u;
      uint32_t enum_base = W_SEED_FRONTEND_NONE;
      return expression->has_integer_value &&
             type_metadata(context, expression->inferred_type, &kind,
                           &is_signed, &width, &enum_base) &&
             kind == W_SEED_FRONTEND_TYPE_INTEGER && width != 0u;
    }
    case W_SEED_FRONTEND_EXPR_IDENTIFIER: {
      if (expression->resolved_local_ordinal != W_SEED_FRONTEND_NONE ||
          expression->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE ||
          expression->resolved_const_declaration == W_SEED_FRONTEND_NONE)
        return false;
      const uint32_t target = expression->resolved_const_declaration;
      const w_seed_frontend_const_declaration *declaration =
          frontend_const_declaration_at(context, target);
      w_seed_frontend_type_kind kind = W_SEED_FRONTEND_TYPE_UNKNOWN;
      bool is_signed = false;
      uint16_t width = 0u;
      uint32_t enum_base = W_SEED_FRONTEND_NONE;
      w_seed_frontend_type_kind target_kind = W_SEED_FRONTEND_TYPE_UNKNOWN;
      bool target_signed = false;
      uint16_t target_width = 0u;
      uint32_t target_enum_base = W_SEED_FRONTEND_NONE;
      if (declaration == NULL || !declaration->lowerable ||
          !type_metadata(context, expression->inferred_type, &kind, &is_signed,
                         &width, &enum_base) ||
          !type_metadata(context, declaration->effective_type, &target_kind,
                         &target_signed, &target_width, &target_enum_base) ||
          (kind != W_SEED_FRONTEND_TYPE_BOOL &&
           kind != W_SEED_FRONTEND_TYPE_INTEGER) ||
          (kind == W_SEED_FRONTEND_TYPE_INTEGER && width == 0u) ||
          target_kind != kind || target_signed != is_signed ||
          target_width != width || target_enum_base != enum_base)
        return false;
      return true;
    }
    case W_SEED_FRONTEND_EXPR_PARENTHESIS:
      return expression->left != W_SEED_FRONTEND_NONE &&
             typed_const_expression_closed(context, expression->left,
                                           depth + 1u);
    case W_SEED_FRONTEND_EXPR_UNARY: {
      const w_seed_constir_operator op =
          unary_operator_for_text(expression->operator_text);
      w_seed_frontend_type_kind kind;
      bool is_signed = false;
      uint16_t width = 0u;
      uint32_t enum_base = W_SEED_FRONTEND_NONE;
      if (expression->left == W_SEED_FRONTEND_NONE ||
          op == W_SEED_CONSTIR_OPERATOR_INVALID ||
          !type_metadata(context, expression->inferred_type, &kind, &is_signed,
                         &width, &enum_base) ||
          (kind != W_SEED_FRONTEND_TYPE_BOOL &&
           kind != W_SEED_FRONTEND_TYPE_INTEGER) ||
          (kind == W_SEED_FRONTEND_TYPE_INTEGER && width == 0u) ||
          !operator_is_supported(op, kind))
        return false;
      return typed_const_expression_closed(context, expression->left,
                                           depth + 1u);
    }
    case W_SEED_FRONTEND_EXPR_BINARY: {
      const w_seed_constir_operator op =
          operator_for_text(expression->operator_text);
      w_seed_frontend_type_kind kind;
      bool is_signed = false;
      uint16_t width = 0u;
      uint32_t enum_base = W_SEED_FRONTEND_NONE;
      if (expression->left == W_SEED_FRONTEND_NONE ||
          expression->right == W_SEED_FRONTEND_NONE ||
          op == W_SEED_CONSTIR_OPERATOR_INVALID ||
          !type_metadata(context, expression->inferred_type, &kind, &is_signed,
                         &width, &enum_base) ||
          (kind != W_SEED_FRONTEND_TYPE_BOOL &&
           kind != W_SEED_FRONTEND_TYPE_INTEGER) ||
          (kind == W_SEED_FRONTEND_TYPE_INTEGER && width == 0u) ||
          !binary_operator_supported(context, expression_index, op, kind))
        return false;
      return typed_const_expression_closed(context, expression->left,
                                           depth + 1u) &&
             typed_const_expression_closed(context, expression->right,
                                           depth + 1u);
    }
    default:
      return false;
  }
}

static bool lower_all(constir_lower_context *context) {
  if (context == NULL || context->frontend_result == NULL ||
      context->frontend == NULL) return false;
  for (size_t frontend_index = 0;
       frontend_index < context->frontend_result->written.functions;
       frontend_index += 1) {
    const uint32_t function_index = (uint32_t)frontend_index;
    const w_seed_frontend_function *function =
        frontend_function_at(context, function_index);
    if (function == NULL || !function->is_const) continue;
    const size_t function_output_index = context->counts.functions;
    if (!add_size(context->counts.functions, 1u, &context->counts.functions) ||
        !count_fits_u32(context->counts.functions)) return false;
    const size_t parameter_start = context->counts.parameters;
    const size_t node_start = context->counts.nodes;
    const size_t call_start = context->counts.call_arguments;
    const size_t switch_start = context->counts.switch_arms;
    const size_t membership_start = context->counts.membership_cases;
    const size_t statement_start = context->counts.statements;
    const size_t local_start = context->counts.locals;
    bool lowerable = function_base_supported(context, function_index);
    uint32_t root = W_SEED_CONSTIR_NONE;
    uint32_t root_statement = W_SEED_CONSTIR_NONE;
    w_seed_span failure_span = function->body_span;
    uint32_t failure_expression = W_SEED_CONSTIR_NONE;
    uint8_t digest[32];
    (void)memset(digest, 0, sizeof(digest));
    if (lowerable) {
      w_seed_sha256_state digest_state;
      w_seed_sha256_init(&digest_state);
      w_seed_sha256_update(&digest_state, CONSTIR_RECEIPT_SCHEMA,
                           sizeof(CONSTIR_RECEIPT_SCHEMA) - 1u);
      digest_u8(&digest_state, 0xd0u);
      const w_seed_frontend_statement *statement =
          frontend_statement_at(context, function->first_statement);
      const bool simple_return =
          statement != NULL && function->statement_count == 1u &&
          statement->kind == W_SEED_FRONTEND_STMT_RETURN &&
          statement->expression_index != W_SEED_FRONTEND_NONE;
      (void)simple_return;
      /* Digest the normalized statement tree for both simple returns and
       * multi-statement bodies.  The tree walker hashes semantic expression
       * content and explicit child-chain framing, never frontend indices. */
      const bool digest_ok =
          statement != NULL &&
          digest_statement_chain(context, function_index,
                                 function->first_statement, &digest_state, 0u);
      if (!digest_ok) {
        lowerable = false;
        if (context->failed) {
          failure_span = context->failure_span;
          failure_expression = context->failure_expression;
        }
        context->failed = false;
      } else {
        w_seed_sha256_final(&digest_state, digest);
      }
    }
    context->current_function = function_index;
    context->failed = false;
    context->failure_expression = W_SEED_CONSTIR_NONE;
    if (lowerable) {
      const w_seed_frontend_statement *statement =
          frontend_statement_at(context, function->first_statement);
      const bool simple_return =
          statement != NULL && function->statement_count == 1u &&
          statement->kind == W_SEED_FRONTEND_STMT_RETURN &&
          statement->expression_index != W_SEED_FRONTEND_NONE;
      bool lowered = false;
      if (simple_return) {
        lowered = lower_expression(context, function_index,
                                   statement->expression_index, &root, 0u);
      } else if (statement != NULL) {
        uint32_t chain_count = 0u;
        lowered = lower_statement_chain(context, function_index,
                                        function->first_statement,
                                        &root_statement, &chain_count, 0u);
      }
      if (!lowered) {
        lowerable = false;
        if (context->failed) {
          failure_span = context->failure_span;
          failure_expression = context->failure_expression;
        }
      }
    }
    if (!lowerable) {
      context->counts.parameters = parameter_start;
      context->counts.nodes = node_start;
      context->counts.call_arguments = call_start;
      context->counts.switch_arms = switch_start;
      context->counts.membership_cases = membership_start;
      context->counts.statements = statement_start;
      context->counts.locals = local_start;
      if (failure_span.start_byte > failure_span.end_byte)
        failure_span = function->span;
      uint32_t diagnostic_index = W_SEED_CONSTIR_NONE;
      if (!append_diagnostic(context, function_index, failure_expression,
                             failure_span, &diagnostic_index)) return false;
      if (context->emit && context->output != NULL &&
          context->output->functions != NULL) {
        w_seed_constir_function record;
        (void)memset(&record, 0, sizeof(record));
        record.origin = W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION;
        record.frontend_function = function_index;
        record.typed_const_expression_index = W_SEED_CONSTIR_NONE;
        record.frontend_const_declaration = W_SEED_CONSTIR_NONE;
        record.lowerable = false;
        record.source_span = function->span;
        record.body_span = function->body_span;
        record.first_parameter = W_SEED_CONSTIR_NONE;
        record.parameter_count = 0u;
        record.first_node = W_SEED_CONSTIR_NONE;
        record.node_count = 0;
        record.root_node = W_SEED_CONSTIR_NONE;
        record.first_statement = W_SEED_CONSTIR_NONE;
        record.statement_count = 0u;
        record.root_statement = W_SEED_CONSTIR_NONE;
        record.first_local = W_SEED_CONSTIR_NONE;
        record.local_count = 0u;
        record.diagnostic_index = diagnostic_index;
        (void)memcpy(record.body_digest, digest, sizeof(record.body_digest));
        if (function_output_index >= context->output->function_capacity) return false;
        context->output->functions[function_output_index] = record;
      }
      continue;
    }
    for (uint32_t parameter_offset = 0;
         parameter_offset < function->parameter_count; parameter_offset += 1u) {
      const uint32_t frontend_parameter_index =
          function->first_parameter + parameter_offset;
      const w_seed_frontend_parameter *frontend_parameter =
          frontend_parameter_at(context, frontend_parameter_index);
      w_seed_frontend_type_kind parameter_kind;
      bool parameter_signed;
      uint16_t parameter_width;
      uint32_t parameter_enum;
      if (frontend_parameter == NULL ||
          !type_metadata(context, frontend_parameter->type_index,
                         &parameter_kind, &parameter_signed, &parameter_width,
                         &parameter_enum)) return false;
      const w_seed_constir_parameter parameter = {
          function_index, parameter_offset, frontend_parameter_index,
          frontend_parameter->type_index, parameter_kind, parameter_signed,
          parameter_width, parameter_enum, frontend_parameter->span};
      uint32_t ignored_parameter = W_SEED_CONSTIR_NONE;
      if (!append_parameter(context, &parameter, &ignored_parameter)) return false;
    }
    if (context->emit && context->output != NULL &&
        context->output->functions != NULL) {
      w_seed_constir_function record;
      (void)memset(&record, 0, sizeof(record));
      record.origin = W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION;
      record.frontend_function = function_index;
      record.typed_const_expression_index = W_SEED_CONSTIR_NONE;
      record.frontend_const_declaration = W_SEED_CONSTIR_NONE;
      record.lowerable = true;
      record.source_span = function->span;
      record.body_span = function->body_span;
      record.first_parameter = (uint32_t)parameter_start;
      record.parameter_count = function->parameter_count;
      record.first_node = (uint32_t)node_start;
      record.node_count = (uint32_t)(context->counts.nodes - node_start);
      record.root_node = root;
      record.first_statement = root_statement;
      record.statement_count = (uint32_t)(context->counts.statements - statement_start);
      record.root_statement = root_statement;
      record.first_local = (uint32_t)local_start;
      record.local_count = (uint32_t)(context->counts.locals - local_start);
      record.diagnostic_index = W_SEED_CONSTIR_NONE;
      (void)memcpy(record.body_digest, digest, sizeof(record.body_digest));
      if (function_output_index >= context->output->function_capacity) return false;
      context->output->functions[function_output_index] = record;
    }
  }
  /* Lower module const declarations after frontend functions and before
   * calculated generic expressions.  Each declaration is a synthetic
   * zero-argument function.  Identifier dependencies stay as explicit CALL
   * nodes, so forward references and cycles remain visible to preflight. */
  for (size_t const_index = 0u;
       const_index < context->frontend_result->written.const_declarations;
       const_index += 1u) {
    if (const_index >= (size_t)UINT32_MAX) return false;
    const uint32_t frontend_const_index = (uint32_t)const_index;
    const w_seed_frontend_const_declaration *declaration =
        frontend_const_declaration_at(context, frontend_const_index);
    if (declaration == NULL) return false;
    const size_t function_output_index = context->counts.functions;
    if (!add_size(context->counts.functions, 1u, &context->counts.functions) ||
        !count_fits_u32(context->counts.functions)) return false;
    const size_t node_start = context->counts.nodes;
    bool lowerable = declaration->lowerable &&
                     declaration->initializer_expression != W_SEED_FRONTEND_NONE;
    w_seed_frontend_type_kind result_kind = W_SEED_FRONTEND_TYPE_UNKNOWN;
    bool result_signed = false;
    uint16_t result_width = 0u;
    uint32_t result_enum = W_SEED_FRONTEND_NONE;
    if (!type_metadata(context, declaration->effective_type, &result_kind,
                       &result_signed, &result_width, &result_enum) ||
        (result_kind != W_SEED_FRONTEND_TYPE_BOOL &&
         result_kind != W_SEED_FRONTEND_TYPE_INTEGER) ||
        (result_kind == W_SEED_FRONTEND_TYPE_INTEGER && result_width == 0u))
      lowerable = false;
    uint32_t root = W_SEED_CONSTIR_NONE;
    uint8_t digest[32];
    (void)memset(digest, 0, sizeof(digest));
    if (lowerable) {
      w_seed_sha256_state digest_state;
      w_seed_sha256_init(&digest_state);
      w_seed_sha256_update(&digest_state, CONSTIR_RECEIPT_SCHEMA,
                           sizeof(CONSTIR_RECEIPT_SCHEMA) - 1u);
      digest_u8(&digest_state, 0xd2u);
      if (!digest_expression(context, W_SEED_FRONTEND_NONE,
                             declaration->initializer_expression,
                             &digest_state, 0u)) {
        lowerable = false;
      } else {
        w_seed_sha256_final(&digest_state, digest);
      }
    }
    if (lowerable &&
        !lower_expression(context, W_SEED_FRONTEND_NONE,
                          declaration->initializer_expression, &root, 0u))
      lowerable = false;
    if (!lowerable) {
      context->counts.nodes = node_start;
      uint32_t diagnostic_index = W_SEED_CONSTIR_NONE;
      if (!append_diagnostic(context, W_SEED_FRONTEND_NONE,
                             declaration->initializer_expression,
                             declaration->body_span, &diagnostic_index))
        return false;
      if (context->emit && context->output != NULL &&
          context->output->functions != NULL) {
        w_seed_constir_function record;
        (void)memset(&record, 0, sizeof(record));
        record.origin =
            W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION;
        record.frontend_function = W_SEED_CONSTIR_NONE;
        record.typed_const_expression_index = W_SEED_CONSTIR_NONE;
        record.frontend_const_declaration = frontend_const_index;
        record.lowerable = false;
        record.source_span = declaration->span;
        record.body_span = declaration->body_span;
        record.first_parameter = W_SEED_CONSTIR_NONE;
        record.parameter_count = 0u;
        record.first_node = W_SEED_CONSTIR_NONE;
        record.node_count = 0u;
        record.root_node = W_SEED_CONSTIR_NONE;
        record.first_statement = W_SEED_CONSTIR_NONE;
        record.statement_count = 0u;
        record.root_statement = W_SEED_CONSTIR_NONE;
        record.first_local = W_SEED_CONSTIR_NONE;
        record.local_count = 0u;
        record.diagnostic_index = diagnostic_index;
        record.frontend_const_declaration = frontend_const_index;
        (void)memcpy(record.body_digest, digest, sizeof(record.body_digest));
        if (function_output_index >= context->output->function_capacity)
          return false;
        context->output->functions[function_output_index] = record;
      }
      continue;
    }
    if (context->emit && context->output != NULL &&
        context->output->functions != NULL) {
      w_seed_constir_function record;
      (void)memset(&record, 0, sizeof(record));
      record.origin = W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION;
      record.frontend_function = W_SEED_CONSTIR_NONE;
      record.typed_const_expression_index = W_SEED_CONSTIR_NONE;
      record.frontend_const_declaration = frontend_const_index;
      record.lowerable = true;
      record.source_span = declaration->span;
      record.body_span = declaration->body_span;
      record.first_parameter = 0u;
      record.parameter_count = 0u;
      record.first_node = (uint32_t)node_start;
      record.node_count = (uint32_t)(context->counts.nodes - node_start);
      record.root_node = root;
      record.first_statement = W_SEED_CONSTIR_NONE;
      record.statement_count = 0u;
      record.root_statement = W_SEED_CONSTIR_NONE;
      record.first_local = W_SEED_CONSTIR_NONE;
      record.local_count = 0u;
      record.diagnostic_index = W_SEED_CONSTIR_NONE;
      record.frontend_const_declaration = frontend_const_index;
      (void)memcpy(record.body_digest, digest, sizeof(record.body_digest));
      if (function_output_index >= context->output->function_capacity)
        return false;
      context->output->functions[function_output_index] = record;
    }
  }
  /* Lower each frontend-owned calculated generic argument as a synthetic
   * zero-parameter function.  These records follow real frontend functions in
   * deterministic typed-expression order. */
  for (size_t typed_index = 0u;
       typed_index < context->frontend_result->written.typed_const_expressions;
       typed_index += 1u) {
    if (typed_index >= (size_t)UINT32_MAX) return false;
    const uint32_t typed_expression_index = (uint32_t)typed_index;
    const w_seed_frontend_typed_const_expression *typed =
        frontend_typed_const_expression_at(context, typed_expression_index);
    if (typed == NULL || typed->expression_index == W_SEED_FRONTEND_NONE ||
        typed->expected_type == W_SEED_FRONTEND_NONE ||
        typed->effective_type == W_SEED_FRONTEND_NONE ||
        !span_valid(typed->span))
      return false;
    const size_t function_output_index = context->counts.functions;
    if (!add_size(context->counts.functions, 1u, &context->counts.functions) ||
        !count_fits_u32(context->counts.functions)) return false;
    const size_t node_start = context->counts.nodes;
    bool lowerable =
        typed_const_expression_application_pending(context,
                                                    typed_expression_index,
                                                    typed) &&
        typed_const_expression_closed(context, typed->expression_index, 1u);
    w_seed_frontend_type_kind result_kind = W_SEED_FRONTEND_TYPE_UNKNOWN;
    bool result_signed = false;
    uint16_t result_width = 0u;
    uint32_t result_enum = W_SEED_FRONTEND_NONE;
    w_seed_frontend_type_kind effective_kind = W_SEED_FRONTEND_TYPE_UNKNOWN;
    bool effective_signed = false;
    uint16_t effective_width = 0u;
    uint32_t effective_enum = W_SEED_FRONTEND_NONE;
    const w_seed_frontend_expression *root_expression =
        frontend_expression_at(context, typed->expression_index);
    if (root_expression == NULL ||
        !type_metadata(context, root_expression->inferred_type, &result_kind,
                       &result_signed, &result_width, &result_enum) ||
        !type_metadata(context, typed->effective_type, &effective_kind,
                       &effective_signed, &effective_width, &effective_enum) ||
        result_kind != effective_kind || result_signed != effective_signed ||
        result_width != effective_width || result_enum != effective_enum ||
        (effective_kind != W_SEED_FRONTEND_TYPE_BOOL &&
         effective_kind != W_SEED_FRONTEND_TYPE_INTEGER) ||
        (effective_kind == W_SEED_FRONTEND_TYPE_INTEGER &&
         effective_width == 0u))
      lowerable = false;
    uint32_t root = W_SEED_CONSTIR_NONE;
    uint8_t digest[32];
    (void)memset(digest, 0, sizeof(digest));
    if (lowerable) {
      w_seed_sha256_state digest_state;
      w_seed_sha256_init(&digest_state);
      w_seed_sha256_update(&digest_state, CONSTIR_RECEIPT_SCHEMA,
                           sizeof(CONSTIR_RECEIPT_SCHEMA) - 1u);
      digest_u8(&digest_state, 0xd1u);
      if (!digest_expression(context, W_SEED_FRONTEND_NONE,
                             typed->expression_index, &digest_state, 0u)) {
        lowerable = false;
      } else {
        w_seed_sha256_final(&digest_state, digest);
      }
    }
    if (lowerable &&
        !lower_expression(context, W_SEED_FRONTEND_NONE,
                          typed->expression_index, &root, 0u))
      lowerable = false;
    if (!lowerable) {
      context->counts.nodes = node_start;
      uint32_t diagnostic_index = W_SEED_CONSTIR_NONE;
      if (!append_diagnostic(context, W_SEED_FRONTEND_NONE,
                             typed->expression_index, typed->span,
                             &diagnostic_index))
        return false;
      if (context->emit && context->output != NULL &&
          context->output->functions != NULL) {
        w_seed_constir_function record;
        (void)memset(&record, 0, sizeof(record));
        record.origin = W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION;
        record.frontend_function = W_SEED_CONSTIR_NONE;
        record.typed_const_expression_index = typed_expression_index;
        record.frontend_const_declaration = W_SEED_CONSTIR_NONE;
        record.lowerable = false;
        record.source_span = typed->span;
        record.body_span = typed->span;
        record.first_parameter = W_SEED_CONSTIR_NONE;
        record.first_node = W_SEED_CONSTIR_NONE;
        record.root_node = W_SEED_CONSTIR_NONE;
        record.first_statement = W_SEED_CONSTIR_NONE;
        record.root_statement = W_SEED_CONSTIR_NONE;
        record.first_local = W_SEED_CONSTIR_NONE;
        record.diagnostic_index = diagnostic_index;
        (void)memcpy(record.body_digest, digest, sizeof(record.body_digest));
        if (function_output_index >= context->output->function_capacity)
          return false;
        context->output->functions[function_output_index] = record;
      }
      continue;
    }
    if (context->emit && context->output != NULL &&
        context->output->functions != NULL) {
      w_seed_constir_function record;
      (void)memset(&record, 0, sizeof(record));
      record.origin = W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION;
      record.frontend_function = W_SEED_CONSTIR_NONE;
      record.typed_const_expression_index = typed_expression_index;
      record.frontend_const_declaration = W_SEED_CONSTIR_NONE;
      record.lowerable = true;
      record.source_span = typed->span;
      record.body_span = typed->span;
      record.first_parameter = 0u;
      record.parameter_count = 0u;
      record.first_node = (uint32_t)node_start;
      record.node_count = (uint32_t)(context->counts.nodes - node_start);
      record.root_node = root;
      record.first_statement = W_SEED_CONSTIR_NONE;
      record.statement_count = 0u;
      record.root_statement = W_SEED_CONSTIR_NONE;
      record.first_local = W_SEED_CONSTIR_NONE;
      record.local_count = 0u;
      record.diagnostic_index = W_SEED_CONSTIR_NONE;
      (void)memcpy(record.body_digest, digest, sizeof(record.body_digest));
      if (function_output_index >= context->output->function_capacity)
        return false;
      context->output->functions[function_output_index] = record;
    }
  }
  return true;
}

static void write_u32_be(uint8_t *destination, size_t *offset, uint32_t value) {
  destination[*offset] = (uint8_t)(value >> 24);
  *offset += 1u;
  destination[*offset] = (uint8_t)(value >> 16);
  *offset += 1u;
  destination[*offset] = (uint8_t)(value >> 8);
  *offset += 1u;
  destination[*offset] = (uint8_t)value;
  *offset += 1u;
}

static void write_u64_be(uint8_t *destination, size_t *offset, uint64_t value) {
  for (size_t shift = 8u; shift > 0; shift -= 1) {
    destination[*offset] = (uint8_t)(value >> ((shift - 1u) * 8u));
    *offset += 1u;
  }
}

static void write_span(uint8_t *destination, size_t *offset, w_seed_span span) {
  write_u64_be(destination, offset, (uint64_t)span.start_byte);
  write_u64_be(destination, offset, (uint64_t)span.end_byte);
}

static bool write_receipt(const w_seed_constir_output *output,
                          const w_seed_constir_counts *counts) {
  if (output == NULL || counts == NULL || output->receipt == NULL) return false;
  size_t expected = 0;
  if (!receipt_size_for_counts(counts, &expected) ||
      expected > output->receipt_capacity) return false;
  size_t offset = 0;
  (void)memcpy(output->receipt + offset, CONSTIR_RECEIPT_SCHEMA,
               sizeof(CONSTIR_RECEIPT_SCHEMA) - 1u);
  offset += sizeof(CONSTIR_RECEIPT_SCHEMA) - 1u;
  for (size_t index = 0; index < counts->functions; index += 1) {
    const w_seed_constir_function *function = &output->functions[index];
    output->receipt[offset] = (uint8_t)function->origin;
    offset += 1u;
    write_u32_be(output->receipt, &offset, function->frontend_function);
    write_u32_be(output->receipt, &offset,
                 function->typed_const_expression_index);
    write_u32_be(output->receipt, &offset,
                 function->frontend_const_declaration);
    output->receipt[offset] = function->lowerable ? 1u : 0u;
    offset += 1u;
    write_span(output->receipt, &offset, function->source_span);
    write_span(output->receipt, &offset, function->body_span);
    write_u32_be(output->receipt, &offset, function->first_node);
    write_u32_be(output->receipt, &offset, function->node_count);
    write_u32_be(output->receipt, &offset, function->root_node);
    write_u32_be(output->receipt, &offset, function->diagnostic_index);
    (void)memcpy(output->receipt + offset, function->body_digest, 32u);
    offset += 32u;
  }
  for (size_t index = 0; index < counts->parameters; index += 1) {
    const w_seed_constir_parameter *parameter = &output->parameters[index];
    write_u32_be(output->receipt, &offset, parameter->owner_function);
    write_u32_be(output->receipt, &offset, parameter->ordinal);
    write_u32_be(output->receipt, &offset, parameter->frontend_parameter);
    write_u32_be(output->receipt, &offset, parameter->type_index);
    output->receipt[offset] = (uint8_t)parameter->type_kind;
    offset += 1u;
    output->receipt[offset] = parameter->type_is_signed ? 1u : 0u;
    offset += 1u;
    output->receipt[offset] = (uint8_t)(parameter->type_bit_width >> 8);
    offset += 1u;
    output->receipt[offset] = (uint8_t)parameter->type_bit_width;
    offset += 1u;
    write_u32_be(output->receipt, &offset, parameter->enum_base_index);
    write_span(output->receipt, &offset, parameter->source_span);
  }
  for (size_t index = 0; index < counts->nodes; index += 1) {
    const w_seed_constir_node *node = &output->nodes[index];
    output->receipt[offset] = (uint8_t)node->kind;
    offset += 1u;
    write_u32_be(output->receipt, &offset, node->owner_function);
    write_u32_be(output->receipt, &offset, node->frontend_expression);
    write_u32_be(output->receipt, &offset, node->type_index);
    output->receipt[offset] = (uint8_t)node->type_kind;
    offset += 1u;
    output->receipt[offset] = node->type_is_signed ? 1u : 0u;
    offset += 1u;
    output->receipt[offset] = (uint8_t)(node->type_bit_width >> 8);
    offset += 1u;
    output->receipt[offset] = (uint8_t)node->type_bit_width;
    offset += 1u;
    write_u32_be(output->receipt, &offset, node->enum_base_index);
    write_u32_be(output->receipt, &offset, node->enum_case_index);
    write_span(output->receipt, &offset, node->source_span);
    write_u32_be(output->receipt, &offset, node->left);
    write_u32_be(output->receipt, &offset, node->right);
    write_u32_be(output->receipt, &offset, node->parameter_ordinal);
    write_u32_be(output->receipt, &offset, node->call_target_function);
    write_u32_be(output->receipt, &offset,
                 node->call_target_const_declaration);
    write_u32_be(output->receipt, &offset, node->first_call_argument);
    write_u32_be(output->receipt, &offset, node->call_argument_count);
    write_u32_be(output->receipt, &offset, node->first_switch_arm);
    write_u32_be(output->receipt, &offset, node->switch_arm_count);
    write_u32_be(output->receipt, &offset, node->first_membership_case);
    write_u32_be(output->receipt, &offset, node->membership_case_count);
    output->receipt[offset] = (uint8_t)node->normalized_operator;
    offset += 1u;
    output->receipt[offset] = node->bool_value ? 1u : 0u;
    offset += 1u;
    (void)memcpy(output->receipt + offset, node->integer_value,
                 sizeof(node->integer_value));
    offset += sizeof(node->integer_value);
    write_u32_be(output->receipt, &offset, node->const_byte_offset);
    write_u32_be(output->receipt, &offset, node->const_byte_count);
  }
  for (size_t index = 0; index < counts->call_arguments; index += 1) {
    const w_seed_constir_call_argument *argument = &output->call_arguments[index];
    write_u32_be(output->receipt, &offset, argument->owner_node);
    write_u32_be(output->receipt, &offset, argument->parameter_ordinal);
    write_u32_be(output->receipt, &offset, argument->node_index);
    write_span(output->receipt, &offset, argument->source_span);
  }
  for (size_t index = 0; index < counts->switch_arms; index += 1) {
    const w_seed_constir_switch_arm *arm = &output->switch_arms[index];
    write_u32_be(output->receipt, &offset, arm->owner_node);
    output->receipt[offset] = (uint8_t)arm->pattern_kind;
    offset += 1u;
    write_u32_be(output->receipt, &offset, arm->enum_base_index);
    write_u32_be(output->receipt, &offset, arm->enum_case_index);
    write_u32_be(output->receipt, &offset, arm->result_node);
    write_span(output->receipt, &offset, arm->pattern_span);
    write_span(output->receipt, &offset, arm->source_span);
  }
  for (size_t index = 0; index < counts->membership_cases; index += 1) {
    const w_seed_constir_membership_case *item = &output->membership_cases[index];
    write_u32_be(output->receipt, &offset, item->owner_node);
    write_u32_be(output->receipt, &offset, item->enum_base_index);
    write_u32_be(output->receipt, &offset, item->enum_case_index);
    write_span(output->receipt, &offset, item->source_span);
  }
  for (size_t index = 0; index < counts->statements; index += 1) {
    const w_seed_constir_statement *statement = &output->statements[index];
    output->receipt[offset] = (uint8_t)statement->kind;
    offset += 1u;
    write_u32_be(output->receipt, &offset, statement->owner_function);
    write_span(output->receipt, &offset, statement->source_span);
    write_u32_be(output->receipt, &offset, statement->expression_node);
    write_u32_be(output->receipt, &offset, statement->condition_node);
    write_u32_be(output->receipt, &offset, statement->first_child);
    write_u32_be(output->receipt, &offset, statement->child_count);
    write_u32_be(output->receipt, &offset, statement->else_child);
    write_u32_be(output->receipt, &offset, statement->next_sibling);
    write_u32_be(output->receipt, &offset, statement->lower_node);
    write_u32_be(output->receipt, &offset, statement->upper_node);
    write_u32_be(output->receipt, &offset, statement->local_ordinal);
    output->receipt[offset] = statement->half_open;
    offset += 1u;
  }
  for (size_t index = 0; index < counts->locals; index += 1u) {
    const w_seed_constir_local *local = &output->locals[index];
    write_u32_be(output->receipt, &offset, local->owner_function);
    write_u32_be(output->receipt, &offset, local->ordinal);
    write_u32_be(output->receipt, &offset, local->type_index);
    output->receipt[offset] = (uint8_t)local->type_kind;
    offset += 1u;
    output->receipt[offset] = local->type_is_signed ? 1u : 0u;
    offset += 1u;
    output->receipt[offset] = (uint8_t)(local->type_bit_width >> 8);
    offset += 1u;
    output->receipt[offset] = (uint8_t)local->type_bit_width;
    offset += 1u;
    write_u32_be(output->receipt, &offset, local->element_type_index);
    write_span(output->receipt, &offset, local->source_span);
  }
  for (size_t index = 0; index < counts->diagnostics; index += 1) {
    const w_seed_constir_diagnostic *diagnostic = &output->diagnostics[index];
    output->receipt[offset] = (uint8_t)diagnostic->code;
    offset += 1u;
    write_u32_be(output->receipt, &offset, diagnostic->owner_function);
    write_u32_be(output->receipt, &offset, diagnostic->frontend_expression);
    write_span(output->receipt, &offset, diagnostic->source_span);
  }
  return offset == expected;
}

static bool validate_constir_input(const w_seed_constir_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL ||
      input->frontend_input->documents == NULL ||
      input->frontend_input->document_count == 0 ||
      input->frontend_result->written.functions > (size_t)UINT32_MAX ||
      input->frontend_result->written.expressions > (size_t)UINT32_MAX)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_counts *counts = &input->frontend_result->written;
  if ((counts->functions != 0 && output->functions == NULL) ||
      (counts->expressions != 0 && output->expressions == NULL) ||
      (counts->types != 0 && output->types == NULL) ||
      (counts->parameters != 0 && output->parameters == NULL) ||
      (counts->statements != 0 && output->statements == NULL) ||
      (counts->modules != 0 && output->modules == NULL) ||
      (counts->arguments != 0 && output->arguments == NULL) ||
      (counts->typed_const_expressions != 0 &&
       output->typed_const_expressions == NULL) ||
      (counts->switch_arms != 0 && output->switch_arms == NULL) ||
      (counts->enum_membership_cases != 0 && output->enum_membership_cases == NULL) ||
      (counts->enums != 0 && output->enums == NULL) ||
      (counts->enum_cases != 0 && output->enum_cases == NULL)) return false;
  return true;
}

static w_seed_constir_status lower_measure_or_run(
    const w_seed_constir_input *input, w_seed_constir_output *output,
    bool emit, w_seed_constir_counts *counts, w_seed_constir_result *result) {
  if (result != NULL) (void)memset(result, 0, sizeof(*result));
  if (counts != NULL) (void)memset(counts, 0, sizeof(*counts));
  if (!validate_constir_input(input) || counts == NULL || result == NULL) {
    if (result != NULL) result->status = W_SEED_CONSTIR_INVALID;
    return W_SEED_CONSTIR_INVALID;
  }
  constir_lower_context context;
  (void)memset(&context, 0, sizeof(context));
  context.input = input;
  context.frontend = input->frontend_output;
  context.frontend_result = input->frontend_result;
  context.emit = emit;
  context.output = output;
  if (!lower_all(&context)) {
    result->status = W_SEED_CONSTIR_INVALID;
    return W_SEED_CONSTIR_INVALID;
  }
  if (!receipt_size_for_counts(&context.counts, &context.counts.receipt_bytes)) {
    result->status = W_SEED_CONSTIR_INVALID;
    return W_SEED_CONSTIR_INVALID;
  }
  *counts = context.counts;
  result->required = context.counts;
  result->barrier_function = W_SEED_CONSTIR_NONE;
  result->barrier_span = (w_seed_span){0, 0};
  if (!emit) {
    result->status = W_SEED_CONSTIR_OK;
    return W_SEED_CONSTIR_OK;
  }
  if (output == NULL || output->function_capacity < counts->functions ||
      output->parameter_capacity < counts->parameters ||
      output->node_capacity < counts->nodes ||
      output->call_argument_capacity < counts->call_arguments ||
      output->switch_arm_capacity < counts->switch_arms ||
      output->membership_case_capacity < counts->membership_cases ||
      output->statement_capacity < counts->statements ||
      output->local_capacity < counts->locals ||
      output->diagnostic_capacity < counts->diagnostics ||
      output->receipt_capacity < counts->receipt_bytes ||
      (counts->functions != 0 && output->functions == NULL) ||
      (counts->parameters != 0 && output->parameters == NULL) ||
      (counts->nodes != 0 && output->nodes == NULL) ||
      (counts->call_arguments != 0 && output->call_arguments == NULL) ||
      (counts->switch_arms != 0 && output->switch_arms == NULL) ||
      (counts->membership_cases != 0 && output->membership_cases == NULL) ||
      (counts->statements != 0 && output->statements == NULL) ||
      (counts->locals != 0 && output->locals == NULL) ||
      (counts->diagnostics != 0 && output->diagnostics == NULL) ||
      (counts->receipt_bytes != 0 && output->receipt == NULL)) {
    result->status = W_SEED_CONSTIR_CAPACITY;
    return W_SEED_CONSTIR_CAPACITY;
  }
  result->written = context.counts;
  if (!write_receipt(output, &context.counts)) {
    result->status = W_SEED_CONSTIR_INVALID;
    return W_SEED_CONSTIR_INVALID;
  }
  result->status = W_SEED_CONSTIR_OK;
  return W_SEED_CONSTIR_OK;
}

static bool output_has_capacity(const w_seed_constir_output *output,
                                const w_seed_constir_counts *counts) {
  if (output == NULL || counts == NULL) return false;
  return output->function_capacity >= counts->functions &&
         output->parameter_capacity >= counts->parameters &&
         output->node_capacity >= counts->nodes &&
         output->call_argument_capacity >= counts->call_arguments &&
         output->switch_arm_capacity >= counts->switch_arms &&
         output->membership_case_capacity >= counts->membership_cases &&
         output->statement_capacity >= counts->statements &&
         output->local_capacity >= counts->locals &&
         output->diagnostic_capacity >= counts->diagnostics &&
         output->receipt_capacity >= counts->receipt_bytes &&
         (counts->functions == 0u || output->functions != NULL) &&
         (counts->parameters == 0u || output->parameters != NULL) &&
         (counts->nodes == 0u || output->nodes != NULL) &&
         (counts->call_arguments == 0u || output->call_arguments != NULL) &&
         (counts->switch_arms == 0u || output->switch_arms != NULL) &&
         (counts->membership_cases == 0u || output->membership_cases != NULL) &&
         (counts->statements == 0u || output->statements != NULL) &&
         (counts->locals == 0u || output->locals != NULL) &&
         (counts->diagnostics == 0u || output->diagnostics != NULL) &&
         (counts->receipt_bytes == 0u || output->receipt != NULL);
}

w_seed_constir_status w_seed_constir_measure(
    const w_seed_constir_input *input, w_seed_constir_counts *counts,
    w_seed_constir_result *result) {
  return lower_measure_or_run(input, NULL, false, counts, result);
}

w_seed_constir_status w_seed_constir_run(
    const w_seed_constir_input *input, w_seed_constir_output *output,
    w_seed_constir_result *result) {
  if (result != NULL) (void)memset(result, 0, sizeof(*result));
  w_seed_constir_counts measured;
  w_seed_constir_result measured_result;
  const w_seed_constir_status measure_status =
      w_seed_constir_measure(input, &measured, &measured_result);
  if (measure_status != W_SEED_CONSTIR_OK) {
    if (result != NULL) result->status = measure_status;
    return measure_status;
  }
  if (!output_has_capacity(output, &measured)) {
    if (result != NULL) {
      result->status = W_SEED_CONSTIR_CAPACITY;
      result->required = measured;
      result->barrier_function = W_SEED_CONSTIR_NONE;
      result->barrier_span = (w_seed_span){0, 0};
    }
    return W_SEED_CONSTIR_CAPACITY;
  }
  return lower_measure_or_run(input, output, true, &measured, result);
}

typedef w_seed_constir_session_entry constir_const_memo_entry;

enum {
  CONSTIR_CONST_MEMO_EMPTY = 0,
  CONSTIR_CONST_MEMO_ACTIVE,
  CONSTIR_CONST_MEMO_READY,
};

typedef struct {
  const w_seed_constir_program *program;
  const w_seed_constir_value *arguments;
  size_t argument_count;
  w_seed_constir_quota quota;
  w_seed_constir_eval_workspace *workspace;
  w_seed_constir_eval_result *result;
  size_t steps;
  size_t current_depth;
  size_t peak_depth;
  bool runtime_failed;
  w_seed_constir_eval_frame *active_frame;
  w_seed_constir_session *session;
} constir_eval_context;

static bool eval_function(constir_eval_context *context,
                          size_t function_index,
                          const w_seed_constir_value *arguments,
                          size_t argument_count, size_t depth,
                          w_seed_constir_value *value);

static bool eval_statement_chain(constir_eval_context *context,
                                 const w_seed_constir_function *function,
                                 uint32_t statement_index, size_t depth,
                                 w_seed_constir_value *value, bool *returned);

/* Result quota uses a versioned, host-independent scalar encoding.  The
 * prefix is: version(1), kind(1), type index(4), type kind(1), signed(1),
 * width(2), enum base(4), enum case(4).  Bool adds one payload byte, integer
 * adds 16 little-endian value bytes, and enum has no additional payload. */
static bool result_encoded_bytes(const w_seed_constir_value *value,
                                 size_t *bytes) {
  if (value == NULL || bytes == NULL ||
      value->kind == W_SEED_CONSTIR_VALUE_INVALID)
    return false;
  const size_t prefix = 18u;
  switch (value->kind) {
    case W_SEED_CONSTIR_VALUE_BOOL:
      *bytes = prefix + 1u;
      return true;
    case W_SEED_CONSTIR_VALUE_INTEGER:
      *bytes = prefix + W_SEED_CONSTIR_INTEGER_BYTES;
      return true;
    case W_SEED_CONSTIR_VALUE_ENUM:
      *bytes = prefix;
      return true;
    default:
      return false;
  }
}

static bool value_kind_matches_type(const w_seed_constir_value *value,
                                    w_seed_frontend_type_kind kind,
                                    bool is_signed, uint16_t width,
                                    uint32_t enum_base) {
  if (value == NULL ||
      (value->type_kind != kind &&
       !(kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET &&
         value->type_kind == W_SEED_FRONTEND_TYPE_ENUM)) ||
      value->type_is_signed != is_signed || value->type_bit_width != width)
    return false;
  if (kind == W_SEED_FRONTEND_TYPE_ENUM ||
      kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET)
    return value->kind == W_SEED_CONSTIR_VALUE_ENUM &&
           value->enum_base_index == enum_base;
  if (kind == W_SEED_FRONTEND_TYPE_INTEGER)
    return value->kind == W_SEED_CONSTIR_VALUE_INTEGER;
  if (kind == W_SEED_FRONTEND_TYPE_BOOL)
    return value->kind == W_SEED_CONSTIR_VALUE_BOOL;
  if (kind == W_SEED_FRONTEND_TYPE_STRING)
    return value->kind == W_SEED_CONSTIR_VALUE_STRING &&
           value->string_count <= W_SEED_CONSTIR_MAX_STRING_BYTES &&
           (value->string_count == 0u || value->string_bytes != NULL);
  if (kind == W_SEED_FRONTEND_TYPE_STATIC_LIST)
    return value->kind == W_SEED_CONSTIR_VALUE_STATIC_LIST;
  return false;
}

static bool frontend_type_metadata_matches(
    const w_seed_frontend_type *left, const w_seed_frontend_type *right) {
  if (left == NULL || right == NULL || left->is_signed != right->is_signed ||
      left->bit_width != right->bit_width)
    return false;
  const bool left_enum = left->kind == W_SEED_FRONTEND_TYPE_ENUM ||
                         left->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  const bool right_enum = right->kind == W_SEED_FRONTEND_TYPE_ENUM ||
                          right->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  if (left_enum || right_enum)
    return left_enum && right_enum &&
           left->enum_base_index == right->enum_base_index;
  return left->kind == right->kind;
}

static bool eval_fail(constir_eval_context *context,
                      w_seed_constir_diagnostic_code code, w_seed_span span,
                      size_t quota_limit) {
  if (context == NULL || context->result == NULL) return false;
  if (!context->runtime_failed) {
    context->runtime_failed = true;
    context->result->diagnostic = code;
    context->result->diagnostic_span = span;
    context->result->quota_limit = quota_limit;
  }
  return false;
}

static bool eval_step(constir_eval_context *context, w_seed_span span) {
  if (context == NULL) return false;
  if (context->quota.steps != SIZE_MAX && context->steps >= context->quota.steps)
    return eval_fail(context, W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003, span,
                     context->quota.steps);
  context->steps += 1u;
  return true;
}

static bool eval_node_at(constir_eval_context *context,
                         const w_seed_constir_function *function,
                         uint32_t node_index, size_t depth,
                         w_seed_constir_value *value);

static bool eval_integer_unary(const w_seed_constir_node *node,
                               const w_seed_constir_value *input,
                               w_seed_constir_value *out) {
  if (node == NULL || input == NULL || out == NULL ||
      input->kind != W_SEED_CONSTIR_VALUE_INTEGER ||
      node->type_kind != W_SEED_FRONTEND_TYPE_INTEGER) return false;
  constir_bits bits;
  (void)memcpy(bits.bytes, input->integer_value, sizeof(bits.bytes));
  if (node->normalized_operator == W_SEED_CONSTIR_OPERATOR_NEGATE) {
    const bool input_negative =
        node->type_is_signed && bits_negative(&bits, node->type_bit_width);
    if (!node->type_is_signed && !bits_is_zero(&bits)) return false;
    bits_negate(&bits);
    if (!bits_fit(bits, node->type_is_signed, node->type_bit_width)) return false;
    if (input_negative) {
      constir_bits minimum;
      bits_limit(node->type_bit_width, true, &minimum);
      if (bits_compare_unsigned(&bits, &minimum) == 0) return false;
    }
    bits_mask(&bits, node->type_bit_width, node->type_is_signed);
  } else {
    return false;
  }
  (void)memset(out, 0, sizeof(*out));
  out->kind = W_SEED_CONSTIR_VALUE_INTEGER;
  out->type_index = node->type_index;
  out->type_kind = node->type_kind;
  out->type_is_signed = node->type_is_signed;
  out->type_bit_width = node->type_bit_width;
  (void)memcpy(out->integer_value, bits.bytes, sizeof(out->integer_value));
  return true;
}

static int compare_integer(const w_seed_constir_value *left,
                           const w_seed_constir_value *right) {
  constir_bits left_bits;
  constir_bits right_bits;
  (void)memcpy(left_bits.bytes, left->integer_value, sizeof(left_bits.bytes));
  (void)memcpy(right_bits.bytes, right->integer_value, sizeof(right_bits.bytes));
  if (left->type_is_signed && right->type_is_signed) {
    const bool left_negative =
        bits_negative(&left_bits, left->type_bit_width);
    const bool right_negative =
        bits_negative(&right_bits, right->type_bit_width);
    if (left_negative != right_negative) return left_negative ? -1 : 1;
  }
  return bits_compare_unsigned(&left_bits, &right_bits);
}

static bool integer_result(const w_seed_constir_node *node, constir_bits bits,
                           w_seed_constir_value *out) {
  if (node == NULL || out == NULL || node->type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
      node->type_bit_width == 0u || node->type_bit_width > 128u) return false;
  bits_mask(&bits, node->type_bit_width, node->type_is_signed);
  (void)memset(out, 0, sizeof(*out));
  out->kind = W_SEED_CONSTIR_VALUE_INTEGER;
  out->type_index = node->type_index;
  out->type_kind = node->type_kind;
  out->type_is_signed = node->type_is_signed;
  out->type_bit_width = node->type_bit_width;
  (void)memcpy(out->integer_value, bits.bytes, sizeof(out->integer_value));
  return true;
}

static bool integer_shift_count(const w_seed_constir_value *value,
                                size_t *count) {
  if (value == NULL || count == NULL ||
      value->kind != W_SEED_CONSTIR_VALUE_INTEGER ||
      value->type_bit_width == 0u ||
      value->type_bit_width > W_SEED_FRONTEND_TARGET_USIZE_BITS) return false;
  constir_bits bits;
  (void)memcpy(bits.bytes, value->integer_value, sizeof(bits.bytes));
  if (value->type_is_signed && bits_negative(&bits, value->type_bit_width))
    return false;
  const size_t target_bytes =
      (W_SEED_FRONTEND_TARGET_USIZE_BITS + 7u) / 8u;
  size_t result = 0u;
  for (size_t index = 0; index < sizeof(bits.bytes); index += 1u) {
    if (index >= target_bytes || index >= sizeof(size_t)) {
      if (bits.bytes[index] != 0u) return false;
      continue;
    }
    const unsigned int shift = (unsigned int)(index * 8u);
    /* The explicit D1 target width governs the value.  sizeof(size_t) is
     * used only as the host storage limit after the compile-time support
     * assertion above. */
    if (bits.bytes[index] != 0u && shift >= sizeof(size_t) * CHAR_BIT)
      return false;
    result |= (size_t)bits.bytes[index] << shift;
  }
  *count = result;
  return true;
}

static void bits_shift_right_one(constir_bits *value, uint16_t width,
                                 bool arithmetic) {
  if (value == NULL || width == 0u || width > 128u) return;
  const bool negative = arithmetic && bits_negative(value, width);
  uint8_t carry = 0u;
  for (size_t index = sizeof(value->bytes); index > 0; index -= 1u) {
    const uint8_t next_carry = (uint8_t)(value->bytes[index - 1u] & 1u);
    value->bytes[index - 1u] =
        (uint8_t)((value->bytes[index - 1u] >> 1u) | (carry << 7u));
    carry = next_carry;
  }
  if (negative) {
    const size_t bit = (size_t)width - 1u;
    value->bytes[bit / 8u] |= (uint8_t)(1u << (bit % 8u));
  }
  bits_mask(value, width, arithmetic);
}

static bool integer_power(const w_seed_constir_node *node,
                          const w_seed_constir_value *base,
                          const w_seed_constir_value *exponent,
                          w_seed_constir_value *out,
                          w_seed_constir_diagnostic_code *fault);

static bool eval_integer_binary(const w_seed_constir_node *node,
                                const w_seed_constir_value *left,
                                const w_seed_constir_value *right,
                                w_seed_constir_value *out,
                                w_seed_constir_diagnostic_code *fault) {
  if (fault != NULL) *fault = W_SEED_CONSTIR_DIAGNOSTIC_NONE;
  if (node == NULL || left == NULL || right == NULL || out == NULL ||
      left->kind != W_SEED_CONSTIR_VALUE_INTEGER ||
      right->kind != W_SEED_CONSTIR_VALUE_INTEGER ||
      node->type_kind != W_SEED_FRONTEND_TYPE_INTEGER) return false;
  constir_bits a;
  constir_bits b;
  constir_bits result;
  (void)memcpy(a.bytes, left->integer_value, sizeof(a.bytes));
  (void)memcpy(b.bytes, right->integer_value, sizeof(b.bytes));
  /* Widen operands to the already-resolved result type before checked
   * arithmetic.  Signed values are sign-extended; unsigned values are zero-
   * extended by the canonical ConstIR representation. */
  bits_mask(&a, node->type_bit_width, node->type_is_signed);
  bits_mask(&b, node->type_bit_width, node->type_is_signed);
  bool overflow = false;
  switch (node->normalized_operator) {
    case W_SEED_CONSTIR_OPERATOR_ADD: {
      bool carry = false;
      (void)bits_add(&a, &b, &result, &carry);
      if (node->type_is_signed) {
        const bool sa = bits_negative(&a, node->type_bit_width);
        const bool sb = bits_negative(&b, node->type_bit_width);
        const bool sr = bits_negative(&result, node->type_bit_width);
        overflow = sa == sb && sa != sr;
      } else {
        overflow = carry || !bits_fit(result, false, node->type_bit_width);
      }
      break;
    }
    case W_SEED_CONSTIR_OPERATOR_SUBTRACT: {
      bool borrow = false;
      (void)bits_subtract(&a, &b, &result, &borrow);
      if (node->type_is_signed) {
        const bool sa = bits_negative(&a, node->type_bit_width);
        const bool sb = bits_negative(&b, node->type_bit_width);
        const bool sr = bits_negative(&result, node->type_bit_width);
        overflow = sa != sb && sa != sr;
      } else {
        overflow = borrow;
      }
      break;
    }
    case W_SEED_CONSTIR_OPERATOR_MULTIPLY: {
      constir_bits am;
      constir_bits bm;
      bool aneg = false;
      bool bneg = false;
      bits_magnitude(a, node->type_is_signed, node->type_bit_width, &am, &aneg);
      bits_magnitude(b, node->type_is_signed, node->type_bit_width, &bm, &bneg);
      bool high = false;
      (void)bits_multiply(&am, &bm, &result, &high);
      const bool negative = aneg != bneg;
      constir_bits limit;
      if (node->type_is_signed) {
        bits_limit(node->type_bit_width, negative, &limit);
      } else {
        bits_limit_unsigned(node->type_bit_width, &limit);
      }
      overflow = high || bits_compare_unsigned(&result, &limit) > 0;
      if (!overflow && negative) bits_negate(&result);
      break;
    }
    case W_SEED_CONSTIR_OPERATOR_DIVIDE:
    case W_SEED_CONSTIR_OPERATOR_REMAINDER: {
      if (bits_is_zero(&b)) {
        if (fault != NULL) *fault = W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006;
        return false;
      }
      constir_bits am;
      constir_bits bm;
      bool aneg = false;
      bool bneg = false;
      bits_magnitude(a, node->type_is_signed, node->type_bit_width, &am, &aneg);
      bits_magnitude(b, node->type_is_signed, node->type_bit_width, &bm, &bneg);
      constir_bits quotient;
      constir_bits remainder;
      if (!bits_divide_unsigned(am, bm, &quotient, &remainder)) return false;
      const bool negative = aneg != bneg;
      if (node->normalized_operator == W_SEED_CONSTIR_OPERATOR_DIVIDE) {
        result = quotient;
        if (!negative) {
          constir_bits max_positive;
          if (node->type_is_signed) {
            bits_limit(node->type_bit_width, false, &max_positive);
          } else {
            bits_limit_unsigned(node->type_bit_width, &max_positive);
          }
          overflow = bits_compare_unsigned(&result, &max_positive) > 0;
        } else {
          constir_bits max_negative;
          bits_limit(node->type_bit_width, true, &max_negative);
          overflow = bits_compare_unsigned(&result, &max_negative) > 0;
        }
        if (!overflow && negative) bits_negate(&result);
      } else {
        result = remainder;
        if (aneg) bits_negate(&result);
      }
      break;
    }
    case W_SEED_CONSTIR_OPERATOR_BIT_AND:
    case W_SEED_CONSTIR_OPERATOR_BIT_OR:
    case W_SEED_CONSTIR_OPERATOR_BIT_XOR:
      for (size_t index = 0; index < sizeof(result.bytes); index += 1) {
        if (node->normalized_operator == W_SEED_CONSTIR_OPERATOR_BIT_AND)
          result.bytes[index] = (uint8_t)(a.bytes[index] & b.bytes[index]);
        else if (node->normalized_operator == W_SEED_CONSTIR_OPERATOR_BIT_OR)
          result.bytes[index] = (uint8_t)(a.bytes[index] | b.bytes[index]);
        else
          result.bytes[index] = (uint8_t)(a.bytes[index] ^ b.bytes[index]);
      }
      break;
    case W_SEED_CONSTIR_OPERATOR_SHIFT_LEFT: {
      size_t count = 0u;
      if (!integer_shift_count(right, &count) || count >= node->type_bit_width) {
        if (fault != NULL) *fault = W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006;
        return false;
      }
      w_seed_constir_node multiply = *node;
      multiply.normalized_operator = W_SEED_CONSTIR_OPERATOR_MULTIPLY;
      w_seed_constir_value current = *left;
      w_seed_constir_value two;
      (void)memset(&two, 0, sizeof(two));
      two.kind = W_SEED_CONSTIR_VALUE_INTEGER;
      two.type_index = node->type_index;
      two.type_kind = node->type_kind;
      two.type_is_signed = node->type_is_signed;
      two.type_bit_width = node->type_bit_width;
      two.integer_value[0] = 2u;
      for (size_t index = 0; index < count; index += 1u) {
        w_seed_constir_diagnostic_code nested_fault =
            W_SEED_CONSTIR_DIAGNOSTIC_NONE;
        if (!eval_integer_binary(&multiply, &current, &two, &current,
                                 &nested_fault)) {
          if (fault != NULL)
            *fault = nested_fault == W_SEED_CONSTIR_DIAGNOSTIC_NONE
                         ? W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006
                         : nested_fault;
          return false;
        }
      }
      *out = current;
      return true;
    }
    case W_SEED_CONSTIR_OPERATOR_SHIFT_RIGHT: {
      size_t count = 0u;
      if (!integer_shift_count(right, &count) || count >= node->type_bit_width) {
        if (fault != NULL) *fault = W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006;
        return false;
      }
      (void)memcpy(result.bytes, a.bytes, sizeof(result.bytes));
      for (size_t index = 0; index < count; index += 1u)
        bits_shift_right_one(&result, node->type_bit_width,
                             node->type_is_signed);
      return integer_result(node, result, out);
    }
    case W_SEED_CONSTIR_OPERATOR_POWER:
      return integer_power(node, left, right, out, fault);
    default:
      return false;
  }
  if (overflow) {
    if (fault != NULL) *fault = W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006;
    return false;
  }
  return integer_result(node, result, out);
}

static bool integer_power(const w_seed_constir_node *node,
                          const w_seed_constir_value *base,
                          const w_seed_constir_value *exponent,
                          w_seed_constir_value *out,
                          w_seed_constir_diagnostic_code *fault) {
  if (node == NULL || base == NULL || exponent == NULL || out == NULL ||
      base->kind != W_SEED_CONSTIR_VALUE_INTEGER ||
      exponent->kind != W_SEED_CONSTIR_VALUE_INTEGER) return false;
  constir_bits exponent_bits;
  (void)memcpy(exponent_bits.bytes, exponent->integer_value,
               sizeof(exponent_bits.bytes));
  if (exponent->type_is_signed &&
      bits_negative(&exponent_bits, exponent->type_bit_width)) {
    if (fault != NULL) *fault = W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006;
    return false;
  }
  size_t highest = SIZE_MAX;
  for (size_t index = sizeof(exponent_bits.bytes); index > 0; index -= 1u) {
    const uint8_t byte = exponent_bits.bytes[index - 1u];
    if (byte != 0u) {
      unsigned int bit = 7u;
      while ((byte & (uint8_t)(1u << bit)) == 0u) bit -= 1u;
      highest = (index - 1u) * 8u + bit;
      break;
    }
  }
  constir_bits one_bits;
  bits_zero(&one_bits);
  one_bits.bytes[0] = 1u;
  if (highest == SIZE_MAX) return integer_result(node, one_bits, out);
  w_seed_constir_value accumulator;
  (void)memset(&accumulator, 0, sizeof(accumulator));
  accumulator.kind = W_SEED_CONSTIR_VALUE_INTEGER;
  accumulator.type_index = node->type_index;
  accumulator.type_kind = node->type_kind;
  accumulator.type_is_signed = node->type_is_signed;
  accumulator.type_bit_width = node->type_bit_width;
  accumulator.integer_value[0] = 1u;
  w_seed_constir_value power = *base;
  w_seed_constir_node multiply = *node;
  multiply.normalized_operator = W_SEED_CONSTIR_OPERATOR_MULTIPLY;
  for (size_t bit = 0; bit <= highest; bit += 1u) {
    if ((exponent_bits.bytes[bit / 8u] & (uint8_t)(1u << (bit % 8u))) != 0u) {
      w_seed_constir_diagnostic_code nested_fault =
          W_SEED_CONSTIR_DIAGNOSTIC_NONE;
      if (!eval_integer_binary(&multiply, &accumulator, &power, &accumulator,
                               &nested_fault)) {
        if (fault != NULL)
          *fault = nested_fault == W_SEED_CONSTIR_DIAGNOSTIC_NONE
                       ? W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006
                       : nested_fault;
        return false;
      }
    }
    if (bit != highest) {
      w_seed_constir_diagnostic_code nested_fault =
          W_SEED_CONSTIR_DIAGNOSTIC_NONE;
      if (!eval_integer_binary(&multiply, &power, &power, &power,
                               &nested_fault)) {
        if (fault != NULL)
          *fault = nested_fault == W_SEED_CONSTIR_DIAGNOSTIC_NONE
                       ? W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006
                       : nested_fault;
        return false;
      }
    }
  }
  *out = accumulator;
  return true;
}

static const w_seed_constir_function *program_function_for_frontend(
    const constir_eval_context *context, uint32_t frontend_function,
    size_t *index) {
  if (context == NULL || context->program == NULL ||
      context->program->functions == NULL) return NULL;
  for (size_t candidate = 0; candidate < context->program->function_count;
       candidate += 1) {
    const w_seed_constir_function *function =
        &context->program->functions[candidate];
    if (function->origin ==
            W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION &&
        function->frontend_function == frontend_function) {
      if (index != NULL) *index = candidate;
      return function;
    }
  }
  return NULL;
}

static const w_seed_constir_function *program_function_for_const(
    const constir_eval_context *context, uint32_t const_declaration,
    size_t *index) {
  if (context == NULL || context->program == NULL ||
      context->program->functions == NULL ||
      const_declaration == W_SEED_CONSTIR_NONE)
    return NULL;
  for (size_t candidate = 0; candidate < context->program->function_count;
       candidate += 1) {
    const w_seed_constir_function *function =
        &context->program->functions[candidate];
    if (function->origin ==
            W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION &&
        function->frontend_const_declaration == const_declaration) {
      if (index != NULL) *index = candidate;
      return function;
    }
  }
  return NULL;
}

static constir_const_memo_entry *const_memo_find(
    constir_eval_context *context, uint32_t const_declaration) {
  if (context == NULL || context->session == NULL ||
      const_declaration == W_SEED_CONSTIR_NONE)
    return NULL;
  for (size_t index = 0u; index < context->session->count; index += 1u) {
    constir_const_memo_entry *entry = &context->session->entries[index];
    if (entry->state != CONSTIR_CONST_MEMO_EMPTY &&
        entry->declaration == const_declaration)
      return entry;
  }
  return NULL;
}

static constir_const_memo_entry *const_memo_add(
    constir_eval_context *context, uint32_t const_declaration) {
  if (context == NULL || context->session == NULL ||
      const_declaration == W_SEED_CONSTIR_NONE ||
      context->session->count >= W_SEED_CONSTIR_MAX_CONST_MEMO_ENTRIES)
    return NULL;
  const size_t index = context->session->count;
  context->session->count += 1u;
  constir_const_memo_entry *entry = &context->session->entries[index];
  (void)memset(entry, 0, sizeof(*entry));
  entry->declaration = const_declaration;
  entry->state = CONSTIR_CONST_MEMO_ACTIVE;
  return entry;
}

static bool const_memo_counter_increment(size_t *counter) {
  if (counter == NULL || *counter == SIZE_MAX) return false;
  *counter += 1u;
  return true;
}

static const w_seed_frontend_const_declaration *program_const_declaration_at(
    const w_seed_constir_program *program, uint32_t index) {
  if (program == NULL || program->frontend_output == NULL ||
      program->frontend_result == NULL || index == W_SEED_CONSTIR_NONE ||
      (size_t)index >= program->frontend_result->written.const_declarations)
    return NULL;
  if (program->frontend_output->const_declarations == NULL) return NULL;
  return &program->frontend_output->const_declarations[index];
}

static const w_seed_constir_parameter *function_parameter_for_ordinal(
    const constir_eval_context *context,
    const w_seed_constir_function *function, uint32_t ordinal) {
  if (context == NULL || context->program == NULL || function == NULL ||
      context->program->parameters == NULL || ordinal >= function->parameter_count ||
      (size_t)function->first_parameter + ordinal >=
          context->program->parameter_count) return NULL;
  const w_seed_constir_parameter *parameter =
      &context->program->parameters[function->first_parameter + ordinal];
  if (parameter->owner_function != function->frontend_function ||
      parameter->ordinal != ordinal) return NULL;
  return parameter;
}

static bool range_valid(size_t start, size_t count, size_t total);
static bool enum_case_identity_valid(const w_seed_constir_program *program,
                                     uint32_t enum_base, uint32_t enum_case);
static bool node_matches_type(const w_seed_constir_node *node,
                              w_seed_frontend_type_kind kind, bool is_signed,
                              uint16_t width, uint32_t enum_base);

static bool constir_string_value_valid(
    const w_seed_constir_value *value,
    const w_seed_constir_program *program) {
  if (value == NULL || value->kind != W_SEED_CONSTIR_VALUE_STRING ||
      value->type_kind != W_SEED_FRONTEND_TYPE_STRING ||
      value->type_is_signed || value->type_bit_width != 0u ||
      value->string_count > W_SEED_CONSTIR_MAX_STRING_BYTES ||
      (value->string_count != 0u && value->string_bytes == NULL))
    return false;
  if (program == NULL || program->frontend_output == NULL ||
      program->frontend_result == NULL)
    return true;
  const w_seed_frontend_output *frontend = program->frontend_output;
  if (frontend->types == NULL || value->type_index == W_SEED_CONSTIR_NONE ||
      (size_t)value->type_index >= program->frontend_result->written.types)
    return false;
  const w_seed_frontend_type *type = &frontend->types[value->type_index];
  if (type->kind != W_SEED_FRONTEND_TYPE_STRING || type->is_signed ||
      type->bit_width != 0u)
    return false;
  /* String values are borrowed caller-owned buffers.  The source-backed
   * relation is checked before conversion; ConstIR itself must not impose an
   * undocumented dependency on the frontend arena. */
  return true;
}

static bool validate_value_against_parameter(
  const w_seed_constir_value *value,
  const w_seed_constir_parameter *parameter,
  const w_seed_constir_program *program) {
  if (value == NULL || parameter == NULL ||
      !value_kind_matches_type(value, parameter->type_kind,
                               parameter->type_is_signed,
                               parameter->type_bit_width,
                               parameter->enum_base_index)) return false;
  /* Existing D1 enum/list values are checked by their canonical domain
   * metadata below.  String conversion additionally carries the exact
   * source type index through its helper. */
  if (value->kind == W_SEED_CONSTIR_VALUE_STRING &&
      !constir_string_value_valid(value, program))
    return false;
  if (value->kind == W_SEED_CONSTIR_VALUE_INTEGER) {
    constir_bits bits;
    (void)memcpy(bits.bytes, value->integer_value, sizeof(bits.bytes));
    return bits_fit(bits, value->type_is_signed, value->type_bit_width);
  }
  if (value->kind == W_SEED_CONSTIR_VALUE_ENUM) {
    if (program == NULL || program->frontend_output == NULL ||
        program->frontend_result == NULL ||
        program->frontend_output->enums == NULL ||
        program->frontend_output->enum_cases == NULL ||
        (size_t)value->enum_base_index >=
            program->frontend_result->written.enums)
      return false;
    if ((size_t)value->enum_case_index >=
        program->frontend_result->written.enum_cases)
      return false;
    const w_seed_frontend_enum_case *enum_case =
        &program->frontend_output->enum_cases[value->enum_case_index];
    if (enum_case->owner_enum != parameter->enum_base_index) return false;
    if (parameter->type_kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET) {
      if (program->frontend_output->types == NULL ||
          (size_t)parameter->type_index >=
              program->frontend_result->written.types)
        return false;
      const w_seed_frontend_type *type =
          &program->frontend_output->types[parameter->type_index];
      if (program->frontend_output->enum_subset_members == NULL ||
          !range_valid(type->first_subset_member, type->subset_member_count,
                       program->frontend_result->written.enum_subset_members))
        return false;
      bool member = false;
      for (uint32_t offset = 0; offset < type->subset_member_count; offset += 1u) {
        const w_seed_frontend_enum_subset_member *item =
            &program->frontend_output->enum_subset_members[
                type->first_subset_member + offset];
        if (item->enum_base_index == value->enum_base_index &&
            item->enum_case_index == value->enum_case_index) {
          member = true;
          break;
        }
      }
      if (!member) return false;
    }
  }
  if (value->kind == W_SEED_CONSTIR_VALUE_STATIC_LIST) {
    if (program == NULL || program->frontend_output == NULL ||
        program->frontend_result == NULL ||
        (value->elements == NULL && value->element_count != 0u) ||
        value->element_type_index == W_SEED_CONSTIR_NONE ||
        parameter->type_kind != W_SEED_FRONTEND_TYPE_STATIC_LIST ||
        program->frontend_output->types == NULL ||
        (size_t)parameter->type_index >=
            program->frontend_result->written.types ||
        (size_t)value->type_index >= program->frontend_result->written.types)
      return false;
    const w_seed_frontend_type *parameter_list_type =
        &program->frontend_output->types[parameter->type_index];
    const w_seed_frontend_type *value_list_type =
        &program->frontend_output->types[value->type_index];
    if (parameter_list_type->kind != W_SEED_FRONTEND_TYPE_STATIC_LIST ||
        value_list_type->kind != W_SEED_FRONTEND_TYPE_STATIC_LIST ||
        !frontend_type_metadata_matches(parameter_list_type,
                                        value_list_type) ||
        parameter_list_type->element_type == W_SEED_CONSTIR_NONE ||
        value_list_type->element_type == W_SEED_CONSTIR_NONE ||
        value_list_type->element_type != value->element_type_index ||
        (size_t)parameter_list_type->element_type >=
            program->frontend_result->written.types ||
        (size_t)value_list_type->element_type >=
            program->frontend_result->written.types)
      return false;
    const w_seed_frontend_type *parameter_element_type =
        &program->frontend_output->types[parameter_list_type->element_type];
    const w_seed_frontend_type *value_element_type =
        &program->frontend_output->types[value_list_type->element_type];
    if (!frontend_type_metadata_matches(parameter_element_type,
                                        value_element_type)) return false;
    /* D1 borrows ordered lists of payloadless enum values.  Scalar, nested,
     * and heap-like element payloads need a separate checked representation. */
    if (parameter_element_type->kind != W_SEED_FRONTEND_TYPE_ENUM &&
        parameter_element_type->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET)
      return false;
    if (value->element_count > W_SEED_CONSTIR_MAX_STATIC_LIST_ELEMENTS)
      return false;
    for (size_t index = 0u; index < value->element_count; index += 1u) {
      const w_seed_constir_value *element = &value->elements[index];
      if (!value_kind_matches_type(element, value_element_type->kind,
                                   value_element_type->is_signed,
                                   value_element_type->bit_width,
                                   value_element_type->enum_base_index))
        return false;
      if (element->kind == W_SEED_CONSTIR_VALUE_ENUM &&
          !enum_case_identity_valid(program, element->enum_base_index,
                                    element->enum_case_index)) return false;
      if (value_element_type->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET) {
        if (program->frontend_output->enum_subset_members == NULL ||
            !range_valid(value_element_type->first_subset_member,
                         value_element_type->subset_member_count,
                         program->frontend_result->written.enum_subset_members))
          return false;
        bool member = false;
        for (uint32_t offset = 0u;
             offset < value_element_type->subset_member_count; offset += 1u) {
          const w_seed_frontend_enum_subset_member *item =
              &program->frontend_output->enum_subset_members[
                  value_element_type->first_subset_member + offset];
          if (item->enum_base_index == element->enum_base_index &&
              item->enum_case_index == element->enum_case_index) {
            member = true;
            break;
          }
        }
        if (!member) return false;
      }
    }
  }
  return true;
}

static bool range_valid(size_t start, size_t count, size_t total) {
  return start <= total && count <= total - start;
}

static bool program_string_slice_valid(const w_seed_constir_program *program,
                                       uint32_t offset, uint32_t count) {
  if (program == NULL || program->frontend_output == NULL ||
      program->frontend_result == NULL || offset == W_SEED_CONSTIR_NONE ||
      count > W_SEED_CONSTIR_MAX_STRING_BYTES)
    return false;
  const size_t total = program->frontend_result->written.const_bytes;
  if (program->frontend_output->const_bytes_capacity < total ||
      (size_t)offset > total || (size_t)count > total - offset)
    return false;
  return count == 0u || program->frontend_output->const_bytes != NULL;
}

static bool node_operator_valid(const w_seed_constir_node *node) {
  if (node == NULL) return false;
  switch (node->kind) {
    case W_SEED_CONSTIR_NODE_UNARY:
      return node->normalized_operator == W_SEED_CONSTIR_OPERATOR_NOT ||
             node->normalized_operator == W_SEED_CONSTIR_OPERATOR_NEGATE;
    case W_SEED_CONSTIR_NODE_BINARY:
      return node->normalized_operator >= W_SEED_CONSTIR_OPERATOR_ADD &&
             node->normalized_operator <= W_SEED_CONSTIR_OPERATOR_POWER;
    case W_SEED_CONSTIR_NODE_BOOL:
    case W_SEED_CONSTIR_NODE_INTEGER:
    case W_SEED_CONSTIR_NODE_ENUM_CASE:
    case W_SEED_CONSTIR_NODE_STRING:
    case W_SEED_CONSTIR_NODE_PARAMETER:
    case W_SEED_CONSTIR_NODE_CALL:
    case W_SEED_CONSTIR_NODE_SWITCH:
    case W_SEED_CONSTIR_NODE_MEMBERSHIP:
    case W_SEED_CONSTIR_NODE_LOCAL:
    case W_SEED_CONSTIR_NODE_STATIC_LIST_COUNT:
    case W_SEED_CONSTIR_NODE_STATIC_LIST_INDEX:
      return node->normalized_operator == W_SEED_CONSTIR_OPERATOR_INVALID;
    default:
      return false;
  }
}

static bool node_index_in_function(const w_seed_constir_function *function,
                                   uint32_t node_index) {
  if (function == NULL || node_index == W_SEED_CONSTIR_NONE ||
      function->first_node == W_SEED_CONSTIR_NONE) return false;
  return (size_t)node_index >= (size_t)function->first_node &&
         (size_t)node_index - (size_t)function->first_node <
              (size_t)function->node_count;
}

static bool statement_index_in_function(
    const w_seed_constir_function *function, uint32_t statement_index) {
  if (function == NULL || statement_index == W_SEED_CONSTIR_NONE ||
      function->first_statement == W_SEED_CONSTIR_NONE) return false;
  return (size_t)statement_index >= (size_t)function->first_statement &&
         (size_t)statement_index - (size_t)function->first_statement <
             (size_t)function->statement_count;
}

static bool local_index_in_function(const w_seed_constir_function *function,
                                    uint32_t local_index) {
  if (function == NULL || local_index == W_SEED_CONSTIR_NONE ||
      function->first_local == W_SEED_CONSTIR_NONE) return false;
  return (size_t)local_index >= (size_t)function->first_local &&
         (size_t)local_index - (size_t)function->first_local <
             (size_t)function->local_count;
}

static bool node_comparison_types_compatible(const w_seed_constir_node *left,
                                             const w_seed_constir_node *right) {
  if (left == NULL || right == NULL ||
      left->type_is_signed != right->type_is_signed) return false;
  const bool left_enum = left->type_kind == W_SEED_FRONTEND_TYPE_ENUM ||
                         left->type_kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  const bool right_enum = right->type_kind == W_SEED_FRONTEND_TYPE_ENUM ||
                          right->type_kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  if (left_enum || right_enum)
    return left_enum && right_enum && left->enum_base_index == right->enum_base_index;
  return left->type_kind == right->type_kind;
}

static bool node_matches_type(const w_seed_constir_node *node,
                              w_seed_frontend_type_kind kind, bool is_signed,
                              uint16_t width, uint32_t enum_base) {
  if (node == NULL || node->type_is_signed != is_signed ||
      node->type_bit_width != width) return false;
  const bool node_enum = node->type_kind == W_SEED_FRONTEND_TYPE_ENUM ||
                         node->type_kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  const bool expected_enum = kind == W_SEED_FRONTEND_TYPE_ENUM ||
                             kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  if (node_enum || expected_enum)
    return node_enum && expected_enum && node->enum_base_index == enum_base;
  return node->type_kind == kind;
}

static bool arithmetic_node_types_valid(const w_seed_constir_node *node,
                                        const w_seed_constir_node *left,
                                        const w_seed_constir_node *right) {
  if (node == NULL || left == NULL || right == NULL ||
      node->type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
      left->type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
      right->type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
      node->type_bit_width == 0u || left->type_bit_width == 0u ||
      right->type_bit_width == 0u ||
      left->type_is_signed != right->type_is_signed ||
      node->type_is_signed != left->type_is_signed)
    return false;
  return node->type_bit_width >= left->type_bit_width &&
         node->type_bit_width >= right->type_bit_width;
}

static bool node_eval_depth_valid(const w_seed_constir_program *program,
                                  const w_seed_constir_function *function,
                                  uint32_t node_index, size_t depth) {
  if (program == NULL || function == NULL || depth == 0u ||
      depth > W_SEED_CONSTIR_MAX_EVAL_DEPTH ||
      (size_t)node_index >= program->node_count ||
      !node_index_in_function(function, node_index)) return false;
  const w_seed_constir_node *node = &program->nodes[node_index];
  if (node->left != W_SEED_CONSTIR_NONE &&
      (!node_index_in_function(function, node->left) ||
       (size_t)node->left >= (size_t)node_index ||
       !node_eval_depth_valid(program, function, node->left, depth + 1u)))
    return false;
  if (node->right != W_SEED_CONSTIR_NONE &&
      (!node_index_in_function(function, node->right) ||
       (size_t)node->right >= (size_t)node_index ||
       !node_eval_depth_valid(program, function, node->right, depth + 1u)))
    return false;
  if (node->kind == W_SEED_CONSTIR_NODE_CALL &&
      node->call_argument_count != 0u) {
    for (uint32_t offset = 0; offset < node->call_argument_count; offset += 1u) {
      const w_seed_constir_call_argument *argument = &program->call_arguments[
          (size_t)node->first_call_argument + offset];
      if ((size_t)argument->node_index >= (size_t)node_index ||
          !node_eval_depth_valid(program, function, argument->node_index,
                                 depth + 1u)) return false;
    }
  }
  if (node->kind == W_SEED_CONSTIR_NODE_SWITCH) {
    for (uint32_t offset = 0; offset < node->switch_arm_count; offset += 1u) {
      const w_seed_constir_switch_arm *arm = &program->switch_arms[
          (size_t)node->first_switch_arm + offset];
      if ((size_t)arm->result_node >= (size_t)node_index ||
          !node_eval_depth_valid(program, function, arm->result_node,
                                 depth + 1u)) return false;
    }
  }
  return true;
}

static bool enum_case_identity_valid(const w_seed_constir_program *program,
                                     uint32_t enum_base, uint32_t enum_case) {
  if (program == NULL || enum_base == W_SEED_CONSTIR_NONE ||
      enum_case == W_SEED_CONSTIR_NONE) return false;
  if (program->frontend_output == NULL || program->frontend_result == NULL)
    return true;
  if (program->frontend_output->enums == NULL ||
      program->frontend_output->enum_cases == NULL ||
      (size_t)enum_base >= program->frontend_result->written.enums ||
      (size_t)enum_case >= program->frontend_result->written.enum_cases)
    return false;
  return program->frontend_output->enum_cases[enum_case].owner_enum == enum_base;
}

static bool function_result_matches_node(const w_seed_constir_program *program,
                                          const w_seed_constir_function *function,
                                          const w_seed_constir_node *node) {
  if (program == NULL || function == NULL || node == NULL) return false;
  if (function->origin ==
      W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION) {
    const w_seed_frontend_const_declaration *declaration =
        program_const_declaration_at(program,
                                     function->frontend_const_declaration);
    if (declaration == NULL || program->frontend_output == NULL ||
        program->frontend_result == NULL ||
        program->frontend_output->types == NULL ||
        (size_t)declaration->effective_type >=
             program->frontend_result->written.types)
      return false;
    const w_seed_frontend_type *type =
        &program->frontend_output->types[declaration->effective_type];
    return constir_result_type_supported(type->kind) &&
           node_matches_type(node, type->kind, type->is_signed,
                             type->bit_width, type->enum_base_index);
  }
  if (program->frontend_output != NULL && program->frontend_result != NULL &&
      program->frontend_output->functions != NULL &&
      program->frontend_output->types != NULL &&
      (size_t)function->frontend_function <
          program->frontend_result->written.functions) {
    const w_seed_frontend_function *frontend_function =
        &program->frontend_output->functions[function->frontend_function];
    if (frontend_function->return_type == W_SEED_FRONTEND_NONE ||
        (size_t)frontend_function->return_type >=
            program->frontend_result->written.types) return false;
    const w_seed_frontend_type *type =
        &program->frontend_output->types[frontend_function->return_type];
    if (!constir_result_type_supported(type->kind)) return false;
    return node_matches_type(node, type->kind, type->is_signed,
                             type->bit_width, type->enum_base_index);
  }
  if (function->root_statement != W_SEED_CONSTIR_NONE &&
      program->statements != NULL) {
    for (uint32_t offset = 0u; offset < function->statement_count; offset += 1u) {
      const w_seed_constir_statement *statement = &program->statements[
          (size_t)function->first_statement + offset];
      if (statement->kind == W_SEED_CONSTIR_STATEMENT_RETURN &&
          statement->expression_node != W_SEED_CONSTIR_NONE &&
          (size_t)statement->expression_node < program->node_count) {
        const w_seed_constir_node *candidate =
            &program->nodes[statement->expression_node];
        if (!constir_result_type_supported(candidate->type_kind)) return false;
        return node_matches_type(node, candidate->type_kind,
                                 candidate->type_is_signed,
                                 candidate->type_bit_width,
                                 candidate->enum_base_index);
      }
    }
  }
  return true;
}

static bool statement_chain_ends_return(
    const w_seed_constir_program *program,
    const w_seed_constir_function *function, uint32_t first_statement) {
  if (program == NULL || function == NULL ||
      !statement_index_in_function(function, first_statement) ||
      program->statements == NULL) return false;
  uint32_t current = first_statement;
  size_t guard = 0u;
  while (current != W_SEED_CONSTIR_NONE &&
         guard < (size_t)function->statement_count) {
    if (!statement_index_in_function(function, current)) return false;
    const w_seed_constir_statement *statement = &program->statements[current];
    if (statement->owner_function != function->frontend_function) return false;
    if (statement->next_sibling == W_SEED_CONSTIR_NONE)
      return statement->kind == W_SEED_CONSTIR_STATEMENT_RETURN;
    current = statement->next_sibling;
    guard += 1u;
  }
  return false;
}

/* Statement edges are caller-owned data.  Range and forward-edge checks alone
 * do not prove that the normalized tree is a tree: a sibling can bleed into a
 * child chain, two parents can alias one node, or an unreachable node can sit
 * in the function range.  Validate the exact direct-child chain lengths and
 * the single-parent/reachability invariant without recursion or allocation. */
static bool validate_statement_structure(
    const w_seed_constir_program *program,
    const w_seed_constir_function *function) {
  if (program == NULL || function == NULL || program->statements == NULL ||
      function->statement_count == 0u ||
      function->root_statement != function->first_statement)
    return false;
  const size_t count = function->statement_count;
  /* The frontend bounds one document at W_SEED_FRONTEND_MAX_CST_NODES.  Keep
   * this validator bounded for caller-owned mutable IR as well. */
  if (count > W_SEED_FRONTEND_MAX_CST_NODES) return false;
  const size_t first = (size_t)function->first_statement;
  uint8_t indegree[W_SEED_FRONTEND_MAX_CST_NODES] = {0u};

  /* Count every incoming relation once.  This pass is linear and lets the
   * chain checks below follow each disjoint sibling chain at most once. */
  for (size_t offset = 0u; offset < count; offset += 1u) {
    const size_t statement_index = first + offset;
    if (statement_index >= program->statement_count) return false;
    const w_seed_constir_statement *statement =
        &program->statements[statement_index];
    if (statement->owner_function != function->frontend_function) return false;

    const uint32_t edges[3] = {statement->next_sibling,
                               statement->first_child, statement->else_child};
    for (size_t edge_index = 0u; edge_index < 3u; edge_index += 1u) {
      const uint32_t edge = edges[edge_index];
      if (edge == W_SEED_CONSTIR_NONE) continue;
      if (!statement_index_in_function(function, edge) ||
          (size_t)edge <= statement_index) return false;
      const size_t target_offset = (size_t)edge - first;
      if (target_offset >= count || indegree[target_offset] == UINT8_MAX)
        return false;
      indegree[target_offset] = (uint8_t)(indegree[target_offset] + 1u);
      if (indegree[target_offset] > 1u) return false;
    }
    if (statement->first_child == W_SEED_CONSTIR_NONE &&
        statement->child_count != 0u)
      return false;
  }

  /* With all edges constrained to move forward, one root and one incoming
   * edge for every other statement prove reachability without a recursive
   * walk. */
  if (indegree[0] != 0u) return false;
  for (size_t offset = 1u; offset < count; offset += 1u) {
    if (indegree[offset] != 1u) return false;
  }

  /* Verify the exact top-level chain and every direct child/else chain.  The
   * indegree pass makes these chains disjoint, so their total work is O(S). */
  uint32_t chain = function->first_statement;
  size_t chain_count = 0u;
  while (chain != W_SEED_CONSTIR_NONE) {
    if (chain_count >= count || !statement_index_in_function(function, chain))
      return false;
    chain_count += 1u;
    chain = program->statements[chain].next_sibling;
  }
  for (size_t offset = 0u; offset < count; offset += 1u) {
    const w_seed_constir_statement *statement =
        &program->statements[first + offset];
    if (statement->first_child != W_SEED_CONSTIR_NONE) {
      size_t child_count = 0u;
      chain = statement->first_child;
      while (chain != W_SEED_CONSTIR_NONE) {
        if (child_count >= count ||
            !statement_index_in_function(function, chain)) return false;
        child_count += 1u;
        chain = program->statements[chain].next_sibling;
      }
      if (child_count != statement->child_count) return false;
    }
    if (statement->else_child != W_SEED_CONSTIR_NONE) {
      size_t else_count = 0u;
      chain = statement->else_child;
      while (chain != W_SEED_CONSTIR_NONE) {
        if (else_count >= count ||
            !statement_index_in_function(function, chain)) return false;
        else_count += 1u;
        chain = program->statements[chain].next_sibling;
      }
    }
  }
  return true;
}

static bool validate_function_origin(const w_seed_constir_program *program,
                                     const w_seed_constir_function *function,
                                     size_t function_index) {
  if (program == NULL || function == NULL) return false;
  if (function->origin ==
      W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION) {
    if (function->frontend_function == W_SEED_CONSTIR_NONE ||
        function->typed_const_expression_index != W_SEED_CONSTIR_NONE ||
        function->frontend_const_declaration != W_SEED_CONSTIR_NONE ||
        program->frontend_result == NULL ||
        (size_t)function->frontend_function >=
            program->frontend_result->written.functions)
      return false;
    for (size_t index = 0u; index < function_index; index += 1u) {
      const w_seed_constir_function *prior = &program->functions[index];
      if (prior->origin ==
              W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION &&
          prior->frontend_function == function->frontend_function)
        return false;
    }
    return true;
  }
  if (function->origin ==
      W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION) {
    if (function->frontend_function != W_SEED_CONSTIR_NONE ||
        function->typed_const_expression_index != W_SEED_CONSTIR_NONE ||
        function->frontend_const_declaration == W_SEED_CONSTIR_NONE ||
        program->frontend_result == NULL || program->frontend_output == NULL)
      return false;
    const w_seed_frontend_const_declaration *declaration =
        program_const_declaration_at(program,
                                     function->frontend_const_declaration);
    if (declaration == NULL ||
        declaration->module_index >=
            program->frontend_result->written.modules ||
        declaration->name.data == NULL || declaration->name.length == 0u ||
        !span_valid(declaration->span) ||
        !span_contains(declaration->span, declaration->body_span) ||
        declaration->initializer_expression == W_SEED_FRONTEND_NONE ||
        program->frontend_output->expressions == NULL ||
        (size_t)declaration->initializer_expression >=
            program->frontend_result->written.expressions)
      return false;
    if (program->frontend_output->modules == NULL) return false;
    const w_seed_frontend_module *module =
        &program->frontend_output->modules[declaration->module_index];
    if (!range_valid(module->first_const_declaration,
                     module->const_declaration_count,
                     program->frontend_result->written.const_declarations) ||
        function->frontend_const_declaration <
            module->first_const_declaration ||
        (size_t)function->frontend_const_declaration -
                module->first_const_declaration >=
            module->const_declaration_count ||
        declaration->symbol_index == W_SEED_FRONTEND_NONE ||
        program->frontend_output->symbols == NULL ||
        (size_t)declaration->symbol_index >=
            program->frontend_result->written.symbols)
      return false;
    const w_seed_frontend_symbol *symbol =
        &program->frontend_output->symbols[declaration->symbol_index];
    if (symbol->kind != W_SEED_FRONTEND_SYMBOL_CONST ||
        symbol->module_index != declaration->module_index ||
        symbol->owner_index != function->frontend_const_declaration ||
        symbol->exported != declaration->exported ||
        symbol->type_index != declaration->effective_type ||
        !frontend_text_equal(symbol->name, declaration->name) ||
        symbol->span.start_byte != declaration->span.start_byte ||
        symbol->span.end_byte != declaration->span.end_byte)
      return false;
    for (size_t index = 0u; index < function_index; index += 1u) {
      const w_seed_constir_function *prior = &program->functions[index];
      if (prior->origin ==
              W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION &&
          prior->frontend_const_declaration ==
              function->frontend_const_declaration)
        return false;
    }
    if (function->source_span.start_byte != declaration->span.start_byte ||
        function->source_span.end_byte != declaration->span.end_byte ||
        function->body_span.start_byte != declaration->body_span.start_byte ||
        function->body_span.end_byte != declaration->body_span.end_byte ||
        function->parameter_count != 0u ||
        function->first_parameter !=
            (function->lowerable ? 0u : W_SEED_CONSTIR_NONE) ||
        function->first_statement != W_SEED_CONSTIR_NONE ||
        function->statement_count != 0u || function->first_local !=
            W_SEED_CONSTIR_NONE || function->local_count != 0u)
      return false;
    const w_seed_frontend_expression *expression =
        &program->frontend_output
             ->expressions[declaration->initializer_expression];
    if (expression->owner_function != W_SEED_FRONTEND_NONE ||
        !span_contains(declaration->body_span, expression->span) ||
        (declaration->has_explicit_type
             ? declaration->declared_type != declaration->effective_type
             : declaration->declared_type != W_SEED_FRONTEND_NONE))
      return false;
    /* A well-formed but non-lowerable D7 initializer remains auditable as a
     * frontend expression.  It has no synthetic UNKNOWN type and therefore
     * no ConstIR body. */
    if (declaration->effective_type == W_SEED_FRONTEND_NONE) {
      if (declaration->has_explicit_type || function->lowerable ||
          function->node_count != 0u ||
          function->root_node != W_SEED_CONSTIR_NONE ||
          (expression->inferred_type != W_SEED_FRONTEND_NONE &&
           (size_t)expression->inferred_type >=
               program->frontend_result->written.types))
        return false;
      return true;
    }
    if (expression->inferred_type != declaration->effective_type) return false;
    const w_seed_frontend_type *type = NULL;
    if (program->frontend_output->types == NULL ||
        (size_t)declaration->effective_type >=
            program->frontend_result->written.types)
      return false;
    type = &program->frontend_output->types[declaration->effective_type];
    const bool scalar =
        type->kind == W_SEED_FRONTEND_TYPE_BOOL ||
        (type->kind == W_SEED_FRONTEND_TYPE_INTEGER && type->bit_width != 0u);
    if (function->lowerable !=
        (declaration->lowerable && expression->supported && scalar))
      return false;
    if (!function->lowerable) {
      if (function->node_count != 0u ||
          function->root_node != W_SEED_CONSTIR_NONE)
        return false;
      return true;
    }
    if (function->root_node == W_SEED_CONSTIR_NONE ||
        function->node_count == 0u || program->nodes == NULL ||
        (size_t)function->root_node >= program->node_count)
      return false;
    const w_seed_constir_node *node = &program->nodes[function->root_node];
    return node->owner_function == W_SEED_CONSTIR_NONE &&
           span_contains(declaration->body_span, node->source_span) &&
           node->type_index == declaration->effective_type &&
           node->type_kind == type->kind &&
           node->type_is_signed == type->is_signed &&
           node->type_bit_width == type->bit_width &&
           node->enum_base_index == type->enum_base_index;
  }
  if (function->origin !=
      W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION ||
      function->frontend_function != W_SEED_CONSTIR_NONE ||
      function->frontend_const_declaration != W_SEED_CONSTIR_NONE ||
      function->typed_const_expression_index == W_SEED_CONSTIR_NONE ||
      program->frontend_result == NULL || program->frontend_output == NULL ||
      program->frontend_output->typed_const_expressions == NULL ||
      program->frontend_output->generic_applications == NULL ||
      program->frontend_output->generic_arguments == NULL ||
      (size_t)function->typed_const_expression_index >=
          program->frontend_result->written.typed_const_expressions)
    return false;
  for (size_t index = 0u; index < function_index; index += 1u) {
    const w_seed_constir_function *prior = &program->functions[index];
    if (prior->origin ==
            W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION &&
        prior->typed_const_expression_index ==
            function->typed_const_expression_index)
      return false;
  }
  const w_seed_frontend_typed_const_expression *typed =
      &program->frontend_output
           ->typed_const_expressions[function->typed_const_expression_index];
  if (typed->owner_application == W_SEED_FRONTEND_NONE ||
      (size_t)typed->owner_application >=
          program->frontend_result->written.generic_applications ||
      typed->argument_ordinal >= W_SEED_FRONTEND_MAX_GENERIC_SLOTS ||
      typed->expression_index == W_SEED_FRONTEND_NONE ||
      (size_t)typed->expression_index >=
          program->frontend_result->written.expressions ||
      typed->expected_type == W_SEED_FRONTEND_NONE ||
      typed->effective_type == W_SEED_FRONTEND_NONE ||
      (size_t)typed->expected_type >= program->frontend_result->written.types ||
      (size_t)typed->effective_type >= program->frontend_result->written.types ||
      !span_valid(typed->span))
    return false;
  const w_seed_frontend_generic_application *application =
      &program->frontend_output->generic_applications[typed->owner_application];
  const bool executable_application =
      application->binding_status ==
      W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST;
  const bool audit_application =
      application->binding_status == W_SEED_FRONTEND_GENERIC_BINDING_INVALID ||
      application->binding_status ==
          W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED;
  if (typed->module_index != application->module_index ||
      (!executable_application && !audit_application) ||
      !application->requires_const_evaluation ||
      typed->argument_ordinal >= application->argument_count ||
      !range_valid(application->first_argument, application->argument_count,
                   program->frontend_result->written.generic_arguments))
    return false;
  const w_seed_frontend_generic_argument *argument =
      &program->frontend_output
           ->generic_arguments[application->first_argument +
                               typed->argument_ordinal];
  if (argument->typed_const_expression_index !=
      function->typed_const_expression_index ||
      argument->binding_status !=
          W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST ||
      argument->owner_application != typed->owner_application ||
      argument->source_ordinal != typed->argument_ordinal ||
      !span_contains(argument->span, typed->span))
    return false;
  if (function->source_span.start_byte != typed->span.start_byte ||
      function->source_span.end_byte != typed->span.end_byte ||
      function->body_span.start_byte != typed->span.start_byte ||
      function->body_span.end_byte != typed->span.end_byte)
    return false;
  /* A record retained for an INVALID/UNSUPPORTED application is audit-only;
   * it must never become an executable synthetic function. */
  if (function->lowerable && !executable_application) return false;
  const w_seed_frontend_expression *expression =
      &program->frontend_output->expressions[typed->expression_index];
  w_seed_frontend_type_kind expression_kind = W_SEED_FRONTEND_TYPE_UNKNOWN;
  bool expression_signed = false;
  uint16_t expression_width = 0u;
  uint32_t expression_enum = W_SEED_FRONTEND_NONE;
  w_seed_frontend_type_kind effective_kind = W_SEED_FRONTEND_TYPE_UNKNOWN;
  bool effective_signed = false;
  uint16_t effective_width = 0u;
  uint32_t effective_enum = W_SEED_FRONTEND_NONE;
  w_seed_frontend_type_kind expected_kind = W_SEED_FRONTEND_TYPE_UNKNOWN;
  bool expected_signed = false;
  uint16_t expected_width = 0u;
  uint32_t expected_enum = W_SEED_FRONTEND_NONE;
  if (expression->inferred_type == W_SEED_FRONTEND_NONE ||
      (size_t)expression->inferred_type >=
          program->frontend_result->written.types ||
      (size_t)typed->effective_type >=
          program->frontend_result->written.types ||
      program->frontend_output->types == NULL)
    return false;
  const w_seed_frontend_type *expression_type =
      &program->frontend_output->types[expression->inferred_type];
  const w_seed_frontend_type *effective_type =
      &program->frontend_output->types[typed->effective_type];
  const w_seed_frontend_type *expected_type =
      &program->frontend_output->types[typed->expected_type];
  expression_kind = expression_type->kind;
  expression_signed = expression_type->is_signed;
  expression_width = expression_type->bit_width;
  expression_enum = expression_type->enum_base_index;
  effective_kind = effective_type->kind;
  effective_signed = effective_type->is_signed;
  effective_width = effective_type->bit_width;
  effective_enum = effective_type->enum_base_index;
  expected_kind = expected_type->kind;
  expected_signed = expected_type->is_signed;
  expected_width = expected_type->bit_width;
  expected_enum = expected_type->enum_base_index;
  if (expression->owner_function != W_SEED_FRONTEND_NONE ||
      expression_kind != effective_kind || expression_signed != effective_signed ||
      expression_width != effective_width || expression_enum != effective_enum ||
      expected_kind != effective_kind || expected_signed != effective_signed ||
      expected_width != effective_width || expected_enum != effective_enum ||
      !expression->supported || !span_contains(typed->span, expression->span))
    return false;
  if (function->lowerable) {
    if (function->parameter_count != 0u || function->first_parameter != 0u ||
        function->root_statement != W_SEED_CONSTIR_NONE ||
        function->statement_count != 0u ||
        function->first_local != W_SEED_CONSTIR_NONE ||
        function->local_count != 0u || function->root_node == W_SEED_CONSTIR_NONE ||
        program->nodes == NULL ||
        (size_t)function->root_node >= program->node_count)
      return false;
    const w_seed_constir_node *node = &program->nodes[function->root_node];
    if (node->owner_function != W_SEED_CONSTIR_NONE ||
        !span_contains(typed->span, node->source_span) ||
        node->type_kind != effective_kind ||
        node->type_is_signed != effective_signed ||
        node->type_bit_width != effective_width ||
        node->enum_base_index != effective_enum)
      return false;
  } else if (function->parameter_count != 0u ||
             function->first_parameter != W_SEED_CONSTIR_NONE ||
             function->node_count != 0u ||
             function->root_node != W_SEED_CONSTIR_NONE ||
             function->statement_count != 0u ||
             function->root_statement != W_SEED_CONSTIR_NONE ||
             function->local_count != 0u ||
             function->first_local != W_SEED_CONSTIR_NONE)
    return false;
  return true;
}

static bool validate_program(const w_seed_constir_program *program) {
  if (program == NULL) return false;
  if (program->function_count == 0u)
    return program->parameter_count == 0u && program->node_count == 0u &&
           program->call_argument_count == 0u &&
           program->switch_arm_count == 0u &&
           program->membership_case_count == 0u &&
           program->statement_count == 0u && program->local_count == 0u;
  if (program->functions == NULL) return false;
  if ((program->parameter_count != 0 && program->parameters == NULL) ||
      (program->node_count != 0 && program->nodes == NULL)) return false;
  bool saw_const_declaration = false;
  bool saw_typed_expression = false;
  for (size_t index = 0; index < program->function_count; index += 1) {
    const w_seed_constir_function *function = &program->functions[index];
    if (!validate_function_origin(program, function, index)) return false;
    if (function->origin ==
        W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION) {
      if (saw_const_declaration || saw_typed_expression) return false;
    } else if (function->origin ==
               W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION) {
      if (saw_typed_expression) return false;
      saw_const_declaration = true;
    } else if (function->origin ==
               W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION) {
      saw_typed_expression = true;
    }
    if (!function->lowerable) continue;
    const bool statement_mode = function->root_statement != W_SEED_CONSTIR_NONE;
    if ((!statement_mode && (function->root_node == W_SEED_CONSTIR_NONE ||
                             (size_t)function->root_node >= program->node_count)) ||
        (statement_mode && (function->root_statement >= program->statement_count ||
                            function->statement_count == 0u)) ||
        (!statement_mode && function->node_count == 0u) ||
        (uint64_t)function->first_node + (uint64_t)function->node_count >
            (uint64_t)UINT32_MAX ||
        (uint64_t)function->first_parameter +
                (uint64_t)function->parameter_count >
            (uint64_t)UINT32_MAX ||
        (uint64_t)function->first_statement +
                (uint64_t)function->statement_count >
            (uint64_t)UINT32_MAX ||
        (uint64_t)function->first_local +
                (uint64_t)function->local_count >
            (uint64_t)UINT32_MAX ||
        ((!statement_mode &&
          !range_valid(function->first_node, function->node_count,
                       program->node_count)) ||
         (statement_mode && function->node_count != 0u &&
          !range_valid(function->first_node, function->node_count,
                       program->node_count))) ||
        (statement_mode &&
         !range_valid(function->first_statement, function->statement_count,
                      program->statement_count)) ||
        (statement_mode && function->local_count != 0u &&
         !range_valid(function->first_local, function->local_count,
                      program->local_count)) ||
        !range_valid(function->first_parameter, function->parameter_count,
                     program->parameter_count)) return false;
    if (!statement_mode && (!node_index_in_function(function, function->root_node) ||
        (program->frontend_output != NULL &&
         program->frontend_result != NULL &&
         (program->frontend_output->types == NULL ||
          program->frontend_result->written.types == 0u)))) return false;
    if (!statement_mode && !constir_result_type_supported(
                               program->nodes[function->root_node].type_kind))
      return false;
    for (uint32_t offset = 0; offset < function->parameter_count; offset += 1) {
      const w_seed_constir_parameter *parameter = function_parameter_for_ordinal(
          &(constir_eval_context){.program = program}, function, offset);
      if (parameter == NULL || !span_valid(parameter->source_span) ||
          parameter->type_index == W_SEED_CONSTIR_NONE ||
          (parameter->type_kind == W_SEED_FRONTEND_TYPE_INTEGER &&
           (parameter->type_bit_width == 0u || parameter->type_bit_width > 128u)))
        return false;
    }
    if (statement_mode) {
      if (program->statements == NULL || function->root_statement !=
              function->first_statement)
        return false;
      for (uint32_t offset = 0u; offset < function->statement_count; offset += 1u) {
        const size_t statement_index =
            (size_t)function->first_statement + offset;
        if (statement_index >= program->statement_count) return false;
        const w_seed_constir_statement *statement =
            &program->statements[statement_index];
        if (statement->owner_function != function->frontend_function ||
            statement->kind == W_SEED_CONSTIR_STATEMENT_INVALID ||
            !span_valid(statement->source_span)) return false;
        if ((statement->next_sibling != W_SEED_CONSTIR_NONE &&
             (!statement_index_in_function(function, statement->next_sibling) ||
              statement->next_sibling <= statement_index)) ||
            (statement->first_child != W_SEED_CONSTIR_NONE &&
             (!statement_index_in_function(function, statement->first_child) ||
              statement->first_child <= statement_index ||
              statement->child_count == 0u)) ||
            (statement->first_child == W_SEED_CONSTIR_NONE &&
             statement->child_count != 0u) ||
            (statement->else_child != W_SEED_CONSTIR_NONE &&
             (!statement_index_in_function(function, statement->else_child) ||
              statement->else_child <= statement_index))) return false;
        if (statement->expression_node != W_SEED_CONSTIR_NONE &&
            (!node_index_in_function(function, statement->expression_node) ||
             (size_t)statement->expression_node >= program->node_count))
          return false;
        if (statement->condition_node != W_SEED_CONSTIR_NONE &&
            (!node_index_in_function(function, statement->condition_node) ||
             (size_t)statement->condition_node >= program->node_count))
          return false;
        if (statement->lower_node != W_SEED_CONSTIR_NONE &&
            (!node_index_in_function(function, statement->lower_node) ||
             (size_t)statement->lower_node >= program->node_count)) return false;
        if (statement->upper_node != W_SEED_CONSTIR_NONE &&
            (!node_index_in_function(function, statement->upper_node) ||
             (size_t)statement->upper_node >= program->node_count)) return false;
        const bool local_fields_clear =
            statement->local_ordinal == W_SEED_CONSTIR_NONE &&
            statement->local_type_index == W_SEED_CONSTIR_NONE &&
            statement->local_type_kind == W_SEED_FRONTEND_TYPE_UNKNOWN &&
            !statement->local_type_is_signed && statement->local_type_bit_width == 0u;
        switch (statement->kind) {
          case W_SEED_CONSTIR_STATEMENT_RETURN:
            if (statement->expression_node == W_SEED_CONSTIR_NONE ||
                statement->condition_node != W_SEED_CONSTIR_NONE ||
                statement->first_child != W_SEED_CONSTIR_NONE ||
                statement->child_count != 0u ||
                statement->else_child != W_SEED_CONSTIR_NONE ||
                statement->lower_node != W_SEED_CONSTIR_NONE ||
                statement->upper_node != W_SEED_CONSTIR_NONE ||
                statement->half_open != 0u || !local_fields_clear ||
                !constir_result_type_supported(
                    program->nodes[statement->expression_node].type_kind) ||
                !function_result_matches_node(
                    program, function,
                    &program->nodes[statement->expression_node]))
              return false;
            break;
          case W_SEED_CONSTIR_STATEMENT_GUARD:
            if (statement->expression_node != W_SEED_CONSTIR_NONE ||
                statement->condition_node == W_SEED_CONSTIR_NONE ||
                statement->first_child != W_SEED_CONSTIR_NONE ||
                statement->child_count != 0u ||
                statement->else_child == W_SEED_CONSTIR_NONE ||
                statement->lower_node != W_SEED_CONSTIR_NONE ||
                statement->upper_node != W_SEED_CONSTIR_NONE ||
                statement->half_open != 0u || !local_fields_clear ||
                program->nodes[statement->condition_node].type_kind !=
                    W_SEED_FRONTEND_TYPE_BOOL ||
                !statement_chain_ends_return(program, function,
                                             statement->else_child))
              return false;
            break;
          case W_SEED_CONSTIR_STATEMENT_IF:
            if (statement->expression_node != W_SEED_CONSTIR_NONE ||
                statement->condition_node == W_SEED_CONSTIR_NONE ||
                statement->first_child == W_SEED_CONSTIR_NONE ||
                statement->child_count == 0u ||
                statement->lower_node != W_SEED_CONSTIR_NONE ||
                statement->upper_node != W_SEED_CONSTIR_NONE ||
                statement->half_open != 0u || !local_fields_clear ||
                program->nodes[statement->condition_node].type_kind !=
                    W_SEED_FRONTEND_TYPE_BOOL)
              return false;
            break;
          case W_SEED_CONSTIR_STATEMENT_FOR_RANGE: {
            if (statement->expression_node != W_SEED_CONSTIR_NONE ||
                statement->condition_node != W_SEED_CONSTIR_NONE ||
                statement->first_child == W_SEED_CONSTIR_NONE ||
                statement->child_count == 0u ||
                statement->else_child != W_SEED_CONSTIR_NONE ||
                statement->lower_node == W_SEED_CONSTIR_NONE ||
                statement->upper_node == W_SEED_CONSTIR_NONE ||
                statement->half_open != 1u ||
                statement->local_ordinal >= W_SEED_CONSTIR_MAX_PARAMETERS ||
                statement->local_ordinal >= function->local_count ||
                statement->local_type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
                statement->local_type_is_signed ||
                statement->local_type_bit_width == 0u ||
                statement->local_type_index == W_SEED_CONSTIR_NONE)
              return false;
            const w_seed_constir_node *lower =
                &program->nodes[statement->lower_node];
            const w_seed_constir_node *upper =
                &program->nodes[statement->upper_node];
            if (lower->type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
                upper->type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
                lower->type_is_signed || upper->type_is_signed ||
                lower->type_bit_width == 0u || upper->type_bit_width == 0u ||
                lower->type_index == W_SEED_CONSTIR_NONE ||
                lower->type_index != upper->type_index ||
                lower->type_kind != upper->type_kind ||
                lower->type_is_signed != upper->type_is_signed ||
                lower->type_bit_width != upper->type_bit_width ||
                lower->type_index != statement->local_type_index ||
                lower->type_kind != statement->local_type_kind ||
                lower->type_is_signed != statement->local_type_is_signed ||
                lower->type_bit_width != statement->local_type_bit_width)
              return false;
            if (program->locals == NULL ||
                !local_index_in_function(
                    function, function->first_local + statement->local_ordinal))
              return false;
            const w_seed_constir_local *local =
                &program->locals[function->first_local + statement->local_ordinal];
            if (local->owner_function != function->frontend_function ||
                local->ordinal != statement->local_ordinal ||
                local->type_index != statement->local_type_index ||
                local->type_kind != statement->local_type_kind ||
                local->type_is_signed != statement->local_type_is_signed ||
                local->type_bit_width != statement->local_type_bit_width)
              return false;
            break;
          }
          default:
            return false;
        }
      }
      if (!validate_statement_structure(program, function)) return false;
      if (function->local_count != 0u &&
          (program->locals == NULL || function->local_count >
               W_SEED_CONSTIR_MAX_PARAMETERS)) return false;
      for (uint32_t offset = 0u; offset < function->local_count; offset += 1u) {
        const size_t local_index = (size_t)function->first_local + offset;
        if (!local_index_in_function(function, (uint32_t)local_index)) return false;
        const w_seed_constir_local *local = &program->locals[local_index];
        if (local->owner_function != function->frontend_function ||
            local->ordinal != offset || local->type_kind !=
                W_SEED_FRONTEND_TYPE_INTEGER || local->type_is_signed ||
            local->type_bit_width == 0u || local->type_index == W_SEED_CONSTIR_NONE ||
            !span_valid(local->source_span)) return false;
        if (program->frontend_output != NULL &&
            program->frontend_result != NULL &&
            program->frontend_output->types != NULL &&
            (size_t)local->type_index < program->frontend_result->written.types) {
          const w_seed_frontend_type *type =
              &program->frontend_output->types[local->type_index];
          if (type->kind != local->type_kind || type->is_signed !=
                  local->type_is_signed || type->bit_width !=
                  local->type_bit_width) return false;
        }
      }
    }
    for (uint32_t offset = 0; offset < function->node_count; offset += 1) {
      const size_t node_index = (size_t)function->first_node + offset;
      if (node_index >= (size_t)UINT32_MAX) return false;
      const w_seed_constir_node *node = &program->nodes[node_index];
      if (node->owner_function != function->frontend_function ||
          node->kind == W_SEED_CONSTIR_NODE_INVALID ||
          node->type_index == W_SEED_CONSTIR_NONE ||
          !span_valid(node->source_span) ||
          !node_operator_valid(node)) return false;
      if (program->frontend_output != NULL &&
          program->frontend_result != NULL) {
        if (program->frontend_output->types == NULL ||
            (size_t)node->type_index >=
                program->frontend_result->written.types) return false;
        const w_seed_frontend_type *type =
            &program->frontend_output->types[node->type_index];
        if (type->kind != node->type_kind ||
            type->is_signed != node->type_is_signed ||
            type->bit_width != node->type_bit_width ||
            type->enum_base_index != node->enum_base_index) return false;
      }
      if (node->left != W_SEED_CONSTIR_NONE &&
          (!node_index_in_function(function, node->left) ||
           (size_t)node->left >= program->node_count ||
           (size_t)node->left >= node_index)) return false;
      if (node->right != W_SEED_CONSTIR_NONE &&
          (!node_index_in_function(function, node->right) ||
           (size_t)node->right >= program->node_count ||
           (size_t)node->right >= node_index)) return false;
      const w_seed_constir_node *left =
          node->left == W_SEED_CONSTIR_NONE ? NULL : &program->nodes[node->left];
      const w_seed_constir_node *right = node->right == W_SEED_CONSTIR_NONE
                                             ? NULL
                                             : &program->nodes[node->right];
      if (node->kind != W_SEED_CONSTIR_NODE_STRING &&
          (node->const_byte_offset != W_SEED_CONSTIR_NONE ||
           node->const_byte_count != 0u))
        return false;
      switch (node->kind) {
        case W_SEED_CONSTIR_NODE_BOOL:
          if (node->type_kind != W_SEED_FRONTEND_TYPE_BOOL) return false;
          break;
        case W_SEED_CONSTIR_NODE_INTEGER:
          if (node->type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
              node->type_bit_width == 0u || node->type_bit_width > 128u) {
            return false;
          } else {
            constir_bits literal;
            (void)memcpy(literal.bytes, node->integer_value,
                         sizeof(literal.bytes));
            if (!bits_fit(literal, node->type_is_signed,
                          node->type_bit_width)) return false;
          }
          break;
        case W_SEED_CONSTIR_NODE_STRING:
          if (node->type_kind != W_SEED_FRONTEND_TYPE_STRING ||
              !program_string_slice_valid(program, node->const_byte_offset,
                                           node->const_byte_count))
            return false;
          break;
        case W_SEED_CONSTIR_NODE_ENUM_CASE:
          if ((node->type_kind != W_SEED_FRONTEND_TYPE_ENUM &&
               node->type_kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET) ||
              node->enum_base_index == W_SEED_CONSTIR_NONE ||
              node->enum_case_index == W_SEED_CONSTIR_NONE ||
              !enum_case_identity_valid(program, node->enum_base_index,
                                         node->enum_case_index))
            return false;
          break;
        case W_SEED_CONSTIR_NODE_PARAMETER:
          if (node->parameter_ordinal >= function->parameter_count) return false;
          {
            constir_eval_context parameter_lookup = {.program = program};
            const w_seed_constir_parameter *parameter =
                function_parameter_for_ordinal(&parameter_lookup, function,
                                               node->parameter_ordinal);
            if (parameter == NULL ||
                !node_matches_type(node, parameter->type_kind,
                                   parameter->type_is_signed,
                                   parameter->type_bit_width,
                                   parameter->enum_base_index)) return false;
          }
          break;
        case W_SEED_CONSTIR_NODE_LOCAL: {
          if (left != NULL || right != NULL || node->local_ordinal >=
                  function->local_count || function->local_count == 0u ||
              program->locals == NULL) return false;
          const w_seed_constir_local *local = &program->locals[
              (size_t)function->first_local + node->local_ordinal];
          if (local->owner_function != function->frontend_function ||
              local->ordinal != node->local_ordinal ||
              !node_matches_type(node, local->type_kind,
                                 local->type_is_signed, local->type_bit_width,
                                 W_SEED_CONSTIR_NONE)) return false;
          break;
        }
        case W_SEED_CONSTIR_NODE_STATIC_LIST_COUNT:
          if (left == NULL || right != NULL ||
              left->type_kind != W_SEED_FRONTEND_TYPE_STATIC_LIST ||
              node->type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
              node->type_is_signed || node->type_bit_width == 0u) return false;
          break;
        case W_SEED_CONSTIR_NODE_STATIC_LIST_INDEX:
          if (left == NULL || right == NULL ||
              left->type_kind != W_SEED_FRONTEND_TYPE_STATIC_LIST ||
              right->type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
              right->type_is_signed || right->type_bit_width == 0u ||
              node->element_type_index == W_SEED_CONSTIR_NONE ||
              node->type_kind == W_SEED_FRONTEND_TYPE_STATIC_LIST ||
              node->type_index == W_SEED_CONSTIR_NONE) return false;
          if (program->frontend_output != NULL &&
              program->frontend_result != NULL &&
              program->frontend_output->types != NULL &&
              (size_t)left->type_index < program->frontend_result->written.types) {
            const w_seed_frontend_type *list_type =
                &program->frontend_output->types[left->type_index];
            if (list_type->element_type != node->element_type_index ||
                (size_t)node->element_type_index >=
                    program->frontend_result->written.types) return false;
            const w_seed_frontend_type *element_type =
                &program->frontend_output->types[node->element_type_index];
            if (!node_matches_type(node, element_type->kind,
                                   element_type->is_signed,
                                   element_type->bit_width,
                                   element_type->enum_base_index)) return false;
          }
          break;
        case W_SEED_CONSTIR_NODE_UNARY:
          if (left == NULL || right != NULL ||
              (node->normalized_operator == W_SEED_CONSTIR_OPERATOR_NOT &&
               (node->type_kind != W_SEED_FRONTEND_TYPE_BOOL ||
                !node_matches_type(left, W_SEED_FRONTEND_TYPE_BOOL, false, 0u,
                                   W_SEED_CONSTIR_NONE))) ||
              (node->normalized_operator == W_SEED_CONSTIR_OPERATOR_NEGATE &&
               (node->type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
                !node_matches_type(left, node->type_kind, node->type_is_signed,
                                   node->type_bit_width,
                                   node->enum_base_index))))
            return false;
          break;
        case W_SEED_CONSTIR_NODE_BINARY:
          if (left == NULL || right == NULL) return false;
          if ((node->normalized_operator == W_SEED_CONSTIR_OPERATOR_AND ||
               node->normalized_operator == W_SEED_CONSTIR_OPERATOR_OR) &&
              (node->type_kind != W_SEED_FRONTEND_TYPE_BOOL ||
               left->type_kind != W_SEED_FRONTEND_TYPE_BOOL ||
               right->type_kind != W_SEED_FRONTEND_TYPE_BOOL)) return false;
          if (operator_is_comparison(node->normalized_operator)) {
            if (node->type_kind != W_SEED_FRONTEND_TYPE_BOOL ||
                !node_comparison_types_compatible(left, right) ||
                (operator_is_ordered_comparison(node->normalized_operator) &&
                 left->type_kind != W_SEED_FRONTEND_TYPE_INTEGER)) return false;
          } else if (node->normalized_operator != W_SEED_CONSTIR_OPERATOR_AND &&
                     node->normalized_operator != W_SEED_CONSTIR_OPERATOR_OR &&
                     !arithmetic_node_types_valid(node, left, right)) return false;
          break;
        case W_SEED_CONSTIR_NODE_CALL:
          if (left != NULL || right != NULL) return false;
          break;
        case W_SEED_CONSTIR_NODE_MEMBERSHIP:
          if (left == NULL || right != NULL ||
              node->type_kind != W_SEED_FRONTEND_TYPE_BOOL ||
              (left->type_kind != W_SEED_FRONTEND_TYPE_ENUM &&
               left->type_kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET)) return false;
          break;
        case W_SEED_CONSTIR_NODE_SWITCH:
          if (left == NULL || right != NULL ||
              (left->type_kind != W_SEED_FRONTEND_TYPE_ENUM &&
               left->type_kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET)) return false;
          break;
        default:
          return false;
      }
      if (node->kind == W_SEED_CONSTIR_NODE_CALL) {
        constir_eval_context lookup = {.program = program};
        if (node->call_target_function != W_SEED_CONSTIR_NONE &&
            node->call_target_const_declaration != W_SEED_CONSTIR_NONE)
          return false;
        if (node->call_target_const_declaration != W_SEED_CONSTIR_NONE) {
          if (program->frontend_output == NULL ||
              program->frontend_result == NULL ||
              program->frontend_output->expressions == NULL ||
              node->frontend_expression == W_SEED_CONSTIR_NONE ||
              (size_t)node->frontend_expression >=
                  program->frontend_result->written.expressions)
            return false;
          const w_seed_frontend_expression *frontend_expression =
              &program->frontend_output->expressions[node->frontend_expression];
          /* The append-only relation must match the lowered dependency.  A
           * caller mutation of its identity invalidates the program before
           * graph preflight can classify or execute it. */
          if (frontend_expression->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
              frontend_expression->resolved_const_declaration !=
                  node->call_target_const_declaration ||
              frontend_expression->owner_function !=
                  (function->origin ==
                           W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION
                       ? function->frontend_function
                       : W_SEED_FRONTEND_NONE) ||
              !span_contains(node->source_span, frontend_expression->span))
            return false;
        }
        const w_seed_constir_function *target =
            node->call_target_function != W_SEED_CONSTIR_NONE
                ? program_function_for_frontend(&lookup,
                                                node->call_target_function,
                                                NULL)
                : program_function_for_const(&lookup,
                                             node->call_target_const_declaration,
                                             NULL);
        if (target == NULL || !target->lowerable ||
            (node->call_target_function == W_SEED_CONSTIR_NONE &&
             node->call_target_const_declaration == W_SEED_CONSTIR_NONE) ||
            target->parameter_count != node->call_argument_count ||
            node->call_argument_count > W_SEED_CONSTIR_MAX_PARAMETERS ||
            (node->call_argument_count != 0u &&
             (node->first_call_argument == W_SEED_CONSTIR_NONE ||
              program->call_arguments == NULL ||
            !range_valid(node->first_call_argument,
                         node->call_argument_count,
                         program->call_argument_count))) ||
            (node->call_argument_count == 0u &&
             node->first_call_argument != W_SEED_CONSTIR_NONE)) return false;
        if (target->root_statement != W_SEED_CONSTIR_NONE) {
          if (!function_result_matches_node(program, target, node)) return false;
        } else if (!node_index_in_function(target, target->root_node) ||
                   !node_matches_type(
                       node, program->nodes[target->root_node].type_kind,
                       program->nodes[target->root_node].type_is_signed,
                       program->nodes[target->root_node].type_bit_width,
                       program->nodes[target->root_node].enum_base_index)) {
          return false;
        }
        for (uint32_t argument_offset = 0;
             argument_offset < node->call_argument_count;
             argument_offset += 1u) {
          const size_t argument_index =
              (size_t)node->first_call_argument + argument_offset;
          const w_seed_constir_call_argument *argument =
              &program->call_arguments[argument_index];
          if (argument->owner_node != (uint32_t)node_index ||
              argument->parameter_ordinal >= target->parameter_count ||
              !node_index_in_function(function, argument->node_index) ||
              (size_t)argument->node_index >= node_index ||
              !span_valid(argument->source_span))
            return false;
          const w_seed_constir_parameter *target_parameter =
              function_parameter_for_ordinal(&lookup, target,
                                             argument->parameter_ordinal);
          if (target_parameter == NULL ||
              !node_matches_type(&program->nodes[argument->node_index],
                                 target_parameter->type_kind,
                                 target_parameter->type_is_signed,
                                 target_parameter->type_bit_width,
                                 target_parameter->enum_base_index)) return false;
          for (uint32_t prior = 0; prior < argument_offset; prior += 1u) {
            const w_seed_constir_call_argument *previous = &program->call_arguments[
                (size_t)node->first_call_argument + prior];
            if (previous->parameter_ordinal == argument->parameter_ordinal)
              return false;
          }
        }
      }
      if (node->kind == W_SEED_CONSTIR_NODE_SWITCH) {
        if (node->first_switch_arm == W_SEED_CONSTIR_NONE ||
            program->switch_arms == NULL ||
            !range_valid(node->first_switch_arm, node->switch_arm_count,
                         program->switch_arm_count) ||
            node->switch_arm_count == 0u) return false;
        for (uint32_t arm_offset = 0; arm_offset < node->switch_arm_count;
             arm_offset += 1u) {
          const w_seed_constir_switch_arm *arm = &program->switch_arms[
              (size_t)node->first_switch_arm + arm_offset];
          if (arm->owner_node != (uint32_t)node_index ||
              !node_index_in_function(function, arm->result_node) ||
              (size_t)arm->result_node >= node_index ||
              !span_valid(arm->source_span) || !span_valid(arm->pattern_span) ||
              !node_matches_type(&program->nodes[arm->result_node],
                                 node->type_kind, node->type_is_signed,
                                 node->type_bit_width, node->enum_base_index) ||
              (arm->pattern_kind != W_SEED_FRONTEND_SWITCH_PATTERN_ENUM_CASE &&
               arm->pattern_kind != W_SEED_FRONTEND_SWITCH_PATTERN_WILDCARD) ||
              (arm->pattern_kind == W_SEED_FRONTEND_SWITCH_PATTERN_WILDCARD
                   ? (arm->enum_base_index != W_SEED_CONSTIR_NONE ||
                      arm->enum_case_index != W_SEED_CONSTIR_NONE)
                   : (arm->enum_base_index != left->enum_base_index ||
                      !enum_case_identity_valid(program, arm->enum_base_index,
                                                arm->enum_case_index))))
            return false;
        }
      }
      if (node->kind == W_SEED_CONSTIR_NODE_MEMBERSHIP) {
        if (node->first_membership_case == W_SEED_CONSTIR_NONE ||
            program->membership_cases == NULL ||
            !range_valid(node->first_membership_case,
                         node->membership_case_count,
                         program->membership_case_count) ||
            node->membership_case_count == 0u) return false;
        for (uint32_t case_offset = 0;
             case_offset < node->membership_case_count;
             case_offset += 1u) {
          const w_seed_constir_membership_case *item =
              &program->membership_cases[(size_t)node->first_membership_case +
                                         case_offset];
          if (item->owner_node != (uint32_t)node_index ||
              !span_valid(item->source_span) ||
              item->enum_base_index != left->enum_base_index ||
              !enum_case_identity_valid(program, item->enum_base_index,
                                        item->enum_case_index)) return false;
        }
      }
    }
    if (!statement_mode &&
        !node_eval_depth_valid(program, function, function->root_node, 1u))
      return false;
  }
  if (program->frontend_result != NULL && program->frontend_output != NULL &&
      program->frontend_result->written.const_declarations != 0u) {
    for (size_t declaration_index = 0u;
         declaration_index <
         program->frontend_result->written.const_declarations;
         declaration_index += 1u) {
      size_t matches = 0u;
      for (size_t function_index = 0u;
           function_index < program->function_count; function_index += 1u) {
        const w_seed_constir_function *function =
            &program->functions[function_index];
        if (function->origin ==
                W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION &&
            function->frontend_const_declaration == declaration_index)
          matches += 1u;
      }
      if (matches != 1u) return false;
    }
  }
  return true;
}

bool w_seed_constir_validate_program(const w_seed_constir_program *program) {
  return validate_program(program);
}

bool w_seed_constir_validate_invocations_in_validated_program(
    const w_seed_constir_program *program,
    const w_seed_constir_invocation *invocations, size_t invocation_count) {
  if (program == NULL || (invocation_count != 0u && invocations == NULL))
    return false;
  if (program->function_count == 0u)
    return invocation_count == 0u && program->parameter_count == 0u &&
           program->node_count == 0u && program->call_argument_count == 0u &&
           program->switch_arm_count == 0u &&
           program->membership_case_count == 0u &&
           program->statement_count == 0u && program->local_count == 0u;
  if (program->functions == NULL) return false;
  for (size_t index = 0u; index < invocation_count; index += 1u) {
    const w_seed_constir_invocation *invocation = &invocations[index];
    if (invocation->function_index >= program->function_count ||
        (invocation->argument_count != 0u && invocation->arguments == NULL))
      return false;
    const w_seed_constir_function *function =
        &program->functions[invocation->function_index];
    if (!function->lowerable ||
        invocation->argument_count != function->parameter_count)
      return false;
    const constir_eval_context context = {.program = program};
    for (uint32_t ordinal = 0u; ordinal < function->parameter_count;
         ordinal += 1u) {
      const w_seed_constir_parameter *parameter =
          function_parameter_for_ordinal(&context, function, ordinal);
      if (!validate_value_against_parameter(
              &invocation->arguments[ordinal], parameter, program))
        return false;
    }
  }
  return true;
}

bool w_seed_constir_validate_invocations(
    const w_seed_constir_program *program,
    const w_seed_constir_invocation *invocations, size_t invocation_count) {
  return validate_program(program) &&
         w_seed_constir_validate_invocations_in_validated_program(
             program, invocations, invocation_count);
}

bool w_seed_constir_validate_invocation(
    const w_seed_constir_program *program, uint32_t function_index,
    const w_seed_constir_value *arguments, size_t argument_count) {
  const w_seed_constir_invocation invocation = {
      function_index, arguments, argument_count};
  return w_seed_constir_validate_invocations(program, &invocation, 1u);
}

static bool eval_comparison(const w_seed_constir_node *node,
                            const w_seed_constir_value *left,
                            const w_seed_constir_value *right,
                            bool *out) {
  if (node == NULL || left == NULL || right == NULL || out == NULL) return false;
  int comparison = 0;
  if (left->kind == W_SEED_CONSTIR_VALUE_INTEGER &&
      right->kind == W_SEED_CONSTIR_VALUE_INTEGER) {
    comparison = compare_integer(left, right);
  } else if (left->kind == W_SEED_CONSTIR_VALUE_BOOL &&
             right->kind == W_SEED_CONSTIR_VALUE_BOOL) {
    comparison = left->bool_value == right->bool_value
                     ? 0
                     : (left->bool_value ? 1 : -1);
  } else if (left->kind == W_SEED_CONSTIR_VALUE_ENUM &&
             right->kind == W_SEED_CONSTIR_VALUE_ENUM &&
             left->enum_base_index == right->enum_base_index) {
    comparison = left->enum_case_index < right->enum_case_index
                     ? -1
                     : (left->enum_case_index > right->enum_case_index ? 1 : 0);
  } else if (left->kind == W_SEED_CONSTIR_VALUE_STRING &&
             right->kind == W_SEED_CONSTIR_VALUE_STRING) {
    const size_t common = left->string_count < right->string_count
                              ? left->string_count
                              : right->string_count;
    if (common != 0u) {
      const int compared = memcmp(left->string_bytes, right->string_bytes,
                                   common);
      comparison = compared < 0 ? -1 : (compared > 0 ? 1 : 0);
    }
    if (comparison == 0) {
      comparison = left->string_count < right->string_count
                       ? -1
                       : (left->string_count > right->string_count ? 1 : 0);
    }
  } else {
    return false;
  }
  switch (node->normalized_operator) {
    case W_SEED_CONSTIR_OPERATOR_EQUAL:
      *out = comparison == 0;
      return true;
    case W_SEED_CONSTIR_OPERATOR_NOT_EQUAL:
      *out = comparison != 0;
      return true;
    case W_SEED_CONSTIR_OPERATOR_LESS:
      *out = comparison < 0;
      return true;
    case W_SEED_CONSTIR_OPERATOR_LESS_EQUAL:
      *out = comparison <= 0;
      return true;
    case W_SEED_CONSTIR_OPERATOR_GREATER:
      *out = comparison > 0;
      return true;
    case W_SEED_CONSTIR_OPERATOR_GREATER_EQUAL:
      *out = comparison >= 0;
      return true;
    default:
      return false;
  }
}

static void make_bool_value(const w_seed_constir_node *node, bool value,
                            w_seed_constir_value *out) {
  (void)memset(out, 0, sizeof(*out));
  out->kind = W_SEED_CONSTIR_VALUE_BOOL;
  out->type_index = node->type_index;
  out->type_kind = W_SEED_FRONTEND_TYPE_BOOL;
  out->bool_value = value;
}

static bool eval_node_at(constir_eval_context *context,
                         const w_seed_constir_function *function,
                         uint32_t node_index, size_t depth,
                         w_seed_constir_value *value) {
  if (context == NULL || function == NULL || value == NULL ||
      context->program == NULL || depth == 0u ||
      depth > W_SEED_CONSTIR_MAX_EVAL_DEPTH ||
      (size_t)node_index >= context->program->node_count ||
      node_index < function->first_node ||
      (size_t)node_index >= (size_t)function->first_node + function->node_count)
    return false;
  const w_seed_constir_node *node = &context->program->nodes[node_index];
  if (!eval_step(context, node->source_span)) return false;
  switch (node->kind) {
    case W_SEED_CONSTIR_NODE_BOOL:
      (void)memset(value, 0, sizeof(*value));
      value->kind = W_SEED_CONSTIR_VALUE_BOOL;
      value->type_index = node->type_index;
      value->type_kind = node->type_kind;
      value->bool_value = node->bool_value;
      return true;
    case W_SEED_CONSTIR_NODE_INTEGER:
      (void)memset(value, 0, sizeof(*value));
      value->kind = W_SEED_CONSTIR_VALUE_INTEGER;
      value->type_index = node->type_index;
      value->type_kind = node->type_kind;
      value->type_is_signed = node->type_is_signed;
      value->type_bit_width = node->type_bit_width;
      (void)memcpy(value->integer_value, node->integer_value,
                   sizeof(value->integer_value));
      return true;
    case W_SEED_CONSTIR_NODE_STRING:
      if (node->type_kind != W_SEED_FRONTEND_TYPE_STRING ||
          !program_string_slice_valid(context->program,
                                       node->const_byte_offset,
                                       node->const_byte_count))
        return false;
      (void)memset(value, 0, sizeof(*value));
      value->kind = W_SEED_CONSTIR_VALUE_STRING;
      value->type_index = node->type_index;
      value->type_kind = node->type_kind;
      value->string_bytes = node->const_byte_count == 0u
                                ? NULL
                                : context->program->frontend_output->const_bytes +
                                      node->const_byte_offset;
      value->string_count = node->const_byte_count;
      return true;
    case W_SEED_CONSTIR_NODE_ENUM_CASE:
      (void)memset(value, 0, sizeof(*value));
      value->kind = W_SEED_CONSTIR_VALUE_ENUM;
      value->type_index = node->type_index;
      value->type_kind = node->type_kind;
      value->enum_base_index = node->enum_base_index;
      value->enum_case_index = node->enum_case_index;
      return true;
    case W_SEED_CONSTIR_NODE_PARAMETER: {
      if (node->parameter_ordinal >= context->argument_count ||
          !validate_value_against_parameter(
              &context->arguments[node->parameter_ordinal],
              function_parameter_for_ordinal(context, function,
                                              node->parameter_ordinal),
              context->program))
        return false;
      *value = context->arguments[node->parameter_ordinal];
      return true;
    }
    case W_SEED_CONSTIR_NODE_LOCAL:
      if (context->active_frame == NULL ||
          node->local_ordinal >= W_SEED_CONSTIR_MAX_PARAMETERS ||
          context->active_frame->locals[node->local_ordinal].kind ==
              W_SEED_CONSTIR_VALUE_INVALID)
        return false;
      *value = context->active_frame->locals[node->local_ordinal];
      if (value->kind == W_SEED_CONSTIR_VALUE_STRING &&
          !constir_string_value_valid(value, context->program))
        return false;
      return true;
    case W_SEED_CONSTIR_NODE_STATIC_LIST_COUNT: {
      w_seed_constir_value list;
      if (!eval_node_at(context, function, node->left, depth + 1u, &list) ||
          list.kind != W_SEED_CONSTIR_VALUE_STATIC_LIST) return false;
      (void)memset(value, 0, sizeof(*value));
      value->kind = W_SEED_CONSTIR_VALUE_INTEGER;
      value->type_index = node->type_index;
      value->type_kind = node->type_kind;
      value->type_is_signed = node->type_is_signed;
      value->type_bit_width = node->type_bit_width;
      size_t count = list.element_count;
      for (size_t byte = 0u; byte < sizeof(count) && byte < sizeof(value->integer_value);
           byte += 1u)
        value->integer_value[byte] = (uint8_t)(count >> (byte * 8u));
      return true;
    }
    case W_SEED_CONSTIR_NODE_STATIC_LIST_INDEX: {
      w_seed_constir_value list;
      w_seed_constir_value index_value;
      if (!eval_node_at(context, function, node->left, depth + 1u, &list) ||
          !eval_node_at(context, function, node->right, depth + 1u,
                        &index_value) ||
          list.kind != W_SEED_CONSTIR_VALUE_STATIC_LIST ||
          index_value.kind != W_SEED_CONSTIR_VALUE_INTEGER) return false;
      size_t index = 0u;
      if (!integer_shift_count(&index_value, &index) ||
          index >= list.element_count || list.elements == NULL)
        return eval_fail(context, W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006,
                         node->source_span, 0u);
      *value = list.elements[index];
      return true;
    }
    case W_SEED_CONSTIR_NODE_UNARY: {
      w_seed_constir_value operand;
      if (!eval_node_at(context, function, node->left, depth + 1u, &operand)) return false;
      if (node->normalized_operator == W_SEED_CONSTIR_OPERATOR_NOT &&
          operand.kind == W_SEED_CONSTIR_VALUE_BOOL) {
        make_bool_value(node, !operand.bool_value, value);
        return true;
      }
      w_seed_constir_diagnostic_code fault = W_SEED_CONSTIR_DIAGNOSTIC_NONE;
      if (!eval_integer_unary(node, &operand, value)) {
        if (fault == W_SEED_CONSTIR_DIAGNOSTIC_NONE)
          fault = W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006;
        return eval_fail(context, fault, node->source_span, 0u);
      }
      return true;
    }
    case W_SEED_CONSTIR_NODE_BINARY: {
      w_seed_constir_value left;
      if (!eval_node_at(context, function, node->left, depth + 1u, &left)) return false;
      if (node->normalized_operator == W_SEED_CONSTIR_OPERATOR_AND ||
          node->normalized_operator == W_SEED_CONSTIR_OPERATOR_OR) {
        if (left.kind != W_SEED_CONSTIR_VALUE_BOOL) return false;
        if ((node->normalized_operator == W_SEED_CONSTIR_OPERATOR_AND &&
             !left.bool_value) ||
            (node->normalized_operator == W_SEED_CONSTIR_OPERATOR_OR &&
             left.bool_value)) {
          make_bool_value(node,
                          node->normalized_operator == W_SEED_CONSTIR_OPERATOR_OR,
                          value);
          return true;
        }
        w_seed_constir_value right;
        if (!eval_node_at(context, function, node->right, depth + 1u, &right) ||
            right.kind != W_SEED_CONSTIR_VALUE_BOOL) return false;
        make_bool_value(node, right.bool_value, value);
        return true;
      }
      if (node->normalized_operator == W_SEED_CONSTIR_OPERATOR_EQUAL ||
          node->normalized_operator == W_SEED_CONSTIR_OPERATOR_NOT_EQUAL ||
          node->normalized_operator == W_SEED_CONSTIR_OPERATOR_LESS ||
          node->normalized_operator == W_SEED_CONSTIR_OPERATOR_LESS_EQUAL ||
          node->normalized_operator == W_SEED_CONSTIR_OPERATOR_GREATER ||
          node->normalized_operator == W_SEED_CONSTIR_OPERATOR_GREATER_EQUAL) {
        w_seed_constir_value right;
        bool comparison = false;
        if (!eval_node_at(context, function, node->right, depth + 1u, &right) ||
            !eval_comparison(node, &left, &right, &comparison)) return false;
        make_bool_value(node, comparison, value);
        return true;
      }
      w_seed_constir_value right;
      if (!eval_node_at(context, function, node->right, depth + 1u, &right)) return false;
      w_seed_constir_diagnostic_code fault = W_SEED_CONSTIR_DIAGNOSTIC_NONE;
      if (!eval_integer_binary(node, &left, &right, value, &fault))
        return eval_fail(context,
                         fault == W_SEED_CONSTIR_DIAGNOSTIC_NONE
                             ? W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006
                             : fault,
                         node->source_span, 0u);
      return true;
    }
    case W_SEED_CONSTIR_NODE_MEMBERSHIP: {
      w_seed_constir_value subject;
      if (!eval_node_at(context, function, node->left, depth + 1u, &subject) ||
          subject.kind != W_SEED_CONSTIR_VALUE_ENUM) return false;
      bool matched = false;
      for (uint32_t offset = 0; offset < node->membership_case_count; offset += 1) {
        const w_seed_constir_membership_case *item = &context->program->membership_cases[
            node->first_membership_case + offset];
        if (item->enum_base_index == subject.enum_base_index &&
            item->enum_case_index == subject.enum_case_index) {
          matched = true;
          break;
        }
      }
      make_bool_value(node, matched, value);
      return true;
    }
    case W_SEED_CONSTIR_NODE_SWITCH: {
      w_seed_constir_value subject;
      if (!eval_node_at(context, function, node->left, depth + 1u, &subject) ||
          subject.kind != W_SEED_CONSTIR_VALUE_ENUM) return false;
      const w_seed_constir_switch_arm *selected = NULL;
      for (uint32_t offset = 0; offset < node->switch_arm_count; offset += 1) {
        const w_seed_constir_switch_arm *arm = &context->program->switch_arms[
            node->first_switch_arm + offset];
        if (arm->pattern_kind == W_SEED_FRONTEND_SWITCH_PATTERN_WILDCARD ||
            (arm->enum_base_index == subject.enum_base_index &&
             arm->enum_case_index == subject.enum_case_index)) {
          selected = arm;
          break;
        }
      }
      if (selected == NULL) return false;
      return eval_node_at(context, function, selected->result_node, depth + 1u,
                          value);
    }
    case W_SEED_CONSTIR_NODE_CALL: {
      size_t target_index = 0;
      constir_const_memo_entry *memo_entry = NULL;
      const bool memoized_const_call =
          node->call_target_const_declaration != W_SEED_CONSTIR_NONE;
      if (node->call_target_function != W_SEED_CONSTIR_NONE &&
          node->call_target_const_declaration != W_SEED_CONSTIR_NONE)
        return false;
      const w_seed_constir_function *target =
          node->call_target_function != W_SEED_CONSTIR_NONE
              ? program_function_for_frontend(context,
                                              node->call_target_function,
                                              &target_index)
              : program_function_for_const(context,
                                           node->call_target_const_declaration,
                                           &target_index);
      if (target == NULL || !target->lowerable ||
          (node->call_target_function == W_SEED_CONSTIR_NONE &&
           node->call_target_const_declaration == W_SEED_CONSTIR_NONE) ||
          node->call_argument_count != target->parameter_count)
        return false;
      /* D5 keys only a zero-argument module-const declaration identity.  D4
       * lowers every local module const to this shape; a different shape is
       * invalid rather than silently using an unsound key. */
      if (memoized_const_call &&
          (target->parameter_count != 0u || node->call_argument_count != 0u))
        return false;
      if (memoized_const_call) {
        memo_entry = const_memo_find(
            context, node->call_target_const_declaration);
        if (memo_entry != NULL &&
            memo_entry->state == CONSTIR_CONST_MEMO_READY) {
          if (!const_memo_counter_increment(&context->result->const_cache_hits))
            return false;
          *value = memo_entry->value;
          return true;
        }
        if (memo_entry != NULL &&
            memo_entry->state == CONSTIR_CONST_MEMO_ACTIVE)
          return eval_fail(context, W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002,
                           node->source_span, 0u);
        memo_entry = const_memo_add(context,
                                    node->call_target_const_declaration);
        if (memo_entry == NULL ||
            !const_memo_counter_increment(
                &context->result->const_cache_misses)) {
          if (memo_entry != NULL) memo_entry->state = CONSTIR_CONST_MEMO_EMPTY;
          return false;
        }
      }
      if (context->current_depth == 0u ||
          context->current_depth >= W_SEED_CONSTIR_MAX_CALL_DEPTH ||
          (context->quota.call_depth != SIZE_MAX &&
           context->current_depth >= context->quota.call_depth)) {
        const size_t limit = context->quota.call_depth == SIZE_MAX
                                 ? (size_t)W_SEED_CONSTIR_MAX_CALL_DEPTH
                                 : context->quota.call_depth;
        return eval_fail(context, W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003,
                         node->source_span, limit);
      }
      /* The active frame at depth N owns the caller's locals.  Reserve the
       * next slot for the callee so evaluating call arguments cannot clear or
       * overwrite the caller frame. */
      if (context->workspace == NULL || context->workspace->frames == NULL ||
          context->current_depth >= context->workspace->frame_capacity)
        return false;
      w_seed_constir_eval_frame *frame =
          &context->workspace->frames[context->current_depth];
      (void)memset(frame, 0, sizeof(*frame));
      for (uint32_t offset = 0; offset < node->call_argument_count; offset += 1) {
        const w_seed_constir_call_argument *argument = &context->program->call_arguments[
            node->first_call_argument + offset];
        if (argument->owner_node != node_index ||
            argument->parameter_ordinal >= target->parameter_count ||
            !eval_node_at(context, function, argument->node_index, depth + 1u,
                          &frame->values[argument->parameter_ordinal])) return false;
      }
      context->current_depth += 1u;
      if (context->current_depth > context->peak_depth)
        context->peak_depth = context->current_depth;
      const w_seed_constir_value *saved_arguments = context->arguments;
      const size_t saved_argument_count = context->argument_count;
      context->arguments = frame->values;
      context->argument_count = target->parameter_count;
      w_seed_constir_eval_frame *saved_frame = context->active_frame;
      context->active_frame = frame;
      const bool success = eval_function(context, target_index, frame->values,
                                         target->parameter_count,
                                         depth + 1u, value);
      context->arguments = saved_arguments;
      context->argument_count = saved_argument_count;
      context->active_frame = saved_frame;
      context->current_depth -= 1u;
      if (memo_entry != NULL) {
        size_t encoded_bytes = 0u;
        if (success && result_encoded_bytes(value, &encoded_bytes)) {
          memo_entry->value = *value;
          memo_entry->state = CONSTIR_CONST_MEMO_READY;
        } else {
          /* A failed, panicking, quota-limited, or invalid body is never
           * reusable.  Keep the slot empty for the remainder of this failed
           * invocation and start from an empty table on the next one. */
          memo_entry->state = CONSTIR_CONST_MEMO_EMPTY;
        }
      }
      return success;
    }
    default:
      return false;
  }
}

static bool eval_statement_chain(constir_eval_context *context,
                                 const w_seed_constir_function *function,
                                 uint32_t statement_index, size_t depth,
                                 w_seed_constir_value *value, bool *returned) {
  if (context == NULL || function == NULL || value == NULL || returned == NULL ||
      context->program == NULL || depth == 0u ||
      depth > W_SEED_CONSTIR_MAX_EVAL_DEPTH ||
      context->program->statements == NULL) return false;
  *returned = false;
  uint32_t current = statement_index;
  size_t guard = 0u;
  while (current != W_SEED_CONSTIR_NONE &&
         guard <= context->program->statement_count) {
    if ((size_t)current >= context->program->statement_count) return false;
    const w_seed_constir_statement *statement =
        &context->program->statements[current];
    if (statement->owner_function != function->frontend_function ||
        !eval_step(context, statement->source_span)) return false;
    bool child_returned = false;
    switch (statement->kind) {
      case W_SEED_CONSTIR_STATEMENT_RETURN:
        if (statement->expression_node == W_SEED_CONSTIR_NONE ||
            !eval_node_at(context, function, statement->expression_node,
                          depth + 1u, value)) return false;
        *returned = true;
        return true;
      case W_SEED_CONSTIR_STATEMENT_GUARD: {
        w_seed_constir_value condition;
        if (!eval_node_at(context, function, statement->condition_node,
                          depth + 1u, &condition) ||
            condition.kind != W_SEED_CONSTIR_VALUE_BOOL) return false;
        if (!condition.bool_value) {
          if (statement->else_child == W_SEED_CONSTIR_NONE ||
              !eval_statement_chain(context, function, statement->else_child,
                                    depth + 1u, value, &child_returned))
            return false;
          if (child_returned) {
            *returned = true;
            return true;
          }
          /* A false guard must leave the function.  A normalized else branch
           * that falls through would make control flow depend on an invalid
           * edge, so reject it before executing the sibling chain. */
          return false;
        }
        break;
      }
      case W_SEED_CONSTIR_STATEMENT_IF: {
        w_seed_constir_value condition;
        if (!eval_node_at(context, function, statement->condition_node,
                          depth + 1u, &condition) ||
            condition.kind != W_SEED_CONSTIR_VALUE_BOOL) return false;
        if (condition.bool_value && statement->first_child != W_SEED_CONSTIR_NONE) {
          if (!eval_statement_chain(context, function, statement->first_child,
                                    depth + 1u, value, &child_returned))
            return false;
          if (child_returned) {
            *returned = true;
            return true;
          }
        } else if (!condition.bool_value &&
                   statement->else_child != W_SEED_CONSTIR_NONE) {
          if (!eval_statement_chain(context, function, statement->else_child,
                                    depth + 1u, value, &child_returned))
            return false;
          if (child_returned) {
            *returned = true;
            return true;
          }
        }
        break;
      }
      case W_SEED_CONSTIR_STATEMENT_FOR_RANGE: {
        w_seed_constir_value lower;
        w_seed_constir_value upper;
        if (statement->lower_node == W_SEED_CONSTIR_NONE ||
            statement->upper_node == W_SEED_CONSTIR_NONE ||
            statement->local_ordinal >= W_SEED_CONSTIR_MAX_PARAMETERS ||
            !eval_node_at(context, function, statement->lower_node, depth + 1u,
                          &lower) ||
            !eval_node_at(context, function, statement->upper_node, depth + 1u,
                          &upper) ||
            lower.kind != W_SEED_CONSTIR_VALUE_INTEGER ||
            upper.kind != W_SEED_CONSTIR_VALUE_INTEGER ||
            lower.type_is_signed || upper.type_is_signed ||
            lower.type_bit_width != upper.type_bit_width)
          return false;
        size_t cursor = 0u;
        size_t limit = 0u;
        if (!integer_shift_count(&lower, &cursor) ||
            !integer_shift_count(&upper, &limit)) return false;
        if (context->active_frame == NULL) return false;
        while (cursor < limit) {
          w_seed_constir_value local = lower;
          (void)memset(local.integer_value, 0, sizeof(local.integer_value));
          for (size_t byte = 0u; byte < sizeof(cursor) &&
                               byte < sizeof(local.integer_value); byte += 1u)
            local.integer_value[byte] = (uint8_t)(cursor >> (byte * 8u));
          context->active_frame->locals[statement->local_ordinal] = local;
          if (statement->first_child != W_SEED_CONSTIR_NONE) {
            if (!eval_statement_chain(context, function, statement->first_child,
                                      depth + 1u, value, &child_returned))
              return false;
            if (child_returned) {
              *returned = true;
              return true;
            }
          }
          cursor += 1u;
          if (cursor == 0u && limit != 0u)
            return eval_fail(context, W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006,
                             statement->source_span, 0u);
          if (!eval_step(context, statement->source_span)) return false;
        }
        break;
      }
      default:
        return false;
    }
    current = statement->next_sibling;
    guard += 1u;
  }
  return current == W_SEED_CONSTIR_NONE;
}

static bool eval_function(constir_eval_context *context,
                          size_t function_index,
                          const w_seed_constir_value *arguments,
                          size_t argument_count, size_t depth,
                          w_seed_constir_value *value) {
  if (context == NULL || context->program == NULL || value == NULL ||
      function_index >= context->program->function_count) return false;
  const w_seed_constir_function *function = &context->program->functions[function_index];
  if (!function->lowerable || argument_count != function->parameter_count) return false;
  for (uint32_t ordinal = 0; ordinal < function->parameter_count; ordinal += 1) {
    const w_seed_constir_parameter *parameter =
        function_parameter_for_ordinal(context, function, ordinal);
    if (!validate_value_against_parameter(&arguments[ordinal], parameter,
                                          context->program)) {
      return false;
    }
  }
  (void)depth;
  if (function->root_statement != W_SEED_CONSTIR_NONE) {
    bool returned = false;
    if (!eval_statement_chain(context, function, function->root_statement,
                              depth, value, &returned) || !returned)
      return false;
    return true;
  }
  return eval_node_at(context, function, function->root_node, depth, value);
}

void w_seed_constir_session_init(w_seed_constir_session *session) {
  if (session != NULL) (void)memset(session, 0, sizeof(*session));
}

w_seed_constir_status w_seed_constir_evaluate_in_session(
    const w_seed_constir_program *program, uint32_t function_index,
    const w_seed_constir_value *arguments, size_t argument_count,
    w_seed_constir_quota quota, w_seed_constir_eval_workspace *workspace,
    w_seed_constir_session *session, w_seed_constir_value *value,
    w_seed_constir_eval_result *result) {
  if (result != NULL) (void)memset(result, 0, sizeof(*result));
  if (value != NULL) (void)memset(value, 0, sizeof(*value));
  if (result == NULL || value == NULL || session == NULL ||
      !validate_program(program) ||
      function_index >= program->function_count ||
      (argument_count != 0u && arguments == NULL)) {
    if (result != NULL) result->status = W_SEED_CONSTIR_INVALID;
    return W_SEED_CONSTIR_INVALID;
  }
  const w_seed_constir_function *function = &program->functions[function_index];
  if (!function->lowerable || argument_count != function->parameter_count) {
    result->status = W_SEED_CONSTIR_INVALID;
    return W_SEED_CONSTIR_INVALID;
  }
  if (function->root_statement != W_SEED_CONSTIR_NONE &&
      function->local_count != 0u &&
      (workspace == NULL || workspace->frames == NULL ||
       workspace->frame_capacity == 0u)) {
    result->status = W_SEED_CONSTIR_INVALID;
    return W_SEED_CONSTIR_INVALID;
  }
  if (quota.call_depth != SIZE_MAX &&
      quota.call_depth > W_SEED_CONSTIR_MAX_CALL_DEPTH) {
    result->status = W_SEED_CONSTIR_INVALID;
    return W_SEED_CONSTIR_INVALID;
  }
  constir_eval_context context;
  (void)memset(&context, 0, sizeof(context));
  context.program = program;
  context.arguments = arguments;
  context.argument_count = argument_count;
  context.quota = quota;
  context.workspace = workspace;
  context.result = result;
  context.session = session;
  if (workspace != NULL && workspace->frames != NULL &&
      workspace->frame_capacity != 0u) {
    context.active_frame = &workspace->frames[0];
    (void)memset(context.active_frame, 0, sizeof(*context.active_frame));
    for (size_t index = 0u; index < argument_count &&
                             index < W_SEED_CONSTIR_MAX_PARAMETERS; index += 1u)
      context.active_frame->values[index] = arguments[index];
  }
  context.current_depth = 1u;
  context.peak_depth = 1u;
  if (quota.call_depth == 0u) {
    (void)eval_fail(&context, W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003,
                    function->body_span, quota.call_depth);
    result->status = W_SEED_CONSTIR_OK;
    result->consumed_call_depth = context.peak_depth;
    return W_SEED_CONSTIR_OK;
  }
  if (!eval_function(&context, function_index, arguments, argument_count, 1u,
                     value)) {
    (void)memset(value, 0, sizeof(*value));
    result->status = context.runtime_failed ? W_SEED_CONSTIR_OK
                                             : W_SEED_CONSTIR_INVALID;
    result->consumed_steps = context.steps;
    result->consumed_heap_bytes = 0u;
    result->consumed_call_depth = context.peak_depth;
    return result->status;
  }
  size_t encoded_bytes = 0u;
  if (!result_encoded_bytes(value, &encoded_bytes)) {
    (void)memset(value, 0, sizeof(*value));
    result->status = W_SEED_CONSTIR_INVALID;
    result->consumed_steps = context.steps;
    result->consumed_heap_bytes = 0u;
    result->consumed_call_depth = context.peak_depth;
    return W_SEED_CONSTIR_INVALID;
  }
  if (quota.result_bytes != SIZE_MAX && encoded_bytes > quota.result_bytes) {
    (void)eval_fail(&context, W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003,
                    function->body_span, quota.result_bytes);
    (void)memset(value, 0, sizeof(*value));
    result->consumed_result_bytes = encoded_bytes;
    result->status = W_SEED_CONSTIR_OK;
    result->consumed_steps = context.steps;
    result->consumed_heap_bytes = 0u;
    result->consumed_call_depth = context.peak_depth;
    return W_SEED_CONSTIR_OK;
  }
  result->status = W_SEED_CONSTIR_OK;
  result->consumed_steps = context.steps;
  result->consumed_heap_bytes = 0u;
  result->consumed_call_depth = context.peak_depth;
  result->consumed_result_bytes = encoded_bytes;
  return W_SEED_CONSTIR_OK;
}

w_seed_constir_status w_seed_constir_evaluate(
    const w_seed_constir_program *program, uint32_t function_index,
    const w_seed_constir_value *arguments, size_t argument_count,
    w_seed_constir_quota quota, w_seed_constir_eval_workspace *workspace,
    w_seed_constir_value *value, w_seed_constir_eval_result *result) {
  w_seed_constir_session session;
  w_seed_constir_session_init(&session);
  return w_seed_constir_evaluate_in_session(
      program, function_index, arguments, argument_count, quota, workspace,
      &session, value, result);
}

bool w_seed_constir_value_bool(uint32_t type_index, bool value,
                               w_seed_constir_value *out) {
  if (out == NULL || type_index == W_SEED_CONSTIR_NONE) return false;
  (void)memset(out, 0, sizeof(*out));
  out->kind = W_SEED_CONSTIR_VALUE_BOOL;
  out->type_index = type_index;
  out->type_kind = W_SEED_FRONTEND_TYPE_BOOL;
  out->bool_value = value;
  return true;
}

bool w_seed_constir_value_integer(
    uint32_t type_index, w_seed_frontend_type_kind type_kind, bool is_signed,
    uint16_t bit_width,
    const uint8_t bytes[W_SEED_CONSTIR_INTEGER_BYTES],
    w_seed_constir_value *out) {
  if (out == NULL || bytes == NULL || type_index == W_SEED_CONSTIR_NONE ||
      type_kind != W_SEED_FRONTEND_TYPE_INTEGER || bit_width == 0u ||
      bit_width > 128u) return false;
  constir_bits bits;
  (void)memcpy(bits.bytes, bytes, sizeof(bits.bytes));
  if (!bits_fit(bits, is_signed, bit_width)) return false;
  (void)memset(out, 0, sizeof(*out));
  out->kind = W_SEED_CONSTIR_VALUE_INTEGER;
  out->type_index = type_index;
  out->type_kind = type_kind;
  out->type_is_signed = is_signed;
  out->type_bit_width = bit_width;
  (void)memcpy(out->integer_value, bytes, sizeof(out->integer_value));
  return true;
}

bool w_seed_constir_value_enum(uint32_t type_index, uint32_t enum_base_index,
                               uint32_t enum_case_index,
                               w_seed_constir_value *out) {
  if (out == NULL || type_index == W_SEED_CONSTIR_NONE ||
      enum_base_index == W_SEED_CONSTIR_NONE ||
      enum_case_index == W_SEED_CONSTIR_NONE) return false;
  (void)memset(out, 0, sizeof(*out));
  out->kind = W_SEED_CONSTIR_VALUE_ENUM;
  out->type_index = type_index;
  out->type_kind = W_SEED_FRONTEND_TYPE_ENUM;
  out->enum_base_index = enum_base_index;
  out->enum_case_index = enum_case_index;
  return true;
}

bool w_seed_constir_value_static_list(
    uint32_t type_index, uint32_t element_type_index,
    const w_seed_constir_value *elements, size_t element_count,
    w_seed_constir_value *out) {
  if (out == NULL || type_index == W_SEED_CONSTIR_NONE ||
      element_type_index == W_SEED_CONSTIR_NONE ||
      (elements == NULL && element_count != 0u) ||
      element_count > W_SEED_CONSTIR_MAX_STATIC_LIST_ELEMENTS) return false;
  (void)memset(out, 0, sizeof(*out));
  out->kind = W_SEED_CONSTIR_VALUE_STATIC_LIST;
  out->type_index = type_index;
  out->type_kind = W_SEED_FRONTEND_TYPE_STATIC_LIST;
  out->element_type_index = element_type_index;
  out->elements = elements;
  out->element_count = element_count;
  return true;
}

bool w_seed_constir_value_string(uint32_t type_index, const uint8_t *bytes,
                                 size_t count, w_seed_constir_value *out) {
  if (out == NULL || type_index == W_SEED_CONSTIR_NONE ||
      (bytes == NULL && count != 0u) ||
      count > W_SEED_CONSTIR_MAX_STRING_BYTES)
    return false;
  (void)memset(out, 0, sizeof(*out));
  out->kind = W_SEED_CONSTIR_VALUE_STRING;
  out->type_index = type_index;
  out->type_kind = W_SEED_FRONTEND_TYPE_STRING;
  out->string_bytes = bytes;
  out->string_count = count;
  return true;
}




static bool lower_expression(constir_lower_context *context,
                             uint32_t function_index, uint32_t expression_index,
                             uint32_t *ir_index, size_t depth) {
  if (context == NULL || ir_index == NULL || depth > W_SEED_FRONTEND_MAX_NESTING) return false;
  const w_seed_frontend_expression *expression =
      frontend_expression_at(context, expression_index);
  if (expression == NULL || expression->owner_function != function_index ||
      !expression->supported || !span_valid(expression->span)) {
    if (expression != NULL) mark_failure(context, expression->span, expression_index);
    return false;
  }
  if (expression->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS) {
    if (expression->left == W_SEED_FRONTEND_NONE) {
      mark_failure(context, expression->span, expression_index);
      return false;
    }
    return lower_expression(context, function_index, expression->left, ir_index,
                            depth + 1u);
  }
  w_seed_constir_node node;
  (void)memset(&node, 0, sizeof(node));
  node.kind = W_SEED_CONSTIR_NODE_INVALID;
  node.owner_function = function_index;
  node.frontend_expression = expression_index;
  node.type_index = expression->inferred_type;
  node.left = W_SEED_CONSTIR_NONE;
  node.right = W_SEED_CONSTIR_NONE;
  node.parameter_ordinal = W_SEED_CONSTIR_NONE;
  node.call_target_function = W_SEED_CONSTIR_NONE;
  node.call_target_const_declaration = W_SEED_CONSTIR_NONE;
  node.first_call_argument = W_SEED_CONSTIR_NONE;
  node.first_switch_arm = W_SEED_CONSTIR_NONE;
  node.first_membership_case = W_SEED_CONSTIR_NONE;
  node.element_type_index = W_SEED_CONSTIR_NONE;
  node.local_ordinal = W_SEED_CONSTIR_NONE;
  node.normalized_operator = W_SEED_CONSTIR_OPERATOR_INVALID;
  node.const_byte_offset = W_SEED_CONSTIR_NONE;
  node.const_byte_count = 0u;
  node.enum_base_index = expression->enum_index;
  node.enum_case_index = expression->enum_case_index;
  node.source_span = expression->span;
  if (!type_metadata(context, expression->inferred_type, &node.type_kind,
                     &node.type_is_signed, &node.type_bit_width,
                     &node.enum_base_index)) {
    mark_failure(context, expression->span, expression_index);
    return false;
  }
  switch (expression->kind) {
    case W_SEED_FRONTEND_EXPR_BOOL: {
      if (!expression->has_bool_value) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      node.kind = W_SEED_CONSTIR_NODE_BOOL;
      node.bool_value = expression->bool_value;
      break;
    }
    case W_SEED_FRONTEND_EXPR_INTEGER: {
      constir_bits value;
      if (!integer_value_from_expression(context, expression, &value)) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      node.kind = W_SEED_CONSTIR_NODE_INTEGER;
      (void)memcpy(node.integer_value, value.bytes, sizeof(node.integer_value));
      break;
    }
    case W_SEED_FRONTEND_EXPR_STRING:
      if (node.type_kind != W_SEED_FRONTEND_TYPE_STRING ||
          !frontend_string_slice_valid(context, expression->const_byte_offset,
                                       expression->const_byte_count)) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      node.kind = W_SEED_CONSTIR_NODE_STRING;
      node.const_byte_offset = expression->const_byte_offset;
      node.const_byte_count = expression->const_byte_count;
      break;
    case W_SEED_FRONTEND_EXPR_ENUM_CASE:
      if (node.type_kind != W_SEED_FRONTEND_TYPE_ENUM &&
          node.type_kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      node.kind = W_SEED_CONSTIR_NODE_ENUM_CASE;
      break;
    case W_SEED_FRONTEND_EXPR_IDENTIFIER: {
      if (expression->resolved_local_ordinal != W_SEED_FRONTEND_NONE) {
        node.kind = W_SEED_CONSTIR_NODE_LOCAL;
        node.local_ordinal = expression->resolved_local_ordinal;
        break;
      }
      if (expression->resolved_const_declaration != W_SEED_FRONTEND_NONE) {
        const uint32_t target = expression->resolved_const_declaration;
        const w_seed_frontend_const_declaration *declaration =
            frontend_const_declaration_at(context, target);
        if (declaration == NULL || !declaration->lowerable) {
          mark_failure(context, expression->span, expression_index);
          return false;
        }
        node.kind = W_SEED_CONSTIR_NODE_CALL;
        node.call_target_const_declaration = target;
        break;
      }
      const uint32_t ordinal = expression->resolved_parameter_ordinal;
      if (ordinal == W_SEED_FRONTEND_NONE) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      node.kind = W_SEED_CONSTIR_NODE_PARAMETER;
      node.parameter_ordinal = ordinal;
      break;
    }
    case W_SEED_FRONTEND_EXPR_MEMBER: {
      if (expression->left == W_SEED_FRONTEND_NONE ||
          expression->member_name.length == 0u ||
          !text_is(expression->member_name, "count") ||
          !lower_expression(context, function_index, expression->left,
                            &node.left, depth + 1u) ||
          node.type_kind != W_SEED_FRONTEND_TYPE_INTEGER) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      node.kind = W_SEED_CONSTIR_NODE_STATIC_LIST_COUNT;
      break;
    }
    case W_SEED_FRONTEND_EXPR_INDEX: {
      if (expression->left == W_SEED_FRONTEND_NONE ||
          expression->right == W_SEED_FRONTEND_NONE ||
          !lower_expression(context, function_index, expression->left,
                            &node.left, depth + 1u) ||
          !lower_expression(context, function_index, expression->right,
                            &node.right, depth + 1u) ||
          node.type_kind == W_SEED_FRONTEND_TYPE_UNKNOWN) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      const w_seed_frontend_type *receiver =
          frontend_type_at(context,
                           frontend_expression_at(context, expression->left)
                               ->inferred_type);
      if (receiver == NULL ||
          receiver->kind != W_SEED_FRONTEND_TYPE_STATIC_LIST ||
          receiver->element_type == W_SEED_FRONTEND_NONE) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      node.kind = W_SEED_CONSTIR_NODE_STATIC_LIST_INDEX;
      node.element_type_index = receiver->element_type;
      break;
    }
    case W_SEED_FRONTEND_EXPR_RANGE:
      mark_failure(context, expression->span, expression_index);
      return false;
    case W_SEED_FRONTEND_EXPR_UNARY:
      node.normalized_operator =
          unary_operator_for_text(expression->operator_text);
      if (!operator_is_supported(node.normalized_operator, node.type_kind) ||
          !lower_expression(context, function_index, expression->left,
                            &node.left, depth + 1u)) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      node.kind = W_SEED_CONSTIR_NODE_UNARY;
      break;
    case W_SEED_FRONTEND_EXPR_BINARY:
      node.normalized_operator = operator_for_text(expression->operator_text);
      if (node.normalized_operator == W_SEED_CONSTIR_OPERATOR_INVALID ||
          !binary_operator_supported(context, expression_index,
                                     node.normalized_operator,
                                     node.type_kind) ||
          !lower_expression(context, function_index, expression->left,
                            &node.left, depth + 1u) ||
          !lower_expression(context, function_index, expression->right,
                            &node.right, depth + 1u)) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      node.kind = W_SEED_CONSTIR_NODE_BINARY;
      break;
    case W_SEED_FRONTEND_EXPR_CALL: {
      const w_seed_frontend_expression *callee =
          frontend_expression_at(context, expression->left);
      if (callee == NULL || callee->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      const uint32_t target = callee->resolved_function_index;
      if (target == W_SEED_FRONTEND_NONE || !function_is_const(context, target) ||
          !function_base_supported(context, target)) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      node.kind = W_SEED_CONSTIR_NODE_CALL;
      node.call_target_function = target;
      if (!lower_call_arguments(context, function_index, expression,
                                &node.first_call_argument,
                                &node.call_argument_count, depth)) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      break;
    }
    case W_SEED_FRONTEND_EXPR_ENUM_MEMBERSHIP: {
      if (expression->left == W_SEED_FRONTEND_NONE ||
          expression->membership_case_count == 0u ||
          expression->first_membership_case == W_SEED_FRONTEND_NONE ||
          !lower_expression(context, function_index, expression->left,
                            &node.left, depth + 1u)) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      node.kind = W_SEED_CONSTIR_NODE_MEMBERSHIP;
      node.first_membership_case = W_SEED_CONSTIR_NONE;
      node.membership_case_count = expression->membership_case_count;
      for (uint32_t offset = 0; offset < expression->membership_case_count;
           offset += 1) {
        const w_seed_frontend_enum_membership_case *item = frontend_membership_at(
            context, expression->first_membership_case + offset);
        if (item == NULL || item->owner_expression != expression_index) {
          mark_failure(context, expression->span, expression_index);
          return false;
        }
        w_seed_constir_membership_case ir_item;
        ir_item.owner_node = W_SEED_CONSTIR_NONE;
        ir_item.enum_base_index = item->enum_base_index;
        ir_item.enum_case_index = item->enum_case_index;
        ir_item.source_span = item->source_span;
        uint32_t item_index = W_SEED_CONSTIR_NONE;
        if (!append_membership_case(context, &ir_item, &item_index)) return false;
        if (offset == 0u) node.first_membership_case = item_index;
      }
      break;
    }
    case W_SEED_FRONTEND_EXPR_SWITCH: {
      if (expression->left == W_SEED_FRONTEND_NONE ||
          expression->first_switch_arm == W_SEED_FRONTEND_NONE ||
          expression->switch_arm_count == 0u ||
          !lower_expression(context, function_index, expression->left,
                            &node.left, depth + 1u)) {
        mark_failure(context, expression->span, expression_index);
        return false;
      }
      node.kind = W_SEED_CONSTIR_NODE_SWITCH;
      node.first_switch_arm = W_SEED_CONSTIR_NONE;
      node.switch_arm_count = expression->switch_arm_count;
      for (uint32_t offset = 0; offset < expression->switch_arm_count; offset += 1) {
        const w_seed_frontend_switch_arm *arm = frontend_switch_arm_at(
            context, expression->first_switch_arm + offset);
        if (arm == NULL || arm->owner_expression != expression_index ||
            !arm->supported || arm->result_expression == W_SEED_FRONTEND_NONE) {
          mark_failure(context, expression->span, expression_index);
          return false;
        }
        uint32_t result_node = W_SEED_CONSTIR_NONE;
        if (!lower_expression(context, function_index, arm->result_expression,
                              &result_node, depth + 1u)) return false;
        w_seed_constir_switch_arm ir_arm;
        ir_arm.owner_node = W_SEED_CONSTIR_NONE;
        ir_arm.pattern_kind = arm->pattern_kind;
        ir_arm.enum_base_index = arm->enum_index;
        ir_arm.enum_case_index = arm->enum_case_index;
        ir_arm.result_node = result_node;
        ir_arm.pattern_span = arm->pattern_span;
        ir_arm.source_span = arm->span;
        uint32_t arm_index = W_SEED_CONSTIR_NONE;
        if (!append_switch_arm(context, &ir_arm, &arm_index)) return false;
        if (offset == 0u) node.first_switch_arm = arm_index;
      }
      break;
    }
    default:
      mark_failure(context, expression->span, expression_index);
      return false;
  }
  if (!append_node(context, &node, ir_index)) return false;
  if (context->emit && *ir_index != W_SEED_CONSTIR_NONE &&
      context->output != NULL) {
    if (node.kind == W_SEED_CONSTIR_NODE_CALL &&
        context->output->call_arguments != NULL &&
        node.first_call_argument != W_SEED_CONSTIR_NONE) {
      for (uint32_t offset = 0; offset < node.call_argument_count; offset += 1)
        context->output->call_arguments[node.first_call_argument + offset].owner_node =
            *ir_index;
    }
    if (node.kind == W_SEED_CONSTIR_NODE_MEMBERSHIP &&
        context->output->membership_cases != NULL &&
        node.first_membership_case != W_SEED_CONSTIR_NONE) {
      for (uint32_t offset = 0; offset < node.membership_case_count; offset += 1)
        context->output->membership_cases[node.first_membership_case + offset].owner_node =
            *ir_index;
    }
    if (node.kind == W_SEED_CONSTIR_NODE_SWITCH &&
        context->output->switch_arms != NULL &&
        node.first_switch_arm != W_SEED_CONSTIR_NONE) {
      for (uint32_t offset = 0; offset < node.switch_arm_count; offset += 1)
        context->output->switch_arms[node.first_switch_arm + offset].owner_node =
            *ir_index;
    }
  }
  return true;
}
