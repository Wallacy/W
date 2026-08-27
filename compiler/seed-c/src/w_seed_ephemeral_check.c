#include "w_seed_ephemeral_check.h"

#include <string.h>

enum { W_SEED_EPHEMERAL_CHECK_MAX_RANGES = 2048 };

typedef struct {
  uintptr_t begin;
  uintptr_t end;
} check_range;

typedef struct {
  check_range values[W_SEED_EPHEMERAL_CHECK_MAX_RANGES];
  size_t count;
} check_ranges;

static bool range_add(check_ranges *ranges, const void *pointer, size_t count,
                      size_t element_size) {
  if (ranges == NULL || element_size == 0u) return false;
  if (count == 0u) return true;
  if (pointer == NULL || count > SIZE_MAX / element_size) return false;
  if (ranges->count >= W_SEED_EPHEMERAL_CHECK_MAX_RANGES) return false;

  const size_t byte_count = count * element_size;
  const uintptr_t begin = (uintptr_t)pointer;
  if ((uintmax_t)byte_count >
      (uintmax_t)UINTPTR_MAX - (uintmax_t)begin)
    return false;
  ranges->values[ranges->count] =
      (check_range){begin, begin + (uintptr_t)byte_count};
  ranges->count += 1u;
  return true;
}

static bool ranges_overlap(check_range first, check_range second) {
  return first.begin < second.end && second.begin < first.end;
}

static bool ranges_are_disjoint(const check_ranges *ranges) {
  if (ranges == NULL) return false;
  for (size_t first = 0u; first < ranges->count; first += 1u) {
    for (size_t second = first + 1u; second < ranges->count; second += 1u) {
      if (ranges_overlap(ranges->values[first], ranges->values[second]))
        return false;
    }
  }
  return true;
}

static bool add_frontend_ranges(check_ranges *ranges,
                                const w_seed_frontend_output *output) {
  if (ranges == NULL || output == NULL) return false;
#define ADD_FRONTEND_RANGE(field, capacity_field, type)                       \
  do {                                                                         \
    if (!range_add(ranges, output->field, output->capacity_field,             \
                   sizeof(type)))                                             \
      return false;                                                            \
  } while (0)
  ADD_FRONTEND_RANGE(modules, module_capacity, w_seed_frontend_module);
  ADD_FRONTEND_RANGE(imports, import_capacity, w_seed_frontend_import);
  ADD_FRONTEND_RANGE(import_items, import_item_capacity,
                     w_seed_frontend_import_item);
  ADD_FRONTEND_RANGE(structs, struct_capacity, w_seed_frontend_struct);
  ADD_FRONTEND_RANGE(fields, field_capacity, w_seed_frontend_field);
  ADD_FRONTEND_RANGE(type_declarations, type_declaration_capacity,
                     w_seed_frontend_type_declaration);
  ADD_FRONTEND_RANGE(aliases, alias_capacity, w_seed_frontend_alias);
  ADD_FRONTEND_RANGE(types, type_capacity, w_seed_frontend_type);
  ADD_FRONTEND_RANGE(functions, function_capacity, w_seed_frontend_function);
  ADD_FRONTEND_RANGE(parameters, parameter_capacity,
                     w_seed_frontend_parameter);
  ADD_FRONTEND_RANGE(arguments, argument_capacity, w_seed_frontend_argument);
  ADD_FRONTEND_RANGE(entries, entry_capacity, w_seed_frontend_entry);
  ADD_FRONTEND_RANGE(statements, statement_capacity,
                     w_seed_frontend_statement);
  ADD_FRONTEND_RANGE(expressions, expression_capacity,
                     w_seed_frontend_expression);
  ADD_FRONTEND_RANGE(symbols, symbol_capacity, w_seed_frontend_symbol);
  ADD_FRONTEND_RANGE(facts, fact_capacity, w_seed_frontend_fact);
  ADD_FRONTEND_RANGE(diagnostics, diagnostic_capacity,
                     w_seed_frontend_diagnostic);
  ADD_FRONTEND_RANGE(receipt, receipt_capacity, uint8_t);
  ADD_FRONTEND_RANGE(enums, enum_capacity, w_seed_frontend_enum);
  ADD_FRONTEND_RANGE(enum_cases, enum_case_capacity,
                     w_seed_frontend_enum_case);
  ADD_FRONTEND_RANGE(enum_case_parameters, enum_case_parameter_capacity,
                     w_seed_frontend_enum_case_parameter);
  ADD_FRONTEND_RANGE(const_declarations, const_declaration_capacity,
                     w_seed_frontend_const_declaration);
  ADD_FRONTEND_RANGE(switch_arms, switch_arm_capacity,
                     w_seed_frontend_switch_arm);
  ADD_FRONTEND_RANGE(enum_subset_members, enum_subset_member_capacity,
                     w_seed_frontend_enum_subset_member);
  ADD_FRONTEND_RANGE(enum_membership_cases,
                     enum_membership_case_capacity,
                     w_seed_frontend_enum_membership_case);
  ADD_FRONTEND_RANGE(generic_parameters, generic_parameter_capacity,
                     w_seed_frontend_generic_parameter);
  ADD_FRONTEND_RANGE(generic_applications,
                     generic_application_capacity,
                     w_seed_frontend_generic_application);
  ADD_FRONTEND_RANGE(generic_arguments, generic_argument_capacity,
                     w_seed_frontend_generic_argument);
  ADD_FRONTEND_RANGE(typed_const_expressions,
                     typed_const_expression_capacity,
                     w_seed_frontend_typed_const_expression);
  ADD_FRONTEND_RANGE(const_values, const_value_capacity,
                     w_seed_frontend_const_value);
  ADD_FRONTEND_RANGE(const_elements, const_element_capacity,
                     w_seed_frontend_const_element);
  ADD_FRONTEND_RANGE(const_bytes, const_bytes_capacity, uint8_t);
#undef ADD_FRONTEND_RANGE
  return true;
}

static bool add_graph_scratch_ranges(check_ranges *ranges,
                                     const w_seed_ephemeral_graph_scratch *scratch) {
  if (ranges == NULL || scratch == NULL) return false;
#define ADD_GRAPH_RANGE(field, capacity_field, type)                          \
  do {                                                                         \
    if (!range_add(ranges, scratch->field, scratch->capacity_field,            \
                   sizeof(type)))                                             \
      return false;                                                            \
  } while (0)
  ADD_GRAPH_RANGE(nodes, node_capacity, w_seed_ephemeral_graph_scratch_node);
  ADD_GRAPH_RANGE(edges, edge_capacity, w_seed_ephemeral_graph_scratch_edge);
  ADD_GRAPH_RANGE(sorted_nodes, sorted_nodes_capacity, size_t);
  ADD_GRAPH_RANGE(node_ordinals, node_ordinals_capacity, size_t);
  ADD_GRAPH_RANGE(sorted_edges, sorted_edges_capacity, size_t);
  ADD_GRAPH_RANGE(sorted_resolved_edges, sorted_resolved_edges_capacity, size_t);
  ADD_GRAPH_RANGE(origins, origin_capacity, w_seed_module_origin);
  ADD_GRAPH_RANGE(indegree, indegree_capacity, uint32_t);
  ADD_GRAPH_RANGE(queue, queue_capacity, uint32_t);
  ADD_GRAPH_RANGE(depths, depths_capacity, uint32_t);
#undef ADD_GRAPH_RANGE
  return true;
}

static bool add_driver_ranges(check_ranges *ranges,
                              const w_seed_ephemeral_check_input *input) {
  if (ranges == NULL || input == NULL || input->driver_input == NULL ||
      input->driver_scratch == NULL || input->driver_staging_output == NULL)
    return false;
  const w_seed_ephemeral_driver_input *driver_input = input->driver_input;
  w_seed_ephemeral_driver_scratch *scratch = input->driver_scratch;
  w_seed_ephemeral_driver_output *output = input->driver_staging_output;
  if (scratch->slot_capacity > W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES ||
      scratch->request_capacity > W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES)
    return false;

  if (!range_add(ranges, driver_input, 1u, sizeof(*driver_input)) ||
      !range_add(ranges, scratch, 1u, sizeof(*scratch)) ||
      !range_add(ranges, output, 1u, sizeof(*output)) ||
      !range_add(ranges, driver_input->root_path.data,
                 driver_input->root_path.length, sizeof(uint8_t)) ||
      !range_add(ranges, driver_input->root_source_id.data,
                 driver_input->root_source_id.length, sizeof(char)) ||
      !range_add(ranges, scratch->slots, scratch->slot_capacity,
                 sizeof(*scratch->slots)) ||
      !range_add(ranges, scratch->requests, scratch->request_capacity,
                 sizeof(*scratch->requests)) ||
      !range_add(ranges, scratch->lexer_frames, scratch->lexer_frame_capacity,
                 sizeof(*scratch->lexer_frames)) ||
      !range_add(ranges, scratch->tokens, scratch->token_capacity,
                 sizeof(*scratch->tokens)) ||
      !range_add(ranges, scratch->parse_frames, scratch->parse_frame_capacity,
                 sizeof(*scratch->parse_frames)) ||
      !range_add(ranges, scratch->issues, scratch->issue_capacity,
                 sizeof(*scratch->issues)) ||
      !range_add(ranges, scratch->origins, scratch->origin_capacity,
                 sizeof(*scratch->origins)) ||
      !range_add(ranges, scratch->candidate_documents,
                 scratch->candidate_document_capacity,
                 sizeof(*scratch->candidate_documents)) ||
      !range_add(ranges, scratch->candidate_facts, scratch->candidate_fact_capacity,
                 sizeof(*scratch->candidate_facts)) ||
      !range_add(ranges, scratch->graph_scratch, 1u,
                 sizeof(*scratch->graph_scratch)) ||
      !add_graph_scratch_ranges(ranges, scratch->graph_scratch) ||
      !range_add(ranges, output->graph.inventory, output->graph.inventory_capacity,
                 sizeof(*output->graph.inventory)) ||
      !range_add(ranges, output->graph.edges, output->graph.edge_capacity,
                 sizeof(*output->graph.edges)) ||
      !range_add(ranges, output->graph.document_order,
                 output->graph.document_order_capacity,
                 sizeof(*output->graph.document_order)) ||
      !range_add(ranges, output->graph.resolved_imports,
                 output->graph.resolved_import_capacity,
                 sizeof(*output->graph.resolved_imports)) ||
      !range_add(ranges, output->documents, output->document_capacity,
                 sizeof(*output->documents)))
    return false;

  for (size_t index = 0u; index < scratch->slot_capacity; index += 1u) {
    const w_seed_ephemeral_driver_slot *slot = &scratch->slots[index];
    if (!range_add(ranges, slot->source_id_storage, slot->source_id_capacity,
                   sizeof(char)) ||
        !range_add(ranges, slot->module_id_storage, slot->module_id_capacity,
                   sizeof(char)) ||
        !range_add(ranges, slot->nodes, slot->node_capacity,
                   sizeof(*slot->nodes)))
      return false;
  }
  for (size_t index = 0u; index < scratch->request_capacity; index += 1u) {
    const w_seed_ephemeral_provider_request *request =
        &scratch->requests[index];
    const w_seed_ephemeral_provider_token_buffers *tokens[] = {
        &request->tokens, &request->revalidation_tokens};
    if (!range_add(ranges, request->staging_bytes, request->staging_capacity,
                   sizeof(uint8_t)) ||
        !range_add(ranges, request->revalidation_bytes,
                   request->revalidation_capacity, sizeof(uint8_t)) ||
        !range_add(ranges, request->bytes, request->byte_capacity,
                   sizeof(uint8_t)))
      return false;
    for (size_t token_set = 0u; token_set < 2u; token_set += 1u) {
      const w_seed_ephemeral_provider_token_buffers *token = tokens[token_set];
      if (!range_add(ranges, token->provider_id, token->provider_id_capacity,
                     sizeof(char)) ||
          !range_add(ranges, token->root_token, token->root_token_capacity,
                     sizeof(char)) ||
          !range_add(ranges, token->source_provider_owner_token,
                     token->source_provider_owner_token_capacity,
                     sizeof(char)) ||
          !range_add(ranges, token->canonical_token,
                     token->canonical_token_capacity, sizeof(char)))
        return false;
    }
  }
  return true;
}

static bool add_all_ranges(check_ranges *ranges,
                           const w_seed_ephemeral_check_input *input,
                           const w_seed_ephemeral_check_output *output) {
  if (ranges == NULL || input == NULL || output == NULL ||
      input->frontend_staging_output == NULL) return false;
  if (!range_add(ranges, input, 1u, sizeof(*input)) ||
      !range_add(ranges, output, 1u, sizeof(*output)) ||
      !add_driver_ranges(ranges, input) ||
      !range_add(ranges, input->frontend_staging_output, 1u,
                 sizeof(*input->frontend_staging_output)) ||
      !add_frontend_ranges(ranges, input->frontend_staging_output) ||
      !range_add(ranges, input->instance, input->instance_length,
                 sizeof(char)) ||
      !range_add(ranges, input->json_staging, input->json_staging_capacity,
                 sizeof(uint8_t)) ||
      !range_add(ranges, output->jsonl, output->jsonl_capacity,
                 sizeof(uint8_t)))
    return false;
  return ranges_are_disjoint(ranges);
}

static bool instance_seed_valid(const char *instance, size_t length) {
  if (instance == NULL || length != 7u || instance[0] != 'D') return false;
  for (size_t index = 1u; index < length; index += 1u) {
    if (instance[index] < '0' || instance[index] > '9') return false;
  }
  return true;
}

static bool make_instance(const char *seed, size_t seed_length, size_t ordinal,
                          char instance[8]) {
  if (instance == NULL || !instance_seed_valid(seed, seed_length)) return false;
  size_t value = 0u;
  for (size_t index = 1u; index < seed_length; index += 1u) {
    const size_t digit = (size_t)(seed[index] - '0');
    if (value > (999999u - digit) / 10u) return false;
    value = value * 10u + digit;
  }
  if (ordinal > 999999u - value) return false;
  (void)memcpy(instance, seed, seed_length);
  value += ordinal;
  for (size_t index = 6u; index > 0u; index -= 1u) {
    instance[index] = (char)('0' + (value % 10u));
    value /= 10u;
  }
  instance[7] = '\0';
  return true;
}

static void result_clear(w_seed_ephemeral_check_result *result) {
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_EPHEMERAL_CHECK_INVALID;
  result->phase = W_SEED_EPHEMERAL_CHECK_PHASE_NONE;
  result->diagnostic_index = SIZE_MAX;
  result->driver_status = W_SEED_EPHEMERAL_DRIVER_INVALID;
  result->frontend_status = W_SEED_FRONTEND_INVALID;
  result->diagnostic_status = W_SEED_DIAGNOSTIC_NO_RECORD;
}

static w_seed_ephemeral_check_status map_driver_status(
    w_seed_ephemeral_driver_status status) {
  switch (status) {
    case W_SEED_EPHEMERAL_DRIVER_OK:
      return W_SEED_EPHEMERAL_CHECK_OK;
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY:
      return W_SEED_EPHEMERAL_CHECK_CAPACITY;
    case W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED:
      return W_SEED_EPHEMERAL_CHECK_UNSUPPORTED;
    case W_SEED_EPHEMERAL_DRIVER_IO:
      return W_SEED_EPHEMERAL_CHECK_IO;
    case W_SEED_EPHEMERAL_DRIVER_INVALID:
    default:
      return W_SEED_EPHEMERAL_CHECK_INVALID;
  }
}

static w_seed_ephemeral_check_status map_frontend_status(
    w_seed_frontend_status status) {
  switch (status) {
    case W_SEED_FRONTEND_OK:
    case W_SEED_FRONTEND_DIAGNOSTICS:
      return W_SEED_EPHEMERAL_CHECK_OK;
    case W_SEED_FRONTEND_CAPACITY:
      return W_SEED_EPHEMERAL_CHECK_CAPACITY;
    case W_SEED_FRONTEND_UNSUPPORTED:
      return W_SEED_EPHEMERAL_CHECK_UNSUPPORTED;
    case W_SEED_FRONTEND_BARRIER:
    case W_SEED_FRONTEND_INVALID:
    default:
      return W_SEED_EPHEMERAL_CHECK_INVALID;
  }
}

static w_seed_ephemeral_check_status map_diagnostic_status(
    w_seed_diagnostic_status status) {
  switch (status) {
    case W_SEED_DIAGNOSTIC_OK:
      return W_SEED_EPHEMERAL_CHECK_OK;
    case W_SEED_DIAGNOSTIC_CAPACITY:
      return W_SEED_EPHEMERAL_CHECK_CAPACITY;
    case W_SEED_DIAGNOSTIC_UNSUPPORTED:
      return W_SEED_EPHEMERAL_CHECK_UNSUPPORTED;
    case W_SEED_DIAGNOSTIC_INVALID:
      return W_SEED_EPHEMERAL_CHECK_INVALID;
    case W_SEED_DIAGNOSTIC_NO_RECORD:
    default:
      return W_SEED_EPHEMERAL_CHECK_INVALID;
  }
}

static w_seed_ephemeral_check_status fail_result(
    w_seed_ephemeral_check_result *result,
    w_seed_ephemeral_check_status status,
    w_seed_ephemeral_check_failure failure,
    w_seed_ephemeral_check_phase phase) {
  result->status = status;
  result->failure = failure;
  result->phase = phase;
  return status;
}

static bool size_add(size_t first, size_t second, size_t *sum) {
  if (sum == NULL || first > SIZE_MAX - second) return false;
  *sum = first + second;
  return true;
}

w_seed_ephemeral_check_status w_seed_ephemeral_check_run(
    const w_seed_ephemeral_check_input *input,
    w_seed_ephemeral_check_output *output,
    w_seed_ephemeral_check_result *result) {
  if (input == NULL || output == NULL || result == NULL) {
    return W_SEED_EPHEMERAL_CHECK_INVALID;
  }

  check_ranges ranges = {0};
  if (!add_all_ranges(&ranges, input, output))
    return W_SEED_EPHEMERAL_CHECK_INVALID;
  check_ranges result_ranges = {0};
  if (!range_add(&result_ranges, result, 1u, sizeof(*result)))
    return W_SEED_EPHEMERAL_CHECK_INVALID;
  const check_range result_range = result_ranges.values[0];
  for (size_t index = 0u; index < ranges.count; index += 1u) {
    if (ranges_overlap(result_range, ranges.values[index]))
      return W_SEED_EPHEMERAL_CHECK_INVALID;
  }

  result_clear(result);
  result->phase = W_SEED_EPHEMERAL_CHECK_PHASE_VALIDATE;
  if (!instance_seed_valid(input->instance, input->instance_length)) {
    result->failure = W_SEED_EPHEMERAL_CHECK_FAILURE_INSTANCE;
    return W_SEED_EPHEMERAL_CHECK_INVALID;
  }

  w_seed_ephemeral_driver_result driver_result = {0};
  const w_seed_ephemeral_driver_status driver_status =
      w_seed_ephemeral_driver_run(input->driver_input, input->driver_scratch,
                                   input->driver_staging_output,
                                   &driver_result);
  result->driver_status = driver_status;
  result->driver_result = driver_result;
  result->phase = W_SEED_EPHEMERAL_CHECK_PHASE_DRIVER;
  if (driver_status != W_SEED_EPHEMERAL_DRIVER_OK) {
    result->required_capacity = driver_result.required_capacity;
    return fail_result(result, map_driver_status(driver_status),
                       W_SEED_EPHEMERAL_CHECK_FAILURE_DRIVER,
                       W_SEED_EPHEMERAL_CHECK_PHASE_DRIVER);
  }

  const size_t resolved_import_count = driver_result.graph_result.written.edges;
  if (input->driver_staging_output->document_count >
          input->driver_staging_output->document_capacity ||
      resolved_import_count >
          input->driver_staging_output->graph.resolved_import_capacity) {
    return fail_result(result, W_SEED_EPHEMERAL_CHECK_INVALID,
                       W_SEED_EPHEMERAL_CHECK_FAILURE_DRIVER,
                       W_SEED_EPHEMERAL_CHECK_PHASE_DRIVER);
  }
  const w_seed_frontend_input frontend_input = {
      input->driver_staging_output->documents,
      input->driver_staging_output->document_count,
      NULL,
      0u,
      true,
      input->driver_staging_output->graph.resolved_imports,
      resolved_import_count};

  w_seed_frontend_counts frontend_counts = {0};
  w_seed_frontend_result frontend_result = {0};
  const w_seed_frontend_status measure_status = w_seed_frontend_measure(
      &frontend_input, &frontend_counts, &frontend_result);
  result->frontend_status = measure_status;
  result->frontend_result = frontend_result;
  result->phase = W_SEED_EPHEMERAL_CHECK_PHASE_FRONTEND_MEASURE;
  if (measure_status != W_SEED_FRONTEND_OK) {
    result->required_capacity = frontend_result.required.receipt_bytes;
    return fail_result(result, map_frontend_status(measure_status),
                       W_SEED_EPHEMERAL_CHECK_FAILURE_FRONTEND,
                       W_SEED_EPHEMERAL_CHECK_PHASE_FRONTEND_MEASURE);
  }

  const w_seed_frontend_status frontend_status = w_seed_frontend_run(
      &frontend_input, input->frontend_staging_output, &frontend_result);
  result->frontend_status = frontend_status;
  result->frontend_result = frontend_result;
  result->phase = W_SEED_EPHEMERAL_CHECK_PHASE_FRONTEND_RUN;
  if (frontend_status != W_SEED_FRONTEND_OK &&
      frontend_status != W_SEED_FRONTEND_DIAGNOSTICS) {
    result->required_capacity = frontend_result.required.receipt_bytes;
    return fail_result(result, map_frontend_status(frontend_status),
                       W_SEED_EPHEMERAL_CHECK_FAILURE_FRONTEND,
                       W_SEED_EPHEMERAL_CHECK_PHASE_FRONTEND_RUN);
  }

  const size_t diagnostic_count = frontend_result.written.diagnostics;
  size_t required_json = 0u;
  w_seed_diagnostic_result diagnostic_result = {0};
  diagnostic_result.status = W_SEED_DIAGNOSTIC_NO_RECORD;
  for (size_t index = 0u; index < diagnostic_count; index += 1u) {
    result->diagnostic_index = index;
    const w_seed_frontend_diagnostic *diagnostic =
        &input->frontend_staging_output->diagnostics[index];
    if (diagnostic->document_index >= frontend_input.document_count) {
      diagnostic_result.status = W_SEED_DIAGNOSTIC_UNSUPPORTED;
      result->diagnostic_status = diagnostic_result.status;
      result->diagnostic_result = diagnostic_result;
      result->phase = W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE;
      return fail_result(result, W_SEED_EPHEMERAL_CHECK_UNSUPPORTED,
                         W_SEED_EPHEMERAL_CHECK_FAILURE_DIAGNOSTIC,
                         W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE);
    }
    char instance[8];
    if (!make_instance(input->instance, input->instance_length, index,
                       instance)) {
      result->phase = W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE;
      return fail_result(result, W_SEED_EPHEMERAL_CHECK_CAPACITY,
                         W_SEED_EPHEMERAL_CHECK_FAILURE_INSTANCE,
                         W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE);
    }
    const w_seed_frontend_document *document =
        &frontend_input.documents[diagnostic->document_index];
    diagnostic_result = (w_seed_diagnostic_result){0};
    const w_seed_diagnostic_status status = w_seed_diagnostic_frontend_record(
        instance, sizeof(instance) - 1u, document->logical_source_id.data,
        document->logical_source_id.length, document->source,
        diagnostic->document_index, diagnostic, NULL, 0u,
        &diagnostic_result);
    result->diagnostic_status = status;
    result->diagnostic_result = diagnostic_result;
    result->phase = W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE;
    if (status != W_SEED_DIAGNOSTIC_CAPACITY) {
      const w_seed_ephemeral_check_status mapped = map_diagnostic_status(status);
      return fail_result(result, mapped,
                         W_SEED_EPHEMERAL_CHECK_FAILURE_DIAGNOSTIC,
                         W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE);
    }
    size_t record_bytes = 0u;
    if (!size_add(diagnostic_result.required_bytes, 1u, &record_bytes) ||
        !size_add(required_json, record_bytes, &required_json)) {
      return fail_result(result, W_SEED_EPHEMERAL_CHECK_CAPACITY,
                         W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT,
                         W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE);
    }
  }

  result->required_capacity = required_json;
  if (required_json > input->json_staging_capacity ||
      (required_json != 0u && input->json_staging == NULL) ||
      required_json > output->jsonl_capacity ||
      (required_json != 0u && output->jsonl == NULL)) {
    result->phase = W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE;
    return fail_result(result, W_SEED_EPHEMERAL_CHECK_CAPACITY,
                       W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT,
                       W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE);
  }

  size_t json_offset = 0u;
  for (size_t index = 0u; index < diagnostic_count; index += 1u) {
    char instance[8];
    if (!make_instance(input->instance, input->instance_length, index,
                       instance)) {
      return fail_result(result, W_SEED_EPHEMERAL_CHECK_CAPACITY,
                         W_SEED_EPHEMERAL_CHECK_FAILURE_INSTANCE,
                         W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_WRITE);
    }
    const w_seed_frontend_diagnostic *diagnostic =
        &input->frontend_staging_output->diagnostics[index];
    const w_seed_frontend_document *document =
        &frontend_input.documents[diagnostic->document_index];
    diagnostic_result = (w_seed_diagnostic_result){0};
    const w_seed_diagnostic_status status = w_seed_diagnostic_frontend_record(
        instance, sizeof(instance) - 1u, document->logical_source_id.data,
        document->logical_source_id.length, document->source,
        diagnostic->document_index, diagnostic,
        input->json_staging + json_offset,
        input->json_staging_capacity - json_offset, &diagnostic_result);
    result->diagnostic_status = status;
    result->diagnostic_result = diagnostic_result;
    result->phase = W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_WRITE;
    if (status != W_SEED_DIAGNOSTIC_OK) {
      const w_seed_ephemeral_check_status mapped = map_diagnostic_status(status);
      return fail_result(result, mapped,
                         W_SEED_EPHEMERAL_CHECK_FAILURE_DIAGNOSTIC,
                         W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_WRITE);
    }
    if (diagnostic_result.written_bytes >
        input->json_staging_capacity - json_offset) {
      return fail_result(result, W_SEED_EPHEMERAL_CHECK_INVALID,
                         W_SEED_EPHEMERAL_CHECK_FAILURE_DIAGNOSTIC,
                         W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_WRITE);
    }
    size_t record_end = 0u;
    if (!size_add(json_offset, diagnostic_result.written_bytes, &record_end)) {
      return fail_result(result, W_SEED_EPHEMERAL_CHECK_INVALID,
                         W_SEED_EPHEMERAL_CHECK_FAILURE_DIAGNOSTIC,
                         W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_WRITE);
    }
    if (record_end >= input->json_staging_capacity) {
      return fail_result(result, W_SEED_EPHEMERAL_CHECK_CAPACITY,
                         W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT,
                         W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_WRITE);
    }
    json_offset = record_end;
    if (json_offset >= input->json_staging_capacity) {
      return fail_result(result, W_SEED_EPHEMERAL_CHECK_CAPACITY,
                         W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT,
                         W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_WRITE);
    }
    input->json_staging[json_offset] = '\n';
    json_offset += 1u;
  }

  if (json_offset != required_json) {
    return fail_result(result, W_SEED_EPHEMERAL_CHECK_INVALID,
                       W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT,
                       W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_WRITE);
  }
  if (json_offset != 0u) (void)memcpy(output->jsonl, input->json_staging, json_offset);
  output->jsonl_length = json_offset;
  result->status = diagnostic_count == 0u
                       ? W_SEED_EPHEMERAL_CHECK_OK
                       : W_SEED_EPHEMERAL_CHECK_DIAGNOSTICS;
  result->failure = W_SEED_EPHEMERAL_CHECK_FAILURE_NONE;
  result->phase = W_SEED_EPHEMERAL_CHECK_PHASE_COMMIT;
  result->required_capacity = required_json;
  result->diagnostic_index = diagnostic_count == 0u ? SIZE_MAX
                                                    : diagnostic_count - 1u;
  if (diagnostic_count == 0u) {
    result->diagnostic_status = W_SEED_DIAGNOSTIC_NO_RECORD;
    result->diagnostic_result = diagnostic_result;
  }
  return result->status;
}
