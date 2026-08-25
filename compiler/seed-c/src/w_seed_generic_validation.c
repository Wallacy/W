#include "w_seed_generic_validation.h"
#include "w_seed_constir_session.h"

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
  uint8_t *output;
  size_t output_capacity;
  size_t output_length;
  bool output_overflow;
} fingerprint_builder;

static const char FALLBACK_BYTES[] = "predicate:false";
static const uint8_t FINGERPRINT_PREFIX[] =
    "w-seed-generic-fingerprint-1";
static const uint8_t SPECIALIZATION_PREFIX[] =
    "w-seed-generic-specialization-2";
static const uint8_t NOMINAL_ORIGIN_PREFIX[] =
    "w-seed-nominal-origin-1";

enum {
  NOMINAL_ORIGIN_ROOT_TAG = 0x4fu,
  NOMINAL_ORIGIN_AUTHORITY_TAG = 0x41u,
  NOMINAL_ORIGIN_PACKAGE_TAG = 0x50u,
  NOMINAL_ORIGIN_MODULE_TAG = 0x4du,
  NOMINAL_ORIGIN_SEGMENT_TAG = 0x49u,
  NOMINAL_ORIGIN_DECLARATION_TAG = 0x44u,
  SPECIALIZATION_ROOT_TAG = 0x49u,
  SPECIALIZATION_ORIGIN_TAG = 0x4fu,
};

_Static_assert(W_SEED_GENERIC_VALIDATION_MAX_PREDICATES <=
                   W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_RECORDS,
               "generic evidence record ceiling must cover predicates");
_Static_assert(W_SEED_GENERIC_VALIDATION_MAX_CONST_DEPENDENCIES ==
                   W_SEED_CONSTIR_MAX_CONST_MEMO_ENTRIES,
               "generic dependency and private session ceilings must match");
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
  if (!function->lowerable) return CONST_GRAPH_OK;
  if (function->root_node == W_SEED_CONSTIR_NONE)
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
    if (!function->lowerable) continue;
    if (function->root_node == W_SEED_CONSTIR_NONE)
      return CONST_GRAPH_UNSUPPORTED;
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

/* Frontend graph-first preflight.  ConstIR has no nodes for a well-formed
 * unsupported initializer, but its normalized expression still preserves
 * module-const identifier edges.  Walk those edges before lowerability or
 * evaluator checks so every D7-shaped reachable cycle keeps W-CONST-0002
 * precedence. */
typedef struct {
  const validation_context *context;
  uint32_t module_index;
  uint32_t path[W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH];
  size_t path_length;
  uint32_t seen[W_SEED_GENERIC_VALIDATION_MAX_CONST_DEPENDENCIES];
  size_t seen_count;
  uint32_t cycle[W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH];
  size_t cycle_length;
} frontend_const_graph_context;

static bool frontend_const_graph_on_path(
    const frontend_const_graph_context *graph, uint32_t function_index,
    size_t *position) {
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

static bool frontend_const_graph_seen(
    const frontend_const_graph_context *graph, uint32_t function_index) {
  if (graph == NULL) return false;
  for (size_t offset = 0u; offset < graph->seen_count; offset += 1u)
    if (graph->seen[offset] == function_index) return true;
  return false;
}

static const_graph_status frontend_const_graph_visit_expression(
    frontend_const_graph_context *graph, uint32_t expression_index,
    size_t depth, w_seed_span enclosing_span);

static unsigned int frontend_const_graph_binary_operator_arity(
    w_seed_frontend_text operator_text) {
  if (text_equal(operator_text, (w_seed_frontend_text){"+", 1}) ||
      text_equal(operator_text, (w_seed_frontend_text){"-", 1}) ||
      text_equal(operator_text, (w_seed_frontend_text){"*", 1}) ||
      text_equal(operator_text, (w_seed_frontend_text){"/", 1}) ||
      text_equal(operator_text, (w_seed_frontend_text){"%", 1}) ||
      text_equal(operator_text, (w_seed_frontend_text){"==", 2}) ||
      text_equal(operator_text, (w_seed_frontend_text){"!=", 2}) ||
      text_equal(operator_text, (w_seed_frontend_text){"<", 1}) ||
      text_equal(operator_text, (w_seed_frontend_text){"<=", 2}) ||
      text_equal(operator_text, (w_seed_frontend_text){">", 1}) ||
      text_equal(operator_text, (w_seed_frontend_text){">=", 2}) ||
      text_equal(operator_text, (w_seed_frontend_text){"&&", 2}) ||
      text_equal(operator_text, (w_seed_frontend_text){"||", 2}))
    return 2u;
  return 0u;
}

static bool frontend_const_graph_expression_shape_allowed(
    const w_seed_frontend_expression *expression) {
  if (expression == NULL) return false;
  const w_seed_frontend_expr_kind kind = expression->kind;
  switch (kind) {
    case W_SEED_FRONTEND_EXPR_IDENTIFIER:
    case W_SEED_FRONTEND_EXPR_INTEGER:
    case W_SEED_FRONTEND_EXPR_BOOL:
    case W_SEED_FRONTEND_EXPR_UNARY:
    case W_SEED_FRONTEND_EXPR_BINARY:
    case W_SEED_FRONTEND_EXPR_PARENTHESIS:
      return true;
    case W_SEED_FRONTEND_EXPR_UNSUPPORTED:
      /* Type-incompatible D7 operators retain their child edges while the
       * frontend marks the root unsupported.  Calls/member/index forms do
       * not use one of these closed operator spellings and stay barriers. */
      return text_equal(expression->operator_text,
                        (w_seed_frontend_text){"!", 1}) ||
             text_equal(expression->operator_text,
                        (w_seed_frontend_text){"-", 1}) ||
             frontend_const_graph_binary_operator_arity(
                 expression->operator_text) != 0u;
    default:
      return false;
  }
}

static bool frontend_const_graph_no_children(
    const w_seed_frontend_expression *expression) {
  return expression != NULL && expression->left == W_SEED_FRONTEND_NONE &&
         expression->right == W_SEED_FRONTEND_NONE &&
         expression->first_argument == W_SEED_FRONTEND_NONE &&
         expression->argument_count == 0u &&
         expression->first_switch_arm == W_SEED_FRONTEND_NONE &&
         expression->switch_arm_count == 0u &&
         expression->first_membership_case == W_SEED_FRONTEND_NONE &&
         expression->membership_case_count == 0u;
}

static bool frontend_const_graph_target_mapping_valid(
    const frontend_const_graph_context *graph,
    const w_seed_frontend_expression *expression) {
  if (graph == NULL || expression == NULL ||
      expression->resolved_const_declaration == W_SEED_FRONTEND_NONE)
    return true;
  if (expression->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
      expression->spelling.length == 0u || !text_valid(expression->spelling) ||
      graph->context == NULL || graph->context->frontend == NULL ||
      graph->context->frontend_result == NULL ||
      graph->context->frontend->const_declarations == NULL ||
      (size_t)expression->resolved_const_declaration >=
          graph->context->frontend_result->written.const_declarations)
    return false;
  const uint32_t target_index = expression->resolved_const_declaration;
  const w_seed_frontend_const_declaration *target =
      &graph->context->frontend
           ->const_declarations[target_index];
  if (target->module_index != graph->module_index ||
      !text_valid(target->name) || target->name.length == 0u ||
      !text_equal(target->name, expression->spelling) ||
      !span_valid(target->span) || !span_valid(target->body_span) ||
      !span_contains(target->span, target->body_span) ||
      target->initializer_expression == W_SEED_FRONTEND_NONE)
    return false;
  size_t matches = 0u;
  uint32_t matched_index = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u;
       index < graph->context->frontend_result->written.const_declarations;
       index += 1u) {
    const w_seed_frontend_const_declaration *candidate =
        &graph->context->frontend->const_declarations[index];
    if (candidate->module_index == graph->module_index &&
        text_equal(candidate->name, expression->spelling)) {
      matches += 1u;
      matched_index = (uint32_t)index;
    }
  }
  return matches == 1u && matched_index == target_index;
}

static bool frontend_const_graph_expression_relation_valid(
    const frontend_const_graph_context *graph,
    const w_seed_frontend_expression *expression,
    w_seed_span enclosing_span) {
  if (graph == NULL || expression == NULL ||
      !span_valid(enclosing_span) || !span_valid(expression->span) ||
      !span_contains(enclosing_span, expression->span) ||
      expression->module_index != graph->module_index ||
      expression->owner_function != W_SEED_FRONTEND_NONE ||
      !text_valid(expression->spelling) ||
      !text_valid(expression->operator_text) ||
      !text_valid(expression->member_name) ||
      (expression->inferred_type != W_SEED_FRONTEND_NONE &&
       (graph->context == NULL || graph->context->frontend_result == NULL ||
        (size_t)expression->inferred_type >=
            graph->context->frontend_result->written.types)) ||
      expression->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE ||
      expression->resolved_function_index != W_SEED_FRONTEND_NONE ||
      expression->resolved_local_ordinal != W_SEED_FRONTEND_NONE ||
      expression->const_byte_offset != W_SEED_FRONTEND_NONE ||
      expression->const_byte_count != 0u ||
      !frontend_const_graph_target_mapping_valid(graph, expression))
    return false;
  switch (expression->kind) {
    case W_SEED_FRONTEND_EXPR_IDENTIFIER:
      return frontend_const_graph_no_children(expression);
    case W_SEED_FRONTEND_EXPR_INTEGER:
    case W_SEED_FRONTEND_EXPR_BOOL:
      return frontend_const_graph_no_children(expression) &&
             expression->resolved_const_declaration == W_SEED_FRONTEND_NONE;
    case W_SEED_FRONTEND_EXPR_UNARY:
      return (text_equal(expression->operator_text,
                         (w_seed_frontend_text){"!", 1}) ||
              text_equal(expression->operator_text,
                         (w_seed_frontend_text){"-", 1})) &&
             expression->left != W_SEED_FRONTEND_NONE &&
             expression->right == W_SEED_FRONTEND_NONE &&
             expression->first_argument == W_SEED_FRONTEND_NONE &&
             expression->argument_count == 0u;
    case W_SEED_FRONTEND_EXPR_BINARY:
      return frontend_const_graph_binary_operator_arity(
                 expression->operator_text) != 0u &&
             expression->left != W_SEED_FRONTEND_NONE &&
             expression->right != W_SEED_FRONTEND_NONE &&
             expression->first_argument == W_SEED_FRONTEND_NONE &&
             expression->argument_count == 0u;
    case W_SEED_FRONTEND_EXPR_PARENTHESIS:
      return expression->left != W_SEED_FRONTEND_NONE &&
             expression->right == W_SEED_FRONTEND_NONE &&
             expression->first_argument == W_SEED_FRONTEND_NONE &&
             expression->argument_count == 0u &&
             expression->resolved_const_declaration == W_SEED_FRONTEND_NONE;
    case W_SEED_FRONTEND_EXPR_UNSUPPORTED: {
      if (!frontend_const_graph_expression_shape_allowed(expression)) return false;
      const bool unary =
          text_equal(expression->operator_text,
                     (w_seed_frontend_text){"!", 1}) ||
          (text_equal(expression->operator_text,
                      (w_seed_frontend_text){"-", 1}) &&
           expression->right == W_SEED_FRONTEND_NONE);
      const bool binary =
          frontend_const_graph_binary_operator_arity(expression->operator_text) !=
          0u;
      return ((unary && expression->left != W_SEED_FRONTEND_NONE &&
               expression->right == W_SEED_FRONTEND_NONE) ||
              (binary && expression->left != W_SEED_FRONTEND_NONE &&
               expression->right != W_SEED_FRONTEND_NONE)) &&
             expression->first_argument == W_SEED_FRONTEND_NONE &&
             expression->argument_count == 0u &&
             expression->resolved_const_declaration == W_SEED_FRONTEND_NONE;
    }
    default:
      return false;
  }
}

/* An unsupported generic value can lack a typed-ConstExpr relation.  The
 * frontend still owns its normalized expression record, so recover that root
 * from the caller-owned argument span for graph-first cycle detection.  This
 * is an audit lookup only; it does not make the argument executable. */
static bool frontend_const_graph_expression_for_argument(
    const validation_context *context,
    const w_seed_frontend_generic_application *application,
    uint32_t argument_offset, uint32_t *expression_index,
    bool *typed_relation) {
  if (expression_index != NULL) *expression_index = W_SEED_FRONTEND_NONE;
  if (typed_relation != NULL) *typed_relation = false;
  if (context == NULL || application == NULL ||
      context->frontend == NULL || context->frontend_result == NULL ||
      context->frontend->generic_arguments == NULL ||
      context->frontend->expressions == NULL ||
      argument_offset >= application->argument_count ||
      !range_valid(application->first_argument, application->argument_count,
                   context->frontend_result->written.generic_arguments))
    return false;
  const w_seed_frontend_generic_argument *argument =
      &context->frontend->generic_arguments[(size_t)application->first_argument +
                                            argument_offset];
  if (argument->typed_const_expression_index != W_SEED_FRONTEND_NONE) {
    if (context->frontend->typed_const_expressions == NULL ||
        (size_t)argument->typed_const_expression_index >=
            context->frontend_result->written.typed_const_expressions)
      return false;
    const w_seed_frontend_typed_const_expression *typed =
        &context->frontend->typed_const_expressions[
            argument->typed_const_expression_index];
    if (typed->expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)typed->expression_index >=
            context->frontend_result->written.expressions)
      return false;
    if (expression_index != NULL) *expression_index = typed->expression_index;
    if (typed_relation != NULL) *typed_relation = true;
    return true;
  }

  /* Prefer an exact span.  A labeled argument has a wider argument span than
   * its value, so the fallback also chooses the widest expression contained
   * by that span.  The latest record wins equal spans because normalization
   * appends a root after its children. */
  const w_seed_span argument_span = argument->span;
  size_t best_width = 0u;
  uint32_t best = W_SEED_FRONTEND_NONE;
  bool found_exact = false;
  for (size_t index = 0u;
       index < context->frontend_result->written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression =
        &context->frontend->expressions[index];
    if (expression->module_index != application->module_index ||
        expression->owner_function != W_SEED_FRONTEND_NONE ||
        !span_contains(argument_span, expression->span))
      continue;
    const bool exact = expression->span.start_byte == argument_span.start_byte &&
                       expression->span.end_byte == argument_span.end_byte;
    if (found_exact && !exact) continue;
    const size_t width = expression->span.end_byte -
                         expression->span.start_byte;
    if (exact ||
        (!found_exact && (best == W_SEED_FRONTEND_NONE ||
                          width >= best_width))) {
      if (exact && !found_exact) {
        found_exact = true;
        best = W_SEED_FRONTEND_NONE;
      }
      best = (uint32_t)index;
      best_width = width;
    }
  }
  if (best == W_SEED_FRONTEND_NONE) return false;
  if (expression_index != NULL) *expression_index = best;
  return true;
}

static const_graph_status frontend_const_graph_visit_declaration(
    frontend_const_graph_context *graph, uint32_t declaration_index) {
  if (graph == NULL || graph->context == NULL ||
      graph->context->frontend == NULL || graph->context->frontend_result == NULL ||
      graph->context->program == NULL ||
      declaration_index == W_SEED_FRONTEND_NONE ||
      (size_t)declaration_index >=
          graph->context->frontend_result->written.const_declarations ||
      graph->context->frontend->const_declarations == NULL)
    return CONST_GRAPH_INVALID;
  const w_seed_frontend_const_declaration *declaration =
      &graph->context->frontend->const_declarations[declaration_index];
  if (declaration->module_index != graph->module_index ||
      !text_valid(declaration->name) || declaration->name.length == 0u ||
      !span_valid(declaration->span) || !span_valid(declaration->body_span) ||
      !span_contains(declaration->span, declaration->body_span) ||
      declaration->initializer_expression == W_SEED_FRONTEND_NONE ||
      (size_t)declaration->initializer_expression >=
          graph->context->frontend_result->written.expressions)
    return CONST_GRAPH_INVALID;
  bool duplicate = false;
  size_t function_index = SIZE_MAX;
  const w_seed_constir_function *function =
      constir_function_for_const_declaration(
          graph->context->program, declaration_index, &function_index,
          &duplicate);
  if (duplicate || function == NULL || function_index == SIZE_MAX ||
      function_index > (size_t)UINT32_MAX)
    return CONST_GRAPH_INVALID;
  size_t cycle_start = SIZE_MAX;
  if (frontend_const_graph_on_path(graph, (uint32_t)function_index,
                                   &cycle_start)) {
    const size_t cycle_members = graph->path_length - cycle_start;
    if (cycle_members == 0u ||
        cycle_members > W_SEED_GENERIC_VALIDATION_MAX_CONST_DEPENDENCIES ||
        cycle_members + 1u >
            W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH)
      return CONST_GRAPH_LIMIT;
    graph->cycle_length = cycle_members + 1u;
    for (size_t offset = 0u; offset < cycle_members; offset += 1u)
      graph->cycle[offset] = graph->path[cycle_start + offset];
    graph->cycle[cycle_members] = (uint32_t)function_index;
    return CONST_GRAPH_CYCLE;
  }
  if (frontend_const_graph_seen(graph, (uint32_t)function_index))
    return CONST_GRAPH_OK;
  if (graph->seen_count >= W_SEED_GENERIC_VALIDATION_MAX_CONST_DEPENDENCIES ||
      graph->path_length >= W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH)
    return CONST_GRAPH_LIMIT;
  if (declaration->effective_type != W_SEED_FRONTEND_NONE) {
    if (graph->context->frontend->types == NULL ||
        (size_t)declaration->effective_type >=
            graph->context->frontend_result->written.types)
      return CONST_GRAPH_INVALID;
    const w_seed_frontend_type *type =
        &graph->context->frontend->types[declaration->effective_type];
    if (type->kind != W_SEED_FRONTEND_TYPE_BOOL &&
        (type->kind != W_SEED_FRONTEND_TYPE_INTEGER ||
         type->bit_width == 0u))
      return CONST_GRAPH_OK;
  }
  graph->seen[graph->seen_count++] = (uint32_t)function_index;
  graph->path[graph->path_length++] = (uint32_t)function_index;
  const_graph_status status = frontend_const_graph_visit_expression(
      graph, declaration->initializer_expression, 1u, declaration->body_span);
  graph->path_length -= 1u;
  return status;
}

static const_graph_status frontend_const_graph_visit_expression(
    frontend_const_graph_context *graph, uint32_t expression_index,
    size_t depth, w_seed_span enclosing_span) {
  if (graph == NULL || graph->context == NULL ||
      graph->context->frontend == NULL || graph->context->frontend_result == NULL ||
      graph->context->frontend->expressions == NULL || depth == 0u ||
      depth > W_SEED_GENERIC_VALIDATION_MAX_DEPTH ||
      expression_index == W_SEED_FRONTEND_NONE ||
      (size_t)expression_index >=
          graph->context->frontend_result->written.expressions)
    return CONST_GRAPH_INVALID;
  const w_seed_frontend_expression *expression =
      &graph->context->frontend->expressions[expression_index];
  if (!frontend_const_graph_expression_shape_allowed(expression))
    return CONST_GRAPH_OK;
  if (!frontend_const_graph_expression_relation_valid(graph, expression,
                                                      enclosing_span))
    return CONST_GRAPH_INVALID;
  if (expression->resolved_const_declaration != W_SEED_FRONTEND_NONE) {
    const_graph_status status = frontend_const_graph_visit_declaration(
        graph, expression->resolved_const_declaration);
    if (status != CONST_GRAPH_OK) return status;
  }
  if (expression->left != W_SEED_FRONTEND_NONE) {
    const_graph_status status = frontend_const_graph_visit_expression(
        graph, expression->left, depth + 1u, expression->span);
    if (status != CONST_GRAPH_OK) return status;
  }
  if (expression->right != W_SEED_FRONTEND_NONE) {
    const_graph_status status = frontend_const_graph_visit_expression(
        graph, expression->right, depth + 1u, expression->span);
    if (status != CONST_GRAPH_OK) return status;
  }
  return CONST_GRAPH_OK;
}

static const_graph_status frontend_const_graph_preflight(
    const validation_context *context,
    const w_seed_frontend_generic_application *application,
    const uint32_t typed_function_indices[W_SEED_FRONTEND_MAX_GENERIC_SLOTS],
    bool include_unbound,
    uint32_t cycle_path[W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH],
    size_t *cycle_path_length, uint32_t *cycle_argument_offset,
    size_t *calculated_argument_count) {
  if (cycle_path_length != NULL) *cycle_path_length = 0u;
  if (cycle_argument_offset != NULL) *cycle_argument_offset = W_SEED_FRONTEND_NONE;
  if (calculated_argument_count != NULL) *calculated_argument_count = 0u;
  if (context == NULL || application == NULL ||
      context->frontend == NULL || context->frontend_result == NULL ||
      context->program == NULL)
    return CONST_GRAPH_INVALID;
  frontend_const_graph_context graph;
  (void)memset(&graph, 0, sizeof(graph));
  graph.context = context;
  graph.module_index = application->module_index;
  uint32_t root_expressions[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  bool recovered_roots[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  for (size_t index = 0u; index < W_SEED_FRONTEND_MAX_GENERIC_SLOTS;
       index += 1u) {
    root_expressions[index] = W_SEED_FRONTEND_NONE;
    recovered_roots[index] = false;
  }

  /* First recover every source-backed D7 root.  This pass is read-only and
   * completes the calculated count before graph traversal can return a cycle. */
  for (uint32_t offset = 0u; offset < application->argument_count; offset += 1u) {
    const bool typed_pending = typed_function_indices != NULL &&
                               typed_function_indices[offset] !=
                               W_SEED_CONSTIR_NONE;
    const w_seed_frontend_generic_argument *argument =
        context->frontend->generic_arguments == NULL ||
                !range_valid(application->first_argument,
                             application->argument_count,
                             context->frontend_result->written.generic_arguments)
            ? NULL
            : &context->frontend->generic_arguments[
                  (size_t)application->first_argument + offset];
    const bool unbound = include_unbound && argument != NULL &&
                         argument->binding_status ==
                             W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED;
    if (!typed_pending && !unbound) continue;
    uint32_t expression_index = W_SEED_FRONTEND_NONE;
    if (!frontend_const_graph_expression_for_argument(
            context, application, offset, &expression_index, NULL)) {
      return CONST_GRAPH_INVALID;
    }
    if (expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)expression_index >= context->frontend_result->written.expressions)
      return CONST_GRAPH_INVALID;
    const w_seed_frontend_expression *expression =
        &context->frontend->expressions[expression_index];
    if (include_unbound && unbound &&
        !frontend_const_graph_expression_shape_allowed(expression))
      continue;
    if (offset >= W_SEED_FRONTEND_MAX_GENERIC_SLOTS)
      return CONST_GRAPH_LIMIT;
    root_expressions[offset] = expression_index;
    recovered_roots[offset] = true;
    if (include_unbound && unbound && calculated_argument_count != NULL) {
      if (*calculated_argument_count == SIZE_MAX) return CONST_GRAPH_LIMIT;
      *calculated_argument_count += 1u;
    }
  }

  for (uint32_t offset = 0u; offset < application->argument_count; offset += 1u) {
    if (!recovered_roots[offset]) continue;
    const_graph_status status = frontend_const_graph_visit_expression(
        &graph, root_expressions[offset], 1u,
        context->frontend->generic_arguments[
            (size_t)application->first_argument + offset]
             .span);
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
    const w_seed_frontend_struct **head_out,
    bool *invalid_argument_out) {
  if (application_out != NULL) *application_out = NULL;
  if (head_out != NULL) *head_out = NULL;
  if (invalid_argument_out != NULL) *invalid_argument_out = false;
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
  bool invalid_argument = false;
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
    if (argument->binding_status ==
        W_SEED_FRONTEND_GENERIC_BINDING_INVALID)
      invalid_argument = true;
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
  if (invalid_argument_out != NULL) *invalid_argument_out = invalid_argument;
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
  if (length > SIZE_MAX - builder->output_length) {
    builder->invalid = true;
    return;
  }
  if (builder->output != NULL) {
    if (builder->output_length > builder->output_capacity ||
        length > builder->output_capacity - builder->output_length) {
      builder->output_overflow = true;
    } else if (length != 0u) {
      (void)memcpy(builder->output + builder->output_length, bytes, length);
    }
  }
  builder->output_length += length;
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

typedef enum {
  NOMINAL_ORIGIN_PARSE_INVALID = 0,
  NOMINAL_ORIGIN_PARSE_AVAILABLE,
  NOMINAL_ORIGIN_PARSE_UNSUPPORTED,
} nominal_origin_parse_status;

typedef struct {
  const uint8_t *bytes;
  size_t length;
  size_t offset;
} nominal_origin_reader;

typedef struct {
  const uint8_t *authority;
  size_t authority_length;
  const uint8_t *package_name;
  size_t package_length;
  const uint8_t *module_segments[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS];
  size_t module_segment_lengths[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS];
  size_t module_segment_count;
  uint8_t declaration_kind;
  uint8_t owner_kinds[W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_OWNER_CHAIN];
  const uint8_t *owner_names[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_OWNER_CHAIN];
  size_t owner_name_lengths[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_OWNER_CHAIN];
  size_t owner_count;
  const uint8_t *declared_name;
  size_t declared_name_length;
  bool non_ascii;
} nominal_origin_decoded;

static void fingerprint_mark_unsupported(fingerprint_builder *builder);

static bool nominal_origin_kind_valid(uint8_t kind) {
  switch (kind) {
    case (uint8_t)W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT:
    case (uint8_t)W_SEED_GENERIC_NOMINAL_DECLARATION_TYPE:
    case (uint8_t)W_SEED_GENERIC_NOMINAL_DECLARATION_OBJECT:
    case (uint8_t)W_SEED_GENERIC_NOMINAL_DECLARATION_ENUM:
    case (uint8_t)W_SEED_GENERIC_NOMINAL_DECLARATION_PROTOCOL:
    case (uint8_t)W_SEED_GENERIC_NOMINAL_DECLARATION_SERVICE:
      return true;
    default:
      return false;
  }
}

typedef enum {
  NOMINAL_ORIGIN_TEXT_INVALID = 0,
  NOMINAL_ORIGIN_TEXT_ASCII,
  NOMINAL_ORIGIN_TEXT_UNSUPPORTED,
} nominal_origin_text_status;

typedef enum {
  NOMINAL_ORIGIN_TEXT_PACKAGE = 0,
  NOMINAL_ORIGIN_TEXT_IDENTIFIER,
} nominal_origin_text_kind;

typedef enum {
  NOMINAL_ORIGIN_INPUT_INVALID = 0,
  NOMINAL_ORIGIN_INPUT_AVAILABLE,
  NOMINAL_ORIGIN_INPUT_UNSUPPORTED,
} nominal_origin_input_status;

static bool nominal_origin_utf8_valid(const uint8_t *bytes, size_t length,
                                      bool *non_ascii) {
  if (non_ascii != NULL) *non_ascii = false;
  if (bytes == NULL || length == 0u) return false;
  size_t index = 0u;
  while (index < length) {
    const uint8_t first = bytes[index];
    if (first == 0u) return false;
    if (first < 0x80u) {
      index += 1u;
      continue;
    }
    size_t sequence_length = 0u;
    uint8_t second_min = 0x80u;
    uint8_t second_max = 0xbfu;
    if (first >= 0xc2u && first <= 0xdfu) {
      sequence_length = 2u;
    } else if (first >= 0xe0u && first <= 0xefu) {
      sequence_length = 3u;
      if (first == 0xe0u) second_min = 0xa0u;
      if (first == 0xedu) second_max = 0x9fu;
    } else if (first >= 0xf0u && first <= 0xf4u) {
      sequence_length = 4u;
      if (first == 0xf0u) second_min = 0x90u;
      if (first == 0xf4u) second_max = 0x8fu;
    } else {
      return false;
    }
    if (sequence_length > length - index) return false;
    const uint8_t second = bytes[index + 1u];
    if (second < second_min || second > second_max) return false;
    for (size_t offset = 2u; offset < sequence_length; offset += 1u) {
      const uint8_t continuation = bytes[index + offset];
      if (continuation < 0x80u || continuation > 0xbfu) return false;
    }
    if (non_ascii != NULL) *non_ascii = true;
    index += sequence_length;
  }
  return true;
}

static bool nominal_origin_identifier_ascii_valid(const uint8_t *bytes,
                                                  size_t length) {
  if (bytes == NULL || length == 0u) return false;
  const uint8_t first = bytes[0];
  if (!((first >= (uint8_t)'A' && first <= (uint8_t)'Z') ||
        (first >= (uint8_t)'a' && first <= (uint8_t)'z') ||
        first == (uint8_t)'_'))
    return false;
  for (size_t index = 1u; index < length; index += 1u) {
    const uint8_t current = bytes[index];
    if (!((current >= (uint8_t)'A' && current <= (uint8_t)'Z') ||
          (current >= (uint8_t)'a' && current <= (uint8_t)'z') ||
          (current >= (uint8_t)'0' && current <= (uint8_t)'9') ||
          current == (uint8_t)'_'))
      return false;
  }
  return true;
}

static bool nominal_origin_package_ascii_valid(const uint8_t *bytes,
                                               size_t length) {
  if (bytes == NULL || length == 0u ||
      length > W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PACKAGE_BYTES)
    return false;
  size_t separator = SIZE_MAX;
  for (size_t index = 0u; index < length; index += 1u) {
    if (bytes[index] == (uint8_t)'/') {
      if (separator != SIZE_MAX) return false;
      separator = index;
    }
  }
  if (separator == SIZE_MAX || separator == 0u || separator + 1u >= length ||
      separator > W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PACKAGE_COMPONENT_BYTES ||
      length - separator - 1u >
          W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PACKAGE_COMPONENT_BYTES)
    return false;
  for (size_t part = 0u; part < 2u; part += 1u) {
    const size_t start = part == 0u ? 0u : separator + 1u;
    const size_t end = part == 0u ? separator : length;
    if (bytes[start] < (uint8_t)'a' || bytes[start] > (uint8_t)'z')
      return false;
    for (size_t index = start + 1u; index < end; index += 1u) {
      const uint8_t current = bytes[index];
      if (!((current >= (uint8_t)'a' && current <= (uint8_t)'z') ||
            (current >= (uint8_t)'0' && current <= (uint8_t)'9') ||
            current == (uint8_t)'-'))
        return false;
    }
  }
  return true;
}

static nominal_origin_text_status nominal_origin_text_validate(
    const uint8_t *bytes, size_t length, size_t maximum,
    nominal_origin_text_kind kind) {
  bool non_ascii = false;
  if (!nominal_origin_utf8_valid(bytes, length, &non_ascii))
    return NOMINAL_ORIGIN_TEXT_INVALID;
  if (non_ascii) {
    /* PackageIdentity is an ASCII scoped name in this seed.  Other names are
     * valid Unicode facts, but NFC resolution is deliberately not here. */
    return kind == NOMINAL_ORIGIN_TEXT_PACKAGE
               ? NOMINAL_ORIGIN_TEXT_INVALID
               : NOMINAL_ORIGIN_TEXT_UNSUPPORTED;
  }
  if (kind == NOMINAL_ORIGIN_TEXT_PACKAGE) {
    return nominal_origin_package_ascii_valid(bytes, length)
               ? NOMINAL_ORIGIN_TEXT_ASCII
               : NOMINAL_ORIGIN_TEXT_INVALID;
  }
  if (!nominal_origin_identifier_ascii_valid(bytes, length))
    return NOMINAL_ORIGIN_TEXT_INVALID;
  return length <= maximum ? NOMINAL_ORIGIN_TEXT_ASCII
                           : NOMINAL_ORIGIN_TEXT_UNSUPPORTED;
}

static nominal_origin_text_status nominal_origin_text_input_status(
    w_seed_frontend_text text, size_t maximum,
    nominal_origin_text_kind kind) {
  if (text.data == NULL || text.length == 0u) return NOMINAL_ORIGIN_TEXT_INVALID;
  return nominal_origin_text_validate((const uint8_t *)text.data, text.length,
                                      maximum, kind);
}

static nominal_origin_input_status nominal_origin_input_validate(
    const w_seed_generic_nominal_origin *origin) {
  if (origin == NULL || origin->authority_preimage_length == 0u ||
      origin->authority_preimage == NULL ||
      origin->module_path_segment_count == 0u ||
      origin->module_path_segments == NULL ||
      (origin->owner_chain_count != 0u && origin->owner_chain == NULL) ||
      !nominal_origin_kind_valid((uint8_t)origin->declaration_kind))
    return NOMINAL_ORIGIN_INPUT_INVALID;
  const bool authority_over_ceiling =
      origin->authority_preimage_length >
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_AUTHORITY_BYTES;
  const bool module_count_over_ceiling =
      origin->module_path_segment_count >
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS;
  const bool owner_count_over_ceiling =
      origin->owner_chain_count > W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_OWNER_CHAIN;
  bool unsupported = false;
  nominal_origin_text_status text_status = nominal_origin_text_input_status(
      origin->scoped_package_name,
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PACKAGE_BYTES,
      NOMINAL_ORIGIN_TEXT_PACKAGE);
  if (text_status == NOMINAL_ORIGIN_TEXT_INVALID)
    return NOMINAL_ORIGIN_INPUT_INVALID;
  unsupported = unsupported || text_status == NOMINAL_ORIGIN_TEXT_UNSUPPORTED;
  text_status = nominal_origin_text_input_status(
      origin->declared_name, W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_NAME_BYTES,
      NOMINAL_ORIGIN_TEXT_IDENTIFIER);
  if (text_status == NOMINAL_ORIGIN_TEXT_INVALID)
    return NOMINAL_ORIGIN_INPUT_INVALID;
  unsupported = unsupported || text_status == NOMINAL_ORIGIN_TEXT_UNSUPPORTED;
  unsupported = unsupported || authority_over_ceiling;
  /* Counts above the supported array sizes are reported without walking the
   * caller-owned arrays.  The package and declaration name were already
   * checked, so malformed scalar facts still take precedence. */
  if (module_count_over_ceiling || owner_count_over_ceiling)
    return NOMINAL_ORIGIN_INPUT_UNSUPPORTED;
  for (size_t index = 0u; index < origin->module_path_segment_count;
       index += 1u) {
    text_status = nominal_origin_text_input_status(
        origin->module_path_segments[index],
        W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_SEGMENT_BYTES,
        NOMINAL_ORIGIN_TEXT_IDENTIFIER);
    if (text_status == NOMINAL_ORIGIN_TEXT_INVALID)
      return NOMINAL_ORIGIN_INPUT_INVALID;
    unsupported = unsupported || text_status == NOMINAL_ORIGIN_TEXT_UNSUPPORTED;
  }
  for (size_t index = 0u; index < origin->owner_chain_count; index += 1u) {
    const w_seed_generic_nominal_owner *owner = &origin->owner_chain[index];
    if (!nominal_origin_kind_valid(owner->kind))
      return NOMINAL_ORIGIN_INPUT_INVALID;
    text_status = nominal_origin_text_input_status(
        owner->name, W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_NAME_BYTES,
        NOMINAL_ORIGIN_TEXT_IDENTIFIER);
    if (text_status == NOMINAL_ORIGIN_TEXT_INVALID)
      return NOMINAL_ORIGIN_INPUT_INVALID;
    unsupported = unsupported || text_status == NOMINAL_ORIGIN_TEXT_UNSUPPORTED;
  }
  return unsupported ? NOMINAL_ORIGIN_INPUT_UNSUPPORTED
                     : NOMINAL_ORIGIN_INPUT_AVAILABLE;
}

static bool nominal_origin_size_add(size_t *total, size_t amount) {
  if (total == NULL || amount > SIZE_MAX - *total) return false;
  *total += amount;
  return true;
}

static nominal_origin_input_status nominal_origin_encoded_size_status(
    const w_seed_generic_nominal_origin *origin) {
  const nominal_origin_input_status input_status =
      nominal_origin_input_validate(origin);
  if (input_status != NOMINAL_ORIGIN_INPUT_AVAILABLE) return input_status;
  size_t total = sizeof(NOMINAL_ORIGIN_PREFIX) - 1u;
  if (!nominal_origin_size_add(&total, 1u + 1u + sizeof(uint32_t) +
                                        origin->authority_preimage_length) ||
      !nominal_origin_size_add(&total, 1u + sizeof(uint32_t) +
                                        origin->scoped_package_name.length) ||
      !nominal_origin_size_add(&total, 1u + sizeof(uint32_t)))
    return NOMINAL_ORIGIN_INPUT_UNSUPPORTED;
  for (size_t index = 0u; index < origin->module_path_segment_count;
       index += 1u) {
    const w_seed_frontend_text segment = origin->module_path_segments[index];
    if (!nominal_origin_size_add(&total, 1u + sizeof(uint32_t) + segment.length))
      return NOMINAL_ORIGIN_INPUT_UNSUPPORTED;
  }
  if (!nominal_origin_size_add(&total, 1u + 1u + sizeof(uint32_t)))
    return NOMINAL_ORIGIN_INPUT_UNSUPPORTED;
  for (size_t index = 0u; index < origin->owner_chain_count;
       index += 1u) {
    const w_seed_generic_nominal_owner owner = origin->owner_chain[index];
    if (!nominal_origin_size_add(&total, 1u + sizeof(uint32_t) +
                                          owner.name.length))
      return NOMINAL_ORIGIN_INPUT_UNSUPPORTED;
  }
  if (!nominal_origin_size_add(&total, sizeof(uint32_t) +
                                        origin->declared_name.length))
    return NOMINAL_ORIGIN_INPUT_UNSUPPORTED;
  return total > W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES
             ? NOMINAL_ORIGIN_INPUT_UNSUPPORTED
             : NOMINAL_ORIGIN_INPUT_AVAILABLE;
}

static bool nominal_origin_reader_bytes(nominal_origin_reader *reader,
                                        size_t count,
                                        const uint8_t **bytes_out) {
  if (bytes_out != NULL) *bytes_out = NULL;
  if (reader == NULL || reader->offset > reader->length ||
      count > reader->length - reader->offset)
    return false;
  if (bytes_out != NULL) *bytes_out = reader->bytes + reader->offset;
  reader->offset += count;
  return true;
}

static bool nominal_origin_reader_u8(nominal_origin_reader *reader,
                                     uint8_t *value_out) {
  const uint8_t *bytes = NULL;
  if (!nominal_origin_reader_bytes(reader, 1u, &bytes)) return false;
  if (value_out != NULL) *value_out = bytes[0];
  return true;
}

static bool nominal_origin_reader_u32(nominal_origin_reader *reader,
                                      uint32_t *value_out) {
  const uint8_t *bytes = NULL;
  if (!nominal_origin_reader_bytes(reader, 4u, &bytes)) return false;
  if (value_out != NULL)
    *value_out = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
                 ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
  return true;
}

static nominal_origin_text_status nominal_origin_reader_text(
    nominal_origin_reader *reader, const uint8_t **bytes_out,
    size_t *length_out, size_t maximum, nominal_origin_text_kind kind) {
  if (bytes_out != NULL) *bytes_out = NULL;
  if (length_out != NULL) *length_out = 0u;
  uint32_t encoded_length = 0u;
  if (!nominal_origin_reader_u32(reader, &encoded_length) || encoded_length == 0u)
    return NOMINAL_ORIGIN_TEXT_INVALID;
  const uint8_t *bytes = NULL;
  if (!nominal_origin_reader_bytes(reader, encoded_length, &bytes) ||
      bytes == NULL)
    return NOMINAL_ORIGIN_TEXT_INVALID;
  if (bytes_out != NULL) *bytes_out = bytes;
  if (length_out != NULL) *length_out = encoded_length;
  return nominal_origin_text_validate(bytes, encoded_length, maximum, kind);
}

static nominal_origin_parse_status nominal_origin_parse(
    const uint8_t *preimage, size_t preimage_length,
    nominal_origin_decoded *decoded) {
  if (decoded != NULL) (void)memset(decoded, 0, sizeof(*decoded));
  if (preimage == NULL || preimage_length == 0u)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  if (preimage_length >
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PARSE_PREIMAGE_BYTES)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  bool unsupported =
      preimage_length > W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES;
  nominal_origin_reader reader = {preimage, preimage_length, 0u};
  const uint8_t *prefix = NULL;
  if (!nominal_origin_reader_bytes(&reader, sizeof(NOMINAL_ORIGIN_PREFIX) - 1u,
                                   &prefix) ||
      memcmp(prefix, NOMINAL_ORIGIN_PREFIX,
             sizeof(NOMINAL_ORIGIN_PREFIX) - 1u) != 0)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  uint8_t tag = 0u;
  if (!nominal_origin_reader_u8(&reader, &tag) || tag != NOMINAL_ORIGIN_ROOT_TAG)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  if (!nominal_origin_reader_u8(&reader, &tag) ||
      tag != NOMINAL_ORIGIN_AUTHORITY_TAG)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  uint32_t authority_length = 0u;
  if (!nominal_origin_reader_u32(&reader, &authority_length) ||
      authority_length == 0u)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  if (!nominal_origin_reader_bytes(&reader, authority_length,
                                   decoded == NULL ? NULL : &decoded->authority))
    return NOMINAL_ORIGIN_PARSE_INVALID;
  if (decoded != NULL) decoded->authority_length = authority_length;
  unsupported = unsupported ||
                authority_length >
                    W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_AUTHORITY_BYTES;
  if (!nominal_origin_reader_u8(&reader, &tag) || tag != NOMINAL_ORIGIN_PACKAGE_TAG)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  nominal_origin_text_status text_status = nominal_origin_reader_text(
      &reader, decoded == NULL ? NULL : &decoded->package_name,
      decoded == NULL ? NULL : &decoded->package_length,
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PACKAGE_BYTES,
      NOMINAL_ORIGIN_TEXT_PACKAGE);
  if (text_status == NOMINAL_ORIGIN_TEXT_INVALID)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  unsupported = unsupported || text_status == NOMINAL_ORIGIN_TEXT_UNSUPPORTED;
  if (!nominal_origin_reader_u8(&reader, &tag) || tag != NOMINAL_ORIGIN_MODULE_TAG)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  uint32_t segment_count = 0u;
  if (!nominal_origin_reader_u32(&reader, &segment_count) || segment_count == 0u)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  const bool module_count_over_ceiling =
      segment_count > W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS;
  unsupported = unsupported || module_count_over_ceiling;
  if (decoded != NULL && !module_count_over_ceiling)
    decoded->module_segment_count = segment_count;
  for (uint32_t index = 0u; index < segment_count; index += 1u) {
    if (!nominal_origin_reader_u8(&reader, &tag) ||
        tag != NOMINAL_ORIGIN_SEGMENT_TAG)
      return NOMINAL_ORIGIN_PARSE_INVALID;
    const bool keep_segment =
        decoded != NULL &&
        index < W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS;
    const uint8_t **segment_out =
        keep_segment ? &decoded->module_segments[index] : NULL;
    size_t *segment_length_out =
        keep_segment ? &decoded->module_segment_lengths[index] : NULL;
    text_status = nominal_origin_reader_text(
        &reader, segment_out, segment_length_out,
        W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_SEGMENT_BYTES,
        NOMINAL_ORIGIN_TEXT_IDENTIFIER);
    if (text_status == NOMINAL_ORIGIN_TEXT_INVALID)
      return NOMINAL_ORIGIN_PARSE_INVALID;
    unsupported = unsupported || text_status == NOMINAL_ORIGIN_TEXT_UNSUPPORTED;
  }
  if (!nominal_origin_reader_u8(&reader, &tag) ||
      tag != NOMINAL_ORIGIN_DECLARATION_TAG)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  if (!nominal_origin_reader_u8(&reader, &tag) || !nominal_origin_kind_valid(tag))
    return NOMINAL_ORIGIN_PARSE_INVALID;
  if (decoded != NULL) decoded->declaration_kind = tag;
  uint32_t owner_count = 0u;
  if (!nominal_origin_reader_u32(&reader, &owner_count))
    return NOMINAL_ORIGIN_PARSE_INVALID;
  const bool owner_count_over_ceiling =
      owner_count > W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_OWNER_CHAIN;
  unsupported = unsupported || owner_count_over_ceiling;
  if (decoded != NULL && !owner_count_over_ceiling)
    decoded->owner_count = owner_count;
  for (uint32_t index = 0u; index < owner_count; index += 1u) {
    if (!nominal_origin_reader_u8(&reader, &tag) || !nominal_origin_kind_valid(tag))
      return NOMINAL_ORIGIN_PARSE_INVALID;
    const bool keep_owner =
        decoded != NULL &&
        index < W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_OWNER_CHAIN;
    if (keep_owner) decoded->owner_kinds[index] = tag;
    const uint8_t **owner_name_out =
        keep_owner ? &decoded->owner_names[index] : NULL;
    size_t *owner_length_out =
        keep_owner ? &decoded->owner_name_lengths[index] : NULL;
    text_status = nominal_origin_reader_text(
        &reader, owner_name_out, owner_length_out,
        W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_NAME_BYTES,
        NOMINAL_ORIGIN_TEXT_IDENTIFIER);
    if (text_status == NOMINAL_ORIGIN_TEXT_INVALID)
      return NOMINAL_ORIGIN_PARSE_INVALID;
    unsupported = unsupported || text_status == NOMINAL_ORIGIN_TEXT_UNSUPPORTED;
  }
  text_status = nominal_origin_reader_text(
      &reader, decoded == NULL ? NULL : &decoded->declared_name,
      decoded == NULL ? NULL : &decoded->declared_name_length,
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_NAME_BYTES,
      NOMINAL_ORIGIN_TEXT_IDENTIFIER);
  if (text_status == NOMINAL_ORIGIN_TEXT_INVALID)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  unsupported = unsupported || text_status == NOMINAL_ORIGIN_TEXT_UNSUPPORTED;
  if (reader.offset != reader.length) return NOMINAL_ORIGIN_PARSE_INVALID;
  return unsupported ? NOMINAL_ORIGIN_PARSE_UNSUPPORTED
                     : NOMINAL_ORIGIN_PARSE_AVAILABLE;
}

static fingerprint_encode_status nominal_origin_encode(
    const w_seed_generic_nominal_origin *origin, fingerprint_builder *builder) {
  const nominal_origin_input_status input_status =
      nominal_origin_encoded_size_status(origin);
  if (input_status == NOMINAL_ORIGIN_INPUT_INVALID)
    return FINGERPRINT_INVALID;
  if (input_status == NOMINAL_ORIGIN_INPUT_UNSUPPORTED) {
    fingerprint_mark_unsupported(builder);
    return FINGERPRINT_UNSUPPORTED;
  }
  fingerprint_bytes(builder, NOMINAL_ORIGIN_PREFIX,
                    sizeof(NOMINAL_ORIGIN_PREFIX) - 1u);
  fingerprint_u8(builder, NOMINAL_ORIGIN_ROOT_TAG);
  fingerprint_u8(builder, NOMINAL_ORIGIN_AUTHORITY_TAG);
  fingerprint_u32(builder, (uint32_t)origin->authority_preimage_length);
  fingerprint_bytes(builder, origin->authority_preimage,
                    origin->authority_preimage_length);
  fingerprint_u8(builder, NOMINAL_ORIGIN_PACKAGE_TAG);
  (void)fingerprint_text(builder, origin->scoped_package_name);
  fingerprint_u8(builder, NOMINAL_ORIGIN_MODULE_TAG);
  fingerprint_u32(builder, (uint32_t)origin->module_path_segment_count);
  for (size_t index = 0u; index < origin->module_path_segment_count; index += 1u) {
    fingerprint_u8(builder, NOMINAL_ORIGIN_SEGMENT_TAG);
    (void)fingerprint_text(builder, origin->module_path_segments[index]);
  }
  fingerprint_u8(builder, NOMINAL_ORIGIN_DECLARATION_TAG);
  fingerprint_u8(builder, (uint8_t)origin->declaration_kind);
  fingerprint_u32(builder, (uint32_t)origin->owner_chain_count);
  for (size_t index = 0u; index < origin->owner_chain_count; index += 1u) {
    fingerprint_u8(builder, origin->owner_chain[index].kind);
    (void)fingerprint_text(builder, origin->owner_chain[index].name);
  }
  (void)fingerprint_text(builder, origin->declared_name);
  if (builder->invalid) return FINGERPRINT_INVALID;
  if (builder->output_length >
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES)
    return FINGERPRINT_INVALID;
  return FINGERPRINT_ENCODED;
}

static void nominal_origin_result_zero(
    w_seed_generic_nominal_origin_result *result) {
  if (result == NULL) return;
  (void)memset(result, 0, sizeof(*result));
  result->state = W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID;
}

/* Pointer arithmetic on unrelated caller-owned objects is not valid C.  Use
 * integer intervals for the preflight and conservatively reject an interval
 * whose endpoint would overflow. */
static bool byte_ranges_overlap(const void *left, size_t left_length,
                                const void *right, size_t right_length) {
  if (left == NULL || right == NULL || left_length == 0u ||
      right_length == 0u)
    return false;
  const uintptr_t left_start = (uintptr_t)left;
  const uintptr_t right_start = (uintptr_t)right;
  if (left_start > UINTPTR_MAX - left_length ||
      right_start > UINTPTR_MAX - right_length)
    return true;
  const uintptr_t left_end = left_start + left_length;
  const uintptr_t right_end = right_start + right_length;
  return left_start < right_end && right_start < left_end;
}

static bool nominal_origin_storage_overlaps(
    const w_seed_generic_nominal_origin *origin, const void *storage,
    size_t storage_length) {
  if (origin == NULL || storage == NULL || storage_length == 0u) return false;
  if (byte_ranges_overlap(storage, storage_length, origin, sizeof(*origin)))
    return true;
  /* Overlap preflight can run before the input status is published.  Never
   * multiply or iterate an untrusted caller count here. */
  const bool module_span_overflow =
      origin->module_path_segment_count >
      SIZE_MAX / sizeof(origin->module_path_segments[0]);
  const bool owner_span_overflow =
      origin->owner_chain_count > SIZE_MAX / sizeof(origin->owner_chain[0]);
  if (origin->module_path_segments == NULL ||
      (origin->owner_chain_count != 0u && origin->owner_chain == NULL))
    return true;
  if (byte_ranges_overlap(storage, storage_length, origin->authority_preimage,
                          origin->authority_preimage_length) ||
      byte_ranges_overlap(storage, storage_length,
                          origin->scoped_package_name.data,
                          origin->scoped_package_name.length) ||
      byte_ranges_overlap(storage, storage_length, origin->declared_name.data,
                          origin->declared_name.length))
    return true;
  if ((!module_span_overflow &&
       byte_ranges_overlap(
           storage, storage_length, origin->module_path_segments,
           origin->module_path_segment_count *
               sizeof(origin->module_path_segments[0]))) ||
      (module_span_overflow &&
       byte_ranges_overlap(storage, storage_length,
                           origin->module_path_segments,
                           sizeof(origin->module_path_segments[0]))) ||
      (!owner_span_overflow &&
       byte_ranges_overlap(storage, storage_length, origin->owner_chain,
                           origin->owner_chain_count *
                               sizeof(origin->owner_chain[0]))) ||
      (owner_span_overflow && origin->owner_chain != NULL &&
       byte_ranges_overlap(storage, storage_length, origin->owner_chain,
                           sizeof(origin->owner_chain[0]))))
    return true;
  /* The element ranges above are enough for framing safety.  Only supported
   * counts may be dereferenced for the finer text-level overlap checks. */
  if (origin->module_path_segment_count <=
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS)
    for (size_t index = 0u; index < origin->module_path_segment_count;
         index += 1u) {
      if (byte_ranges_overlap(storage, storage_length,
                              origin->module_path_segments[index].data,
                              origin->module_path_segments[index].length))
        return true;
    }
  if (origin->owner_chain_count <=
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_OWNER_CHAIN)
    for (size_t index = 0u; index < origin->owner_chain_count; index += 1u) {
      if (byte_ranges_overlap(storage, storage_length,
                              origin->owner_chain[index].name.data,
                              origin->owner_chain[index].name.length))
        return true;
    }
  return false;
}

static bool nominal_origin_output_preflight(
    const w_seed_generic_nominal_origin *origin, uint8_t *output,
    size_t output_capacity,
    const w_seed_generic_nominal_origin_result *result) {
  if (output == NULL || output_capacity == 0u) return false;
  return byte_ranges_overlap(output, output_capacity, result, sizeof(*result)) ||
         nominal_origin_storage_overlaps(origin, output, output_capacity);
}

w_seed_generic_nominal_origin_state w_seed_generic_nominal_origin_measure(
    const w_seed_generic_nominal_origin *origin,
    w_seed_generic_nominal_origin_result *result) {
  if (result == NULL) return W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID;
  const nominal_origin_input_status input_status =
      nominal_origin_encoded_size_status(origin);
  if (nominal_origin_storage_overlaps(origin, result, sizeof(*result)))
    return W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID;
  nominal_origin_result_zero(result);
  if (input_status == NOMINAL_ORIGIN_INPUT_INVALID)
    return W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID;
  if (input_status == NOMINAL_ORIGIN_INPUT_UNSUPPORTED) {
    result->state = W_SEED_GENERIC_NOMINAL_ORIGIN_UNSUPPORTED;
    return result->state;
  }
  fingerprint_builder builder;
  (void)memset(&builder, 0, sizeof(builder));
  w_seed_sha256_init(&builder.sha);
  const fingerprint_encode_status status = nominal_origin_encode(origin, &builder);
  if (status == FINGERPRINT_UNSUPPORTED || builder.unsupported) {
    result->state = W_SEED_GENERIC_NOMINAL_ORIGIN_UNSUPPORTED;
    return result->state;
  }
  if (status != FINGERPRINT_ENCODED || builder.invalid || builder.output_overflow) {
    result->state = W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID;
    return result->state;
  }
  result->bytes_required = builder.output_length;
  w_seed_sha256_final(&builder.sha, result->digest);
  result->state = W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE;
  return result->state;
}

w_seed_generic_nominal_origin_state w_seed_generic_nominal_origin_write(
    const w_seed_generic_nominal_origin *origin, uint8_t *output,
    size_t output_capacity, w_seed_generic_nominal_origin_result *result) {
  if (result == NULL) return W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID;
  const nominal_origin_input_status input_status =
      nominal_origin_encoded_size_status(origin);
  if (nominal_origin_storage_overlaps(origin, result, sizeof(*result)))
    return W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID;
  if ((output == NULL && output_capacity != 0u) ||
      nominal_origin_output_preflight(origin, output, output_capacity, result))
    return W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID;
  nominal_origin_result_zero(result);
  if (input_status == NOMINAL_ORIGIN_INPUT_INVALID)
    return W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID;
  if (input_status == NOMINAL_ORIGIN_INPUT_UNSUPPORTED) {
    result->state = W_SEED_GENERIC_NOMINAL_ORIGIN_UNSUPPORTED;
    return result->state;
  }
  w_seed_generic_nominal_origin_result measured;
  const w_seed_generic_nominal_origin_state measured_state =
      w_seed_generic_nominal_origin_measure(origin, &measured);
  if (measured_state != W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE) {
    *result = measured;
    return result->state;
  }
  *result = measured;
  if (output_capacity < measured.bytes_required) {
    result->state = W_SEED_GENERIC_NOMINAL_ORIGIN_CAPACITY;
    (void)memset(result->digest, 0, sizeof(result->digest));
    return result->state;
  }
  fingerprint_builder builder;
  (void)memset(&builder, 0, sizeof(builder));
  builder.output = output;
  builder.output_capacity = output_capacity;
  w_seed_sha256_init(&builder.sha);
  const fingerprint_encode_status status = nominal_origin_encode(origin, &builder);
  if (status != FINGERPRINT_ENCODED || builder.invalid || builder.unsupported ||
      builder.output_overflow || builder.output_length != measured.bytes_required) {
    /* The output is not exposed as a receipt.  A correct measurement keeps
     * this branch unreachable; callers still receive an explicit invalid
     * state rather than a partial publication. */
    result->state = W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID;
    result->bytes_written = 0u;
    result->bytes_required = 0u;
    (void)memset(result->digest, 0, sizeof(result->digest));
    return result->state;
  }
  w_seed_sha256_final(&builder.sha, result->digest);
  result->bytes_written = builder.output_length;
  result->state = W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE;
  return result->state;
}

static nominal_origin_parse_status nominal_origin_view_decode_and_digest(
    const w_seed_generic_nominal_origin_view *view,
    nominal_origin_decoded *decoded, bool verify_digest) {
  if (view == NULL || view->preimage == NULL || view->digest == NULL ||
      view->preimage_length == 0u)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  const nominal_origin_parse_status status = nominal_origin_parse(
      view->preimage, view->preimage_length, decoded);
  /* A structurally complete receipt may be feature-UNSUPPORTED, but its
   * supplied digest is still mandatory.  Only malformed framing skips the
   * hash because there is no parsed receipt to authenticate. */
  if (status == NOMINAL_ORIGIN_PARSE_INVALID || !verify_digest)
    return status;
  uint8_t digest[W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES];
  w_seed_sha256_state sha;
  w_seed_sha256_init(&sha);
  w_seed_sha256_update(&sha, view->preimage, view->preimage_length);
  w_seed_sha256_final(&sha, digest);
  if (memcmp(digest, view->digest,
             W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES) != 0)
    return NOMINAL_ORIGIN_PARSE_INVALID;
  return status;
}

bool w_seed_generic_nominal_origin_view_valid(
    const w_seed_generic_nominal_origin_view *view) {
  return nominal_origin_view_decode_and_digest(view, NULL, true) ==
         NOMINAL_ORIGIN_PARSE_AVAILABLE;
}

bool w_seed_generic_nominal_origin_equal(
    const w_seed_generic_nominal_origin_view *left,
    const w_seed_generic_nominal_origin_view *right) {
  /* Equality is collision-safe but does not use view_valid as a shortcut:
   * two valid preimages with a forced equal digest must still compare bytes. */
  if (left == NULL || right == NULL || left->preimage == NULL ||
      right->preimage == NULL || left->digest == NULL || right->digest == NULL ||
      left->preimage_length == 0u || right->preimage_length == 0u ||
      left->preimage_length >
          W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES ||
      right->preimage_length >
          W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES ||
      left->preimage_length != right->preimage_length ||
      memcmp(left->digest, right->digest,
             W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES) != 0)
    return false;
  if (nominal_origin_parse(left->preimage, left->preimage_length, NULL) !=
          NOMINAL_ORIGIN_PARSE_AVAILABLE ||
      nominal_origin_parse(right->preimage, right->preimage_length, NULL) !=
          NOMINAL_ORIGIN_PARSE_AVAILABLE)
    return false;
  return memcmp(left->preimage, right->preimage, left->preimage_length) == 0;
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

/* Encode the collision-safe semantic specialization identity.  The builder
 * can run once for measurement and once for caller-owned output. */
static fingerprint_encode_status specialization_application(
    const validation_context *context,
    const w_seed_frontend_generic_application *application,
    const w_seed_frontend_struct *head, const predicate_candidate *candidates,
    size_t candidate_count, const uint32_t *value_indices,
    fingerprint_builder *builder) {
  if (context == NULL || application == NULL || head == NULL ||
      builder == NULL || context->frontend == NULL ||
      context->frontend_result == NULL ||
      context->frontend->generic_arguments == NULL ||
      context->frontend->generic_parameters == NULL ||
      application->module_index >= context->frontend_result->written.modules ||
      application->argument_count != head->generic_parameter_count ||
      !range_valid(head->first_generic_parameter,
                   head->generic_parameter_count,
                   context->frontend_result->written.generic_parameters))
    return FINGERPRINT_INVALID;

  const w_seed_frontend_module *module =
      &context->frontend->modules[application->module_index];
  if (module->module_id.length == 0u || !text_valid(module->module_id) ||
      head->name.length == 0u || !text_valid(head->name))
    return FINGERPRINT_INVALID;
  fingerprint_bytes(builder, SPECIALIZATION_PREFIX,
                    sizeof(SPECIALIZATION_PREFIX) - 1u);
  fingerprint_u8(builder, SPECIALIZATION_ROOT_TAG);
  const w_seed_generic_nominal_origin_view *origin =
      context->input == NULL ? NULL : context->input->nominal_origin;
  if (origin == NULL || origin->preimage == NULL ||
      origin->preimage_length == 0u || origin->preimage_length > UINT32_MAX)
    return FINGERPRINT_INVALID;
  fingerprint_u8(builder, SPECIALIZATION_ORIGIN_TAG);
  fingerprint_u32(builder, (uint32_t)origin->preimage_length);
  fingerprint_bytes(builder, origin->preimage, origin->preimage_length);
  /* The origin receipt already carries declaration kind, module path, owners,
   * and declared name.  The specialization schema therefore starts its D8
   * parameter/refinement portion with a count only; it never repeats module
   * or head text outside the receipt. */
  fingerprint_u8(builder, 0x44u);
  fingerprint_u32(builder, head->generic_parameter_count);

  fingerprint_encode_status status = FINGERPRINT_ENCODED;

  for (uint32_t offset = 0u; offset < head->generic_parameter_count;
       offset += 1u) {
    const w_seed_frontend_generic_parameter *parameter = NULL;
    if (!parameter_relation_valid(context, application, offset, &parameter) ||
        parameter == NULL) {
      fingerprint_mark_unsupported(builder);
      return FINGERPRINT_UNSUPPORTED;
    }
    fingerprint_u8(builder, 0x50u);
    fingerprint_u32(builder, parameter->ordinal);
    if (parameter->kind == W_SEED_FRONTEND_GENERIC_KIND_TYPE) {
      fingerprint_u8(builder, 1u);
      fingerprint_u8(builder, 0u);
    } else if (parameter->kind == W_SEED_FRONTEND_GENERIC_KIND_VALUE) {
      fingerprint_u8(builder, 2u);
      if (parameter->domain_kind ==
          W_SEED_FRONTEND_GENERIC_DOMAIN_CONCRETE) {
        fingerprint_u8(builder, 1u);
        status = fingerprint_type(context, application->module_index,
                                  parameter->domain_type, builder, 1u);
        if (status != FINGERPRINT_ENCODED) return status;
      } else if (parameter->domain_kind ==
                 W_SEED_FRONTEND_GENERIC_DOMAIN_DEPENDENT) {
        fingerprint_u8(builder, 2u);
        fingerprint_u32(builder,
                        parameter->dependent_type_parameter_ordinal);
      } else {
        fingerprint_mark_unsupported(builder);
        return FINGERPRINT_UNSUPPORTED;
      }
    } else {
      fingerprint_mark_unsupported(builder);
      return FINGERPRINT_UNSUPPORTED;
    }
    if (parameter->refinement_kind ==
        W_SEED_FRONTEND_GENERIC_REFINEMENT_NONE) {
      fingerprint_u8(builder, 0u);
    } else if (parameter->refinement_kind ==
               W_SEED_FRONTEND_GENERIC_REFINEMENT_PREDICATE) {
      const predicate_candidate *candidate = fingerprint_candidate_for_argument(
          candidates, candidate_count,
          application->first_argument + offset);
      if (candidate == NULL || candidate->function == NULL) {
        fingerprint_mark_unsupported(builder);
        return FINGERPRINT_UNSUPPORTED;
      }
      fingerprint_u8(builder, 1u);
      fingerprint_bytes(builder, candidate->function->body_digest,
                        sizeof(candidate->function->body_digest));
    } else {
      fingerprint_mark_unsupported(builder);
      return FINGERPRINT_UNSUPPORTED;
    }
  }

  fingerprint_u8(builder, 0x53u);
  fingerprint_u32(builder, application->argument_count);
  for (uint32_t offset = 0u; offset < application->argument_count; offset += 1u) {
    const w_seed_frontend_generic_argument *argument =
        &context->frontend->generic_arguments[
            (size_t)application->first_argument + offset];
    const w_seed_frontend_generic_parameter *parameter = NULL;
    if (!parameter_relation_valid(context, application, offset, &parameter) ||
        parameter == NULL || argument->parameter_ordinal != offset)
      return FINGERPRINT_INVALID;
    fingerprint_u8(builder, 0x41u);
    fingerprint_u32(builder, offset);
    if (parameter->kind == W_SEED_FRONTEND_GENERIC_KIND_TYPE) {
      if (argument->kind != W_SEED_FRONTEND_GENERIC_ARGUMENT_TYPE ||
          argument->type_index == W_SEED_FRONTEND_NONE) {
        fingerprint_mark_unsupported(builder);
        return FINGERPRINT_UNSUPPORTED;
      }
      fingerprint_u8(builder, 1u);
      fingerprint_u8(builder, 0x54u);
      status = fingerprint_type(context, application->module_index,
                                argument->type_index, builder, 1u);
      if (status != FINGERPRINT_ENCODED) return status;
      continue;
    }
    if (parameter->kind != W_SEED_FRONTEND_GENERIC_KIND_VALUE) {
      fingerprint_mark_unsupported(builder);
      return FINGERPRINT_UNSUPPORTED;
    }
    fingerprint_u8(builder, 2u);
    fingerprint_u8(builder, 0x56u);
    uint32_t effective_domain_type = W_SEED_FRONTEND_NONE;
    if (!effective_domain_type_index(context, application, offset,
                                     &effective_domain_type) ||
        value_indices == NULL || value_indices[offset] == W_SEED_FRONTEND_NONE ||
        (size_t)value_indices[offset] >= context->arena_count)
      return FINGERPRINT_INVALID;
    status = fingerprint_type(context, application->module_index,
                              effective_domain_type, builder, 1u);
    if (status != FINGERPRINT_ENCODED) return status;
    status = fingerprint_constir_value(
        context, application->module_index,
        &context->input->conversion_values[value_indices[offset]],
        effective_domain_type, builder, 1u);
    if (status != FINGERPRINT_ENCODED) return status;
  }
  fingerprint_u8(builder, 0x57u);
  fingerprint_u32(builder, 0u);
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

const char *w_seed_generic_validation_specialization_state_name(
    w_seed_generic_validation_specialization_state state) {
  switch (state) {
    case W_SEED_GENERIC_VALIDATION_SPECIALIZATION_NOT_AVAILABLE:
      return "NOT_AVAILABLE";
    case W_SEED_GENERIC_VALIDATION_SPECIALIZATION_AVAILABLE:
      return "AVAILABLE";
    case W_SEED_GENERIC_VALIDATION_SPECIALIZATION_UNSUPPORTED:
      return "UNSUPPORTED";
    case W_SEED_GENERIC_VALIDATION_SPECIALIZATION_CAPACITY:
      return "CAPACITY";
    case W_SEED_GENERIC_VALIDATION_SPECIALIZATION_IDENTITY_REQUIRED:
      return "IDENTITY_REQUIRED";
  }
  return "UNKNOWN";
}

bool w_seed_generic_specialization_equal(
    const w_seed_generic_specialization_view *left,
    const w_seed_generic_specialization_view *right) {
  /* D8 canonical preimages are non-empty.  A zero/unavailable projection is
   * never an identity view and must not compare equal to another sentinel. */
  if (left == NULL || right == NULL || left->digest == NULL ||
      right->digest == NULL || left->preimage == NULL ||
      right->preimage == NULL || left->preimage_length == 0u ||
      right->preimage_length == 0u)
    return false;
  if (left->preimage_length != right->preimage_length ||
      memcmp(left->digest, right->digest,
             W_SEED_GENERIC_VALIDATION_SPECIALIZATION_DIGEST_BYTES) != 0)
    return false;
  return memcmp(left->preimage, right->preimage, left->preimage_length) == 0;
}

typedef enum {
  NOMINAL_ORIGIN_RELATION_MISSING = 0,
  NOMINAL_ORIGIN_RELATION_VALID,
  NOMINAL_ORIGIN_RELATION_UNSUPPORTED,
  NOMINAL_ORIGIN_RELATION_INVALID,
} nominal_origin_relation_status;

static bool nominal_origin_decoded_text_equal(const uint8_t *bytes,
                                              size_t length,
                                              w_seed_frontend_text text) {
  return text.length == length && text.data != NULL && bytes != NULL &&
         memcmp(bytes, text.data, length) == 0;
}

static bool nominal_origin_decoded_module_path_equal(
    const nominal_origin_decoded *decoded, w_seed_frontend_text module_id) {
  if (decoded == NULL || module_id.data == NULL || module_id.length == 0u)
    return false;
  size_t offset = 0u;
  for (size_t index = 0u; index < decoded->module_segment_count; index += 1u) {
    if (index != 0u) {
      if (offset >= module_id.length ||
          ((const uint8_t *)module_id.data)[offset] != (uint8_t)'.')
        return false;
      offset += 1u;
    }
    const size_t segment_length = decoded->module_segment_lengths[index];
    if (segment_length > module_id.length - offset ||
        memcmp(decoded->module_segments[index],
               (const uint8_t *)module_id.data + offset, segment_length) != 0)
      return false;
    offset += segment_length;
  }
  return decoded->module_segment_count != 0u && offset == module_id.length;
}

/* Bind a resolver receipt to the exact frontend module/head selected by the
 * application.  The module path is canonical data and is compared in full;
 * no allocation or last-segment shortcut is permitted. */
static nominal_origin_relation_status nominal_origin_relation_for_head(
    const validation_context *context,
    const w_seed_frontend_generic_application *application,
    const w_seed_frontend_struct *head) {
  if (context == NULL || context->input == NULL || application == NULL ||
      head == NULL)
    return NOMINAL_ORIGIN_RELATION_INVALID;
  const w_seed_generic_nominal_origin_view *view =
      context->input->nominal_origin;
  if (view == NULL) return NOMINAL_ORIGIN_RELATION_MISSING;
  if (view->preimage == NULL || view->digest == NULL ||
      view->preimage_length == 0u ||
      view->frontend_module_index != application->module_index ||
      view->frontend_head_struct_index != application->head_struct)
    return NOMINAL_ORIGIN_RELATION_INVALID;
  nominal_origin_decoded decoded;
  const nominal_origin_parse_status parse_status =
      nominal_origin_view_decode_and_digest(view, &decoded, true);
  if (parse_status == NOMINAL_ORIGIN_PARSE_INVALID)
    return NOMINAL_ORIGIN_RELATION_INVALID;
  if (parse_status == NOMINAL_ORIGIN_PARSE_UNSUPPORTED)
    return NOMINAL_ORIGIN_RELATION_UNSUPPORTED;
  if (context->frontend == NULL || context->frontend->modules == NULL ||
      context->frontend_result == NULL ||
      (size_t)application->module_index >= context->frontend_result->written.modules)
    return NOMINAL_ORIGIN_RELATION_INVALID;
  const w_seed_frontend_module *module =
      &context->frontend->modules[application->module_index];
  if (decoded.declaration_kind !=
          (uint8_t)W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT ||
      !nominal_origin_decoded_text_equal(decoded.declared_name,
                                         decoded.declared_name_length,
                                         head->name) ||
      decoded.module_segment_count == 0u ||
      !nominal_origin_decoded_module_path_equal(&decoded, module->module_id) ||
      decoded.owner_count != 0u)
    return NOMINAL_ORIGIN_RELATION_INVALID;
  return NOMINAL_ORIGIN_RELATION_VALID;
}

static bool validation_specialization_output_overlaps_input(
    const w_seed_generic_validation_input *input,
    const w_seed_generic_validation_result *result) {
  if (input == NULL || input->specialization_preimage == NULL ||
      input->specialization_preimage_capacity == 0u)
    return false;
  const uint8_t *output = input->specialization_preimage;
  const size_t capacity = input->specialization_preimage_capacity;
  if (byte_ranges_overlap(output, capacity, input, sizeof(*input)) ||
      byte_ranges_overlap(output, capacity, result, sizeof(*result)) ||
      byte_ranges_overlap(output, capacity, input->frontend_output,
                          sizeof(*input->frontend_output)) ||
      byte_ranges_overlap(output, capacity, input->frontend_result,
                          sizeof(*input->frontend_result)) ||
      byte_ranges_overlap(output, capacity, input->constir_program,
                          sizeof(*input->constir_program)) ||
      byte_ranges_overlap(output, capacity, input->nominal_origin,
                          sizeof(*input->nominal_origin)))
    return true;
  if (input->nominal_origin != NULL &&
      (byte_ranges_overlap(output, capacity,
                           input->nominal_origin->preimage,
                           input->nominal_origin->preimage_length) ||
       byte_ranges_overlap(
           output, capacity, input->nominal_origin->digest,
           W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES)))
    return true;
  if (input->conversion_values != NULL &&
      input->conversion_value_capacity <=
          SIZE_MAX / sizeof(input->conversion_values[0]) &&
      byte_ranges_overlap(
          output, capacity, input->conversion_values,
          input->conversion_value_capacity *
              sizeof(input->conversion_values[0])))
    return true;
  if (input->evidence_bytes != NULL &&
      byte_ranges_overlap(output, capacity, input->evidence_bytes,
                          input->evidence_byte_capacity))
    return true;
  if (input->receipts != NULL &&
      input->receipt_capacity <= SIZE_MAX / sizeof(input->receipts[0]) &&
      byte_ranges_overlap(output, capacity, input->receipts,
                          input->receipt_capacity * sizeof(input->receipts[0])))
    return true;
  return false;
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
  receipt->effective_type = W_SEED_FRONTEND_NONE;
}

static void publish_const_cycle_failure(
    const validation_context *context,
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
  if (argument->typed_const_expression_index != W_SEED_FRONTEND_NONE &&
      input->frontend_output->typed_const_expressions != NULL &&
      (size_t)argument->typed_const_expression_index <
          input->frontend_result->written.typed_const_expressions)
    receipt->effective_type =
        input->frontend_output
            ->typed_const_expressions[argument->typed_const_expression_index]
            .effective_type;
  if (receipt->effective_type == W_SEED_FRONTEND_NONE && context != NULL)
    (void)effective_domain_type_index(context, application, argument_offset,
                                       &receipt->effective_type);
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
  if (result == NULL) return W_SEED_GENERIC_VALIDATION_INVALID;
  if (input != NULL &&
      validation_specialization_output_overlaps_input(input, result))
    return W_SEED_GENERIC_VALIDATION_INVALID;
  (void)memset(result, 0, sizeof(*result));
  if (input == NULL) {
    result->state = W_SEED_GENERIC_VALIDATION_INVALID;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
    return W_SEED_GENERIC_VALIDATION_INVALID;
  }
  result->state = W_SEED_GENERIC_VALIDATION_INVALID;
  result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
  result->application_index = input->application_index;
  /* A non-zero capacity with no output storage is an invalid caller input.
   * Reject it before any frontend, ConstIR, evaluator, or receipt work.  The
   * zero-capacity/non-null case remains the normal specialization CAPACITY
   * projection after VERIFIED evaluation. */
  if (input->specialization_preimage == NULL &&
      input->specialization_preimage_capacity != 0u)
    return result->state;
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
  bool invalid_argument = false;
  if (!application_relations_valid(&context, &application, &head,
                                   &invalid_argument))
    return result->state;
  result->head_struct_index = application->head_struct;
  if (invalid_argument) return result->state;

  /* The resolver receipt is an integrity/relation gate, not a registry
   * lookup.  It must fail before graph, conversion, evaluator, or receipt
   * writes.  Missing origin is intentionally allowed for semantic validation
   * and becomes IDENTITY_REQUIRED only after VERIFIED. */
  const nominal_origin_relation_status origin_relation =
      nominal_origin_relation_for_head(&context, application, head);
  if (origin_relation == NOMINAL_ORIGIN_RELATION_INVALID) {
    result->state = W_SEED_GENERIC_VALIDATION_INVALID;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
    return result->state;
  }
  const bool origin_missing =
      origin_relation == NOMINAL_ORIGIN_RELATION_MISSING;
  const bool origin_unsupported =
      origin_relation == NOMINAL_ORIGIN_RELATION_UNSUPPORTED;

  /* A frontend UNSUPPORTED application can still contain a normalized D7
   * expression graph.  Check that graph after the complete application
   * relation, but before the binding barrier.  INVALID applications and
   * arguments keep their invalid-input barrier. */
  if (application->binding_status ==
      W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED) {
    uint32_t early_cycle_path[W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH];
    size_t early_cycle_path_length = 0u;
    uint32_t early_cycle_argument_offset = W_SEED_FRONTEND_NONE;
    size_t early_calculated_argument_count = 0u;
    const const_graph_status early_graph_status =
        frontend_const_graph_preflight(
            &context, application, NULL, true, early_cycle_path,
            &early_cycle_path_length, &early_cycle_argument_offset,
            &early_calculated_argument_count);
    if (early_graph_status == CONST_GRAPH_INVALID) {
      result->state = W_SEED_GENERIC_VALIDATION_INVALID;
      result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
      return result->state;
    }
    if (early_graph_status == CONST_GRAPH_CYCLE) {
      /* Publish every recovered D7 argument before the capacity-dependent
       * causal receipt write. */
      result->computed_argument_count = early_calculated_argument_count;
      result->state = W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED;
      result->failure =
          W_SEED_GENERIC_VALIDATION_FAILURE_EVALUATOR_DIAGNOSTIC;
      result->const_cycle_path_length = early_cycle_path_length;
      if (early_cycle_path_length != 0u)
        (void)memcpy(result->const_cycle_path, early_cycle_path,
                     early_cycle_path_length * sizeof(early_cycle_path[0]));
      result->diagnostic = W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002;
      result->diagnostic_span = application->span;
      publish_const_cycle_failure(
          &context, input, application, early_cycle_argument_offset,
          early_cycle_path, early_cycle_path_length, result);
      return result->state;
    }
    if (early_graph_status == CONST_GRAPH_LIMIT) {
      result->state = W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
      result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_DEPENDENCY_LIMIT;
      return result->state;
    }
  }
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
    candidate_count += 1u;
  }
  result->predicate_count = candidate_count;
  /* A D4 dependency graph is checked before any conversion, receipt-capacity,
   * quota, or evaluator decision.  This gives a reachable cycle its own
   * deterministic diagnostic even when a caller supplied zero capacity. */
  uint32_t cycle_path[W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH];
  size_t cycle_path_length = 0u;
  uint32_t cycle_argument_offset = W_SEED_FRONTEND_NONE;
  const const_graph_status frontend_graph_status =
      frontend_const_graph_preflight(
          &context, application, typed_function_indices, false, cycle_path,
          &cycle_path_length, &cycle_argument_offset, NULL);
  if (frontend_graph_status == CONST_GRAPH_INVALID) {
    result->state = W_SEED_GENERIC_VALIDATION_INVALID;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT;
    return result->state;
  }
  if (frontend_graph_status == CONST_GRAPH_UNSUPPORTED) {
    result->state = W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_FUNCTION;
    return result->state;
  }
  if (frontend_graph_status == CONST_GRAPH_LIMIT) {
    result->state = W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_DEPENDENCY_LIMIT;
    return result->state;
  }
  if (frontend_graph_status == CONST_GRAPH_CYCLE) {
    result->state = W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED;
    result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_EVALUATOR_DIAGNOSTIC;
    result->const_cycle_path_length = cycle_path_length;
    if (cycle_path_length != 0u)
      (void)memcpy(result->const_cycle_path, cycle_path,
                   cycle_path_length * sizeof(cycle_path[0]));
    result->diagnostic = W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002;
    result->diagnostic_span = application->span;
    publish_const_cycle_failure(&context, input, application,
                                cycle_argument_offset, cycle_path,
                                cycle_path_length, result);
    return result->state;
  }
  for (uint32_t offset = 0u; offset < application->argument_count; offset += 1u) {
    const uint32_t function_index = typed_function_indices[offset];
    if (function_index == W_SEED_CONSTIR_NONE) continue;
    if ((size_t)function_index >= context.program->function_count ||
        !context.program->functions[function_index].lowerable) {
      result->state = W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
      result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_FUNCTION;
      return result->state;
    }
  }
  for (size_t index = 0u; index < candidate_count; index += 1u) {
    if (!candidates[index].function->lowerable) {
      result->state = W_SEED_GENERIC_VALIDATION_UNSUPPORTED;
      result->failure = W_SEED_GENERIC_VALIDATION_FAILURE_FUNCTION;
      return result->state;
    }
  }
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
    publish_const_cycle_failure(&context, input, application,
                                cycle_argument_offset, cycle_path,
                                cycle_path_length, result);
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
  w_seed_constir_session session;
  w_seed_constir_session_init(&session);
  /* Evaluate calculated values in argument order.  Immediate values are
   * converted into the same bounded arena and receive the same normalized
   * value representation.  The private session is alive only for this loop. */
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
      const w_seed_constir_status status = w_seed_constir_evaluate_in_session(
          context.program, (uint32_t)function_index, NULL, 0u,
          remaining, input->eval_workspace, &session, &value, &evaluation);
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
      receipt->effective_type = value.type_index;
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
      receipt->effective_type = receipt->eval_value.type_index;
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
    receipt->effective_type = candidate->effective_domain_type_index;
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

  if (origin_missing) {
    result->specialization_state =
        W_SEED_GENERIC_VALIDATION_SPECIALIZATION_IDENTITY_REQUIRED;
    result->specialization_bytes_written = 0u;
    result->specialization_bytes_required = 0u;
    (void)memset(result->specialization_digest, 0,
                 sizeof(result->specialization_digest));
    return result->state;
  }
  if (origin_unsupported) {
    result->specialization_state =
        W_SEED_GENERIC_VALIDATION_SPECIALIZATION_UNSUPPORTED;
    return result->state;
  }

  /* The specialization preimage has a separate schema and output contract.
   * Measure before writing so a short caller buffer stays untouched. */
  fingerprint_builder specialization_measure;
  (void)memset(&specialization_measure, 0, sizeof(specialization_measure));
  w_seed_sha256_init(&specialization_measure.sha);
  const fingerprint_encode_status specialization_status =
      specialization_application(&context, application, head, candidates,
                                 candidate_count, value_indices,
                                 &specialization_measure);
  if (specialization_status != FINGERPRINT_ENCODED ||
      specialization_measure.invalid || specialization_measure.unsupported) {
    result->specialization_state =
        W_SEED_GENERIC_VALIDATION_SPECIALIZATION_UNSUPPORTED;
    return result->state;
  }
  result->specialization_bytes_required =
      specialization_measure.output_length;
  if (result->specialization_bytes_required == 0u) {
    result->specialization_state =
        W_SEED_GENERIC_VALIDATION_SPECIALIZATION_UNSUPPORTED;
    result->specialization_bytes_required = 0u;
    (void)memset(result->specialization_digest, 0,
                 sizeof(result->specialization_digest));
    return result->state;
  }
  if (input->specialization_preimage_capacity <
      result->specialization_bytes_required) {
    result->specialization_state =
        W_SEED_GENERIC_VALIDATION_SPECIALIZATION_CAPACITY;
    result->specialization_bytes_written = 0u;
    (void)memset(result->specialization_digest, 0,
                 sizeof(result->specialization_digest));
    return result->state;
  }
  fingerprint_builder specialization_write;
  (void)memset(&specialization_write, 0, sizeof(specialization_write));
  specialization_write.output = input->specialization_preimage;
  specialization_write.output_capacity =
      input->specialization_preimage_capacity;
  w_seed_sha256_init(&specialization_write.sha);
  const fingerprint_encode_status write_status = specialization_application(
      &context, application, head, candidates, candidate_count, value_indices,
      &specialization_write);
  if (write_status != FINGERPRINT_ENCODED || specialization_write.invalid ||
      specialization_write.unsupported || specialization_write.output_overflow ||
      specialization_write.output_length !=
          result->specialization_bytes_required) {
    result->specialization_state =
        W_SEED_GENERIC_VALIDATION_SPECIALIZATION_UNSUPPORTED;
    result->specialization_bytes_written = 0u;
    result->specialization_bytes_required = 0u;
    (void)memset(result->specialization_digest, 0,
                 sizeof(result->specialization_digest));
    return result->state;
  }
  w_seed_sha256_final(&specialization_write.sha,
                      result->specialization_digest);
  result->specialization_bytes_written = specialization_write.output_length;
  result->specialization_state =
      W_SEED_GENERIC_VALIDATION_SPECIALIZATION_AVAILABLE;
  return result->state;
}
