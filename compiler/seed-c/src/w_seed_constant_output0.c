#include "w_seed_constant_output0.h"

#include <limits.h>
#include <string.h>

#include "w_seed_scalar_evaluator0.h"

enum {
  CONSTANT_OUTPUT0_MAX_VALUES = 1024,
  CONSTANT_OUTPUT0_MAX_ELEMENTS = 1024,
  CONSTANT_OUTPUT0_MAX_FUNCTIONS = 16,
  CONSTANT_OUTPUT0_MAX_SOURCE_VALUES = 512,
  CONSTANT_OUTPUT0_MAX_PARAMETERS = 32,
  CONSTANT_OUTPUT0_MAX_BINDINGS = 128,
  CONSTANT_OUTPUT0_MAX_CALLS = 32,
  CONSTANT_OUTPUT0_MAX_BLOCKS = 400,
  CONSTANT_OUTPUT0_MAX_STEPS = 16384,
};

typedef enum {
  CONSTANT_OUTPUT0_VALUE_UNIT = 0,
  CONSTANT_OUTPUT0_VALUE_I64,
  CONSTANT_OUTPUT0_VALUE_BOOL,
  CONSTANT_OUTPUT0_VALUE_STRING,
  CONSTANT_OUTPUT0_VALUE_PRODUCT,
} constant_output0_value_kind;

typedef struct {
  constant_output0_value_kind kind;
  uint32_t type_index;
  union {
    int64_t integer;
    bool boolean;
    struct {
      const uint8_t *bytes;
      size_t length;
    } string;
    struct {
      uint32_t first_element;
      uint32_t element_count;
    } product;
  } as;
} constant_output0_value;

typedef struct {
  uint32_t binding_values[CONSTANT_OUTPUT0_MAX_BINDINGS];
  bool binding_initialized[CONSTANT_OUTPUT0_MAX_BINDINGS];
  uint32_t parameter_values[CONSTANT_OUTPUT0_MAX_PARAMETERS];
  bool parameter_initialized[CONSTANT_OUTPUT0_MAX_PARAMETERS];
  uint32_t call_values[CONSTANT_OUTPUT0_MAX_CALLS];
  bool call_initialized[CONSTANT_OUTPUT0_MAX_CALLS];
  bool visited_blocks[CONSTANT_OUTPUT0_MAX_BLOCKS];
  uint32_t function_index;
} constant_output0_frame;

typedef struct {
  const w_seed_hir0_program *program;
  constant_output0_value values[CONSTANT_OUTPUT0_MAX_VALUES];
  uint32_t elements[CONSTANT_OUTPUT0_MAX_ELEMENTS];
  size_t value_count;
  size_t element_count;
  uint8_t string_bytes[W_SEED_CONSTANT_OUTPUT0_MAX_BYTES];
  size_t string_byte_count;
  uint8_t stdout_bytes[W_SEED_CONSTANT_OUTPUT0_MAX_BYTES];
  size_t stdout_length;
  size_t steps;
  bool evaluated_flat_product;
  bool active_functions[CONSTANT_OUTPUT0_MAX_FUNCTIONS];
  constant_output0_frame frames[CONSTANT_OUTPUT0_MAX_FUNCTIONS];
} constant_output0_context;

static bool hir_text_is(const w_seed_hir0_program *program,
                        w_seed_hir0_text text, const char *literal) {
  if (program == NULL || literal == NULL ||
      text.offset > program->text_byte_count ||
      text.count > program->text_byte_count - text.offset ||
      (text.count != 0u && program->text_bytes == NULL))
    return false;
  const size_t length = strlen(literal);
  return text.count == length &&
         (length == 0u ||
          memcmp(program->text_bytes + text.offset, literal, length) == 0);
}

static bool range_valid(uint32_t first, uint32_t count, size_t total) {
  return first != W_SEED_HIR0_NONE && first <= total &&
         (size_t)count <= total - first;
}

static bool type_is(const w_seed_hir0_program *program, uint32_t type_index,
                    w_seed_hir0_type_kind kind) {
  return program != NULL && type_index < program->type_count &&
         program->types != NULL && program->types[type_index].kind == kind;
}

static bool fixed_i64_type(const w_seed_hir0_program *program,
                           uint32_t type_index) {
  return type_is(program, type_index, W_SEED_HIR0_TYPE_I64);
}

static bool append_bytes(uint8_t *bytes, size_t capacity, size_t *length,
                         const uint8_t *source, size_t source_length) {
  if (bytes == NULL || length == NULL ||
      (source_length != 0u && source == NULL) || *length > capacity ||
      source_length > capacity - *length)
    return false;
  if (source_length != 0u)
    (void)memcpy(bytes + *length, source, source_length);
  *length += source_length;
  return true;
}

static bool append_i64(uint8_t *bytes, size_t capacity, size_t *length,
                       int64_t value) {
  uint8_t digits[20];
  size_t digit_count = 0u;
  const bool negative = value < 0;
  uint64_t magnitude =
      negative ? (uint64_t)(-(value + 1)) + 1u : (uint64_t)value;
  do {
    digits[digit_count] = (uint8_t)('0' + (uint8_t)(magnitude % 10u));
    magnitude /= 10u;
    digit_count += 1u;
  } while (magnitude != 0u);
  if (negative &&
      !append_bytes(bytes, capacity, length, (const uint8_t *)"-", 1u))
    return false;
  while (digit_count != 0u) {
    digit_count -= 1u;
    if (!append_bytes(bytes, capacity, length, &digits[digit_count], 1u))
      return false;
  }
  return true;
}

static bool new_value(constant_output0_context *context,
                      constant_output0_value value, uint32_t *index) {
  if (context == NULL || index == NULL ||
      context->value_count >= CONSTANT_OUTPUT0_MAX_VALUES ||
      context->value_count > UINT32_MAX)
    return false;
  *index = (uint32_t)context->value_count;
  context->values[context->value_count] = value;
  context->value_count += 1u;
  return true;
}

static bool append_i64_display(constant_output0_context *context,
                               int64_t integer) {
  return context != NULL &&
         append_i64(context->stdout_bytes, sizeof(context->stdout_bytes),
                    &context->stdout_length, integer);
}

static bool append_i64_to_string(constant_output0_context *context,
                                 int64_t integer) {
  return context != NULL &&
         append_i64(context->string_bytes, sizeof(context->string_bytes),
                    &context->string_byte_count, integer);
}

static bool append_value_display(constant_output0_context *context,
                                 uint32_t value_index, size_t depth,
                                 bool to_interpolation);

static bool append_string_value(constant_output0_context *context,
                                const constant_output0_value *value,
                                bool to_interpolation) {
  if (context == NULL || value == NULL ||
      value->kind != CONSTANT_OUTPUT0_VALUE_STRING)
    return false;
  if (to_interpolation)
    return append_bytes(context->string_bytes, sizeof(context->string_bytes),
                        &context->string_byte_count, value->as.string.bytes,
                        value->as.string.length);
  return append_bytes(context->stdout_bytes, sizeof(context->stdout_bytes),
                      &context->stdout_length, value->as.string.bytes,
                      value->as.string.length);
}

static bool append_value_display(constant_output0_context *context,
                                 uint32_t value_index, size_t depth,
                                 bool to_interpolation) {
  if (context == NULL || depth > W_SEED_HIR0_MAX_NESTING ||
      value_index >= context->value_count)
    return false;
  const constant_output0_value value = context->values[value_index];
  switch (value.kind) {
  case CONSTANT_OUTPUT0_VALUE_I64:
    return to_interpolation ? append_i64_to_string(context, value.as.integer)
                            : append_i64_display(context, value.as.integer);
  case CONSTANT_OUTPUT0_VALUE_BOOL: {
    const uint8_t *literal =
        value.as.boolean ? (const uint8_t *)"true" : (const uint8_t *)"false";
    const size_t length = value.as.boolean ? 4u : 5u;
    return to_interpolation
               ? append_bytes(context->string_bytes,
                              sizeof(context->string_bytes),
                              &context->string_byte_count, literal, length)
               : append_bytes(context->stdout_bytes,
                              sizeof(context->stdout_bytes),
                              &context->stdout_length, literal, length);
  }
  case CONSTANT_OUTPUT0_VALUE_STRING:
    return append_string_value(context, &value, to_interpolation);
  case CONSTANT_OUTPUT0_VALUE_UNIT:
  case CONSTANT_OUTPUT0_VALUE_PRODUCT:
    return false;
  }
  return false;
}

static bool eval_value(constant_output0_context *context, uint32_t value_index,
                       constant_output0_frame *frame, size_t depth,
                       uint32_t *result);

static bool eval_function(constant_output0_context *context,
                          uint32_t function_index, const uint32_t *parameters,
                          size_t parameter_count, size_t call_depth,
                          uint32_t *result);

static bool function_is_effect_safe(const w_seed_hir0_program *program,
                                    uint32_t function_index) {
  if (program == NULL || function_index >= program->function_count ||
      program->functions == NULL)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  return !function->is_async && !function->is_throws && !function->is_unsafe &&
         !function->has_borrow_clause &&
         function->error_type == W_SEED_HIR0_NONE;
}

static bool host_print_identity(const w_seed_hir0_program *program,
                                const w_seed_hir0_call *call) {
  if (program == NULL || call == NULL ||
      call->callee_identity >= program->identity_count ||
      program->identities == NULL || call->result_type >= program->type_count ||
      program->types == NULL ||
      program->types[call->result_type].kind != W_SEED_HIR0_TYPE_UNIT)
    return false;
  const w_seed_hir0_identity *identity =
      &program->identities[call->callee_identity];
  if (identity->kind != W_SEED_HIR0_IDENTITY_HOST_PRELUDE ||
      !hir_text_is(program, identity->name, "print") ||
      !hir_text_is(program, identity->profile, "native-process@1") ||
      identity->return_type >= program->type_count ||
      program->types[identity->return_type].kind != W_SEED_HIR0_TYPE_UNIT ||
      identity->is_const ||
      identity->parameter_count != 1u ||
      identity->first_parameter == W_SEED_HIR0_NONE ||
      identity->first_parameter >= program->host_parameter_count ||
      identity->first_requirement == W_SEED_HIR0_NONE ||
      identity->requirement_count != 1u ||
      !range_valid(identity->first_requirement, identity->requirement_count,
                   program->requirement_count) ||
      program->host_parameters == NULL || program->requirements == NULL)
    return false;
  const w_seed_hir0_host_parameter *parameter =
      &program->host_parameters[identity->first_parameter];
  const w_seed_hir0_requirement *requirement =
      &program->requirements[identity->first_requirement];
  return parameter->owner_identity == call->callee_identity &&
         parameter->ordinal == 0u &&
         type_is(program, parameter->type_index, W_SEED_HIR0_TYPE_STRING) &&
         parameter->label_kind == W_SEED_HIR0_LABEL_POSITIONAL_ONLY &&
         parameter->label.count == 0u &&
         hir_text_is(program, parameter->name, "message") &&
         requirement->owner_kind == W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY &&
         requirement->owner_index == call->callee_identity &&
         requirement->ordinal == 0u &&
         hir_text_is(program, requirement->name, "Console");
}

static bool eval_local_call(constant_output0_context *context,
                            uint32_t call_index, constant_output0_frame *caller,
                            size_t depth, uint32_t *result) {
  const w_seed_hir0_program *program = context->program;
  if (call_index >= program->call_count || program->calls == NULL ||
      result == NULL || depth >= CONSTANT_OUTPUT0_MAX_FUNCTIONS)
    return false;
  const w_seed_hir0_call *call = &program->calls[call_index];
  if (call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
      call->callee_identity >= program->identity_count ||
      program->identities == NULL ||
      !range_valid(call->first_argument, call->argument_count,
                   program->argument_count) ||
      call->argument_count > CONSTANT_OUTPUT0_MAX_PARAMETERS ||
      call->requirement_count != 0u)
    return false;
  const w_seed_hir0_identity *identity =
      &program->identities[call->callee_identity];
  if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      identity->target_index >= program->function_count ||
      identity->first_requirement != W_SEED_HIR0_NONE ||
      identity->requirement_count != 0u)
    return false;
  const uint32_t callee = identity->target_index;
  const w_seed_hir0_function *function = &program->functions[callee];
  if (!function_is_effect_safe(program, callee) ||
      function->is_anonymous_entry ||
      function->parameter_count != call->argument_count ||
      function->return_type != call->result_type ||
      function->identity_index != call->callee_identity ||
      function->first_parameter > program->parameter_count ||
      function->parameter_count >
          program->parameter_count - function->first_parameter ||
      program->parameters == NULL || context->active_functions[callee])
    return false;

  uint32_t values[CONSTANT_OUTPUT0_MAX_PARAMETERS];
  bool initialized[CONSTANT_OUTPUT0_MAX_PARAMETERS] = {false};
  for (size_t ordinal = 0u; ordinal < call->argument_count; ordinal += 1u) {
    const w_seed_hir0_argument *argument =
        &program->arguments[(size_t)call->first_argument + ordinal];
    if (argument->owner_call != call_index || argument->ordinal != ordinal ||
        argument->parameter_ordinal >= call->argument_count ||
        argument->value_index >= program->value_count ||
        argument->type_index >= program->type_count ||
        program->values[argument->value_index].type_index !=
            argument->type_index)
      return false;
    const size_t parameter_index =
        (size_t)function->first_parameter + argument->parameter_ordinal;
    const w_seed_hir0_parameter *parameter =
        &program->parameters[parameter_index];
    if (parameter->owner_function != callee ||
        parameter->ordinal != argument->parameter_ordinal ||
        parameter->type_index != argument->type_index ||
        initialized[argument->parameter_ordinal] ||
        !eval_value(context, argument->value_index, caller, depth + 1u,
                    &values[argument->parameter_ordinal]))
      return false;
    initialized[argument->parameter_ordinal] = true;
  }
  for (size_t ordinal = 0u; ordinal < call->argument_count; ordinal += 1u)
    if (!initialized[ordinal])
      return false;
  return eval_function(context, callee, values, call->argument_count,
                       depth + 1u, result);
}

static bool eval_interpolation(constant_output0_context *context,
                               uint32_t value_index,
                               constant_output0_frame *frame, size_t depth,
                               uint32_t *result) {
  const w_seed_hir0_program *program = context->program;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (!type_is(program, value->type_index, W_SEED_HIR0_TYPE_STRING) ||
      !range_valid(value->first_interpolation_segment,
                   value->interpolation_segment_count,
                   program->interpolation_segment_count) ||
      (value->interpolation_segment_count != 0u &&
       program->interpolation_segments == NULL))
    return false;
  const size_t first = context->string_byte_count;
  for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
       ordinal += 1u) {
    const w_seed_hir0_interpolation_segment *segment =
        &program->interpolation_segments
             [(size_t)value->first_interpolation_segment + ordinal];
    if (segment->owner_value != value_index || segment->ordinal != ordinal)
      return false;
    if (segment->kind == W_SEED_HIR0_INTERPOLATION_TEXT) {
      if (segment->byte_offset > program->value_byte_count ||
          segment->byte_count >
              program->value_byte_count - segment->byte_offset ||
          (segment->byte_count != 0u && program->value_bytes == NULL))
        return false;
      const uint8_t *text_bytes =
          segment->byte_count == 0u
              ? NULL
              : program->value_bytes + segment->byte_offset;
      if (!append_bytes(context->string_bytes, sizeof(context->string_bytes),
                        &context->string_byte_count, text_bytes,
                        segment->byte_count))
        return false;
    } else if (segment->kind == W_SEED_HIR0_INTERPOLATION_VALUE) {
      uint32_t child_index = W_SEED_HIR0_NONE;
      if (segment->value_index >= program->value_count ||
          !eval_value(context, segment->value_index, frame, depth + 1u,
                      &child_index) ||
          !append_value_display(context, child_index, depth + 1u, true))
        return false;
    } else {
      return false;
    }
  }
  const size_t byte_count = context->string_byte_count - first;
  if (first > UINT32_MAX || byte_count > UINT32_MAX)
    return false;
  const constant_output0_value candidate = {
      .kind = CONSTANT_OUTPUT0_VALUE_STRING,
      .type_index = value->type_index,
      .as.string = {context->string_bytes + first, byte_count}};
  return new_value(context, candidate, result);
}

static bool eval_value(constant_output0_context *context, uint32_t value_index,
                       constant_output0_frame *frame, size_t depth,
                       uint32_t *result) {
  if (context == NULL || context->program == NULL || frame == NULL ||
      result == NULL || value_index >= context->program->value_count ||
      depth > W_SEED_HIR0_MAX_NESTING ||
      ++context->steps > CONSTANT_OUTPUT0_MAX_STEPS)
    return false;
  const w_seed_hir0_program *program = context->program;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index >= program->type_count)
    return false;
  switch (value->kind) {
  case W_SEED_HIR0_VALUE_CONST_I64:
    if (!fixed_i64_type(program, value->type_index))
      return false;
    return new_value(
        context,
        (constant_output0_value){.kind = CONSTANT_OUTPUT0_VALUE_I64,
                                 .type_index = value->type_index,
                                 .as.integer = value->integer_value},
        result);
  case W_SEED_HIR0_VALUE_CONST_BOOL:
    if (!type_is(program, value->type_index, W_SEED_HIR0_TYPE_BOOL))
      return false;
    return new_value(
        context,
        (constant_output0_value){.kind = CONSTANT_OUTPUT0_VALUE_BOOL,
                                 .type_index = value->type_index,
                                 .as.boolean = value->bool_value},
        result);
  case W_SEED_HIR0_VALUE_CONST_STRING:
    if (!type_is(program, value->type_index, W_SEED_HIR0_TYPE_STRING) ||
        value->byte_offset > program->value_byte_count ||
        value->byte_count > program->value_byte_count - value->byte_offset ||
        (value->byte_count != 0u && program->value_bytes == NULL))
      return false;
    return new_value(
        context,
        (constant_output0_value){
            .kind = CONSTANT_OUTPUT0_VALUE_STRING,
            .type_index = value->type_index,
            .as.string = {value->byte_count == 0u
                              ? NULL
                              : program->value_bytes + value->byte_offset,
                          value->byte_count}},
        result);
  case W_SEED_HIR0_VALUE_BINDING_READ:
    if (value->binding_index >= CONSTANT_OUTPUT0_MAX_BINDINGS ||
        value->binding_index >= program->binding_count ||
        !frame->binding_initialized[value->binding_index])
      return false;
    if (context->values[frame->binding_values[value->binding_index]]
            .type_index != value->type_index)
      return false;
    *result = frame->binding_values[value->binding_index];
    return true;
  case W_SEED_HIR0_VALUE_PARAMETER_READ: {
    const w_seed_hir0_function *function =
        &program->functions[frame->function_index];
    if (value->parameter_index < function->first_parameter ||
        value->parameter_index - function->first_parameter >=
            function->parameter_count)
      return false;
    const uint32_t ordinal = value->parameter_index - function->first_parameter;
    if (ordinal >= CONSTANT_OUTPUT0_MAX_PARAMETERS ||
        !frame->parameter_initialized[ordinal] ||
        context->values[frame->parameter_values[ordinal]].type_index !=
            value->type_index)
      return false;
    *result = frame->parameter_values[ordinal];
    return true;
  }
  case W_SEED_HIR0_VALUE_CALL_RESULT:
    if (value->call_index >= CONSTANT_OUTPUT0_MAX_CALLS ||
        value->call_index >= program->call_count ||
        !frame->call_initialized[value->call_index] ||
        context->values[frame->call_values[value->call_index]].type_index !=
            value->type_index)
      return false;
    *result = frame->call_values[value->call_index];
    return true;
  case W_SEED_HIR0_VALUE_UNARY_I64: {
    uint32_t child_index = W_SEED_HIR0_NONE;
    if (!fixed_i64_type(program, value->type_index) ||
        !eval_value(context, value->left_value, frame, depth + 1u,
                    &child_index) ||
        context->values[child_index].kind != CONSTANT_OUTPUT0_VALUE_I64)
      return false;
    const int64_t child = context->values[child_index].as.integer;
    int64_t evaluated = 0;
    if (value->unary_operator == W_SEED_HIR0_UNARY_NEGATE) {
      if (child == INT64_MIN)
        return false;
      evaluated = -child;
    } else if (value->unary_operator == W_SEED_HIR0_UNARY_BIT_NOT) {
      evaluated = ~child;
    } else {
      return false;
    }
    return new_value(
        context,
        (constant_output0_value){.kind = CONSTANT_OUTPUT0_VALUE_I64,
                                 .type_index = value->type_index,
                                 .as.integer = evaluated},
        result);
  }
  case W_SEED_HIR0_VALUE_BINARY_I64: {
    uint32_t left_index = W_SEED_HIR0_NONE;
    uint32_t right_index = W_SEED_HIR0_NONE;
    if (!eval_value(context, value->left_value, frame, depth + 1u,
                    &left_index) ||
        !eval_value(context, value->right_value, frame, depth + 1u,
                    &right_index) ||
        context->values[left_index].kind != CONSTANT_OUTPUT0_VALUE_I64 ||
        context->values[right_index].kind != CONSTANT_OUTPUT0_VALUE_I64 ||
        !fixed_i64_type(program,
                        program->values[value->left_value].type_index) ||
        !fixed_i64_type(program,
                        program->values[value->right_value].type_index))
      return false;
    const int64_t left = context->values[left_index].as.integer;
    const int64_t right = context->values[right_index].as.integer;
    if (value->binary_operator >= W_SEED_HIR0_BINARY_EQUAL &&
        value->binary_operator <= W_SEED_HIR0_BINARY_GREATER_EQUAL) {
      if (!type_is(program, value->type_index, W_SEED_HIR0_TYPE_BOOL))
        return false;
      bool compared = false;
      switch (value->binary_operator) {
      case W_SEED_HIR0_BINARY_EQUAL:
        compared = left == right;
        break;
      case W_SEED_HIR0_BINARY_NOT_EQUAL:
        compared = left != right;
        break;
      case W_SEED_HIR0_BINARY_LESS:
        compared = left < right;
        break;
      case W_SEED_HIR0_BINARY_LESS_EQUAL:
        compared = left <= right;
        break;
      case W_SEED_HIR0_BINARY_GREATER:
        compared = left > right;
        break;
      case W_SEED_HIR0_BINARY_GREATER_EQUAL:
        compared = left >= right;
        break;
      default:
        return false;
      }
      return new_value(
          context,
          (constant_output0_value){.kind = CONSTANT_OUTPUT0_VALUE_BOOL,
                                   .type_index = value->type_index,
                                   .as.boolean = compared},
          result);
    }
    if (!fixed_i64_type(program, value->type_index))
      return false;
    uint64_t bits = 0u;
    if (!w_seed_scalar_evaluator0_checked_integer_arithmetic(
            value->binary_operator, true, 64u, (uint64_t)left, (uint64_t)right,
            &bits))
      return false;
    int64_t evaluated = 0;
    (void)memcpy(&evaluated, &bits, sizeof(evaluated));
    return new_value(
        context,
        (constant_output0_value){.kind = CONSTANT_OUTPUT0_VALUE_I64,
                                 .type_index = value->type_index,
                                 .as.integer = evaluated},
        result);
  }
  case W_SEED_HIR0_VALUE_INTERPOLATED_STRING:
    return eval_interpolation(context, value_index, frame, depth + 1u, result);
  case W_SEED_HIR0_VALUE_TUPLE:
  case W_SEED_HIR0_VALUE_VALUE_STRUCT: {
    const bool tuple = value->kind == W_SEED_HIR0_VALUE_TUPLE;
    const uint32_t element_count = tuple
                                       ? value->tuple_element_count
                                       : value->value_struct_initializer_count;
    const uint32_t first_element = (uint32_t)context->element_count;
    if (element_count == 0u ||
        (tuple &&
         !type_is(program, value->type_index, W_SEED_HIR0_TYPE_TUPLE)) ||
        (!tuple &&
         !type_is(program, value->type_index, W_SEED_HIR0_TYPE_VALUE_STRUCT)) ||
        !range_valid(tuple ? value->first_tuple_element
                           : value->first_value_struct_initializer,
                     element_count,
                     tuple ? program->tuple_element_count
                           : program->value_struct_initializer_count) ||
        (tuple && program->tuple_elements == NULL) ||
        (!tuple && program->value_struct_initializers == NULL) ||
        element_count > CONSTANT_OUTPUT0_MAX_ELEMENTS - context->element_count)
      return false;
    context->element_count += element_count;
    for (size_t ordinal = 0u; ordinal < element_count; ordinal += 1u) {
      uint32_t child_value = W_SEED_HIR0_NONE;
      uint32_t child_type = W_SEED_HIR0_NONE;
      size_t source_ordinal = ordinal;
      if (tuple) {
        const w_seed_hir0_tuple_element *element =
            &program
                 ->tuple_elements[(size_t)value->first_tuple_element + ordinal];
        if (element->owner_value != value_index || element->ordinal != ordinal)
          return false;
        child_value = element->value_index;
        child_type = element->type_index;
      } else {
        const w_seed_hir0_value_struct_initializer *initializer =
            &program->value_struct_initializers
                 [(size_t)value->first_value_struct_initializer + ordinal];
        if (initializer->owner_value != value_index ||
            initializer->ordinal != ordinal ||
            initializer->field_ordinal >= element_count)
          return false;
        child_value = initializer->value_index;
        child_type = initializer->type_index;
        source_ordinal = initializer->field_ordinal;
      }
      if (child_value >= program->value_count ||
          !fixed_i64_type(program, child_type) ||
          program->values[child_value].type_index != child_type ||
          !eval_value(
              context, child_value, frame, depth + 1u,
              &context->elements[(size_t)first_element + source_ordinal]))
        return false;
    }
    if (tuple) {
      const w_seed_hir0_type *type = &program->types[value->type_index];
      if (type->lifecycle != W_SEED_HIR0_LIFECYCLE_VALUE_COPY ||
          type->tuple_component_count != element_count)
        return false;
      for (size_t ordinal = 0u; ordinal < element_count; ordinal += 1u)
        if (!fixed_i64_type(
                program,
                program
                    ->tuple_components[(size_t)type->first_tuple_component +
                                       ordinal]
                    .type_index))
          return false;
    } else {
      const w_seed_hir0_type *type = &program->types[value->type_index];
      if (type->lifecycle != W_SEED_HIR0_LIFECYCLE_VALUE_COPY ||
          type->value_struct_index >= program->value_struct_count ||
          program->value_structs == NULL)
        return false;
      const w_seed_hir0_value_struct *structure =
          &program->value_structs[type->value_struct_index];
      if (structure->field_count != element_count ||
          !range_valid(structure->first_field, structure->field_count,
                       program->value_struct_field_count) ||
          program->value_struct_fields == NULL)
        return false;
      for (size_t ordinal = 0u; ordinal < element_count; ordinal += 1u)
        if (!fixed_i64_type(
                program,
                program
                    ->value_struct_fields[(size_t)structure->first_field +
                                          ordinal]
                    .type_index))
          return false;
    }
    context->evaluated_flat_product = true;
    return new_value(
        context,
        (constant_output0_value){.kind = CONSTANT_OUTPUT0_VALUE_PRODUCT,
                                 .type_index = value->type_index,
                                 .as.product = {first_element, element_count}},
        result);
  }
  case W_SEED_HIR0_VALUE_TUPLE_ELEMENT:
  case W_SEED_HIR0_VALUE_VALUE_STRUCT_FIELD: {
    uint32_t product_index = W_SEED_HIR0_NONE;
    if (!eval_value(context, value->left_value, frame, depth + 1u,
                    &product_index) ||
        context->values[product_index].kind != CONSTANT_OUTPUT0_VALUE_PRODUCT ||
        context->values[product_index].type_index !=
            program->values[value->left_value].type_index ||
        value->projection_ordinal >=
            context->values[product_index].as.product.element_count)
      return false;
    const uint32_t child_index =
        context->elements[(size_t)context->values[product_index]
                              .as.product.first_element +
                          value->projection_ordinal];
    if (child_index >= context->value_count ||
        context->values[child_index].type_index != value->type_index)
      return false;
    context->evaluated_flat_product = true;
    *result = child_index;
    return true;
  }
  case W_SEED_HIR0_VALUE_INTEGER_WIDEN: {
    uint32_t child_index = W_SEED_HIR0_NONE;
    if (value->left_value >= program->value_count ||
        !fixed_i64_type(program,
                        program->values[value->left_value].type_index) ||
        !fixed_i64_type(program, value->type_index) ||
        !eval_value(context, value->left_value, frame, depth + 1u,
                    &child_index) ||
        context->values[child_index].kind != CONSTANT_OUTPUT0_VALUE_I64)
      return false;
    return new_value(context,
                     (constant_output0_value){
                         .kind = CONSTANT_OUTPUT0_VALUE_I64,
                         .type_index = value->type_index,
                         .as.integer = context->values[child_index].as.integer},
                     result);
  }
  default:
    return false;
  }
}

static bool eval_print_call(constant_output0_context *context,
                            uint32_t call_index, constant_output0_frame *frame,
                            uint32_t *result) {
  const w_seed_hir0_program *program = context->program;
  if (call_index >= program->call_count || program->calls == NULL ||
      !host_print_identity(program, &program->calls[call_index]))
    return false;
  const w_seed_hir0_call *call = &program->calls[call_index];
  if (call->argument_count != 1u ||
      !range_valid(call->first_argument, call->argument_count,
                   program->argument_count) ||
      program->arguments == NULL ||
      call->execution_kind != W_SEED_HIR0_CALL_DIRECT)
    return false;
  const w_seed_hir0_argument *argument =
      &program->arguments[call->first_argument];
  uint32_t value_index = W_SEED_HIR0_NONE;
  if (argument->owner_call != call_index || argument->ordinal != 0u ||
      argument->parameter_ordinal != 0u ||
      !type_is(program, argument->type_index, W_SEED_HIR0_TYPE_STRING) ||
      !eval_value(context, argument->value_index, frame, 0u, &value_index) ||
      context->values[value_index].kind != CONSTANT_OUTPUT0_VALUE_STRING ||
      !append_value_display(context, value_index, 0u, false) ||
      !append_bytes(context->stdout_bytes, sizeof(context->stdout_bytes),
                    &context->stdout_length, (const uint8_t *)"\n", 1u))
    return false;
  return new_value(context,
                   (constant_output0_value){.kind = CONSTANT_OUTPUT0_VALUE_UNIT,
                                            .type_index = call->result_type},
                   result);
}

static bool eval_instruction(constant_output0_context *context,
                             const w_seed_hir0_instruction *instruction,
                             constant_output0_frame *frame, size_t call_depth) {
  const w_seed_hir0_program *program = context->program;
  if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
    if (instruction->binding_index >= CONSTANT_OUTPUT0_MAX_BINDINGS ||
        instruction->binding_index >= program->binding_count ||
        program->bindings == NULL)
      return false;
    const w_seed_hir0_binding *binding =
        &program->bindings[instruction->binding_index];
    if (binding->is_mutable ||
        binding->source_binding != instruction->binding_index ||
        binding->previous_version != W_SEED_HIR0_NONE ||
        !type_is(program, instruction->result_type, W_SEED_HIR0_TYPE_UNIT) ||
        frame->binding_initialized[instruction->binding_index])
      return false;
    uint32_t value_index = W_SEED_HIR0_NONE;
    if (!eval_value(context, binding->initializer_value, frame, 0u,
                    &value_index) ||
        context->values[value_index].type_index != binding->type_index)
      return false;
    frame->binding_values[instruction->binding_index] = value_index;
    frame->binding_initialized[instruction->binding_index] = true;
    return true;
  }
  if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
      instruction->call_index >= program->call_count ||
      instruction->call_index >= CONSTANT_OUTPUT0_MAX_CALLS ||
      frame->call_initialized[instruction->call_index])
    return false;
  uint32_t value_index = W_SEED_HIR0_NONE;
  const w_seed_hir0_call *call = &program->calls[instruction->call_index];
  bool success = false;
  if (host_print_identity(program, call)) {
    success =
        eval_print_call(context, instruction->call_index, frame, &value_index);
  } else {
    success = eval_local_call(context, instruction->call_index, frame,
                              call_depth, &value_index);
  }
  if (!success || value_index >= context->value_count ||
      context->values[value_index].type_index != instruction->result_type)
    return false;
  frame->call_values[instruction->call_index] = value_index;
  frame->call_initialized[instruction->call_index] = true;
  return true;
}

static bool eval_function(constant_output0_context *context,
                          uint32_t function_index, const uint32_t *parameters,
                          size_t parameter_count, size_t call_depth,
                          uint32_t *result) {
  const w_seed_hir0_program *program = context->program;
  if (function_index >= program->function_count ||
      function_index >= CONSTANT_OUTPUT0_MAX_FUNCTIONS ||
      call_depth >= CONSTANT_OUTPUT0_MAX_FUNCTIONS || result == NULL ||
      (parameter_count != 0u && parameters == NULL) ||
      parameter_count > CONSTANT_OUTPUT0_MAX_PARAMETERS ||
      context->active_functions[function_index] ||
      !function_is_effect_safe(program, function_index))
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->parameter_count != parameter_count ||
      function->block_count == 0u ||
      function->block_count > CONSTANT_OUTPUT0_MAX_BLOCKS ||
      !range_valid(function->first_block, function->block_count,
                   program->block_count) ||
      !range_valid(function->first_parameter, function->parameter_count,
                   program->parameter_count) ||
      program->blocks == NULL || program->parameters == NULL ||
      program->terminators == NULL || program->instructions == NULL ||
      program->bindings == NULL)
    return false;
  constant_output0_frame *frame = &context->frames[call_depth];
  (void)memset(frame, 0, sizeof(*frame));
  frame->function_index = function_index;
  for (size_t ordinal = 0u; ordinal < parameter_count; ordinal += 1u) {
    const w_seed_hir0_parameter *parameter =
        &program->parameters[(size_t)function->first_parameter + ordinal];
    if (parameter->owner_function != function_index ||
        parameter->ordinal != ordinal ||
        parameters[ordinal] >= context->value_count ||
        context->values[parameters[ordinal]].type_index !=
            parameter->type_index)
      return false;
    frame->parameter_values[ordinal] = parameters[ordinal];
    frame->parameter_initialized[ordinal] = true;
  }
  context->active_functions[function_index] = true;
  uint32_t block_index = function->first_block;
  bool terminated = false;
  bool ok = true;
  while (ok && !terminated) {
    if (block_index < function->first_block ||
        block_index - function->first_block >= function->block_count ||
        block_index >= program->block_count ||
        frame->visited_blocks[block_index - function->first_block] ||
        ++context->steps > CONSTANT_OUTPUT0_MAX_STEPS) {
      ok = false;
      break;
    }
    const w_seed_hir0_block *block = &program->blocks[block_index];
    if (block->owner_function != function_index ||
        block->first_block_argument != W_SEED_HIR0_NONE ||
        block->block_argument_count != 0u ||
        !range_valid(block->first_instruction, block->instruction_count,
                     program->instruction_count) ||
        block->terminator_index >= program->terminator_count) {
      ok = false;
      break;
    }
    frame->visited_blocks[block_index - function->first_block] = true;
    for (size_t ordinal = 0u; ok && ordinal < block->instruction_count;
         ordinal += 1u) {
      if (++context->steps > CONSTANT_OUTPUT0_MAX_STEPS) {
        ok = false;
        break;
      }
      const w_seed_hir0_instruction *instruction =
          &program->instructions[(size_t)block->first_instruction + ordinal];
      if (instruction->owner_block != block_index ||
          instruction->ordinal != ordinal ||
          !eval_instruction(context, instruction, frame, call_depth))
        ok = false;
    }
    if (!ok)
      break;
    const w_seed_hir0_terminator *terminator =
        &program->terminators[block->terminator_index];
    if (terminator->owner_block != block_index) {
      ok = false;
      break;
    }
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT) {
      if (!type_is(program, function->return_type, W_SEED_HIR0_TYPE_UNIT)) {
        ok = false;
        break;
      }
      ok = new_value(
          context,
          (constant_output0_value){.kind = CONSTANT_OUTPUT0_VALUE_UNIT,
                                   .type_index = function->return_type},
          result);
      terminated = ok;
    } else if (terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE) {
      uint32_t returned = W_SEED_HIR0_NONE;
      ok = eval_value(context, terminator->value_index, frame, 0u, &returned) &&
           context->values[returned].type_index == function->return_type;
      if (ok)
        *result = returned;
      terminated = ok;
    } else if (terminator->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
      uint32_t condition = W_SEED_HIR0_NONE;
      ok =
          terminator->edge_argument_count == 0u &&
          terminator->target_block != W_SEED_HIR0_NONE &&
          terminator->else_block != W_SEED_HIR0_NONE &&
          eval_value(context, terminator->value_index, frame, 0u, &condition) &&
          context->values[condition].kind == CONSTANT_OUTPUT0_VALUE_BOOL;
      if (ok)
        block_index = context->values[condition].as.boolean
                          ? terminator->target_block
                          : terminator->else_block;
    } else if (terminator->kind == W_SEED_HIR0_TERMINATOR_JUMP) {
      ok = terminator->edge_argument_count == 0u &&
           terminator->target_block != W_SEED_HIR0_NONE &&
           terminator->else_block == W_SEED_HIR0_NONE;
      if (ok)
        block_index = terminator->target_block;
    } else {
      /* Throws, faults, panic, invokes, enum captures, and all other effects
       * are deliberately left to the normal lowering path. */
      ok = false;
    }
  }
  context->active_functions[function_index] = false;
  return ok && terminated;
}

bool w_seed_constant_output0_evaluate(const w_seed_hir0_program *program,
                                      const w_seed_hir0_result *hir_result,
                                      w_seed_constant_output0_result *result) {
  if (program == NULL || hir_result == NULL || result == NULL ||
      !w_seed_hir0_verify(program, hir_result) || program->entry_count != 1u ||
      program->entries == NULL || program->cleanup_count != 0u ||
      program->function_count == 0u ||
      program->function_count > CONSTANT_OUTPUT0_MAX_FUNCTIONS ||
      program->binding_count > CONSTANT_OUTPUT0_MAX_BINDINGS ||
      program->call_count > CONSTANT_OUTPUT0_MAX_CALLS ||
      program->block_count > CONSTANT_OUTPUT0_MAX_BLOCKS ||
      program->value_count == 0u ||
      program->value_count > CONSTANT_OUTPUT0_MAX_SOURCE_VALUES ||
      program->types == NULL || program->values == NULL)
    return false;
  const w_seed_hir0_entry *entry = &program->entries[0];
  if (entry->adapter_kind != W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT ||
      entry->cleanup_obligation != W_SEED_HIR0_ENTRY_CLEANUP_NONE ||
      entry->cleanup_owner_parameter_count != 0u ||
      entry->target_function >= program->function_count ||
      entry->target_identity >= program->identity_count ||
      entry->identity_index >= program->identity_count ||
      program->identities == NULL)
    return false;
  const w_seed_hir0_function *root =
      &program->functions[entry->target_function];
  if (program->identities[entry->identity_index].kind !=
          W_SEED_HIR0_IDENTITY_ENTRY ||
      program->identities[entry->identity_index].target_index != 0u ||
      program->identities[entry->target_identity].kind !=
          W_SEED_HIR0_IDENTITY_FUNCTION ||
      program->identities[entry->target_identity].target_index !=
          entry->target_function ||
      root->identity_index != entry->target_identity ||
      root->parameter_count != 0u ||
      !type_is(program, root->return_type, W_SEED_HIR0_TYPE_UNIT) ||
      !function_is_effect_safe(program, entry->target_function))
    return false;

  constant_output0_context context;
  (void)memset(&context, 0, sizeof(context));
  context.program = program;
  uint32_t ignored = W_SEED_HIR0_NONE;
  if (!eval_function(&context, entry->target_function, NULL, 0u, 0u,
                     &ignored) ||
      context.stdout_length == 0u)
    return false;
  w_seed_constant_output0_result candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  candidate.stdout_length = context.stdout_length;
  candidate.success_exit_status = 0;
  candidate.evaluated_flat_product = context.evaluated_flat_product;
  (void)memcpy(candidate.stdout_bytes, context.stdout_bytes,
               context.stdout_length);
  (void)memcpy(candidate.hir_semantic_digest, hir_result->semantic_digest,
               sizeof(candidate.hir_semantic_digest));
  (void)memcpy(candidate.hir_provenance_digest, hir_result->provenance_digest,
               sizeof(candidate.hir_provenance_digest));
  *result = candidate;
  return true;
}
