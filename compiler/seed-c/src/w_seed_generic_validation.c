#include "w_seed_generic_validation.h"

#include <string.h>

typedef enum {
  CONVERSION_OK = 0,
  CONVERSION_UNSUPPORTED,
  CONVERSION_INVALID,
  CONVERSION_CAPACITY,
} conversion_status;

typedef struct {
  const w_seed_generic_validation_input *input;
  const w_seed_frontend_output *frontend;
  const w_seed_frontend_result *frontend_result;
  const w_seed_constir_program *program;
  size_t arena_count;
} validation_context;

typedef struct {
  uint32_t module_index;
  uint32_t argument_index;
  uint32_t argument_const_value_index;
  uint32_t typed_const_expression_index;
  uint32_t parameter_index;
  uint32_t effective_domain_type_index;
  uint32_t predicate_function_index;
  uint32_t predicate_constir_index;
  uint32_t value_index;
  const w_seed_frontend_generic_argument *argument;
  const w_seed_frontend_generic_parameter *parameter;
  const w_seed_constir_function *function;
} predicate_candidate;

typedef enum {
  FINGERPRINT_ENCODED = 0,
  FINGERPRINT_UNSUPPORTED,
  FINGERPRINT_INVALID,
} fingerprint_encode_status;

typedef struct {
  w_seed_sha256_state sha;
  bool unsupported;
  bool invalid;
} fingerprint_builder;

static const char FALLBACK_BYTES[] = "predicate:false";
static const uint8_t FINGERPRINT_PREFIX[] =
    "w-seed-generic-fingerprint-1";

_Static_assert(W_SEED_GENERIC_VALIDATION_MAX_PREDICATES <=
                   W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_RECORDS,
               "generic evidence record ceiling must cover predicates");
_Static_assert(sizeof("predicate:false") - 1u <=
                   W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES,
               "generic rejection evidence exceeds byte ceiling");
_Static_assert(sizeof(FALLBACK_BYTES) - 1u ==
                   W_SEED_GENERIC_VALIDATION_FALLBACK_BYTES,
               "generic fallback byte count must stay canonical");

static bool range_valid(size_t start, size_t count, size_t total) {
  return start <= total && count <= total - start;
}

static bool span_valid(w_seed_span span) {
  return span.start_byte <= span.end_byte;
}

static bool text_valid(w_seed_frontend_text text) {
  return text.length == 0u || text.data != NULL;
}

static bool text_equal(w_seed_frontend_text left, w_seed_frontend_text right) {
  return left.length == right.length &&
         (left.length == 0u ||
          (left.data != NULL && right.data != NULL &&
           memcmp(left.data, right.data, left.length) == 0));
}

static bool pointer_count_valid(const void *pointer, size_t count,
                                size_t capacity) {
  return count == 0u || (pointer != NULL && capacity >= count);
}

static bool frontend_arrays_valid(const validation_context *context) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL ||
      (context->frontend_result->status != W_SEED_FRONTEND_OK &&
       context->frontend_result->status != W_SEED_FRONTEND_UNSUPPORTED))
    return false;
  const w_seed_frontend_output *output = context->frontend;
  const w_seed_frontend_counts *written = &context->frontend_result->written;
  return pointer_count_valid(output->modules, written->modules,
                             output->module_capacity) &&
         pointer_count_valid(output->structs, written->structs,
                             output->struct_capacity) &&
         pointer_count_valid(output->type_declarations,
                             written->type_declarations,
                             output->type_declaration_capacity) &&
         pointer_count_valid(output->aliases, written->aliases,
                             output->alias_capacity) &&
         pointer_count_valid(output->types, written->types,
                             output->type_capacity) &&
         pointer_count_valid(output->functions, written->functions,
                             output->function_capacity) &&
         pointer_count_valid(output->expressions, written->expressions,
                             output->expression_capacity) &&
         pointer_count_valid(output->parameters, written->parameters,
                             output->parameter_capacity) &&
         pointer_count_valid(output->generic_parameters,
                             written->generic_parameters,
                             output->generic_parameter_capacity) &&
         pointer_count_valid(output->generic_applications,
                             written->generic_applications,
                             output->generic_application_capacity) &&
         pointer_count_valid(output->generic_arguments,
                             written->generic_arguments,
                             output->generic_argument_capacity) &&
         pointer_count_valid(output->typed_const_expressions,
                             written->typed_const_expressions,
                             output->typed_const_expression_capacity) &&
         pointer_count_valid(output->const_values, written->const_values,
                             output->const_value_capacity) &&
         pointer_count_valid(output->const_elements, written->const_elements,
                             output->const_element_capacity) &&
         pointer_count_valid(output->const_bytes, written->const_bytes,
                             output->const_bytes_capacity) &&
         pointer_count_valid(output->enums, written->enums,
                             output->enum_capacity) &&
         pointer_count_valid(output->enum_cases, written->enum_cases,
                             output->enum_case_capacity) &&
         pointer_count_valid(output->enum_case_parameters,
                             written->enum_case_parameters,
                             output->enum_case_parameter_capacity) &&
         pointer_count_valid(output->const_declarations,
                             written->const_declarations,
                             output->const_declaration_capacity) &&
         pointer_count_valid(output->enum_subset_members,
                             written->enum_subset_members,
                             output->enum_subset_member_capacity);
}

static bool type_index_valid_depth(const validation_context *context,
                                   uint32_t type_index, size_t depth) {
  if (context == NULL || context->frontend_result == NULL ||
      context->frontend == NULL || type_index == W_SEED_FRONTEND_NONE ||
      depth == 0u || depth > W_SEED_GENERIC_VALIDATION_MAX_DEPTH ||
      (size_t)type_index >= context->frontend_result->written.types ||
      context->frontend->types == NULL)
    return false;
  const w_seed_frontend_type *type = &context->frontend->types[type_index];
  if (type->kind == W_SEED_FRONTEND_TYPE_INVALID ||
      !text_valid(type->spelling) || !text_valid(type->nominal_name))
    return false;
  if (type->kind == W_SEED_FRONTEND_TYPE_STATIC_LIST) {
    if (type->element_type == W_SEED_FRONTEND_NONE ||
        !type_index_valid_depth(context, type->element_type, depth + 1u))
      return false;
  }
  if (type->kind == W_SEED_FRONTEND_TYPE_ENUM ||
      type->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET) {
    if (type->enum_base_index == W_SEED_FRONTEND_NONE ||
        (size_t)type->enum_base_index >=
            context->frontend_result->written.enums ||
        context->frontend->enums == NULL)
      return false;
  }
  if (type->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET) {
    if (type->enum_base_index == W_SEED_FRONTEND_NONE ||
        (size_t)type->enum_base_index >= context->frontend_result->written.enums ||
        (type->subset_member_count == 0u &&
         type->first_subset_member != W_SEED_FRONTEND_NONE) ||
        (type->subset_member_count != 0u &&
         (context->frontend->enum_subset_members == NULL ||
          type->first_subset_member == W_SEED_FRONTEND_NONE ||
          !range_valid(type->first_subset_member, type->subset_member_count,
                       context->frontend_result->written.enum_subset_members))))
      return false;
  }
  if (type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
      (type->bit_width == 0u || type->bit_width > 128u))
    return false;
  return true;
}

static bool type_index_valid(const validation_context *context,
                             uint32_t type_index) {
  return type_index_valid_depth(context, type_index, 1u);
}

static bool enum_case_valid(const validation_context *context,
                            uint32_t enum_base, uint32_t enum_case,
                            bool require_payloadless) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || enum_base == W_SEED_FRONTEND_NONE ||
      enum_case == W_SEED_FRONTEND_NONE ||
      (size_t)enum_base >= context->frontend_result->written.enums ||
      (size_t)enum_case >= context->frontend_result->written.enum_cases ||
      context->frontend->enums == NULL || context->frontend->enum_cases == NULL)
    return false;
  const w_seed_frontend_enum_case *value =
      &context->frontend->enum_cases[enum_case];
  const w_seed_frontend_enum *enumeration =
      &context->frontend->enums[enum_base];
  if (!range_valid(enumeration->first_case, enumeration->case_count,
                   context->frontend_result->written.enum_cases) ||
      enumeration->module_index >= context->frontend_result->written.modules ||
      enumeration->name.length == 0u || !text_valid(enumeration->name) ||
      !span_valid(enumeration->span) ||
      enum_case < enumeration->first_case ||
      (size_t)enum_case - enumeration->first_case >=
          enumeration->case_count ||
      value->owner_enum != enum_base ||
      value->module_index != enumeration->module_index ||
      !span_valid(value->span) ||
      value->name.length == 0u || !text_valid(value->name) ||
      !range_valid(value->first_payload, value->payload_count,
                   context->frontend_result->written.enum_case_parameters))
    return false;
  if (require_payloadless && value->payload_count != 0u) return false;
  return true;
}

static bool enum_case_member_of_type(const validation_context *context,
                                     const w_seed_frontend_type *type,
                                     uint32_t expected_type_index,
                                     uint32_t enum_base,
                                     uint32_t enum_case) {
  if (context == NULL || type == NULL ||
      !enum_case_valid(context, enum_base, enum_case, false) ||
      type->enum_base_index != enum_base ||
      expected_type_index >= context->frontend_result->written.types)
    return false;
  if (type->kind == W_SEED_FRONTEND_TYPE_ENUM) return true;
  if (type->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET ||
      context->frontend->enum_subset_members == NULL)
    return false;
  for (uint32_t offset = 0u; offset < type->subset_member_count; offset += 1u) {
    const w_seed_frontend_enum_subset_member *item =
        &context->frontend->enum_subset_members[
            (size_t)type->first_subset_member + offset];
    if (!enum_case_valid(context, item->enum_base_index,
                         item->enum_case_index, false) ||
        item->owner_type != expected_type_index ||
        !span_valid(item->source_span) ||
        item->enum_base_index != type->enum_base_index)
      return false;
    if (item->enum_base_index == enum_base &&
        item->enum_case_index == enum_case)
      return true;
  }
  return false;
}

static bool frontend_integer_canonical(
    const w_seed_frontend_const_value *value,
    const w_seed_frontend_type *type) {
  if (value == NULL || type == NULL ||
      type->kind != W_SEED_FRONTEND_TYPE_INTEGER ||
      value->integer_bit_width == 0u || value->integer_bit_width > 128u ||
      value->integer_bit_width != type->bit_width ||
      value->integer_signed != type->is_signed)
    return false;
  const size_t byte_count =
      ((size_t)value->integer_bit_width + 7u) / 8u;
  if (value->integer_byte_count != byte_count || byte_count == 0u ||
      byte_count > W_SEED_CONSTIR_INTEGER_BYTES)
    return false;
  for (size_t index = byte_count; index < W_SEED_CONSTIR_INTEGER_BYTES;
       index += 1u) {
    if (value->integer_bytes[index] != 0u) return false;
  }
  const unsigned remainder = (unsigned)(value->integer_bit_width % 8u);
  if (remainder != 0u &&
      (value->integer_bytes[byte_count - 1u] &
       (uint8_t)~((uint8_t)((1u << remainder) - 1u))) != 0u)
    return false;
  /* Frontend immediate integers use a non-negative magnitude.  A signed
   * value therefore cannot set its sign bit in the canonical bytes. */
  if (value->integer_signed &&
      (value->integer_bytes[(value->integer_bit_width - 1u) / 8u] &
       (uint8_t)(1u << ((value->integer_bit_width - 1u) % 8u))) != 0u)
    return false;
  return true;
}

static bool frontend_const_value_basic_valid(const validation_context *context,
                                             uint32_t value_index) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->const_values == NULL ||
      (size_t)value_index >= context->frontend_result->written.const_values)
    return false;
  const w_seed_frontend_const_value *value =
      &context->frontend->const_values[value_index];
  if (!span_valid(value->span) || !type_index_valid(context, value->type_index))
    return false;
  const w_seed_frontend_type *type = &context->frontend->types[value->type_index];
  switch (value->kind) {
    case W_SEED_FRONTEND_CONST_BOOL:
      return type->kind == W_SEED_FRONTEND_TYPE_BOOL;
    case W_SEED_FRONTEND_CONST_INTEGER:
      return frontend_integer_canonical(value, type);
    case W_SEED_FRONTEND_CONST_STRING:
      return type->kind == W_SEED_FRONTEND_TYPE_STRING &&
             context->frontend->const_bytes_capacity >=
                 context->frontend_result->written.const_bytes &&
             (value->byte_count == 0u ||
              context->frontend->const_bytes != NULL) &&
             range_valid(value->first_byte, value->byte_count,
                         context->frontend_result->written.const_bytes);
    case W_SEED_FRONTEND_CONST_ENUM_CASE:
      return enum_case_valid(context, value->enum_base_index,
                             value->enum_case_index, false);
    case W_SEED_FRONTEND_CONST_STATIC_LIST:
      if (value->element_count == 0u)
        return value->first_element == W_SEED_FRONTEND_NONE;
      return value->first_element != W_SEED_FRONTEND_NONE &&
             range_valid(value->first_element, value->element_count,
                         context->frontend_result->written.const_elements);
    case W_SEED_FRONTEND_CONST_INVALID:
      return false;
  }
  return false;
}

static bool span_contains(w_seed_span outer, w_seed_span inner) {
  return span_valid(outer) && span_valid(inner) &&
         outer.start_byte <= inner.start_byte &&
         inner.end_byte <= outer.end_byte;
}

/* Validate only normalized ConstValue relations.  D1 support decisions stay
 * in conversion_value_count; this pass catches cross-index corruption before
 * any evaluator call, including applications without a predicate. */
static bool frontend_const_value_relation_valid(
    const validation_context *context, uint32_t value_index, size_t depth) {
  if (context == NULL || depth == 0u ||
      depth > W_SEED_GENERIC_VALIDATION_MAX_DEPTH ||
      !frontend_const_value_basic_valid(context, value_index))
    return false;
  const w_seed_frontend_const_value *value =
      &context->frontend->const_values[value_index];
  if (value->kind != W_SEED_FRONTEND_CONST_STATIC_LIST) return true;
  if (value->element_count == 0u) return true;
  if (value->element_count > W_SEED_FRONTEND_MAX_STATIC_LIST_ELEMENTS)
    return true;
  for (uint32_t offset = 0u; offset < value->element_count; offset += 1u) {
    const w_seed_frontend_const_element *element =
        &context->frontend->const_elements[(size_t)value->first_element + offset];
    if (element->owner_value != value_index || element->ordinal != offset ||
        !span_contains(value->span, element->span) ||
        !frontend_const_value_relation_valid(context, element->value_index,
                                             depth + 1u))
      return false;
  }
  return true;
}

static bool frontend_function_index_valid(const validation_context *context,
                                          uint32_t function_index) {
  return context != NULL && context->frontend_result != NULL &&
         context->frontend != NULL && function_index != W_SEED_FRONTEND_NONE &&
         (size_t)function_index < context->frontend_result->written.functions &&
         context->frontend->functions != NULL;
}

static const w_seed_constir_function *constir_function_for_frontend(
    const w_seed_constir_program *program, uint32_t frontend_function,
    size_t *index, bool *duplicate) {
  if (index != NULL) *index = SIZE_MAX;
  if (duplicate != NULL) *duplicate = false;
  if (program == NULL || program->functions == NULL ||
      frontend_function == W_SEED_CONSTIR_NONE)
    return NULL;
  const w_seed_constir_function *found = NULL;
  size_t found_index = SIZE_MAX;
  for (size_t offset = 0u; offset < program->function_count; offset += 1u) {
    if (program->functions[offset].frontend_function != frontend_function)
      continue;
    if (found != NULL) {
      if (duplicate != NULL) *duplicate = true;
      return NULL;
    }
    found = &program->functions[offset];
    found_index = offset;
  }
  if (index != NULL) *index = found_index;
  return found;
}

static const w_seed_constir_function *constir_function_for_typed_expression(
    const w_seed_constir_program *program, uint32_t typed_index,
    size_t *index, bool *duplicate) {
  if (index != NULL) *index = SIZE_MAX;
  if (duplicate != NULL) *duplicate = false;
  if (program == NULL || program->functions == NULL ||
      typed_index == W_SEED_CONSTIR_NONE)
    return NULL;
  const w_seed_constir_function *found = NULL;
  size_t found_index = SIZE_MAX;
  for (size_t offset = 0u; offset < program->function_count; offset += 1u) {
    const w_seed_constir_function *function = &program->functions[offset];
    if (function->origin !=
            W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION ||
        function->typed_const_expression_index != typed_index)
      continue;
    if (found != NULL) {
      if (duplicate != NULL) *duplicate = true;
      return NULL;
    }
    found = function;
    found_index = offset;
  }
  if (index != NULL) *index = found_index;
  return found;
}

typedef enum {
  CONST_GRAPH_OK = 0,
  CONST_GRAPH_CYCLE,
  CONST_GRAPH_UNSUPPORTED,
  CONST_GRAPH_INVALID,
  CONST_GRAPH_LIMIT,
} const_graph_status;

typedef struct {
  const validation_context *context;
  uint32_t path[W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH];
  size_t path_length;
  uint32_t seen[W_SEED_GENERIC_VALIDATION_MAX_CONST_DEPENDENCIES];
  size_t seen_count;
  uint32_t cycle[W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH];
  size_t cycle_length;
} const_graph_context;

static const w_seed_constir_function *constir_function_for_const_declaration(
    const w_seed_constir_program *program, uint32_t declaration_index,
    size_t *function_index, bool *duplicate) {
  if (function_index != NULL) *function_index = SIZE_MAX;
  if (duplicate != NULL) *duplicate = false;
  if (program == NULL || program->functions == NULL ||
      declaration_index == W_SEED_CONSTIR_NONE)
    return NULL;
  const w_seed_constir_function *found = NULL;
  size_t found_index = SIZE_MAX;
  for (size_t offset = 0u; offset < program->function_count; offset += 1u) {
    const w_seed_constir_function *function = &program->functions[offset];
    if (function->origin !=
            W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION ||
        function->frontend_const_declaration != declaration_index)
      continue;
    if (found != NULL) {
      if (duplicate != NULL) *duplicate = true;
      return NULL;
    }
    found = function;
    found_index = offset;
  }
  if (function_index != NULL) *function_index = found_index;
  return found;
}

static bool const_graph_seen(const const_graph_context *graph,
                             uint32_t function_index) {
  if (graph == NULL) return false;
  for (size_t offset = 0u; offset < graph->seen_count; offset += 1u)
    if (graph->seen[offset] == function_index) return true;
  return false;
}

static bool const_graph_on_path(const const_graph_context *graph,
                                uint32_t function_index, size_t *position) {
  if (position != NULL) *position = SIZE_MAX;
  if (graph == NULL) return false;
  for (size_t offset = 0u; offset < graph->path_length; offset += 1u) {
    if (graph->path[offset] == function_index) {
      if (position != NULL) *position = offset;
      return true;
    }
  }
  return false;
}

static const_graph_status const_graph_visit_node(const_graph_context *graph,
                                                 uint32_t function_index,
                                                 uint32_t node_index,
                                                 size_t depth);

static const_graph_status const_graph_visit_function(
    const_graph_context *graph, uint32_t function_index, size_t depth) {
  if (graph == NULL || graph->context == NULL ||
      graph->context->program == NULL ||
      (size_t)function_index >= graph->context->program->function_count)
    return CONST_GRAPH_INVALID;
  const w_seed_constir_function *function =
      &graph->context->program->functions[function_index];
  if (!function->lowerable || function->root_node == W_SEED_CONSTIR_NONE)
    return CONST_GRAPH_UNSUPPORTED;
  /* A dependency edge starts a fresh expression-depth budget.  The graph
   * itself is bounded by MAX_CONST_DEPENDENCIES; charging each edge to the
   * expression depth would reject a valid 256-member forward chain. */
  (void)depth;
  return const_graph_visit_node(graph, function_index, function->root_node, 1u);
}

static const_graph_status const_graph_visit_const_target(
    const_graph_context *graph, uint32_t declaration_index, size_t depth) {
  if (graph == NULL || graph->context == NULL ||
      graph->context->program == NULL || declaration_index == W_SEED_CONSTIR_NONE)
    return CONST_GRAPH_INVALID;
  bool duplicate = false;
  size_t target_index = SIZE_MAX;
  const w_seed_constir_function *target =
      constir_function_for_const_declaration(graph->context->program,
                                             declaration_index, &target_index,
                                             &duplicate);
  if (duplicate || target == NULL || target_index == SIZE_MAX ||
      target->origin !=
          W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION)
    return CONST_GRAPH_INVALID;
  size_t cycle_start = SIZE_MAX;
  if (const_graph_on_path(graph, (uint32_t)target_index, &cycle_start)) {
    const size_t cycle_members = graph->path_length - cycle_start;
    if (cycle_members == 0u)
      return CONST_GRAPH_INVALID;
    if (cycle_members > W_SEED_GENERIC_VALIDATION_MAX_CONST_DEPENDENCIES ||
        cycle_members + 1u > W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH)
      return CONST_GRAPH_LIMIT;
    graph->cycle_length = cycle_members + 1u;
    for (size_t offset = 0u; offset < cycle_members; offset += 1u)
      graph->cycle[offset] = graph->path[cycle_start + offset];
    graph->cycle[cycle_members] = (uint32_t)target_index;
    return CONST_GRAPH_CYCLE;
  }
  if (const_graph_seen(graph, (uint32_t)target_index))
    return CONST_GRAPH_OK;
  if (graph->seen_count >= W_SEED_GENERIC_VALIDATION_MAX_CONST_DEPENDENCIES)
    return CONST_GRAPH_LIMIT;
  graph->seen[graph->seen_count] = (uint32_t)target_index;
  /* The entry is provisional.  const_graph_visit_function replaces it with
   * the same identity only after its complete dependency tree succeeds. */
  graph->seen_count += 1u;
  if (graph->path_length >= W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH)
    return CONST_GRAPH_LIMIT;
  graph->path[graph->path_length++] = (uint32_t)target_index;
  const_graph_status status =
      const_graph_visit_function(graph, (uint32_t)target_index, 1u);
  (void)depth;
  graph->path_length -= 1u;
  return status;
}

static const_graph_status const_graph_visit_node(const_graph_context *graph,
                                                 uint32_t function_index,
                                                 uint32_t node_index,
                                                 size_t depth) {
  if (graph == NULL || graph->context == NULL ||
      graph->context->program == NULL || depth == 0u ||
      depth > W_SEED_GENERIC_VALIDATION_MAX_DEPTH ||
      (size_t)node_index >= graph->context->program->node_count)
    return CONST_GRAPH_INVALID;
  const w_seed_constir_program *program = graph->context->program;
  const w_seed_constir_node *node = &program->nodes[node_index];
  if (node->owner_function !=
          (program->functions[function_index].origin ==
                   W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION
               ? program->functions[function_index].frontend_function
               : W_SEED_CONSTIR_NONE))
    return CONST_GRAPH_INVALID;
  if (node->kind == W_SEED_CONSTIR_NODE_CALL &&
      node->call_target_const_declaration != W_SEED_CONSTIR_NONE) {
    const_graph_status status = const_graph_visit_const_target(
        graph, node->call_target_const_declaration, depth + 1u);
    if (status != CONST_GRAPH_OK) return status;
  }
  if (node->left != W_SEED_CONSTIR_NONE) {
    const_graph_status status = const_graph_visit_node(
        graph, function_index, node->left, depth + 1u);
    if (status != CONST_GRAPH_OK) return status;
  }
  if (node->right != W_SEED_CONSTIR_NONE) {
    const_graph_status status = const_graph_visit_node(
        graph, function_index, node->right, depth + 1u);
    if (status != CONST_GRAPH_OK) return status;
  }
  if (node->kind == W_SEED_CONSTIR_NODE_CALL &&
      node->call_argument_count != 0u) {
    if (program->call_arguments == NULL ||
        !range_valid(node->first_call_argument, node->call_argument_count,
                     program->call_argument_count))
      return CONST_GRAPH_INVALID;
    for (uint32_t offset = 0u; offset < node->call_argument_count;
         offset += 1u) {
      const w_seed_constir_call_argument *argument = &program->call_arguments[
          (size_t)node->first_call_argument + offset];
      const_graph_status status = const_graph_visit_node(
          graph, function_index, argument->node_index, depth + 1u);
      if (status != CONST_GRAPH_OK) return status;
    }
  }
  if (node->kind == W_SEED_CONSTIR_NODE_SWITCH &&
      node->switch_arm_count != 0u) {
    if (program->switch_arms == NULL ||
        !range_valid(node->first_switch_arm, node->switch_arm_count,
                     program->switch_arm_count))
      return CONST_GRAPH_INVALID;
    for (uint32_t offset = 0u; offset < node->switch_arm_count; offset += 1u) {
      const w_seed_constir_switch_arm *arm = &program->switch_arms[
          (size_t)node->first_switch_arm + offset];
      const_graph_status status = const_graph_visit_node(
          graph, function_index, arm->result_node, depth + 1u);
      if (status != CONST_GRAPH_OK) return status;
    }
  }
  return CONST_GRAPH_OK;
}

static const_graph_status const_graph_preflight(
    const validation_context *context, const w_seed_frontend_generic_application *application,
    const uint32_t typed_function_indices[W_SEED_FRONTEND_MAX_GENERIC_SLOTS],
    uint32_t cycle_path[W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH],
    size_t *cycle_path_length, uint32_t *cycle_argument_offset) {
  if (cycle_path_length != NULL) *cycle_path_length = 0u;
  if (cycle_argument_offset != NULL) *cycle_argument_offset = W_SEED_FRONTEND_NONE;
  if (context == NULL || context->program == NULL || application == NULL ||
      typed_function_indices == NULL)
    return CONST_GRAPH_INVALID;
  const_graph_context graph;
  (void)memset(&graph, 0, sizeof(graph));
  graph.context = context;
  for (uint32_t offset = 0u; offset < application->argument_count; offset += 1u) {
    const uint32_t function_index = typed_function_indices[offset];
    if (function_index == W_SEED_CONSTIR_NONE) continue;
    if ((size_t)function_index >= context->program->function_count)
      return CONST_GRAPH_INVALID;
    const_graph_status status;
    const w_seed_constir_function *function =
        &context->program->functions[function_index];
    if (function->origin !=
        W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION)
      return CONST_GRAPH_INVALID;
    status = const_graph_visit_node(&graph, function_index, function->root_node, 1u);
    if (status != CONST_GRAPH_OK) {
      if (status == CONST_GRAPH_CYCLE && cycle_path != NULL &&
          cycle_path_length != NULL) {
        *cycle_path_length = graph.cycle_length;
        (void)memcpy(cycle_path, graph.cycle,
                     graph.cycle_length * sizeof(graph.cycle[0]));
        if (cycle_argument_offset != NULL) *cycle_argument_offset = offset;
      }
      return status;
    }
  }
  return CONST_GRAPH_OK;
}

static conversion_status append_value(validation_context *context,
                                      const w_seed_constir_value *value,
                                      uint32_t *index) {
  if (context == NULL || value == NULL || index == NULL ||
      context->input == NULL ||
      context->input->conversion_values == NULL ||
      context->arena_count >= context->input->conversion_value_capacity ||
      context->arena_count > (size_t)UINT32_MAX)
    return CONVERSION_CAPACITY;
  *index = (uint32_t)context->arena_count;
  context->input->conversion_values[context->arena_count] = *value;
  context->arena_count += 1u;
  return CONVERSION_OK;
}

static conversion_status convert_const_value(validation_context *context,
                                             uint32_t value_index,
                                             uint32_t expected_type_index,
                                             size_t depth, uint32_t *arena_index);

static conversion_status convert_enum_value(validation_context *context,
                                            const w_seed_frontend_const_value *source,
                                            const w_seed_frontend_type *expected,
                                            uint32_t expected_type_index,
                                            uint32_t *arena_index) {
  if (!enum_case_valid(context, source->enum_base_index,
                       source->enum_case_index, true))
    return CONVERSION_UNSUPPORTED;
  if (!enum_case_member_of_type(context, expected, expected_type_index,
                                source->enum_base_index,
                                source->enum_case_index))
    return CONVERSION_INVALID;
  w_seed_constir_value converted;
  if (!w_seed_constir_value_enum(source->type_index, source->enum_base_index,
                                 source->enum_case_index, &converted))
    return CONVERSION_INVALID;
  return append_value(context, &converted, arena_index);
}

static conversion_status convert_const_value(validation_context *context,
                                             uint32_t value_index,
                                             uint32_t expected_type_index,
                                             size_t depth, uint32_t *arena_index) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || arena_index == NULL || depth == 0u ||
      depth > W_SEED_GENERIC_VALIDATION_MAX_DEPTH ||
      !type_index_valid(context, expected_type_index) ||
      !frontend_const_value_basic_valid(context, value_index))
    return CONVERSION_INVALID;
  const w_seed_frontend_const_value *source =
      &context->frontend->const_values[value_index];
  const w_seed_frontend_type *expected =
      &context->frontend->types[expected_type_index];
  if (source->type_index != expected_type_index) return CONVERSION_INVALID;
  switch (source->kind) {
    case W_SEED_FRONTEND_CONST_BOOL: {
      if (expected->kind != W_SEED_FRONTEND_TYPE_BOOL) return CONVERSION_UNSUPPORTED;
      w_seed_constir_value converted;
      if (!w_seed_constir_value_bool(source->type_index, source->bool_value,
                                     &converted))
        return CONVERSION_INVALID;
      return append_value(context, &converted, arena_index);
    }
    case W_SEED_FRONTEND_CONST_INTEGER: {
      if (expected->kind != W_SEED_FRONTEND_TYPE_INTEGER) return CONVERSION_UNSUPPORTED;
      if (source->integer_byte_count > W_SEED_CONSTIR_INTEGER_BYTES ||
          source->integer_bit_width != expected->bit_width ||
          source->integer_signed != expected->is_signed)
        return CONVERSION_INVALID;
      uint8_t bytes[W_SEED_CONSTIR_INTEGER_BYTES] = {0u};
      (void)memcpy(bytes, source->integer_bytes, source->integer_byte_count);
      w_seed_constir_value converted;
      if (!w_seed_constir_value_integer(
              source->type_index, W_SEED_FRONTEND_TYPE_INTEGER,
              source->integer_signed, source->integer_bit_width, bytes,
              &converted))
        return CONVERSION_INVALID;
      return append_value(context, &converted, arena_index);
    }
    case W_SEED_FRONTEND_CONST_ENUM_CASE:
      if (expected->kind != W_SEED_FRONTEND_TYPE_ENUM &&
          expected->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET)
        return CONVERSION_UNSUPPORTED;
      return convert_enum_value(context, source, expected, expected_type_index,
                                arena_index);
    case W_SEED_FRONTEND_CONST_STATIC_LIST: {
      if (expected->kind != W_SEED_FRONTEND_TYPE_STATIC_LIST) return CONVERSION_UNSUPPORTED;
      if (source->element_count == 0u &&
          source->first_element != W_SEED_FRONTEND_NONE)
        return CONVERSION_INVALID;
      if (source->element_count > W_SEED_FRONTEND_MAX_STATIC_LIST_ELEMENTS ||
          (source->element_count != 0u &&
           !range_valid(source->first_element, source->element_count,
                        context->frontend_result->written.const_elements)))
        return source->element_count > W_SEED_FRONTEND_MAX_STATIC_LIST_ELEMENTS
                   ? CONVERSION_UNSUPPORTED
                   : CONVERSION_INVALID;
      const uint32_t element_type_index = expected->element_type;
      if (element_type_index == W_SEED_FRONTEND_NONE ||
          !type_index_valid(context, element_type_index))
        return CONVERSION_INVALID;
      const w_seed_frontend_type *element_type =
          &context->frontend->types[element_type_index];
      if (element_type->kind != W_SEED_FRONTEND_TYPE_ENUM &&
          element_type->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET)
        return CONVERSION_UNSUPPORTED;
      const size_t first_child = context->arena_count;
      for (uint32_t offset = 0u; offset < source->element_count; offset += 1u) {
        const w_seed_frontend_const_element *element =
            &context->frontend->const_elements[(size_t)source->first_element +
                                               offset];
        if (element->owner_value != value_index || element->ordinal != offset ||
            !span_valid(element->span) ||
            !frontend_const_value_basic_valid(context, element->value_index))
          return CONVERSION_INVALID;
        conversion_status child = convert_const_value(
            context, element->value_index, element_type_index, depth + 1u,
            arena_index);
        if (child != CONVERSION_OK) return child;
      }
      w_seed_constir_value converted;
      if (!w_seed_constir_value_static_list(
              source->type_index, element_type_index,
              source->element_count == 0u
                  ? NULL
                  : &context->input->conversion_values[first_child],
              source->element_count, &converted))
        return CONVERSION_INVALID;
      return append_value(context, &converted, arena_index);
    }
    case W_SEED_FRONTEND_CONST_STRING:
      if (expected->kind != W_SEED_FRONTEND_TYPE_STRING)
        return CONVERSION_UNSUPPORTED;
      if (source->byte_count > W_SEED_CONSTIR_MAX_STRING_BYTES)
        return CONVERSION_UNSUPPORTED;
      {
        const uint8_t *bytes = source->byte_count == 0u
                                   ? NULL
                                   : context->frontend->const_bytes +
                                         source->first_byte;
        w_seed_constir_value converted;
        if (!w_seed_constir_value_string(source->type_index, bytes,
                                          source->byte_count, &converted))
          return CONVERSION_INVALID;
        return append_value(context, &converted, arena_index);
      }
    case W_SEED_FRONTEND_CONST_INVALID:
      return CONVERSION_UNSUPPORTED;
  }
  return CONVERSION_UNSUPPORTED;
}

static bool parameter_relation_valid(const validation_context *context,
                                     const w_seed_frontend_generic_application *application,
                                     uint32_t offset,
                                     const w_seed_frontend_generic_parameter **out) {
  if (out != NULL) *out = NULL;
  if (context == NULL || application == NULL ||
      context->frontend == NULL || context->frontend_result == NULL ||
      application->head_struct >= context->frontend_result->written.structs ||
      context->frontend->structs == NULL)
    return false;
  const w_seed_frontend_struct *head =
      &context->frontend->structs[application->head_struct];
  if (!range_valid(head->first_generic_parameter,
                   head->generic_parameter_count,
                   context->frontend_result->written.generic_parameters) ||
      offset >= head->generic_parameter_count)
    return false;
  const w_seed_frontend_generic_parameter *parameter =
      &context->frontend->generic_parameters[(size_t)head->first_generic_parameter +
                                             offset];
  if (parameter->owner_kind != W_SEED_FRONTEND_DECL_STRUCT ||
      parameter->owner_index != application->head_struct ||
      parameter->module_index != application->module_index ||
      parameter->ordinal != offset || !span_valid(parameter->span) ||
      !text_valid(parameter->external_label) ||
      !text_valid(parameter->internal_name))
    return false;
  if (parameter->kind == W_SEED_FRONTEND_GENERIC_KIND_VALUE) {
    if (parameter->domain_kind != W_SEED_FRONTEND_GENERIC_DOMAIN_CONCRETE &&
        parameter->domain_kind != W_SEED_FRONTEND_GENERIC_DOMAIN_DEPENDENT)
      return false;
    if (parameter->domain_kind == W_SEED_FRONTEND_GENERIC_DOMAIN_CONCRETE &&
        (parameter->domain_type == W_SEED_FRONTEND_NONE ||
         !type_index_valid(context, parameter->domain_type) ||
         parameter->dependent_type_parameter_ordinal !=
             W_SEED_FRONTEND_NONE))
      return false;
    if (parameter->domain_kind == W_SEED_FRONTEND_GENERIC_DOMAIN_DEPENDENT &&
        (parameter->domain_type != W_SEED_FRONTEND_NONE ||
         parameter->dependent_type_parameter_ordinal ==
             W_SEED_FRONTEND_NONE))
      return false;
    if (parameter->refinement_kind ==
        W_SEED_FRONTEND_GENERIC_REFINEMENT_PREDICATE) {
      if (parameter->predicate_function_index == W_SEED_FRONTEND_NONE ||
          !span_valid(parameter->predicate_span) ||
          !span_valid(parameter->predicate_function_span) ||
          parameter->subject_kind != W_SEED_FRONTEND_GENERIC_SUBJECT_MEMBER)
        return false;
    } else if (parameter->refinement_kind ==
               W_SEED_FRONTEND_GENERIC_REFINEMENT_NONE) {
      if (parameter->predicate_function_index != W_SEED_FRONTEND_NONE ||
          parameter->subject_kind != W_SEED_FRONTEND_GENERIC_SUBJECT_NONE ||
          !span_valid(parameter->predicate_span) ||
          parameter->predicate_span.start_byte !=
              parameter->predicate_span.end_byte ||
          !span_valid(parameter->predicate_function_span) ||
          parameter->predicate_function_span.start_byte !=
              parameter->predicate_function_span.end_byte)
        return false;
    } else if (parameter->refinement_kind !=
                   W_SEED_FRONTEND_GENERIC_REFINEMENT_NONE &&
               parameter->refinement_kind !=
                   W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID) {
      return false;
    }
  } else if (parameter->kind == W_SEED_FRONTEND_GENERIC_KIND_TYPE) {
    if (parameter->domain_type != W_SEED_FRONTEND_NONE ||
        parameter->refinement_kind != W_SEED_FRONTEND_GENERIC_REFINEMENT_NONE)
      return false;
    if (parameter->predicate_function_index != W_SEED_FRONTEND_NONE ||
        parameter->subject_kind != W_SEED_FRONTEND_GENERIC_SUBJECT_NONE ||
        !span_valid(parameter->predicate_span) ||
        parameter->predicate_span.start_byte !=
            parameter->predicate_span.end_byte ||
        !span_valid(parameter->predicate_function_span) ||
        parameter->predicate_function_span.start_byte !=
            parameter->predicate_function_span.end_byte)
      return false;
  } else {
    return false;
  }
  if (out != NULL) *out = parameter;
  return true;
}

/* Resolve the concrete domain consumed by a value slot without changing any
 * frontend record.  A dependent domain is valid only when it names a prior
 * TYPE argument in this exact application and that argument was bound
 * immediately. */
static bool effective_domain_type_index(
    const validation_context *context,
    const w_seed_frontend_generic_application *application, uint32_t offset,
    uint32_t *out) {
  if (out != NULL) *out = W_SEED_FRONTEND_NONE;
  if (context == NULL || context->input == NULL || application == NULL ||
      out == NULL ||
      context->frontend == NULL || context->frontend_result == NULL ||
      context->frontend->generic_arguments == NULL ||
      context->frontend->generic_parameters == NULL ||
      context->frontend->structs == NULL ||
      application->head_struct >= context->frontend_result->written.structs ||
      !range_valid(application->first_argument, application->argument_count,
                   context->frontend_result->written.generic_arguments) ||
      offset >= application->argument_count)
    return false;
  const w_seed_frontend_generic_parameter *parameter = NULL;
  if (!parameter_relation_valid(context, application, offset, &parameter) ||
      parameter == NULL ||
      parameter->kind != W_SEED_FRONTEND_GENERIC_KIND_VALUE)
    return false;
  if (parameter->domain_kind == W_SEED_FRONTEND_GENERIC_DOMAIN_CONCRETE) {
    if (parameter->dependent_type_parameter_ordinal !=
            W_SEED_FRONTEND_NONE ||
        parameter->domain_type == W_SEED_FRONTEND_NONE ||
        !type_index_valid(context, parameter->domain_type))
      return false;
    *out = parameter->domain_type;
    return true;
  }
  if (parameter->domain_kind != W_SEED_FRONTEND_GENERIC_DOMAIN_DEPENDENT ||
      parameter->dependent_type_parameter_ordinal >= offset ||
      parameter->dependent_type_parameter_ordinal >= application->argument_count)
    return false;
  const uint32_t dependent_offset =
      parameter->dependent_type_parameter_ordinal;
  const w_seed_frontend_struct *head =
      &context->frontend->structs[application->head_struct];
  const w_seed_frontend_generic_parameter *dependent_parameter = NULL;
  if (!parameter_relation_valid(context, application, dependent_offset,
                                &dependent_parameter) ||
      dependent_parameter == NULL ||
      dependent_parameter->kind != W_SEED_FRONTEND_GENERIC_KIND_TYPE)
    return false;
  const w_seed_frontend_generic_argument *dependent_argument =
      &context->frontend->generic_arguments[(size_t)application->first_argument +
                                             dependent_offset];
  if (dependent_argument->owner_application !=
          context->input->application_index ||
      dependent_argument->source_ordinal != dependent_offset ||
      dependent_argument->parameter_ordinal != dependent_offset ||
      dependent_argument->parameter_index !=
          head->first_generic_parameter + dependent_offset ||
      dependent_argument->kind != W_SEED_FRONTEND_GENERIC_ARGUMENT_TYPE ||
      dependent_argument->binding_status !=
          W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE ||
      dependent_argument->type_index == W_SEED_FRONTEND_NONE ||
      !type_index_valid(context, dependent_argument->type_index) ||
      dependent_argument->const_value_index != W_SEED_FRONTEND_NONE)
    return false;
  *out = dependent_argument->type_index;
  return true;
}

static bool predicate_parameter_type_compatible(
    const validation_context *context, uint32_t left_index,
    uint32_t right_index, size_t depth);

static bool typed_const_expression_relation_valid(
    const validation_context *context,
    const w_seed_frontend_generic_application *application,
    uint32_t argument_ordinal,
    const w_seed_frontend_generic_argument *argument,
    const w_seed_frontend_typed_const_expression **typed_out) {
  if (typed_out != NULL) *typed_out = NULL;
  if (context == NULL || application == NULL || argument == NULL ||
      context->frontend == NULL || context->frontend_result == NULL ||
      context->frontend->typed_const_expressions == NULL ||
      argument->typed_const_expression_index == W_SEED_FRONTEND_NONE ||
      (size_t)argument->typed_const_expression_index >=
          context->frontend_result->written.typed_const_expressions)
    return false;
  const w_seed_frontend_typed_const_expression *typed =
      &context->frontend->typed_const_expressions[
          argument->typed_const_expression_index];
  if (typed->module_index != application->module_index ||
      typed->owner_application != context->input->application_index ||
      typed->argument_ordinal != argument_ordinal ||
      typed->expression_index == W_SEED_FRONTEND_NONE ||
      (size_t)typed->expression_index >=
          context->frontend_result->written.expressions ||
      typed->expected_type == W_SEED_FRONTEND_NONE ||
      typed->effective_type == W_SEED_FRONTEND_NONE ||
      !type_index_valid(context, typed->expected_type) ||
      !type_index_valid(context, typed->effective_type) ||
      !span_contains(argument->span, typed->span))
    return false;
  const w_seed_frontend_expression *expression =
      &context->frontend->expressions[typed->expression_index];
  if (!span_contains(typed->span, expression->span) ||
      expression->owner_function != W_SEED_FRONTEND_NONE ||
      !expression->supported ||
      !predicate_parameter_type_compatible(context, expression->inferred_type,
                                           typed->effective_type, 1u))
    return false;
  if (typed_out != NULL) *typed_out = typed;
  return true;
}

static bool application_relations_valid(
    const validation_context *context,
    const w_seed_frontend_generic_application **application_out,
    const w_seed_frontend_struct **head_out) {
  if (application_out != NULL) *application_out = NULL;
  if (head_out != NULL) *head_out = NULL;
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->generic_applications == NULL ||
      context->frontend->structs == NULL ||
      (size_t)context->input->application_index >=
          context->frontend_result->written.generic_applications)
    return false;
  const w_seed_frontend_generic_application *application =
      &context->frontend->generic_applications[context->input->application_index];
  if (application->module_index >= context->frontend_result->written.modules ||
      application->head_struct >= context->frontend_result->written.structs ||
      application->owner_type >= context->frontend_result->written.types ||
      !span_valid(application->span) || !span_valid(application->envelope_span) ||
      !text_valid(application->head_name) ||
      !range_valid(application->first_argument, application->argument_count,
                   context->frontend_result->written.generic_arguments) ||
      application->argument_count > W_SEED_FRONTEND_MAX_GENERIC_SLOTS)
    return false;
  const w_seed_frontend_struct *head =
      &context->frontend->structs[application->head_struct];
  if (head->module_index != application->module_index ||
      !text_valid(head->name) || !span_valid(head->span) ||
      head->name.length != application->head_name.length ||
      (head->name.length != 0u &&
       memcmp(head->name.data, application->head_name.data,
              head->name.length) != 0) ||
      context->frontend->types[application->owner_type].generic_application_index !=
          context->input->application_index ||
      !range_valid(head->first_generic_parameter,
                   head->generic_parameter_count,
                   context->frontend_result->written.generic_parameters))
    return false;
  if (application->binding_status < W_SEED_FRONTEND_GENERIC_BINDING_INVALID ||
      application->binding_status >
          W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE)
    return false;
  bool saw_typed_pending = false;
  bool declared_const_evaluation = false;
  for (uint32_t offset = 0u; offset < application->argument_count; offset += 1u) {
    const w_seed_frontend_generic_argument *argument =
        &context->frontend->generic_arguments[(size_t)application->first_argument + offset];
    const w_seed_frontend_generic_parameter *parameter = NULL;
    if (argument->owner_application != context->input->application_index ||
        argument->source_ordinal != offset || argument->module_index != application->module_index ||
        argument->parameter_ordinal != offset ||
        argument->parameter_index !=
            head->first_generic_parameter + offset ||
        !span_valid(argument->span) || !text_valid(argument->label) ||
        !parameter_relation_valid(context, application, offset, &parameter))
      return false;
    const bool value_kind = parameter->kind == W_SEED_FRONTEND_GENERIC_KIND_VALUE;
    if (value_kind && parameter->refinement_kind ==
                         W_SEED_FRONTEND_GENERIC_REFINEMENT_PREDICATE)
      declared_const_evaluation = true;
    if ((value_kind && argument->kind != W_SEED_FRONTEND_GENERIC_ARGUMENT_VALUE) ||
        (!value_kind && argument->kind != W_SEED_FRONTEND_GENERIC_ARGUMENT_TYPE) ||
        argument->binding_status < W_SEED_FRONTEND_GENERIC_BINDING_INVALID ||
        argument->binding_status > W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE)
      return false;
    if (value_kind) {
      if (argument->binding_status ==
          W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED) {
        if (argument->type_index != W_SEED_FRONTEND_NONE ||
            argument->const_value_index != W_SEED_FRONTEND_NONE ||
            argument->typed_const_expression_index != W_SEED_FRONTEND_NONE)
          return false;
        continue;
      }
      if (argument->binding_status ==
          W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST) {
        saw_typed_pending = true;
        if (argument->type_index != W_SEED_FRONTEND_NONE ||
            argument->const_value_index != W_SEED_FRONTEND_NONE ||
            !typed_const_expression_relation_valid(context, application,
                                                   offset, argument, NULL))
          return false;
        continue;
      }
      uint32_t effective_domain = W_SEED_FRONTEND_NONE;
      if (argument->type_index != W_SEED_FRONTEND_NONE ||
          !effective_domain_type_index(context, application, offset,
                                       &effective_domain) ||
          argument->const_value_index == W_SEED_FRONTEND_NONE ||
          !frontend_const_value_relation_valid(context,
                                               argument->const_value_index, 1u) ||
          context->frontend->const_values[argument->const_value_index]
                  .type_index != effective_domain ||
          !span_contains(argument->span,
                         context->frontend
                             ->const_values[argument->const_value_index]
                             .span))
        return false;
      if (argument->typed_const_expression_index != W_SEED_FRONTEND_NONE)
        return false;
    } else if (argument->type_index == W_SEED_FRONTEND_NONE ||
               !type_index_valid(context, argument->type_index) ||
               argument->const_value_index != W_SEED_FRONTEND_NONE ||
               argument->typed_const_expression_index != W_SEED_FRONTEND_NONE) {
      return false;
    }
  }
  if (application->binding_status ==
          W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE &&
      saw_typed_pending)
    return false;
  if (application->binding_status ==
          W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST &&
      !saw_typed_pending)
    return false;
  if ((saw_typed_pending || declared_const_evaluation) &&
      !application->requires_const_evaluation)
    return false;
  if (application_out != NULL) *application_out = application;
  if (head_out != NULL) *head_out = head;
  return true;
}

static conversion_status conversion_value_count(
    const validation_context *context, uint32_t value_index,
    uint32_t expected_type_index, size_t *count) {
  if (context == NULL || count == NULL ||
      !type_index_valid(context, expected_type_index) ||
      !frontend_const_value_basic_valid(context, value_index))
    return CONVERSION_INVALID;
  const w_seed_frontend_const_value *source =
      &context->frontend->const_values[value_index];
  const w_seed_frontend_type *expected =
      &context->frontend->types[expected_type_index];
  if (source->type_index != expected_type_index) return CONVERSION_INVALID;
  if (source->kind == W_SEED_FRONTEND_CONST_INVALID)
    return CONVERSION_UNSUPPORTED;
  if (source->kind == W_SEED_FRONTEND_CONST_STRING) {
    if (expected->kind != W_SEED_FRONTEND_TYPE_STRING)
      return CONVERSION_UNSUPPORTED;
    if (source->byte_count > W_SEED_CONSTIR_MAX_STRING_BYTES)
      return CONVERSION_UNSUPPORTED;
    *count = 1u;
    return CONVERSION_OK;
  }
  if (source->kind == W_SEED_FRONTEND_CONST_BOOL)
    return expected->kind == W_SEED_FRONTEND_TYPE_BOOL ? (*count = 1u,
                                                          CONVERSION_OK)
                                                       : CONVERSION_UNSUPPORTED;
  if (source->kind == W_SEED_FRONTEND_CONST_INTEGER) {
    if (expected->kind != W_SEED_FRONTEND_TYPE_INTEGER)
      return CONVERSION_UNSUPPORTED;
    if (source->integer_byte_count > W_SEED_CONSTIR_INTEGER_BYTES ||
        source->integer_bit_width != expected->bit_width ||
        source->integer_signed != expected->is_signed)
      return CONVERSION_INVALID;
    *count = 1u;
    return CONVERSION_OK;
  }
  if (source->kind == W_SEED_FRONTEND_CONST_ENUM_CASE) {
    if (expected->kind != W_SEED_FRONTEND_TYPE_ENUM &&
        expected->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET)
      return CONVERSION_UNSUPPORTED;
    if (!enum_case_valid(context, source->enum_base_index,
                         source->enum_case_index, true))
      return CONVERSION_UNSUPPORTED;
    if (!enum_case_member_of_type(context, expected, expected_type_index,
                                  source->enum_base_index,
                                  source->enum_case_index))
      return CONVERSION_INVALID;
    *count = 1u;
    return CONVERSION_OK;
  }
  if (source->kind != W_SEED_FRONTEND_CONST_STATIC_LIST ||
      expected->kind != W_SEED_FRONTEND_TYPE_STATIC_LIST)
    return CONVERSION_UNSUPPORTED;
  if (source->element_count > W_SEED_FRONTEND_MAX_STATIC_LIST_ELEMENTS)
    return CONVERSION_UNSUPPORTED;
  const uint32_t element_type_index = expected->element_type;
  if (element_type_index == W_SEED_FRONTEND_NONE ||
      !type_index_valid(context, element_type_index))
    return CONVERSION_INVALID;
  const w_seed_frontend_type *element_type =
      &context->frontend->types[element_type_index];
  if (element_type->kind != W_SEED_FRONTEND_TYPE_ENUM &&
      element_type->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET)
    return CONVERSION_UNSUPPORTED;
  if (source->element_count != 0u &&
      !range_valid(source->first_element, source->element_count,
                   context->frontend_result->written.const_elements))
    return CONVERSION_INVALID;
  if (source->element_count == 0u &&
      source->first_element != W_SEED_FRONTEND_NONE)
    return CONVERSION_INVALID;
  *count = source->element_count + 1u;
  for (uint32_t offset = 0u; offset < source->element_count; offset += 1u) {
    const w_seed_frontend_const_element *element =
        &context->frontend->const_elements[(size_t)source->first_element +
                                           offset];
    if (element->owner_value != value_index || element->ordinal != offset ||
        !span_valid(element->span) ||
        !frontend_const_value_basic_valid(context, element->value_index))
      return CONVERSION_INVALID;
    const w_seed_frontend_const_value *child =
        &context->frontend->const_values[element->value_index];
    if (child->kind != W_SEED_FRONTEND_CONST_ENUM_CASE ||
        child->type_index != element_type_index ||
        !enum_case_valid(context, child->enum_base_index,
                         child->enum_case_index, true))
      return child->kind == W_SEED_FRONTEND_CONST_STATIC_LIST
                 ? CONVERSION_UNSUPPORTED
                 : CONVERSION_INVALID;
    if (!enum_case_member_of_type(context, element_type, element_type_index,
                                  child->enum_base_index,
                                  child->enum_case_index))
      return CONVERSION_INVALID;
  }
  return CONVERSION_OK;
}

static bool predicate_candidate_function_valid(
    const validation_context *context, predicate_candidate *candidate,
    bool *duplicate) {
  if (duplicate != NULL) *duplicate = false;
  if (context == NULL || candidate == NULL || candidate->parameter == NULL ||
      !frontend_function_index_valid(context,
                                     candidate->parameter->predicate_function_index))
    return false;
  candidate->predicate_function_index =
      candidate->parameter->predicate_function_index;
  size_t constir_index = SIZE_MAX;
  bool duplicate_mapping = false;
  const w_seed_constir_function *function = constir_function_for_frontend(
      context->program, candidate->predicate_function_index, &constir_index,
      &duplicate_mapping);
  if (duplicate != NULL) *duplicate = duplicate_mapping;
  candidate->predicate_constir_index =
      constir_index == SIZE_MAX ? W_SEED_CONSTIR_NONE : (uint32_t)constir_index;
  candidate->function = function;
  return true;
}

static bool predicate_parameter_type_compatible(
    const validation_context *context, uint32_t left_index,
    uint32_t right_index, size_t depth) {
  if (context == NULL || depth == 0u ||
      depth > W_SEED_GENERIC_VALIDATION_MAX_DEPTH ||
      !type_index_valid(context, left_index) ||
      !type_index_valid(context, right_index))
    return false;
  const w_seed_frontend_type *left = &context->frontend->types[left_index];
  const w_seed_frontend_type *right = &context->frontend->types[right_index];
  if ((left->kind == W_SEED_FRONTEND_TYPE_ENUM ||
       left->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET) &&
      (right->kind == W_SEED_FRONTEND_TYPE_ENUM ||
       right->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET))
    return left->enum_base_index == right->enum_base_index;
  if (left->kind != right->kind) return false;
  if (left->kind == W_SEED_FRONTEND_TYPE_INTEGER)
    return left->is_signed == right->is_signed &&
           left->bit_width == right->bit_width;
  if (left->kind == W_SEED_FRONTEND_TYPE_STATIC_LIST)
    return predicate_parameter_type_compatible(context, left->element_type,
                                               right->element_type,
                                               depth + 1u);
  return true;
}

static bool predicate_frontend_signature_valid(
    const validation_context *context, const predicate_candidate *candidate) {
  if (context == NULL || candidate == NULL || candidate->parameter == NULL ||
      !frontend_function_index_valid(context,
                                     candidate->predicate_function_index))
    return false;
  const w_seed_frontend_function *function =
      &context->frontend->functions[candidate->predicate_function_index];
  if (function->module_index != candidate->module_index ||
      !text_valid(function->name) || !span_valid(function->span) ||
      !span_valid(function->body_span) ||
      !span_contains(function->span, function->body_span) ||
      function->span.start_byte !=
          candidate->parameter->predicate_function_span.start_byte ||
      function->span.end_byte !=
          candidate->parameter->predicate_function_span.end_byte ||
      !function->is_const || function->parameter_count != 1u ||
      function->return_type == W_SEED_FRONTEND_NONE ||
      !type_index_valid(context, function->return_type) ||
      context->frontend->types[function->return_type].kind !=
          W_SEED_FRONTEND_TYPE_BOOL ||
      !range_valid(function->first_parameter, function->parameter_count,
                   context->frontend_result->written.parameters))
    return false;
  const w_seed_frontend_parameter *parameter =
      &context->frontend->parameters[function->first_parameter];
  return parameter->module_index == function->module_index &&
         parameter->owner_function == candidate->predicate_function_index &&
         text_valid(parameter->name) && text_valid(parameter->label) &&
         span_contains(function->span, parameter->span) &&
         predicate_parameter_type_compatible(context, parameter->type_index,
                                              candidate->effective_domain_type_index,
                                              1u);
}

static bool set_rejection(
    const validation_context *context,
    const w_seed_frontend_generic_application *application,
    const predicate_candidate *candidate,
    w_seed_generic_validation_result *result) {
  if (context == NULL || context->input == NULL ||
      context->input->evidence_bytes == NULL ||
      context->input->evidence_byte_capacity <
          W_SEED_GENERIC_VALIDATION_FALLBACK_BYTES)
    return false;
  (void)memcpy(context->input->evidence_bytes, FALLBACK_BYTES,
               W_SEED_GENERIC_VALIDATION_FALLBACK_BYTES);
  const w_seed_frontend_text fallback = {
      (const char *)context->input->evidence_bytes,
      W_SEED_GENERIC_VALIDATION_FALLBACK_BYTES};
  result->rejection.application_index = context->input->application_index;
  result->rejection.head_struct_index = application->head_struct;
  result->rejection.head_name = application->head_name;
  result->rejection.generic_argument_index = candidate->argument_index;
  result->rejection.argument_const_value_index =
      candidate->argument_const_value_index;
  result->rejection.typed_const_expression_index =
      candidate->typed_const_expression_index;
  result->rejection.argument_span = candidate->argument->span;
  result->rejection.predicate_function_index =
      candidate->predicate_function_index;
  result->rejection.predicate_span = candidate->parameter->predicate_span;
  result->rejection.predicate_function_span =
      candidate->parameter->predicate_function_span;
  result->rejection.failure = fallback;
  result->rejection.rejection_trace[0] = fallback;
  result->rejection.rejection_trace_count = 1u;
  return true;
}

static void fingerprint_bytes(fingerprint_builder *builder,
                              const uint8_t *bytes, size_t length);

static void fingerprint_u8(fingerprint_builder *builder, uint8_t value) {
  if (builder == NULL || builder->unsupported || builder->invalid) return;
  fingerprint_bytes(builder, &value, 1u);
}

static void fingerprint_u16(fingerprint_builder *builder, uint16_t value) {
  uint8_t bytes[2];
  if (builder == NULL || builder->unsupported || builder->invalid) return;
  bytes[0] = (uint8_t)(value >> 8);
  bytes[1] = (uint8_t)value;
  fingerprint_bytes(builder, bytes, sizeof(bytes));
}

static void fingerprint_u32(fingerprint_builder *builder, uint32_t value) {
  uint8_t bytes[4];
  if (builder == NULL || builder->unsupported || builder->invalid) return;
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
  fingerprint_bytes(builder, bytes, sizeof(bytes));
}

static void fingerprint_bytes(fingerprint_builder *builder,
                              const uint8_t *bytes, size_t length) {
  if (builder == NULL || builder->unsupported || builder->invalid ||
      (length != 0u && bytes == NULL)) {
    if (builder != NULL && length != 0u && bytes == NULL)
      builder->invalid = true;
    return;
  }
  w_seed_sha256_update(&builder->sha, bytes, length);
}

static fingerprint_encode_status fingerprint_text(
    fingerprint_builder *builder, w_seed_frontend_text text) {
  if (builder == NULL || !text_valid(text) || text.length > (size_t)UINT32_MAX)
    return FINGERPRINT_INVALID;
  fingerprint_u32(builder, (uint32_t)text.length);
  fingerprint_bytes(builder, (const uint8_t *)text.data, text.length);
  return FINGERPRINT_ENCODED;
}

static void fingerprint_mark_unsupported(fingerprint_builder *builder) {
  if (builder != NULL && !builder->invalid) builder->unsupported = true;
}

static fingerprint_encode_status fingerprint_module_text(
    const validation_context *context, uint32_t module_index,
    fingerprint_builder *builder) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->modules == NULL ||
      (size_t)module_index >= context->frontend_result->written.modules)
    return FINGERPRINT_INVALID;
  const w_seed_frontend_text module_id =
      context->frontend->modules[module_index].module_id;
  if (module_id.length == 0u || !text_valid(module_id))
    return FINGERPRINT_INVALID;
  return fingerprint_text(builder, module_id);
}

static fingerprint_encode_status fingerprint_enum_identity(
    const validation_context *context, uint32_t module_index, uint32_t enum_index,
    fingerprint_builder *builder) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || context->frontend->enums == NULL ||
      (size_t)enum_index >= context->frontend_result->written.enums)
    return FINGERPRINT_INVALID;
  const w_seed_frontend_enum *enumeration =
      &context->frontend->enums[enum_index];
  if (enumeration->module_index != module_index) {
    fingerprint_mark_unsupported(builder);
    return FINGERPRINT_UNSUPPORTED;
  }
  if (enumeration->name.length == 0u || !text_valid(enumeration->name) ||
      !span_valid(enumeration->span) ||
      !range_valid(enumeration->first_case, enumeration->case_count,
                   context->frontend_result->written.enum_cases))
    return FINGERPRINT_INVALID;
  for (uint32_t offset = 0u; offset < enumeration->case_count; offset += 1u) {
    if (!enum_case_valid(context, enum_index,
                         enumeration->first_case + offset, false))
      return FINGERPRINT_INVALID;
  }
  fingerprint_encode_status status =
      fingerprint_module_text(context, module_index, builder);
  if (status != FINGERPRINT_ENCODED) return status;
  return fingerprint_text(builder, enumeration->name);
}

static fingerprint_encode_status fingerprint_enum_case_name(
    const validation_context *context, uint32_t enum_base_index,
    uint32_t enum_case_index, fingerprint_builder *builder) {
  if (!enum_case_valid(context, enum_base_index, enum_case_index, false))
    return FINGERPRINT_INVALID;
  if (context->frontend == NULL || context->frontend->enum_cases == NULL)
    return FINGERPRINT_INVALID;
  return fingerprint_text(builder,
                          context->frontend->enum_cases[enum_case_index].name);
}

typedef enum {
  FINGERPRINT_NOMINAL_STRUCT = 0,
  FINGERPRINT_NOMINAL_TYPE,
  FINGERPRINT_NOMINAL_UNSUPPORTED,
} fingerprint_nominal_kind;

static fingerprint_encode_status local_nominal_resolution(
    const validation_context *context, uint32_t module_index,
    w_seed_frontend_text name, fingerprint_nominal_kind *kind_out) {
  if (kind_out != NULL) *kind_out = FINGERPRINT_NOMINAL_UNSUPPORTED;
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || !text_valid(name))
    return FINGERPRINT_INVALID;
  size_t struct_matches = 0u;
  size_t type_matches = 0u;
  size_t alias_matches = 0u;
  bool struct_generic = false;
  bool type_generic = false;
  for (size_t index = 0u; index < context->frontend_result->written.structs;
       index += 1u) {
    const w_seed_frontend_struct *item = &context->frontend->structs[index];
    if (item->module_index != module_index ||
        !text_equal(item->name, name))
      continue;
    if (item->name.length == 0u || !text_valid(item->name) ||
        !span_valid(item->span) ||
        !range_valid(item->first_generic_parameter,
                     item->generic_parameter_count,
                     context->frontend_result->written.generic_parameters))
      return FINGERPRINT_INVALID;
    struct_matches += 1u;
    struct_generic = struct_generic || item->generic_parameter_count != 0u;
  }
  for (size_t index = 0u;
       index < context->frontend_result->written.type_declarations;
       index += 1u) {
    const w_seed_frontend_type_declaration *item =
        &context->frontend->type_declarations[index];
    if (item->module_index != module_index ||
        !text_equal(item->name, name))
      continue;
    if (item->name.length == 0u || !text_valid(item->name) ||
        !span_valid(item->span) ||
        !type_index_valid(context, item->type_index))
      return FINGERPRINT_INVALID;
    type_matches += 1u;
    type_generic =
        type_generic ||
        context->frontend->types[item->type_index].generic_application_index !=
            W_SEED_FRONTEND_NONE;
  }
  for (size_t index = 0u; index < context->frontend_result->written.aliases;
       index += 1u) {
    const w_seed_frontend_alias *item = &context->frontend->aliases[index];
    if (item->module_index != module_index ||
        !text_equal(item->name, name))
      continue;
    if (item->name.length == 0u || !text_valid(item->name) ||
        !span_valid(item->span) || !type_index_valid(context, item->type_index))
      return FINGERPRINT_INVALID;
    alias_matches += 1u;
  }
  if (alias_matches != 0u || struct_matches + type_matches != 1u)
    return FINGERPRINT_UNSUPPORTED;
  if (struct_matches == 1u && !struct_generic) {
    if (kind_out != NULL) *kind_out = FINGERPRINT_NOMINAL_STRUCT;
    return FINGERPRINT_ENCODED;
  }
  if (type_matches == 1u && !type_generic) {
    if (kind_out != NULL) *kind_out = FINGERPRINT_NOMINAL_TYPE;
    return FINGERPRINT_ENCODED;
  }
  return FINGERPRINT_UNSUPPORTED;
}

static fingerprint_encode_status fingerprint_type(
    const validation_context *context, uint32_t module_index,
    uint32_t type_index, fingerprint_builder *builder, size_t depth);

static fingerprint_encode_status fingerprint_type(
    const validation_context *context, uint32_t module_index,
    uint32_t type_index, fingerprint_builder *builder, size_t depth) {
  if (context == NULL || context->frontend == NULL ||
      context->frontend_result == NULL || builder == NULL || depth == 0u ||
      depth > W_SEED_GENERIC_VALIDATION_MAX_DEPTH ||
      !type_index_valid(context, type_index))
    return FINGERPRINT_INVALID;
  const w_seed_frontend_type *type = &context->frontend->types[type_index];
  fingerprint_u8(builder, 0x74u);
  switch (type->kind) {
    case W_SEED_FRONTEND_TYPE_UNIT:
      fingerprint_u8(builder, 1u);
      return FINGERPRINT_ENCODED;
    case W_SEED_FRONTEND_TYPE_BOOL:
      fingerprint_u8(builder, 2u);
      return FINGERPRINT_ENCODED;
    case W_SEED_FRONTEND_TYPE_STRING:
      fingerprint_u8(builder, 3u);
      return FINGERPRINT_ENCODED;
    case W_SEED_FRONTEND_TYPE_BYTES:
      fingerprint_u8(builder, 4u);
      return FINGERPRINT_ENCODED;
    case W_SEED_FRONTEND_TYPE_INTEGER:
      if (type->bit_width == 0u) return FINGERPRINT_INVALID;
      fingerprint_u8(builder, 5u);
      fingerprint_u8(builder, type->is_signed ? 1u : 0u);
      fingerprint_u16(builder, type->bit_width);
      return FINGERPRINT_ENCODED;
    case W_SEED_FRONTEND_TYPE_FLOAT:
      if (type->bit_width == 0u) return FINGERPRINT_INVALID;
      fingerprint_u8(builder, 6u);
      fingerprint_u8(builder, type->is_signed ? 1u : 0u);
      fingerprint_u16(builder, type->bit_width);
      return FINGERPRINT_ENCODED;
    case W_SEED_FRONTEND_TYPE_OPTION:
    case W_SEED_FRONTEND_TYPE_STATIC_LIST:
    case W_SEED_FRONTEND_TYPE_RANGE: {
      if (type->element_type == W_SEED_FRONTEND_NONE)
        return FINGERPRINT_INVALID;
      const uint8_t kind =
          type->kind == W_SEED_FRONTEND_TYPE_OPTION
              ? 7u
              : type->kind == W_SEED_FRONTEND_TYPE_STATIC_LIST ? 11u : 12u;
      fingerprint_u8(builder, kind);
      return fingerprint_type(context, module_index, type->element_type,
                              builder, depth + 1u);
    }
    case W_SEED_FRONTEND_TYPE_NOMINAL: {
      if (type->generic_application_index != W_SEED_FRONTEND_NONE) {
        if ((size_t)type->generic_application_index >=
            context->frontend_result->written.generic_applications)
          return FINGERPRINT_INVALID;
        fingerprint_mark_unsupported(builder);
        return FINGERPRINT_UNSUPPORTED;
      }
      if (type->nominal_name.length == 0u || !text_valid(type->nominal_name))
        return FINGERPRINT_INVALID;
      fingerprint_nominal_kind nominal_kind = FINGERPRINT_NOMINAL_UNSUPPORTED;
      fingerprint_encode_status nominal_status = local_nominal_resolution(
          context, module_index, type->nominal_name, &nominal_kind);
      if (nominal_status != FINGERPRINT_ENCODED) {
        if (nominal_status == FINGERPRINT_UNSUPPORTED)
          fingerprint_mark_unsupported(builder);
        return nominal_status;
      }
      if (nominal_kind == FINGERPRINT_NOMINAL_UNSUPPORTED) {
        fingerprint_mark_unsupported(builder);
        return FINGERPRINT_UNSUPPORTED;
      }
      fingerprint_u8(builder, 8u);
      fingerprint_encode_status status =
          fingerprint_module_text(context, module_index, builder);
      if (status != FINGERPRINT_ENCODED) return status;
      return fingerprint_text(builder, type->nominal_name);
    }
    case W_SEED_FRONTEND_TYPE_ENUM:
      fingerprint_u8(builder, 9u);
      return fingerprint_enum_identity(context, module_index,
                                       type->enum_base_index, builder);
    case W_SEED_FRONTEND_TYPE_ENUM_SUBSET: {
      if (type->enum_base_index == W_SEED_FRONTEND_NONE ||
          context->frontend->enums == NULL ||
          (size_t)type->enum_base_index >=
              context->frontend_result->written.enums)
        return FINGERPRINT_INVALID;
      const w_seed_frontend_enum *enumeration =
          &context->frontend->enums[type->enum_base_index];
      if ((type->subset_member_count == 0u &&
           type->first_subset_member != W_SEED_FRONTEND_NONE) ||
          (type->subset_member_count != 0u &&
           (type->first_subset_member == W_SEED_FRONTEND_NONE ||
            !range_valid(type->first_subset_member, type->subset_member_count,
                         context->frontend_result->written.enum_subset_members))))
        return FINGERPRINT_INVALID;
      if (enumeration->module_index != module_index) {
        fingerprint_mark_unsupported(builder);
        return FINGERPRINT_UNSUPPORTED;
      }
      if (enumeration->name.length == 0u || !text_valid(enumeration->name) ||
          !span_valid(enumeration->span) ||
          !range_valid(enumeration->first_case, enumeration->case_count,
                       context->frontend_result->written.enum_cases))
        return FINGERPRINT_INVALID;
      fingerprint_u8(builder, 10u);
      fingerprint_encode_status status =
          fingerprint_module_text(context, module_index, builder);
      if (status != FINGERPRINT_ENCODED) return status;
      status = fingerprint_text(builder, enumeration->name);
      if (status != FINGERPRINT_ENCODED) return status;
      fingerprint_u32(builder, type->subset_member_count);
      uint32_t emitted_members = 0u;
      for (uint32_t case_offset = 0u; case_offset < enumeration->case_count;
           case_offset += 1u) {
        const uint32_t case_index = enumeration->first_case + case_offset;
        uint32_t matches = 0u;
        for (uint32_t member_offset = 0u;
             member_offset < type->subset_member_count; member_offset += 1u) {
          const w_seed_frontend_enum_subset_member *member =
              &context->frontend->enum_subset_members[
                  (size_t)type->first_subset_member + member_offset];
          if (member->owner_type != type_index ||
              member->enum_base_index != type->enum_base_index ||
              !enum_case_valid(context, member->enum_base_index,
                               member->enum_case_index, false) ||
              !span_valid(member->source_span))
            return FINGERPRINT_INVALID;
          if (member->enum_case_index == case_index) matches += 1u;
        }
        if (matches > 1u) return FINGERPRINT_INVALID;
        if (matches == 1u) {
          if (fingerprint_enum_case_name(context, type->enum_base_index,
                                         case_index, builder) !=
              FINGERPRINT_ENCODED)
            return FINGERPRINT_INVALID;
          emitted_members += 1u;
        }
      }
      if (emitted_members != type->subset_member_count)
        return FINGERPRINT_INVALID;
      return FINGERPRINT_ENCODED;
    }
    case W_SEED_FRONTEND_TYPE_FUNCTION:
    case W_SEED_FRONTEND_TYPE_UNKNOWN:
      fingerprint_mark_unsupported(builder);
      return FINGERPRINT_UNSUPPORTED;
    case W_SEED_FRONTEND_TYPE_INVALID:
      return FINGERPRINT_INVALID;
  }
  return FINGERPRINT_INVALID;
}

/* Encode the normalized ConstIR value with the same semantic framing as an
 * immediate frontend ConstValue.  This is the bridge that makes `42` and
 * `(6 * 7)` share one fingerprint preimage. */
static fingerprint_encode_status fingerprint_constir_value(
    const validation_context *context, uint32_t module_index,
    const w_seed_constir_value *value, uint32_t expected_type_index,
    fingerprint_builder *builder, size_t depth) {
  if (context == NULL || value == NULL || builder == NULL || depth == 0u ||
      depth > W_SEED_GENERIC_VALIDATION_MAX_DEPTH ||
      !type_index_valid(context, expected_type_index))
    return FINGERPRINT_INVALID;
  uint8_t kind = 0u;
  switch (value->kind) {
    case W_SEED_CONSTIR_VALUE_BOOL:
      kind = 1u;
      break;
    case W_SEED_CONSTIR_VALUE_INTEGER:
      kind = 2u;
      break;
    case W_SEED_CONSTIR_VALUE_STRING:
      kind = 3u;
      break;
    case W_SEED_CONSTIR_VALUE_ENUM:
      kind = 4u;
      break;
    case W_SEED_CONSTIR_VALUE_STATIC_LIST:
      kind = 5u;
      break;
    default:
      return FINGERPRINT_INVALID;
  }
  fingerprint_u8(builder, 0x76u);
  fingerprint_u8(builder, kind);
  fingerprint_encode_status status = fingerprint_type(
      context, module_index, expected_type_index, builder, depth);
  if (status != FINGERPRINT_ENCODED) return status;
  const w_seed_frontend_type *expected =
      &context->frontend->types[expected_type_index];
  switch (value->kind) {
    case W_SEED_CONSTIR_VALUE_BOOL:
      if (expected->kind != W_SEED_FRONTEND_TYPE_BOOL ||
          value->type_kind != W_SEED_FRONTEND_TYPE_BOOL)
        return FINGERPRINT_INVALID;
      fingerprint_u8(builder, value->bool_value ? 1u : 0u);
      return FINGERPRINT_ENCODED;
    case W_SEED_CONSTIR_VALUE_INTEGER: {
      if (expected->kind != W_SEED_FRONTEND_TYPE_INTEGER ||
          value->type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
          value->type_is_signed != expected->is_signed ||
          value->type_bit_width != expected->bit_width ||
          value->type_bit_width == 0u || value->type_bit_width > 128u)
        return FINGERPRINT_INVALID;
      const uint8_t byte_count =
          (uint8_t)((value->type_bit_width + 7u) / 8u);
      fingerprint_u8(builder, value->type_is_signed ? 1u : 0u);
      fingerprint_u16(builder, value->type_bit_width);
      fingerprint_u8(builder, byte_count);
      fingerprint_bytes(builder, value->integer_value, byte_count);
      return FINGERPRINT_ENCODED;
    }
    case W_SEED_CONSTIR_VALUE_STRING:
      if (expected->kind != W_SEED_FRONTEND_TYPE_STRING ||
          value->string_count > W_SEED_CONSTIR_MAX_STRING_BYTES ||
          (value->string_count != 0u && value->string_bytes == NULL))
        return FINGERPRINT_INVALID;
      fingerprint_u32(builder, (uint32_t)value->string_count);
      fingerprint_bytes(builder, value->string_bytes, value->string_count);
      return FINGERPRINT_ENCODED;
    case W_SEED_CONSTIR_VALUE_ENUM:
      if ((expected->kind != W_SEED_FRONTEND_TYPE_ENUM &&
           expected->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET) ||
          !enum_case_member_of_type(context, expected, expected_type_index,
                                    value->enum_base_index,
                                    value->enum_case_index))
        return FINGERPRINT_INVALID;
      return fingerprint_enum_case_name(context, value->enum_base_index,
                                        value->enum_case_index, builder);
    case W_SEED_CONSTIR_VALUE_STATIC_LIST:
      if (expected->kind != W_SEED_FRONTEND_TYPE_STATIC_LIST ||
          expected->element_type == W_SEED_FRONTEND_NONE ||
          value->element_count > W_SEED_FRONTEND_MAX_STATIC_LIST_ELEMENTS ||
          (value->element_count != 0u && value->elements == NULL))
        return FINGERPRINT_INVALID;
      fingerprint_u32(builder, (uint32_t)value->element_count);
      for (size_t index = 0u; index < value->element_count; index += 1u) {
        status = fingerprint_constir_value(
            context, module_index, &value->elements[index],
            expected->element_type, builder, depth + 1u);
        if (status != FINGERPRINT_ENCODED) return status;
      }
      return FINGERPRINT_ENCODED;
    default:
      return FINGERPRINT_INVALID;
  }
}

static const predicate_candidate *fingerprint_candidate_for_argument(
    const predicate_candidate *candidates, size_t candidate_count,
    uint32_t argument_index) {
  if (candidates == NULL) return NULL;
  for (size_t index = 0u; index < candidate_count; index += 1u)
    if (candidates[index].argument_index == argument_index) return &candidates[index];
  return NULL;
}

static fingerprint_encode_status fingerprint_application(
    const validation_context *context,
    const w_seed_frontend_generic_application *application,
    const w_seed_frontend_struct *head, const predicate_candidate *candidates,
    size_t candidate_count, const uint32_t *value_indices,
    fingerprint_builder *builder) {
  if (context == NULL || application == NULL || head == NULL || builder == NULL ||
      context->frontend == NULL || context->frontend_result == NULL ||
      context->frontend->generic_arguments == NULL ||
      context->frontend->generic_parameters == NULL ||
      application->module_index >= context->frontend_result->written.modules ||
      application->argument_count != head->generic_parameter_count)
    return FINGERPRINT_INVALID;
  const w_seed_frontend_module *module =
      &context->frontend->modules[application->module_index];
  if (module->module_id.length == 0u || !text_valid(module->module_id) ||
      application->head_name.length == 0u || !text_valid(application->head_name))
    return FINGERPRINT_INVALID;
  fingerprint_bytes(builder, FINGERPRINT_PREFIX, sizeof(FINGERPRINT_PREFIX) - 1u);
  fingerprint_u8(builder, 0x47u);
  fingerprint_encode_status status =
      fingerprint_text(builder, module->module_id);
  if (status != FINGERPRINT_ENCODED) return status;
  status = fingerprint_text(builder, application->head_name);
  if (status != FINGERPRINT_ENCODED) return status;
  fingerprint_u32(builder, application->argument_count);
  for (uint32_t offset = 0u; offset < application->argument_count; offset += 1u) {
    const w_seed_frontend_generic_argument *argument =
        &context->frontend->generic_arguments[
            (size_t)application->first_argument + offset];
    const w_seed_frontend_generic_parameter *parameter =
        &context->frontend->generic_parameters[
            (size_t)head->first_generic_parameter + offset];
    fingerprint_u8(builder, 0x41u);
    fingerprint_u32(builder, offset);
    if (parameter->kind == W_SEED_FRONTEND_GENERIC_KIND_TYPE) {
      fingerprint_u8(builder, 1u);
      fingerprint_u8(builder, 0x54u);
      status = fingerprint_type(context, application->module_index,
                                argument->type_index, builder, 1u);
      if (status == FINGERPRINT_INVALID) return status;
      continue;
    }
    if (parameter->kind != W_SEED_FRONTEND_GENERIC_KIND_VALUE)
      return FINGERPRINT_INVALID;
    fingerprint_u8(builder, 2u);
    fingerprint_u8(builder, 0x56u);
    uint32_t effective_domain_type = W_SEED_FRONTEND_NONE;
    if (!effective_domain_type_index(context, application, offset,
                                     &effective_domain_type))
      return FINGERPRINT_INVALID;
    status = fingerprint_type(context, application->module_index,
                              effective_domain_type, builder, 1u);
    if (status == FINGERPRINT_INVALID) return status;
    if (value_indices == NULL || value_indices[offset] == W_SEED_FRONTEND_NONE ||
        (size_t)value_indices[offset] >= context->arena_count)
      return FINGERPRINT_INVALID;
    status = fingerprint_constir_value(
        context, application->module_index,
        &context->input->conversion_values[value_indices[offset]],
        effective_domain_type, builder, 1u);
    if (status == FINGERPRINT_INVALID) return status;
    if (parameter->refinement_kind == W_SEED_FRONTEND_GENERIC_REFINEMENT_NONE) {
      fingerprint_u8(builder, 0u);
    } else if (parameter->refinement_kind ==
               W_SEED_FRONTEND_GENERIC_REFINEMENT_PREDICATE) {
      const predicate_candidate *candidate = fingerprint_candidate_for_argument(
          candidates, candidate_count,
          application->first_argument + offset);
      if (candidate == NULL || candidate->function == NULL)
        return FINGERPRINT_INVALID;
      fingerprint_u8(builder, 1u);
      fingerprint_bytes(builder, candidate->function->body_digest,
                        sizeof(candidate->function->body_digest));
    } else {
      return FINGERPRINT_INVALID;
    }
  }
  if (builder->invalid) return FINGERPRINT_INVALID;
  return builder->unsupported ? FINGERPRINT_UNSUPPORTED
                              : FINGERPRINT_ENCODED;
}

const char *w_seed_generic_validation_state_name(
    w_seed_generic_validation_state state) {
  switch (state) {
    case W_SEED_GENERIC_VALIDATION_VERIFIED:
      return "VERIFIED";
    case W_SEED_GENERIC_VALIDATION_REJECTED:
      return "REJECTED";
    case W_SEED_GENERIC_VALIDATION_UNSUPPORTED:
      return "UNSUPPORTED";
    case W_SEED_GENERIC_VALIDATION_INVALID:
      return "INVALID";
    case W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED:
      return "EVALUATION_FAILED";
    case W_SEED_GENERIC_VALIDATION_CAPACITY:
      return "CAPACITY";
  }
  return "UNKNOWN";
}

const char *w_seed_generic_validation_failure_name(
    w_seed_generic_validation_failure failure) {
  switch (failure) {
    case W_SEED_GENERIC_VALIDATION_FAILURE_NONE:
      return "none";
    case W_SEED_GENERIC_VALIDATION_FAILURE_PREDICATE_FALSE:
      return "predicate:false";
    case W_SEED_GENERIC_VALIDATION_FAILURE_BINDING:
      return "binding";
    case W_SEED_GENERIC_VALIDATION_FAILURE_VALUE:
      return "value";
    case W_SEED_GENERIC_VALIDATION_FAILURE_FUNCTION:
      return "function";
    case W_SEED_GENERIC_VALIDATION_FAILURE_CAPACITY:
      return "capacity";
    case W_SEED_GENERIC_VALIDATION_FAILURE_EVALUATOR_DIAGNOSTIC:
      return "evaluator-diagnostic";
    case W_SEED_GENERIC_VALIDATION_FAILURE_RESULT_TYPE:
      return "result-type";
    case W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT:
      return "invalid-input";
    case W_SEED_GENERIC_VALIDATION_FAILURE_DEPENDENCY_LIMIT:
      return "dependency-limit";
  }
  return "unknown";
}

const char *w_seed_generic_validation_fingerprint_state_name(
    w_seed_generic_validation_fingerprint_state state) {
  switch (state) {
    case W_SEED_GENERIC_VALIDATION_FINGERPRINT_NOT_AVAILABLE:
      return "NOT_AVAILABLE";
    case W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE:
      return "AVAILABLE";
    case W_SEED_GENERIC_VALIDATION_FINGERPRINT_UNSUPPORTED:
      return "UNSUPPORTED";
  }
  return "UNKNOWN";
}

static void quota_consume(w_seed_constir_quota *remaining,
                          const w_seed_constir_eval_result *evaluation) {
  if (remaining == NULL || evaluation == NULL) return;
  if (remaining->steps != SIZE_MAX) {
    remaining->steps = evaluation->consumed_steps >= remaining->steps
                           ? 0u
                           : remaining->steps - evaluation->consumed_steps;
  }
  if (remaining->heap_bytes != SIZE_MAX) {
    remaining->heap_bytes = evaluation->consumed_heap_bytes >=
                                    remaining->heap_bytes
                                ? 0u
                                : remaining->heap_bytes -
                                      evaluation->consumed_heap_bytes;
  }
  if (remaining->result_bytes != SIZE_MAX) {
    remaining->result_bytes = evaluation->consumed_result_bytes >=
                                      remaining->result_bytes
                                  ? 0u
                                  : remaining->result_bytes -
                                        evaluation->consumed_result_bytes;
  }
}

static void receipt_init(w_seed_generic_validation_receipt *receipt) {
  if (receipt == NULL) return;
  (void)memset(receipt, 0, sizeof(*receipt));
  receipt->generic_argument_index = W_SEED_FRONTEND_NONE;
  receipt->argument_const_value_index = W_SEED_FRONTEND_NONE;
  receipt->typed_const_expression_index = W_SEED_FRONTEND_NONE;
  receipt->predicate_parameter_index = W_SEED_FRONTEND_NONE;
  receipt->predicate_function_index = W_SEED_FRONTEND_NONE;
}

static void publish_const_cycle_failure(
    const w_seed_generic_validation_input *input,
    const w_seed_frontend_generic_application *application,
    uint32_t argument_offset, const uint32_t *cycle_path,
    size_t cycle_path_length, w_seed_generic_validation_result *result) {
  if (input == NULL || application == NULL || result == NULL ||
      argument_offset >= application->argument_count ||
      input->receipts == NULL || input->receipt_capacity == 0u ||
      input->frontend_output == NULL ||
      input->frontend_output->generic_arguments == NULL)
    return;
  const w_seed_frontend_generic_argument *argument =
      &input->frontend_output->generic_arguments[
          (size_t)application->first_argument + argument_offset];
  w_seed_generic_validation_receipt *receipt = &input->receipts[0];
  receipt_init(receipt);
  receipt->kind = W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT;
  receipt->generic_argument_index = application->first_argument + argument_offset;
  receipt->argument_const_value_index = argument->const_value_index;
  receipt->typed_const_expression_index =
      argument->typed_const_expression_index;
  receipt->argument_span = argument->span;
  receipt->evaluation.status = W_SEED_CONSTIR_OK;
  receipt->evaluation.diagnostic =
      W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002;
  receipt->evaluation.diagnostic_span = argument->span;
  if (cycle_path != NULL && cycle_path_length != 0u &&
      input->constir_program != NULL &&
      cycle_path[0] < input->constir_program->function_count) {
    receipt->evaluation.diagnostic_span =
        input->constir_program->functions[cycle_path[0]].body_span;
  }
  result->receipts_written = 1u;
  result->evaluation = receipt->evaluation;
  result->diagnostic = receipt->evaluation.diagnostic;
  result->diagnostic_span = receipt->evaluation.diagnostic_span;
}

w_seed_generic_validation_state w_seed_generic_validation_run(
    const w_seed_generic_validation_input *input,
    w_seed_generic_validation_result *result) {
  if (result != NULL) (void)memset(result, 0, sizeof(*result));
  if (result == NULL || input == NULL) {
    if (result != NULL) {
      result->state = W_SEED_GENERIC_VALIDATION_INVALID;
      result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
    }
    return W_SEED_GENERIC_VALIDATION_INVALID;
  }
  result->state = W_SEED_GENERIC_VALIDATION_INVALID;
  result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
  result->application_index = input->application_index;
  validation_context context = {
      input, input->frontend_output, input->frontend_result,
      input->constir_program, 0u};
  if (!frontend_arrays_valid(&context) || context.program == NULL ||
      context.program->frontend_output != context.frontend ||
      context.program->frontend_result != context.frontend_result ||
      !w_seed_constir_validate_program(context.program))
    return result->state;
  const w_seed_frontend_generic_application *application = NULL;
  const w_seed_frontend_struct *head = NULL;
  if (!application_relations_valid(&context, &application, &head))
    return result->state;
  result->head_struct_index = application->head_struct;
  if (application->binding_status ==
      W_SEED_FRONTEND_GENERIC_BINDING_INVALID)
    return result->state;
  if (application->binding_status ==
      W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED) {
    result->state = W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_BINDING;
    return result->state;
  }
  if (application->binding_status !=
          W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE &&
      application->binding_status !=
          W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST)
    return result->state;
  if (application->argument_count != head->generic_parameter_count)
    return result->state;

  predicate_candidate candidates[W_SEED_GENERIC_VALIDATION_MAX_PREDICATES];
  size_t candidate_count = 0u;
  uint32_t value_indices[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  uint32_t typed_function_indices[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  size_t value_counts[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  for (size_t index = 0u; index < W_SEED_FRONTEND_MAX_GENERIC_SLOTS;
       index += 1u) {
    value_indices[index] = W_SEED_FRONTEND_NONE;
    typed_function_indices[index] = W_SEED_CONSTIR_NONE;
    value_counts[index] = 0u;
  }
  size_t computed_argument_count = 0u;
  size_t required_values = 0u;
  /* Read-only preflight of every argument, including synthetic functions and
   * conversion shape.  No caller-owned arena or receipt is written below. */
  for (uint32_t offset = 0u; offset < application->argument_count;
       offset += 1u) {
    const w_seed_frontend_generic_parameter *parameter = NULL;
    if (!parameter_relation_valid(&context, application, offset, &parameter) ||
        parameter == NULL)
      return result->state;
    const w_seed_frontend_generic_argument *argument =
        &context.frontend->generic_arguments[(size_t)application->first_argument +
                                             offset];
    if (parameter->kind != W_SEED_FRONTEND_GENERIC_KIND_VALUE) {
      if (argument->binding_status !=
              W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE ||
          argument->typed_const_expression_index != W_SEED_FRONTEND_NONE)
        return result->state;
      continue;
    }
    uint32_t effective_domain = W_SEED_FRONTEND_NONE;
    if (!effective_domain_type_index(&context, application, offset,
                                     &effective_domain))
      return result->state;
    if (argument->binding_status ==
        W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST) {
      const w_seed_frontend_typed_const_expression *typed = NULL;
      if (!typed_const_expression_relation_valid(&context, application, offset,
                                                 argument, &typed) ||
          typed->effective_type != effective_domain)
        return result->state;
      bool duplicate = false;
      size_t function_index = SIZE_MAX;
      const w_seed_constir_function *function =
          constir_function_for_typed_expression(
              context.program, argument->typed_const_expression_index,
              &function_index, &duplicate);
      if (duplicate || function == NULL) {
        result->state = W_SEED_GENERIC_VALIDATION_INVALID;
        result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
        return result->state;
      }
      if (!function->lowerable) {
        result->state = W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
        result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_FUNCTION;
        return result->state;
      }
      if (function->parameter_count != 0u ||
          function->origin !=
              W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION)
        return result->state;
      if (computed_argument_count == SIZE_MAX)
        return result->state;
      computed_argument_count += 1u;
      typed_function_indices[offset] = (uint32_t)function_index;
      value_counts[offset] = 1u;
    } else if (argument->binding_status ==
               W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE) {
      if (argument->typed_const_expression_index != W_SEED_FRONTEND_NONE)
        return result->state;
      size_t count = 0u;
      const conversion_status count_status = conversion_value_count(
          &context, argument->const_value_index, effective_domain, &count);
      if (count_status == CONVERSION_UNSUPPORTED) {
        result->state = W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
        result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_VALUE;
        return result->state;
      }
      if (count_status != CONVERSION_OK)
        return result->state;
      value_counts[offset] = count;
    } else {
      result->state = W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
      result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_BINDING;
      return result->state;
    }
    if (value_counts[offset] > SIZE_MAX - required_values)
      return result->state;
    required_values += value_counts[offset];
  }
  /* Publish the complete calculated-argument count as soon as the argument
   * preflight is complete.  This remains observable on a later preflight
   * capacity result, and is established before any evaluator step. */
  result->computed_argument_count = computed_argument_count;
  /* Preflight all declared predicates after every value relation is known. */
  for (uint32_t offset = 0u; offset < application->argument_count;
       offset += 1u) {
    const w_seed_frontend_generic_parameter *parameter = NULL;
    if (!parameter_relation_valid(&context, application, offset, &parameter) ||
        parameter == NULL || parameter->kind !=
                                 W_SEED_FRONTEND_GENERIC_KIND_VALUE)
      continue;
    if (parameter->refinement_kind !=
        W_SEED_FRONTEND_GENERIC_REFINEMENT_PREDICATE)
      continue;
    const w_seed_frontend_generic_argument *argument =
        &context.frontend->generic_arguments[(size_t)application->first_argument +
                                             offset];
    predicate_candidate *candidate = &candidates[candidate_count];
    (void)memset(candidate, 0, sizeof(*candidate));
    candidate->argument_index = application->first_argument + offset;
    candidate->module_index = application->module_index;
    candidate->argument_const_value_index = argument->const_value_index;
    candidate->typed_const_expression_index =
        argument->typed_const_expression_index;
    candidate->parameter_index = argument->parameter_index;
    candidate->argument = argument;
    candidate->parameter = parameter;
    if (!effective_domain_type_index(&context, application, offset,
                                     &candidate->effective_domain_type_index))
      return result->state;
    if (!frontend_function_index_valid(
            &context, candidate->parameter->predicate_function_index))
      return result->state;
    bool duplicate_mapping = false;
    if (!predicate_candidate_function_valid(&context, candidate,
                                            &duplicate_mapping)) {
      result->failure = duplicate_mapping
                            ? W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT
                            : W_SEED_GENERIC_VALIDATION_FAILURE_FUNCTION;
      return result->state = duplicate_mapping
                                 ? W_SEED_GENERIC_VALIDATION_INVALID
                                 : W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
    }
    if (!predicate_frontend_signature_valid(&context, candidate))
      return result->state;
    if (candidate->function == NULL) {
      result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
      return result->state = W_SEED_GENERIC_VALIDATION_INVALID;
    }
    if (!candidate->function->lowerable) {
      result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_FUNCTION;
      return result->state = W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
    }
    candidate_count += 1u;
  }
  result->predicate_count = candidate_count;
  /* A D4 dependency graph is checked before any conversion, receipt-capacity,
   * quota, or evaluator decision.  This gives a reachable cycle its own
   * deterministic diagnostic even when a caller supplied zero capacity. */
  uint32_t cycle_path[W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH];
  size_t cycle_path_length = 0u;
  uint32_t cycle_argument_offset = W_SEED_FRONTEND_NONE;
  const const_graph_status graph_status = const_graph_preflight(
      &context, application, typed_function_indices, cycle_path,
      &cycle_path_length, &cycle_argument_offset);
  if (graph_status == CONST_GRAPH_INVALID) {
    result->state = W_SEED_GENERIC_VALIDATION_INVALID;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
    return result->state;
  }
  if (graph_status == CONST_GRAPH_UNSUPPORTED) {
    result->state = W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_FUNCTION;
    return result->state;
  }
  if (graph_status == CONST_GRAPH_LIMIT) {
    result->state = W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_DEPENDENCY_LIMIT;
    return result->state;
  }
  if (graph_status == CONST_GRAPH_CYCLE) {
    result->state = W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_EVALUATOR_DIAGNOSTIC;
    result->const_cycle_path_length = cycle_path_length;
    if (cycle_path_length != 0u)
      (void)memcpy(result->const_cycle_path, cycle_path,
                   cycle_path_length * sizeof(cycle_path[0]));
    result->diagnostic = W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002;
    publish_const_cycle_failure(input, application, cycle_argument_offset,
                                cycle_path, cycle_path_length, result);
    return result->state;
  }
  /* Publish the complete computed count from read-only preflight.  No
   * evaluator step has run at this point, and the count remains stable for
   * all later success/failure states. */
  /* Capacity is decided only after every argument and predicate relation,
   * synthetic function, and signature has passed read-only preflight.  This
   * preserves INVALID precedence when malformed input also has short arenas. */
  if (required_values > input->conversion_value_capacity ||
      (required_values != 0u && input->conversion_values == NULL)) {
    result->state = W_SEED_GENERIC_VALIDATION_CAPACITY;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_CAPACITY;
    return result->state;
  }
  if (candidate_count != 0u &&
      (input->evidence_bytes == NULL ||
       input->evidence_byte_capacity <
           W_SEED_GENERIC_VALIDATION_FALLBACK_BYTES)) {
    result->state = W_SEED_GENERIC_VALIDATION_CAPACITY;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_CAPACITY;
    return result->state;
  }
  /* Receipts are causal records only for calculated arguments and predicates;
   * immediate values are converted without an evaluation receipt. */
  if (computed_argument_count > SIZE_MAX - candidate_count) {
    result->state = W_SEED_GENERIC_VALIDATION_INVALID;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
    return result->state;
  }
  const size_t required_receipts = computed_argument_count + candidate_count;
  if (required_receipts > input->receipt_capacity ||
      (required_receipts != 0u && input->receipts == NULL)) {
    result->state = W_SEED_GENERIC_VALIDATION_CAPACITY;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_CAPACITY;
    return result->state;
  }

  w_seed_constir_quota remaining = input->quota;
  size_t const_receipt_index = 0u;
  bool evaluation_started = false;
  /* Evaluate calculated values in argument order.  Immediate values are
   * converted into the same bounded arena and receive the same normalized
   * value representation. */
  for (uint32_t offset = 0u; offset < application->argument_count;
       offset += 1u) {
    const w_seed_frontend_generic_parameter *parameter = NULL;
    if (!parameter_relation_valid(&context, application, offset, &parameter) ||
        parameter == NULL || parameter->kind !=
                                 W_SEED_FRONTEND_GENERIC_KIND_VALUE)
      continue;
    const w_seed_frontend_generic_argument *argument =
        &context.frontend->generic_arguments[(size_t)application->first_argument +
                                             offset];
    uint32_t value_index = W_SEED_FRONTEND_NONE;
    w_seed_constir_eval_result evaluation;
    (void)memset(&evaluation, 0, sizeof(evaluation));
    w_seed_constir_value value;
    (void)memset(&value, 0, sizeof(value));
    if (argument->binding_status ==
        W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST) {
      const size_t function_index = typed_function_indices[offset];
      evaluation_started = true;
      const w_seed_constir_status status = w_seed_constir_evaluate(
          context.program, (uint32_t)function_index, NULL, 0u,
          remaining, input->eval_workspace, &value, &evaluation);
      quota_consume(&remaining, &evaluation);
      result->evaluation = evaluation;
      result->diagnostic = evaluation.diagnostic;
      result->diagnostic_span = evaluation.diagnostic_span;
      w_seed_generic_validation_receipt *receipt =
          &input->receipts[const_receipt_index++];
      receipt_init(receipt);
      receipt->kind = W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT;
      receipt->generic_argument_index = application->first_argument + offset;
      receipt->argument_const_value_index = argument->const_value_index;
      receipt->typed_const_expression_index =
          argument->typed_const_expression_index;
      receipt->argument_span = argument->span;
      receipt->evaluation = evaluation;
      receipt->eval_value = value;
      result->receipts_written = const_receipt_index;
      if (status != W_SEED_CONSTIR_OK ||
          evaluation.diagnostic != W_SEED_CONSTIR_DIAGNOSTIC_NONE) {
        result->state = evaluation.diagnostic !=
                                W_SEED_CONSTIR_DIAGNOSTIC_NONE
                            ? W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED
                            : W_SEED_GENERIC_VALIDATION_INVALID;
        result->failure = evaluation.diagnostic !=
                                  W_SEED_CONSTIR_DIAGNOSTIC_NONE
                              ? W_SEED_GENERIC_VALIDATION_FAILURE_EVALUATOR_DIAGNOSTIC
                              : W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
        return result->state;
      }
    } else {
      uint32_t domain = W_SEED_FRONTEND_NONE;
      if (!effective_domain_type_index(&context, application, offset, &domain))
        return result->state;
      const conversion_status conversion = convert_const_value(
          &context, argument->const_value_index, domain, 1u, &value_index);
      if (conversion != CONVERSION_OK) {
        const bool post_step_capacity =
            conversion == CONVERSION_CAPACITY && evaluation_started;
        result->state = conversion == CONVERSION_UNSUPPORTED
                            ? W_SEED_GENERIC_VALIDATION_UNSUPPORTED
                            : conversion == CONVERSION_CAPACITY
                                  ? (post_step_capacity
                                         ? W_SEED_GENERIC_VALIDATION_INVALID
                                         : W_SEED_GENERIC_VALIDATION_CAPACITY)
                                  : W_SEED_GENERIC_VALIDATION_INVALID;
        result->failure = conversion == CONVERSION_UNSUPPORTED
                              ? W_SEED_GENERIC_VALIDATION_FAILURE_VALUE
                              : conversion == CONVERSION_CAPACITY
                                    ? (post_step_capacity
                                           ? W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT
                                           : W_SEED_GENERIC_VALIDATION_FAILURE_CAPACITY)
                                    : W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
        return result->state;
      }
      value = context.input->conversion_values[value_index];
    }
    if (argument->binding_status ==
        W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST) {
      const conversion_status append_status = append_value(
          &context, &value, &value_index);
      if (append_status != CONVERSION_OK) {
        /* Preflight reserved this exact scalar slot.  A post-step append
         * failure is therefore an internal relation failure, not a caller
         * capacity result. */
        result->state = W_SEED_GENERIC_VALIDATION_INVALID;
        result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
        return result->state;
      }
      w_seed_generic_validation_receipt *receipt =
          &input->receipts[const_receipt_index - 1u];
      receipt->eval_value = context.input->conversion_values[value_index];
    }
    value_indices[offset] = value_index;
  }

  w_seed_constir_invocation invocations[W_SEED_GENERIC_VALIDATION_MAX_PREDICATES];
  for (size_t index = 0u; index < candidate_count; index += 1u) {
    const uint32_t offset = candidates[index].argument_index -
                            application->first_argument;
    if (offset >= application->argument_count ||
        value_indices[offset] == W_SEED_FRONTEND_NONE)
      return result->state;
    candidates[index].value_index = value_indices[offset];
    invocations[index] = (w_seed_constir_invocation){
        candidates[index].predicate_constir_index,
        &context.input->conversion_values[candidates[index].value_index], 1u};
  }
  if (candidate_count != 0u &&
      !w_seed_constir_validate_invocations_in_validated_program(
          context.program, invocations, candidate_count)) {
    result->state = W_SEED_GENERIC_VALIDATION_INVALID;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
    return result->state;
  }
  for (size_t index = 0u; index < candidate_count; index += 1u) {
    predicate_candidate *candidate = &candidates[index];
    w_seed_constir_value predicate_value;
    w_seed_constir_eval_result evaluation;
    (void)memset(&predicate_value, 0, sizeof(predicate_value));
    (void)memset(&evaluation, 0, sizeof(evaluation));
    const w_seed_constir_status status = w_seed_constir_evaluate(
        context.program, candidate->predicate_constir_index,
        &context.input->conversion_values[candidate->value_index], 1u,
        remaining, input->eval_workspace, &predicate_value, &evaluation);
    quota_consume(&remaining, &evaluation);
    result->evaluation = evaluation;
    result->diagnostic = evaluation.diagnostic;
    result->diagnostic_span = evaluation.diagnostic_span;
    w_seed_generic_validation_receipt *receipt =
        &input->receipts[computed_argument_count + index];
    receipt_init(receipt);
    receipt->kind = W_SEED_GENERIC_VALIDATION_RECEIPT_PREDICATE;
    receipt->generic_argument_index = candidate->argument_index;
    receipt->argument_const_value_index = candidate->argument_const_value_index;
    receipt->typed_const_expression_index =
        candidate->typed_const_expression_index;
    receipt->argument_span = candidate->argument->span;
    receipt->predicate_parameter_index = candidate->parameter_index;
    receipt->predicate_function_index = candidate->predicate_function_index;
    receipt->predicate_span = candidate->parameter->predicate_span;
    receipt->predicate_function_span = candidate->parameter->predicate_function_span;
    receipt->evaluation = evaluation;
    receipt->eval_value = predicate_value;
    receipt->result_is_bool = predicate_value.kind == W_SEED_CONSTIR_VALUE_BOOL;
    receipt->bool_value = predicate_value.bool_value;
    result->receipts_written = computed_argument_count + index + 1u;
    if (status != W_SEED_CONSTIR_OK) {
      result->state = evaluation.diagnostic != W_SEED_CONSTIR_DIAGNOSTIC_NONE
                          ? W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED
                          : W_SEED_GENERIC_VALIDATION_INVALID;
      result->failure = evaluation.diagnostic !=
                                W_SEED_CONSTIR_DIAGNOSTIC_NONE
                            ? W_SEED_GENERIC_VALIDATION_FAILURE_EVALUATOR_DIAGNOSTIC
                            : W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
      return result->state;
    }
    if (evaluation.diagnostic != W_SEED_CONSTIR_DIAGNOSTIC_NONE) {
      result->state = W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED;
      result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_EVALUATOR_DIAGNOSTIC;
      return result->state;
    }
    if (!receipt->result_is_bool) {
      result->state = W_SEED_GENERIC_VALIDATION_INVALID;
      result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_RESULT_TYPE;
      return result->state;
    }
    if (!receipt->bool_value) {
      result->state = W_SEED_GENERIC_VALIDATION_REJECTED;
      result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_PREDICATE_FALSE;
      result->diagnostic = W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0004;
      result->diagnostic_span = candidate->parameter->predicate_span;
      if (!set_rejection(&context, application, candidate, result)) {
        result->state = W_SEED_GENERIC_VALIDATION_CAPACITY;
        result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_CAPACITY;
      }
      return result->state;
    }
  }
  fingerprint_builder fingerprint;
  (void)memset(&fingerprint, 0, sizeof(fingerprint));
  w_seed_sha256_init(&fingerprint.sha);
  const fingerprint_encode_status fingerprint_status = fingerprint_application(
      &context, application, head, candidates, candidate_count, value_indices,
      &fingerprint);
  if (fingerprint_status == FINGERPRINT_INVALID) {
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
    return result->state;
  }
  result->state = W_SEED_GENERIC_VALIDATION_VERIFIED;
  result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_NONE;
  if (fingerprint_status == FINGERPRINT_UNSUPPORTED) {
    result->fingerprint_state =
        W_SEED_GENERIC_VALIDATION_FINGERPRINT_UNSUPPORTED;
  } else {
    w_seed_sha256_final(&fingerprint.sha, result->fingerprint_digest);
    result->fingerprint_state =
        W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE;
  }
  return result->state;
}
