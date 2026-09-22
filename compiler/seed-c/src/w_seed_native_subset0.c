#include "w_seed_native_subset0.h"
#include "w_seed_scalar_evaluator0.h"

#include <limits.h>
#include <string.h>

static const uint8_t NATIVE_SUBSET0_PROFILE[] = "native-process@1";
static const uint8_t NATIVE_SUBSET0_SLOT[] = ".default";
static const uint8_t NATIVE_SUBSET0_CALLEE[] = "print";
static const uint8_t NATIVE_SUBSET0_REQUIREMENT[] = "Console";

static bool native_panic_terminator_supported(
    const w_seed_hir0_program *program, size_t terminator_index,
    uint32_t function_index, const w_seed_hir0_terminator *terminator);

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

/* NativeSubset0 carries every fixed-width integer through one unsigned
 * 64-bit slot.  The HIR keeps the logical signedness and width on the type
 * record; the historical I64/U64 records are merely the canonical 64-bit
 * spellings.  Keeping this fact lookup here makes every native admission and
 * constant check agree without adding per-width value kinds or helpers. */
typedef struct {
  bool is_signed;
  uint16_t bit_width;
} native_integer_facts;

typedef struct {
  bool source_is_float;
  bool source_is_signed;
  uint16_t source_width;
  uint16_t destination_width;
} native_numeric_widening_facts;

typedef struct {
  uint16_t bit_width;
} native_float_bits_facts;

static bool native_integer_type_facts(const w_seed_hir0_program *program,
                                      uint32_t type_index,
                                      native_integer_facts *facts) {
  if (program == NULL || facts == NULL || type_index >= program->type_count)
    return false;
  const w_seed_hir0_type *type = &program->types[type_index];
  if (type->kind == W_SEED_HIR0_TYPE_I64) {
    if (!type->integer_is_signed || type->integer_bit_width != 64u)
      return false;
    facts->is_signed = true;
    facts->bit_width = 64u;
    return true;
  }
  if (type->kind == W_SEED_HIR0_TYPE_U64) {
    if (type->integer_is_signed || type->integer_bit_width != 64u)
      return false;
    facts->is_signed = false;
    facts->bit_width = 64u;
    return true;
  }
  if (type->kind != W_SEED_HIR0_TYPE_INTEGER ||
      (type->integer_bit_width != 8u && type->integer_bit_width != 16u &&
       type->integer_bit_width != 32u))
    return false;
  facts->is_signed = type->integer_is_signed;
  facts->bit_width = type->integer_bit_width;
  return true;
}

static bool native_integer_facts_equal(native_integer_facts left,
                                       native_integer_facts right) {
  return left.is_signed == right.is_signed &&
         left.bit_width == right.bit_width;
}

static bool native_integer_is_bitwise_binary(
    w_seed_hir0_binary_operator operation) {
  return operation >= W_SEED_HIR0_BINARY_BIT_AND &&
         operation <= W_SEED_HIR0_BINARY_BIT_XOR;
}

static bool native_integer_is_bit_primitive_unary(
    w_seed_hir0_unary_operator operation) {
  return operation == W_SEED_HIR0_UNARY_COUNT_ONES ||
         operation == W_SEED_HIR0_UNARY_COUNT_ZEROS ||
         operation == W_SEED_HIR0_UNARY_COUNT_LEADING_ZEROS ||
         operation == W_SEED_HIR0_UNARY_COUNT_TRAILING_ZEROS ||
         operation == W_SEED_HIR0_UNARY_REVERSED_BITS ||
         operation == W_SEED_HIR0_UNARY_REVERSED_BYTES;
}

static bool native_integer_is_bit_count_unary(
    w_seed_hir0_unary_operator operation) {
  return operation == W_SEED_HIR0_UNARY_COUNT_ONES ||
         operation == W_SEED_HIR0_UNARY_COUNT_ZEROS ||
         operation == W_SEED_HIR0_UNARY_COUNT_LEADING_ZEROS ||
         operation == W_SEED_HIR0_UNARY_COUNT_TRAILING_ZEROS;
}

static bool native_integer_is_rotation_binary(
    w_seed_hir0_binary_operator operation) {
  return operation == W_SEED_HIR0_BINARY_ROTATED_LEFT ||
         operation == W_SEED_HIR0_BINARY_ROTATED_RIGHT;
}

/* The old primitive identities now describe operations over the logical
 * width carried by HIR, rather than u64-only operations. Count results are
 * UInt; reversed results keep the operand's exact integer type. */
static bool native_integer_bit_primitive_shape_valid(
    const w_seed_hir0_program *program, const w_seed_hir0_value *value,
    native_integer_facts *operand_facts,
    native_integer_facts *result_facts) {
  if (program == NULL || value == NULL ||
      (value->kind != W_SEED_HIR0_VALUE_UNARY_I64 &&
       value->kind != W_SEED_HIR0_VALUE_UNARY_U64) ||
      !native_integer_is_bit_primitive_unary(value->unary_operator) ||
      value->left_value == W_SEED_HIR0_NONE ||
      value->left_value >= program->value_count ||
      value->right_value != W_SEED_HIR0_NONE ||
      value->binding_index != W_SEED_HIR0_NONE ||
      value->parameter_index != W_SEED_HIR0_NONE ||
      value->call_index != W_SEED_HIR0_NONE ||
      value->first_interpolation_segment != W_SEED_HIR0_NONE ||
      value->interpolation_segment_count != 0u ||
      value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
      value->block_argument_index != W_SEED_HIR0_NONE)
    return false;

  native_integer_facts input;
  native_integer_facts output;
  if (!native_integer_type_facts(program, value->type_index, &output) ||
      !native_integer_type_facts(
          program, program->values[value->left_value].type_index, &input))
    return false;

  if (native_integer_is_bit_count_unary(value->unary_operator)) {
    if (value->kind != W_SEED_HIR0_VALUE_UNARY_U64 ||
        program->types[value->type_index].kind != W_SEED_HIR0_TYPE_U64 ||
        output.is_signed || output.bit_width != 64u)
      return false;
  } else if (value->type_index !=
                 program->values[value->left_value].type_index ||
             !native_integer_facts_equal(output, input) ||
             value->kind != (input.is_signed ? W_SEED_HIR0_VALUE_UNARY_I64
                                              : W_SEED_HIR0_VALUE_UNARY_U64)) {
    return false;
  }

  if (operand_facts != NULL) *operand_facts = input;
  if (result_facts != NULL) *result_facts = output;
  return true;
}

static bool native_integer_rotation_shape_valid(
    const w_seed_hir0_program *program, const w_seed_hir0_value *value,
    native_integer_facts *result_facts) {
  if (program == NULL || value == NULL ||
      (value->kind != W_SEED_HIR0_VALUE_BINARY_I64 &&
       value->kind != W_SEED_HIR0_VALUE_BINARY_U64) ||
      !native_integer_is_rotation_binary(value->binary_operator) ||
      value->left_value == W_SEED_HIR0_NONE ||
      value->right_value == W_SEED_HIR0_NONE ||
      value->left_value >= program->value_count ||
      value->right_value >= program->value_count)
    return false;

  native_integer_facts result;
  native_integer_facts operand;
  native_integer_facts count;
  const bool expected_signed = value->kind == W_SEED_HIR0_VALUE_BINARY_I64;
  if (!native_integer_type_facts(program, value->type_index, &result) ||
      result.is_signed != expected_signed ||
      !native_integer_type_facts(
          program, program->values[value->left_value].type_index, &operand) ||
      !native_integer_type_facts(
          program, program->values[value->right_value].type_index, &count) ||
      !native_integer_facts_equal(result, operand) ||
      program->values[value->left_value].type_index != value->type_index ||
      program->types[program->values[value->right_value].type_index].kind !=
          W_SEED_HIR0_TYPE_U64 ||
      count.is_signed || count.bit_width != 64u)
    return false;

  if (result_facts != NULL) *result_facts = result;
  return true;
}

static bool native_integer_bitwise_shape_valid(
    const w_seed_hir0_program *program, const w_seed_hir0_value *value,
    bool expected_signed) {
  native_integer_facts result_facts;
  native_integer_facts left_facts;
  native_integer_facts right_facts;
  return program != NULL && value != NULL &&
         (value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
          value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
         value->kind == (expected_signed ? W_SEED_HIR0_VALUE_BINARY_I64
                                         : W_SEED_HIR0_VALUE_BINARY_U64) &&
         native_integer_is_bitwise_binary(value->binary_operator) &&
         value->left_value < program->value_count &&
         value->right_value < program->value_count &&
         native_integer_type_facts(program, value->type_index,
                                   &result_facts) &&
         result_facts.is_signed == expected_signed &&
         native_integer_type_facts(
             program, program->values[value->left_value].type_index,
             &left_facts) &&
         native_integer_type_facts(
             program, program->values[value->right_value].type_index,
             &right_facts) &&
         native_integer_facts_equal(result_facts, left_facts) &&
         native_integer_facts_equal(result_facts, right_facts) &&
         value->type_index ==
             program->values[value->left_value].type_index &&
         value->type_index ==
             program->values[value->right_value].type_index;
}

static bool native_integer_comparison_shape_valid(
    const w_seed_hir0_program *program, const w_seed_hir0_value *value) {
  if (program == NULL || value == NULL ||
      value->kind != W_SEED_HIR0_VALUE_BINARY_INTEGER_COMPARISON ||
      value->binary_operator < W_SEED_HIR0_BINARY_EQUAL ||
      value->binary_operator > W_SEED_HIR0_BINARY_GREATER_EQUAL ||
      value->type_index >= program->type_count ||
      program->types[value->type_index].kind != W_SEED_HIR0_TYPE_BOOL ||
      value->left_value >= program->value_count ||
      value->right_value >= program->value_count)
    return false;
  native_integer_facts left_facts;
  native_integer_facts right_facts;
  if (!native_integer_type_facts(
          program, program->values[value->left_value].type_index,
          &left_facts) ||
      !native_integer_type_facts(
          program, program->values[value->right_value].type_index,
          &right_facts) ||
      !native_integer_facts_equal(left_facts, right_facts))
    return false;
  return left_facts.bit_width == 8u || left_facts.bit_width == 16u ||
         left_facts.bit_width == 32u || left_facts.bit_width == 64u;
}

/* A widening value is admitted only when its explicit source identity and
 * destination identity describe the one frontend route.  NativeSubset0 does
 * not infer this from the carrier kind: the HIR wrapper remains the source
 * of truth so a forged retag cannot cross this boundary. */
static bool native_integer_widening_route(
    const w_seed_hir0_program *program, uint32_t source_type,
    uint32_t destination_type, native_integer_facts *source_facts,
    native_integer_facts *destination_facts) {
  if (program == NULL || source_facts == NULL || destination_facts == NULL ||
      source_type == destination_type ||
      !native_integer_type_facts(program, source_type, source_facts) ||
      !native_integer_type_facts(program, destination_type,
                                 destination_facts) ||
      destination_facts->bit_width <= source_facts->bit_width)
    return false;
  return source_facts->is_signed == destination_facts->is_signed ||
         (!source_facts->is_signed && destination_facts->is_signed);
}

static bool native_integer_conversion_route(
    const w_seed_hir0_program *program, uint32_t source_type,
    uint32_t destination_type, native_integer_facts *source_facts,
    native_integer_facts *destination_facts) {
  return program != NULL && source_facts != NULL &&
         destination_facts != NULL &&
         native_integer_type_facts(program, source_type, source_facts) &&
         native_integer_type_facts(program, destination_type,
                                   destination_facts);
}

static bool native_float_type_width(const w_seed_hir0_program *program,
                                    uint32_t type_index,
                                    uint16_t *bit_width) {
  if (program == NULL || bit_width == NULL || type_index >= program->type_count)
    return false;
  const w_seed_hir0_type *type = &program->types[type_index];
  if (type->integer_is_signed || type->integer_bit_width != 0u) return false;
  if (type->kind == W_SEED_HIR0_TYPE_F32) {
    *bit_width = 32u;
    return true;
  }
  if (type->kind == W_SEED_HIR0_TYPE_F64) {
    *bit_width = 64u;
    return true;
  }
  return false;
}

/* W-1646 is deliberately narrower than an all-numeric conversion lattice.
 * The seed admits only exact-binary32-to-binary64 extension, narrow signed or
 * unsigned integers through 16 bits to f32, and integers through 32 bits to
 * f64.  In particular, TYPE_I64/TYPE_U64 (including Int/UInt) are never
 * integer sources for this route. */
static bool native_numeric_widening_route(
    const w_seed_hir0_program *program, uint32_t source_type,
    uint32_t destination_type, native_numeric_widening_facts *facts) {
  if (program == NULL || facts == NULL || source_type >= program->type_count ||
      destination_type >= program->type_count)
    return false;
  uint16_t destination_width = 0u;
  if (!native_float_type_width(program, destination_type,
                               &destination_width))
    return false;

  const w_seed_hir0_type *source = &program->types[source_type];
  if (!source->integer_is_signed && source->integer_bit_width == 0u &&
      source->kind == W_SEED_HIR0_TYPE_F32 && destination_width == 64u) {
    *facts = (native_numeric_widening_facts){
        .source_is_float = true,
        .source_is_signed = false,
        .source_width = 32u,
        .destination_width = destination_width};
    return true;
  }

  native_integer_facts source_facts;
  if (source->kind != W_SEED_HIR0_TYPE_INTEGER ||
      !native_integer_type_facts(program, source_type, &source_facts) ||
      (destination_width == 32u && source_facts.bit_width > 16u) ||
      (destination_width == 64u && source_facts.bit_width > 32u))
    return false;
  *facts = (native_numeric_widening_facts){
      .source_is_float = false,
      .source_is_signed = source_facts.is_signed,
      .source_width = source_facts.bit_width,
      .destination_width = destination_width};
  return true;
}

static bool native_numeric_widening_shape_valid(
    const w_seed_hir0_program *program, uint32_t value_index,
    native_numeric_widening_facts *facts) {
  if (program == NULL || facts == NULL || value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->kind != W_SEED_HIR0_VALUE_NUMERIC_WIDEN ||
      value->left_value == W_SEED_HIR0_NONE ||
      value->left_value >= program->value_count ||
      value->right_value != W_SEED_HIR0_NONE ||
      value->source_type >= program->type_count ||
      program->values[value->left_value].type_index != value->source_type ||
      program->values[value->left_value].owner_kind !=
          W_SEED_HIR0_VALUE_OWNER_NUMERIC_WIDEN ||
      program->values[value->left_value].owner_index != value_index ||
      program->values[value->left_value].owner_ordinal != 0u ||
      !native_numeric_widening_route(program, value->source_type,
                                     value->type_index, facts))
    return false;
  return true;
}

/* The representation bridge is intentionally exact-width and unsigned on
 * its integer side: u32 <-> f32 and u64 <-> f64 only. The physical carrier
 * remains i64, but the logical type pair is re-derived from verified HIR
 * rather than inferred from that shared carrier. */
static bool native_float_bits_route(
    const w_seed_hir0_program *program, w_seed_hir0_value_kind kind,
    uint32_t source_type, uint32_t destination_type,
    native_float_bits_facts *facts) {
  if (program == NULL || facts == NULL) return false;

  native_integer_facts integer_facts;
  uint16_t float_width = 0u;
  if (kind == W_SEED_HIR0_VALUE_FLOAT_FROM_BITS) {
    if (!native_integer_type_facts(program, source_type, &integer_facts) ||
        integer_facts.is_signed ||
        !native_float_type_width(program, destination_type, &float_width) ||
        integer_facts.bit_width != float_width)
      return false;
    facts->bit_width = float_width;
    return true;
  }
  if (kind == W_SEED_HIR0_VALUE_FLOAT_TO_BITS) {
    if (!native_float_type_width(program, source_type, &float_width) ||
        !native_integer_type_facts(program, destination_type,
                                   &integer_facts) ||
        integer_facts.is_signed || integer_facts.bit_width != float_width)
      return false;
    facts->bit_width = float_width;
    return true;
  }
  return false;
}

static bool native_float_bits_shape_valid(
    const w_seed_hir0_program *program, uint32_t value_index,
    native_float_bits_facts *facts) {
  if (program == NULL || facts == NULL || value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if ((value->kind != W_SEED_HIR0_VALUE_FLOAT_FROM_BITS &&
       value->kind != W_SEED_HIR0_VALUE_FLOAT_TO_BITS) ||
      value->left_value == W_SEED_HIR0_NONE ||
      value->left_value >= program->value_count ||
      value->right_value != W_SEED_HIR0_NONE ||
      value->source_type >= program->type_count ||
      program->values[value->left_value].type_index != value->source_type ||
      program->values[value->left_value].owner_kind !=
          W_SEED_HIR0_VALUE_OWNER_FLOAT_BITS_CONVERSION ||
      program->values[value->left_value].owner_index != value_index ||
      program->values[value->left_value].owner_ordinal != 0u ||
      !native_float_bits_route(program, value->kind, value->source_type,
                               value->type_index, facts))
    return false;
  return true;
}

static uint64_t native_integer_width_mask(native_integer_facts facts) {
  return facts.bit_width == 64u
             ? UINT64_MAX
             : (UINT64_C(1) << facts.bit_width) - UINT64_C(1);
}

static uint64_t native_integer_mask_bits(uint64_t bits,
                                         native_integer_facts facts) {
  return bits & native_integer_width_mask(facts);
}

static uint64_t native_integer_carrier_bits(uint64_t bits,
                                             native_integer_facts facts) {
  const uint64_t mask = native_integer_width_mask(facts);
  bits &= mask;
  if (facts.is_signed && facts.bit_width < 64u &&
      (bits & (UINT64_C(1) << (facts.bit_width - 1u))) != 0u)
    bits |= ~mask;
  return bits;
}

static uint64_t native_integer_saturating_conversion_bits(
    uint64_t source_bits, native_integer_facts source_facts,
    native_integer_facts destination_facts) {
  const uint64_t source_carrier =
      native_integer_carrier_bits(source_bits, source_facts);
  const uint64_t destination_mask =
      native_integer_width_mask(destination_facts);
  uint64_t result = 0u;
  if (destination_facts.is_signed) {
    uint64_t maximum =
        destination_facts.bit_width == 64u
            ? (uint64_t)INT64_MAX
            : (UINT64_C(1) << (destination_facts.bit_width - 1u)) -
                  UINT64_C(1);
    if (source_facts.is_signed) {
      int64_t value = 0;
      (void)memcpy(&value, &source_carrier, sizeof(value));
      if (destination_facts.bit_width < 64u) {
        const int64_t maximum_signed = (int64_t)maximum;
        const int64_t minimum_signed = -maximum_signed - INT64_C(1);
        if (value < minimum_signed) value = minimum_signed;
        if (value > maximum_signed) value = maximum_signed;
      }
      result = (uint64_t)value;
    } else {
      if (source_carrier > maximum) result = maximum;
      else result = source_carrier;
    }
    return native_integer_mask_bits(result, destination_facts);
  }

  uint64_t value = source_carrier;
  if (source_facts.is_signed) {
    int64_t signed_value = 0;
    (void)memcpy(&signed_value, &source_carrier, sizeof(signed_value));
    if (signed_value < 0) return 0u;
    value = (uint64_t)signed_value;
  }
  if (value > destination_mask) value = destination_mask;
  return value;
}

static bool native_integer_is_checked_shift(
    w_seed_hir0_binary_operator operation) {
  return operation == W_SEED_HIR0_BINARY_SHIFT_LEFT ||
         operation == W_SEED_HIR0_BINARY_SHIFT_RIGHT;
}

static bool native_integer_checked_shift_shape_valid(
    const w_seed_hir0_program *program, const w_seed_hir0_value *value,
    native_integer_facts *result_facts) {
  if (program == NULL || value == NULL ||
      !native_integer_is_checked_shift(value->binary_operator) ||
      (value->kind != W_SEED_HIR0_VALUE_BINARY_I64 &&
       value->kind != W_SEED_HIR0_VALUE_BINARY_U64) ||
      value->left_value >= program->value_count ||
      value->right_value >= program->value_count)
    return false;

  const bool expected_signed = value->kind == W_SEED_HIR0_VALUE_BINARY_I64;
  native_integer_facts result;
  native_integer_facts left;
  native_integer_facts count;
  if (!native_integer_type_facts(program, value->type_index, &result) ||
      result.is_signed != expected_signed ||
      !native_integer_type_facts(
          program, program->values[value->left_value].type_index, &left) ||
      !native_integer_type_facts(
          program, program->values[value->right_value].type_index, &count) ||
      !native_integer_facts_equal(result, left) ||
      program->values[value->left_value].type_index != value->type_index ||
      count.is_signed || count.bit_width != 64u)
    return false;

  if (result_facts != NULL) *result_facts = result;
  return true;
}

static bool native_integer_is_masked_shift(
    w_seed_hir0_binary_operator operation) {
  return operation == W_SEED_HIR0_BINARY_MASKED_SHIFT_LEFT ||
         operation == W_SEED_HIR0_BINARY_MASKED_SHIFT_RIGHT ||
         operation == W_SEED_HIR0_BINARY_LOGICAL_SHIFT_RIGHT;
}

static bool native_integer_masked_shift_shape_valid(
    const w_seed_hir0_program *program, const w_seed_hir0_value *value,
    native_integer_facts *result_facts) {
  if (program == NULL || value == NULL ||
      !native_integer_is_masked_shift(value->binary_operator) ||
      (value->kind != W_SEED_HIR0_VALUE_BINARY_I64 &&
       value->kind != W_SEED_HIR0_VALUE_BINARY_U64) ||
      value->left_value == W_SEED_HIR0_NONE ||
      value->right_value == W_SEED_HIR0_NONE ||
      value->left_value >= program->value_count ||
      value->right_value >= program->value_count)
    return false;

  const bool expected_signed = value->kind == W_SEED_HIR0_VALUE_BINARY_I64;
  native_integer_facts result;
  native_integer_facts left;
  native_integer_facts count;
  if (!native_integer_type_facts(program, value->type_index, &result) ||
      result.is_signed != expected_signed ||
      !native_integer_type_facts(
          program, program->values[value->left_value].type_index, &left) ||
      !native_integer_type_facts(
          program, program->values[value->right_value].type_index, &count) ||
      !native_integer_facts_equal(result, left) ||
      value->type_index != program->values[value->left_value].type_index ||
      count.is_signed || count.bit_width != 64u)
    return false;

  if (result_facts != NULL) *result_facts = result;
  return true;
}

static bool native_integer_is_shift_policy(
    w_seed_hir0_binary_operator operation) {
  return native_integer_is_checked_shift(operation) ||
         native_integer_is_masked_shift(operation);
}

static bool native_integer_shift_policy_shape_valid(
    const w_seed_hir0_program *program, const w_seed_hir0_value *value,
    native_integer_facts *result_facts) {
  return native_integer_is_checked_shift(value != NULL
                                             ? value->binary_operator
                                             : W_SEED_HIR0_BINARY_ADD)
             ? native_integer_checked_shift_shape_valid(program, value,
                                                        result_facts)
             : native_integer_masked_shift_shape_valid(program, value,
                                                       result_facts);
}

static uint64_t native_integer_arithmetic_shift_right_bits(
    uint64_t bits, native_integer_facts facts, uint64_t count) {
  const uint64_t mask = native_integer_width_mask(facts);
  const uint64_t normalized = bits & mask;
  uint64_t shifted = normalized >> count;
  const uint64_t sign_bit = UINT64_C(1) << (facts.bit_width - 1u);
  if (count != 0u && (normalized & sign_bit) != 0u)
    shifted |= mask & ~(mask >> count);
  return shifted & mask;
}

/* The evaluator carries integer values in uint64_t. Normalize that carrier to
 * the logical width, then use unsigned shifts only; a reverse arithmetic
 * shift proves signed representability without host signed overflow. */
static bool native_integer_checked_shift_bits(
    w_seed_hir0_binary_operator operation, native_integer_facts facts,
    uint64_t left_bits, uint64_t count, uint64_t *result_bits) {
  if (result_bits == NULL ||
      !native_integer_is_checked_shift(operation) ||
      (facts.bit_width != 8u && facts.bit_width != 16u &&
       facts.bit_width != 32u && facts.bit_width != 64u))
    return false;

  const uint64_t mask = native_integer_width_mask(facts);
  const uint64_t left = left_bits & mask;
  const uint64_t carrier = native_integer_carrier_bits(left, facts);
  if (left_bits != carrier || count >= facts.bit_width) return false;

  uint64_t candidate = 0u;
  if (operation == W_SEED_HIR0_BINARY_SHIFT_LEFT) {
    candidate = (left << count) & mask;
    if (facts.is_signed) {
      if (native_integer_arithmetic_shift_right_bits(candidate, facts,
                                                     count) != left)
        return false;
    } else if ((candidate >> count) != left) {
      return false;
    }
  } else if (facts.is_signed) {
    candidate = native_integer_arithmetic_shift_right_bits(left, facts,
                                                           count);
  } else {
    candidate = left >> count;
  }

  *result_bits = candidate;
  return true;
}

static bool native_integer_masked_shift_bits(
    w_seed_hir0_binary_operator operation, native_integer_facts facts,
    uint64_t left_bits, uint64_t count, uint64_t *result_bits) {
  if (result_bits == NULL || !native_integer_is_masked_shift(operation) ||
      (facts.bit_width != 8u && facts.bit_width != 16u &&
       facts.bit_width != 32u && facts.bit_width != 64u))
    return false;

  const uint64_t mask = native_integer_width_mask(facts);
  const uint64_t left = left_bits & mask;
  if (operation == W_SEED_HIR0_BINARY_LOGICAL_SHIFT_RIGHT) {
    if (count >= facts.bit_width) return false;
    *result_bits = (left >> count) & mask;
    return true;
  }

  const uint64_t normalized_count = count & ((uint64_t)facts.bit_width - 1u);
  if (operation == W_SEED_HIR0_BINARY_MASKED_SHIFT_LEFT) {
    *result_bits = (left << normalized_count) & mask;
    return true;
  }

  *result_bits = facts.is_signed
                     ? native_integer_arithmetic_shift_right_bits(
                           left, facts, normalized_count)
                     : (left >> normalized_count) & mask;
  return true;
}

static bool native_integer_is_checked_binary(
    w_seed_hir0_binary_operator operation) {
  return operation == W_SEED_HIR0_BINARY_ADD ||
         operation == W_SEED_HIR0_BINARY_SUBTRACT ||
         operation == W_SEED_HIR0_BINARY_MULTIPLY ||
         operation == W_SEED_HIR0_BINARY_DIVIDE ||
         operation == W_SEED_HIR0_BINARY_REMAINDER;
}

static bool native_integer_is_wrapping_binary(w_seed_hir0_binary_operator op) {
  return op == W_SEED_HIR0_BINARY_WRAPPING_ADD ||
         op == W_SEED_HIR0_BINARY_WRAPPING_SUBTRACT ||
         op == W_SEED_HIR0_BINARY_WRAPPING_MULTIPLY ||
         op == W_SEED_HIR0_BINARY_WRAPPING_POWER ||
         op == W_SEED_HIR0_BINARY_WRAPPING_SHIFT_LEFT;
}

static bool native_integer_is_wrapping_unary(w_seed_hir0_unary_operator op) {
  return op == W_SEED_HIR0_UNARY_WRAPPING_NEGATE;
}

static bool native_scalar_type_supported(const w_seed_hir0_program *program,
                                         uint32_t type_index) {
  if (program == NULL || type_index >= program->type_count) return false;
  return program->types[type_index].kind == W_SEED_HIR0_TYPE_BOOL ||
         native_integer_type_facts(program, type_index, &(native_integer_facts){0});
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
  if (program->cleanup_count != 0u || program->module_count != 1u ||
      program->function_count != 1u ||
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

/* U64 is a logical unsigned type even though Native0's bounded ABI carries
 * it in the same 64-bit slot as i64. Keep all constant evaluation unsigned;
 * converting through int64_t would turn values above INT64_MAX into a
 * different mathematical value before the overflow check runs. */
static bool checked_u64_add(uint64_t left, uint64_t right, uint64_t *result) {
  if (result == NULL || right > UINT64_MAX - left) return false;
  *result = left + right;
  return true;
}

static bool checked_u64_subtract(uint64_t left, uint64_t right,
                                 uint64_t *result) {
  if (result == NULL || right > left) return false;
  *result = left - right;
  return true;
}

static bool checked_u64_multiply(uint64_t left, uint64_t right,
                                 uint64_t *result) {
  if (result == NULL || (left != 0u && right > UINT64_MAX / left))
    return false;
  *result = left * right;
  return true;
}

static bool checked_u64_power(uint64_t base, uint64_t exponent,
                              uint64_t *result) {
  if (result == NULL) return false;
  uint64_t accumulator = 1u;
  size_t steps = 0u;
  while (exponent != 0u && steps < 64u) {
    if ((exponent & 1u) != 0u &&
        !checked_u64_multiply(accumulator, base, &accumulator))
      return false;
    exponent >>= 1u;
    if (exponent != 0u && !checked_u64_multiply(base, base, &base))
      return false;
    steps += 1u;
  }
  if (exponent != 0u) return false;
  *result = accumulator;
  return true;
}

/* The wrapping-power constant oracle uses the same bounded algorithm as the
 * runtime lowering. A u64 exponent has at most 64 nonzero-bit steps; keeping
 * the explicit bound makes malformed forged trees fail closed instead of
 * turning constant validation into an unbounded host loop. */
static bool wrapping_u64_power(uint64_t base, uint64_t exponent,
                               uint64_t *result) {
  if (result == NULL) return false;
  uint64_t accumulator = 1u;
  size_t steps = 0u;
  while (exponent != 0u && steps < 64u) {
    if ((exponent & 1u) != 0u) accumulator *= base;
    exponent >>= 1u;
    if (exponent != 0u) base *= base;
    steps += 1u;
  }
  if (exponent != 0u) return false;
  *result = accumulator;
  return true;
}

/* Evaluate the complete wrapping family in the logical bit domain.  Every
 * operation is deliberately expressed with uint64_t arithmetic: unsigned C
 * overflow is defined, and masking after each step gives the requested
 * modulo-2^N result for N < 64 as well as the native u64 case.  No signed
 * intermediate is formed, so INT_MIN negation and signed multiplication never
 * reach host C semantics. */
static bool evaluate_integer_bits(const w_seed_hir0_program *program,
                                  uint32_t value_index, size_t depth,
                                  native_integer_facts expected,
                                  uint64_t *result) {
  if (program == NULL || result == NULL || depth > 256u ||
      value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  native_integer_facts actual;
  if (!native_integer_type_facts(program, value->type_index, &actual) ||
      !native_integer_facts_equal(actual, expected))
    return false;
  const uint64_t mask = native_integer_width_mask(expected);
  const bool signed_carrier = expected.is_signed;
  if (value->kind == (signed_carrier ? W_SEED_HIR0_VALUE_CONST_I64
                                     : W_SEED_HIR0_VALUE_CONST_U64)) {
    *result = signed_carrier
                 ? native_integer_mask_bits((uint64_t)value->integer_value,
                                             expected)
                 : native_integer_mask_bits(value->unsigned_integer_value,
                                             expected);
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_INTEGER_TRUNCATING_BITS ||
      value->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING) {
    const bool saturating =
        value->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING;
    const w_seed_hir0_value_owner_kind owner_kind =
        saturating ? W_SEED_HIR0_VALUE_OWNER_INTEGER_SATURATING
                   : W_SEED_HIR0_VALUE_OWNER_INTEGER_TRUNCATING_BITS;
    native_integer_facts source_facts;
    native_integer_facts destination_facts;
    if (value->source_type >= program->type_count ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->left_value >= program->value_count ||
        value->right_value != W_SEED_HIR0_NONE ||
        !native_integer_conversion_route(
            program, value->source_type, value->type_index, &source_facts,
            &destination_facts) ||
        !native_integer_facts_equal(destination_facts, expected) ||
        program->values[value->left_value].type_index != value->source_type ||
        program->values[value->left_value].owner_kind !=
            owner_kind ||
        program->values[value->left_value].owner_index != value_index ||
        program->values[value->left_value].owner_ordinal != 0u)
      return false;
    uint64_t source_bits = 0u;
    if (!evaluate_integer_bits(program, value->left_value, depth + 1u,
                               source_facts, &source_bits))
      return false;
    *result = saturating
                  ? native_integer_saturating_conversion_bits(
                        source_bits, source_facts, destination_facts)
                  : native_integer_mask_bits(
                        native_integer_carrier_bits(source_bits,
                                                    source_facts),
                        expected);
    return true;
  }

  if ((value->kind == W_SEED_HIR0_VALUE_UNARY_I64 ||
       value->kind == W_SEED_HIR0_VALUE_UNARY_U64) &&
      native_integer_is_bit_primitive_unary(value->unary_operator)) {
    native_integer_facts operand_facts;
    native_integer_facts result_facts;
    uint64_t operand = 0u;
    if (!native_integer_bit_primitive_shape_valid(
            program, value, &operand_facts, &result_facts) ||
        !native_integer_facts_equal(result_facts, expected) ||
        !evaluate_integer_bits(program, value->left_value, depth + 1u,
                               operand_facts, &operand))
      return false;

    const uint16_t width = operand_facts.bit_width;
    const uint64_t operand_mask = native_integer_width_mask(operand_facts);
    operand &= operand_mask;
    uint64_t transformed = 0u;
    if (value->unary_operator == W_SEED_HIR0_UNARY_COUNT_ONES ||
        value->unary_operator == W_SEED_HIR0_UNARY_COUNT_ZEROS) {
      uint64_t ones = 0u;
      for (uint16_t bit = 0u; bit < width; bit += 1u)
        ones += (operand >> bit) & UINT64_C(1);
      transformed = value->unary_operator == W_SEED_HIR0_UNARY_COUNT_ONES
                        ? ones
                        : (uint64_t)width - ones;
    } else if (value->unary_operator ==
               W_SEED_HIR0_UNARY_COUNT_LEADING_ZEROS) {
      uint16_t zeros = 0u;
      for (uint16_t bit = width; bit != 0u;
           bit = (uint16_t)(bit - 1u)) {
        if (((operand >> (bit - 1u)) & UINT64_C(1)) != 0u) break;
        zeros += 1u;
      }
      transformed = zeros;
    } else if (value->unary_operator ==
               W_SEED_HIR0_UNARY_COUNT_TRAILING_ZEROS) {
      uint16_t zeros = 0u;
      while (zeros < width &&
             ((operand >> zeros) & UINT64_C(1)) == 0u)
        zeros += 1u;
      transformed = zeros;
    } else if (value->unary_operator == W_SEED_HIR0_UNARY_REVERSED_BITS) {
      for (uint16_t bit = 0u; bit < width; bit += 1u) {
        transformed = (transformed << 1u) | (operand & UINT64_C(1));
        operand >>= 1u;
      }
    } else if (value->unary_operator == W_SEED_HIR0_UNARY_REVERSED_BYTES) {
      const uint16_t byte_count = (uint16_t)(width / 8u);
      for (uint16_t byte = 0u; byte < byte_count; byte += 1u) {
        transformed = (transformed << 8u) | (operand & UINT64_C(0xff));
        operand >>= 8u;
      }
    } else {
      return false;
    }
    *result = native_integer_mask_bits(transformed, expected);
    return true;
  }

  if (value->kind == (signed_carrier ? W_SEED_HIR0_VALUE_UNARY_I64
                                     : W_SEED_HIR0_VALUE_UNARY_U64)) {
    if (value->left_value == W_SEED_HIR0_NONE) return false;
    uint64_t operand = 0u;
    if (!evaluate_integer_bits(program, value->left_value, depth + 1u,
                               expected, &operand))
      return false;
    if (value->unary_operator == W_SEED_HIR0_UNARY_NEGATE && signed_carrier) {
      /* Checked ordinary signed negation is accepted as a constant child of a
       * wrapping operation.  Detect the logical minimum in bits before using
       * unsigned subtraction, so -INT_MIN never reaches host signed C. */
      const uint64_t sign_bit = UINT64_C(1) << (expected.bit_width - 1u);
      if ((operand & native_integer_width_mask(expected)) == sign_bit)
        return false;
      *result = (UINT64_C(0) - operand) & mask;
      return true;
    }
    if (value->unary_operator == W_SEED_HIR0_UNARY_BIT_NOT) {
      *result = (~operand) & mask;
      return true;
    }
    if (!native_integer_is_wrapping_unary(value->unary_operator)) return false;
    /* 0 - operand is uint64_t subtraction, never signed negation. */
    *result = (UINT64_C(0) - operand) & mask;
    return true;
  }

  if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
       value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
      native_integer_is_rotation_binary(value->binary_operator)) {
    native_integer_facts result_facts;
    const native_integer_facts count_facts = {false, 64u};
    uint64_t left = 0u;
    uint64_t count_value = 0u;
    if (!native_integer_rotation_shape_valid(program, value, &result_facts) ||
        !native_integer_facts_equal(result_facts, expected) ||
        !evaluate_integer_bits(program, value->left_value, depth + 1u,
                               expected, &left) ||
        !evaluate_integer_bits(program, value->right_value, depth + 1u,
                               count_facts, &count_value))
      return false;

    const uint64_t normalized_count = count_value % expected.bit_width;
    left &= mask;
    if (normalized_count == 0u) {
      *result = left;
    } else if (value->binary_operator == W_SEED_HIR0_BINARY_ROTATED_LEFT) {
      *result = ((left << normalized_count) |
                 (left >> (expected.bit_width - normalized_count))) &
                mask;
    } else {
      *result = ((left >> normalized_count) |
                 (left << (expected.bit_width - normalized_count))) &
                mask;
    }
    return true;
  }

  const bool checked_arithmetic =
      native_integer_is_checked_binary(value->binary_operator);
  const bool checked_shift =
      native_integer_is_checked_shift(value->binary_operator);
  const bool masked_shift =
      native_integer_is_masked_shift(value->binary_operator);
  const bool bitwise = native_integer_is_bitwise_binary(value->binary_operator);
  if (value->kind != (signed_carrier ? W_SEED_HIR0_VALUE_BINARY_I64
                                     : W_SEED_HIR0_VALUE_BINARY_U64) ||
      (!native_integer_is_wrapping_binary(value->binary_operator) &&
       !checked_arithmetic && !checked_shift && !masked_shift && !bitwise) ||
      (checked_shift &&
       !native_integer_checked_shift_shape_valid(program, value, NULL)) ||
      (masked_shift &&
       !native_integer_masked_shift_shape_valid(program, value, NULL)) ||
      (bitwise &&
       !native_integer_bitwise_shape_valid(program, value, signed_carrier)) ||
      value->left_value == W_SEED_HIR0_NONE ||
      value->right_value == W_SEED_HIR0_NONE)
    return false;

  uint64_t left = 0u;
  uint64_t right = 0u;
  if (!evaluate_integer_bits(program, value->left_value, depth + 1u, expected,
                             &left))
    return false;
  if (checked_shift || masked_shift ||
      value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_POWER ||
      value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_SHIFT_LEFT) {
    const native_integer_facts count_facts = {false, 64u};
    if (!evaluate_integer_bits(program, value->right_value, depth + 1u,
                               count_facts, &right))
      return false;
  } else if (!evaluate_integer_bits(program, value->right_value, depth + 1u,
                                   expected, &right)) {
    return false;
  }

  switch (value->binary_operator) {
    case W_SEED_HIR0_BINARY_ADD:
    case W_SEED_HIR0_BINARY_SUBTRACT:
    case W_SEED_HIR0_BINARY_MULTIPLY:
    case W_SEED_HIR0_BINARY_DIVIDE:
    case W_SEED_HIR0_BINARY_REMAINDER: {
      uint64_t carrier_result = 0u;
      if (!w_seed_scalar_evaluator0_checked_integer_arithmetic(
              value->binary_operator, expected.is_signed,
              expected.bit_width,
              native_integer_carrier_bits(left, expected),
              native_integer_carrier_bits(right, expected),
              &carrier_result))
        return false;
      *result = native_integer_mask_bits(carrier_result, expected);
      return true;
    }
    case W_SEED_HIR0_BINARY_BIT_AND:
      *result = (left & right) & mask;
      return true;
    case W_SEED_HIR0_BINARY_BIT_OR:
      *result = (left | right) & mask;
      return true;
    case W_SEED_HIR0_BINARY_BIT_XOR:
      *result = (left ^ right) & mask;
      return true;
    case W_SEED_HIR0_BINARY_WRAPPING_ADD:
      *result = (left + right) & mask;
      return true;
    case W_SEED_HIR0_BINARY_WRAPPING_SUBTRACT:
      *result = (left - right) & mask;
      return true;
    case W_SEED_HIR0_BINARY_WRAPPING_MULTIPLY:
      *result = (left * right) & mask;
      return true;
    case W_SEED_HIR0_BINARY_WRAPPING_POWER: {
      uint64_t accumulator = UINT64_C(1) & mask;
      uint64_t base = left & mask;
      uint64_t exponent = right;
      size_t steps = 0u;
      while (exponent != 0u && steps < 64u) {
        if ((exponent & UINT64_C(1)) != 0u)
          accumulator = (accumulator * base) & mask;
        exponent >>= 1u;
        if (exponent != 0u) base = (base * base) & mask;
        steps += 1u;
      }
      if (exponent != 0u) return false;
      *result = accumulator;
      return true;
    }
    case W_SEED_HIR0_BINARY_WRAPPING_SHIFT_LEFT:
      /* Do not mask the count: the selected policy traps when it reaches the
       * logical width, and checking first keeps every C shift defined. */
      if (right >= expected.bit_width) return false;
      *result = (left << right) & mask;
      return true;
    case W_SEED_HIR0_BINARY_SHIFT_LEFT:
    case W_SEED_HIR0_BINARY_SHIFT_RIGHT:
      return native_integer_checked_shift_bits(value->binary_operator,
                                               expected,
                                               native_integer_carrier_bits(
                                                   left, expected),
                                               right, result);
    case W_SEED_HIR0_BINARY_MASKED_SHIFT_LEFT:
    case W_SEED_HIR0_BINARY_MASKED_SHIFT_RIGHT:
    case W_SEED_HIR0_BINARY_LOGICAL_SHIFT_RIGHT:
      return native_integer_masked_shift_bits(value->binary_operator,
                                              expected, left, right, result);
    default:
      return false;
  }
}

/* Saturating power uses exponentiation by squaring without a policy-imposed
 * iteration ceiling. The exponent is shifted until it is zero; every exact
 * unsigned multiplication is checked before its result is retained. */
static bool saturating_u64_power(uint64_t base, uint64_t exponent,
                                 uint64_t *result) {
  if (result == NULL) return false;
  uint64_t accumulator = 1u;
  while (exponent != 0u) {
    if ((exponent & 1u) != 0u) {
      accumulator = accumulator != 0u && base > UINT64_MAX / accumulator
                        ? UINT64_MAX
                        : accumulator * base;
    }
    exponent >>= 1u;
    if (exponent != 0u)
      base = base != 0u && base > UINT64_MAX / base ? UINT64_MAX
                                                   : base * base;
  }
  *result = accumulator;
  return true;
}

static bool evaluate_u64(const w_seed_hir0_program *program,
                         uint32_t value_index, size_t depth,
                         uint64_t *result) {
  if (program == NULL || result == NULL || depth > 256u ||
      value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index >= program->type_count ||
      program->types[value->type_index].kind != W_SEED_HIR0_TYPE_U64)
    return false;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_U64) {
    *result = value->unsigned_integer_value;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_INTEGER_TRUNCATING_BITS ||
      value->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING) {
    native_integer_facts facts;
    return native_integer_type_facts(program, value->type_index, &facts) &&
           !facts.is_signed && facts.bit_width == 64u &&
           evaluate_integer_bits(program, value_index, depth, facts, result);
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_U64 &&
      native_integer_is_bit_primitive_unary(value->unary_operator)) {
    native_integer_facts facts;
    return native_integer_type_facts(program, value->type_index, &facts) &&
           native_integer_bit_primitive_shape_valid(program, value, NULL,
                                                    NULL) &&
           evaluate_integer_bits(program, value_index, depth, facts, result);
  }
  if (value->kind == W_SEED_HIR0_VALUE_BINARY_U64 &&
      native_integer_is_rotation_binary(value->binary_operator)) {
    native_integer_facts facts;
    return native_integer_type_facts(program, value->type_index, &facts) &&
           native_integer_rotation_shape_valid(program, value, NULL) &&
           evaluate_integer_bits(program, value_index, depth, facts, result);
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_U64) {
    uint64_t operand = 0u;
    if ((value->unary_operator != W_SEED_HIR0_UNARY_BIT_NOT &&
         value->unary_operator != W_SEED_HIR0_UNARY_WRAPPING_NEGATE &&
         value->unary_operator != W_SEED_HIR0_UNARY_SATURATING_NEGATE) ||
        value->left_value == W_SEED_HIR0_NONE ||
        !evaluate_u64(program, value->left_value, depth + 1u, &operand))
      return false;
    if (value->unary_operator == W_SEED_HIR0_UNARY_SATURATING_NEGATE) {
      *result = 0u;
    } else if (value->unary_operator == W_SEED_HIR0_UNARY_WRAPPING_NEGATE) {
      *result = 0u - operand;
    } else if (value->unary_operator == W_SEED_HIR0_UNARY_BIT_NOT) {
      *result = ~operand;
    } else {
      return false;
    }
    return true;
  }
  if (value->kind != W_SEED_HIR0_VALUE_BINARY_U64 ||
      (value->binary_operator > W_SEED_HIR0_BINARY_REMAINDER &&
        (value->binary_operator < W_SEED_HIR0_BINARY_BIT_AND ||
         (value->binary_operator > W_SEED_HIR0_BINARY_BIT_XOR &&
          value->binary_operator != W_SEED_HIR0_BINARY_WRAPPING_ADD &&
          value->binary_operator != W_SEED_HIR0_BINARY_WRAPPING_SUBTRACT &&
          value->binary_operator != W_SEED_HIR0_BINARY_WRAPPING_MULTIPLY &&
          value->binary_operator != W_SEED_HIR0_BINARY_WRAPPING_POWER &&
          value->binary_operator != W_SEED_HIR0_BINARY_SHIFT_LEFT &&
          value->binary_operator != W_SEED_HIR0_BINARY_SHIFT_RIGHT &&
          value->binary_operator != W_SEED_HIR0_BINARY_POWER &&
          value->binary_operator != W_SEED_HIR0_BINARY_WRAPPING_SHIFT_LEFT &&
          value->binary_operator != W_SEED_HIR0_BINARY_SATURATING_ADD &&
          value->binary_operator != W_SEED_HIR0_BINARY_SATURATING_SUBTRACT &&
          value->binary_operator != W_SEED_HIR0_BINARY_SATURATING_MULTIPLY &&
          value->binary_operator != W_SEED_HIR0_BINARY_SATURATING_POWER))))
    return false;
  uint64_t left = 0u;
  uint64_t right = 0u;
  if (!evaluate_u64(program, value->left_value, depth + 1u, &left) ||
      !evaluate_u64(program, value->right_value, depth + 1u, &right))
    return false;
  switch (value->binary_operator) {
    case W_SEED_HIR0_BINARY_ADD:
      return checked_u64_add(left, right, result);
    case W_SEED_HIR0_BINARY_WRAPPING_ADD:
      *result = left + right;
      return true;
    case W_SEED_HIR0_BINARY_SATURATING_ADD:
      *result = UINT64_MAX - left < right ? UINT64_MAX : left + right;
      return true;
    case W_SEED_HIR0_BINARY_SATURATING_SUBTRACT:
      *result = left < right ? 0u : left - right;
      return true;
    case W_SEED_HIR0_BINARY_SATURATING_MULTIPLY:
      /* Check before multiplying: unsigned overflow is defined in C, but W's
       * saturating result must never observe the wrapped product. */
      *result = right != 0u && left > UINT64_MAX / right
                    ? UINT64_MAX
                    : left * right;
      return true;
    case W_SEED_HIR0_BINARY_SATURATING_POWER:
      return saturating_u64_power(left, right, result);
    case W_SEED_HIR0_BINARY_WRAPPING_SUBTRACT:
      *result = left - right;
      return true;
    case W_SEED_HIR0_BINARY_WRAPPING_MULTIPLY:
      *result = left * right;
      return true;
    case W_SEED_HIR0_BINARY_WRAPPING_POWER:
      return wrapping_u64_power(left, right, result);
    case W_SEED_HIR0_BINARY_POWER:
      return checked_u64_power(left, right, result);
    case W_SEED_HIR0_BINARY_SHIFT_LEFT:
      if (right >= UINT64_C(64) ||
          (right != 0u && left > (UINT64_MAX >> right)))
        return false;
      *result = left << right;
      return true;
    case W_SEED_HIR0_BINARY_SHIFT_RIGHT:
      if (right >= UINT64_C(64)) return false;
      *result = left >> right;
      return true;
    case W_SEED_HIR0_BINARY_WRAPPING_SHIFT_LEFT:
      /* The count check must precede the shift: C shifts by 64 or more are
       * undefined, while W's wrapping policy traps for an invalid count. */
      if (right >= UINT64_C(64)) return false;
      *result = left << right;
      return true;
    case W_SEED_HIR0_BINARY_SUBTRACT:
      return checked_u64_subtract(left, right, result);
    case W_SEED_HIR0_BINARY_MULTIPLY:
      return checked_u64_multiply(left, right, result);
    case W_SEED_HIR0_BINARY_DIVIDE:
      if (right == 0u) return false;
      *result = left / right;
      return true;
    case W_SEED_HIR0_BINARY_REMAINDER:
      if (right == 0u) return false;
      *result = left % right;
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
  if (value->kind == W_SEED_HIR0_VALUE_INTEGER_TRUNCATING_BITS ||
      value->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING) {
    native_integer_facts facts;
    uint64_t bits = 0u;
    if (!native_integer_type_facts(program, value->type_index, &facts) ||
        !facts.is_signed || facts.bit_width != 64u ||
        !evaluate_integer_bits(program, value_index, depth, facts, &bits))
      return false;
    (void)memcpy(result, &bits, sizeof(*result));
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64) {
    int64_t operand = 0;
    if ((value->unary_operator != W_SEED_HIR0_UNARY_NEGATE &&
         value->unary_operator != W_SEED_HIR0_UNARY_BIT_NOT) ||
        !evaluate_i64(program, value->left_value, depth + 1u, &operand))
      return false;
    if (value->unary_operator == W_SEED_HIR0_UNARY_NEGATE) {
      if (operand == INT64_MIN) return false;
      *result = -operand;
    } else {
      *result = ~operand;
    }
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

/* A constant arithmetic tree is still checked here so the native selector
 * cannot turn a constant overflow into a runtime program. Binding reads are
 * intentionally not considered constant by this syntactic predicate; their
 * initializers are checked independently when the binding is selected. */
static bool program_value_is_constant_integer(
    const w_seed_hir0_program *program, uint32_t value_index, size_t depth);

static bool program_value_is_constant_i64(
    const w_seed_hir0_program *program, uint32_t value_index, size_t depth) {
  if (program == NULL || depth > 256u || value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index >= program->type_count ||
      program->types[value->type_index].kind != W_SEED_HIR0_TYPE_I64)
    return false;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_I64) return true;
  if (value->kind == W_SEED_HIR0_VALUE_INTEGER_TRUNCATING_BITS ||
      value->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING)
    return program_value_is_constant_integer(program, value_index,
                                             depth + 1u);
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64)
    return (value->unary_operator == W_SEED_HIR0_UNARY_NEGATE ||
            value->unary_operator == W_SEED_HIR0_UNARY_BIT_NOT) &&
           value->left_value != W_SEED_HIR0_NONE &&
           program_value_is_constant_i64(program, value->left_value,
                                         depth + 1u);
  if (value->kind != W_SEED_HIR0_VALUE_BINARY_I64 ||
      (value->binary_operator > W_SEED_HIR0_BINARY_REMAINDER &&
       (value->binary_operator < W_SEED_HIR0_BINARY_BIT_AND ||
        value->binary_operator > W_SEED_HIR0_BINARY_BIT_XOR)))
    return false;
  return program_value_is_constant_i64(program, value->left_value,
                                       depth + 1u) &&
         program_value_is_constant_i64(program, value->right_value,
                                       depth + 1u);
}

static bool program_value_is_constant_u64(
    const w_seed_hir0_program *program, uint32_t value_index, size_t depth) {
  if (program == NULL || depth > 256u || value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index >= program->type_count ||
      program->types[value->type_index].kind != W_SEED_HIR0_TYPE_U64)
    return false;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_U64) return true;
  if (value->kind == W_SEED_HIR0_VALUE_INTEGER_TRUNCATING_BITS ||
      value->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING)
    return program_value_is_constant_integer(program, value_index,
                                             depth + 1u);
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_U64 &&
      native_integer_is_bit_primitive_unary(value->unary_operator))
    return program_value_is_constant_integer(program, value_index,
                                             depth + 1u);
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_U64)
    return (value->unary_operator == W_SEED_HIR0_UNARY_BIT_NOT ||
            value->unary_operator == W_SEED_HIR0_UNARY_WRAPPING_NEGATE ||
            value->unary_operator == W_SEED_HIR0_UNARY_SATURATING_NEGATE) &&
           value->left_value != W_SEED_HIR0_NONE &&
           program_value_is_constant_u64(program, value->left_value,
                                         depth + 1u);
  if (value->kind == W_SEED_HIR0_VALUE_BINARY_U64 &&
      native_integer_is_rotation_binary(value->binary_operator))
    return program_value_is_constant_integer(program, value_index,
                                             depth + 1u);
  return value->kind == W_SEED_HIR0_VALUE_BINARY_U64 &&
         (value->binary_operator <= W_SEED_HIR0_BINARY_REMAINDER ||
          (value->binary_operator >= W_SEED_HIR0_BINARY_BIT_AND &&
           value->binary_operator <= W_SEED_HIR0_BINARY_BIT_XOR) ||
           value->binary_operator == W_SEED_HIR0_BINARY_SHIFT_LEFT ||
           value->binary_operator == W_SEED_HIR0_BINARY_SHIFT_RIGHT ||
           value->binary_operator == W_SEED_HIR0_BINARY_POWER ||
           value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_ADD ||
           value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_SUBTRACT ||
           value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_MULTIPLY ||
           value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_POWER ||
           value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_SHIFT_LEFT ||
           value->binary_operator == W_SEED_HIR0_BINARY_SATURATING_ADD ||
           value->binary_operator == W_SEED_HIR0_BINARY_SATURATING_SUBTRACT ||
           value->binary_operator == W_SEED_HIR0_BINARY_SATURATING_MULTIPLY ||
           value->binary_operator == W_SEED_HIR0_BINARY_SATURATING_POWER) &&
         program_value_is_constant_u64(program, value->left_value,
                                       depth + 1u) &&
          program_value_is_constant_u64(program, value->right_value,
                                        depth + 1u);
 }

static bool program_value_is_constant_integer(
    const w_seed_hir0_program *program, uint32_t value_index, size_t depth) {
  if (program == NULL || depth > 256u || value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  native_integer_facts facts;
  if (!native_integer_type_facts(program, value->type_index, &facts))
    return false;
  const w_seed_hir0_value_kind constant_kind =
      facts.is_signed ? W_SEED_HIR0_VALUE_CONST_I64
                      : W_SEED_HIR0_VALUE_CONST_U64;
  const w_seed_hir0_value_kind unary_kind =
      facts.is_signed ? W_SEED_HIR0_VALUE_UNARY_I64
                      : W_SEED_HIR0_VALUE_UNARY_U64;
  const w_seed_hir0_value_kind binary_kind =
      facts.is_signed ? W_SEED_HIR0_VALUE_BINARY_I64
                      : W_SEED_HIR0_VALUE_BINARY_U64;
  if (value->kind == constant_kind) return true;
  if (value->kind == W_SEED_HIR0_VALUE_INTEGER_TRUNCATING_BITS ||
      value->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING) {
    const bool saturating =
        value->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING;
    const w_seed_hir0_value_owner_kind owner_kind =
        saturating ? W_SEED_HIR0_VALUE_OWNER_INTEGER_SATURATING
                   : W_SEED_HIR0_VALUE_OWNER_INTEGER_TRUNCATING_BITS;
    native_integer_facts source_facts;
    native_integer_facts destination_facts;
    return value->source_type < program->type_count &&
           value->left_value != W_SEED_HIR0_NONE &&
           value->left_value < program->value_count &&
           value->right_value == W_SEED_HIR0_NONE &&
           native_integer_conversion_route(
               program, value->source_type, value->type_index, &source_facts,
               &destination_facts) &&
           native_integer_facts_equal(facts, destination_facts) &&
           program->values[value->left_value].type_index ==
               value->source_type &&
           program->values[value->left_value].owner_kind ==
               owner_kind &&
           program->values[value->left_value].owner_index == value_index &&
           program->values[value->left_value].owner_ordinal == 0u &&
           program_value_is_constant_integer(program, value->left_value,
                                             depth + 1u);
  }
  if (value->kind == unary_kind) {
    if (native_integer_is_bit_primitive_unary(value->unary_operator))
      return native_integer_bit_primitive_shape_valid(
                 program, value, NULL, NULL) &&
             program_value_is_constant_integer(program, value->left_value,
                                               depth + 1u);
    const bool ordinary =
        value->unary_operator == W_SEED_HIR0_UNARY_BIT_NOT ||
        (facts.is_signed && value->unary_operator == W_SEED_HIR0_UNARY_NEGATE);
    return (native_integer_is_wrapping_unary(value->unary_operator) ||
            ordinary) &&
           value->left_value != W_SEED_HIR0_NONE &&
           program_value_is_constant_integer(program, value->left_value,
                                              depth + 1u);
  }
  const bool bitwise =
      native_integer_is_bitwise_binary(value->binary_operator);
  const bool rotation =
      native_integer_is_rotation_binary(value->binary_operator);
  const bool shape_valid =
      rotation ? native_integer_rotation_shape_valid(program, value, NULL)
               : (bitwise ? native_integer_bitwise_shape_valid(
                                program, value, facts.is_signed)
                          : (native_integer_is_shift_policy(
                                 value->binary_operator)
                                 ? native_integer_shift_policy_shape_valid(
                                       program, value, NULL)
                                 : (native_integer_is_wrapping_binary(
                                        value->binary_operator) ||
                                    native_integer_is_checked_binary(
                                        value->binary_operator))));
  if (value->kind != binary_kind ||
      value->left_value == W_SEED_HIR0_NONE ||
      value->right_value == W_SEED_HIR0_NONE || !shape_valid)
    return false;
  return program_value_is_constant_integer(program, value->left_value,
                                           depth + 1u) &&
         program_value_is_constant_integer(program, value->right_value,
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

static uint32_t program_enum_carrier_width(size_t case_count) {
  size_t representable = 1u;
  uint32_t bits = 0u;
  /* Native0 owns at most 64 enum cases; MLIR0 consequently emits i1..i6. */
  if (case_count == 0u || case_count > 64u) return 0u;
  while (representable < case_count) {
    if (representable > SIZE_MAX / 2u || bits == UINT32_MAX) return 0u;
    representable *= 2u;
    bits += 1u;
  }
  return bits == 0u ? 1u : bits;
}

static bool program_enum_subset_case_at(
    const w_seed_hir0_program *program, uint32_t type_index, size_t ordinal,
    uint32_t *enum_case_index) {
  if (program == NULL || type_index >= program->type_count ||
      program->types[type_index].kind != W_SEED_HIR0_TYPE_ENUM_SUBSET ||
      ordinal >= program->types[type_index].subset_member_count)
    return false;
  const w_seed_hir0_type *type = &program->types[type_index];
  if (type->enum_index >= program->enum_count ||
      type->first_subset_member == W_SEED_HIR0_NONE ||
      (size_t)type->first_subset_member > program->enum_subset_member_count ||
      type->subset_member_count >
          program->enum_subset_member_count - type->first_subset_member)
    return false;
  const w_seed_hir0_enum *decl = &program->enums[type->enum_index];
  if (decl->type_index >= program->type_count ||
      program->types[decl->type_index].kind != W_SEED_HIR0_TYPE_ENUM ||
      decl->type_index == type_index || decl->case_count == 0u ||
      decl->first_case > program->enum_case_count ||
      decl->case_count > program->enum_case_count - decl->first_case)
    return false;
  const w_seed_hir0_enum_subset_member *member =
      &program->enum_subset_members[(size_t)type->first_subset_member + ordinal];
  if (member->owner_type != type_index || member->ordinal != ordinal ||
      member->enum_index != type->enum_index ||
      member->enum_case_index < decl->first_case ||
      (size_t)member->enum_case_index >=
          (size_t)decl->first_case + decl->case_count)
    return false;
  const w_seed_hir0_enum_case *item =
      &program->enum_cases[member->enum_case_index];
  const size_t base_ordinal =
      (size_t)member->enum_case_index - (size_t)decl->first_case;
  if (item->owner_enum != type->enum_index || item->ordinal != base_ordinal ||
      item->tag != base_ordinal)
    return false;
  if (ordinal != 0u) {
    const w_seed_hir0_enum_subset_member *previous =
        &program->enum_subset_members[(size_t)type->first_subset_member +
                                      ordinal - 1u];
    if (previous->enum_case_index >= member->enum_case_index) return false;
  }
  if (enum_case_index != NULL) *enum_case_index = member->enum_case_index;
  return true;
}

static bool program_enum_subset_contains_case(
    const w_seed_hir0_program *program, uint32_t type_index,
    uint32_t enum_case_index) {
  if (program == NULL || type_index >= program->type_count ||
      program->types[type_index].kind != W_SEED_HIR0_TYPE_ENUM_SUBSET)
    return false;
  const w_seed_hir0_type *type = &program->types[type_index];
  for (size_t ordinal = 0u; ordinal < type->subset_member_count; ordinal += 1u) {
    uint32_t candidate = W_SEED_HIR0_NONE;
    if (!program_enum_subset_case_at(program, type_index, ordinal, &candidate))
      return false;
    if (candidate == enum_case_index) return true;
  }
  return false;
}

static bool program_enum_type_supported(const w_seed_hir0_program *program,
                                        uint32_t type_index,
                                        uint32_t *enum_index,
                                        uint32_t *carrier_width) {
  if (program == NULL || type_index >= program->type_count ||
      (program->types[type_index].kind != W_SEED_HIR0_TYPE_ENUM &&
       program->types[type_index].kind != W_SEED_HIR0_TYPE_ENUM_SUBSET))
    return false;
  const bool subset =
      program->types[type_index].kind == W_SEED_HIR0_TYPE_ENUM_SUBSET;
  const uint32_t selected_enum = program->types[type_index].enum_index;
  if (selected_enum >= program->enum_count) return false;
  const w_seed_hir0_enum *decl = &program->enums[selected_enum];
  if (decl->type_index >= program->type_count ||
      program->types[decl->type_index].kind != W_SEED_HIR0_TYPE_ENUM ||
      (!subset && decl->type_index != type_index) ||
      (subset && decl->type_index == type_index) ||
      decl->case_count == 0u ||
      decl->first_case > program->enum_case_count ||
      decl->case_count > program->enum_case_count - decl->first_case)
    return false;
  for (size_t ordinal = 0u; ordinal < decl->case_count; ordinal += 1u) {
    const w_seed_hir0_enum_case *item =
        &program->enum_cases[(size_t)decl->first_case + ordinal];
    if (item->owner_enum != selected_enum || item->ordinal != ordinal ||
        item->tag != ordinal ||
        item->first_payload > program->enum_case_parameter_count ||
        item->payload_count >
            program->enum_case_parameter_count - item->first_payload)
      return false;
    if (subset && item->payload_count != 0u) return false;
    for (size_t slot = 0u; slot < item->payload_count; slot += 1u) {
      const uint32_t field_type = program->enum_case_parameters[
          (size_t)item->first_payload + slot].type_index;
      if (field_type >= program->type_count ||
          (program->types[field_type].kind != W_SEED_HIR0_TYPE_I64 &&
           program->types[field_type].kind != W_SEED_HIR0_TYPE_BOOL))
        return false;
    }
  }
  if (subset) {
    const w_seed_hir0_type *type = &program->types[type_index];
    if (type->first_subset_member == W_SEED_HIR0_NONE ||
        type->subset_member_count == 0u ||
        type->subset_member_count == decl->case_count ||
        (size_t)type->first_subset_member > program->enum_subset_member_count ||
        type->subset_member_count >
            program->enum_subset_member_count - type->first_subset_member)
      return false;
    for (size_t ordinal = 0u; ordinal < type->subset_member_count; ordinal += 1u)
      if (!program_enum_subset_case_at(program, type_index, ordinal, NULL))
        return false;
  } else if (program->types[type_index].first_subset_member != W_SEED_HIR0_NONE ||
             program->types[type_index].subset_member_count != 0u) {
    return false;
  }
  const uint32_t width = program_enum_carrier_width(decl->case_count);
  if (width == 0u) return false;
  if (enum_index != NULL) *enum_index = selected_enum;
  if (carrier_width != NULL) *carrier_width = width;
  return true;
}

static bool program_value_lowerable(const w_seed_hir0_program *program,
                                    uint32_t value_index,
                                    uint32_t owner_function,
                                    bool allow_string, size_t depth);

static bool process_value_lowerable(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t owner_function, const w_seed_native_subset0_process *process,
    bool allow_string, size_t depth);

static bool program_wrapping_integer_value_lowerable(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t owner_function, size_t depth) {
  if (program == NULL || depth > 256u || value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  native_integer_facts facts;
  if (!native_integer_type_facts(program, value->type_index, &facts))
    return false;
  const w_seed_hir0_value_kind unary_kind =
      facts.is_signed ? W_SEED_HIR0_VALUE_UNARY_I64
                      : W_SEED_HIR0_VALUE_UNARY_U64;
  const w_seed_hir0_value_kind binary_kind =
      facts.is_signed ? W_SEED_HIR0_VALUE_BINARY_I64
                      : W_SEED_HIR0_VALUE_BINARY_U64;
  if (value->kind == unary_kind) {
    if (!native_integer_is_wrapping_unary(value->unary_operator) ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->left_value >= program->value_count)
      return false;
    native_integer_facts operand_facts;
    if (!native_integer_type_facts(
            program, program->values[value->left_value].type_index,
            &operand_facts) ||
        !native_integer_facts_equal(facts, operand_facts) ||
        !program_value_lowerable(program, value->left_value, owner_function,
                                 false, depth + 1u))
      return false;
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, facts, &ignored))
        return false;
    }
    return true;
  }
  if (value->kind != binary_kind ||
      !native_integer_is_wrapping_binary(value->binary_operator) ||
      value->left_value == W_SEED_HIR0_NONE ||
      value->right_value == W_SEED_HIR0_NONE ||
      value->left_value >= program->value_count ||
      value->right_value >= program->value_count)
    return false;
  native_integer_facts left_facts;
  native_integer_facts right_facts;
  if (!native_integer_type_facts(
          program, program->values[value->left_value].type_index,
          &left_facts) ||
      !native_integer_type_facts(
          program, program->values[value->right_value].type_index,
          &right_facts) ||
      !native_integer_facts_equal(facts, left_facts))
    return false;
  const bool count_operand =
      value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_POWER ||
      value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_SHIFT_LEFT;
  const native_integer_facts count_facts = {false, 64u};
  if ((count_operand && !native_integer_facts_equal(right_facts, count_facts)) ||
      (!count_operand && !native_integer_facts_equal(facts, right_facts)) ||
      !program_value_lowerable(program, value->left_value, owner_function,
                               false, depth + 1u) ||
      !program_value_lowerable(program, value->right_value, owner_function,
                               false, depth + 1u))
    return false;
  if (program_value_is_constant_integer(program, value_index, 0u)) {
    uint64_t ignored = 0u;
    if (!evaluate_integer_bits(program, value_index, 0u, facts, &ignored))
      return false;
  }
  return true;
}

static bool interpolation_maximum_bytes(
    const w_seed_hir0_program *program, const w_seed_hir0_value *value,
    const w_seed_hir0_binding *const *bindings, size_t binding_count,
    size_t current_instruction, size_t *binding_reads,
    size_t *maximum_bytes, const w_seed_native_subset0_process *process) {
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
          parameter_read ||
          embedded->kind == W_SEED_HIR0_VALUE_BINDING_READ ||
          effective->kind == W_SEED_HIR0_VALUE_PARAMETER_READ ||
          effective->kind == W_SEED_HIR0_VALUE_CALL_RESULT ||
          effective->kind == W_SEED_HIR0_VALUE_UNARY_I64 ||
          effective->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
          effective->kind == W_SEED_HIR0_VALUE_BINARY_U64 ||
          effective->kind ==
              W_SEED_HIR0_VALUE_BINARY_INTEGER_COMPARISON ||
          effective->kind == W_SEED_HIR0_VALUE_UNARY_U64 ||
          effective->kind == W_SEED_HIR0_VALUE_INTEGER_WIDEN ||
          effective->kind ==
              W_SEED_HIR0_VALUE_INTEGER_TRUNCATING_BITS ||
          effective->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING ||
          effective->kind == W_SEED_HIR0_VALUE_TUPLE_ELEMENT ||
          effective->kind ==
              W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON ||
          effective->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER;
      switch (program->types[embedded->type_index].kind) {
        case W_SEED_HIR0_TYPE_I64: {
          if (!runtime_value) {
            const uint32_t effective_index =
                (uint32_t)(effective - program->values);
            if (program_value_is_constant_integer(program, effective_index,
                                                  0u)) {
              native_integer_facts facts = {true, 64u};
              uint64_t ignored = 0u;
              if (!evaluate_integer_bits(program, effective_index, 0u, facts,
                                         &ignored))
                return false;
            } else {
              int64_t ignored = 0;
              if (!evaluate_i64(program, effective_index, 0u, &ignored))
                return false;
            }
          } else if (process != NULL
                         ? !process_value_lowerable(
                               program,
                               (uint32_t)(effective - program->values),
                               program->blocks[program->instructions[current_instruction]
                                                  .owner_block]
                                   .owner_function,
                               process, false, 0u)
                         : !program_value_lowerable(
                               program,
                               (uint32_t)(effective - program->values),
                               program->blocks[program->instructions[current_instruction]
                                                  .owner_block]
                                   .owner_function,
                               false, 0u)) {
            return false;
          }
          bytes = 20u;
          break;
        }
        case W_SEED_HIR0_TYPE_U64:
          if (runtime_value) {
            if (process != NULL
                    ? !process_value_lowerable(
                          program,
                          (uint32_t)(effective - program->values),
                          program->blocks[program->instructions[current_instruction]
                                              .owner_block]
                              .owner_function,
                          process, false, 0u)
                    : !program_value_lowerable(
                          program,
                          (uint32_t)(effective - program->values),
                          program->blocks[program->instructions[current_instruction]
                                              .owner_block]
                              .owner_function,
                          false, 0u))
              return false;
          } else {
            const uint32_t effective_index =
                (uint32_t)(effective - program->values);
            native_integer_facts facts = {false, 64u};
            uint64_t ignored = 0u;
            if (!program_value_is_constant_integer(program, effective_index,
                                                   0u) ||
                !evaluate_integer_bits(program, effective_index, 0u, facts,
                                       &ignored))
              return false;
          }
          bytes = 20u;
          break;
        case W_SEED_HIR0_TYPE_INTEGER: {
          native_integer_facts facts;
          if (!native_integer_type_facts(program, embedded->type_index,
                                         &facts))
            return false;
          const uint32_t effective_index =
              (uint32_t)(effective - program->values);
          if (runtime_value) {
            if (process != NULL
                    ? !process_value_lowerable(
                          program, effective_index,
                          program->blocks[program->instructions[current_instruction]
                                             .owner_block]
                              .owner_function,
                          process, false, 0u)
                    : !program_value_lowerable(
                          program, effective_index,
                          program->blocks[program->instructions[current_instruction]
                                             .owner_block]
                              .owner_function,
                          false, 0u))
              return false;
          } else {
            uint64_t ignored = 0u;
            if (!program_value_is_constant_integer(program, effective_index,
                                                   0u) ||
                !evaluate_integer_bits(program, effective_index, 0u, facts,
                                       &ignored))
              return false;
          }
          bytes = 20u;
          break;
        }
        case W_SEED_HIR0_TYPE_USIZE:
          if (!runtime_value || process == NULL ||
              !process_value_lowerable(
                  program, (uint32_t)(effective - program->values),
                  program->blocks[program->instructions[current_instruction]
                                      .owner_block]
                      .owner_function,
                  process, false, 0u))
            return false;
          /* The current public executable target proves a 64-bit usize. */
          bytes = 20u;
          break;
        case W_SEED_HIR0_TYPE_BOOL:
          if (runtime_value &&
              (process != NULL
                   ? !process_value_lowerable(
                         program, (uint32_t)(effective - program->values),
                         program->blocks[program->instructions[current_instruction]
                                            .owner_block]
                             .owner_function,
                         process, false, 0u)
                   : !program_value_lowerable(
                         program, (uint32_t)(effective - program->values),
                         program->blocks[program->instructions[current_instruction]
                                            .owner_block]
                             .owner_function,
                         false, 0u)))
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

  if (program->cleanup_count != 0u || program->module_count != 1u ||
      program->function_count != 1u ||
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
               instruction_index, binding_reads, &maximum_payload_bytes,
               NULL))
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
  if (value->kind == W_SEED_HIR0_VALUE_INTEGER_WIDEN) {
    native_integer_facts source_facts;
    native_integer_facts destination_facts;
    if (value->source_type >= program->type_count ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->left_value >= program->value_count ||
        value->right_value != W_SEED_HIR0_NONE ||
        !native_integer_widening_route(
            program, value->source_type, value->type_index, &source_facts,
            &destination_facts) ||
        program->values[value->left_value].type_index != value->source_type)
      return false;
    (void)source_facts;
    (void)destination_facts;
    return program_value_lowerable(program, value->left_value, owner_function,
                                   false, depth + 1u);
  }
  if (value->kind == W_SEED_HIR0_VALUE_NUMERIC_WIDEN) {
    native_numeric_widening_facts facts;
    return native_numeric_widening_shape_valid(program, value_index, &facts) &&
           program_value_lowerable(program, value->left_value,
                                   owner_function, false, depth + 1u);
  }
  if (value->kind == W_SEED_HIR0_VALUE_FLOAT_FROM_BITS ||
      value->kind == W_SEED_HIR0_VALUE_FLOAT_TO_BITS) {
    native_float_bits_facts facts;
    return native_float_bits_shape_valid(program, value_index, &facts) &&
           program_value_lowerable(program, value->left_value,
                                   owner_function, false, depth + 1u);
  }
  if (value->kind == W_SEED_HIR0_VALUE_INTEGER_TRUNCATING_BITS ||
      value->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING) {
    const w_seed_hir0_value_owner_kind owner_kind =
        value->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING
            ? W_SEED_HIR0_VALUE_OWNER_INTEGER_SATURATING
            : W_SEED_HIR0_VALUE_OWNER_INTEGER_TRUNCATING_BITS;
    native_integer_facts source_facts;
    native_integer_facts destination_facts;
    if (value->source_type >= program->type_count ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->left_value >= program->value_count ||
        value->right_value != W_SEED_HIR0_NONE ||
        !native_integer_conversion_route(
            program, value->source_type, value->type_index, &source_facts,
            &destination_facts) ||
        program->values[value->left_value].type_index != value->source_type ||
        program->values[value->left_value].owner_kind !=
            owner_kind ||
        program->values[value->left_value].owner_index != value_index ||
        program->values[value->left_value].owner_ordinal != 0u)
      return false;
    (void)source_facts;
    (void)destination_facts;
    return program_value_lowerable(program, value->left_value, owner_function,
                                   false, depth + 1u);
  }
  if (value->kind == W_SEED_HIR0_VALUE_CONST_I64 ||
      value->kind == W_SEED_HIR0_VALUE_CONST_U64) {
    native_integer_facts facts;
    if (!native_integer_type_facts(program, value->type_index, &facts))
      return false;
    return (facts.is_signed && value->kind == W_SEED_HIR0_VALUE_CONST_I64) ||
           (!facts.is_signed && value->kind == W_SEED_HIR0_VALUE_CONST_U64);
  }
  if (value->kind == W_SEED_HIR0_VALUE_CONST_FLOAT) {
    if (type == W_SEED_HIR0_TYPE_F32)
      return (value->float_bits >> 32u) == 0u &&
             ((value->float_bits >> 23u) & UINT64_C(0xff)) !=
                 UINT64_C(0xff);
    if (type == W_SEED_HIR0_TYPE_F64)
      return ((value->float_bits >> 52u) & UINT64_C(0x7ff)) !=
             UINT64_C(0x7ff);
    return false;
  }
  if (value->kind == W_SEED_HIR0_VALUE_CONST_BOOL)
    return type == W_SEED_HIR0_TYPE_BOOL;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_STRING)
    return allow_string && type == W_SEED_HIR0_TYPE_STRING;
  if (value->kind == W_SEED_HIR0_VALUE_ENUM_CASE) {
    uint32_t enum_index = W_SEED_HIR0_NONE;
    if (!((type == W_SEED_HIR0_TYPE_ENUM ||
           type == W_SEED_HIR0_TYPE_ENUM_SUBSET) &&
           program_enum_type_supported(program, value->type_index,
                                       &enum_index, NULL) &&
           value->enum_index == enum_index &&
           value->enum_case_index < program->enum_case_count &&
           program->enum_cases[value->enum_case_index].owner_enum ==
               enum_index &&
           program->enum_cases[value->enum_case_index].payload_count ==
               value->enum_payload_count) ||
        value->first_enum_payload > program->enum_payload_count ||
        value->enum_payload_count >
            program->enum_payload_count - value->first_enum_payload)
      return false;
    if (type == W_SEED_HIR0_TYPE_ENUM_SUBSET &&
        !program_enum_subset_contains_case(program, value->type_index,
                                           value->enum_case_index))
      return false;
    for (size_t index = 0u; index < value->enum_payload_count; index += 1u)
      if (!program_value_lowerable(program, program->enum_payloads[
              (size_t)value->first_enum_payload + index].value_index,
              owner_function, false, depth + 1u))
        return false;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_PATTERN_CAPTURE_READ) {
    if ((type != W_SEED_HIR0_TYPE_I64 && type != W_SEED_HIR0_TYPE_INTEGER &&
         type != W_SEED_HIR0_TYPE_BOOL) ||
        value->pattern_capture_index >= program->switch_capture_count)
      return false;
    const w_seed_hir0_switch_capture *capture =
        &program->switch_captures[value->pattern_capture_index];
    return capture->owner_switch_edge < program->switch_edge_count &&
           program->switch_edges[capture->owner_switch_edge].target_block < program->block_count &&
           program->blocks[program->switch_edges[capture->owner_switch_edge].target_block]
               .owner_function == owner_function;
  }
  if (value->kind == W_SEED_HIR0_VALUE_PARAMETER_READ) {
    const bool scalar = type == W_SEED_HIR0_TYPE_I64 ||
                        type == W_SEED_HIR0_TYPE_U64 ||
                        type == W_SEED_HIR0_TYPE_INTEGER ||
                        type == W_SEED_HIR0_TYPE_F32 ||
                        type == W_SEED_HIR0_TYPE_F64 ||
                        type == W_SEED_HIR0_TYPE_BOOL;
    const bool enumeration =
        (type == W_SEED_HIR0_TYPE_ENUM ||
         type == W_SEED_HIR0_TYPE_ENUM_SUBSET) &&
        program_enum_type_supported(program, value->type_index, NULL, NULL);
    return (scalar || enumeration) &&
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
  if ((value->kind == W_SEED_HIR0_VALUE_UNARY_I64 ||
       value->kind == W_SEED_HIR0_VALUE_UNARY_U64) &&
      native_integer_is_bit_primitive_unary(value->unary_operator)) {
    native_integer_facts result_facts;
    if (!native_integer_bit_primitive_shape_valid(program, value, NULL,
                                                 &result_facts) ||
        !program_value_lowerable(program, value->left_value, owner_function,
                                 false, depth + 1u))
      return false;
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, result_facts,
                                 &ignored))
        return false;
    }
    return true;
  }
  if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
       value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
      native_integer_is_rotation_binary(value->binary_operator)) {
    native_integer_facts result_facts;
    if (!native_integer_rotation_shape_valid(program, value, &result_facts) ||
        !program_value_lowerable(program, value->left_value, owner_function,
                                 false, depth + 1u) ||
        !program_value_lowerable(program, value->right_value, owner_function,
                                 false, depth + 1u))
      return false;
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, result_facts,
                                 &ignored))
        return false;
    }
    return true;
  }
  if ((value->kind == W_SEED_HIR0_VALUE_UNARY_I64 &&
       value->unary_operator == W_SEED_HIR0_UNARY_WRAPPING_NEGATE) ||
      (value->kind == W_SEED_HIR0_VALUE_UNARY_U64 &&
       value->unary_operator == W_SEED_HIR0_UNARY_WRAPPING_NEGATE) ||
      (value->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
       native_integer_is_wrapping_binary(value->binary_operator)) ||
      (value->kind == W_SEED_HIR0_VALUE_BINARY_U64 &&
       native_integer_is_wrapping_binary(value->binary_operator)))
    return program_wrapping_integer_value_lowerable(
        program, value_index, owner_function, depth);
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64) {
    native_integer_facts ordinary_facts;
    native_integer_facts operand_facts;
    if (!native_integer_type_facts(program, value->type_index,
                                  &ordinary_facts) ||
        !ordinary_facts.is_signed ||
        (value->unary_operator != W_SEED_HIR0_UNARY_NEGATE &&
         value->unary_operator != W_SEED_HIR0_UNARY_BIT_NOT) ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->left_value >= program->value_count ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        !native_integer_type_facts(
            program, program->values[value->left_value].type_index,
            &operand_facts) ||
        !native_integer_facts_equal(ordinary_facts, operand_facts) ||
        !program_value_lowerable(program, value->left_value, owner_function,
                                 false, depth + 1u))
      return false;
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, ordinary_facts,
                                 &ignored))
        return false;
    }
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_U64) {
    native_integer_facts ordinary_facts;
    native_integer_facts operand_facts;
    if (value->unary_operator == W_SEED_HIR0_UNARY_BIT_NOT &&
        native_integer_type_facts(program, value->type_index,
                                  &ordinary_facts) &&
        !ordinary_facts.is_signed &&
        value->left_value != W_SEED_HIR0_NONE &&
        value->left_value < program->value_count &&
        value->right_value == W_SEED_HIR0_NONE &&
        value->binding_index == W_SEED_HIR0_NONE &&
        value->parameter_index == W_SEED_HIR0_NONE &&
        value->call_index == W_SEED_HIR0_NONE &&
        value->first_interpolation_segment == W_SEED_HIR0_NONE &&
        value->interpolation_segment_count == 0u &&
        value->binary_operator == W_SEED_HIR0_BINARY_ADD &&
        value->block_argument_index == W_SEED_HIR0_NONE &&
        native_integer_type_facts(
            program, program->values[value->left_value].type_index,
            &operand_facts) &&
        native_integer_facts_equal(ordinary_facts, operand_facts) &&
        program_value_lowerable(program, value->left_value, owner_function,
                                false, depth + 1u)) {
      if (program_value_is_constant_integer(program, value_index, 0u)) {
        uint64_t ignored = 0u;
        if (!evaluate_integer_bits(program, value_index, 0u, ordinary_facts,
                                   &ignored))
          return false;
      }
      return true;
    }
    const bool overflowing =
        value->unary_operator == W_SEED_HIR0_UNARY_OVERFLOWING_NEGATE;
    if (type != (overflowing ? W_SEED_HIR0_TYPE_U64_BOOL_TUPLE
                             : W_SEED_HIR0_TYPE_U64) ||
        (value->unary_operator != W_SEED_HIR0_UNARY_WRAPPING_NEGATE &&
         value->unary_operator != W_SEED_HIR0_UNARY_SATURATING_NEGATE &&
         !overflowing) ||
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
    if (!overflowing &&
        program_value_is_constant_u64(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_u64(program, value_index, 0u, &ignored)) return false;
    }
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_FLOAT)
    return (type == W_SEED_HIR0_TYPE_F32 ||
            type == W_SEED_HIR0_TYPE_F64) &&
           value->unary_operator == W_SEED_HIR0_UNARY_NEGATE &&
           value->left_value < program->value_count &&
           value->right_value == W_SEED_HIR0_NONE &&
           program->values[value->left_value].type_index ==
               value->type_index &&
           program_value_lowerable(program, value->left_value,
                                   owner_function, false, depth + 1u);
  if (value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ) {
    if ((type != W_SEED_HIR0_TYPE_I64 && type != W_SEED_HIR0_TYPE_U64 &&
         type != W_SEED_HIR0_TYPE_INTEGER && type != W_SEED_HIR0_TYPE_BOOL) ||
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
    if ((type != W_SEED_HIR0_TYPE_I64 && type != W_SEED_HIR0_TYPE_U64 &&
         type != W_SEED_HIR0_TYPE_INTEGER &&
         type != W_SEED_HIR0_TYPE_F32 &&
         type != W_SEED_HIR0_TYPE_F64 &&
         type != W_SEED_HIR0_TYPE_BOOL &&
         !program_enum_type_supported(program, value->type_index, NULL, NULL)) ||
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
  if (value->kind == W_SEED_HIR0_VALUE_TUPLE_ELEMENT) {
    if ((type != W_SEED_HIR0_TYPE_U64 && type != W_SEED_HIR0_TYPE_BOOL) ||
        value->left_value >= program->value_count ||
        value->unsigned_integer_value > 1u ||
        (value->unsigned_integer_value == 0u
             ? type != W_SEED_HIR0_TYPE_U64
             : type != W_SEED_HIR0_TYPE_BOOL))
      return false;
    const w_seed_hir0_value *tuple = &program->values[value->left_value];
    return tuple->type_index < program->type_count &&
           program->types[tuple->type_index].kind ==
               W_SEED_HIR0_TYPE_U64_BOOL_TUPLE &&
           program_value_lowerable(program, value->left_value, owner_function,
                                   false, depth + 1u);
  }
  if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
       value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
      native_integer_is_shift_policy(value->binary_operator)) {
    native_integer_facts result_facts;
    if (!native_integer_shift_policy_shape_valid(program, value,
                                                 &result_facts) ||
        !program_value_lowerable(program, value->left_value, owner_function,
                                 false, depth + 1u) ||
        !program_value_lowerable(program, value->right_value, owner_function,
                                 false, depth + 1u))
      return false;
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, result_facts,
                                 &ignored))
        return false;
    }
    return true;
  }

  if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
       value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
      native_integer_is_checked_binary(value->binary_operator)) {
    native_integer_facts result_facts;
    native_integer_facts left_facts;
    native_integer_facts right_facts;
    const bool expected_signed =
        value->kind == W_SEED_HIR0_VALUE_BINARY_I64;
    if (value->left_value >= program->value_count ||
        value->right_value >= program->value_count ||
        !native_integer_type_facts(program, value->type_index,
                                   &result_facts) ||
        result_facts.is_signed != expected_signed ||
        !native_integer_type_facts(
            program, program->values[value->left_value].type_index,
            &left_facts) ||
        !native_integer_type_facts(
            program, program->values[value->right_value].type_index,
            &right_facts) ||
        !native_integer_facts_equal(result_facts, left_facts) ||
        !native_integer_facts_equal(result_facts, right_facts) ||
        !program_value_lowerable(program, value->left_value, owner_function,
                                 false, depth + 1u) ||
        !program_value_lowerable(program, value->right_value, owner_function,
                                 false, depth + 1u))
      return false;
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, result_facts,
                                 &ignored))
        return false;
    }
    return true;
  }

  if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
       value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
      native_integer_is_bitwise_binary(value->binary_operator)) {
    native_integer_facts result_facts;
    const bool expected_signed =
        value->kind == W_SEED_HIR0_VALUE_BINARY_I64;
    if (!native_integer_bitwise_shape_valid(program, value,
                                            expected_signed) ||
        !native_integer_type_facts(program, value->type_index,
                                   &result_facts) ||
        !program_value_lowerable(program, value->left_value, owner_function,
                                 false, depth + 1u) ||
        !program_value_lowerable(program, value->right_value, owner_function,
                                 false, depth + 1u))
      return false;
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, result_facts,
                                 &ignored))
        return false;
    }
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_BINARY_INTEGER_COMPARISON) {
    return native_integer_comparison_shape_valid(program, value) &&
           program_value_lowerable(program, value->left_value,
                                   owner_function, false, depth + 1u) &&
           program_value_lowerable(program, value->right_value,
                                   owner_function, false, depth + 1u);
  }
  if (value->kind == W_SEED_HIR0_VALUE_BINARY_I64) {
    const bool shift =
        value->binary_operator == W_SEED_HIR0_BINARY_SHIFT_LEFT ||
        value->binary_operator == W_SEED_HIR0_BINARY_SHIFT_RIGHT;
    const bool power = value->binary_operator == W_SEED_HIR0_BINARY_POWER;
    if (value->left_value >= program->value_count ||
        value->right_value >= program->value_count ||
        program->values[value->left_value].type_index >= program->type_count ||
        program->values[value->right_value].type_index >= program->type_count ||
        ((shift || power)
             ? (program->types[program->values[value->left_value].type_index]
                        .kind != type ||
                program->types[program->values[value->right_value].type_index]
                        .kind != W_SEED_HIR0_TYPE_U64)
             : (program->types[program->values[value->left_value].type_index]
                        .kind != W_SEED_HIR0_TYPE_I64 ||
                program->types[program->values[value->right_value].type_index]
                        .kind != W_SEED_HIR0_TYPE_I64)))
      return false;
    if (value->binary_operator >= W_SEED_HIR0_BINARY_EQUAL &&
        value->binary_operator <= W_SEED_HIR0_BINARY_GREATER_EQUAL)
      return type == W_SEED_HIR0_TYPE_BOOL &&
             program_value_lowerable(program, value->left_value,
                                      owner_function, false, depth + 1u) &&
             program_value_lowerable(program, value->right_value,
                                      owner_function, false, depth + 1u);
    if ((!shift && !power &&
         value->binary_operator > W_SEED_HIR0_BINARY_REMAINDER &&
         (value->binary_operator < W_SEED_HIR0_BINARY_BIT_AND ||
          value->binary_operator > W_SEED_HIR0_BINARY_BIT_XOR)) ||
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
  if (value->kind == W_SEED_HIR0_VALUE_BINARY_U64) {
    const bool comparison =
        value->binary_operator >= W_SEED_HIR0_BINARY_EQUAL &&
        value->binary_operator <= W_SEED_HIR0_BINARY_GREATER_EQUAL;
    const bool arithmetic =
        value->binary_operator <= W_SEED_HIR0_BINARY_REMAINDER;
    const bool shift_or_power =
        value->binary_operator == W_SEED_HIR0_BINARY_SHIFT_LEFT ||
        value->binary_operator == W_SEED_HIR0_BINARY_SHIFT_RIGHT ||
        value->binary_operator == W_SEED_HIR0_BINARY_POWER;
    const bool wrapping =
        value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_ADD ||
        value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_SUBTRACT ||
        value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_MULTIPLY ||
        value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_POWER ||
        value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_SHIFT_LEFT ||
        value->binary_operator == W_SEED_HIR0_BINARY_SATURATING_ADD ||
        value->binary_operator == W_SEED_HIR0_BINARY_SATURATING_SUBTRACT ||
        value->binary_operator == W_SEED_HIR0_BINARY_SATURATING_MULTIPLY ||
        value->binary_operator == W_SEED_HIR0_BINARY_SATURATING_POWER;
    const bool overflowing =
        value->binary_operator == W_SEED_HIR0_BINARY_OVERFLOWING_ADD ||
        value->binary_operator == W_SEED_HIR0_BINARY_OVERFLOWING_SUBTRACT ||
        value->binary_operator == W_SEED_HIR0_BINARY_OVERFLOWING_MULTIPLY ||
        value->binary_operator == W_SEED_HIR0_BINARY_OVERFLOWING_POWER;
    const bool bitwise =
        value->binary_operator >= W_SEED_HIR0_BINARY_BIT_AND &&
        value->binary_operator <= W_SEED_HIR0_BINARY_BIT_XOR;
    if ((!arithmetic && !comparison && !bitwise && !wrapping &&
         !shift_or_power &&
         !overflowing) ||
        value->left_value >= program->value_count ||
        value->right_value >= program->value_count ||
        program->values[value->left_value].type_index >= program->type_count ||
        program->values[value->right_value].type_index >= program->type_count ||
        program->types[program->values[value->left_value].type_index].kind !=
            W_SEED_HIR0_TYPE_U64 ||
        program->types[program->values[value->right_value].type_index].kind !=
            W_SEED_HIR0_TYPE_U64 ||
        (comparison
             ? type != W_SEED_HIR0_TYPE_BOOL
             : (overflowing ? type != W_SEED_HIR0_TYPE_U64_BOOL_TUPLE
                            : type != W_SEED_HIR0_TYPE_U64)) ||
        !program_value_lowerable(program, value->left_value, owner_function,
                                 false, depth + 1u) ||
        !program_value_lowerable(program, value->right_value, owner_function,
                                 false, depth + 1u))
      return false;
    if ((arithmetic || bitwise || wrapping || shift_or_power) && !overflowing &&
        program_value_is_constant_u64(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_u64(program, value_index, 0u, &ignored)) return false;
    } else if (comparison) {
      /* A comparison may contain a constant arithmetic tree on either side;
       * evaluate each side independently so overflow and zero division still
       * fail closed even though the comparison itself is not arithmetic. */
      if (program_value_is_constant_u64(program, value->left_value, 0u)) {
        uint64_t ignored = 0u;
        if (!evaluate_u64(program, value->left_value, 0u, &ignored))
          return false;
      }
      if (program_value_is_constant_u64(program, value->right_value, 0u)) {
        uint64_t ignored = 0u;
        if (!evaluate_u64(program, value->right_value, 0u, &ignored))
          return false;
      }
    }
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_BINARY_FLOAT) {
    const bool comparison =
        value->binary_operator >= W_SEED_HIR0_BINARY_EQUAL &&
        value->binary_operator <= W_SEED_HIR0_BINARY_GREATER_EQUAL;
    const bool arithmetic =
        value->binary_operator >= W_SEED_HIR0_BINARY_ADD &&
        value->binary_operator <= W_SEED_HIR0_BINARY_DIVIDE;
    if (value->left_value >= program->value_count ||
        value->right_value >= program->value_count ||
        program->values[value->left_value].type_index >= program->type_count ||
        program->values[value->right_value].type_index >= program->type_count ||
        program->values[value->left_value].type_index !=
            program->values[value->right_value].type_index ||
        (program->types[program->values[value->left_value].type_index].kind !=
             W_SEED_HIR0_TYPE_F32 &&
         program->types[program->values[value->left_value].type_index].kind !=
             W_SEED_HIR0_TYPE_F64) ||
        (comparison ? type != W_SEED_HIR0_TYPE_BOOL
                    : (!arithmetic ||
                       value->type_index !=
                           program->values[value->left_value].type_index)))
      return false;
    return program_value_lowerable(program, value->left_value,
                                   owner_function, false, depth + 1u) &&
           program_value_lowerable(program, value->right_value,
                                   owner_function, false, depth + 1u);
  }
  return false;
}

/* Process executable values have two nominal leaves which the ordinary
 * scalar subset deliberately rejects: Args.isEmpty/count and ExitCode cases. Keep
 * this extension local to the selected process entry.  In particular, a
 * nominal value is never treated as an integer merely because it happens to
 * be passed in an ABI pointer register. */
static bool process_nominal_type_is(const w_seed_hir0_program *program,
                                    uint32_t type_index, uint32_t module_index,
                                    uint32_t symbol_index) {
  if (program == NULL || type_index >= program->type_count ||
      module_index >= program->external_module_count ||
      symbol_index >= program->external_symbol_count)
    return false;
  const w_seed_hir0_type *type = &program->types[type_index];
  return type->kind == W_SEED_HIR0_TYPE_NOMINAL &&
         type->external_module_index == module_index &&
         type->external_symbol_index == symbol_index;
}

static bool process_external_symbol_is(const w_seed_hir0_program *program,
                                       uint32_t module_index,
                                       uint32_t symbol_index,
                                       w_seed_hir0_external_kind kind,
                                       const uint8_t *name,
                                       size_t name_count) {
  if (program == NULL || name == NULL || module_index >=
          program->external_module_count ||
      symbol_index >= program->external_symbol_count)
    return false;
  const w_seed_hir0_external_symbol *symbol =
      &program->external_symbols[symbol_index];
  return symbol->module_index == module_index && symbol->kind == kind &&
         text_is(program, symbol->name, name, name_count);
}

static bool process_usize_count_comparison_operands(
    const w_seed_hir0_program *program, const w_seed_hir0_value *value,
    const w_seed_native_subset0_process *process) {
  if (program == NULL || value == NULL || process == NULL ||
      value->left_value >= program->value_count ||
      value->right_value >= program->value_count ||
      value->type_index >= program->type_count ||
      program->types[value->type_index].kind != W_SEED_HIR0_TYPE_BOOL)
    return false;
  const w_seed_hir0_value *left = &program->values[value->left_value];
  const w_seed_hir0_value *right = &program->values[value->right_value];
  const bool left_count =
      left->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
      left->external_module_index == 0u &&
      left->external_symbol_index == process->count_symbol_index &&
      left->type_index < program->type_count &&
      program->types[left->type_index].kind == W_SEED_HIR0_TYPE_USIZE &&
      text_is(program, left->member_name, (const uint8_t *)"count", 5u) &&
      process_external_symbol_is(
          program, 0u, process->count_symbol_index,
          W_SEED_HIR0_EXTERNAL_VALUE, (const uint8_t *)"count", 5u);
  const bool right_count =
      right->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
      right->external_module_index == 0u &&
      right->external_symbol_index == process->count_symbol_index &&
      right->type_index < program->type_count &&
      program->types[right->type_index].kind == W_SEED_HIR0_TYPE_USIZE &&
      text_is(program, right->member_name, (const uint8_t *)"count", 5u) &&
      process_external_symbol_is(
          program, 0u, process->count_symbol_index,
          W_SEED_HIR0_EXTERNAL_VALUE, (const uint8_t *)"count", 5u);
  const bool left_literal = left->kind == W_SEED_HIR0_VALUE_CONST_USIZE &&
                            left->type_index < program->type_count &&
                            program->types[left->type_index].kind ==
                                W_SEED_HIR0_TYPE_USIZE;
  const bool right_literal = right->kind == W_SEED_HIR0_VALUE_CONST_USIZE &&
                             right->type_index < program->type_count &&
                             program->types[right->type_index].kind ==
                                 W_SEED_HIR0_TYPE_USIZE;
  return (left_count && right_literal) || (right_count && left_literal);
}

static bool process_failure_constant(const w_seed_hir0_program *program,
                                     uint32_t value_index) {
  return program != NULL && value_index < program->value_count &&
         value_index < W_SEED_NATIVE_SUBSET0_MAX_VALUES &&
         program->values[value_index].kind == W_SEED_HIR0_VALUE_CONST_I64 &&
         program->values[value_index].integer_value >= 1 &&
         program->values[value_index].integer_value <= 255;
}

static bool process_value_lowerable(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t owner_function, const w_seed_native_subset0_process *process,
    bool allow_string, size_t depth);

static bool process_value_lowerable(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t owner_function, const w_seed_native_subset0_process *process,
    bool allow_string, size_t depth) {
  if (program == NULL || process == NULL || depth > 256u ||
      value_index >= program->value_count ||
      owner_function != process->function_index)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index >= program->type_count) return false;

  if (value->kind == W_SEED_HIR0_VALUE_INTEGER_WIDEN) {
    native_integer_facts source_facts;
    native_integer_facts destination_facts;
    if (value->source_type >= program->type_count ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->left_value >= program->value_count ||
        value->right_value != W_SEED_HIR0_NONE ||
        !native_integer_widening_route(
            program, value->source_type, value->type_index, &source_facts,
            &destination_facts) ||
        program->values[value->left_value].type_index != value->source_type)
      return false;
    (void)source_facts;
    (void)destination_facts;
    return process_value_lowerable(program, value->left_value, owner_function,
                                   process, false, depth + 1u);
  }

  if (value->kind == W_SEED_HIR0_VALUE_NUMERIC_WIDEN) {
    native_numeric_widening_facts facts;
    return native_numeric_widening_shape_valid(program, value_index, &facts) &&
           process_value_lowerable(program, value->left_value, owner_function,
                                   process, false, depth + 1u);
  }

  if (value->kind == W_SEED_HIR0_VALUE_FLOAT_FROM_BITS ||
      value->kind == W_SEED_HIR0_VALUE_FLOAT_TO_BITS) {
    native_float_bits_facts facts;
    return native_float_bits_shape_valid(program, value_index, &facts) &&
           process_value_lowerable(program, value->left_value, owner_function,
                                   process, false, depth + 1u);
  }

  if (value->kind == W_SEED_HIR0_VALUE_INTEGER_TRUNCATING_BITS ||
      value->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING) {
    const w_seed_hir0_value_owner_kind owner_kind =
        value->kind == W_SEED_HIR0_VALUE_INTEGER_SATURATING
            ? W_SEED_HIR0_VALUE_OWNER_INTEGER_SATURATING
            : W_SEED_HIR0_VALUE_OWNER_INTEGER_TRUNCATING_BITS;
    native_integer_facts source_facts;
    native_integer_facts destination_facts;
    if (value->source_type >= program->type_count ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->left_value >= program->value_count ||
        value->right_value != W_SEED_HIR0_NONE ||
        !native_integer_conversion_route(
            program, value->source_type, value->type_index, &source_facts,
            &destination_facts) ||
        program->values[value->left_value].type_index != value->source_type ||
        program->values[value->left_value].owner_kind !=
            owner_kind ||
        program->values[value->left_value].owner_index != value_index ||
        program->values[value->left_value].owner_ordinal != 0u)
      return false;
    (void)source_facts;
    (void)destination_facts;
    return process_value_lowerable(program, value->left_value, owner_function,
                                   process, false, depth + 1u);
  }

  if (value->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER) {
    const bool is_empty =
        value->external_symbol_index == process->is_empty_symbol_index &&
        program->types[value->type_index].kind == W_SEED_HIR0_TYPE_BOOL &&
        process_external_symbol_is(
            program, 0u, process->is_empty_symbol_index,
            W_SEED_HIR0_EXTERNAL_VALUE, (const uint8_t *)"isEmpty", 7u) &&
        text_is(program, value->member_name, (const uint8_t *)"isEmpty", 7u);
    const bool is_count =
        value->external_symbol_index == process->count_symbol_index &&
        program->types[value->type_index].kind == W_SEED_HIR0_TYPE_USIZE &&
        process_external_symbol_is(
            program, 0u, process->count_symbol_index,
            W_SEED_HIR0_EXTERNAL_VALUE, (const uint8_t *)"count", 5u) &&
        text_is(program, value->member_name, (const uint8_t *)"count", 5u);
    if ((!is_empty && !is_count) || value->external_module_index != 0u ||
        value->left_value >= program->value_count)
      return false;
    const w_seed_hir0_value *receiver = &program->values[value->left_value];
    return receiver->kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
           receiver->parameter_index ==
               process->function->first_parameter +
                   process->arguments_parameter_ordinal &&
           process_nominal_type_is(program, receiver->type_index, 0u,
                                   process->arguments_symbol_index);
  }

  if (value->kind == W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE) {
    if (!process_nominal_type_is(program, value->type_index, 0u,
                                 process->exit_code_symbol_index) ||
        value->external_module_index != 0u ||
        (value->left_value != W_SEED_HIR0_NONE &&
         value->left_value >= program->value_count))
      return false;
    if (value->external_symbol_index == process->success_symbol_index)
      return value->left_value == W_SEED_HIR0_NONE &&
             process_external_symbol_is(
                 program, 0u, process->success_symbol_index,
                 W_SEED_HIR0_EXTERNAL_VALUE, (const uint8_t *)"success", 7u) &&
             text_is(program, value->member_name, (const uint8_t *)"success",
                     7u);
    if (value->external_symbol_index == process->failure_symbol_index)
      return value->left_value != W_SEED_HIR0_NONE &&
             process_external_symbol_is(
                 program, 0u, process->failure_symbol_index,
                 W_SEED_HIR0_EXTERNAL_VALUE, (const uint8_t *)"failure", 7u) &&
             text_is(program, value->member_name, (const uint8_t *)"failure",
                     7u) &&
             process_failure_constant(program, value->left_value);
    return false;
  }

  if (value->kind == W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON) {
    if (value->binary_operator < W_SEED_HIR0_BINARY_EQUAL ||
        value->binary_operator > W_SEED_HIR0_BINARY_GREATER_EQUAL ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->right_value == W_SEED_HIR0_NONE ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u ||
        !process_usize_count_comparison_operands(program, value, process) ||
        !process_value_lowerable(program, value->left_value, owner_function,
                                 process, false, depth + 1u) ||
        !process_value_lowerable(program, value->right_value, owner_function,
                                 process, false, depth + 1u))
      return false;
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_BINARY_INTEGER_COMPARISON) {
    return native_integer_comparison_shape_valid(program, value) &&
           process_value_lowerable(program, value->left_value,
                                  owner_function, process, false,
                                  depth + 1u) &&
           process_value_lowerable(program, value->right_value,
                                  owner_function, process, false,
                                  depth + 1u);
  }

  if (value->kind == W_SEED_HIR0_VALUE_CONST_USIZE)
    return value->type_index < program->type_count &&
           program->types[value->type_index].kind == W_SEED_HIR0_TYPE_USIZE;

  /* Raw root-owner parameter reads are never lowerable as values.  The only
   * admitted use is the direct receiver checked in the external-member branch
   * above; this prevents binding, call arguments, enum payloads, or returns
   * from copying/escaping Arguments or Context. */
  if (value->kind == W_SEED_HIR0_VALUE_PARAMETER_READ) return false;

  if (value->kind == W_SEED_HIR0_VALUE_ENUM_CASE) {
    uint32_t enum_index = W_SEED_HIR0_NONE;
    if (!program_enum_type_supported(program, value->type_index, &enum_index,
                                     NULL) ||
        value->enum_index != enum_index ||
        value->enum_case_index >= program->enum_case_count ||
        program->enum_cases[value->enum_case_index].owner_enum != enum_index ||
        value->first_enum_payload > program->enum_payload_count ||
        value->enum_payload_count >
            program->enum_payload_count - value->first_enum_payload ||
        program->enum_cases[value->enum_case_index].payload_count !=
            value->enum_payload_count)
      return false;
    if (program->types[value->type_index].kind ==
            W_SEED_HIR0_TYPE_ENUM_SUBSET &&
        !program_enum_subset_contains_case(program, value->type_index,
                                           value->enum_case_index))
      return false;
    for (size_t ordinal = 0u; ordinal < value->enum_payload_count;
         ordinal += 1u)
      if (!process_value_lowerable(
              program,
              program->enum_payloads[(size_t)value->first_enum_payload +
                                     ordinal]
                  .value_index,
              owner_function, process, false, depth + 1u))
        return false;
    return true;
  }

  if ((value->kind == W_SEED_HIR0_VALUE_UNARY_I64 ||
       value->kind == W_SEED_HIR0_VALUE_UNARY_U64) &&
      native_integer_is_bit_primitive_unary(value->unary_operator)) {
    native_integer_facts result_facts;
    if (!native_integer_bit_primitive_shape_valid(program, value, NULL,
                                                 &result_facts) ||
        !process_value_lowerable(program, value->left_value, owner_function,
                                 process, false, depth + 1u))
      return false;
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, result_facts,
                                 &ignored))
        return false;
    }
    return true;
  }

  if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
       value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
      native_integer_is_rotation_binary(value->binary_operator)) {
    native_integer_facts result_facts;
    if (!native_integer_rotation_shape_valid(program, value, &result_facts) ||
        !process_value_lowerable(program, value->left_value, owner_function,
                                 process, false, depth + 1u) ||
        !process_value_lowerable(program, value->right_value, owner_function,
                                 process, false, depth + 1u))
      return false;
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, result_facts,
                                 &ignored))
        return false;
    }
    return true;
  }

  if ((value->kind == W_SEED_HIR0_VALUE_UNARY_I64 &&
       value->unary_operator == W_SEED_HIR0_UNARY_WRAPPING_NEGATE) ||
      (value->kind == W_SEED_HIR0_VALUE_UNARY_U64 &&
       value->unary_operator == W_SEED_HIR0_UNARY_WRAPPING_NEGATE) ||
      (value->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
       native_integer_is_wrapping_binary(value->binary_operator)) ||
      (value->kind == W_SEED_HIR0_VALUE_BINARY_U64 &&
       native_integer_is_wrapping_binary(value->binary_operator))) {
    native_integer_facts facts;
    if (!native_integer_type_facts(program, value->type_index, &facts))
      return false;
    const w_seed_hir0_value_kind unary_kind =
        facts.is_signed ? W_SEED_HIR0_VALUE_UNARY_I64
                        : W_SEED_HIR0_VALUE_UNARY_U64;
    const w_seed_hir0_value_kind binary_kind =
        facts.is_signed ? W_SEED_HIR0_VALUE_BINARY_I64
                        : W_SEED_HIR0_VALUE_BINARY_U64;
    if (value->kind == unary_kind) {
      native_integer_facts operand_facts;
      if (!native_integer_is_wrapping_unary(value->unary_operator) ||
          value->left_value == W_SEED_HIR0_NONE ||
          value->right_value != W_SEED_HIR0_NONE ||
          value->left_value >= program->value_count ||
          !native_integer_type_facts(
              program, program->values[value->left_value].type_index,
              &operand_facts) ||
          !native_integer_facts_equal(facts, operand_facts) ||
          !process_value_lowerable(program, value->left_value, owner_function,
                                   process, false, depth + 1u))
        return false;
    } else {
      native_integer_facts left_facts;
      native_integer_facts right_facts;
      if (value->kind != binary_kind ||
          !native_integer_is_wrapping_binary(value->binary_operator) ||
          value->left_value == W_SEED_HIR0_NONE ||
          value->right_value == W_SEED_HIR0_NONE ||
          value->left_value >= program->value_count ||
          value->right_value >= program->value_count ||
          !native_integer_type_facts(
              program, program->values[value->left_value].type_index,
              &left_facts) ||
          !native_integer_type_facts(
              program, program->values[value->right_value].type_index,
              &right_facts) ||
          !native_integer_facts_equal(facts, left_facts))
        return false;
      const bool count_operand =
          value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_POWER ||
          value->binary_operator == W_SEED_HIR0_BINARY_WRAPPING_SHIFT_LEFT;
      const native_integer_facts count_facts = {false, 64u};
      if ((count_operand &&
           !native_integer_facts_equal(right_facts, count_facts)) ||
          (!count_operand &&
           !native_integer_facts_equal(facts, right_facts)) ||
          !process_value_lowerable(program, value->left_value, owner_function,
                                   process, false, depth + 1u) ||
          !process_value_lowerable(program, value->right_value, owner_function,
                                   process, false, depth + 1u))
        return false;
    }
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, facts, &ignored))
        return false;
    }
    return true;
  }

  if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
       value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
      native_integer_is_shift_policy(value->binary_operator)) {
    native_integer_facts result_facts;
    if (!native_integer_shift_policy_shape_valid(program, value,
                                                 &result_facts) ||
        !process_value_lowerable(program, value->left_value, owner_function,
                                 process, false, depth + 1u) ||
        !process_value_lowerable(program, value->right_value, owner_function,
                                 process, false, depth + 1u))
      return false;
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, result_facts,
                                 &ignored))
        return false;
    }
    return true;
  }

  if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
       value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
      native_integer_is_checked_binary(value->binary_operator)) {
    native_integer_facts result_facts;
    native_integer_facts left_facts;
    native_integer_facts right_facts;
    const bool expected_signed =
        value->kind == W_SEED_HIR0_VALUE_BINARY_I64;
    if (value->left_value >= program->value_count ||
        value->right_value >= program->value_count ||
        !native_integer_type_facts(program, value->type_index,
                                   &result_facts) ||
        result_facts.is_signed != expected_signed ||
        !native_integer_type_facts(
            program, program->values[value->left_value].type_index,
            &left_facts) ||
        !native_integer_type_facts(
            program, program->values[value->right_value].type_index,
            &right_facts) ||
        !native_integer_facts_equal(result_facts, left_facts) ||
        !native_integer_facts_equal(result_facts, right_facts) ||
        !process_value_lowerable(program, value->left_value, owner_function,
                                 process, false, depth + 1u) ||
        !process_value_lowerable(program, value->right_value, owner_function,
                                 process, false, depth + 1u))
      return false;
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, result_facts,
                                 &ignored))
        return false;
    }
    return true;
  }

  if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
       value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
      native_integer_is_bitwise_binary(value->binary_operator)) {
    native_integer_facts result_facts;
    const bool expected_signed =
        value->kind == W_SEED_HIR0_VALUE_BINARY_I64;
    if (!native_integer_bitwise_shape_valid(program, value,
                                            expected_signed) ||
        !native_integer_type_facts(program, value->type_index,
                                   &result_facts) ||
        !process_value_lowerable(program, value->left_value, owner_function,
                                 process, false, depth + 1u) ||
        !process_value_lowerable(program, value->right_value, owner_function,
                                 process, false, depth + 1u))
      return false;
    if (program_value_is_constant_integer(program, value_index, 0u)) {
      uint64_t ignored = 0u;
      if (!evaluate_integer_bits(program, value_index, 0u, result_facts,
                                 &ignored))
        return false;
    }
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_UNARY_BOOL ||
      value->kind == W_SEED_HIR0_VALUE_UNARY_I64 ||
      value->kind == W_SEED_HIR0_VALUE_UNARY_U64 ||
      value->kind == W_SEED_HIR0_VALUE_BINARY_I64) {
    if (value->kind == W_SEED_HIR0_VALUE_UNARY_BOOL) {
      if (program->types[value->type_index].kind != W_SEED_HIR0_TYPE_BOOL ||
          value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
          value->left_value == W_SEED_HIR0_NONE ||
          value->right_value != W_SEED_HIR0_NONE ||
          value->binding_index != W_SEED_HIR0_NONE ||
          value->parameter_index != W_SEED_HIR0_NONE ||
          value->call_index != W_SEED_HIR0_NONE ||
          value->first_interpolation_segment != W_SEED_HIR0_NONE ||
          value->interpolation_segment_count != 0u ||
          value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
          value->block_argument_index != W_SEED_HIR0_NONE)
        return false;
      return process_value_lowerable(program, value->left_value, owner_function,
                                     process, false, depth + 1u);
    }
    if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64) {
      native_integer_facts ordinary_facts;
      native_integer_facts operand_facts;
      if (!native_integer_type_facts(program, value->type_index,
                                     &ordinary_facts) ||
          !ordinary_facts.is_signed ||
          (value->unary_operator != W_SEED_HIR0_UNARY_NEGATE &&
           value->unary_operator != W_SEED_HIR0_UNARY_BIT_NOT) ||
          value->left_value == W_SEED_HIR0_NONE ||
          value->left_value >= program->value_count ||
          value->right_value != W_SEED_HIR0_NONE ||
          value->binding_index != W_SEED_HIR0_NONE ||
          value->parameter_index != W_SEED_HIR0_NONE ||
          value->call_index != W_SEED_HIR0_NONE ||
          value->first_interpolation_segment != W_SEED_HIR0_NONE ||
          value->interpolation_segment_count != 0u ||
          value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
          value->block_argument_index != W_SEED_HIR0_NONE ||
          !native_integer_type_facts(
              program, program->values[value->left_value].type_index,
              &operand_facts) ||
          !native_integer_facts_equal(ordinary_facts, operand_facts) ||
          !process_value_lowerable(program, value->left_value, owner_function,
                                   process, false, depth + 1u))
        return false;
      if (program_value_is_constant_integer(program, value_index, 0u)) {
        uint64_t ignored = 0u;
        if (!evaluate_integer_bits(program, value_index, 0u, ordinary_facts,
                                   &ignored))
          return false;
      }
      return true;
    }
    if (value->kind == W_SEED_HIR0_VALUE_UNARY_U64) {
      native_integer_facts ordinary_facts;
      native_integer_facts operand_facts;
      if (value->unary_operator == W_SEED_HIR0_UNARY_BIT_NOT &&
          native_integer_type_facts(program, value->type_index,
                                    &ordinary_facts) &&
          !ordinary_facts.is_signed &&
          value->left_value != W_SEED_HIR0_NONE &&
          value->left_value < program->value_count &&
          value->right_value == W_SEED_HIR0_NONE &&
          value->binding_index == W_SEED_HIR0_NONE &&
          value->parameter_index == W_SEED_HIR0_NONE &&
          value->call_index == W_SEED_HIR0_NONE &&
          value->first_interpolation_segment == W_SEED_HIR0_NONE &&
          value->interpolation_segment_count == 0u &&
          value->binary_operator == W_SEED_HIR0_BINARY_ADD &&
          value->block_argument_index == W_SEED_HIR0_NONE &&
          native_integer_type_facts(
              program, program->values[value->left_value].type_index,
              &operand_facts) &&
          native_integer_facts_equal(ordinary_facts, operand_facts) &&
          process_value_lowerable(program, value->left_value,
                                  owner_function, process, false,
                                  depth + 1u)) {
        if (program_value_is_constant_integer(program, value_index, 0u)) {
          uint64_t ignored = 0u;
          if (!evaluate_integer_bits(program, value_index, 0u,
                                     ordinary_facts, &ignored))
            return false;
        }
        return true;
      }
      if (program->types[value->type_index].kind != W_SEED_HIR0_TYPE_U64 ||
          value->unary_operator != W_SEED_HIR0_UNARY_WRAPPING_NEGATE ||
          value->left_value == W_SEED_HIR0_NONE ||
          value->right_value != W_SEED_HIR0_NONE ||
          value->binding_index != W_SEED_HIR0_NONE ||
          value->parameter_index != W_SEED_HIR0_NONE ||
          value->call_index != W_SEED_HIR0_NONE ||
          value->first_interpolation_segment != W_SEED_HIR0_NONE ||
          value->interpolation_segment_count != 0u ||
          value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
          value->block_argument_index != W_SEED_HIR0_NONE ||
          !process_value_lowerable(program, value->left_value, owner_function,
                                   process, false, depth + 1u))
        return false;
      if (program_value_is_constant_u64(program, value_index, 0u)) {
        uint64_t ignored = 0u;
        if (!evaluate_u64(program, value_index, 0u, &ignored)) return false;
      }
      return true;
    }

    if (value->left_value >= program->value_count ||
        value->right_value >= program->value_count ||
        program->values[value->left_value].type_index >= program->type_count ||
        program->values[value->right_value].type_index >= program->type_count ||
        program->types[program->values[value->left_value].type_index].kind !=
            W_SEED_HIR0_TYPE_I64 ||
        program->types[program->values[value->right_value].type_index].kind !=
            W_SEED_HIR0_TYPE_I64 ||
        !process_value_lowerable(program, value->left_value, owner_function,
                                 process, false, depth + 1u) ||
        !process_value_lowerable(program, value->right_value, owner_function,
                                 process, false, depth + 1u))
      return false;
    if (value->binary_operator >= W_SEED_HIR0_BINARY_EQUAL &&
        value->binary_operator <= W_SEED_HIR0_BINARY_GREATER_EQUAL)
      return program->types[value->type_index].kind == W_SEED_HIR0_TYPE_BOOL;
    if ((value->binary_operator > W_SEED_HIR0_BINARY_REMAINDER &&
         (value->binary_operator < W_SEED_HIR0_BINARY_BIT_AND ||
          value->binary_operator > W_SEED_HIR0_BINARY_BIT_XOR)) ||
        program->types[value->type_index].kind != W_SEED_HIR0_TYPE_I64)
      return false;
    if (program_value_is_constant_i64(program, value_index, 0u)) {
      int64_t ignored = 0;
      if (!evaluate_i64(program, value_index, 0u, &ignored)) return false;
    }
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
    if (value->binding_index >= program->binding_count) return false;
    const w_seed_hir0_binding *binding =
        &program->bindings[value->binding_index];
    return binding->owner_block < program->block_count &&
           program->blocks[binding->owner_block].owner_function ==
               owner_function &&
           process_value_lowerable(program, binding->initializer_value,
                                   owner_function, process, allow_string,
                                   depth + 1u);
  }

  if (value->kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
    if (value->type_index >= program->type_count ||
        program->types[value->type_index].kind != W_SEED_HIR0_TYPE_STRING ||
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
      if (segment->kind == W_SEED_HIR0_INTERPOLATION_VALUE &&
          !process_value_lowerable(program, segment->value_index,
                                   owner_function, process, false,
                                   depth + 1u))
        return false;
      if (segment->kind != W_SEED_HIR0_INTERPOLATION_TEXT &&
          segment->kind != W_SEED_HIR0_INTERPOLATION_VALUE)
        return false;
    }
    return true;
  }

  /* All remaining leaves and local values use exactly the already-validated
   * scalar/enum rules.  This fallback intentionally rejects nominal values,
   * local external reads, and unknown value kinds. */
  return program_value_lowerable(program, value_index, owner_function,
                                 allow_string, depth);
}

static bool process_or_program_value_lowerable(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t owner_function, const w_seed_native_subset0_process *process,
    bool allow_string, size_t depth) {
  if (process != NULL && owner_function == process->function_index)
    return process_value_lowerable(program, value_index, owner_function, process,
                                   allow_string, depth);
  return program_value_lowerable(program, value_index, owner_function,
                                 allow_string, depth);
}

static bool process_host_call_supported(
    const w_seed_hir0_program *program, const w_seed_hir0_call *call,
    uint32_t owner_function,
    const w_seed_native_subset0_process *process);

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
      !native_scalar_type_supported(program, expected_type))
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
  if (!native_scalar_type_supported(program, expected_type))
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
        !native_scalar_type_supported(program, argument->type_index))
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
    if (!native_scalar_type_supported(program, expected_type) ||
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

/* The process entry may end in a terminal source-level if. HIR deliberately
 * elides that if's synthetic join because each arm exits directly, either by
 * returning ExitCode or by panicking. Keep this admission separate from the
 * ordinary diamond recognizer: the shared maximum walker still proves
 * forward-only edges and checks every value/body shape before emission. */
static bool program_process_terminal_return_cfg_is_supported(
    const w_seed_hir0_program *program, size_t function_index) {
  if (program == NULL || function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->block_count != 3u ||
      function->first_block >= program->block_count ||
      function->block_count > program->block_count - function->first_block)
    return false;
  const size_t start = function->first_block;
  const size_t first_arm = start + 1u;
  const size_t second_arm = start + 2u;
  const w_seed_hir0_block *entry = &program->blocks[start];
  const w_seed_hir0_block *then_block = &program->blocks[first_arm];
  const w_seed_hir0_block *else_block = &program->blocks[second_arm];
  if (entry->owner_function != function_index ||
      then_block->owner_function != function_index ||
      else_block->owner_function != function_index ||
      entry->terminator_index >= program->terminator_count ||
      then_block->terminator_index >= program->terminator_count ||
      else_block->terminator_index >= program->terminator_count ||
      then_block->block_argument_count != 0u ||
      else_block->block_argument_count != 0u)
    return false;
  const w_seed_hir0_terminator *branch =
      &program->terminators[entry->terminator_index];
  const w_seed_hir0_terminator *then_return =
      &program->terminators[then_block->terminator_index];
  const w_seed_hir0_terminator *else_return =
      &program->terminators[else_block->terminator_index];
  if (branch->owner_block != start ||
      branch->kind != W_SEED_HIR0_TERMINATOR_BRANCH ||
      branch->target_block != first_arm || branch->else_block != second_arm ||
      branch->target_block <= start || branch->else_block <= start ||
      then_return->owner_block != first_arm ||
      else_return->owner_block != second_arm)
    return false;
  const w_seed_hir0_terminator *arms[] = {then_return, else_return};
  for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
    const w_seed_hir0_terminator *terminator = arms[ordinal];
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_PANIC) {
      if (!native_panic_terminator_supported(
              program, (size_t)(terminator - program->terminators),
              (uint32_t)function_index, terminator))
        return false;
      continue;
    }
    if (terminator->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
        terminator->result_type != function->return_type ||
        terminator->value_index >= program->value_count ||
        terminator->target_block != W_SEED_HIR0_NONE ||
        terminator->else_block != W_SEED_HIR0_NONE ||
        terminator->first_edge_argument != W_SEED_HIR0_NONE ||
        terminator->edge_argument_count != 0u)
      return false;
  }
  return true;
}

static bool program_value_contains_block_argument(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t block_argument_index, size_t depth) {
  if (program == NULL || depth > 256u || value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ)
    return value->block_argument_index == block_argument_index;
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
    if (value->binding_index == W_SEED_HIR0_NONE ||
        value->binding_index >= program->binding_count)
      return false;
    const w_seed_hir0_binding *binding =
        &program->bindings[value->binding_index];
    return binding->initializer_value < program->value_count &&
           program_value_contains_block_argument(
               program, binding->initializer_value, block_argument_index,
               depth + 1u);
  }
  return (value->left_value != W_SEED_HIR0_NONE &&
          program_value_contains_block_argument(
              program, value->left_value, block_argument_index,
              depth + 1u)) ||
         (value->right_value != W_SEED_HIR0_NONE &&
          program_value_contains_block_argument(
              program, value->right_value, block_argument_index,
              depth + 1u));
}

static bool program_value_contains_any_block_argument(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t first_block_argument, size_t block_argument_count, size_t depth) {
  if (program == NULL || block_argument_count == 0u ||
      first_block_argument > program->block_argument_count ||
      block_argument_count >
          program->block_argument_count - first_block_argument)
    return false;
  for (size_t ordinal = 0u; ordinal < block_argument_count; ordinal += 1u)
    if (program_value_contains_block_argument(
            program, value_index, (uint32_t)((size_t)first_block_argument +
                                             ordinal),
            depth))
      return true;
  return false;
}

static bool program_value_contains_binding_read(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t binding_index, size_t depth) {
  if (program == NULL || depth > 256u || value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ)
    return value->binding_index == binding_index;
  return (value->left_value != W_SEED_HIR0_NONE &&
          program_value_contains_binding_read(program, value->left_value,
                                              binding_index, depth + 1u)) ||
         (value->right_value != W_SEED_HIR0_NONE &&
          program_value_contains_binding_read(program, value->right_value,
                                              binding_index, depth + 1u));
}

/* Keep an exit-side continuation narrower than the ordinary value emitter:
 * constants, i64 parameters, arithmetic, and reads of the loop's body
 * results only.  In particular, calls, unrelated block arguments, and
 * effects cannot be smuggled into the post-loop SSA operation by malformed
 * HIR. */
static bool program_natural_loop_continuation_value_ok(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t function_index, const uint32_t *result_bindings,
    uint32_t first_block_argument, size_t carrier_count, bool *uses_result,
    size_t depth) {
  if (program == NULL || result_bindings == NULL || uses_result == NULL ||
      carrier_count == 0u || depth > 256u || value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index >= program->type_count ||
      program->types[value->type_index].kind != W_SEED_HIR0_TYPE_I64)
    return false;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_I64) return true;
  if (value->kind == W_SEED_HIR0_VALUE_PARAMETER_READ)
    return value->parameter_index < program->parameter_count &&
           program->parameters[value->parameter_index].owner_function ==
               function_index;
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
    if (value->binding_index >= program->binding_count) return false;
    for (size_t lane = 0u; lane < carrier_count; lane += 1u)
      if (result_bindings[lane] == value->binding_index) {
        *uses_result = true;
        return true;
      }
    return false;
  }
  if (value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ) {
    if (value->block_argument_index == W_SEED_HIR0_NONE ||
        value->block_argument_index < first_block_argument ||
        (size_t)value->block_argument_index - first_block_argument >=
            carrier_count)
      return false;
    *uses_result = true;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64)
    return (value->unary_operator == W_SEED_HIR0_UNARY_NEGATE ||
            value->unary_operator == W_SEED_HIR0_UNARY_BIT_NOT) &&
           value->left_value != W_SEED_HIR0_NONE &&
           value->right_value == W_SEED_HIR0_NONE &&
           program_natural_loop_continuation_value_ok(
               program, value->left_value, function_index, result_bindings,
               first_block_argument, carrier_count, uses_result, depth + 1u);
  if (value->kind != W_SEED_HIR0_VALUE_BINARY_I64 ||
      value->binary_operator > W_SEED_HIR0_BINARY_REMAINDER ||
      value->left_value >= program->value_count ||
      value->right_value >= program->value_count)
    return false;
  return program_natural_loop_continuation_value_ok(
             program, value->left_value, function_index, result_bindings,
             first_block_argument, carrier_count, uses_result, depth + 1u) &&
         program_natural_loop_continuation_value_ok(
             program, value->right_value, function_index, result_bindings,
             first_block_argument, carrier_count, uses_result, depth + 1u);
}

/* HIR0 has already proved the exact bounded natural-loop invariants. This
 * selector repeats the backend-relevant boundary: four ordered blocks, a
 * nonempty tuple of carried i64 values, one tuple backedge, and only scalar
 * value trees the MLIR adapter can emit. It deliberately does not admit an
 * arbitrary cyclic CFG. */
static bool program_natural_loop_is_supported(
    const w_seed_hir0_program *program, size_t function_index) {
  if (program == NULL || function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->block_count != 4u || program->block_count < 4u ||
      function->first_block > program->block_count - 4u)
    return false;
  const size_t preheader_index = function->first_block;
  const size_t header_index = preheader_index + 1u;
  const size_t body_index = header_index + 1u;
  const size_t exit_index = body_index + 1u;
  const w_seed_hir0_block *preheader = &program->blocks[preheader_index];
  const w_seed_hir0_block *header = &program->blocks[header_index];
  const w_seed_hir0_block *body = &program->blocks[body_index];
  const w_seed_hir0_block *exit = &program->blocks[exit_index];
  const size_t carrier_count = header->block_argument_count;
  if (preheader->owner_function != function_index ||
      header->owner_function != function_index ||
      body->owner_function != function_index ||
      exit->owner_function != function_index || carrier_count == 0u ||
      carrier_count > UINT32_MAX ||
      header->first_block_argument == W_SEED_HIR0_NONE ||
      (size_t)header->first_block_argument >
          program->block_argument_count ||
      carrier_count > program->block_argument_count -
                           header->first_block_argument ||
      header->instruction_count != 0u ||
      body->instruction_count != carrier_count ||
      body->first_instruction == W_SEED_HIR0_NONE ||
      (size_t)body->first_instruction > program->instruction_count ||
      carrier_count > program->instruction_count - body->first_instruction ||
      exit->instruction_count > 1u ||
      (exit->instruction_count != 0u &&
       (exit->first_instruction == W_SEED_HIR0_NONE ||
        (size_t)exit->first_instruction > program->instruction_count ||
        exit->instruction_count >
            program->instruction_count - exit->first_instruction)) ||
      preheader->block_argument_count != 0u ||
      preheader->first_block_argument != W_SEED_HIR0_NONE ||
      body->block_argument_count != 0u ||
      body->first_block_argument != W_SEED_HIR0_NONE ||
      exit->block_argument_count != 0u ||
      exit->first_block_argument != W_SEED_HIR0_NONE ||
      preheader->terminator_index >= program->terminator_count ||
      header->terminator_index >= program->terminator_count ||
      body->terminator_index >= program->terminator_count ||
      exit->terminator_index >= program->terminator_count)
    return false;
  const w_seed_hir0_terminator *preheader_term =
      &program->terminators[preheader->terminator_index];
  const w_seed_hir0_terminator *header_term =
      &program->terminators[header->terminator_index];
  const w_seed_hir0_terminator *body_term =
      &program->terminators[body->terminator_index];
  const w_seed_hir0_terminator *exit_term =
      &program->terminators[exit->terminator_index];
  if (preheader_term->owner_block != preheader_index ||
      preheader_term->ordinal != preheader->instruction_count ||
      preheader_term->kind != W_SEED_HIR0_TERMINATOR_JUMP ||
      preheader_term->target_block != header_index ||
      preheader_term->else_block != W_SEED_HIR0_NONE ||
      preheader_term->value_index != W_SEED_HIR0_NONE ||
      preheader_term->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      preheader_term->edge_argument_count != carrier_count ||
      preheader_term->first_edge_argument == W_SEED_HIR0_NONE ||
      (size_t)preheader_term->first_edge_argument >
          program->edge_argument_count ||
      carrier_count > program->edge_argument_count -
                           preheader_term->first_edge_argument ||
      header_term->owner_block != header_index ||
      header_term->ordinal != header->instruction_count ||
      header_term->kind != W_SEED_HIR0_TERMINATOR_BRANCH ||
      header_term->target_block != body_index ||
      header_term->else_block != exit_index ||
      header_term->first_edge_argument != W_SEED_HIR0_NONE ||
      header_term->edge_argument_count != 0u ||
      header_term->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      header_term->value_index >= program->value_count ||
      body_term->owner_block != body_index ||
      body_term->ordinal != body->instruction_count ||
      body_term->kind != W_SEED_HIR0_TERMINATOR_JUMP ||
      body_term->target_block != header_index ||
      body_term->else_block != W_SEED_HIR0_NONE ||
      body_term->value_index != W_SEED_HIR0_NONE ||
      body_term->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      body_term->edge_argument_count != carrier_count ||
      body_term->first_edge_argument == W_SEED_HIR0_NONE ||
      (size_t)body_term->first_edge_argument > program->edge_argument_count ||
      carrier_count > program->edge_argument_count -
                           body_term->first_edge_argument ||
      exit_term->owner_block != exit_index ||
      exit_term->ordinal != exit->instruction_count ||
      exit_term->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      exit_term->target_block != W_SEED_HIR0_NONE ||
      exit_term->else_block != W_SEED_HIR0_NONE ||
      exit_term->first_edge_argument != W_SEED_HIR0_NONE ||
      exit_term->edge_argument_count != 0u ||
      exit_term->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      exit_term->value_index >= program->value_count ||
      !program_value_lowerable(program, header_term->value_index,
                               (uint32_t)function_index, false, 0u) ||
      !program_value_contains_any_block_argument(
          program, header_term->value_index, header->first_block_argument,
          carrier_count, 0u) ||
      !program_value_lowerable(program, exit_term->value_index,
                               (uint32_t)function_index, false, 0u))
    return false;
  uint32_t initial_bindings[W_SEED_NATIVE_SUBSET0_MAX_VALUES] = {0u};
  uint32_t result_bindings[W_SEED_NATIVE_SUBSET0_MAX_VALUES] = {0u};
  for (size_t ordinal = 0u; ordinal < carrier_count; ordinal += 1u) {
    const size_t argument_index =
        (size_t)header->first_block_argument + ordinal;
    const size_t initial_edge_index =
        (size_t)preheader_term->first_edge_argument + ordinal;
    const size_t back_edge_index =
        (size_t)body_term->first_edge_argument + ordinal;
    const w_seed_hir0_block_argument *argument =
        &program->block_arguments[argument_index];
    const w_seed_hir0_edge_argument *initial_edge =
        &program->edge_arguments[initial_edge_index];
    const w_seed_hir0_edge_argument *back_edge =
        &program->edge_arguments[back_edge_index];
    if (argument->owner_block != header_index || argument->ordinal != ordinal ||
        argument->type_index >= program->type_count ||
        program->types[argument->type_index].kind != W_SEED_HIR0_TYPE_I64 ||
        initial_edge->owner_terminator != preheader->terminator_index ||
        initial_edge->owner_block != preheader_index ||
        initial_edge->ordinal != ordinal ||
        initial_edge->type_index != argument->type_index ||
        initial_edge->value_index >= program->value_count ||
        program->values[initial_edge->value_index].kind !=
            W_SEED_HIR0_VALUE_BINDING_READ ||
        program->values[initial_edge->value_index].binding_index ==
            W_SEED_HIR0_NONE ||
        program->values[initial_edge->value_index].binding_index >=
            program->binding_count ||
        !program_value_lowerable(program, initial_edge->value_index,
                                 (uint32_t)function_index, false, 0u) ||
        back_edge->owner_terminator != body->terminator_index ||
        back_edge->owner_block != body_index ||
        back_edge->ordinal != ordinal ||
        back_edge->type_index != argument->type_index ||
        back_edge->value_index >= program->value_count ||
        program->values[back_edge->value_index].kind !=
            W_SEED_HIR0_VALUE_BINDING_READ ||
        !program_value_lowerable(program, back_edge->value_index,
                                 (uint32_t)function_index, false, 0u))
      return false;
    const uint32_t source_index =
        program->values[initial_edge->value_index].binding_index;
    if (ordinal != 0u) {
      const w_seed_hir0_edge_argument *previous_edge =
          &program->edge_arguments[initial_edge_index - 1u];
      if (previous_edge->value_index >= program->value_count ||
          program->values[previous_edge->value_index].kind !=
              W_SEED_HIR0_VALUE_BINDING_READ ||
          program->values[previous_edge->value_index].binding_index >=
              source_index)
        return false;
    }
    const w_seed_hir0_binding *source = &program->bindings[source_index];
    if (source->owner_instruction == W_SEED_HIR0_NONE ||
        source->owner_instruction >= program->instruction_count ||
        source->owner_block != preheader_index || !source->is_mutable ||
        source->type_index != argument->type_index ||
        source->source_binding != source_index ||
        source->previous_version != W_SEED_HIR0_NONE ||
        source->next_version == W_SEED_HIR0_NONE ||
        source->initializer_value >= program->value_count ||
        !program_value_lowerable(program, source->initializer_value,
                                 (uint32_t)function_index, false, 0u))
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
         instruction_ordinal < body->instruction_count;
         instruction_ordinal += 1u) {
      const size_t body_instruction_index =
          (size_t)body->first_instruction + instruction_ordinal;
      const w_seed_hir0_instruction *body_instruction =
          &program->instructions[body_instruction_index];
      if (body_instruction->owner_block != body_index ||
          body_instruction->ordinal != instruction_ordinal ||
          body_instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
          body_instruction->binding_index == W_SEED_HIR0_NONE ||
          body_instruction->binding_index >= program->binding_count)
        return false;
      const w_seed_hir0_binding *candidate =
          &program->bindings[body_instruction->binding_index];
      if (candidate->owner_instruction != body_instruction_index ||
          candidate->owner_block != body_index ||
          candidate->type_index != argument->type_index ||
          candidate->source_binding == W_SEED_HIR0_NONE)
        return false;
      if (candidate->source_binding == source_index) {
        matching_updates += 1u;
        update_index = body_instruction->binding_index;
      }
    }
    if (matching_updates != 1u || update_index == W_SEED_HIR0_NONE ||
        update_index >= program->binding_count)
      return false;
    const w_seed_hir0_binding *update = &program->bindings[update_index];
    if (!update->is_mutable || update->source_binding != source_index ||
        update->type_index != argument->type_index ||
        update->previous_version != source_index ||
        source->next_version != update_index ||
        update->initializer_value >= program->value_count ||
        !program_value_contains_any_block_argument(
            program, update->initializer_value,
            header->first_block_argument, carrier_count, 0u) ||
        !program_value_lowerable(program, update->initializer_value,
                                 (uint32_t)function_index, false, 0u) ||
        program->values[back_edge->value_index].binding_index != update_index)
      return false;
    initial_bindings[ordinal] = source_index;
    result_bindings[ordinal] = update_index;
  }
  uint32_t continuation_binding = W_SEED_HIR0_NONE;
  size_t continuation_lane = carrier_count;
  if (exit->instruction_count == 1u) {
    const w_seed_hir0_instruction *instruction =
        &program->instructions[exit->first_instruction];
    if (instruction->owner_block != exit_index || instruction->ordinal != 0u ||
        instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
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
        continuation->initializer_value >= program->value_count ||
        !program_value_lowerable(program, continuation->initializer_value,
                                 (uint32_t)function_index, false, 0u))
      return false;
    bool uses_result = false;
    if (!program_natural_loop_continuation_value_ok(
            program, continuation->initializer_value,
            (uint32_t)function_index, result_bindings,
            header->first_block_argument, carrier_count,
            &uses_result, 0u) ||
        !uses_result ||
        !program_value_contains_binding_read(
            program, exit_term->value_index, instruction->binding_index, 0u))
      return false;
    continuation_binding = instruction->binding_index;
  }
  for (size_t lane = 0u; lane < carrier_count; lane += 1u)
    if (program->bindings[result_bindings[lane]].next_version !=
        (lane == continuation_lane ? continuation_binding
                                    : W_SEED_HIR0_NONE))
      return false;
  return true;
}

/* HIR0 verification proves the source-level repeat restrictions.  Keep a
 * distinct NativeSubset0 projection for the five-block post-test shape so a
 * backend cannot accidentally route it through the four-block pre-test
 * natural-loop recognizer.  The latch carries updated values to the body;
 * the condition branch deliberately has no edge arguments. */
static bool program_post_test_loop_is_supported(
    const w_seed_hir0_program *program, size_t function_index) {
  if (program == NULL || function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->block_count != 5u || program->block_count < 5u ||
      function->first_block > program->block_count - 5u)
    return false;
  const size_t preheader_index = function->first_block;
  const size_t body_index = preheader_index + 1u;
  const size_t condition_index = body_index + 1u;
  const size_t latch_index = condition_index + 1u;
  const size_t exit_index = latch_index + 1u;
  const w_seed_hir0_block *preheader = &program->blocks[preheader_index];
  const w_seed_hir0_block *body = &program->blocks[body_index];
  const w_seed_hir0_block *condition = &program->blocks[condition_index];
  const w_seed_hir0_block *latch = &program->blocks[latch_index];
  const w_seed_hir0_block *exit = &program->blocks[exit_index];
  const size_t carrier_count = body->block_argument_count;
  if (preheader->owner_function != function_index ||
      body->owner_function != function_index ||
      condition->owner_function != function_index ||
      latch->owner_function != function_index ||
      exit->owner_function != function_index || carrier_count == 0u ||
      carrier_count > W_SEED_NATIVE_SUBSET0_MAX_VALUES ||
      body->first_block_argument == W_SEED_HIR0_NONE ||
      (size_t)body->first_block_argument > program->block_argument_count ||
      carrier_count >
          program->block_argument_count - body->first_block_argument ||
      preheader->instruction_count != carrier_count ||
      preheader->first_instruction == W_SEED_HIR0_NONE ||
      (size_t)preheader->first_instruction > program->instruction_count ||
      carrier_count >
          program->instruction_count - preheader->first_instruction ||
      body->instruction_count != carrier_count ||
      body->first_instruction == W_SEED_HIR0_NONE ||
      (size_t)body->first_instruction > program->instruction_count ||
      carrier_count > program->instruction_count - body->first_instruction ||
      condition->instruction_count != 0u ||
      latch->instruction_count != 0u || exit->instruction_count > 1u ||
      (exit->instruction_count != 0u &&
       (exit->first_instruction == W_SEED_HIR0_NONE ||
        (size_t)exit->first_instruction > program->instruction_count ||
        exit->instruction_count >
            program->instruction_count - exit->first_instruction)) ||
      preheader->block_argument_count != 0u ||
      preheader->first_block_argument != W_SEED_HIR0_NONE ||
      condition->block_argument_count != 0u ||
      condition->first_block_argument != W_SEED_HIR0_NONE ||
      latch->block_argument_count != 0u ||
      latch->first_block_argument != W_SEED_HIR0_NONE ||
      exit->block_argument_count != 0u ||
      exit->first_block_argument != W_SEED_HIR0_NONE ||
      preheader->terminator_index >= program->terminator_count ||
      body->terminator_index >= program->terminator_count ||
      condition->terminator_index >= program->terminator_count ||
      latch->terminator_index >= program->terminator_count ||
      exit->terminator_index >= program->terminator_count)
    return false;

  const w_seed_hir0_terminator *preheader_term =
      &program->terminators[preheader->terminator_index];
  const w_seed_hir0_terminator *body_term =
      &program->terminators[body->terminator_index];
  const w_seed_hir0_terminator *condition_term =
      &program->terminators[condition->terminator_index];
  const w_seed_hir0_terminator *latch_term =
      &program->terminators[latch->terminator_index];
  const w_seed_hir0_terminator *exit_term =
      &program->terminators[exit->terminator_index];
  if (preheader_term->owner_block != preheader_index ||
      preheader_term->ordinal != preheader->instruction_count ||
      preheader_term->kind != W_SEED_HIR0_TERMINATOR_JUMP ||
      preheader_term->target_block != body_index ||
      preheader_term->else_block != W_SEED_HIR0_NONE ||
      preheader_term->value_index != W_SEED_HIR0_NONE ||
      preheader_term->result_type != 0u ||
      preheader_term->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      preheader_term->edge_argument_count != carrier_count ||
      preheader_term->first_edge_argument == W_SEED_HIR0_NONE ||
      (size_t)preheader_term->first_edge_argument >
          program->edge_argument_count ||
      carrier_count > program->edge_argument_count -
                           preheader_term->first_edge_argument ||
      body_term->owner_block != body_index ||
      body_term->ordinal != body->instruction_count ||
      body_term->kind != W_SEED_HIR0_TERMINATOR_JUMP ||
      body_term->target_block != condition_index ||
      body_term->else_block != W_SEED_HIR0_NONE ||
      body_term->value_index != W_SEED_HIR0_NONE ||
      body_term->result_type != 0u ||
      body_term->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      body_term->edge_argument_count != 0u ||
      body_term->first_edge_argument != W_SEED_HIR0_NONE ||
      condition_term->owner_block != condition_index ||
      condition_term->ordinal != condition->instruction_count ||
      condition_term->kind != W_SEED_HIR0_TERMINATOR_BRANCH ||
      condition_term->target_block != latch_index ||
      condition_term->else_block != exit_index ||
      condition_term->result_type != 0u ||
      condition_term->first_edge_argument != W_SEED_HIR0_NONE ||
      condition_term->edge_argument_count != 0u ||
      condition_term->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      condition_term->value_index >= program->value_count ||
      latch_term->owner_block != latch_index ||
      latch_term->ordinal != latch->instruction_count ||
      latch_term->kind != W_SEED_HIR0_TERMINATOR_JUMP ||
      latch_term->target_block != body_index ||
      latch_term->else_block != W_SEED_HIR0_NONE ||
      latch_term->value_index != W_SEED_HIR0_NONE ||
      latch_term->result_type != 0u ||
      latch_term->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      latch_term->edge_argument_count != carrier_count ||
      latch_term->first_edge_argument == W_SEED_HIR0_NONE ||
      (size_t)latch_term->first_edge_argument >
          program->edge_argument_count ||
      carrier_count >
          program->edge_argument_count - latch_term->first_edge_argument ||
      exit_term->owner_block != exit_index ||
      exit_term->ordinal != exit->instruction_count ||
      exit_term->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      exit_term->target_block != W_SEED_HIR0_NONE ||
      exit_term->else_block != W_SEED_HIR0_NONE ||
      exit_term->first_edge_argument != W_SEED_HIR0_NONE ||
      exit_term->edge_argument_count != 0u ||
      exit_term->result_type != function->return_type ||
      exit_term->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      exit_term->value_index >= program->value_count ||
      function->return_type >= program->type_count ||
      program->types[function->return_type].kind != W_SEED_HIR0_TYPE_I64 ||
      program->types[program->values[condition_term->value_index].type_index]
              .kind != W_SEED_HIR0_TYPE_BOOL ||
      !program_value_lowerable(program, condition_term->value_index,
                               (uint32_t)function_index, false, 0u) ||
      !program_value_lowerable(program, exit_term->value_index,
                               (uint32_t)function_index, false, 0u))
    return false;

  uint32_t initial_bindings[W_SEED_NATIVE_SUBSET0_MAX_VALUES] = {0u};
  uint32_t result_bindings[W_SEED_NATIVE_SUBSET0_MAX_VALUES] = {0u};
  bool source_seen[W_SEED_NATIVE_SUBSET0_MAX_BINDINGS] = {false};
  bool update_seen[W_SEED_NATIVE_SUBSET0_MAX_BINDINGS] = {false};
  for (size_t ordinal = 0u; ordinal < carrier_count; ordinal += 1u) {
    const size_t argument_index =
        (size_t)body->first_block_argument + ordinal;
    const size_t initial_edge_index =
        (size_t)preheader_term->first_edge_argument + ordinal;
    const size_t latch_edge_index =
        (size_t)latch_term->first_edge_argument + ordinal;
    const w_seed_hir0_block_argument *argument =
        &program->block_arguments[argument_index];
    const w_seed_hir0_edge_argument *initial_edge =
        &program->edge_arguments[initial_edge_index];
    const w_seed_hir0_edge_argument *latch_edge =
        &program->edge_arguments[latch_edge_index];
    if (argument->owner_block != body_index || argument->ordinal != ordinal ||
        argument->type_index >= program->type_count ||
        program->types[argument->type_index].kind != W_SEED_HIR0_TYPE_I64 ||
        initial_edge->owner_terminator != preheader->terminator_index ||
        initial_edge->owner_block != preheader_index ||
        initial_edge->ordinal != ordinal ||
        initial_edge->type_index != argument->type_index ||
        initial_edge->value_index >= program->value_count ||
        program->values[initial_edge->value_index].kind !=
            W_SEED_HIR0_VALUE_BINDING_READ ||
        program->values[initial_edge->value_index].binding_index >=
            program->binding_count ||
        !program_value_lowerable(program, initial_edge->value_index,
                                 (uint32_t)function_index, false, 0u) ||
        latch_edge->owner_terminator != latch->terminator_index ||
        latch_edge->owner_block != latch_index ||
        latch_edge->ordinal != ordinal ||
        latch_edge->type_index != argument->type_index ||
        latch_edge->value_index >= program->value_count ||
        program->values[latch_edge->value_index].kind !=
            W_SEED_HIR0_VALUE_BINDING_READ ||
        program->values[latch_edge->value_index].binding_index >=
            program->binding_count ||
        !program_value_lowerable(program, latch_edge->value_index,
                                 (uint32_t)function_index, false, 0u))
      return false;
    const uint32_t source_index =
        program->values[initial_edge->value_index].binding_index;
    const uint32_t update_index =
        program->values[latch_edge->value_index].binding_index;
    if (source_index >= W_SEED_NATIVE_SUBSET0_MAX_BINDINGS ||
        update_index >= W_SEED_NATIVE_SUBSET0_MAX_BINDINGS ||
        source_seen[source_index] || update_seen[update_index])
      return false;
    source_seen[source_index] = true;
    update_seen[update_index] = true;
    const w_seed_hir0_binding *source = &program->bindings[source_index];
    const w_seed_hir0_binding *update = &program->bindings[update_index];
    if (source->owner_block != preheader_index || !source->is_mutable ||
        source->source_binding != source_index ||
        source->previous_version != W_SEED_HIR0_NONE ||
        source->next_version == W_SEED_HIR0_NONE ||
        source->type_index != argument->type_index ||
        source->owner_instruction == W_SEED_HIR0_NONE ||
        source->owner_instruction >= program->instruction_count ||
        source->initializer_value >= program->value_count ||
        !program_value_lowerable(program, source->initializer_value,
                                 (uint32_t)function_index, false, 0u) ||
        program->instructions[source->owner_instruction].owner_block !=
            preheader_index ||
        program->instructions[source->owner_instruction].kind !=
            W_SEED_HIR0_INSTRUCTION_BINDING ||
        program->instructions[source->owner_instruction].binding_index !=
            source_index ||
        update->owner_block != body_index || !update->is_mutable ||
        update->source_binding != source_index ||
        update->previous_version != source_index ||
        update->type_index != argument->type_index ||
        update->owner_instruction == W_SEED_HIR0_NONE ||
        update->owner_instruction >= program->instruction_count ||
        update->initializer_value >= program->value_count ||
        !program_value_contains_any_block_argument(
            program, update->initializer_value, body->first_block_argument,
            carrier_count, 0u) ||
        !program_value_lowerable(program, update->initializer_value,
                                 (uint32_t)function_index, false, 0u) ||
        (program->bindings[update_index].next_version != W_SEED_HIR0_NONE &&
         program->bindings[update_index].next_version >=
             program->binding_count))
      return false;
    initial_bindings[ordinal] = source_index;
    result_bindings[ordinal] = update_index;
  }
  for (size_t ordinal = 0u; ordinal < body->instruction_count; ordinal += 1u) {
    const size_t instruction_index =
        (size_t)body->first_instruction + ordinal;
    const w_seed_hir0_instruction *instruction =
        &program->instructions[instruction_index];
    if (instruction->owner_block != body_index ||
        instruction->ordinal != ordinal ||
        instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
        instruction->binding_index >= program->binding_count)
      return false;
    bool found = false;
    for (size_t lane = 0u; lane < carrier_count; lane += 1u)
      if (result_bindings[lane] == instruction->binding_index) found = true;
    if (!found) return false;
  }
  bool condition_uses_update = false;
  for (size_t lane = 0u; lane < carrier_count; lane += 1u)
    if (program_value_contains_binding_read(
            program, condition_term->value_index, result_bindings[lane], 0u))
      condition_uses_update = true;
  if (!condition_uses_update) return false;

  uint32_t continuation_binding = W_SEED_HIR0_NONE;
  size_t continuation_lane = carrier_count;
  if (exit->instruction_count == 1u) {
    const w_seed_hir0_instruction *instruction =
        &program->instructions[exit->first_instruction];
    if (instruction->owner_block != exit_index || instruction->ordinal != 0u ||
        instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
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
        continuation->initializer_value >= program->value_count ||
        !program_value_lowerable(program, continuation->initializer_value,
                                 (uint32_t)function_index, false, 0u))
      return false;
    bool uses_result = false;
    if (!program_natural_loop_continuation_value_ok(
            program, continuation->initializer_value,
            (uint32_t)function_index, result_bindings,
            body->first_block_argument, carrier_count, &uses_result, 0u) ||
        !uses_result ||
        !program_value_contains_binding_read(
            program, exit_term->value_index, instruction->binding_index, 0u))
      return false;
    continuation_binding = instruction->binding_index;
  }
  for (size_t lane = 0u; lane < carrier_count; lane += 1u)
    if (program->bindings[result_bindings[lane]].next_version !=
        (lane == continuation_lane ? continuation_binding
                                    : W_SEED_HIR0_NONE))
      return false;
  return true;
}

/* The only admitted enum CFG is the HIR21 dense dispatch followed by one
 * return block per canonical payloadless case.  Keep this proof independent
 * from the scalar-diamond recognizer so a forged switch cannot be treated as
 * an ordinary branch. */
static bool program_enum_switch_is_supported(
    const w_seed_hir0_program *program, size_t function_index) {
  if (program == NULL || function_index >= program->function_count) return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->block_count < 2u || function->first_block >= program->block_count ||
      function->block_count > program->block_count - function->first_block)
    return false;
  const size_t dispatch_index = function->first_block;
  const w_seed_hir0_block *dispatch_block = &program->blocks[dispatch_index];
  if (dispatch_block->owner_function != function_index ||
      dispatch_block->terminator_index >= program->terminator_count)
    return false;
  const w_seed_hir0_terminator *dispatch =
      &program->terminators[dispatch_block->terminator_index];
  if (dispatch->kind != W_SEED_HIR0_TERMINATOR_SWITCH_ENUM ||
      dispatch->owner_block != dispatch_index ||
      dispatch->switch_edge_count == 0u ||
      (size_t)dispatch->switch_edge_count + 1u != function->block_count ||
      dispatch->switch_enum_index >= program->enum_count ||
      dispatch->value_index >= program->value_count ||
      dispatch->first_switch_edge == W_SEED_HIR0_NONE ||
      (size_t)dispatch->first_switch_edge > program->switch_edge_count ||
      dispatch->switch_edge_count >
          program->switch_edge_count - dispatch->first_switch_edge)
    return false;
  uint32_t subject_enum = W_SEED_HIR0_NONE;
  uint32_t carrier_width = 0u;
  const w_seed_hir0_value *subject = &program->values[dispatch->value_index];
  if (!program_enum_type_supported(program, subject->type_index, &subject_enum,
                                   &carrier_width) ||
      subject_enum != dispatch->switch_enum_index ||
      dispatch->switch_carrier_width != carrier_width)
    return false;
  const w_seed_hir0_enum *decl = &program->enums[dispatch->switch_enum_index];
  const size_t end = dispatch_index + function->block_count;
  if (end > program->block_count) return false;
  const w_seed_hir0_type *subject_type = &program->types[subject->type_index];
  const size_t expected_edge_count =
      subject_type->kind == W_SEED_HIR0_TYPE_ENUM_SUBSET
          ? subject_type->subset_member_count
          : decl->case_count;
  if (expected_edge_count == 0u ||
      expected_edge_count != dispatch->switch_edge_count)
    return false;
  for (size_t ordinal = 0u; ordinal < dispatch->switch_edge_count; ordinal += 1u) {
    const w_seed_hir0_switch_edge *edge =
        &program->switch_edges[(size_t)dispatch->first_switch_edge + ordinal];
    const size_t target = dispatch_index + 1u + ordinal;
    uint32_t expected_case_index = (uint32_t)((size_t)decl->first_case + ordinal);
    if (subject_type->kind == W_SEED_HIR0_TYPE_ENUM_SUBSET &&
        !program_enum_subset_case_at(program, subject->type_index, ordinal,
                                     &expected_case_index))
      return false;
    if (edge->owner_terminator != dispatch_block->terminator_index ||
        edge->ordinal != ordinal || edge->enum_index != dispatch->switch_enum_index ||
        edge->enum_case_index != expected_case_index ||
        edge->target_block != target || target >= end ||
        program->blocks[target].owner_function != function_index ||
        program->blocks[target].block_argument_count != 0u ||
        program->blocks[target].terminator_index >= program->terminator_count)
      return false;
    const w_seed_hir0_terminator *arm =
        &program->terminators[program->blocks[target].terminator_index];
    if (arm->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
        arm->owner_block != target || arm->target_block != W_SEED_HIR0_NONE ||
        arm->else_block != W_SEED_HIR0_NONE ||
        arm->first_edge_argument != W_SEED_HIR0_NONE ||
        arm->edge_argument_count != 0u || arm->result_type != function->return_type ||
        arm->value_index >= program->value_count)
      return false;
  }
  return true;
}

static bool program_host_print_maximum(
    const w_seed_hir0_program *program, const w_seed_hir0_call *call,
    const w_seed_hir0_binding *const *bindings, size_t *binding_reads,
    size_t *maximum, bool *has_interpolation,
    const w_seed_native_subset0_process *process) {
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
            call->owner_instruction, binding_reads, &payload, process))
      return false;
    *has_interpolation = true;
  } else if (!interpolation_string_bytes(
                 program, root, bindings, program->binding_count,
                 call->owner_instruction, binding_reads, &payload)) {
    return false;
  }
  return sequence_stdout_add(0u, payload, maximum);
}


static bool program_function_has_static_yields(
    const w_seed_hir0_program *program, size_t function_index) {
  if (program == NULL || function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (!function->is_async || function->is_const || function->is_throws ||
      function->is_unsafe || function->has_borrow_clause ||
      function->is_anonymous_entry ||
      function->direct_entry != W_SEED_HIR0_DIRECT_ENTRY_ABSENT ||
      function->block_count != 1u ||
      function->first_block >= program->block_count)
    return false;
  if (function->return_type >= program->type_count ||
      (program->types[function->return_type].kind != W_SEED_HIR0_TYPE_I64 &&
       program->types[function->return_type].kind != W_SEED_HIR0_TYPE_BOOL))
    return false;
  for (size_t ordinal = 0u; ordinal < function->parameter_count;
       ordinal += 1u) {
    const size_t parameter_index =
        (size_t)function->first_parameter + ordinal;
    if (parameter_index >= program->parameter_count) return false;
    const uint32_t type_index = program->parameters[parameter_index].type_index;
    if (type_index >= program->type_count ||
        (program->types[type_index].kind != W_SEED_HIR0_TYPE_I64 &&
         program->types[type_index].kind != W_SEED_HIR0_TYPE_BOOL))
      return false;
  }
  const w_seed_hir0_block *block = &program->blocks[function->first_block];
  if (block->owner_function != function_index ||
      block->first_instruction > program->instruction_count ||
      block->instruction_count >
          program->instruction_count - block->first_instruction)
    return false;
  size_t yields = 0u;
  for (size_t ordinal = 0u; ordinal < block->instruction_count; ordinal += 1u) {
    const w_seed_hir0_instruction *instruction =
        &program->instructions[(size_t)block->first_instruction + ordinal];
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD)
      yields += 1u;
    else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= program->binding_count ||
          program->bindings[instruction->binding_index].type_index >=
              program->type_count ||
          (program->types[program->bindings[instruction->binding_index]
                              .type_index]
                   .kind != W_SEED_HIR0_TYPE_I64 &&
           program->types[program->bindings[instruction->binding_index]
                              .type_index]
                   .kind != W_SEED_HIR0_TYPE_BOOL))
        return false;
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (instruction->call_index >= program->call_count) return false;
      const w_seed_hir0_call *call =
          &program->calls[instruction->call_index];
      if (call->owner_block != function->first_block ||
          call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
          call->callee_identity >= program->identity_count)
        return false;
      const w_seed_hir0_identity *identity =
          &program->identities[call->callee_identity];
      if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
          identity->target_index >= program->function_count ||
          program->functions[identity->target_index].is_async ||
          program->functions[identity->target_index].module_index !=
              function->module_index)
        return false;
    } else {
      return false;
    }
  }
  return yields != 0u;
}

/* HIR0 verification proves that each function has a dense, forward-only
 * block order. Reverse topological dynamic programming computes each block
 * exactly once. A shared continuation is therefore read from the cache once,
 * while a branch combines mutually-exclusive arm maxima with max(). The
 * private process-parallel projection may carry a direct call result through
 * the selected task closure; ordinary selectors keep this disabled. */
static bool native_panic_terminator_supported(
    const w_seed_hir0_program *program, size_t terminator_index,
    uint32_t function_index, const w_seed_hir0_terminator *terminator) {
  if (program == NULL || terminator == NULL ||
      terminator_index >= program->terminator_count ||
      function_index >= program->function_count ||
      terminator->kind != W_SEED_HIR0_TERMINATOR_PANIC ||
      terminator->panic_code != W_SEED_HIR0_PANIC_CODE_EXPLICIT ||
      terminator->call_index != W_SEED_HIR0_NONE ||
      terminator->value_index == W_SEED_HIR0_NONE ||
      terminator->value_index >= program->value_count ||
      terminator->result_type == W_SEED_HIR0_NONE ||
      terminator->result_type >= program->type_count ||
      program->types[terminator->result_type].kind !=
          W_SEED_HIR0_TYPE_NEVER ||
      terminator->owner_block >= program->block_count ||
      program->blocks[terminator->owner_block].owner_function !=
          function_index ||
      terminator->target_block != W_SEED_HIR0_NONE ||
      terminator->else_block != W_SEED_HIR0_NONE ||
      terminator->first_edge_argument != W_SEED_HIR0_NONE ||
      terminator->edge_argument_count != 0u ||
      terminator->logical_operator != W_SEED_HIR0_LOGICAL_NONE ||
      terminator->switch_enum_index != W_SEED_HIR0_NONE ||
      terminator->first_switch_edge != W_SEED_HIR0_NONE ||
      terminator->switch_edge_count != 0u ||
      terminator->switch_carrier_width != 0u)
    return false;
  const w_seed_hir0_value *message =
      &program->values[terminator->value_index];
  if (message->kind != W_SEED_HIR0_VALUE_CONST_STRING ||
      message->owner_kind != W_SEED_HIR0_VALUE_OWNER_TERMINATOR ||
      message->owner_index != terminator_index ||
      message->owner_ordinal != 0u || message->type_index != 1u ||
      message->binding_index != W_SEED_HIR0_NONE ||
      message->parameter_index != W_SEED_HIR0_NONE ||
      message->call_index != W_SEED_HIR0_NONE ||
      message->left_value != W_SEED_HIR0_NONE ||
      message->right_value != W_SEED_HIR0_NONE ||
      message->first_interpolation_segment != W_SEED_HIR0_NONE ||
      message->interpolation_segment_count != 0u ||
      message->byte_offset > program->value_byte_count ||
      message->byte_count >
          program->value_byte_count - message->byte_offset ||
      message->byte_count > W_SEED_HIR0_MAX_VALUE_BYTES ||
      (message->byte_count != 0u && program->value_bytes == NULL))
    return false;
  return program->types[message->type_index].kind == W_SEED_HIR0_TYPE_STRING;
}

/* Derive panic reachability from the selected entry call graph.  All blocks
 * of a reachable function are inspected so a terminal CFG arm is admitted;
 * dead functions do not contribute a panic fact. */
static bool native_program_reachable_panic(
    const w_seed_hir0_program *program, uint32_t root_function,
    bool *has_panic) {
  if (program == NULL || has_panic == NULL ||
      root_function >= program->function_count ||
      program->function_count > W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS)
    return false;
  bool reachable[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS] = {false};
  uint32_t stack[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS] = {0u};
  size_t stack_count = 0u;
  reachable[root_function] = true;
  stack[stack_count] = root_function;
  stack_count += 1u;
  bool found = false;
  while (stack_count != 0u) {
    const uint32_t function_index = stack[stack_count - 1u];
    stack_count -= 1u;
    const w_seed_hir0_function *function =
        &program->functions[function_index];
    if (function->first_block >= program->block_count ||
        function->block_count == 0u ||
        function->block_count > program->block_count - function->first_block)
      return false;
    for (size_t block_ordinal = 0u; block_ordinal < function->block_count;
         block_ordinal += 1u) {
      const size_t block_index =
          (size_t)function->first_block + block_ordinal;
      const w_seed_hir0_block *block = &program->blocks[block_index];
      if (block->owner_function != function_index ||
          (size_t)block->first_instruction > program->instruction_count ||
          block->instruction_count >
              program->instruction_count - block->first_instruction ||
          block->terminator_index >= program->terminator_count)
        return false;
      const w_seed_hir0_terminator *terminator =
          &program->terminators[block->terminator_index];
      if (terminator->kind == W_SEED_HIR0_TERMINATOR_PANIC) {
        if (!native_panic_terminator_supported(
                program, block->terminator_index, function_index,
                terminator))
          return false;
        found = true;
      }
      for (size_t instruction_ordinal = 0u;
           instruction_ordinal < block->instruction_count;
           instruction_ordinal += 1u) {
        const w_seed_hir0_instruction *instruction =
            &program->instructions[(size_t)block->first_instruction +
                                   instruction_ordinal];
        if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL) continue;
        if (instruction->call_index >= program->call_count) return false;
        const w_seed_hir0_call *call = &program->calls[instruction->call_index];
        if (call->callee_identity >= program->identity_count) return false;
        const w_seed_hir0_identity *identity =
            &program->identities[call->callee_identity];
        if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION) continue;
        if (identity->target_index >= program->function_count) return false;
        if (!reachable[identity->target_index]) {
          reachable[identity->target_index] = true;
          if (stack_count >= W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS)
            return false;
          stack[stack_count] = identity->target_index;
          stack_count += 1u;
        }
      }
    }
  }
  *has_panic = found;
  return true;
}

static bool program_function_maximum(
    const w_seed_hir0_program *program, size_t function_index,
    const w_seed_native_subset0_process *process,
    const w_seed_hir0_binding *const *bindings, size_t *binding_reads,
    uint8_t *state, size_t *cached, bool *has_interpolation,
    bool *has_local_calls, bool allow_call_result_return) {
  if (program == NULL || bindings == NULL || binding_reads == NULL ||
      state == NULL || cached == NULL || has_interpolation == NULL ||
      has_local_calls == NULL || function_index >= program->function_count)
    return false;
  if (state[function_index] == 2u) return true;
  /* Preserve call-cycle detection for the function graph. */
  if (state[function_index] == 1u) return false;
  state[function_index] = 1u;

  const w_seed_hir0_function *function = &program->functions[function_index];
  const bool process_entry =
      process != NULL && process->function_index == function_index;
  const bool async_direct_entry =
      function->is_async && !function->is_const &&
      !function->is_anonymous_entry &&
      function->direct_entry == W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE;
  const bool async_static_yield =
      program_function_has_static_yields(program, function_index);
  if (function->return_type >= program->type_count ||
      (!(program->types[function->return_type].kind ==
             W_SEED_HIR0_TYPE_UNIT ||
          program->types[function->return_type].kind == W_SEED_HIR0_TYPE_I64 ||
          program->types[function->return_type].kind == W_SEED_HIR0_TYPE_U64 ||
          program->types[function->return_type].kind ==
              W_SEED_HIR0_TYPE_INTEGER ||
          program->types[function->return_type].kind == W_SEED_HIR0_TYPE_F32 ||
          program->types[function->return_type].kind == W_SEED_HIR0_TYPE_F64 ||
         program->types[function->return_type].kind == W_SEED_HIR0_TYPE_BOOL ||
         program_enum_type_supported(program, function->return_type, NULL,
                                      NULL)) &&
       !(process_entry &&
         process_nominal_type_is(program, function->return_type, 0u,
                                 process->exit_code_symbol_index))) ||
      (function->is_async && !process_entry && !async_direct_entry &&
       !async_static_yield) ||
      function->is_throws ||
      function->is_unsafe ||
      function->has_borrow_clause || function->block_count == 0u ||
      function->block_count > W_SEED_NATIVE_SUBSET0_MAX_BLOCKS ||
      function->first_block >= program->block_count ||
      function->block_count > program->block_count - function->first_block)
    return false;
  const bool enum_switch =
      program_enum_switch_is_supported(program, function_index);
  const bool process_terminal_return_cfg =
      process_entry &&
      program_process_terminal_return_cfg_is_supported(program, function_index);
  const bool post_test_loop =
      program_post_test_loop_is_supported(program, function_index);
  if (program->types[function->return_type].kind != W_SEED_HIR0_TYPE_UNIT &&
      function->block_count > 1u &&
      !program_scalar_cfg_is_supported(program, function_index) &&
      !program_natural_loop_is_supported(program, function_index) &&
      !post_test_loop &&
      !enum_switch && !process_terminal_return_cfg)
    return false;
  for (size_t parameter = 0u; parameter < function->parameter_count;
       parameter += 1u) {
    const size_t parameter_index =
        (size_t)function->first_parameter + parameter;
    if (parameter_index >= program->parameter_count) return false;
    const uint32_t type_index = program->parameters[parameter_index].type_index;
    if (type_index >= program->type_count) return false;
    const bool scalar_or_enum =
        program->types[type_index].kind == W_SEED_HIR0_TYPE_I64 ||
        program->types[type_index].kind == W_SEED_HIR0_TYPE_U64 ||
        program->types[type_index].kind == W_SEED_HIR0_TYPE_INTEGER ||
        program->types[type_index].kind == W_SEED_HIR0_TYPE_F32 ||
        program->types[type_index].kind == W_SEED_HIR0_TYPE_F64 ||
        program->types[type_index].kind == W_SEED_HIR0_TYPE_BOOL ||
        program_enum_type_supported(program, type_index, NULL, NULL);
    const bool process_owner =
        process_entry &&
        ((parameter_index ==
              (size_t)(process->arguments_parameter - program->parameters) &&
          process_nominal_type_is(program, type_index, 0u,
                                  process->arguments_symbol_index)) ||
         (parameter_index ==
              (size_t)(process->context_parameter - program->parameters) &&
          process_nominal_type_is(program, type_index, 0u,
                                  process->context_symbol_index)));
    if (!scalar_or_enum && !process_owner)
      return false;
  }

  if (program_natural_loop_is_supported(program, function_index)) {
    if (process_entry) return false;
    cached[function_index] = 0u;
    state[function_index] = 2u;
    return true;
  }
  if (post_test_loop) {
    if (process_entry) return false;
    cached[function_index] = 0u;
    state[function_index] = 2u;
    return true;
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
            !process_or_program_value_lowerable(
                program, program->bindings[instruction->binding_index]
                           .initializer_value,
                (uint32_t)function_index, process, true, 0u))
          return false;
        continue;
      }
      if (instruction->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) {
        if (!async_static_yield) return false;
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
                                        &addition, has_interpolation,
                                        process_entry ? process : NULL) ||
            (process_entry &&
             !process_host_call_supported(program, call,
                                          (uint32_t)function_index, process)))
          return false;
      } else if (callee->kind == W_SEED_HIR0_IDENTITY_FUNCTION) {
        if (callee->target_index >= program->function_count ||
            call->argument_count != callee->parameter_count)
          return false;
        const w_seed_hir0_function *target =
            &program->functions[callee->target_index];
        if (target->is_async) {
          const bool direct =
              call->execution_kind ==
                  W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED &&
              target->direct_entry == W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE;
          const bool static_yield =
              call->execution_kind ==
                  W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED &&
              program_function_has_static_yields(program,
                                                 callee->target_index);
          if (!direct && !static_yield) return false;
        }
        for (size_t argument = 0u; argument < call->argument_count;
             argument += 1u) {
          const w_seed_hir0_argument *item =
              &program->arguments[(size_t)call->first_argument + argument];
          if (!process_or_program_value_lowerable(
                   program, item->value_index, (uint32_t)function_index,
                   process, false, 0u))
            return false;
        }
        if (!program_function_maximum(
                program, callee->target_index, NULL, bindings, binding_reads,
                state, cached, has_interpolation, has_local_calls,
                allow_call_result_return))
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
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_PANIC) {
      if (!native_panic_terminator_supported(
              program, block->terminator_index, (uint32_t)function_index,
              terminator))
        return false;
      /* Panic has no stdout contribution and never reaches a successor. */
      block_maximum[local_block] = total;
      continue;
    }
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
          (!allow_call_result_return && !enum_switch &&
           terminator->value_index < program->value_count &&
           program->values[terminator->value_index].kind ==
               W_SEED_HIR0_VALUE_CALL_RESULT) ||
          !process_or_program_value_lowerable(
              program, terminator->value_index, (uint32_t)function_index,
              process, false, 0u)) {
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
           !process_or_program_value_lowerable(
               program, terminator->value_index, (uint32_t)function_index,
               process, false, 0u))
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
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_SWITCH_ENUM) {
       if (!enum_switch || terminator->value_index >= program->value_count ||
           !process_or_program_value_lowerable(
               program, terminator->value_index, (uint32_t)function_index,
               process, false, 0u))
        return false;
      size_t arm_maximum = 0u;
      for (size_t ordinal = 1u; ordinal < function->block_count; ordinal += 1u)
        if (block_maximum[ordinal] > arm_maximum)
          arm_maximum = block_maximum[ordinal];
      if (arm_maximum > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES ||
          total > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES - arm_maximum)
        return false;
      block_maximum[local_block] = total + arm_maximum;
      continue;
    }
    return false;
  }

  cached[function_index] = block_maximum[0u];
  state[function_index] = 2u;
  return true;
}

/* W-1617 is deliberately a separate native selector.  The ordinary program
 * selector below remains non-throwing and therefore rejects this HIR.  Keep
 * this witness exact so the private MLIR adapter never has to infer an error
 * carrier from a general throwing CFG. */
static bool typed_cleanup_selection_derive(
    const w_seed_hir0_program *program,
    w_seed_native_subset0_typed_propagation *selection) {
  if (program == NULL || selection == NULL || program->module_count != 1u ||
      program->identity_count < 6u || program->type_count != 5u ||
      program->function_count != 4u || program->parameter_count != 0u ||
      program->block_count != 6u || program->block_argument_count != 2u ||
      program->edge_argument_count != 0u || program->instruction_count != 2u ||
      program->binding_count != 0u || program->call_count != 3u ||
      program->argument_count != 0u || program->value_count != 3u ||
      program->interpolation_segment_count != 0u ||
      program->terminator_count != 6u || program->entry_count != 1u ||
      program->enum_count != 1u || program->enum_case_count != 1u ||
      program->enum_case_parameter_count != 0u ||
      program->enum_subset_member_count != 0u ||
      program->enum_payload_count != 0u || program->switch_edge_count != 0u ||
      program->switch_capture_count != 0u || program->cleanup_count != 1u ||
      program->external_module_count != 0u ||
      program->external_symbol_count != 0u)
    return false;

  const w_seed_hir0_enum *error_enum = &program->enums[0];
  const w_seed_hir0_type *error_type = &program->types[4];
  const w_seed_hir0_enum_case *error_case = &program->enum_cases[0];
  if (error_type->kind != W_SEED_HIR0_TYPE_ENUM ||
      error_type->enum_index != 0u || error_type->owner_module != 0u ||
      !text_is(program, error_type->name, (const uint8_t *)"Failure", 7u) ||
      error_enum->module_index != 0u || error_enum->type_index != 4u ||
      !text_is(program, error_enum->name, (const uint8_t *)"Failure", 7u) ||
      !error_enum->error_conformance || error_enum->first_case != 0u ||
      error_enum->case_count != 1u || error_case->owner_enum != 0u ||
      error_case->ordinal != 0u || error_case->tag != 0u ||
      error_case->first_payload != 0u || error_case->payload_count != 0u ||
      !text_is(program, error_case->name, (const uint8_t *)"denied", 6u))
    return false;

  const w_seed_hir0_function *cleanup_function = &program->functions[0];
  const w_seed_hir0_function *leaf = &program->functions[1];
  const w_seed_hir0_function *relay = &program->functions[2];
  const w_seed_hir0_function *entry_function = &program->functions[3];
  const w_seed_hir0_entry *entry = &program->entries[0];
  if (!text_is(program, cleanup_function->name, (const uint8_t *)"clean", 5u) ||
      cleanup_function->identity_index != 1u ||
      cleanup_function->module_index != 0u || cleanup_function->is_async ||
      cleanup_function->is_const || cleanup_function->is_throws ||
      cleanup_function->is_unsafe || cleanup_function->has_borrow_clause ||
      cleanup_function->is_anonymous_entry ||
      cleanup_function->parameter_count != 0u ||
      cleanup_function->return_type != 0u || cleanup_function->first_block != 0u ||
      cleanup_function->block_count != 1u ||
      !text_is(program, leaf->name, (const uint8_t *)"leaf", 4u) ||
      leaf->identity_index != 2u || leaf->module_index != 0u ||
      leaf->return_type != 2u || leaf->error_type != 4u ||
      leaf->parameter_count != 0u || !leaf->is_throws || leaf->is_async ||
      leaf->is_const || leaf->is_unsafe || leaf->has_borrow_clause ||
      leaf->is_anonymous_entry || leaf->first_block != 1u ||
      leaf->block_count != 1u ||
      !text_is(program, relay->name, (const uint8_t *)"relay", 5u) ||
      relay->identity_index != 3u || relay->module_index != 0u ||
      relay->return_type != 2u || relay->error_type != 4u ||
      relay->parameter_count != 0u || !relay->is_throws || relay->is_async ||
      relay->is_const || relay->is_unsafe || relay->has_borrow_clause ||
      relay->is_anonymous_entry || relay->first_block != 2u ||
      relay->block_count != 3u || entry_function->identity_index != 4u ||
      entry_function->module_index != 0u ||
      !entry_function->is_anonymous_entry || entry_function->is_async ||
      entry_function->is_const || entry_function->is_throws ||
      entry_function->is_unsafe || entry_function->has_borrow_clause ||
      entry_function->parameter_count != 0u || entry_function->return_type != 0u ||
      entry_function->first_block != 5u || entry_function->block_count != 1u ||
      entry->identity_index != 5u || !entry->is_body ||
      entry->module_index != 0u || entry->target_function != 3u ||
      entry->target_identity != 4u ||
      entry->adapter_kind != W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT)
    return false;

  for (uint32_t identity_index = 0u; identity_index < 6u; identity_index += 1u) {
    const w_seed_hir0_identity *identity = &program->identities[identity_index];
    if (identity_index == 0u) {
      if (identity->kind != W_SEED_HIR0_IDENTITY_MODULE ||
          identity->owner_module != W_SEED_HIR0_NONE)
        return false;
    } else if (identity_index < 5u) {
      if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
          identity->target_index != identity_index - 1u)
        return false;
    } else if (identity->kind != W_SEED_HIR0_IDENTITY_ENTRY ||
               identity->target_index != 0u) {
      return false;
    }
  }

  const w_seed_hir0_cleanup *cleanup = &program->cleanups[0];
  if (cleanup->owner_function != 2u || cleanup->invoke_terminator != 2u ||
      cleanup->cleanup_identity != 1u || cleanup->normal_block != 3u ||
      cleanup->error_block != 4u || cleanup->normal_instruction != 0u ||
      cleanup->error_instruction != 1u || cleanup->normal_call != 1u ||
      cleanup->error_call != 2u)
    return false;
  if (program->blocks[0].owner_function != 0u ||
      program->blocks[0].instruction_count != 0u ||
      program->terminators[0].kind != W_SEED_HIR0_TERMINATOR_RETURN_UNIT ||
      program->blocks[1].owner_function != 1u ||
      program->blocks[1].instruction_count != 0u ||
      program->terminators[1].kind != W_SEED_HIR0_TERMINATOR_THROW ||
      program->blocks[2].owner_function != 2u ||
      program->blocks[2].instruction_count != 0u ||
      program->terminators[2].kind != W_SEED_HIR0_TERMINATOR_INVOKE ||
      program->terminators[2].call_index != 0u ||
      program->terminators[2].target_block != 3u ||
      program->terminators[2].else_block != 4u ||
      program->blocks[3].owner_function != 2u ||
      program->blocks[3].instruction_count != 1u ||
      program->blocks[4].owner_function != 2u ||
      program->blocks[4].instruction_count != 1u ||
      program->terminators[3].kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      program->terminators[4].kind != W_SEED_HIR0_TERMINATOR_THROW ||
      program->blocks[5].owner_function != 3u ||
      program->blocks[5].instruction_count != 0u ||
      program->terminators[5].kind != W_SEED_HIR0_TERMINATOR_RETURN_UNIT)
    return false;

  const w_seed_hir0_call *invoke_call = &program->calls[0];
  const w_seed_hir0_call *normal_cleanup = &program->calls[1];
  const w_seed_hir0_call *error_cleanup = &program->calls[2];
  if (invoke_call->callee_identity != 2u || invoke_call->owner_block != 2u ||
      invoke_call->owner_terminator != 2u ||
      invoke_call->owner_instruction != W_SEED_HIR0_NONE ||
      invoke_call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
      invoke_call->argument_count != 0u || invoke_call->result_type != 2u ||
      normal_cleanup->callee_identity != 1u ||
      error_cleanup->callee_identity != 1u ||
      normal_cleanup->owner_block != 3u || error_cleanup->owner_block != 4u ||
      normal_cleanup->owner_instruction != 0u ||
      error_cleanup->owner_instruction != 1u ||
      normal_cleanup->owner_terminator != W_SEED_HIR0_NONE ||
      error_cleanup->owner_terminator != W_SEED_HIR0_NONE ||
      normal_cleanup->argument_count != 0u ||
      error_cleanup->argument_count != 0u ||
      normal_cleanup->result_type != 0u || error_cleanup->result_type != 0u)
    return false;

  *selection = (w_seed_native_subset0_typed_propagation){
      .entry = entry,
      .leaf_function = leaf,
      .relay_function = relay,
      .leaf_throw = &program->terminators[1],
      .invoke = &program->terminators[2],
      .normal_return = &program->terminators[3],
      .error_throw = &program->terminators[4],
      .cleanup = cleanup,
      .cleanup_function = cleanup_function,
      .normal_cleanup_call = normal_cleanup,
      .error_cleanup_call = error_cleanup,
      .entry_function_index = 3u,
      .leaf_function_index = 1u,
      .relay_function_index = 2u,
      .cleanup_function_index = 0u,
      .invoke_block_index = 2u,
      .normal_block_index = 3u,
      .error_block_index = 4u,
      .error_type_index = 4u,
      .error_enum_index = 0u,
      .error_case_index = 0u};
  return true;
}

static bool typed_propagation_selection_derive(
    const w_seed_hir0_program *program,
    w_seed_native_subset0_typed_propagation *selection) {
  if (program != NULL && program->cleanup_count != 0u)
    return typed_cleanup_selection_derive(program, selection);
  if (program == NULL || selection == NULL || program->module_count != 1u ||
      program->identity_count < 5u || program->type_count != 5u ||
      program->function_count != 3u || program->parameter_count != 0u ||
      program->block_count != 5u || program->block_argument_count != 2u ||
      program->edge_argument_count != 0u || program->instruction_count != 0u ||
      program->binding_count != 0u || program->call_count != 1u ||
      program->argument_count != 0u ||
      program->value_count != 3u ||
      program->interpolation_segment_count != 0u ||
      program->terminator_count != 5u || program->entry_count != 1u ||
      program->enum_count != 1u || program->enum_case_count != 1u ||
      program->enum_case_parameter_count != 0u ||
      program->enum_subset_member_count != 0u ||
      program->enum_payload_count != 0u || program->switch_edge_count != 0u ||
      program->switch_capture_count != 0u ||
      program->cleanup_count != 0u ||
      program->external_module_count != 0u ||
      program->external_symbol_count != 0u)
    return false;

  const w_seed_hir0_enum *error_enum = &program->enums[0];
  const w_seed_hir0_type *error_type = &program->types[4];
  const w_seed_hir0_enum_case *error_case = &program->enum_cases[0];
  if (error_type->kind != W_SEED_HIR0_TYPE_ENUM ||
      error_type->enum_index != 0u || error_type->owner_module != 0u ||
      !text_is(program, error_type->name, (const uint8_t *)"Failure", 7u) ||
      error_enum->module_index != 0u || error_enum->type_index != 4u ||
      !text_is(program, error_enum->name, (const uint8_t *)"Failure", 7u) ||
      !error_enum->error_conformance || error_enum->first_case != 0u ||
      error_enum->case_count != 1u || error_case->owner_enum != 0u ||
      error_case->ordinal != 0u || error_case->tag != 0u ||
      error_case->first_payload != 0u || error_case->payload_count != 0u ||
      !text_is(program, error_case->name, (const uint8_t *)"denied", 6u))
    return false;

  const w_seed_hir0_entry *entry = &program->entries[0];
  const w_seed_hir0_function *leaf = &program->functions[0];
  const w_seed_hir0_function *relay = &program->functions[1];
  const w_seed_hir0_function *entry_function = &program->functions[2];
  if (program->identities[0].kind != W_SEED_HIR0_IDENTITY_MODULE ||
      program->identities[0].owner_module != W_SEED_HIR0_NONE ||
      program->identities[1].kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      program->identities[1].target_index != 0u ||
      program->identities[2].kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      program->identities[2].target_index != 1u ||
      program->identities[3].kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      program->identities[3].target_index != 2u ||
      program->identities[4].kind != W_SEED_HIR0_IDENTITY_ENTRY ||
      program->identities[4].target_index != 0u ||
      leaf->identity_index != 1u || relay->identity_index != 2u ||
      entry_function->identity_index != 3u || entry->identity_index != 4u ||
      !entry->is_body || entry->module_index != 0u ||
      entry->target_function != 2u || entry->target_identity != 3u ||
      entry->adapter_kind != W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT ||
      entry_function->module_index != 0u ||
      !entry_function->is_anonymous_entry || entry_function->is_async ||
      entry_function->is_const || entry_function->is_throws ||
      entry_function->is_unsafe || entry_function->has_borrow_clause ||
      entry_function->parameter_count != 0u || entry_function->return_type != 0u ||
      entry_function->first_block != 4u || entry_function->block_count != 1u ||
      !text_is(program, leaf->name, (const uint8_t *)"leaf", 4u) ||
      !text_is(program, relay->name, (const uint8_t *)"relay", 5u))
    return false;

  if (leaf->module_index != 0u || leaf->return_type != 2u ||
      leaf->error_type != 4u || leaf->parameter_count != 0u ||
      !leaf->is_throws || leaf->is_async || leaf->is_const || leaf->is_unsafe ||
      leaf->has_borrow_clause || leaf->is_anonymous_entry ||
      leaf->first_block != 0u || leaf->block_count != 1u ||
      relay->module_index != 0u || relay->return_type != 2u ||
      relay->error_type != 4u || relay->parameter_count != 0u ||
      !relay->is_throws || relay->is_async || relay->is_const ||
      relay->is_unsafe || relay->has_borrow_clause || relay->is_anonymous_entry ||
      relay->first_block != 1u || relay->block_count != 3u)
    return false;

  const w_seed_hir0_block *leaf_block = &program->blocks[0];
  const w_seed_hir0_block *invoke_block = &program->blocks[1];
  const w_seed_hir0_block *normal_block = &program->blocks[2];
  const w_seed_hir0_block *error_block = &program->blocks[3];
  const w_seed_hir0_block *entry_block = &program->blocks[4];
  if (leaf_block->owner_function != 0u || leaf_block->first_instruction != 0u ||
      leaf_block->instruction_count != 0u || leaf_block->terminator_index != 0u ||
      invoke_block->owner_function != 1u ||
      invoke_block->first_instruction != 0u ||
      invoke_block->instruction_count != 0u || invoke_block->terminator_index != 1u ||
      invoke_block->block_argument_count != 0u ||
      invoke_block->first_block_argument != W_SEED_HIR0_NONE ||
      normal_block->owner_function != 1u ||
      normal_block->first_instruction != 0u ||
      normal_block->instruction_count != 0u || normal_block->terminator_index != 2u ||
      normal_block->first_block_argument != 0u ||
      normal_block->block_argument_count != 1u || error_block->owner_function != 1u ||
      error_block->first_instruction != 0u ||
      error_block->instruction_count != 0u || error_block->terminator_index != 3u ||
      error_block->first_block_argument != 1u ||
      error_block->block_argument_count != 1u || entry_block->owner_function != 2u ||
      entry_block->first_instruction != 0u ||
      entry_block->instruction_count != 0u || entry_block->terminator_index != 4u)
    return false;

  const w_seed_hir0_terminator *leaf_throw = &program->terminators[0];
  const w_seed_hir0_terminator *invoke = &program->terminators[1];
  const w_seed_hir0_terminator *normal_return = &program->terminators[2];
  const w_seed_hir0_terminator *error_throw = &program->terminators[3];
  const w_seed_hir0_terminator *entry_return = &program->terminators[4];
  if (leaf_throw->owner_block != 0u ||
      leaf_throw->kind != W_SEED_HIR0_TERMINATOR_THROW ||
      leaf_throw->value_index != 0u || leaf_throw->result_type != 4u ||
      invoke->owner_block != 1u ||
      invoke->kind != W_SEED_HIR0_TERMINATOR_INVOKE || invoke->call_index != 0u ||
      invoke->value_index != W_SEED_HIR0_NONE || invoke->result_type != 2u ||
      invoke->error_type != 4u || invoke->target_block != 2u ||
      invoke->else_block != 3u || normal_return->owner_block != 2u ||
      normal_return->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      normal_return->value_index != 1u || normal_return->result_type != 2u ||
      error_throw->owner_block != 3u ||
      error_throw->kind != W_SEED_HIR0_TERMINATOR_THROW ||
      error_throw->value_index != 2u || error_throw->result_type != 4u ||
      entry_return->owner_block != 4u ||
      entry_return->kind != W_SEED_HIR0_TERMINATOR_RETURN_UNIT)
    return false;

  const w_seed_hir0_block_argument *normal_argument =
      &program->block_arguments[0];
  const w_seed_hir0_block_argument *error_argument =
      &program->block_arguments[1];
  const w_seed_hir0_value *leaf_value = &program->values[0];
  const w_seed_hir0_value *normal_value = &program->values[1];
  const w_seed_hir0_value *error_value = &program->values[2];
  if (normal_argument->owner_block != 2u || normal_argument->ordinal != 0u ||
      normal_argument->type_index != 2u || error_argument->owner_block != 3u ||
      error_argument->ordinal != 0u || error_argument->type_index != 4u ||
      leaf_value->kind != W_SEED_HIR0_VALUE_ENUM_CASE ||
      leaf_value->type_index != 4u || leaf_value->enum_index != 0u ||
      leaf_value->enum_case_index != 0u || leaf_value->first_enum_payload != 0u ||
      leaf_value->enum_payload_count != 0u ||
      normal_value->kind != W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ ||
      normal_value->type_index != 2u || normal_value->block_argument_index != 0u ||
      error_value->kind != W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ ||
      error_value->type_index != 4u || error_value->block_argument_index != 1u)
    return false;

  const w_seed_hir0_call *call = &program->calls[0];
  if (call->owner_instruction != W_SEED_HIR0_NONE ||
      call->owner_terminator != 1u || call->owner_block != 1u ||
      call->ordinal != 0u || call->callee_identity >= program->identity_count ||
      program->identities[call->callee_identity].kind !=
          W_SEED_HIR0_IDENTITY_FUNCTION ||
      program->identities[call->callee_identity].target_index != 0u ||
      call->first_argument != 0u || call->argument_count != 0u ||
      call->first_requirement != W_SEED_HIR0_NONE ||
      call->requirement_count != 0u || call->result_type != 2u ||
      call->execution_kind != W_SEED_HIR0_CALL_DIRECT)
    return false;

  *selection = (w_seed_native_subset0_typed_propagation){
      .entry = entry,
      .leaf_function = leaf,
      .relay_function = relay,
      .leaf_throw = leaf_throw,
      .invoke = invoke,
      .normal_return = normal_return,
      .error_throw = error_throw,
      .cleanup = NULL,
      .cleanup_function = NULL,
      .normal_cleanup_call = NULL,
      .error_cleanup_call = NULL,
      .entry_function_index = 2u,
      .leaf_function_index = 0u,
      .relay_function_index = 1u,
      .cleanup_function_index = W_SEED_HIR0_NONE,
      .invoke_block_index = 1u,
      .normal_block_index = 2u,
      .error_block_index = 3u,
      .error_type_index = 4u,
      .error_enum_index = 0u,
      .error_case_index = 0u};
  return true;
}

w_seed_native_subset0_status
w_seed_native_subset0_select_typed_propagation(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_typed_propagation *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return W_SEED_NATIVE_SUBSET0_INVALID;
  w_seed_native_subset0_typed_propagation candidate;
  if (!typed_propagation_selection_derive(program, &candidate))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  *selection = candidate;
  return W_SEED_NATIVE_SUBSET0_OK;
}

bool w_seed_native_subset0_verify_typed_propagation(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const w_seed_native_subset0_typed_propagation *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return false;
  w_seed_native_subset0_typed_propagation candidate;
  if (!typed_propagation_selection_derive(program, &candidate)) return false;
  return selection->entry == candidate.entry &&
         selection->leaf_function == candidate.leaf_function &&
         selection->relay_function == candidate.relay_function &&
         selection->leaf_throw == candidate.leaf_throw &&
         selection->invoke == candidate.invoke &&
         selection->normal_return == candidate.normal_return &&
         selection->error_throw == candidate.error_throw &&
         selection->cleanup == candidate.cleanup &&
         selection->cleanup_function == candidate.cleanup_function &&
         selection->normal_cleanup_call == candidate.normal_cleanup_call &&
         selection->error_cleanup_call == candidate.error_cleanup_call &&
         selection->entry_function_index == candidate.entry_function_index &&
         selection->leaf_function_index == candidate.leaf_function_index &&
         selection->relay_function_index == candidate.relay_function_index &&
         selection->cleanup_function_index == candidate.cleanup_function_index &&
         selection->invoke_block_index == candidate.invoke_block_index &&
         selection->normal_block_index == candidate.normal_block_index &&
         selection->error_block_index == candidate.error_block_index &&
         selection->error_type_index == candidate.error_type_index &&
         selection->error_enum_index == candidate.error_enum_index &&
         selection->error_case_index == candidate.error_case_index;
}

static bool integer_exactly_selection_derive(
    const w_seed_hir0_program *program,
    w_seed_native_subset0_integer_exactly *selection) {
  if (program == NULL || selection == NULL || program->module_count != 1u ||
      program->identity_count < 5u || program->function_count != 2u ||
      program->parameter_count != 1u || program->block_count != 4u ||
      program->block_argument_count != 2u ||
      program->edge_argument_count != 0u ||
      program->switch_capture_count != 0u ||
      program->instruction_count != 0u || program->binding_count != 0u ||
      program->call_count != 0u ||
      program->argument_count != 0u || program->enum_payload_count != 0u ||
      program->value_count != 3u ||
      program->interpolation_segment_count != 0u ||
      program->terminator_count != 4u || program->entry_count != 1u ||
      program->enum_count != 0u || program->enum_case_count != 0u ||
      program->enum_case_parameter_count != 0u ||
      program->enum_subset_member_count != 0u ||
      program->cleanup_count != 0u || program->external_module_count != 0u ||
      program->external_symbol_count != 0u || program->type_count < 5u)
    return false;

  const w_seed_hir0_module *module = &program->modules[0];
  const w_seed_hir0_identity *module_identity = &program->identities[0];
  const w_seed_hir0_identity *function_identity = &program->identities[1];
  const w_seed_hir0_identity *entry_identity = &program->identities[2];
  const w_seed_hir0_identity *body_identity = &program->identities[3];
  const w_seed_hir0_function *function = &program->functions[0];
  const w_seed_hir0_function *entry_function = &program->functions[1];
  const w_seed_hir0_parameter *parameter = &program->parameters[0];
  const w_seed_hir0_entry *entry = &program->entries[0];
  if (module->first_function != 0u || module->function_count != 2u ||
      module->first_entry != 0u || module->entry_count != 1u ||
      module_identity->kind != W_SEED_HIR0_IDENTITY_MODULE ||
      module_identity->owner_module != W_SEED_HIR0_NONE ||
      function_identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      function_identity->target_index != 0u ||
      function_identity->first_parameter != 0u ||
      function_identity->parameter_count != 1u ||
      function_identity->return_type != function->return_type ||
      entry_identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      entry_identity->target_index != 1u ||
      entry_identity->first_parameter != 1u ||
      entry_identity->parameter_count != 0u ||
      entry_identity->return_type != 0u ||
      body_identity->kind != W_SEED_HIR0_IDENTITY_ENTRY ||
      body_identity->target_index != 0u || function->identity_index != 1u ||
      function->module_index != 0u || function->return_type >= program->type_count ||
      !function->is_throws || function->error_type >= program->type_count ||
      function->parameter_count != 1u || function->first_parameter != 0u ||
      function->first_block != 0u || function->block_count != 3u ||
      function->is_async || function->is_const || function->is_unsafe ||
      function->has_borrow_clause || function->is_anonymous_entry ||
      entry_function->identity_index != 2u ||
      entry_function->module_index != 0u ||
      entry_function->return_type != 0u || entry_function->is_throws ||
      entry_function->error_type != W_SEED_HIR0_NONE ||
      entry_function->parameter_count != 0u ||
      entry_function->first_parameter != 1u ||
      entry_function->first_block != 3u || entry_function->block_count != 1u ||
      entry_function->is_async || entry_function->is_const ||
      entry_function->is_unsafe || entry_function->has_borrow_clause ||
      !entry_function->is_anonymous_entry ||
      parameter->owner_function != 0u || parameter->ordinal != 0u ||
      !entry->is_body || entry->module_index != 0u ||
      entry->target_function != 1u || entry->target_identity != 2u ||
      entry->identity_index != 3u ||
      entry->adapter_kind != W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT ||
      entry->cleanup_obligation != W_SEED_HIR0_ENTRY_CLEANUP_NONE ||
      entry->cleanup_owner_parameter_count != 0u)
    return false;

  uint32_t error_type_index = W_SEED_HIR0_NONE;
  for (size_t type_index = 0u; type_index < program->type_count;
       type_index += 1u) {
    const w_seed_hir0_type *type = &program->types[type_index];
    if (type_index < 4u) continue;
    if (type->kind == W_SEED_HIR0_TYPE_NUMERIC_CONVERSION_ERROR) {
      if (error_type_index != W_SEED_HIR0_NONE ||
          type_index + 1u != program->type_count ||
          type->owner_module != W_SEED_HIR0_NONE ||
          !text_is(program, type->name,
                   (const uint8_t *)"NumericConversionError", 22u))
        return false;
      error_type_index = (uint32_t)type_index;
    } else {
      native_integer_facts facts;
      if (!native_integer_type_facts(program, (uint32_t)type_index, &facts))
        return false;
    }
  }

  native_integer_facts source_facts;
  native_integer_facts destination_facts;
  if (error_type_index == W_SEED_HIR0_NONE ||
      function->error_type != error_type_index ||
      !native_integer_type_facts(program, parameter->type_index,
                                 &source_facts) ||
      !native_integer_type_facts(program, function->return_type,
                                 &destination_facts))
    return false;

  const w_seed_hir0_block *split_block = &program->blocks[0];
  const w_seed_hir0_block *normal_block = &program->blocks[1];
  const w_seed_hir0_block *error_block = &program->blocks[2];
  const w_seed_hir0_block *entry_block = &program->blocks[3];
  const w_seed_hir0_terminator *conversion = &program->terminators[0];
  const w_seed_hir0_terminator *normal_return = &program->terminators[1];
  const w_seed_hir0_terminator *error_throw = &program->terminators[2];
  const w_seed_hir0_terminator *entry_return = &program->terminators[3];
  if (split_block->owner_function != 0u ||
      split_block->first_instruction != 0u ||
      split_block->instruction_count != 0u ||
      split_block->terminator_index != 0u ||
      split_block->block_argument_count != 0u ||
      split_block->first_block_argument != W_SEED_HIR0_NONE ||
      normal_block->owner_function != 0u ||
      normal_block->first_instruction != 0u ||
      normal_block->instruction_count != 0u ||
      normal_block->terminator_index != 1u ||
      normal_block->block_argument_count != 1u ||
      normal_block->first_block_argument != 0u ||
      error_block->owner_function != 0u ||
      error_block->first_instruction != 0u ||
      error_block->instruction_count != 0u ||
      error_block->terminator_index != 2u ||
      error_block->block_argument_count != 1u ||
      error_block->first_block_argument != 1u ||
      entry_block->owner_function != 1u ||
      entry_block->first_instruction != 0u ||
      entry_block->instruction_count != 0u ||
      entry_block->terminator_index != 3u ||
      conversion->kind != W_SEED_HIR0_TERMINATOR_INTEGER_EXACTLY ||
      conversion->value_index != 0u ||
      conversion->result_type != function->return_type ||
      conversion->error_type != error_type_index ||
      conversion->target_block != 1u || conversion->else_block != 2u ||
      conversion->numeric_conversion_error_case !=
          W_SEED_HIR0_NUMERIC_CONVERSION_ERROR_OUT_OF_RANGE ||
      normal_return->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      normal_return->value_index != 1u ||
      normal_return->result_type != function->return_type ||
      error_throw->kind != W_SEED_HIR0_TERMINATOR_THROW ||
      error_throw->value_index != 2u ||
      error_throw->result_type != error_type_index ||
      entry_return->kind != W_SEED_HIR0_TERMINATOR_RETURN_UNIT)
    return false;

  const w_seed_hir0_block_argument *normal_argument =
      &program->block_arguments[0];
  const w_seed_hir0_block_argument *error_argument =
      &program->block_arguments[1];
  const w_seed_hir0_value *source_value = &program->values[0];
  const w_seed_hir0_value *normal_value = &program->values[1];
  const w_seed_hir0_value *error_value = &program->values[2];
  if (source_value->kind != W_SEED_HIR0_VALUE_PARAMETER_READ ||
      source_value->owner_kind != W_SEED_HIR0_VALUE_OWNER_TERMINATOR ||
      source_value->owner_index != 0u || source_value->owner_ordinal != 0u ||
      source_value->type_index != parameter->type_index ||
      source_value->parameter_index != 0u ||
      normal_argument->owner_block != 1u || normal_argument->ordinal != 0u ||
      normal_argument->type_index != function->return_type ||
      error_argument->owner_block != 2u || error_argument->ordinal != 0u ||
      error_argument->type_index != error_type_index ||
      normal_value->kind != W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ ||
      normal_value->type_index != function->return_type ||
      normal_value->block_argument_index != 0u ||
      error_value->kind != W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ ||
      error_value->type_index != error_type_index ||
      error_value->block_argument_index != 1u)
    return false;

  *selection = (w_seed_native_subset0_integer_exactly){
      .entry = entry,
      .function = function,
      .source_parameter = parameter,
      .source_value = source_value,
      .conversion = conversion,
      .normal_argument = normal_argument,
      .error_argument = error_argument,
      .normal_return = normal_return,
      .error_throw = error_throw,
      .entry_index = 0u,
      .function_index = 0u,
      .entry_function_index = 1u,
      .split_block_index = 0u,
      .normal_block_index = 1u,
      .error_block_index = 2u,
      .source_type_index = parameter->type_index,
      .destination_type_index = function->return_type,
      .error_type_index = error_type_index,
      .source_bit_width = source_facts.bit_width,
      .destination_bit_width = destination_facts.bit_width,
      .source_is_signed = source_facts.is_signed,
      .destination_is_signed = destination_facts.is_signed};
  return true;
}

w_seed_native_subset0_status
w_seed_native_subset0_select_integer_exactly(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_integer_exactly *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return W_SEED_NATIVE_SUBSET0_INVALID;
  w_seed_native_subset0_integer_exactly candidate;
  if (!integer_exactly_selection_derive(program, &candidate))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  *selection = candidate;
  return W_SEED_NATIVE_SUBSET0_OK;
}

bool w_seed_native_subset0_verify_integer_exactly(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const w_seed_native_subset0_integer_exactly *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return false;
  w_seed_native_subset0_integer_exactly candidate;
  if (!integer_exactly_selection_derive(program, &candidate)) return false;
  return selection->entry == candidate.entry &&
         selection->function == candidate.function &&
         selection->source_parameter == candidate.source_parameter &&
         selection->source_value == candidate.source_value &&
         selection->conversion == candidate.conversion &&
         selection->normal_argument == candidate.normal_argument &&
         selection->error_argument == candidate.error_argument &&
         selection->normal_return == candidate.normal_return &&
         selection->error_throw == candidate.error_throw &&
         selection->entry_index == candidate.entry_index &&
         selection->function_index == candidate.function_index &&
         selection->entry_function_index == candidate.entry_function_index &&
         selection->split_block_index == candidate.split_block_index &&
         selection->normal_block_index == candidate.normal_block_index &&
         selection->error_block_index == candidate.error_block_index &&
         selection->source_type_index == candidate.source_type_index &&
         selection->destination_type_index ==
             candidate.destination_type_index &&
         selection->error_type_index == candidate.error_type_index &&
         selection->source_bit_width == candidate.source_bit_width &&
         selection->destination_bit_width ==
             candidate.destination_bit_width &&
         selection->source_is_signed == candidate.source_is_signed &&
         selection->destination_is_signed == candidate.destination_is_signed;
}

static bool float_to_integer_rounding_selection_derive(
    const w_seed_hir0_program *program,
    w_seed_native_subset0_float_to_integer_rounding *selection) {
  if (program == NULL || selection == NULL || program->module_count != 1u ||
      program->identity_count < 5u || program->function_count != 2u ||
      program->parameter_count != 1u || program->block_count != 5u ||
      program->block_argument_count != 3u ||
      program->edge_argument_count != 0u ||
      program->switch_capture_count != 0u ||
      program->instruction_count != 0u || program->binding_count != 0u ||
      program->call_count != 0u || program->argument_count != 0u ||
      program->enum_payload_count != 0u || program->value_count != 4u ||
      program->interpolation_segment_count != 0u ||
      program->terminator_count != 5u || program->entry_count != 1u ||
      program->enum_count != 0u || program->enum_case_count != 0u ||
      program->enum_case_parameter_count != 0u ||
      program->enum_subset_member_count != 0u ||
      program->cleanup_count != 0u || program->external_module_count != 0u ||
      program->external_symbol_count != 0u || program->type_count < 6u)
    return false;

  const w_seed_hir0_module *module = &program->modules[0];
  const w_seed_hir0_identity *module_identity = &program->identities[0];
  const w_seed_hir0_identity *function_identity = &program->identities[1];
  const w_seed_hir0_identity *entry_identity = &program->identities[2];
  const w_seed_hir0_identity *body_identity = &program->identities[3];
  const w_seed_hir0_function *function = &program->functions[0];
  const w_seed_hir0_function *entry_function = &program->functions[1];
  const w_seed_hir0_parameter *parameter = &program->parameters[0];
  const w_seed_hir0_entry *entry = &program->entries[0];
  if (module->first_function != 0u || module->function_count != 2u ||
      module->first_entry != 0u || module->entry_count != 1u ||
      module_identity->kind != W_SEED_HIR0_IDENTITY_MODULE ||
      module_identity->owner_module != W_SEED_HIR0_NONE ||
      function_identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      function_identity->target_index != 0u ||
      function_identity->first_parameter != 0u ||
      function_identity->parameter_count != 1u ||
      function_identity->return_type != function->return_type ||
      entry_identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      entry_identity->target_index != 1u ||
      entry_identity->first_parameter != 1u ||
      entry_identity->parameter_count != 0u ||
      entry_identity->return_type != 0u ||
      body_identity->kind != W_SEED_HIR0_IDENTITY_ENTRY ||
      body_identity->target_index != 0u || function->identity_index != 1u ||
      function->module_index != 0u ||
      function->return_type >= program->type_count || !function->is_throws ||
      function->error_type >= program->type_count ||
      function->parameter_count != 1u || function->first_parameter != 0u ||
      function->first_block != 0u || function->block_count != 4u ||
      function->is_async || function->is_const || function->is_unsafe ||
      function->has_borrow_clause || function->is_anonymous_entry ||
      entry_function->identity_index != 2u ||
      entry_function->module_index != 0u ||
      entry_function->return_type != 0u || entry_function->is_throws ||
      entry_function->error_type != W_SEED_HIR0_NONE ||
      entry_function->parameter_count != 0u ||
      entry_function->first_parameter != 1u ||
      entry_function->first_block != 4u || entry_function->block_count != 1u ||
      entry_function->is_async || entry_function->is_const ||
      entry_function->is_unsafe || entry_function->has_borrow_clause ||
      !entry_function->is_anonymous_entry ||
      parameter->owner_function != 0u || parameter->ordinal != 0u ||
      !entry->is_body || entry->module_index != 0u ||
      entry->target_function != 1u || entry->target_identity != 2u ||
      entry->identity_index != 3u ||
      entry->adapter_kind != W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT ||
      entry->cleanup_obligation != W_SEED_HIR0_ENTRY_CLEANUP_NONE ||
      entry->cleanup_owner_parameter_count != 0u)
    return false;

  uint32_t error_type_index = W_SEED_HIR0_NONE;
  for (size_t type_index = 0u; type_index < program->type_count;
       type_index += 1u) {
    const w_seed_hir0_type *type = &program->types[type_index];
    if (type_index < 4u) continue;
    if (type->kind == W_SEED_HIR0_TYPE_NUMERIC_CONVERSION_ERROR) {
      if (error_type_index != W_SEED_HIR0_NONE ||
          type_index + 1u != program->type_count ||
          type->owner_module != W_SEED_HIR0_NONE ||
          !text_is(program, type->name,
                   (const uint8_t *)"NumericConversionError", 22u))
        return false;
      error_type_index = (uint32_t)type_index;
      continue;
    }
    native_integer_facts integer_facts;
    uint16_t float_width = 0u;
    if (!native_integer_type_facts(program, (uint32_t)type_index,
                                   &integer_facts) &&
        !native_float_type_width(program, (uint32_t)type_index, &float_width))
      return false;
  }

  uint16_t source_width = 0u;
  native_integer_facts destination_facts;
  if (error_type_index == W_SEED_HIR0_NONE ||
      function->error_type != error_type_index ||
      !native_float_type_width(program, parameter->type_index, &source_width) ||
      !native_integer_type_facts(program, function->return_type,
                                 &destination_facts))
    return false;

  const w_seed_hir0_block *split_block = &program->blocks[0];
  const w_seed_hir0_block *normal_block = &program->blocks[1];
  const w_seed_hir0_block *non_finite_block = &program->blocks[2];
  const w_seed_hir0_block *out_of_range_block = &program->blocks[3];
  const w_seed_hir0_block *entry_block = &program->blocks[4];
  const w_seed_hir0_terminator *conversion = &program->terminators[0];
  const w_seed_hir0_terminator *normal_return = &program->terminators[1];
  const w_seed_hir0_terminator *non_finite_throw = &program->terminators[2];
  const w_seed_hir0_terminator *out_of_range_throw = &program->terminators[3];
  const w_seed_hir0_terminator *entry_return = &program->terminators[4];
  if (split_block->owner_function != 0u ||
      split_block->first_instruction != 0u ||
      split_block->instruction_count != 0u ||
      split_block->terminator_index != 0u ||
      split_block->block_argument_count != 0u ||
      split_block->first_block_argument != W_SEED_HIR0_NONE ||
      normal_block->owner_function != 0u ||
      normal_block->first_instruction != 0u ||
      normal_block->instruction_count != 0u ||
      normal_block->terminator_index != 1u ||
      normal_block->block_argument_count != 1u ||
      normal_block->first_block_argument != 0u ||
      non_finite_block->owner_function != 0u ||
      non_finite_block->first_instruction != 0u ||
      non_finite_block->instruction_count != 0u ||
      non_finite_block->terminator_index != 2u ||
      non_finite_block->block_argument_count != 1u ||
      non_finite_block->first_block_argument != 1u ||
      out_of_range_block->owner_function != 0u ||
      out_of_range_block->first_instruction != 0u ||
      out_of_range_block->instruction_count != 0u ||
      out_of_range_block->terminator_index != 3u ||
      out_of_range_block->block_argument_count != 1u ||
      out_of_range_block->first_block_argument != 2u ||
      entry_block->owner_function != 1u ||
      entry_block->first_instruction != 0u ||
      entry_block->instruction_count != 0u ||
      entry_block->terminator_index != 4u ||
      conversion->kind !=
          W_SEED_HIR0_TERMINATOR_FLOAT_TO_INTEGER_ROUNDING ||
      conversion->value_index != 0u ||
      conversion->result_type != function->return_type ||
      conversion->error_type != error_type_index ||
      conversion->target_block != 1u || conversion->else_block != 2u ||
      conversion->third_block != 3u ||
      conversion->rounding_mode < W_SEED_HIR0_ROUNDING_MODE_NEAREST_EVEN ||
      conversion->rounding_mode > W_SEED_HIR0_ROUNDING_MODE_TOWARD_NEGATIVE ||
      conversion->numeric_conversion_error_case !=
          W_SEED_HIR0_NUMERIC_CONVERSION_ERROR_NONE ||
      normal_return->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      normal_return->value_index != 1u ||
      normal_return->result_type != function->return_type ||
      non_finite_throw->kind != W_SEED_HIR0_TERMINATOR_THROW ||
      non_finite_throw->value_index != 2u ||
      non_finite_throw->result_type != error_type_index ||
      out_of_range_throw->kind != W_SEED_HIR0_TERMINATOR_THROW ||
      out_of_range_throw->value_index != 3u ||
      out_of_range_throw->result_type != error_type_index ||
      entry_return->kind != W_SEED_HIR0_TERMINATOR_RETURN_UNIT)
    return false;

  const w_seed_hir0_block_argument *normal_argument =
      &program->block_arguments[0];
  const w_seed_hir0_block_argument *non_finite_argument =
      &program->block_arguments[1];
  const w_seed_hir0_block_argument *out_of_range_argument =
      &program->block_arguments[2];
  const w_seed_hir0_value *source_value = &program->values[0];
  const w_seed_hir0_value *normal_value = &program->values[1];
  const w_seed_hir0_value *non_finite_value = &program->values[2];
  const w_seed_hir0_value *out_of_range_value = &program->values[3];
  if (source_value->kind != W_SEED_HIR0_VALUE_PARAMETER_READ ||
      source_value->owner_kind != W_SEED_HIR0_VALUE_OWNER_TERMINATOR ||
      source_value->owner_index != 0u || source_value->owner_ordinal != 0u ||
      source_value->type_index != parameter->type_index ||
      source_value->parameter_index != 0u ||
      normal_argument->owner_block != 1u || normal_argument->ordinal != 0u ||
      normal_argument->type_index != function->return_type ||
      non_finite_argument->owner_block != 2u ||
      non_finite_argument->ordinal != 0u ||
      non_finite_argument->type_index != error_type_index ||
      out_of_range_argument->owner_block != 3u ||
      out_of_range_argument->ordinal != 0u ||
      out_of_range_argument->type_index != error_type_index ||
      normal_value->kind != W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ ||
      normal_value->type_index != function->return_type ||
      normal_value->block_argument_index != 0u ||
      non_finite_value->kind != W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ ||
      non_finite_value->type_index != error_type_index ||
      non_finite_value->block_argument_index != 1u ||
      out_of_range_value->kind != W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ ||
      out_of_range_value->type_index != error_type_index ||
      out_of_range_value->block_argument_index != 2u)
    return false;

  *selection = (w_seed_native_subset0_float_to_integer_rounding){
      .entry = entry,
      .function = function,
      .source_parameter = parameter,
      .source_value = source_value,
      .conversion = conversion,
      .normal_argument = normal_argument,
      .non_finite_argument = non_finite_argument,
      .out_of_range_argument = out_of_range_argument,
      .normal_return = normal_return,
      .non_finite_throw = non_finite_throw,
      .out_of_range_throw = out_of_range_throw,
      .entry_index = 0u,
      .function_index = 0u,
      .entry_function_index = 1u,
      .split_block_index = 0u,
      .normal_block_index = 1u,
      .non_finite_block_index = 2u,
      .out_of_range_block_index = 3u,
      .source_type_index = parameter->type_index,
      .destination_type_index = function->return_type,
      .error_type_index = error_type_index,
      .source_bit_width = source_width,
      .destination_bit_width = destination_facts.bit_width,
      .destination_is_signed = destination_facts.is_signed,
      .rounding_mode = conversion->rounding_mode};
  return true;
}

w_seed_native_subset0_status
w_seed_native_subset0_select_float_to_integer_rounding(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_float_to_integer_rounding *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return W_SEED_NATIVE_SUBSET0_INVALID;
  w_seed_native_subset0_float_to_integer_rounding candidate;
  if (!float_to_integer_rounding_selection_derive(program, &candidate))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  *selection = candidate;
  return W_SEED_NATIVE_SUBSET0_OK;
}

bool w_seed_native_subset0_verify_float_to_integer_rounding(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const w_seed_native_subset0_float_to_integer_rounding *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return false;
  w_seed_native_subset0_float_to_integer_rounding candidate;
  if (!float_to_integer_rounding_selection_derive(program, &candidate))
    return false;
  return selection->entry == candidate.entry &&
         selection->function == candidate.function &&
         selection->source_parameter == candidate.source_parameter &&
         selection->source_value == candidate.source_value &&
         selection->conversion == candidate.conversion &&
         selection->normal_argument == candidate.normal_argument &&
         selection->non_finite_argument == candidate.non_finite_argument &&
         selection->out_of_range_argument == candidate.out_of_range_argument &&
         selection->normal_return == candidate.normal_return &&
         selection->non_finite_throw == candidate.non_finite_throw &&
         selection->out_of_range_throw == candidate.out_of_range_throw &&
         selection->entry_index == candidate.entry_index &&
         selection->function_index == candidate.function_index &&
         selection->entry_function_index == candidate.entry_function_index &&
         selection->split_block_index == candidate.split_block_index &&
         selection->normal_block_index == candidate.normal_block_index &&
         selection->non_finite_block_index ==
             candidate.non_finite_block_index &&
         selection->out_of_range_block_index ==
             candidate.out_of_range_block_index &&
         selection->source_type_index == candidate.source_type_index &&
         selection->destination_type_index ==
             candidate.destination_type_index &&
         selection->error_type_index == candidate.error_type_index &&
         selection->source_bit_width == candidate.source_bit_width &&
         selection->destination_bit_width ==
             candidate.destination_bit_width &&
         selection->destination_is_signed ==
             candidate.destination_is_signed &&
         selection->rounding_mode == candidate.rounding_mode;
}

w_seed_native_subset0_status w_seed_native_subset0_select_program(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_program *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return W_SEED_NATIVE_SUBSET0_INVALID;
  if (program->cleanup_count != 0u || program->module_count == 0u ||
      program->module_count > W_SEED_NATIVE_SUBSET0_MAX_MODULES ||
      program->function_count == 0u ||
      program->function_count > W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS ||
      program->parameter_count > W_SEED_NATIVE_SUBSET0_MAX_PARAMETERS ||
      program->block_count == 0u ||
      program->block_count > W_SEED_NATIVE_SUBSET0_MAX_BLOCKS ||
      program->instruction_count > W_SEED_NATIVE_SUBSET0_MAX_INSTRUCTIONS ||
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
            program, function, NULL, bindings, binding_reads, state, cached,
            &has_interpolation, &has_local_calls, false))
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  bool has_reachable_panic = false;
  if (!native_program_reachable_panic(program, entry->target_function,
                                      &has_reachable_panic))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  if ((!has_reachable_panic &&
       (program->instruction_count == 0u || program->call_count == 0u)) ||
      (!has_reachable_panic && cached[entry->target_function] == 0u))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  bool has_cfg = false;
  for (size_t function = 0u; function < program->function_count;
       function += 1u)
    if (program->functions[function].block_count > 1u) has_cfg = true;
  bool has_u64_linear_value = false;
  for (size_t value = 0u; value < program->value_count; value += 1u)
    if (program->values[value].kind == W_SEED_HIR0_VALUE_BINARY_U64 ||
        program->values[value].kind == W_SEED_HIR0_VALUE_UNARY_U64 ||
        program->values[value].kind == W_SEED_HIR0_VALUE_TUPLE_ELEMENT) {
      has_u64_linear_value = true;
      break;
    }
  /* The finite U64 bundle is linear/local-call only. Existing i64-carrier
   * shift/power loop forms remain governed by their established recognizers,
   * but ordinary BINARY_U64/UNARY_U64 never crosses a CFG/loop boundary here. */
  if (has_u64_linear_value && has_cfg)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  bool has_bool = false;
  for (size_t value = 0u; value < program->value_count; value += 1u)
    if (program->values[value].type_index < program->type_count &&
        program->types[program->values[value].type_index].kind ==
            W_SEED_HIR0_TYPE_BOOL)
      has_bool = true;
  bool has_enum_switch = false;
  for (size_t function = 0u; function < program->function_count;
       function += 1u)
    if (program_enum_switch_is_supported(program, function))
      has_enum_switch = true;
  bool has_mutable_bindings = false;
  for (size_t binding = 0u; binding < program->binding_count; binding += 1u)
    if (program->bindings[binding].is_mutable ||
        program->bindings[binding].source_binding != binding) {
      has_mutable_bindings = true;
      break;
    }
  bool natural_loop_functions[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS] = {false};
  bool post_test_loop_functions[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS] = {false};
  for (size_t function = 0u; function < program->function_count;
       function += 1u)
    natural_loop_functions[function] =
        program_natural_loop_is_supported(program, function);
  for (size_t function = 0u; function < program->function_count;
       function += 1u)
    post_test_loop_functions[function] =
        program_post_test_loop_is_supported(program, function);
  *selection = (w_seed_native_subset0_program){
      .entry = entry,
      .module_count = program->module_count,
      .function_count = program->function_count,
      .parameter_count = program->parameter_count,
      .instruction_count = program->instruction_count,
      .binding_count = program->binding_count,
      .call_count = program->call_count,
      .maximum_stdout_bytes = cached[entry->target_function],
      .has_reachable_panic = has_reachable_panic,
      .has_interpolation = has_interpolation,
      .has_bool = has_bool,
      .has_local_calls = has_local_calls,
      .has_cfg = has_cfg,
      .has_enum_switch = has_enum_switch,
      .has_mutable_bindings = has_mutable_bindings};
  (void)memcpy(selection->natural_loop_functions, natural_loop_functions,
               sizeof(natural_loop_functions));
  (void)memcpy(selection->post_test_loop_functions, post_test_loop_functions,
               sizeof(post_test_loop_functions));
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
  if (program->cleanup_count != 0u || program->module_count != 1u ||
      program->external_module_count != 1u ||
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
  selection->function_index = (uint32_t)(function - program->functions);
  selection->arguments_symbol_index = (uint32_t)arguments_symbol;
  selection->context_symbol_index = (uint32_t)context_symbol;
  selection->exit_code_symbol_index = (uint32_t)exit_code_symbol;
  selection->is_empty_symbol_index = W_SEED_HIR0_NONE;
  selection->count_symbol_index = W_SEED_HIR0_NONE;
  selection->success_symbol_index = (uint32_t)success_symbol;
  selection->failure_symbol_index = W_SEED_HIR0_NONE;
  selection->arguments_parameter_ordinal = 0u;
  selection->context_parameter_ordinal = 1u;
  return W_SEED_NATIVE_SUBSET0_OK;
}

static bool process_host_call_supported(
    const w_seed_hir0_program *program, const w_seed_hir0_call *call,
    uint32_t owner_function,
    const w_seed_native_subset0_process *process) {
  if (program == NULL || call == NULL || process == NULL ||
      call->callee_identity >= program->identity_count ||
      call->first_argument > program->argument_count ||
      call->argument_count != 1u || call->first_requirement >=
          program->requirement_count || call->requirement_count != 1u)
    return false;
  const w_seed_hir0_identity *callee =
      &program->identities[call->callee_identity];
  const w_seed_hir0_requirement *requirement =
      &program->requirements[call->first_requirement];
  const w_seed_hir0_argument *argument =
      &program->arguments[call->first_argument];
  if (callee->kind != W_SEED_HIR0_IDENTITY_HOST_PRELUDE ||
      !text_is(program, callee->name, (const uint8_t *)"print", 5u) ||
      !text_is(program, callee->profile, NATIVE_SUBSET0_PROFILE,
               sizeof(NATIVE_SUBSET0_PROFILE) - 1u) ||
      requirement->owner_kind != W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY ||
      requirement->owner_index != call->callee_identity ||
      !text_is(program, requirement->name, NATIVE_SUBSET0_REQUIREMENT,
               sizeof(NATIVE_SUBSET0_REQUIREMENT) - 1u) ||
      argument->owner_call != (uint32_t)(call - program->calls) ||
      argument->parameter_ordinal != 0u || argument->type_index >=
                                               program->type_count ||
      program->types[argument->type_index].kind != W_SEED_HIR0_TYPE_STRING ||
      argument->value_index >= program->value_count)
    return false;
  return process_value_lowerable(program, argument->value_index,
                                 owner_function, process, true, 0u) &&
         program->values[argument->value_index].type_index ==
             argument->type_index;
}

static bool process_print_observes_binding(
    const w_seed_hir0_program *program, const w_seed_hir0_call *call,
    uint32_t owner_function, const w_seed_native_subset0_process *process,
    uint32_t binding_index, uint32_t type_index) {
  if (program == NULL || call == NULL || process == NULL ||
      !process_host_call_supported(program, call, owner_function, process) ||
      call->first_argument >= program->argument_count)
    return false;
  const w_seed_hir0_argument *argument =
      &program->arguments[call->first_argument];
  const w_seed_hir0_value *root = &program->values[argument->value_index];
  if (root->kind != W_SEED_HIR0_VALUE_INTERPOLATED_STRING ||
      root->first_interpolation_segment >
          program->interpolation_segment_count ||
      root->interpolation_segment_count >
          program->interpolation_segment_count -
              root->first_interpolation_segment)
    return false;
  size_t dynamic_values = 0u;
  for (size_t ordinal = 0u; ordinal < root->interpolation_segment_count;
       ordinal += 1u) {
    const w_seed_hir0_interpolation_segment *segment =
        &program->interpolation_segments[
            (size_t)root->first_interpolation_segment + ordinal];
    if (segment->kind == W_SEED_HIR0_INTERPOLATION_TEXT) continue;
    if (segment->kind != W_SEED_HIR0_INTERPOLATION_VALUE ||
        segment->value_index >= program->value_count)
      return false;
    const w_seed_hir0_value *value = &program->values[segment->value_index];
    if (value->kind != W_SEED_HIR0_VALUE_BINDING_READ ||
        value->binding_index != binding_index || value->type_index != type_index)
      return false;
    dynamic_values += 1u;
  }
  return dynamic_values == 1u;
}

static bool process_local_call_supported(
    const w_seed_hir0_program *program, const w_seed_hir0_call *call,
    uint32_t owner_function,
    const w_seed_native_subset0_process *process,
    const w_seed_parallel_selection0 *parallel_selection) {
  if (program == NULL || call == NULL || process == NULL ||
      call->callee_identity >= program->identity_count ||
      call->first_argument > program->argument_count ||
      call->argument_count > program->argument_count - call->first_argument)
    return false;
  const size_t call_index = (size_t)(call - program->calls);
  const bool process_parallel_dispatch =
      parallel_selection != NULL &&
      parallel_selection->task_count == 1u &&
      parallel_selection->root_function_index == process->function_index &&
      call_index == parallel_selection->task_call_indices[0] &&
      call->execution_kind ==
          W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH;
  if (call->execution_kind != W_SEED_HIR0_CALL_DIRECT &&
      !process_parallel_dispatch)
    return false;
  const w_seed_hir0_identity *callee =
      &program->identities[call->callee_identity];
  if (callee->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      callee->target_index >= program->function_count ||
      callee->target_index == process->function_index ||
      call->argument_count != callee->parameter_count)
    return false;
  const w_seed_hir0_function *target = &program->functions[callee->target_index];
  if (call->result_type != target->return_type ||
      call->result_type >= program->type_count)
    return false;
  uint32_t values[W_SEED_NATIVE_SUBSET0_MAX_PARAMETERS];
  if (call->argument_count > sizeof(values) / sizeof(values[0])) return false;
  for (size_t index = 0u; index < sizeof(values) / sizeof(values[0]); index += 1u)
    values[index] = W_SEED_HIR0_NONE;
  for (size_t ordinal = 0u; ordinal < call->argument_count; ordinal += 1u) {
    const w_seed_hir0_argument *argument =
        &program->arguments[(size_t)call->first_argument + ordinal];
    if (argument->parameter_ordinal >= call->argument_count ||
        values[argument->parameter_ordinal] != W_SEED_HIR0_NONE ||
        argument->value_index >= program->value_count)
      return false;
    values[argument->parameter_ordinal] = argument->value_index;
  }
  for (size_t ordinal = 0u; ordinal < target->parameter_count; ordinal += 1u) {
    const size_t parameter_index = (size_t)target->first_parameter + ordinal;
    if (parameter_index >= program->parameter_count ||
        values[ordinal] == W_SEED_HIR0_NONE ||
        program->parameters[parameter_index].type_index >= program->type_count ||
        program->values[values[ordinal]].type_index !=
            program->parameters[parameter_index].type_index ||
        !process_value_lowerable(program, values[ordinal], owner_function,
                                 process, false, 0u))
      return false;
  }
  return program->types[call->result_type].kind !=
             W_SEED_HIR0_TYPE_NOMINAL &&
         (call->result_type == 0u ||
          program->types[call->result_type].kind == W_SEED_HIR0_TYPE_I64 ||
          program->types[call->result_type].kind == W_SEED_HIR0_TYPE_U64 ||
          program->types[call->result_type].kind == W_SEED_HIR0_TYPE_INTEGER ||
          program->types[call->result_type].kind == W_SEED_HIR0_TYPE_BOOL ||
          program_enum_type_supported(program, call->result_type, NULL, NULL));
}

/* The native-process adapter owns one deliberately narrow typed-error route:
 * a three-block integer-exactly split whose normal arm returns ExitCode.success
 * and whose error arm throws the verified NumericConversionError value.  The
 * error payload is an internal HIR fact only; the process ABI receives status
 * 1 after the same Context-then-Arguments owner release used by every
 * structured outcome. */
static bool process_integer_exact_root_supported(
    const w_seed_hir0_program *program,
    w_seed_native_subset0_process *process) {
  if (program == NULL || process == NULL || process->function == NULL ||
      process->function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = process->function;
  if (!function->is_throws || function->error_type >= program->type_count ||
      program->types[function->error_type].kind !=
          W_SEED_HIR0_TYPE_NUMERIC_CONVERSION_ERROR ||
      function->first_block >= program->block_count ||
      function->block_count != 3u ||
      function->block_count > program->block_count - function->first_block ||
      program->call_count > 1u || program->binding_count != 1u)
    return false;

  const uint32_t split_index = function->first_block;
  const w_seed_hir0_block *split = &program->blocks[split_index];
  if (split->owner_function != process->function_index ||
      split->terminator_index >= program->terminator_count ||
      split->instruction_count != 0u || split->block_argument_count != 0u)
    return false;
  const w_seed_hir0_terminator *conversion =
      &program->terminators[split->terminator_index];
  if (conversion->owner_block != split_index ||
      conversion->kind != W_SEED_HIR0_TERMINATOR_INTEGER_EXACTLY ||
      conversion->target_block < function->first_block ||
      conversion->target_block >= function->first_block + function->block_count ||
      conversion->else_block < function->first_block ||
      conversion->else_block >= function->first_block + function->block_count ||
      conversion->target_block == conversion->else_block ||
      conversion->value_index >= program->value_count ||
      conversion->result_type >= program->type_count ||
      conversion->error_type != function->error_type ||
      conversion->numeric_conversion_error_case !=
          W_SEED_HIR0_NUMERIC_CONVERSION_ERROR_OUT_OF_RANGE)
    return false;

  native_integer_facts source_facts;
  native_integer_facts destination_facts;
  const w_seed_hir0_value *source = &program->values[conversion->value_index];
  const bool target_usize_source =
      source->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
      source->type_index < program->type_count &&
      program->types[source->type_index].kind == W_SEED_HIR0_TYPE_USIZE &&
      source->external_module_index == 0u &&
      source->external_symbol_index == process->count_symbol_index &&
      text_is(program, source->member_name, (const uint8_t *)"count", 5u);
  if ((!native_integer_type_facts(program, source->type_index, &source_facts) &&
       !target_usize_source) ||
      !native_integer_type_facts(program, conversion->result_type,
                                 &destination_facts) ||
      !process_value_lowerable(program, conversion->value_index,
                               process->function_index, process, false, 0u))
    return false;
  if (target_usize_source) {
    source_facts.is_signed = false;
    source_facts.bit_width = 0u;
  }

  const uint32_t normal_index = conversion->target_block;
  const uint32_t error_index = conversion->else_block;
  const w_seed_hir0_block *normal = &program->blocks[normal_index];
  const w_seed_hir0_block *error = &program->blocks[error_index];
  if (normal->owner_function != process->function_index ||
      error->owner_function != process->function_index ||
      normal->block_argument_count != 1u ||
      error->block_argument_count != 1u ||
      normal->first_block_argument >= program->block_argument_count ||
      error->first_block_argument >= program->block_argument_count ||
      normal->instruction_count != 1u + program->call_count ||
      normal->first_instruction >= program->instruction_count ||
      normal->instruction_count >
          program->instruction_count - normal->first_instruction ||
      error->instruction_count != 0u ||
      normal->terminator_index >= program->terminator_count ||
      error->terminator_index >= program->terminator_count)
    return false;
  const w_seed_hir0_block_argument *normal_argument =
      &program->block_arguments[normal->first_block_argument];
  const w_seed_hir0_block_argument *error_argument =
      &program->block_arguments[error->first_block_argument];
  if (normal_argument->owner_block != normal_index ||
      normal_argument->ordinal != 0u ||
      normal_argument->type_index != conversion->result_type ||
      error_argument->owner_block != error_index ||
      error_argument->ordinal != 0u ||
      error_argument->type_index != function->error_type)
    return false;
  const w_seed_hir0_instruction *instruction =
      &program->instructions[normal->first_instruction];
  if (instruction->owner_block != normal_index || instruction->ordinal != 0u ||
      instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
      instruction->binding_index >= program->binding_count)
    return false;
  const w_seed_hir0_binding *binding =
      &program->bindings[instruction->binding_index];
  if (binding->owner_block != normal_index || binding->is_mutable ||
      binding->type_index != conversion->result_type ||
      binding->initializer_value >= program->value_count)
    return false;
  const w_seed_hir0_value *initializer =
      &program->values[binding->initializer_value];
  if (initializer->kind != W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ ||
      initializer->owner_kind != W_SEED_HIR0_VALUE_OWNER_BINDING ||
      initializer->owner_index != instruction->binding_index ||
      initializer->owner_ordinal != 0u ||
      initializer->type_index != conversion->result_type ||
      initializer->block_argument_index != normal->first_block_argument)
    return false;

  size_t maximum_stdout_bytes = 0u;
  if (program->call_count == 1u) {
    const w_seed_hir0_instruction *observation = instruction + 1;
    if (observation->owner_block != normal_index ||
        observation->ordinal != 1u ||
        observation->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
        observation->call_index >= program->call_count)
      return false;
    const w_seed_hir0_call *print = &program->calls[observation->call_index];
    if (print->owner_instruction != normal->first_instruction + 1u ||
        print->owner_terminator != W_SEED_HIR0_NONE ||
        print->owner_block != normal_index || print->ordinal != 1u ||
        !process_host_call_supported(program, print, process->function_index,
                                     process))
      return false;
    const w_seed_hir0_binding *bindings[W_SEED_NATIVE_SUBSET0_MAX_BINDINGS] =
        {NULL};
    size_t binding_reads[W_SEED_NATIVE_SUBSET0_MAX_BINDINGS] = {0u};
    for (size_t index = 0u; index < program->binding_count; index += 1u)
      bindings[index] = &program->bindings[index];
    bool has_interpolation = false;
    if (!program_host_print_maximum(program, print, bindings, binding_reads,
                                    &maximum_stdout_bytes,
                                    &has_interpolation, process) ||
        !has_interpolation || maximum_stdout_bytes == 0u)
      return false;
  }

  const w_seed_hir0_terminator *normal_return =
      &program->terminators[normal->terminator_index];
  const w_seed_hir0_terminator *error_throw =
      &program->terminators[error->terminator_index];
  if (normal_return->owner_block != normal_index ||
      normal_return->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      normal_return->result_type != function->return_type ||
      normal_return->value_index >= program->value_count ||
      error_throw->owner_block != error_index ||
      error_throw->kind != W_SEED_HIR0_TERMINATOR_THROW ||
      error_throw->result_type != function->error_type ||
      error_throw->value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *success =
      &program->values[normal_return->value_index];
  const w_seed_hir0_value *error_value =
      &program->values[error_throw->value_index];
  if (success->kind != W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE ||
      success->type_index != function->return_type ||
      success->external_module_index != 0u ||
      success->external_symbol_index != process->success_symbol_index ||
      success->left_value != W_SEED_HIR0_NONE ||
      !text_is(program, success->member_name, (const uint8_t *)"success", 7u) ||
      error_value->kind != W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ ||
      error_value->owner_kind != W_SEED_HIR0_VALUE_OWNER_TERMINATOR ||
      error_value->owner_index != error->terminator_index ||
      error_value->owner_ordinal != 0u ||
      error_value->type_index != function->error_type ||
      error_value->block_argument_index != error->first_block_argument)
    return false;

  process->exact_source_value = source;
  process->exact_conversion = conversion;
  process->exact_normal_return = normal_return;
  process->exact_error_throw = error_throw;
  process->exact_split_block_index = split_index;
  process->exact_normal_block_index = normal_index;
  process->exact_error_block_index = error_index;
  process->exact_source_type_index = source->type_index;
  process->exact_destination_type_index = conversion->result_type;
  process->exact_error_type_index = function->error_type;
  process->exact_source_bit_width = source_facts.bit_width;
  process->exact_destination_bit_width = destination_facts.bit_width;
  process->exact_source_is_signed = source_facts.is_signed;
  process->exact_destination_is_signed = destination_facts.is_signed;
  process->exact_source_is_target_usize = target_usize_source;
  process->maximum_stdout_bytes = maximum_stdout_bytes;
  process->has_integer_exactly = true;
  return true;
}

/* Admit the bounded native-process witness whose body rounds one compile-time
 * float and publishes the normal, non-finite, and out-of-range edges.  This
 * is a separate relation from INTEGER_EXACTLY: the source remains a constant
 * float, the result is carried only through the normal block argument, and
 * both typed error arms are consumed by the process cleanup contract. */
static bool process_float_to_integer_rounding_root_supported(
    const w_seed_hir0_program *program,
    w_seed_native_subset0_process *process) {
  if (program == NULL || process == NULL || process->function == NULL ||
      process->function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = process->function;
  if (!function->is_throws || function->error_type >= program->type_count ||
      program->types[function->error_type].kind !=
          W_SEED_HIR0_TYPE_NUMERIC_CONVERSION_ERROR ||
      function->first_block >= program->block_count ||
      function->block_count != 4u ||
      function->block_count > program->block_count - function->first_block ||
      program->call_count > 1u || program->binding_count != 1u ||
      program->instruction_count != 1u + program->call_count)
    return false;

  const uint32_t split_index = function->first_block;
  const uint32_t block_limit = function->first_block + function->block_count;
  const w_seed_hir0_block *split = &program->blocks[split_index];
  if (split->owner_function != process->function_index ||
      split->terminator_index >= program->terminator_count ||
      split->instruction_count != 0u || split->block_argument_count != 0u)
    return false;
  const w_seed_hir0_terminator *conversion =
      &program->terminators[split->terminator_index];
  const uint32_t normal_index = conversion->target_block;
  const uint32_t non_finite_index = conversion->else_block;
  const uint32_t out_of_range_index = conversion->third_block;
  if (conversion->owner_block != split_index ||
      conversion->kind != W_SEED_HIR0_TERMINATOR_FLOAT_TO_INTEGER_ROUNDING ||
      conversion->error_type != function->error_type ||
      conversion->numeric_conversion_error_case !=
          W_SEED_HIR0_NUMERIC_CONVERSION_ERROR_NONE ||
      conversion->rounding_mode < W_SEED_HIR0_ROUNDING_MODE_NEAREST_EVEN ||
      conversion->rounding_mode > W_SEED_HIR0_ROUNDING_MODE_TOWARD_NEGATIVE ||
      conversion->value_index >= program->value_count ||
      conversion->result_type >= program->type_count ||
      normal_index < function->first_block || normal_index >= block_limit ||
      non_finite_index < function->first_block ||
      non_finite_index >= block_limit ||
      out_of_range_index < function->first_block ||
      out_of_range_index >= block_limit || normal_index == split_index ||
      non_finite_index == split_index || out_of_range_index == split_index ||
      normal_index == non_finite_index || normal_index == out_of_range_index ||
      non_finite_index == out_of_range_index)
    return false;

  uint16_t source_width = 0u;
  native_integer_facts destination_facts;
  const w_seed_hir0_value *source = &program->values[conversion->value_index];
  if (source->kind != W_SEED_HIR0_VALUE_CONST_FLOAT ||
      !native_float_type_width(program, source->type_index, &source_width) ||
      !native_integer_type_facts(program, conversion->result_type,
                                 &destination_facts))
    return false;

  const w_seed_hir0_block *normal = &program->blocks[normal_index];
  const w_seed_hir0_block *non_finite = &program->blocks[non_finite_index];
  const w_seed_hir0_block *out_of_range =
      &program->blocks[out_of_range_index];
  if (normal->owner_function != process->function_index ||
      non_finite->owner_function != process->function_index ||
      out_of_range->owner_function != process->function_index ||
      normal->instruction_count != 1u + program->call_count ||
      non_finite->instruction_count != 0u ||
      out_of_range->instruction_count != 0u ||
      normal->block_argument_count != 1u ||
      non_finite->block_argument_count != 1u ||
      out_of_range->block_argument_count != 1u ||
      normal->first_instruction >= program->instruction_count ||
      normal->instruction_count >
          program->instruction_count - normal->first_instruction ||
      non_finite->first_instruction > program->instruction_count ||
      out_of_range->first_instruction > program->instruction_count ||
      normal->first_block_argument >= program->block_argument_count ||
      non_finite->first_block_argument >= program->block_argument_count ||
      out_of_range->first_block_argument >= program->block_argument_count ||
      normal->terminator_index >= program->terminator_count ||
      non_finite->terminator_index >= program->terminator_count ||
      out_of_range->terminator_index >= program->terminator_count)
    return false;
  const w_seed_hir0_block_argument *normal_argument =
      &program->block_arguments[normal->first_block_argument];
  const w_seed_hir0_block_argument *non_finite_argument =
      &program->block_arguments[non_finite->first_block_argument];
  const w_seed_hir0_block_argument *out_of_range_argument =
      &program->block_arguments[out_of_range->first_block_argument];
  if (normal_argument->owner_block != normal_index ||
      normal_argument->ordinal != 0u ||
      normal_argument->type_index != conversion->result_type ||
      non_finite_argument->owner_block != non_finite_index ||
      non_finite_argument->ordinal != 0u ||
      non_finite_argument->type_index != function->error_type ||
      out_of_range_argument->owner_block != out_of_range_index ||
      out_of_range_argument->ordinal != 0u ||
      out_of_range_argument->type_index != function->error_type)
    return false;

  const w_seed_hir0_instruction *instruction =
      &program->instructions[normal->first_instruction];
  if (instruction->owner_block != normal_index || instruction->ordinal != 0u ||
      instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
      instruction->binding_index >= program->binding_count)
    return false;
  const w_seed_hir0_binding *binding =
      &program->bindings[instruction->binding_index];
  if (binding->owner_block != normal_index || binding->is_mutable ||
      binding->type_index != conversion->result_type ||
      binding->initializer_value >= program->value_count)
    return false;
  const w_seed_hir0_value *initializer =
      &program->values[binding->initializer_value];
  if (initializer->kind != W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ ||
      initializer->owner_kind != W_SEED_HIR0_VALUE_OWNER_BINDING ||
      initializer->owner_index != instruction->binding_index ||
      initializer->owner_ordinal != 0u ||
      initializer->type_index != conversion->result_type ||
      initializer->block_argument_index != normal->first_block_argument)
    return false;

  size_t maximum_stdout_bytes = 0u;
  if (program->call_count == 1u) {
    const w_seed_hir0_instruction *observation = instruction + 1;
    if (observation->owner_block != normal_index || observation->ordinal != 1u ||
        observation->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
        observation->call_index >= program->call_count)
      return false;
    const w_seed_hir0_call *print = &program->calls[observation->call_index];
    const w_seed_hir0_binding *bindings[W_SEED_NATIVE_SUBSET0_MAX_BINDINGS] =
        {NULL};
    size_t binding_reads[W_SEED_NATIVE_SUBSET0_MAX_BINDINGS] = {0u};
    for (size_t index = 0u; index < program->binding_count; index += 1u)
      bindings[index] = &program->bindings[index];
    bool has_interpolation = false;
    if (!process_print_observes_binding(
            program, print, process->function_index, process,
            instruction->binding_index, conversion->result_type) ||
        !program_host_print_maximum(program, print, bindings, binding_reads,
                                    &maximum_stdout_bytes,
                                    &has_interpolation, process) ||
        !has_interpolation || maximum_stdout_bytes == 0u)
      return false;
  }

  const w_seed_hir0_terminator *normal_return =
      &program->terminators[normal->terminator_index];
  const w_seed_hir0_terminator *non_finite_throw =
      &program->terminators[non_finite->terminator_index];
  const w_seed_hir0_terminator *out_of_range_throw =
      &program->terminators[out_of_range->terminator_index];
  if (normal_return->owner_block != normal_index ||
      normal_return->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
      normal_return->result_type != function->return_type ||
      normal_return->error_type != W_SEED_HIR0_NONE ||
      normal_return->target_block != W_SEED_HIR0_NONE ||
      normal_return->else_block != W_SEED_HIR0_NONE ||
      normal_return->third_block != W_SEED_HIR0_NONE ||
      normal_return->value_index >= program->value_count ||
      non_finite_throw->owner_block != non_finite_index ||
      non_finite_throw->kind != W_SEED_HIR0_TERMINATOR_THROW ||
      non_finite_throw->result_type != function->error_type ||
      non_finite_throw->error_type != W_SEED_HIR0_NONE ||
      non_finite_throw->target_block != W_SEED_HIR0_NONE ||
      non_finite_throw->else_block != W_SEED_HIR0_NONE ||
      non_finite_throw->third_block != W_SEED_HIR0_NONE ||
      non_finite_throw->value_index >= program->value_count ||
      out_of_range_throw->owner_block != out_of_range_index ||
      out_of_range_throw->kind != W_SEED_HIR0_TERMINATOR_THROW ||
      out_of_range_throw->result_type != function->error_type ||
      out_of_range_throw->error_type != W_SEED_HIR0_NONE ||
      out_of_range_throw->target_block != W_SEED_HIR0_NONE ||
      out_of_range_throw->else_block != W_SEED_HIR0_NONE ||
      out_of_range_throw->third_block != W_SEED_HIR0_NONE ||
      out_of_range_throw->value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *success =
      &program->values[normal_return->value_index];
  const w_seed_hir0_value *non_finite_value =
      &program->values[non_finite_throw->value_index];
  const w_seed_hir0_value *out_of_range_value =
      &program->values[out_of_range_throw->value_index];
  if (success->kind != W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE ||
      success->type_index != function->return_type ||
      success->external_module_index != 0u ||
      success->external_symbol_index != process->success_symbol_index ||
      success->left_value != W_SEED_HIR0_NONE ||
      !text_is(program, success->member_name, (const uint8_t *)"success", 7u) ||
      non_finite_value->kind != W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ ||
      non_finite_value->owner_kind != W_SEED_HIR0_VALUE_OWNER_TERMINATOR ||
      non_finite_value->owner_index != non_finite->terminator_index ||
      non_finite_value->owner_ordinal != 0u ||
      non_finite_value->type_index != function->error_type ||
      non_finite_value->block_argument_index !=
          non_finite->first_block_argument ||
      out_of_range_value->kind != W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ ||
      out_of_range_value->owner_kind != W_SEED_HIR0_VALUE_OWNER_TERMINATOR ||
      out_of_range_value->owner_index != out_of_range->terminator_index ||
      out_of_range_value->owner_ordinal != 0u ||
      out_of_range_value->type_index != function->error_type ||
      out_of_range_value->block_argument_index !=
          out_of_range->first_block_argument)
    return false;

  process->rounding_source_value = source;
  process->rounding_conversion = conversion;
  process->rounding_normal_return = normal_return;
  process->rounding_non_finite_throw = non_finite_throw;
  process->rounding_out_of_range_throw = out_of_range_throw;
  process->rounding_split_block_index = split_index;
  process->rounding_normal_block_index = normal_index;
  process->rounding_non_finite_block_index = non_finite_index;
  process->rounding_out_of_range_block_index = out_of_range_index;
  process->rounding_source_type_index = source->type_index;
  process->rounding_destination_type_index = conversion->result_type;
  process->rounding_error_type_index = function->error_type;
  process->rounding_source_bit_width = source_width;
  process->rounding_destination_bit_width = destination_facts.bit_width;
  process->rounding_destination_is_signed = destination_facts.is_signed;
  process->rounding_mode = conversion->rounding_mode;
  process->maximum_stdout_bytes = maximum_stdout_bytes;
  process->has_float_to_integer_rounding = true;
  return true;
}

static bool process_function_body_supported(
    const w_seed_hir0_program *program,
    const w_seed_native_subset0_process *process,
    const w_seed_parallel_selection0 *parallel_selection) {
  if (program == NULL || process == NULL || process->function == NULL ||
      process->function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = process->function;
  if (function->first_block >= program->block_count || function->block_count == 0u ||
      function->block_count > program->block_count - function->first_block)
    return false;
  const size_t start = function->first_block;
  const size_t end = start + function->block_count;
  for (size_t ordinal = 0u; ordinal < function->block_count; ordinal += 1u) {
    const size_t block_index = start + ordinal;
    const w_seed_hir0_block *block = &program->blocks[block_index];
    if (block->owner_function != process->function_index ||
        block->terminator_index >= program->terminator_count ||
        block->first_instruction > program->instruction_count ||
        block->instruction_count >
            program->instruction_count - block->first_instruction)
      return false;
    for (size_t instruction_ordinal = 0u;
         instruction_ordinal < block->instruction_count; instruction_ordinal += 1u) {
      const size_t instruction_index =
          (size_t)block->first_instruction + instruction_ordinal;
      const w_seed_hir0_instruction *instruction =
          &program->instructions[instruction_index];
      if (instruction->owner_block != block_index ||
          instruction->ordinal != instruction_ordinal)
        return false;
      if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
        if (instruction->binding_index >= program->binding_count ||
            !process_value_lowerable(
                program,
                program->bindings[instruction->binding_index].initializer_value,
                process->function_index, process, true, 0u))
          return false;
        continue;
      }
      if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
          instruction->call_index >= program->call_count)
        return false;
      const w_seed_hir0_call *call = &program->calls[instruction->call_index];
      if (call->owner_instruction != instruction_index ||
          call->owner_block != block_index ||
          call->result_type >= program->type_count)
        return false;
      if (call->callee_identity >= program->identity_count)
        return false;
      const w_seed_hir0_identity *callee =
          &program->identities[call->callee_identity];
      if (callee->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE) {
        if (!process_host_call_supported(program, call,
                                         process->function_index, process))
          return false;
      } else if (callee->kind == W_SEED_HIR0_IDENTITY_FUNCTION) {
        if (!process_local_call_supported(program, call,
                                          process->function_index, process,
                                          parallel_selection))
          return false;
      } else {
        return false;
      }
    }
    const w_seed_hir0_terminator *terminator =
        &program->terminators[block->terminator_index];
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_PANIC) {
      if (!native_panic_terminator_supported(
              program, block->terminator_index, process->function_index,
              terminator))
        return false;
      continue;
    }
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE) {
      if (terminator->value_index >= program->value_count ||
          terminator->target_block != W_SEED_HIR0_NONE ||
          terminator->else_block != W_SEED_HIR0_NONE ||
          program->values[terminator->value_index].type_index !=
              function->return_type ||
          !process_value_lowerable(program, terminator->value_index,
                                   process->function_index, process, false,
                                   0u))
        return false;
      continue;
    }
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT)
      return false;
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
      if (terminator->value_index >= program->value_count ||
          terminator->target_block < start || terminator->target_block >= end ||
          terminator->else_block < start || terminator->else_block >= end ||
          program->values[terminator->value_index].type_index >=
              program->type_count ||
          program->types[program->values[terminator->value_index].type_index]
                  .kind != W_SEED_HIR0_TYPE_BOOL ||
          !process_value_lowerable(program, terminator->value_index,
                                   process->function_index, process, false,
                                   0u))
        return false;
      continue;
    }
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_JUMP) {
      if (terminator->target_block < start || terminator->target_block >= end ||
          terminator->else_block != W_SEED_HIR0_NONE ||
          terminator->edge_argument_count > program->edge_argument_count ||
          (terminator->edge_argument_count != 0u &&
           (terminator->first_edge_argument == W_SEED_HIR0_NONE ||
            (size_t)terminator->first_edge_argument >
                program->edge_argument_count - terminator->edge_argument_count)))
        return false;
      for (size_t edge = 0u; edge < terminator->edge_argument_count; edge += 1u) {
        const w_seed_hir0_edge_argument *argument =
            &program->edge_arguments[(size_t)terminator->first_edge_argument + edge];
        if (argument->value_index >= program->value_count ||
            !process_value_lowerable(program, argument->value_index,
                                     process->function_index, process, false,
                                     0u))
          return false;
      }
      continue;
    }
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_SWITCH_ENUM) {
      if (terminator->value_index >= program->value_count ||
          terminator->switch_enum_index >= program->enum_count ||
          !program_enum_switch_is_supported(program, process->function_index) ||
          !process_value_lowerable(program, terminator->value_index,
                                   process->function_index, process, false,
                                   0u))
        return false;
      continue;
    }
    return false;
  }
  return true;
}

static w_seed_native_subset0_status
select_process_executable_mode(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *parallel_selection,
    w_seed_native_subset0_process *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return W_SEED_NATIVE_SUBSET0_INVALID;
  if (parallel_selection != NULL &&
      !w_seed_parallel_selection0_verify(program, hir_result,
                                         parallel_selection))
    return W_SEED_NATIVE_SUBSET0_INVALID;

  /* HIR verification proves the ownership/direct-entry contract.  The
   * executable consumer binds the exact public seven-symbol catalog; helper
   * functions, local enums, bindings, and source order remain independent of
   * that fixed external ABI. */
  if (program->cleanup_count != 0u || program->module_count != 1u ||
      program->external_module_count != 1u ||
      program->external_symbol_count != 7u ||
      program->external_symbol_count > W_SEED_NATIVE_SUBSET0_MAX_VALUES ||
      program->function_count == 0u ||
      program->function_count > W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS ||
      program->parameter_count > W_SEED_NATIVE_SUBSET0_MAX_PARAMETERS ||
      program->block_count == 0u ||
      program->block_count > W_SEED_NATIVE_SUBSET0_MAX_BLOCKS ||
      program->instruction_count > W_SEED_NATIVE_SUBSET0_MAX_INSTRUCTIONS *
                                        W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS ||
      program->binding_count > W_SEED_NATIVE_SUBSET0_MAX_BINDINGS ||
      program->call_count > W_SEED_NATIVE_SUBSET0_MAX_CALLS ||
      program->argument_count > W_SEED_NATIVE_SUBSET0_MAX_PARAMETERS *
                                    W_SEED_NATIVE_SUBSET0_MAX_CALLS ||
      program->value_count == 0u ||
      program->value_count > W_SEED_NATIVE_SUBSET0_MAX_VALUES ||
      program->terminator_count != program->block_count ||
      program->entry_count != 1u)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  const w_seed_hir0_external_module *external_module =
      &program->external_modules[0];
  if (external_module->module_index != 0u ||
      external_module->first_symbol > program->external_symbol_count ||
      external_module->symbol_count >
          program->external_symbol_count - external_module->first_symbol ||
      external_module->first_symbol != 0u ||
      external_module->symbol_count != 7u ||
      !text_is(program, external_module->module_id,
               (const uint8_t *)"std.process", 11u))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  size_t arguments_symbol = SIZE_MAX;
  size_t context_symbol = SIZE_MAX;
  size_t exit_code_symbol = SIZE_MAX;
  size_t is_empty_symbol = SIZE_MAX;
  size_t count_symbol = SIZE_MAX;
  size_t success_symbol = SIZE_MAX;
  size_t failure_symbol = SIZE_MAX;
  for (size_t symbol_index = external_module->first_symbol;
       symbol_index < (size_t)external_module->first_symbol +
                           external_module->symbol_count;
       symbol_index += 1u) {
    const w_seed_hir0_external_symbol *symbol =
        &program->external_symbols[symbol_index];
    if (symbol->module_index != 0u || !symbol->exported) continue;
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
             text_is(program, symbol->name, (const uint8_t *)"isEmpty", 7u))
      is_empty_symbol = symbol_index;
    else if (symbol->kind == W_SEED_HIR0_EXTERNAL_VALUE && symbol->is_const &&
             text_is(program, symbol->name, (const uint8_t *)"count", 5u))
      count_symbol = symbol_index;
    else if (symbol->kind == W_SEED_HIR0_EXTERNAL_VALUE && symbol->is_const &&
             text_is(program, symbol->name, (const uint8_t *)"success", 7u))
      success_symbol = symbol_index;
    else if (symbol->kind == W_SEED_HIR0_EXTERNAL_VALUE && symbol->is_const &&
             text_is(program, symbol->name, (const uint8_t *)"failure", 7u))
      failure_symbol = symbol_index;
  }
  if (arguments_symbol == SIZE_MAX || context_symbol == SIZE_MAX ||
      exit_code_symbol == SIZE_MAX || is_empty_symbol == SIZE_MAX ||
      count_symbol == SIZE_MAX ||
      success_symbol == SIZE_MAX || failure_symbol == SIZE_MAX)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  const w_seed_hir0_external_symbol *arguments_external =
      &program->external_symbols[arguments_symbol];
  const w_seed_hir0_external_symbol *context_external =
      &program->external_symbols[context_symbol];
  const w_seed_hir0_external_symbol *exit_code_external =
      &program->external_symbols[exit_code_symbol];
  const w_seed_hir0_external_symbol *is_empty_external =
      &program->external_symbols[is_empty_symbol];
  const w_seed_hir0_external_symbol *count_external =
      &program->external_symbols[count_symbol];
  const w_seed_hir0_external_symbol *success_external =
      &program->external_symbols[success_symbol];
  const w_seed_hir0_external_symbol *failure_external =
      &program->external_symbols[failure_symbol];
  if (arguments_external->parameter_count != 0u ||
      context_external->parameter_count != 0u ||
      exit_code_external->parameter_count != 0u ||
      is_empty_external->parameter_count != 0u ||
      count_external->parameter_count != 0u ||
      success_external->parameter_count != 0u ||
      failure_external->parameter_count != 1u ||
      failure_external->parameter_abi !=
          W_SEED_HIR0_EXTERNAL_PARAMETER_PROCESS_FAILURE_I64 ||
      is_empty_external->parameter_abi != W_SEED_HIR0_EXTERNAL_PARAMETER_NONE ||
      count_external->parameter_abi != W_SEED_HIR0_EXTERNAL_PARAMETER_NONE ||
      success_external->parameter_abi != W_SEED_HIR0_EXTERNAL_PARAMETER_NONE ||
      !text_is(program, is_empty_external->receiver_type,
               (const uint8_t *)"Arguments", 9u) ||
      !text_is(program, is_empty_external->return_type,
               (const uint8_t *)"Bool", 4u) ||
      !text_is(program, count_external->receiver_type,
               (const uint8_t *)"Arguments", 9u) ||
      !text_is(program, count_external->return_type,
               (const uint8_t *)"usize", 5u) ||
      !text_is(program, success_external->receiver_type,
               (const uint8_t *)"ExitCode", 8u) ||
      !text_is(program, success_external->return_type,
               (const uint8_t *)"ExitCode", 8u) ||
      !text_is(program, failure_external->receiver_type,
               (const uint8_t *)"ExitCode", 8u) ||
      !text_is(program, failure_external->return_type,
               (const uint8_t *)"ExitCode", 8u) ||
      arguments_symbol != external_module->first_symbol ||
      context_symbol != external_module->first_symbol + 1u ||
      exit_code_symbol != external_module->first_symbol + 2u ||
      success_symbol != external_module->first_symbol + 3u ||
      is_empty_symbol != external_module->first_symbol + 4u ||
      failure_symbol != external_module->first_symbol + 5u ||
      count_symbol != external_module->first_symbol + 6u)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  const w_seed_hir0_entry *entry = &program->entries[0];
  if (entry->target_function >= program->function_count ||
      entry->adapter_kind != W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS ||
      (entry->cleanup_obligation !=
           W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS &&
       entry->cleanup_obligation !=
           W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS_REVERSE_ON_ALL_OUTCOMES) ||
      entry->cleanup_owner_parameter_count != 2u)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  w_seed_native_subset0_process candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  candidate.entry = entry;
  candidate.function_index = entry->target_function;
  candidate.function = &program->functions[candidate.function_index];
  candidate.arguments_symbol_index = (uint32_t)arguments_symbol;
  candidate.context_symbol_index = (uint32_t)context_symbol;
  candidate.exit_code_symbol_index = (uint32_t)exit_code_symbol;
  candidate.is_empty_symbol_index = (uint32_t)is_empty_symbol;
  candidate.count_symbol_index = (uint32_t)count_symbol;
  candidate.success_symbol_index = (uint32_t)success_symbol;
  candidate.failure_symbol_index = (uint32_t)failure_symbol;
  const w_seed_hir0_function *function = candidate.function;
  if (function->first_parameter >= program->parameter_count ||
      function->parameter_count != 2u ||
      function->first_parameter >
          program->parameter_count - function->parameter_count ||
      !function->is_async || function->is_const ||
      function->is_unsafe || function->has_borrow_clause ||
      function->is_anonymous_entry ||
      function->suspension != W_SEED_HIR0_SUSPENSION_MAY ||
      (function->is_throws
           ? function->direct_entry != W_SEED_HIR0_DIRECT_ENTRY_ABSENT
           : function->direct_entry != W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE) ||
      !process_nominal_type_is(program, function->return_type, 0u,
                               (uint32_t)exit_code_symbol) ||
      program->types[function->return_type].lifecycle !=
          W_SEED_HIR0_LIFECYCLE_VALUE_COPY ||
      program->types[function->return_type].release_contract !=
          W_SEED_HIR0_RELEASE_CONTRACT_NONE)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  candidate.arguments_parameter =
      &program->parameters[(size_t)function->first_parameter];
  candidate.context_parameter =
      &program->parameters[(size_t)function->first_parameter + 1u];
  candidate.arguments_parameter_ordinal = candidate.arguments_parameter->ordinal;
  candidate.context_parameter_ordinal = candidate.context_parameter->ordinal;
  if (candidate.arguments_parameter_ordinal >= 2u ||
      candidate.context_parameter_ordinal >= 2u ||
      candidate.arguments_parameter_ordinal == candidate.context_parameter_ordinal ||
      entry->first_cleanup_owner_parameter != function->first_parameter ||
      candidate.arguments_parameter->owner_function != candidate.function_index ||
      candidate.context_parameter->owner_function != candidate.function_index ||
      !process_nominal_type_is(program, candidate.arguments_parameter->type_index,
                               0u, (uint32_t)arguments_symbol) ||
      !process_nominal_type_is(program, candidate.context_parameter->type_index,
                               0u, (uint32_t)context_symbol) ||
      program->types[candidate.arguments_parameter->type_index].lifecycle !=
          W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER ||
      program->types[candidate.context_parameter->type_index].lifecycle !=
          W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER ||
      program->types[candidate.arguments_parameter->type_index]
              .release_contract !=
          W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE ||
      program->types[candidate.context_parameter->type_index]
              .release_contract !=
          W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  if (function->is_throws) {
    if (entry->cleanup_obligation !=
            W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS_REVERSE_ON_ALL_OUTCOMES ||
        (!process_integer_exact_root_supported(program, &candidate) &&
         !process_float_to_integer_rounding_root_supported(program,
                                                           &candidate)))
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  } else if (entry->cleanup_obligation !=
             W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS) {
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  }

  /* All non-entry functions remain on the ordinary, nominal-free lowering
   * path.  A local call back into the async process entry is rejected there;
   * the process body itself is checked with its explicit nominal context. */
  const w_seed_hir0_binding *bindings[W_SEED_NATIVE_SUBSET0_MAX_BINDINGS] =
      {NULL};
  size_t binding_reads[W_SEED_NATIVE_SUBSET0_MAX_BINDINGS] = {0u};
  for (size_t binding = 0u; binding < program->binding_count; binding += 1u)
    bindings[binding] = &program->bindings[binding];
  uint8_t state[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS] = {0u};
  size_t cached[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS] = {0u};
  bool has_interpolation = false;
  bool has_local_calls = false;
  if (parallel_selection != NULL &&
      parallel_selection->root_function_index != candidate.function_index)
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  if ((!candidate.has_integer_exactly &&
       !candidate.has_float_to_integer_rounding &&
       !process_function_body_supported(program, &candidate,
                                        parallel_selection)))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  if (candidate.has_integer_exactly ||
      candidate.has_float_to_integer_rounding) {
    if ((candidate.has_integer_exactly && program->call_count != 0u &&
         candidate.maximum_stdout_bytes == 0u) ||
        candidate.maximum_stdout_bytes > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES)
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  } else {
    if (!program_function_maximum(
            program, candidate.function_index, &candidate, bindings,
            binding_reads, state, cached, &has_interpolation, &has_local_calls,
            parallel_selection != NULL))
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
    candidate.maximum_stdout_bytes = cached[candidate.function_index];
  }
  if (!native_program_reachable_panic(program, candidate.function_index,
                                      &candidate.has_reachable_panic))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  for (size_t index = 0u; index < program->function_count; index += 1u)
    candidate.natural_loop_functions[index] =
        program_natural_loop_is_supported(program, index);
  for (size_t index = 0u; index < program->function_count; index += 1u)
    candidate.post_test_loop_functions[index] =
        program_post_test_loop_is_supported(program, index);
  for (size_t index = 0u; index < program->function_count; index += 1u)
    if (index != candidate.function_index &&
        !program_function_maximum(
            program, index, NULL, bindings, binding_reads, state, cached,
            &has_interpolation, &has_local_calls, false))
      return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;

  (void)memcpy(selection, &candidate, sizeof(candidate));
  return W_SEED_NATIVE_SUBSET0_OK;
}

w_seed_native_subset0_status
w_seed_native_subset0_select_process_executable(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_process *selection) {
  return select_process_executable_mode(program, hir_result, NULL, selection);
}

w_seed_native_subset0_status
w_seed_native_subset0_select_process_parallel(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *parallel_selection,
    w_seed_native_subset0_process *selection) {
  if (parallel_selection == NULL)
    return W_SEED_NATIVE_SUBSET0_INVALID;
  return select_process_executable_mode(program, hir_result,
                                        parallel_selection, selection);
}

/* W-1584 M1 is deliberately only a target-neutral admission boundary for a
 * one-block subset of the wider verified HIR0 cooperative envelope. Keep its proof
 * here rather than borrowing COOP0's execution-plan builder: a future emitter
 * must be unable to make a compiler-host oracle record look like a product
 * selection by construction. */
static bool cooperative_selection_range(size_t first, size_t count,
                                        size_t total) {
  return first <= total && count <= total - first;
}

static bool cooperative_selection_scalar_type(
    const w_seed_hir0_program *program, uint32_t type_index) {
  if (program == NULL || type_index >= program->type_count) return false;
  return program->types[type_index].kind == W_SEED_HIR0_TYPE_I64 ||
         program->types[type_index].kind == W_SEED_HIR0_TYPE_BOOL;
}

static bool cooperative_selection_function_span(
    const w_seed_hir0_program *program, uint32_t function_index,
    uint32_t *first, uint32_t *count) {
  if (program == NULL || first == NULL || count == NULL ||
      function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->block_count != 1u || function->first_block >= program->block_count)
    return false;
  const w_seed_hir0_block *block = &program->blocks[function->first_block];
  if (block->owner_function != function_index ||
      !cooperative_selection_range(block->first_instruction,
                                   block->instruction_count,
                                   program->instruction_count) ||
      block->instruction_count > UINT32_MAX - block->first_instruction)
    return false;
  *first = block->first_instruction;
  *count = block->instruction_count;
  return true;
}

static bool cooperative_selection_sync_function(
    const w_seed_hir0_program *program, uint32_t function_index,
    uint32_t module_index,
    uint8_t state[W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS],
    bool reachable[W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS], size_t depth);

static bool cooperative_selection_sync_function(
    const w_seed_hir0_program *program, uint32_t function_index,
    uint32_t module_index,
    uint8_t state[W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS],
    bool reachable[W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS], size_t depth) {
  if (program == NULL || state == NULL || reachable == NULL ||
      function_index >= program->function_count ||
      function_index >= W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS ||
      depth > W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS)
    return false;
  reachable[function_index] = true;
  if (state[function_index] == 2u) return true;
  if (state[function_index] == 1u || state[function_index] == 3u)
    return false;
  state[function_index] = 1u;
  const w_seed_hir0_function *function = &program->functions[function_index];
  bool valid = function->module_index == module_index && !function->is_async &&
               !function->is_const && !function->is_throws &&
               !function->is_unsafe && !function->has_borrow_clause &&
               !function->is_anonymous_entry &&
               cooperative_selection_scalar_type(program, function->return_type) &&
               function->parameter_count <= 16u &&
               cooperative_selection_range(function->first_parameter,
                                           function->parameter_count,
                                           program->parameter_count);
  for (size_t ordinal = 0u; valid && ordinal < function->parameter_count;
       ordinal += 1u) {
    const w_seed_hir0_parameter *parameter =
        &program->parameters[(size_t)function->first_parameter + ordinal];
    valid = parameter->owner_function == function_index &&
            parameter->ordinal == ordinal &&
            cooperative_selection_scalar_type(program, parameter->type_index);
  }
  uint32_t first = 0u;
  uint32_t instruction_count = 0u;
  if (!cooperative_selection_function_span(program, function_index, &first,
                                           &instruction_count))
    valid = false;
  for (size_t ordinal = 0u; valid && ordinal < instruction_count;
       ordinal += 1u) {
    const uint32_t instruction_index = first + (uint32_t)ordinal;
    const w_seed_hir0_instruction *instruction =
        &program->instructions[instruction_index];
    if (instruction->owner_block != function->first_block ||
        instruction->ordinal != ordinal)
      return false;
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= program->binding_count)
        valid = false;
      else {
        const w_seed_hir0_binding *binding =
            &program->bindings[instruction->binding_index];
        valid = binding->owner_instruction == instruction_index &&
                binding->owner_block == function->first_block &&
                binding->task_role == W_SEED_HIR0_TASK_ROLE_NONE &&
                cooperative_selection_scalar_type(program, binding->type_index);
      }
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (instruction->call_index >= program->call_count) {
        valid = false;
      } else {
        const w_seed_hir0_call *call = &program->calls[instruction->call_index];
        if (call->owner_instruction != instruction_index ||
            call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
            call->callee_identity >= program->identity_count ||
            !cooperative_selection_scalar_type(program, call->result_type)) {
          valid = false;
        } else {
          const w_seed_hir0_identity *identity =
              &program->identities[call->callee_identity];
          valid = identity->kind == W_SEED_HIR0_IDENTITY_FUNCTION &&
                  identity->target_index < program->function_count &&
                  cooperative_selection_sync_function(
                      program, identity->target_index, module_index, state,
                      reachable, depth + 1u);
        }
      }
    } else {
      valid = false;
    }
  }
  const w_seed_hir0_block *block =
      function->first_block < program->block_count
          ? &program->blocks[function->first_block]
          : NULL;
  if (block == NULL || block->terminator_index >= program->terminator_count) {
    valid = false;
  } else {
    const w_seed_hir0_terminator *terminator =
        &program->terminators[block->terminator_index];
    valid = valid &&
            terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
            terminator->value_index < program->value_count &&
            cooperative_selection_scalar_type(program, terminator->result_type) &&
            terminator->result_type == function->return_type;
  }
  state[function_index] = valid ? 2u : 3u;
  return valid;
}

static bool cooperative_selection_async_function(
    const w_seed_hir0_program *program, uint32_t function_index,
    uint32_t module_index,
    uint8_t state[W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS],
    bool reachable[W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS],
    uint32_t *yield_count) {
  if (program == NULL || state == NULL || reachable == NULL ||
      yield_count == NULL || function_index >= program->function_count ||
      function_index >= W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS)
    return false;
  reachable[function_index] = true;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (!function->is_async || function->is_const || function->is_throws ||
      function->is_unsafe || function->has_borrow_clause ||
      function->is_anonymous_entry || function->module_index != module_index ||
      !cooperative_selection_scalar_type(program, function->return_type) ||
      function->parameter_count > 16u ||
      !cooperative_selection_range(function->first_parameter,
                                   function->parameter_count,
                                   program->parameter_count))
    return false;
  for (size_t ordinal = 0u; ordinal < function->parameter_count; ordinal += 1u) {
    const w_seed_hir0_parameter *parameter =
        &program->parameters[(size_t)function->first_parameter + ordinal];
    if (parameter->owner_function != function_index ||
        parameter->ordinal != ordinal ||
        !cooperative_selection_scalar_type(program, parameter->type_index))
      return false;
  }
  uint32_t first = 0u;
  uint32_t instruction_count = 0u;
  if (!cooperative_selection_function_span(program, function_index, &first,
                                           &instruction_count) ||
      instruction_count == 0u)
    return false;
  uint32_t yields = 0u;
  for (size_t ordinal = 0u; ordinal < instruction_count; ordinal += 1u) {
    const uint32_t instruction_index = first + (uint32_t)ordinal;
    const w_seed_hir0_instruction *instruction =
        &program->instructions[instruction_index];
    if (instruction->owner_block != function->first_block ||
        instruction->ordinal != ordinal)
      return false;
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) {
      yields += 1u;
      if (yields > W_SEED_HIR0_COOPERATIVE_MAX_YIELDS_PER_TASK) return false;
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= program->binding_count) return false;
      const w_seed_hir0_binding *binding =
          &program->bindings[instruction->binding_index];
      if (binding->owner_instruction != instruction_index ||
          binding->owner_block != function->first_block ||
          binding->task_role != W_SEED_HIR0_TASK_ROLE_NONE ||
          !cooperative_selection_scalar_type(program, binding->type_index))
        return false;
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (instruction->call_index >= program->call_count) return false;
      const w_seed_hir0_call *call = &program->calls[instruction->call_index];
      if (call->owner_instruction != instruction_index ||
          call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
          call->callee_identity >= program->identity_count ||
          !cooperative_selection_scalar_type(program, call->result_type))
        return false;
      const w_seed_hir0_identity *identity =
          &program->identities[call->callee_identity];
      if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
          identity->target_index >= program->function_count ||
          !cooperative_selection_sync_function(
              program, identity->target_index, module_index, state, reachable,
              0u))
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
      !cooperative_selection_scalar_type(program, terminator->result_type) ||
      terminator->result_type != function->return_type)
    return false;
  *yield_count = yields;
  return true;
}

static bool cooperative_selection_host_print(
    const w_seed_hir0_program *program, const w_seed_hir0_call *call) {
  if (program == NULL || call == NULL || call->callee_identity >= program->identity_count)
    return false;
  const w_seed_hir0_identity *identity =
      &program->identities[call->callee_identity];
  return identity->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE &&
         text_is(program, identity->name, (const uint8_t *)"print",
                 sizeof("print") - 1u) &&
         call->argument_count == 1u && call->result_type < program->type_count &&
         program->types[call->result_type].kind == W_SEED_HIR0_TYPE_UNIT;
}

static bool cooperative_selection_derive(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    w_seed_cooperative_selection0 *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      program->module_count != 1u || program->entry_count != 1u ||
      program->function_count == 0u ||
      program->function_count > W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS ||
      program->function_count > UINT32_MAX || program->cleanup_count != 0u ||
      program->instruction_count > UINT32_MAX ||
      program->binding_count > UINT32_MAX || program->call_count > UINT32_MAX)
    return false;
  const w_seed_hir0_entry *entry = &program->entries[0];
  if (!entry->is_body || entry->module_index != 0u ||
      entry->adapter_kind != W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT ||
      entry->target_function >= program->function_count)
    return false;
  const uint32_t root_index = entry->target_function;
  const w_seed_hir0_function *root = &program->functions[root_index];
  if (root->module_index != 0u || !root->is_anonymous_entry || root->is_async ||
      root->is_const || root->is_throws || root->is_unsafe ||
      root->has_borrow_clause || root->parameter_count != 0u ||
      root->return_type >= program->type_count ||
      program->types[root->return_type].kind != W_SEED_HIR0_TYPE_UNIT ||
      root->block_count != 1u || root->first_block >= program->block_count)
    return false;
  const w_seed_hir0_block *root_block = &program->blocks[root->first_block];
  uint32_t root_first = 0u;
  uint32_t root_instruction_count = 0u;
  if (root_block->owner_function != root_index ||
      root_block->terminator_index >= program->terminator_count ||
      !cooperative_selection_function_span(program, root_index, &root_first,
                                           &root_instruction_count) ||
      program->terminators[root_block->terminator_index].kind !=
          W_SEED_HIR0_TERMINATOR_RETURN_UNIT)
    return false;

  uint32_t physical_calls[W_SEED_HIR0_COOPERATIVE_MAX_TASKS] = {
      W_SEED_HIR0_NONE};
  uint32_t task_functions[W_SEED_HIR0_COOPERATIVE_MAX_TASKS] = {
      W_SEED_HIR0_NONE};
  uint32_t task_yields[W_SEED_HIR0_COOPERATIVE_MAX_TASKS] = {0u};
  uint8_t state[W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS] = {0u};
  bool reachable[W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS] = {false};
  reachable[root_index] = true;
  size_t physical_count = 0u;
  w_seed_hir0_call_execution_kind physical_kind = W_SEED_HIR0_CALL_DIRECT;
  for (size_t call_index = 0u; call_index < program->call_count; call_index += 1u) {
    const w_seed_hir0_call *call = &program->calls[call_index];
    if (call->execution_kind == W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED ||
        call->execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED)
      return false;
    if (call->execution_kind !=
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_COOPERATIVE_TRACE &&
        call->execution_kind !=
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH)
      continue;
    if (physical_count >= W_SEED_HIR0_COOPERATIVE_MAX_TASKS ||
        call->owner_block != root->first_block ||
        call->owner_instruction < root_first ||
        call->owner_instruction >= root_first + root_instruction_count ||
        call->callee_identity >= program->identity_count)
      return false;
    if (physical_count == 0u)
      physical_kind = call->execution_kind;
    else if (call->execution_kind != physical_kind)
      return false;
    const w_seed_hir0_identity *identity =
        &program->identities[call->callee_identity];
    if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
        identity->target_index >= program->function_count ||
        identity->target_index == root_index ||
        !cooperative_selection_async_function(
            program, identity->target_index, root->module_index, state,
            reachable, &task_yields[physical_count]))
      return false;
    physical_calls[physical_count] = (uint32_t)call_index;
    task_functions[physical_count] = identity->target_index;
    physical_count += 1u;
  }
  if (physical_count == 0u) return false;
  const size_t required_tasks =
      physical_kind == W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH
          ? physical_count
          : W_SEED_HIR0_COOPERATIVE_ORACLE_MAX_TASKS;
  if (physical_count != required_tasks) return false;

  uint32_t launch_bindings[W_SEED_HIR0_COOPERATIVE_MAX_TASKS] = {
      W_SEED_HIR0_NONE};
  uint32_t join_bindings[W_SEED_HIR0_COOPERATIVE_MAX_TASKS] = {
      W_SEED_HIR0_NONE};
  size_t physical_seen = 0u;
  size_t launch_count = 0u;
  size_t join_count = 0u;
  for (size_t ordinal = 0u; ordinal < root_instruction_count; ordinal += 1u) {
    const uint32_t instruction_index = root_first + (uint32_t)ordinal;
    const w_seed_hir0_instruction *instruction =
        &program->instructions[instruction_index];
    if (instruction->owner_block != root->first_block ||
        instruction->ordinal != ordinal)
      return false;
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= program->binding_count) return false;
      const w_seed_hir0_binding *binding =
          &program->bindings[instruction->binding_index];
      if (binding->owner_instruction != instruction_index ||
          binding->owner_block != root->first_block)
        return false;
      if (binding->task_role == W_SEED_HIR0_TASK_ROLE_LAUNCH) {
        if (join_count != 0u || launch_count >= required_tasks) return false;
        launch_bindings[launch_count] = instruction->binding_index;
        launch_count += 1u;
      } else if (binding->task_role == W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT) {
        if (launch_count != required_tasks || join_count >= required_tasks)
          return false;
        join_bindings[join_count] = instruction->binding_index;
        join_count += 1u;
      } else {
        return false;
      }
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (instruction->call_index >= program->call_count) return false;
      const w_seed_hir0_call *call = &program->calls[instruction->call_index];
      if (call->owner_instruction != instruction_index) return false;
      if (call->execution_kind ==
              W_SEED_HIR0_CALL_STRUCTURED_ASYNC_COOPERATIVE_TRACE ||
          call->execution_kind ==
              W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH) {
        if (physical_seen >= required_tasks ||
            instruction->call_index != physical_calls[physical_seen] ||
            ordinal + 1u >= root_instruction_count)
          return false;
        const w_seed_hir0_instruction *binding_instruction =
            &program->instructions[instruction_index + 1u];
        if (binding_instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
            binding_instruction->binding_index >= program->binding_count ||
            program->bindings[binding_instruction->binding_index].task_role !=
                W_SEED_HIR0_TASK_ROLE_LAUNCH)
          return false;
        physical_seen += 1u;
      } else if (!cooperative_selection_host_print(program, call) ||
                 launch_count != required_tasks ||
                 join_count != required_tasks) {
        return false;
      }
    } else {
      return false;
    }
  }
  if (physical_seen != required_tasks || launch_count != required_tasks ||
      join_count != required_tasks)
    return false;
  for (size_t task = 0u; task < required_tasks; task += 1u) {
    const w_seed_hir0_binding *launch = &program->bindings[launch_bindings[task]];
    const w_seed_hir0_binding *join = &program->bindings[join_bindings[task]];
    const w_seed_hir0_call *call = &program->calls[physical_calls[task]];
    if (launch->task_peer_binding != join_bindings[task] ||
        join->task_peer_binding != launch_bindings[task] ||
        launch->owner_instruction <= call->owner_instruction ||
        join->owner_instruction <= launch->owner_instruction)
      return false;
  }
  for (size_t function = 0u; function < program->function_count; function += 1u)
    if (!reachable[function]) return false;

  (void)memset(selection, 0, sizeof(*selection));
  (void)memcpy(selection->schema,
               W_SEED_COOPERATIVE_SELECTION0_SCHEMA_VERSION,
               sizeof(selection->schema));
  selection->root_function_index = root_index;
  selection->task_count = (uint32_t)required_tasks;
  selection->function_count = (uint32_t)program->function_count;
  selection->instruction_count = (uint32_t)program->instruction_count;
  selection->binding_count = (uint32_t)program->binding_count;
  selection->call_count = (uint32_t)program->call_count;
  selection->yield_count = 0u;
  for (size_t task = 0u; task < required_tasks; task += 1u) {
    selection->task_call_indices[task] = physical_calls[task];
    selection->task_function_indices[task] = task_functions[task];
    selection->launch_binding_indices[task] = launch_bindings[task];
    selection->join_binding_indices[task] = join_bindings[task];
    selection->task_yield_counts[task] = task_yields[task];
    selection->yield_count += task_yields[task];
  }
  selection->execution_profile =
      physical_kind == W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH
          ? W_SEED_HIR0_EXECUTION_PROFILE_MAIN_SERIAL
          : W_SEED_HIR0_EXECUTION_PROFILE_COOPERATIVE_TRACE;
  (void)memcpy(selection->hir_semantic_digest, hir_result->semantic_digest,
               sizeof(selection->hir_semantic_digest));
  return true;
}

static bool cooperative_selection_equal(
    const w_seed_cooperative_selection0 *left,
    const w_seed_cooperative_selection0 *right) {
  if (left == NULL || right == NULL ||
      memcmp(left->schema, right->schema, sizeof(left->schema)) != 0 ||
      left->root_function_index != right->root_function_index ||
      left->task_count != right->task_count ||
      left->function_count != right->function_count ||
      left->instruction_count != right->instruction_count ||
      left->binding_count != right->binding_count ||
      left->call_count != right->call_count ||
      left->yield_count != right->yield_count ||
      left->execution_profile != right->execution_profile ||
      memcmp(left->reserved, right->reserved, sizeof(left->reserved)) != 0 ||
      memcmp(left->hir_semantic_digest, right->hir_semantic_digest,
             sizeof(left->hir_semantic_digest)) != 0)
    return false;
  return memcmp(left->task_call_indices, right->task_call_indices,
                sizeof(left->task_call_indices)) == 0 &&
         memcmp(left->task_function_indices, right->task_function_indices,
                sizeof(left->task_function_indices)) == 0 &&
         memcmp(left->launch_binding_indices, right->launch_binding_indices,
                sizeof(left->launch_binding_indices)) == 0 &&
         memcmp(left->join_binding_indices, right->join_binding_indices,
                sizeof(left->join_binding_indices)) == 0 &&
         memcmp(left->task_yield_counts, right->task_yield_counts,
                sizeof(left->task_yield_counts)) == 0;
}

w_seed_native_subset0_status w_seed_native_subset0_select_cooperative(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    w_seed_cooperative_selection0 *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return W_SEED_NATIVE_SUBSET0_INVALID;
  w_seed_cooperative_selection0 candidate;
  if (!cooperative_selection_derive(program, hir_result, &candidate))
    return W_SEED_NATIVE_SUBSET0_UNSUPPORTED;
  *selection = candidate;
  return W_SEED_NATIVE_SUBSET0_OK;
}

bool w_seed_native_subset0_verify_cooperative(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative_selection0 *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return false;
  w_seed_cooperative_selection0 expected;
  return cooperative_selection_derive(program, hir_result, &expected) &&
         cooperative_selection_equal(selection, &expected);
}
