#include "w_seed_ephemeral_driver.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

/* CHK6 is deliberately a small, caller-owned orchestration layer.  It keeps
 * all parser and resolver state outside the heap and never asks the host to
 * discover a path. */
enum {
  DRIVER_MAX_RANGES = 2048,
  DRIVER_MAX_CANDIDATES = W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES,
};

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool present;
} driver_range;

typedef enum {
  DRIVER_PATH_OK = 0,
  DRIVER_PATH_INVALID,
  DRIVER_PATH_UTF8,
  DRIVER_PATH_NFC,
} driver_path_status;

static w_seed_span empty_span(size_t offset) {
  return (w_seed_span){offset, offset};
}

static void clear_result(w_seed_ephemeral_driver_result *result) {
  if (result == NULL) return;
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_EPHEMERAL_DRIVER_INVALID;
  result->failure = W_SEED_EPHEMERAL_DRIVER_FAILURE_NONE;
  result->phase = W_SEED_EPHEMERAL_DRIVER_PHASE_NONE;
  result->round = SIZE_MAX;
  result->candidate_index = SIZE_MAX;
  result->origin_index = SIZE_MAX;
  result->document_index = SIZE_MAX;
  result->capacity_field =
      W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_NONE;
  result->provider_status = W_SEED_EPHEMERAL_PROVIDER_INVALID;
  result->provider_result.status = W_SEED_EPHEMERAL_PROVIDER_INVALID;
  result->provider_result.request_index = SIZE_MAX;
  result->parser_status = W_SEED_PARSE_FATAL;
  result->scan_status = W_SEED_MODULE_SCAN_INVALID;
  result->graph_status = W_SEED_EPHEMERAL_GRAPH_INVALID;
  result->graph_result.status = W_SEED_EPHEMERAL_GRAPH_INVALID;
  result->graph_result.candidate_index = SIZE_MAX;
  result->graph_result.document_ordinal = SIZE_MAX;
  result->graph_result.edge_ordinal = SIZE_MAX;
}

static w_seed_ephemeral_driver_status fail_driver(
    w_seed_ephemeral_driver_result *result,
    w_seed_ephemeral_driver_status status,
    w_seed_ephemeral_driver_failure failure,
    w_seed_ephemeral_driver_phase phase, size_t round,
    size_t candidate_index, size_t origin_index, size_t document_index,
    w_seed_span span, size_t required_capacity) {
  if (result != NULL) {
    result->status = status;
    result->failure = failure;
    result->phase = phase;
    result->round = round;
    result->candidate_index = candidate_index;
    result->origin_index = origin_index;
    result->document_index = document_index;
    result->span = span;
    result->required_capacity = required_capacity;
  }
  return status;
}

static w_seed_ephemeral_driver_status fail_capacity(
    w_seed_ephemeral_driver_result *result,
    w_seed_ephemeral_driver_capacity_field field,
    size_t required_capacity, w_seed_ephemeral_driver_phase phase,
    size_t round, size_t candidate_index, size_t origin_index,
    size_t document_index, w_seed_span span) {
  const w_seed_ephemeral_driver_status status =
      fail_driver(result, W_SEED_EPHEMERAL_DRIVER_CAPACITY,
                  W_SEED_EPHEMERAL_DRIVER_FAILURE_STORAGE, phase, round,
                  candidate_index, origin_index, document_index, span,
                  required_capacity);
  if (result != NULL) result->capacity_field = field;
  return status;
}

typedef struct {
  w_seed_ephemeral_driver_capacity_field field;
  size_t required;
} driver_parser_capacity;

static size_t capacity_after(size_t capacity) {
  return capacity == SIZE_MAX ? SIZE_MAX : capacity + 1u;
}

/* Parser capacity is not exposed as a separate upstream error enum. The
 * parser state still identifies the saturated caller-owned store for the
 * normal failure modes. Lexer frame exhaustion is reported only when the
 * lexer itself has a non-zero saturated frame count. */
static driver_parser_capacity parser_capacity_failure(
    const w_seed_parser *parser) {
  if (parser == NULL) {
    return (driver_parser_capacity){
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE, 1u};
  }
  switch (parser->capacity_kind) {
    case W_SEED_PARSE_CAPACITY_NODE:
      return (driver_parser_capacity){
          W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE,
          parser->capacity_required};
    case W_SEED_PARSE_CAPACITY_TOKEN:
      return (driver_parser_capacity){
          W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_TOKEN,
          parser->capacity_required};
    case W_SEED_PARSE_CAPACITY_FRAME:
      return (driver_parser_capacity){
          W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_FRAME,
          parser->capacity_required};
    case W_SEED_PARSE_CAPACITY_ISSUE:
      return (driver_parser_capacity){
          W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_ISSUE,
          parser->capacity_required};
    case W_SEED_PARSE_CAPACITY_LEXER_FRAME:
      return (driver_parser_capacity){
          W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_LEXER_FRAME,
          parser->capacity_required};
    case W_SEED_PARSE_CAPACITY_NONE:
      break;
  }
  if (parser->node_count >= parser->node_capacity)
    return (driver_parser_capacity){
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE,
        capacity_after(parser->node_capacity)};
  if (parser->token_count >= parser->token_capacity)
    return (driver_parser_capacity){
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_TOKEN,
        capacity_after(parser->token_capacity)};
  if (parser->frame_count >= parser->frame_capacity)
    return (driver_parser_capacity){
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_FRAME,
        capacity_after(parser->frame_capacity)};
  if (parser->issue_count >= parser->issue_capacity)
    return (driver_parser_capacity){
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_ISSUE,
        capacity_after(parser->issue_capacity)};
  if (parser->lexer.frame_capacity != 0u &&
      parser->lexer.frame_count >= parser->lexer.frame_capacity)
    return (driver_parser_capacity){
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_LEXER_FRAME,
        capacity_after(parser->lexer.frame_capacity)};
  return (driver_parser_capacity){
      W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE,
      capacity_after(parser->node_capacity)};
}

static w_seed_ephemeral_driver_status fail_parser_capacity(
    w_seed_ephemeral_driver_result *result, const w_seed_parser *parser,
    size_t round, size_t candidate_index, w_seed_span span) {
  const driver_parser_capacity capacity = parser_capacity_failure(parser);
  const w_seed_ephemeral_driver_status status = fail_driver(
      result, W_SEED_EPHEMERAL_DRIVER_CAPACITY,
      W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE,
      W_SEED_EPHEMERAL_DRIVER_PHASE_PARSE, round, candidate_index, SIZE_MAX,
      candidate_index, span, capacity.required);
  if (result != NULL) result->capacity_field = capacity.field;
  return status;
}

static bool text_equal(w_seed_frontend_text left,
                       w_seed_frontend_text right) {
  return left.length == right.length &&
         (left.length == 0u ||
          (left.data != NULL && right.data != NULL &&
           memcmp(left.data, right.data, left.length) == 0));
}

/* w_seed_source_init needs a destination.  Keep this wrapper separate so a
 * text view can be checked without retaining a temporary source object. */
static driver_path_status text_encoding(w_seed_frontend_text text) {
  if (text.length != 0u && text.data == NULL) return DRIVER_PATH_INVALID;
  w_seed_source source;
  if (!w_seed_source_init(
          (w_seed_byte_view){(const uint8_t *)text.data, text.length},
          &source, NULL))
    return DRIVER_PATH_UTF8;
  return DRIVER_PATH_OK;
}

static bool ascii_identifier_byte(uint8_t byte) {
  return (byte >= (uint8_t)'a' && byte <= (uint8_t)'z') ||
         (byte >= (uint8_t)'A' && byte <= (uint8_t)'Z') || byte == (uint8_t)'_';
}

static bool ascii_identifier_tail_byte(uint8_t byte) {
  return ascii_identifier_byte(byte) ||
         (byte >= (uint8_t)'0' && byte <= (uint8_t)'9');
}

static bool ascii_identifier_range(const char *data, size_t start,
                                   size_t end) {
  if (data == NULL || start >= end ||
      !ascii_identifier_byte((uint8_t)data[start]))
    return false;
  for (size_t index = start + 1u; index < end; index += 1u) {
    if (!ascii_identifier_tail_byte((uint8_t)data[index])) return false;
  }
  return true;
}

static driver_path_status module_path_shape(w_seed_frontend_text path,
                                             bool *is_std) {
  if (is_std != NULL) *is_std = false;
  const driver_path_status encoding = text_encoding(path);
  if (encoding != DRIVER_PATH_OK) return encoding;
  if (path.length == 0u || path.data == NULL) return DRIVER_PATH_INVALID;
  for (size_t index = 0u; index < path.length; index += 1u) {
    const uint8_t byte = (uint8_t)path.data[index];
    if (byte >= 0x80u) return DRIVER_PATH_NFC;
    if (byte == 0u || byte == (uint8_t)'/' || byte == (uint8_t)'\\' ||
        byte == (uint8_t)':')
      return DRIVER_PATH_INVALID;
  }
  size_t component_start = 0u;
  size_t component_count = 0u;
  size_t first_end = path.length;
  for (size_t index = 0u; index <= path.length; index += 1u) {
    if (index != path.length && path.data[index] != '.') continue;
    if (!ascii_identifier_range(path.data, component_start, index))
      return DRIVER_PATH_INVALID;
    if (component_count == 0u) first_end = index;
    component_count += 1u;
    component_start = index + 1u;
  }
  if (is_std != NULL && first_end == 3u &&
      memcmp(path.data, "std", 3u) == 0)
    *is_std = true;
  return component_count == 0u ? DRIVER_PATH_INVALID : DRIVER_PATH_OK;
}

static driver_path_status root_source_id_shape(w_seed_frontend_text source_id) {
  const driver_path_status encoding = text_encoding(source_id);
  if (encoding != DRIVER_PATH_OK) return encoding;
  if (source_id.length < 3u || source_id.data == NULL ||
      source_id.data[source_id.length - 2u] != '.' ||
      source_id.data[source_id.length - 1u] != 'w')
    return DRIVER_PATH_INVALID;
  for (size_t index = 0u; index < source_id.length; index += 1u) {
    const uint8_t byte = (uint8_t)source_id.data[index];
    if (byte >= 0x80u) return DRIVER_PATH_NFC;
    if (byte == 0u || byte == (uint8_t)'/' || byte == (uint8_t)'\\' ||
        byte == (uint8_t)':')
      return DRIVER_PATH_INVALID;
  }
  return ascii_identifier_range(source_id.data, 0u, source_id.length - 2u)
             ? DRIVER_PATH_OK
             : DRIVER_PATH_INVALID;
}

static bool module_last_component(w_seed_frontend_text module,
                                  w_seed_frontend_text *last) {
  if (last != NULL) *last = (w_seed_frontend_text){NULL, 0u};
  if (module.data == NULL || module.length == 0u || last == NULL) return false;
  size_t start = 0u;
  for (size_t index = 0u; index < module.length; index += 1u) {
    if (module.data[index] == '.') start = index + 1u;
  }
  if (start >= module.length) return false;
  *last = (w_seed_frontend_text){module.data + start, module.length - start};
  return true;
}

static bool source_id_matches_import(w_seed_frontend_text source_id,
                                     w_seed_frontend_text import_path) {
  if (source_id.length != import_path.length + 2u || source_id.data == NULL ||
      import_path.data == NULL || source_id.length < 3u ||
      source_id.data[source_id.length - 2u] != '.' ||
      source_id.data[source_id.length - 1u] != 'w')
    return false;
  for (size_t index = 0u; index < import_path.length; index += 1u) {
    const char expected = import_path.data[index] == '.'
                              ? '/'
                              : import_path.data[index];
    if (source_id.data[index] != expected) return false;
  }
  return true;
}

static bool copy_text(char *destination, size_t capacity,
                      w_seed_frontend_text text, size_t *written) {
  if (written != NULL) *written = 0u;
  if (text.length > capacity || (text.length != 0u &&
                                 (destination == NULL || text.data == NULL)))
    return false;
  if (text.length != 0u) (void)memcpy(destination, text.data, text.length);
  if (written != NULL) *written = text.length;
  return true;
}

static bool source_id_from_import(char *destination, size_t capacity,
                                  w_seed_frontend_text import_path,
                                  size_t *written) {
  if (written != NULL) *written = 0u;
  if (import_path.length > SIZE_MAX - 2u ||
      import_path.length + 2u > capacity || destination == NULL ||
      import_path.data == NULL)
    return false;
  for (size_t index = 0u; index < import_path.length; index += 1u)
    destination[index] = import_path.data[index] == '.'
                             ? '/'
                             : import_path.data[index];
  destination[import_path.length] = '.';
  destination[import_path.length + 1u] = 'w';
  if (written != NULL) *written = import_path.length + 2u;
  return true;
}

static bool make_range(const void *pointer, size_t bytes, driver_range *range) {
  if (range == NULL) return false;
  *range = (driver_range){0u, 0u, false};
  if (bytes == 0u) return true;
  if (pointer == NULL) return false;
  const uintptr_t start = (uintptr_t)pointer;
  if ((uintmax_t)bytes > (uintmax_t)UINTPTR_MAX - (uintmax_t)start)
    return false;
  range->start = start;
  range->end = start + (uintptr_t)bytes;
  range->present = true;
  return true;
}

static bool range_for_array(const void *pointer, size_t count, size_t element,
                            driver_range *range) {
  if (element != 0u && count > SIZE_MAX / element) return false;
  return make_range(pointer, count * element, range);
}

static bool ranges_overlap(driver_range left, driver_range right) {
  return left.present && right.present && left.start < right.end &&
         right.start < left.end;
}

static bool add_range(driver_range *ranges, size_t *count, const void *pointer,
                      size_t bytes) {
  if (ranges == NULL || count == NULL || *count >= DRIVER_MAX_RANGES ||
      !make_range(pointer, bytes, &ranges[*count])) {
    return false;
  }
  *count += 1u;
  return true;
}

static bool add_array_range(driver_range *ranges, size_t *count,
                            const void *pointer, size_t elements,
                            size_t element_size) {
  driver_range range;
  if (!range_for_array(pointer, elements, element_size, &range)) return false;
  if (!range.present) return true;
  return add_range(ranges, count, pointer, elements * element_size);
}

static bool bounded_pointer(const void *pointer, size_t capacity,
                            size_t maximum) {
  return capacity == 0u || (pointer != NULL && capacity <= maximum);
}

/* Check only container shape before storage_ranges_valid walks per-slot and
 * per-request fields. This keeps malformed NULL arrays fail-closed. */
static bool driver_storage_shape_valid(
    const w_seed_ephemeral_driver_input *input,
    const w_seed_ephemeral_driver_scratch *scratch,
    const w_seed_ephemeral_driver_output *output) {
  if (input == NULL || scratch == NULL || output == NULL ||
      scratch->slots == NULL || scratch->requests == NULL ||
      scratch->graph_scratch == NULL ||
      scratch->slot_capacity > DRIVER_MAX_CANDIDATES ||
      scratch->request_capacity > DRIVER_MAX_CANDIDATES ||
      scratch->candidate_document_capacity > DRIVER_MAX_CANDIDATES ||
      scratch->candidate_fact_capacity > DRIVER_MAX_CANDIDATES ||
      scratch->origin_capacity > (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      output->document_capacity > (size_t)W_SEED_FRONTEND_MAX_DOCUMENTS ||
      output->graph.inventory_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      output->graph.edge_capacity > (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      output->graph.document_order_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      output->graph.resolved_import_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      !bounded_pointer(scratch->candidate_documents,
                       scratch->candidate_document_capacity,
                       DRIVER_MAX_CANDIDATES) ||
      !bounded_pointer(scratch->candidate_facts, scratch->candidate_fact_capacity,
                       DRIVER_MAX_CANDIDATES) ||
      !bounded_pointer(output->documents, output->document_capacity,
                       (size_t)W_SEED_FRONTEND_MAX_DOCUMENTS) ||
      !bounded_pointer(output->graph.inventory, output->graph.inventory_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(output->graph.edges, output->graph.edge_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) ||
      !bounded_pointer(output->graph.document_order,
                       output->graph.document_order_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(output->graph.resolved_imports,
                       output->graph.resolved_import_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES))
    return false;
  if (!bounded_pointer(input->root_path.data, input->root_path.length,
                       (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES) ||
      !bounded_pointer(input->root_source_id.data, input->root_source_id.length,
                       (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES))
    return false;
  if (!bounded_pointer(scratch->lexer_frames, scratch->lexer_frame_capacity,
                       (size_t)W_SEED_FRONTEND_MAX_CST_NODES) ||
      !bounded_pointer(scratch->tokens, scratch->token_capacity,
                       (size_t)(4u * W_SEED_FRONTEND_MAX_CST_NODES)) ||
      !bounded_pointer(scratch->parse_frames, scratch->parse_frame_capacity,
                       (size_t)W_SEED_FRONTEND_MAX_CST_NODES) ||
      !bounded_pointer(scratch->issues, scratch->issue_capacity,
                       (size_t)W_SEED_FRONTEND_MAX_CST_NODES) ||
      !bounded_pointer(scratch->origins, scratch->origin_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES))
    return false;
  const w_seed_ephemeral_graph_scratch *graph = scratch->graph_scratch;
  if (!bounded_pointer(graph->nodes, graph->node_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(graph->edges, graph->edge_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) ||
      !bounded_pointer(graph->sorted_nodes, graph->sorted_nodes_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(graph->node_ordinals, graph->node_ordinals_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(graph->sorted_edges, graph->sorted_edges_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) ||
      !bounded_pointer(graph->sorted_resolved_edges,
                       graph->sorted_resolved_edges_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) ||
      !bounded_pointer(graph->origins, graph->origin_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) ||
      !bounded_pointer(graph->indegree, graph->indegree_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(graph->queue, graph->queue_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(graph->depths, graph->depths_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES))
    return false;
  for (size_t index = 0u; index < scratch->slot_capacity; index += 1u) {
    const w_seed_ephemeral_driver_slot *slot = &scratch->slots[index];
    if (!bounded_pointer(slot->source_id_storage, slot->source_id_capacity,
                         (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES) ||
        !bounded_pointer(slot->module_id_storage, slot->module_id_capacity,
                         (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES) ||
        !bounded_pointer(slot->nodes, slot->node_capacity,
                         (size_t)W_SEED_FRONTEND_MAX_CST_NODES))
      return false;
  }
  for (size_t index = 0u; index < scratch->request_capacity; index += 1u) {
    const w_seed_ephemeral_provider_request *request = &scratch->requests[index];
    if (!bounded_pointer(request->staging_bytes, request->staging_capacity,
                         (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES) ||
        !bounded_pointer(request->revalidation_bytes,
                         request->revalidation_capacity,
                         (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES) ||
        !bounded_pointer(request->bytes, request->byte_capacity,
                         (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES))
      return false;
#define CHECK_TOKEN(pointer, capacity) \
    if (!bounded_pointer((pointer), (capacity), \
                         (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES)) return false
    CHECK_TOKEN(request->tokens.provider_id,
                request->tokens.provider_id_capacity);
    CHECK_TOKEN(request->tokens.root_token, request->tokens.root_token_capacity);
    CHECK_TOKEN(request->tokens.source_provider_owner_token,
                request->tokens.source_provider_owner_token_capacity);
    CHECK_TOKEN(request->tokens.canonical_token,
                request->tokens.canonical_token_capacity);
    CHECK_TOKEN(request->revalidation_tokens.provider_id,
                request->revalidation_tokens.provider_id_capacity);
    CHECK_TOKEN(request->revalidation_tokens.root_token,
                request->revalidation_tokens.root_token_capacity);
    CHECK_TOKEN(request->revalidation_tokens.source_provider_owner_token,
                request->revalidation_tokens.source_provider_owner_token_capacity);
    CHECK_TOKEN(request->revalidation_tokens.canonical_token,
                request->revalidation_tokens.canonical_token_capacity);
#undef CHECK_TOKEN
  }
  return true;
}

static bool storage_ranges_valid(
    const w_seed_ephemeral_driver_input *input,
    const w_seed_ephemeral_driver_scratch *scratch,
    const w_seed_ephemeral_driver_output *output,
  const w_seed_ephemeral_driver_result *result, bool *result_overlap) {
  if (result_overlap != NULL) *result_overlap = false;
  if (!driver_storage_shape_valid(input, scratch, output) || result == NULL) {
    /* Nested addresses cannot be inspected safely when their container shape
     * is invalid. Treat the result as potentially aliased and do not write. */
    if (result_overlap != NULL) *result_overlap = true;
    return false;
  }
  driver_range ranges[DRIVER_MAX_RANGES];
  size_t count = 0u;
#define ADD_OBJECT(pointer, type) \
  if (!add_array_range(ranges, &count, (pointer), 1u, sizeof(type))) return false
#define ADD_ARRAY(pointer, length, type) \
  if (!add_array_range(ranges, &count, (pointer), (length), sizeof(type))) return false
  ADD_OBJECT(input, *input);
  ADD_OBJECT(scratch, *scratch);
  ADD_OBJECT(output, *output);
  ADD_ARRAY(input->root_path.data, input->root_path.length, uint8_t);
  ADD_ARRAY(input->root_source_id.data, input->root_source_id.length, char);
  ADD_ARRAY(scratch->slots, scratch->slot_capacity,
            w_seed_ephemeral_driver_slot);
  ADD_ARRAY(scratch->requests, scratch->request_capacity,
            w_seed_ephemeral_provider_request);
  ADD_ARRAY(scratch->lexer_frames, scratch->lexer_frame_capacity,
            w_seed_lexer_frame);
  ADD_ARRAY(scratch->tokens, scratch->token_capacity, w_seed_parse_token);
  ADD_ARRAY(scratch->parse_frames, scratch->parse_frame_capacity,
            w_seed_parse_frame);
  ADD_ARRAY(scratch->issues, scratch->issue_capacity, w_seed_parse_issue);
  ADD_ARRAY(scratch->origins, scratch->origin_capacity, w_seed_module_origin);
  ADD_ARRAY(scratch->candidate_documents, scratch->candidate_document_capacity,
            w_seed_frontend_document);
  ADD_ARRAY(scratch->candidate_facts, scratch->candidate_fact_capacity,
            w_seed_ephemeral_graph_provider_facts);
  ADD_OBJECT(scratch->graph_scratch, *scratch->graph_scratch);
  ADD_ARRAY(output->documents, output->document_capacity,
            w_seed_frontend_document);
  ADD_ARRAY(output->graph.inventory, output->graph.inventory_capacity,
            w_seed_ephemeral_graph_inventory_item);
  ADD_ARRAY(output->graph.edges, output->graph.edge_capacity,
            w_seed_ephemeral_graph_edge);
  ADD_ARRAY(output->graph.document_order, output->graph.document_order_capacity,
            uint32_t);
  ADD_ARRAY(output->graph.resolved_imports,
            output->graph.resolved_import_capacity,
            w_seed_frontend_resolved_import);
  w_seed_ephemeral_graph_scratch *graph = scratch->graph_scratch;
  ADD_ARRAY(graph->nodes, graph->node_capacity,
            w_seed_ephemeral_graph_scratch_node);
  ADD_ARRAY(graph->edges, graph->edge_capacity,
            w_seed_ephemeral_graph_scratch_edge);
  ADD_ARRAY(graph->sorted_nodes, graph->sorted_nodes_capacity, size_t);
  ADD_ARRAY(graph->node_ordinals, graph->node_ordinals_capacity, size_t);
  ADD_ARRAY(graph->sorted_edges, graph->sorted_edges_capacity, size_t);
  ADD_ARRAY(graph->sorted_resolved_edges, graph->sorted_resolved_edges_capacity,
            size_t);
  ADD_ARRAY(graph->origins, graph->origin_capacity, w_seed_module_origin);
  ADD_ARRAY(graph->indegree, graph->indegree_capacity, uint32_t);
  ADD_ARRAY(graph->queue, graph->queue_capacity, uint32_t);
  ADD_ARRAY(graph->depths, graph->depths_capacity, uint32_t);
  for (size_t index = 0u; index < scratch->slot_capacity; index += 1u) {
    const w_seed_ephemeral_driver_slot *slot = &scratch->slots[index];
    ADD_ARRAY(slot->source_id_storage, slot->source_id_capacity, char);
    ADD_ARRAY(slot->module_id_storage, slot->module_id_capacity, char);
    ADD_ARRAY(slot->nodes, slot->node_capacity, w_seed_cst_node);
  }
  for (size_t index = 0u; index < scratch->request_capacity; index += 1u) {
    const w_seed_ephemeral_provider_request *request = &scratch->requests[index];
    ADD_ARRAY(request->staging_bytes, request->staging_capacity, uint8_t);
    ADD_ARRAY(request->revalidation_bytes, request->revalidation_capacity,
              uint8_t);
    ADD_ARRAY(request->bytes, request->byte_capacity, uint8_t);
    ADD_ARRAY(request->tokens.provider_id, request->tokens.provider_id_capacity,
              char);
    ADD_ARRAY(request->tokens.root_token, request->tokens.root_token_capacity,
              char);
    ADD_ARRAY(request->tokens.source_provider_owner_token,
              request->tokens.source_provider_owner_token_capacity, char);
    ADD_ARRAY(request->tokens.canonical_token,
              request->tokens.canonical_token_capacity, char);
    ADD_ARRAY(request->revalidation_tokens.provider_id,
              request->revalidation_tokens.provider_id_capacity, char);
    ADD_ARRAY(request->revalidation_tokens.root_token,
              request->revalidation_tokens.root_token_capacity, char);
    ADD_ARRAY(request->revalidation_tokens.source_provider_owner_token,
              request->revalidation_tokens.source_provider_owner_token_capacity,
              char);
    ADD_ARRAY(request->revalidation_tokens.canonical_token,
              request->revalidation_tokens.canonical_token_capacity, char);
  }
  driver_range result_range;
  if (!make_range(result, sizeof(*result), &result_range)) return false;
  for (size_t index = 0u; index < count; index += 1u) {
    if (ranges_overlap(result_range, ranges[index])) {
      if (result_overlap != NULL) *result_overlap = true;
      return false;
    }
  }
  for (size_t left = 0u; left < count; left += 1u) {
    for (size_t right = left + 1u; right < count; right += 1u) {
      if (ranges_overlap(ranges[left], ranges[right])) return false;
    }
  }
#undef ADD_OBJECT
#undef ADD_ARRAY
  return true;
}

static bool limits_valid(const w_seed_ephemeral_driver_input *input) {
  if (input == NULL || input->provider_limits.max_sources == 0u ||
      input->provider_limits.max_sources >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES ||
      input->provider_limits.max_source_bytes == 0u ||
      input->provider_limits.max_source_bytes >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES ||
      input->provider_limits.max_total_source_bytes == 0u ||
      input->provider_limits.max_total_source_bytes >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOTAL_SOURCE_BYTES ||
      input->provider_limits.max_path_bytes == 0u ||
      input->provider_limits.max_path_bytes >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES ||
      input->provider_limits.max_token_bytes == 0u ||
      input->provider_limits.max_token_bytes >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES ||
      input->max_edges > (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      input->max_depth > (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_DEPTH ||
      input->root_path.data == NULL || input->root_path.length == 0u ||
      input->root_path.length > input->provider_limits.max_path_bytes ||
      input->root_source_id.data == NULL || input->root_source_id.length == 0u ||
      input->root_source_id.length > input->provider_limits.max_path_bytes)
    return false;
  return true;
}

static w_seed_ephemeral_driver_status map_path_failure(
    driver_path_status status, w_seed_ephemeral_driver_failure invalid_failure,
    w_seed_ephemeral_driver_result *result,
    w_seed_ephemeral_driver_phase phase, size_t round, size_t candidate,
    size_t origin, size_t document, w_seed_span span) {
  if (status == DRIVER_PATH_NFC)
    return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED,
                       W_SEED_EPHEMERAL_DRIVER_FAILURE_UNSUPPORTED_NFC, phase,
                       round, candidate, origin, document, span, 0u);
  return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_INVALID, invalid_failure,
                     phase, round, candidate, origin, document, span, 0u);
}

static w_seed_ephemeral_driver_status bind_requests(
    const w_seed_ephemeral_driver_input *input,
    w_seed_ephemeral_driver_scratch *scratch, size_t candidate_count,
    w_seed_ephemeral_driver_result *result, size_t round) {
  for (size_t index = 0u; index < candidate_count; index += 1u) {
    w_seed_ephemeral_provider_request *request = &scratch->requests[index];
    const w_seed_ephemeral_driver_slot *slot = &scratch->slots[index];
    request->source_id = (w_seed_frontend_text){slot->source_id_storage,
                                                slot->source_id_length};
    request->source = &scratch->slots[index].source;
    request->facts = &scratch->slots[index].facts;
    if (request->source_id.data == NULL || request->source_id.length == 0u)
      return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_INVALID,
                         W_SEED_EPHEMERAL_DRIVER_FAILURE_SOURCE_ID,
                         W_SEED_EPHEMERAL_DRIVER_PHASE_ACQUIRE, round, index,
                         SIZE_MAX, SIZE_MAX, empty_span(0u),
                         request->source_id.length + 1u);
  }
  (void)input;
  return W_SEED_EPHEMERAL_DRIVER_OK;
}

static w_seed_ephemeral_driver_status acquire_wave(
    const w_seed_ephemeral_driver_input *input,
    w_seed_ephemeral_driver_scratch *scratch, size_t candidate_count,
    size_t round, w_seed_ephemeral_driver_result *result) {
  const w_seed_ephemeral_driver_status bound =
      bind_requests(input, scratch, candidate_count, result, round);
  if (bound != W_SEED_EPHEMERAL_DRIVER_OK) return bound;
  const w_seed_ephemeral_provider_input provider_input = {
      input->root_path,
      scratch->requests,
      candidate_count,
      0u,
      input->provider_limits,
      input->backend};
  w_seed_ephemeral_provider_result provider_result;
  const w_seed_ephemeral_provider_status provider_status =
      w_seed_ephemeral_provider_acquire(&provider_input, &provider_result);
  result->provider_status = provider_status;
  result->provider_result = provider_result;
  if (provider_status == W_SEED_EPHEMERAL_PROVIDER_OK) return W_SEED_EPHEMERAL_DRIVER_OK;
  if (provider_status == W_SEED_EPHEMERAL_PROVIDER_CAPACITY)
    {
      const w_seed_ephemeral_driver_status status = fail_driver(
          result, W_SEED_EPHEMERAL_DRIVER_CAPACITY,
          W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER,
          W_SEED_EPHEMERAL_DRIVER_PHASE_ACQUIRE, round,
          provider_result.request_index, SIZE_MAX, SIZE_MAX, empty_span(0u),
          provider_result.required_capacity);
      result->capacity_field = W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER;
      return status;
    }
  if (provider_status == W_SEED_EPHEMERAL_PROVIDER_UNSUPPORTED)
    return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED,
                       W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER,
                       W_SEED_EPHEMERAL_DRIVER_PHASE_ACQUIRE, round,
                       provider_result.request_index, SIZE_MAX, SIZE_MAX,
                       empty_span(0u), provider_result.required_capacity);
  if (provider_status == W_SEED_EPHEMERAL_PROVIDER_IO &&
      provider_result.backend_status ==
          W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND &&
      provider_result.request_index != 0u)
    return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED,
                       W_SEED_EPHEMERAL_DRIVER_FAILURE_MISSING_LOCAL,
                       W_SEED_EPHEMERAL_DRIVER_PHASE_ACQUIRE, round,
                       provider_result.request_index, SIZE_MAX, SIZE_MAX,
                       empty_span(0u), 0u);
  if (provider_status == W_SEED_EPHEMERAL_PROVIDER_INVALID)
    return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_INVALID,
                       W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER,
                       W_SEED_EPHEMERAL_DRIVER_PHASE_ACQUIRE, round,
                       provider_result.request_index, SIZE_MAX, SIZE_MAX,
                       empty_span(0u), provider_result.required_capacity);
  return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_IO,
                     W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER,
                     W_SEED_EPHEMERAL_DRIVER_PHASE_ACQUIRE, round,
                     provider_result.request_index, SIZE_MAX, SIZE_MAX,
                     empty_span(0u), provider_result.required_capacity);
}

static w_seed_ephemeral_driver_status parse_and_scan(
    const w_seed_ephemeral_driver_input *input,
    w_seed_ephemeral_driver_scratch *scratch, size_t candidate_count,
    size_t round, w_seed_ephemeral_driver_result *result) {
  for (size_t index = 0u; index < candidate_count; index += 1u) {
    w_seed_ephemeral_driver_slot *slot = &scratch->slots[index];
    if (!w_seed_source_validate_span(
            &slot->source, (w_seed_span){0u, slot->source.bytes.length},
            NULL))
      return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_INVALID,
                         W_SEED_EPHEMERAL_DRIVER_FAILURE_SOURCE_ID,
                         W_SEED_EPHEMERAL_DRIVER_PHASE_PARSE, round, index,
                         SIZE_MAX, index, empty_span(0u), 0u);
    w_seed_parser parser;
    w_seed_lex_error lex_error;
    if (!w_seed_parser_init(
            &slot->source, (w_seed_span){0u, slot->source.bytes.length},
            input->foreign_limits, scratch->lexer_frames,
            scratch->lexer_frame_capacity, scratch->tokens,
            scratch->token_capacity, slot->nodes, slot->node_capacity,
            scratch->parse_frames, scratch->parse_frame_capacity, scratch->issues,
            scratch->issue_capacity, &parser, &lex_error)) {
      result->parser_status = W_SEED_PARSE_FATAL;
      return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_INVALID,
                         W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSER_INIT,
                         W_SEED_EPHEMERAL_DRIVER_PHASE_PARSE, round, index,
                         SIZE_MAX, index, empty_span(0u), 0u);
    }
    w_seed_parse_result parse_result;
    if (!w_seed_parser_parse(&parser, &parse_result)) {
      result->parser_status = parser.status;
      if (parser.issue_count != 0u)
        result->parser_issue_kind = scratch->issues[0u].kind;
      if (parser.capacity_kind != W_SEED_PARSE_CAPACITY_NONE ||
          result->parser_issue_kind == W_SEED_PARSE_ISSUE_CAPACITY)
        return fail_parser_capacity(
            result, &parser, round, index,
            parser.issue_count == 0u ? empty_span(0u)
                                     : scratch->issues[0u].primary);
      return fail_driver(
          result, W_SEED_EPHEMERAL_DRIVER_INVALID,
          W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE,
          W_SEED_EPHEMERAL_DRIVER_PHASE_PARSE, round, index, SIZE_MAX, index,
          parser.issue_count == 0u ? empty_span(0u)
                                   : scratch->issues[0u].primary,
          slot->node_capacity);
    }
    result->parser_status = parse_result.status;
    if (parse_result.issue_count != 0u ||
        parse_result.status != W_SEED_PARSE_COMPLETE) {
      if (parse_result.issue_count != 0u)
        result->parser_issue_kind = scratch->issues[0u].kind;
      if (parser.capacity_kind != W_SEED_PARSE_CAPACITY_NONE ||
          result->parser_issue_kind == W_SEED_PARSE_ISSUE_CAPACITY)
        return fail_parser_capacity(
            result, &parser, round, index,
            parse_result.issue_count == 0u
                ? empty_span(0u)
                : scratch->issues[0u].primary);
      return fail_driver(
          result, W_SEED_EPHEMERAL_DRIVER_INVALID,
          W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE,
          W_SEED_EPHEMERAL_DRIVER_PHASE_PARSE, round, index, SIZE_MAX, index,
          parse_result.issue_count == 0u
              ? empty_span(0u)
              : scratch->issues[0u].primary,
          slot->node_capacity);
    }
    slot->document = (w_seed_frontend_document){
        {slot->source_id_storage, slot->source_id_length},
        {slot->module_id_storage, slot->module_id_length},
        {slot->module_id_storage, slot->module_id_length}, &slot->source,
        slot->nodes, parse_result.node_count, parse_result};
    slot->document.logical_source_id =
        (w_seed_frontend_text){slot->source_id_storage, slot->source_id_length};
    slot->document.module_id =
        (w_seed_frontend_text){slot->module_id_storage, slot->module_id_length};
    slot->document.local_module_name = (w_seed_frontend_text){
        slot->module_id_storage, slot->module_id_length};
  }
  return W_SEED_EPHEMERAL_DRIVER_OK;
}

static w_seed_ephemeral_driver_status discover(
    const w_seed_ephemeral_driver_input *input,
    w_seed_ephemeral_driver_scratch *scratch, size_t wave_count,
    size_t *candidate_count, size_t round,
    w_seed_ephemeral_driver_result *result) {
  if (candidate_count == NULL) return W_SEED_EPHEMERAL_DRIVER_INVALID;
  for (size_t document_index = 0u; document_index < wave_count;
       document_index += 1u) {
    w_seed_ephemeral_driver_slot *slot = &scratch->slots[document_index];
    w_seed_module_scan_result scan_result;
    const w_seed_module_scan_status scan_status = w_seed_module_scan(
        &slot->source, slot->nodes, slot->document.node_count,
        &slot->document.parse, scratch->origins, scratch->origin_capacity,
        &scan_result);
    result->scan_status = scan_status;
    result->scan_result = scan_result;
    if (scan_status != W_SEED_MODULE_SCAN_OK) {
      if (scan_status == W_SEED_MODULE_SCAN_CAPACITY)
        return fail_capacity(
            result, W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_ORIGIN,
            scan_result.required,
            W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round, document_index,
            SIZE_MAX, document_index, scan_result.module_header_name_span);
      return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_INVALID,
                         W_SEED_EPHEMERAL_DRIVER_FAILURE_SCAN,
                         W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round,
                         document_index, SIZE_MAX, document_index,
                         scan_result.module_header_name_span,
                         scan_result.required);
    }
    if (document_index == 0u) {
      w_seed_frontend_text root_module;
      if (scan_result.has_module_header_name) {
        root_module = (w_seed_frontend_text){
            (const char *)(slot->source.bytes.data +
                           scan_result.module_header_name_span.start_byte),
            scan_result.module_header_name_span.end_byte -
                scan_result.module_header_name_span.start_byte};
      } else {
        root_module = (w_seed_frontend_text){slot->source_id_storage,
                                             slot->source_id_length - 2u};
      }
      const driver_path_status root_module_status =
          module_path_shape(root_module, NULL);
      if (root_module_status != DRIVER_PATH_OK)
        return map_path_failure(
            root_module_status, W_SEED_EPHEMERAL_DRIVER_FAILURE_MODULE_PATH,
            result, W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round,
            document_index, SIZE_MAX, document_index,
            scan_result.has_module_header_name
                ? scan_result.module_header_name_span
                : empty_span(0u));
      if (!copy_text(slot->module_id_storage, slot->module_id_capacity,
                     root_module, &slot->module_id_length))
        return fail_capacity(
            result, W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_MODULE_ID,
            root_module.length, W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round,
            document_index, SIZE_MAX, document_index,
            scan_result.has_module_header_name
                ? scan_result.module_header_name_span
                : empty_span(0u));
    }
    const w_seed_frontend_text module_id = {
        slot->module_id_storage, slot->module_id_length};
    w_seed_frontend_text local_name;
    if (!module_last_component(module_id, &local_name))
      return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_INVALID,
                         W_SEED_EPHEMERAL_DRIVER_FAILURE_MODULE_PATH,
                         W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round,
                         document_index, SIZE_MAX, document_index,
                         empty_span(0u), 0u);
    if (scan_result.has_module_header_name && document_index != 0u) {
      const w_seed_frontend_text header = {
          (const char *)(slot->source.bytes.data +
                         scan_result.module_header_name_span.start_byte),
          scan_result.module_header_name_span.end_byte -
              scan_result.module_header_name_span.start_byte};
      const driver_path_status header_status = module_path_shape(header, NULL);
      if (header_status != DRIVER_PATH_OK)
        return map_path_failure(
            header_status, W_SEED_EPHEMERAL_DRIVER_FAILURE_HEADER, result,
            W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round, document_index,
            SIZE_MAX, document_index, scan_result.module_header_name_span);
      if (!text_equal(header, local_name))
        return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_INVALID,
                           W_SEED_EPHEMERAL_DRIVER_FAILURE_HEADER,
                           W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round,
                           document_index, SIZE_MAX, document_index,
                           scan_result.module_header_name_span, 0u);
    }
    slot->document.logical_source_id =
        (w_seed_frontend_text){slot->source_id_storage, slot->source_id_length};
    slot->document.module_id = module_id;
    slot->document.local_module_name = local_name;
    for (size_t origin_index = 0u; origin_index < scan_result.written;
         origin_index += 1u) {
      const w_seed_module_origin *origin = &scratch->origins[origin_index];
      const w_seed_frontend_text import_path = {
          (const char *)(slot->source.bytes.data +
                         origin->module_path_span.start_byte),
          origin->module_path_span.end_byte - origin->module_path_span.start_byte};
      bool is_std = false;
      const driver_path_status path_status =
          module_path_shape(import_path, &is_std);
      if (path_status != DRIVER_PATH_OK)
        return map_path_failure(path_status,
                                W_SEED_EPHEMERAL_DRIVER_FAILURE_MODULE_PATH,
                                result,
                                W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round,
                                document_index, origin_index, document_index,
                                origin->module_path_span);
      if (is_std)
        return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED,
                           W_SEED_EPHEMERAL_DRIVER_FAILURE_STD_PROVIDER,
                           W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round,
                           document_index, origin_index, document_index,
                           origin->module_path_span, 0u);
      if (text_equal(import_path,
                     (w_seed_frontend_text){scratch->slots[0u].module_id_storage,
                                            scratch->slots[0u].module_id_length}))
        continue;
      if (import_path.length > SIZE_MAX - 2u ||
          import_path.length + 2u > input->provider_limits.max_path_bytes)
        return fail_capacity(
            result, W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SOURCE_ID,
            import_path.length + 2u,
            W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round, document_index,
            origin_index, document_index, origin->module_path_span);
      bool found = false;
      for (size_t index = 0u; index < *candidate_count; index += 1u) {
        if (source_id_matches_import(
                (w_seed_frontend_text){scratch->slots[index].source_id_storage,
                                       scratch->slots[index].source_id_length},
                import_path)) {
          found = true;
          break;
        }
      }
      if (found) continue;
      if (*candidate_count >= input->provider_limits.max_sources ||
          *candidate_count >= scratch->slot_capacity ||
          *candidate_count >= scratch->request_capacity ||
          *candidate_count >= scratch->candidate_document_capacity ||
          *candidate_count >= scratch->candidate_fact_capacity)
        return fail_capacity(
            result, W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SLOT,
            *candidate_count + 1u,
            W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round, document_index,
            origin_index, document_index, origin->module_path_span);
      slot = &scratch->slots[*candidate_count];
      if (!copy_text(slot->module_id_storage, slot->module_id_capacity,
                     import_path, &slot->module_id_length))
        return fail_capacity(
            result, W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_MODULE_ID,
            import_path.length,
            W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round, document_index,
            origin_index, document_index, origin->module_path_span);
      if (!source_id_from_import(slot->source_id_storage,
                                 slot->source_id_capacity, import_path,
                                 &slot->source_id_length))
        return fail_capacity(
            result, W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SOURCE_ID,
            import_path.length + 2u,
            W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER, round, document_index,
            origin_index, document_index, origin->module_path_span);
      *candidate_count += 1u;
      /* Keep the source slot stable while this document's remaining imports
       * are examined; `slot` temporarily names the newly discovered child. */
      slot = &scratch->slots[document_index];
    }
  }
  (void)input;
  return W_SEED_EPHEMERAL_DRIVER_OK;
}

static w_seed_ephemeral_driver_status graph_publish(
    const w_seed_ephemeral_driver_input *input,
    w_seed_ephemeral_driver_scratch *scratch,
    w_seed_ephemeral_driver_output *output, size_t candidate_count,
    size_t round, w_seed_ephemeral_driver_result *result) {
  if (candidate_count > scratch->candidate_document_capacity ||
      candidate_count > scratch->candidate_fact_capacity)
    return fail_capacity(
        result,
        candidate_count > scratch->candidate_document_capacity
            ? W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_CANDIDATE_DOCUMENT
            : W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_CANDIDATE_FACT,
        candidate_count, W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_MEASURE, round,
        SIZE_MAX, SIZE_MAX, SIZE_MAX, empty_span(0u));
  for (size_t index = 0u; index < candidate_count; index += 1u) {
    scratch->candidate_documents[index] = scratch->slots[index].document;
    scratch->candidate_facts[index] = scratch->slots[index].facts;
  }
  const w_seed_ephemeral_graph_input graph_input = {
      scratch->candidate_documents,
      scratch->candidate_facts,
      candidate_count,
      0u,
      scratch->graph_scratch,
      input->provider_limits.max_sources,
      input->max_edges,
      input->max_depth,
      input->provider_limits.max_total_source_bytes};
  w_seed_ephemeral_graph_counts counts;
  w_seed_ephemeral_graph_result graph_result;
  const w_seed_ephemeral_graph_status measure_status =
      w_seed_ephemeral_graph_measure(&graph_input, &counts, &graph_result);
  result->graph_status = measure_status;
  result->graph_result = graph_result;
  if (measure_status != W_SEED_EPHEMERAL_GRAPH_OK) {
    const w_seed_ephemeral_driver_status status =
        measure_status == W_SEED_EPHEMERAL_GRAPH_CAPACITY
            ? W_SEED_EPHEMERAL_DRIVER_CAPACITY
            : (measure_status == W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED
                   ? W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED
                   : W_SEED_EPHEMERAL_DRIVER_INVALID);
    return fail_driver(result, status, W_SEED_EPHEMERAL_DRIVER_FAILURE_GRAPH,
                       W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_MEASURE, round,
                       graph_result.candidate_index, graph_result.edge_ordinal,
                       graph_result.document_ordinal, graph_result.span,
                       graph_result.required.sources != 0u
                           ? graph_result.required.sources
                           : graph_result.required.edges);
  }
  if (output->graph.inventory_capacity < counts.sources)
    return fail_capacity(
        result, W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_INVENTORY,
        counts.sources, W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_WRITE, round,
        SIZE_MAX, SIZE_MAX, SIZE_MAX, empty_span(0u));
  if (output->graph.edge_capacity < counts.edges)
    return fail_capacity(result,
                         W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_EDGE,
                         counts.edges, W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_WRITE,
                         round, SIZE_MAX, SIZE_MAX, SIZE_MAX, empty_span(0u));
  if (output->graph.document_order_capacity < counts.sources)
    return fail_capacity(result,
                         W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_ORDER,
                         counts.sources,
                         W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_WRITE, round,
                         SIZE_MAX, SIZE_MAX, SIZE_MAX, empty_span(0u));
  if (output->graph.resolved_import_capacity < counts.edges)
    return fail_capacity(
        result, W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_RESOLVED,
        counts.edges, W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_WRITE, round,
        SIZE_MAX, SIZE_MAX, SIZE_MAX, empty_span(0u));
  if (output->document_capacity < counts.sources)
    return fail_capacity(result,
                         W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_DOCUMENT,
                         counts.sources,
                         W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_WRITE, round,
                         SIZE_MAX, SIZE_MAX, SIZE_MAX, empty_span(0u));
  const w_seed_ephemeral_graph_status write_status =
      w_seed_ephemeral_graph_write(&graph_input, &output->graph, &graph_result);
  result->graph_status = write_status;
  result->graph_result = graph_result;
  if (write_status != W_SEED_EPHEMERAL_GRAPH_OK) {
    const w_seed_ephemeral_driver_status status =
        write_status == W_SEED_EPHEMERAL_GRAPH_CAPACITY
            ? W_SEED_EPHEMERAL_DRIVER_CAPACITY
            : (write_status == W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED
                   ? W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED
                   : W_SEED_EPHEMERAL_DRIVER_INVALID);
    return fail_driver(result, status, W_SEED_EPHEMERAL_DRIVER_FAILURE_GRAPH,
                       W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_WRITE, round,
                       graph_result.candidate_index, graph_result.edge_ordinal,
                       graph_result.document_ordinal, graph_result.span,
                       graph_result.required.sources != 0u
                           ? graph_result.required.sources
                           : graph_result.required.edges);
  }
  for (size_t ordinal = 0u; ordinal < counts.sources; ordinal += 1u) {
    const uint32_t candidate = output->graph.document_order[ordinal];
    /* CHK4 graph_write validates this as a post-condition. No fallible work
     * remains after publication, so documents and their count commit together. */
    output->documents[ordinal] = scratch->candidate_documents[candidate];
  }
  output->document_count = counts.sources;
  (void)input;
  return W_SEED_EPHEMERAL_DRIVER_OK;
}

w_seed_ephemeral_driver_status w_seed_ephemeral_driver_run(
    const w_seed_ephemeral_driver_input *input,
    w_seed_ephemeral_driver_scratch *scratch,
    w_seed_ephemeral_driver_output *output,
    w_seed_ephemeral_driver_result *result) {
  if (result == NULL) return W_SEED_EPHEMERAL_DRIVER_INVALID;
  bool result_overlap = false;
  if (!storage_ranges_valid(input, scratch, output, result, &result_overlap)) {
    if (result_overlap) return W_SEED_EPHEMERAL_DRIVER_INVALID;
    clear_result(result);
    return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_INVALID,
                       W_SEED_EPHEMERAL_DRIVER_FAILURE_STORAGE,
                       W_SEED_EPHEMERAL_DRIVER_PHASE_VALIDATE, SIZE_MAX,
                       SIZE_MAX, SIZE_MAX, SIZE_MAX, empty_span(0u), 0u);
  }
  clear_result(result);
  if (input == NULL || scratch == NULL || output == NULL ||
      scratch->slots == NULL || scratch->requests == NULL ||
      scratch->slot_capacity == 0u || scratch->request_capacity == 0u ||
      scratch->slot_capacity > DRIVER_MAX_CANDIDATES ||
      scratch->request_capacity > DRIVER_MAX_CANDIDATES ||
      scratch->candidate_document_capacity > DRIVER_MAX_CANDIDATES ||
      scratch->candidate_fact_capacity > DRIVER_MAX_CANDIDATES ||
      scratch->origins == NULL || scratch->origin_capacity == 0u ||
      scratch->graph_scratch == NULL || !limits_valid(input))
    return fail_driver(result, W_SEED_EPHEMERAL_DRIVER_INVALID,
                       W_SEED_EPHEMERAL_DRIVER_FAILURE_LIMIT,
                       W_SEED_EPHEMERAL_DRIVER_PHASE_VALIDATE, SIZE_MAX,
                       SIZE_MAX, SIZE_MAX, SIZE_MAX, empty_span(0u), 0u);
  const driver_path_status source_status =
      root_source_id_shape(input->root_source_id);
  if (source_status != DRIVER_PATH_OK)
    return map_path_failure(source_status,
                            W_SEED_EPHEMERAL_DRIVER_FAILURE_ROOT_SOURCE_ID,
                            result, W_SEED_EPHEMERAL_DRIVER_PHASE_VALIDATE,
                            SIZE_MAX, SIZE_MAX, SIZE_MAX, SIZE_MAX,
                            empty_span(0u));
  w_seed_ephemeral_driver_slot *root = &scratch->slots[0u];
  if (!copy_text(root->source_id_storage, root->source_id_capacity,
                 input->root_source_id, &root->source_id_length))
    return fail_capacity(
        result, W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SOURCE_ID,
        input->root_source_id.length,
        W_SEED_EPHEMERAL_DRIVER_PHASE_VALIDATE, SIZE_MAX, 0u, SIZE_MAX,
        SIZE_MAX, empty_span(0u));
  root->module_id_length = 0u;
  size_t candidate_count = 1u;
  for (size_t round = 0u; round < W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS;
       round += 1u) {
    result->round = round;
    const w_seed_ephemeral_driver_status acquired =
        acquire_wave(input, scratch, candidate_count, round, result);
    if (acquired != W_SEED_EPHEMERAL_DRIVER_OK) return acquired;
    const w_seed_ephemeral_driver_status parsed =
        parse_and_scan(input, scratch, candidate_count, round, result);
    if (parsed != W_SEED_EPHEMERAL_DRIVER_OK) return parsed;
    const size_t before_discovery = candidate_count;
    const w_seed_ephemeral_driver_status discovered =
        discover(input, scratch, before_discovery, &candidate_count, round,
                 result);
    if (discovered != W_SEED_EPHEMERAL_DRIVER_OK) return discovered;
    if (candidate_count == before_discovery) {
      const w_seed_ephemeral_driver_status published = graph_publish(
          input, scratch, output, candidate_count, round, result);
      if (published != W_SEED_EPHEMERAL_DRIVER_OK) return published;
      result->status = W_SEED_EPHEMERAL_DRIVER_OK;
      result->failure = W_SEED_EPHEMERAL_DRIVER_FAILURE_NONE;
      result->phase = W_SEED_EPHEMERAL_DRIVER_PHASE_COMMIT;
      return W_SEED_EPHEMERAL_DRIVER_OK;
    }
  }
  const w_seed_ephemeral_driver_status exhausted = fail_driver(
      result, W_SEED_EPHEMERAL_DRIVER_CAPACITY,
      W_SEED_EPHEMERAL_DRIVER_FAILURE_LIMIT,
      W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER,
      W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS, SIZE_MAX, SIZE_MAX, SIZE_MAX,
      empty_span(0u), W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS + 1u);
  result->capacity_field = W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_ROUNDS;
  return exhausted;
}
