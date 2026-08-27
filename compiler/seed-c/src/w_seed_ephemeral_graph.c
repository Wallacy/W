#include "w_seed_ephemeral_graph.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "w_seed_ephemeral_graph requires 8-bit bytes");

typedef w_seed_ephemeral_graph_scratch_node graph_node;
typedef w_seed_ephemeral_graph_scratch_edge graph_edge;

typedef struct {
  w_seed_ephemeral_graph_scratch *scratch;
  size_t node_count;
  size_t edge_count;
  size_t total_source_bytes;
  bool provider_set;
  w_seed_frontend_text provider_id;
  w_seed_frontend_text root_token;
  w_seed_frontend_text source_owner_token;
} graph_plan;

/* The plan is only caller-owned pointers, counts, and provider views. All
 * graph/origin/sort/Kahn arrays are supplied through scratch. */
_Static_assert(sizeof(graph_plan) <= 512u,
               "ephemeral graph control state must stay small");

typedef struct {
  w_seed_ephemeral_graph_status status;
  w_seed_ephemeral_graph_failure failure;
  size_t candidate_index;
  size_t document_ordinal;
  size_t edge_ordinal;
  w_seed_span span;
} graph_error;

static w_seed_span empty_span(size_t offset) {
  const w_seed_span span = {offset, offset};
  return span;
}

static void clear_result(w_seed_ephemeral_graph_result *result) {
  if (result == NULL) return;
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_EPHEMERAL_GRAPH_INVALID;
  result->failure = W_SEED_EPHEMERAL_GRAPH_FAILURE_NONE;
  result->candidate_index = SIZE_MAX;
  result->document_ordinal = SIZE_MAX;
  result->edge_ordinal = SIZE_MAX;
  result->span = empty_span(0u);
}

static void clear_counts(w_seed_ephemeral_graph_counts *counts) {
  if (counts != NULL) (void)memset(counts, 0, sizeof(*counts));
}

static bool text_view_valid(w_seed_frontend_text text) {
  return text.length == 0u || text.data != NULL;
}

static bool nonempty_text_valid(w_seed_frontend_text text) {
  return text.length != 0u && text.data != NULL;
}

static bool text_equal(w_seed_frontend_text left, w_seed_frontend_text right) {
  return left.length == right.length &&
         (left.length == 0u ||
          memcmp(left.data, right.data, left.length) == 0);
}

static bool text_ascii(w_seed_frontend_text text, bool *non_ascii) {
  if (non_ascii != NULL) *non_ascii = false;
  if (!text_view_valid(text)) return false;
  for (size_t index = 0u; index < text.length; index += 1u) {
    if ((uint8_t)text.data[index] >= 0x80u) {
      if (non_ascii != NULL) *non_ascii = true;
      return false;
    }
  }
  return true;
}

static bool ascii_identifier_byte(uint8_t byte) {
  return (byte >= (uint8_t)'a' && byte <= (uint8_t)'z') ||
         (byte >= (uint8_t)'A' && byte <= (uint8_t)'Z') || byte == (uint8_t)'_';
}

static bool ascii_identifier_tail_byte(uint8_t byte) {
  return ascii_identifier_byte(byte) ||
         (byte >= (uint8_t)'0' && byte <= (uint8_t)'9');
}

static bool ascii_identifier_range(const char *data, size_t start, size_t end) {
  if (data == NULL || start >= end || !ascii_identifier_byte((uint8_t)data[start]))
    return false;
  for (size_t index = start + 1u; index < end; index += 1u) {
    if (!ascii_identifier_tail_byte((uint8_t)data[index])) return false;
  }
  return true;
}

static bool module_path_shape(w_seed_frontend_text path, bool *is_std,
                              bool *non_ascii) {
  if (is_std != NULL) *is_std = false;
  if (!text_ascii(path, non_ascii) || path.length == 0u) return false;
  size_t component_start = 0u;
  size_t component_count = 0u;
  for (size_t index = 0u; index <= path.length; index += 1u) {
    if (index != path.length && path.data[index] != '.') continue;
    if (!ascii_identifier_range(path.data, component_start, index)) return false;
    component_count += 1u;
    if (component_start == 0u && index == 3u &&
        memcmp(path.data, "std", 3u) == 0 &&
        (index == path.length || path.data[index] == '.')) {
      if (is_std != NULL) *is_std = true;
    }
    component_start = index + 1u;
  }
  return component_count != 0u;
}

static bool root_source_id_shape(w_seed_frontend_text source_id,
                                 bool *non_ascii) {
  if (non_ascii != NULL) *non_ascii = false;
  if (!text_ascii(source_id, non_ascii) || source_id.length < 3u ||
      source_id.data[0] == '/' || source_id.data[0] == '\\' ||
      source_id.data[source_id.length - 2u] != '.' ||
      source_id.data[source_id.length - 1u] != 'w') {
    return false;
  }
  for (size_t index = 0u; index + 2u < source_id.length; index += 1u) {
    if (source_id.data[index] == '/' || source_id.data[index] == '\\')
      return false;
  }
  return ascii_identifier_range(source_id.data, 0u, source_id.length - 2u);
}

static bool source_id_equals_import(w_seed_frontend_text source_id,
                                     w_seed_frontend_text module_path) {
  if (source_id.length != module_path.length + 2u ||
      source_id.data == NULL || module_path.data == NULL ||
      source_id.length < 3u || source_id.data[source_id.length - 2u] != '.' ||
      source_id.data[source_id.length - 1u] != 'w') {
    return false;
  }
  size_t source_index = 0u;
  for (size_t path_index = 0u; path_index < module_path.length;
       path_index += 1u) {
    const char expected = module_path.data[path_index] == '.'
                              ? '/'
                              : module_path.data[path_index];
    if (source_index >= source_id.length || source_id.data[source_index] != expected)
      return false;
    source_index += 1u;
  }
  return source_index + 2u == source_id.length;
}

static bool module_last_component(w_seed_frontend_text module,
                                  w_seed_frontend_text *last) {
  if (last != NULL) {
    last->data = NULL;
    last->length = 0u;
  }
  if (!nonempty_text_valid(module)) return false;
  size_t start = 0u;
  for (size_t index = 0u; index < module.length; index += 1u) {
    if (module.data[index] == '.') start = index + 1u;
  }
  if (start >= module.length || last == NULL) return false;
  last->data = module.data + start;
  last->length = module.length - start;
  return true;
}

static bool limits_valid(const w_seed_ephemeral_graph_input *input) {
  return input != NULL && input->max_sources != 0u &&
         input->max_sources <= (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES &&
         input->max_edges <= (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES &&
         input->max_depth <= (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_DEPTH &&
         input->max_total_source_bytes <=
             (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_TOTAL_SOURCE_BYTES;
}

static bool scratch_array_valid(const void *data, size_t capacity,
                                size_t hard_capacity) {
  return capacity <= hard_capacity && (capacity == 0u || data != NULL);
}

static bool scratch_fields_valid(
    const w_seed_ephemeral_graph_scratch *scratch) {
  return scratch != NULL &&
         scratch_array_valid(scratch->nodes, scratch->node_capacity,
                             W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) &&
         scratch_array_valid(scratch->edges, scratch->edge_capacity,
                             W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) &&
         scratch_array_valid(scratch->sorted_nodes,
                             scratch->sorted_nodes_capacity,
                             W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) &&
         scratch_array_valid(scratch->node_ordinals,
                             scratch->node_ordinals_capacity,
                             W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) &&
         scratch_array_valid(scratch->sorted_edges,
                             scratch->sorted_edges_capacity,
                             W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) &&
         scratch_array_valid(scratch->sorted_resolved_edges,
                             scratch->sorted_resolved_edges_capacity,
                             W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) &&
         scratch_array_valid(scratch->origins, scratch->origin_capacity,
                             W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) &&
         scratch_array_valid(scratch->indegree, scratch->indegree_capacity,
                             W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) &&
         scratch_array_valid(scratch->queue, scratch->queue_capacity,
                             W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) &&
         scratch_array_valid(scratch->depths, scratch->depths_capacity,
                             W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES);
}

#if !defined(UINTPTR_MAX)
#error "w_seed_ephemeral_graph requires uintptr_t for scratch overlap validation"
#endif

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} scratch_range;

static bool scratch_range_make(const void *data, size_t capacity,
                               size_t element_size, scratch_range *range) {
  if (range == NULL) return false;
  range->start = 0u;
  range->end = 0u;
  range->active = false;
  if (capacity == 0u) return true;
  if (data == NULL || element_size == 0u ||
      capacity > SIZE_MAX / element_size) {
    return false;
  }
  const uintptr_t start = (uintptr_t)data;
  const size_t byte_count = capacity * element_size;
  if (byte_count > (size_t)UINTPTR_MAX ||
      start > UINTPTR_MAX - (uintptr_t)byte_count) {
    return false;
  }
  range->start = start;
  range->end = start + (uintptr_t)byte_count;
  range->active = true;
  return true;
}

static bool scratch_ranges_disjoint(
    const w_seed_ephemeral_graph_scratch *scratch) {
  const scratch_range ranges[] = {
      {0u, 0u, false}, {0u, 0u, false}, {0u, 0u, false},
      {0u, 0u, false}, {0u, 0u, false}, {0u, 0u, false},
      {0u, 0u, false}, {0u, 0u, false}, {0u, 0u, false},
      {0u, 0u, false},
  };
  scratch_range mutable_ranges[sizeof(ranges) / sizeof(ranges[0])];
  (void)memcpy(mutable_ranges, ranges, sizeof(mutable_ranges));
  const bool made =
      scratch_range_make(scratch->nodes, scratch->node_capacity,
                         sizeof(*scratch->nodes), &mutable_ranges[0]) &&
      scratch_range_make(scratch->edges, scratch->edge_capacity,
                         sizeof(*scratch->edges), &mutable_ranges[1]) &&
      scratch_range_make(scratch->sorted_nodes,
                         scratch->sorted_nodes_capacity,
                         sizeof(*scratch->sorted_nodes), &mutable_ranges[2]) &&
      scratch_range_make(scratch->node_ordinals,
                         scratch->node_ordinals_capacity,
                         sizeof(*scratch->node_ordinals), &mutable_ranges[3]) &&
      scratch_range_make(scratch->sorted_edges, scratch->sorted_edges_capacity,
                         sizeof(*scratch->sorted_edges), &mutable_ranges[4]) &&
      scratch_range_make(scratch->sorted_resolved_edges,
                         scratch->sorted_resolved_edges_capacity,
                         sizeof(*scratch->sorted_resolved_edges),
                         &mutable_ranges[5]) &&
      scratch_range_make(scratch->origins, scratch->origin_capacity,
                         sizeof(*scratch->origins), &mutable_ranges[6]) &&
      scratch_range_make(scratch->indegree, scratch->indegree_capacity,
                         sizeof(*scratch->indegree), &mutable_ranges[7]) &&
      scratch_range_make(scratch->queue, scratch->queue_capacity,
                         sizeof(*scratch->queue), &mutable_ranges[8]) &&
      scratch_range_make(scratch->depths, scratch->depths_capacity,
                         sizeof(*scratch->depths), &mutable_ranges[9]);
  if (!made) return false;
  for (size_t left = 0u; left < sizeof(mutable_ranges) / sizeof(mutable_ranges[0]);
       left += 1u) {
    if (!mutable_ranges[left].active) continue;
    for (size_t right = left + 1u;
         right < sizeof(mutable_ranges) / sizeof(mutable_ranges[0]);
         right += 1u) {
      if (!mutable_ranges[right].active) continue;
      if (mutable_ranges[left].start < mutable_ranges[right].end &&
          mutable_ranges[right].start < mutable_ranges[left].end)
        return false;
    }
  }
  return true;
}

static bool scratch_shape_valid(
    const w_seed_ephemeral_graph_scratch *scratch) {
  return scratch_fields_valid(scratch) && scratch_ranges_disjoint(scratch);
}

static bool input_shape_valid(const w_seed_ephemeral_graph_input *input) {
  if (!limits_valid(input) || input->candidate_count == 0u ||
      input->candidate_count > (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      input->candidate_count > (size_t)UINT32_MAX ||
      input->root_candidate_index >= input->candidate_count ||
      input->documents == NULL || input->provider_facts == NULL) {
    return false;
  }
  return scratch_shape_valid(input->scratch);
}

static w_seed_ephemeral_graph_failure input_failure_kind(
    const w_seed_ephemeral_graph_input *input) {
  if (input == NULL || input->documents == NULL ||
      input->provider_facts == NULL || input->scratch == NULL)
    return W_SEED_EPHEMERAL_GRAPH_FAILURE_POINTER;
  if (input->root_candidate_index >= input->candidate_count)
    return W_SEED_EPHEMERAL_GRAPH_FAILURE_INDEX;
  if (!limits_valid(input) || input->candidate_count == 0u ||
      input->candidate_count > (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      input->candidate_count > (size_t)UINT32_MAX)
    return W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT;
  if (!scratch_fields_valid(input->scratch))
    return W_SEED_EPHEMERAL_GRAPH_FAILURE_POINTER;
  if (!scratch_ranges_disjoint(input->scratch))
    return W_SEED_EPHEMERAL_GRAPH_FAILURE_SCRATCH;
  return W_SEED_EPHEMERAL_GRAPH_FAILURE_POINTER;
}

static void set_error(graph_error *error, w_seed_ephemeral_graph_status status,
                      w_seed_ephemeral_graph_failure failure,
                      size_t candidate_index, size_t document_ordinal,
                      size_t edge_ordinal, w_seed_span span) {
  if (error == NULL) return;
  error->status = status;
  error->failure = failure;
  error->candidate_index = candidate_index;
  error->document_ordinal = document_ordinal;
  error->edge_ordinal = edge_ordinal;
  error->span = span;
}

static bool source_and_document_shape(const w_seed_frontend_document *document,
                                      w_seed_span *span) {
  if (span != NULL) *span = empty_span(0u);
  if (document == NULL || document->source == NULL ||
      (document->source->bytes.length != 0u &&
       document->source->bytes.data == NULL) ||
      !nonempty_text_valid(document->logical_source_id) ||
      !nonempty_text_valid(document->module_id) ||
      !nonempty_text_valid(document->local_module_name) ||
      document->nodes == NULL || document->node_count == 0u ||
      document->parse.status != W_SEED_PARSE_COMPLETE ||
      document->parse.issue_count != 0u ||
      document->parse.root >= document->parse.node_count ||
      document->parse.node_count == 0u ||
      document->parse.node_count > document->node_count ||
      document->parse.node_count > (size_t)W_SEED_FRONTEND_MAX_CST_NODES ||
      document->nodes[document->parse.root].kind != W_SEED_CST_DOCUMENT) {
    return false;
  }
  w_seed_source validated_source;
  if (!w_seed_source_init(document->source->bytes, &validated_source, NULL))
    return false;
  if (!w_seed_source_validate_span(
          document->source,
          (w_seed_span){0u, document->source->bytes.length}, NULL)) {
    return false;
  }
  for (size_t index = 0u; index < document->parse.node_count; index += 1u) {
    const w_seed_cst_node *node = &document->nodes[index];
    if (node->raw_span.start_byte > node->raw_span.end_byte ||
        node->raw_span.end_byte > document->source->bytes.length ||
        !w_seed_source_validate_span(document->source, node->raw_span, NULL) ||
        (node->first_child != W_SEED_CST_NONE &&
         node->first_child >= document->parse.node_count) ||
        (node->next_sibling != W_SEED_CST_NONE &&
         node->next_sibling >= document->parse.node_count)) {
      if (span != NULL) *span = node->raw_span;
      return false;
    }
  }
  return true;
}

static void digest_source(const w_seed_source *source, uint8_t digest[32]) {
  static const uint8_t tag[] = "w-module-source-v1\0";
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, tag, sizeof(tag) - 1u);
  w_seed_sha256_update(&state, source->bytes.data, source->bytes.length);
  w_seed_sha256_final(&state, digest);
}

static bool digest_equal(const uint8_t left[32], const uint8_t right[32]) {
  return memcmp(left, right, 32u) == 0;
}

static bool provider_texts_valid(const w_seed_ephemeral_graph_provider_facts *facts) {
  return facts != NULL && nonempty_text_valid(facts->provider_id) &&
         nonempty_text_valid(facts->root_token) &&
         nonempty_text_valid(facts->source_provider_owner_token) &&
         nonempty_text_valid(facts->canonical_token);
}

static bool provider_facts_valid(
    const w_seed_ephemeral_graph_provider_facts *facts,
    const w_seed_frontend_document *document, uint8_t digest[32]) {
  if (!provider_texts_valid(facts) || document == NULL || document->source == NULL ||
      !facts->opened || !facts->containment_inside ||
      (facts->symlink != W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE &&
       facts->symlink != W_SEED_EPHEMERAL_GRAPH_SYMLINK_INSIDE)) {
    return false;
  }
  if (facts->snapshot_before_byte_count != document->source->bytes.length ||
      facts->snapshot_after_byte_count != document->source->bytes.length) {
    return false;
  }
  digest_source(document->source, digest);
  return digest_equal(facts->snapshot_before_digest, digest) &&
         digest_equal(facts->snapshot_after_digest, digest);
}

static w_seed_ephemeral_graph_failure provider_failure_kind(
    const w_seed_ephemeral_graph_provider_facts *facts) {
  if (!provider_texts_valid(facts) || !facts->opened ||
      !facts->containment_inside ||
      (facts->symlink != W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE &&
       facts->symlink != W_SEED_EPHEMERAL_GRAPH_SYMLINK_INSIDE)) {
    return W_SEED_EPHEMERAL_GRAPH_FAILURE_PROVIDER;
  }
  return W_SEED_EPHEMERAL_GRAPH_FAILURE_SNAPSHOT;
}

static bool provider_same(const graph_plan *plan,
                          const w_seed_ephemeral_graph_provider_facts *facts) {
  return plan != NULL && facts != NULL && plan->provider_set &&
         text_equal(plan->provider_id, facts->provider_id) &&
         text_equal(plan->root_token, facts->root_token) &&
         text_equal(plan->source_owner_token,
                    facts->source_provider_owner_token);
}

static bool nested_source_id_shape(w_seed_frontend_text source_id,
                                   bool *non_ascii) {
  if (non_ascii != NULL) *non_ascii = false;
  if (!text_ascii(source_id, non_ascii) || source_id.length < 3u ||
      source_id.data[0] == '/' || source_id.data[0] == '\\' ||
      source_id.data[source_id.length - 2u] != '.' ||
      source_id.data[source_id.length - 1u] != 'w') {
    return false;
  }
  size_t component_start = 0u;
  size_t component_count = 0u;
  for (size_t index = 0u; index + 2u <= source_id.length; index += 1u) {
    if (index != source_id.length - 2u && source_id.data[index] != '/')
      continue;
    if (!ascii_identifier_range(source_id.data, component_start, index))
      return false;
    component_count += 1u;
    component_start = index + 1u;
  }
  return component_count != 0u;
}

static bool document_matches_expected(const w_seed_frontend_document *document,
                                      w_seed_frontend_text expected_source_id,
                                      w_seed_frontend_text expected_module_id,
                                      w_seed_frontend_text expected_local_name,
                                      bool root_document, w_seed_span *span,
                                      w_seed_ephemeral_graph_failure *failure) {
  if (failure != NULL) *failure = W_SEED_EPHEMERAL_GRAPH_FAILURE_IDENTITY;
  if (!source_and_document_shape(document, span)) {
    if (failure != NULL) *failure = W_SEED_EPHEMERAL_GRAPH_FAILURE_SOURCE;
    return false;
  }
  bool non_ascii = false;
  if (root_document) {
    if (!root_source_id_shape(document->logical_source_id, &non_ascii)) {
      if (failure != NULL) {
        *failure = non_ascii
                          ? W_SEED_EPHEMERAL_GRAPH_FAILURE_UNSUPPORTED_NFC
                          : W_SEED_EPHEMERAL_GRAPH_FAILURE_PATH;
      }
      return false;
    }
  } else {
    if (!nested_source_id_shape(document->logical_source_id, &non_ascii)) {
      if (failure != NULL) {
        *failure = non_ascii
                          ? W_SEED_EPHEMERAL_GRAPH_FAILURE_UNSUPPORTED_NFC
                          : W_SEED_EPHEMERAL_GRAPH_FAILURE_PATH;
      }
      return false;
    }
  }
  if (!text_equal(document->logical_source_id, expected_source_id) ||
      !text_equal(document->module_id, expected_module_id) ||
      !text_equal(document->local_module_name, expected_local_name)) {
    return false;
  }
  w_seed_module_scan_result scan_result;
  const w_seed_module_scan_status scan_status = w_seed_module_scan(
      document->source, document->nodes, document->parse.node_count,
      &document->parse, NULL, 0u, &scan_result);
  if (scan_status != W_SEED_MODULE_SCAN_OK &&
      scan_status != W_SEED_MODULE_SCAN_CAPACITY) {
    if (failure != NULL) {
      *failure = scan_status == W_SEED_MODULE_SCAN_UNSUPPORTED
                     ? W_SEED_EPHEMERAL_GRAPH_FAILURE_PATH
                     : W_SEED_EPHEMERAL_GRAPH_FAILURE_SOURCE;
    }
    return false;
  }
  if (scan_result.has_module_header_name &&
      !text_equal(
          (w_seed_frontend_text){
              (const char *)(document->source->bytes.data +
                             scan_result.module_header_name_span.start_byte),
              scan_result.module_header_name_span.end_byte -
                  scan_result.module_header_name_span.start_byte},
          expected_local_name)) {
    if (failure != NULL) *failure = W_SEED_EPHEMERAL_GRAPH_FAILURE_IDENTITY;
    if (span != NULL) *span = scan_result.module_header_name_span;
    return false;
  }
  if (!module_path_shape(document->module_id, NULL, &non_ascii) || non_ascii) {
    if (failure != NULL) {
      *failure = non_ascii ? W_SEED_EPHEMERAL_GRAPH_FAILURE_UNSUPPORTED_NFC
                           : W_SEED_EPHEMERAL_GRAPH_FAILURE_IDENTITY;
    }
    return false;
  }
  return true;
}

static bool scan_document(const w_seed_frontend_document *document,
                          w_seed_module_origin *origins, size_t origin_capacity,
                          size_t *origin_count, w_seed_span *span,
                          w_seed_ephemeral_graph_status *status) {
  if (origin_count != NULL) *origin_count = 0u;
  if (status != NULL) *status = W_SEED_EPHEMERAL_GRAPH_INVALID;
  if (document == NULL || origin_count == NULL || status == NULL ||
      (origin_capacity != 0u && origins == NULL)) {
    return false;
  }
  w_seed_module_scan_result measured;
  const w_seed_module_scan_status measure_status = w_seed_module_scan(
      document->source, document->nodes, document->parse.node_count,
      &document->parse, NULL, 0u, &measured);
  if (measure_status != W_SEED_MODULE_SCAN_CAPACITY &&
      measure_status != W_SEED_MODULE_SCAN_OK) {
    if (span != NULL) *span = document->nodes[document->parse.root].raw_span;
    *status = measure_status == W_SEED_MODULE_SCAN_UNSUPPORTED
                  ? W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED
                  : W_SEED_EPHEMERAL_GRAPH_INVALID;
    return false;
  }
  if (measured.required > (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) {
    if (status != NULL) *status = W_SEED_EPHEMERAL_GRAPH_CAPACITY;
    return false;
  }
  if (measured.required == 0u) {
    *origin_count = 0u;
    *status = W_SEED_EPHEMERAL_GRAPH_OK;
    return true;
  }
  if (origin_capacity < measured.required || origins == NULL) {
    if (status != NULL) *status = W_SEED_EPHEMERAL_GRAPH_CAPACITY;
    return false;
  }
  w_seed_module_scan_result written;
  const w_seed_module_scan_status write_status = w_seed_module_scan(
      document->source, document->nodes, document->parse.node_count,
      &document->parse, origins, origin_capacity, &written);
  if (write_status != W_SEED_MODULE_SCAN_OK) {
    if (span != NULL) *span = document->nodes[document->parse.root].raw_span;
    *status = write_status == W_SEED_MODULE_SCAN_UNSUPPORTED
                  ? W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED
                  : (write_status == W_SEED_MODULE_SCAN_CAPACITY
                         ? W_SEED_EPHEMERAL_GRAPH_CAPACITY
                         : W_SEED_EPHEMERAL_GRAPH_INVALID);
    return false;
  }
  *origin_count = written.written;
  *status = W_SEED_EPHEMERAL_GRAPH_OK;
  return true;
}

static bool import_matches_root(w_seed_frontend_text import_path,
                                w_seed_frontend_text root_module_id) {
  return text_equal(import_path, root_module_id);
}

static int text_compare(w_seed_frontend_text left, w_seed_frontend_text right) {
  const size_t length = left.length < right.length ? left.length : right.length;
  if (length != 0u) {
    const int comparison = memcmp(left.data, right.data, length);
    if (comparison < 0) return -1;
    if (comparison > 0) return 1;
  }
  if (left.length < right.length) return -1;
  if (left.length > right.length) return 1;
  return 0;
}

static size_t candidate_for_import(
    const w_seed_ephemeral_graph_input *input, w_seed_frontend_text import_path,
    size_t *matches) {
  size_t found = SIZE_MAX;
  size_t count = 0u;
  for (size_t index = 0u; index < input->candidate_count; index += 1u) {
    const w_seed_frontend_document *document = &input->documents[index];
    if (!text_view_valid(document->logical_source_id) ||
        !source_id_equals_import(document->logical_source_id, import_path))
      continue;
    found = index;
    count += 1u;
  }
  if (matches != NULL) *matches = count;
  return found;
}

static bool graph_find_candidate(const graph_plan *plan, size_t candidate_index,
                                 size_t *node_index) {
  if (plan == NULL || node_index == NULL) return false;
  for (size_t index = 0u; index < plan->node_count; index += 1u) {
    if (plan->scratch->nodes[index].candidate_index == candidate_index) {
      *node_index = index;
      return true;
    }
  }
  return false;
}

static bool graph_add_node(
    graph_plan *plan, const w_seed_ephemeral_graph_input *input,
    size_t candidate_index, w_seed_frontend_text expected_source_id,
    w_seed_frontend_text expected_module_id, w_seed_frontend_text expected_local,
    bool root_document, size_t *node_index, graph_error *error) {
  const w_seed_frontend_document *document = &input->documents[candidate_index];
  w_seed_span bad_span = empty_span(0u);
  w_seed_ephemeral_graph_failure identity_failure =
      W_SEED_EPHEMERAL_GRAPH_FAILURE_IDENTITY;
  if (!document_matches_expected(document, expected_source_id,
                                 expected_module_id, expected_local,
                                 root_document, &bad_span, &identity_failure)) {
    set_error(error,
              identity_failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_SOURCE
                  ? W_SEED_EPHEMERAL_GRAPH_INVALID
                  : (identity_failure ==
                             W_SEED_EPHEMERAL_GRAPH_FAILURE_UNSUPPORTED_NFC
                         ? W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED
                         : W_SEED_EPHEMERAL_GRAPH_INVALID),
              identity_failure, candidate_index, SIZE_MAX, SIZE_MAX, bad_span);
    return false;
  }
  const w_seed_ephemeral_graph_provider_facts *facts =
      &input->provider_facts[candidate_index];
  uint8_t digest[W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES];
  if (!provider_facts_valid(facts, document, digest)) {
    set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
              provider_failure_kind(facts),
              candidate_index, SIZE_MAX, SIZE_MAX,
              document->nodes[document->parse.root].raw_span);
    return false;
  }
  if (!plan->provider_set) {
    plan->provider_set = true;
    plan->provider_id = facts->provider_id;
    plan->root_token = facts->root_token;
    plan->source_owner_token = facts->source_provider_owner_token;
  } else if (!provider_same(plan, facts)) {
    set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
              W_SEED_EPHEMERAL_GRAPH_FAILURE_PROVIDER, candidate_index,
              SIZE_MAX, SIZE_MAX, document->nodes[document->parse.root].raw_span);
    return false;
  }
  for (size_t prior = 0u; prior < plan->node_count; prior += 1u) {
    const graph_node *existing = &plan->scratch->nodes[prior];
    if (text_equal(existing->source_id, document->logical_source_id) ||
        text_equal(existing->module_id, document->module_id)) {
      set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                W_SEED_EPHEMERAL_GRAPH_FAILURE_IDENTITY, candidate_index,
                SIZE_MAX, SIZE_MAX, document->nodes[document->parse.root].raw_span);
      return false;
    }
    const w_seed_ephemeral_graph_provider_facts *prior_facts =
        &input->provider_facts[existing->candidate_index];
    if (text_equal(prior_facts->canonical_token, facts->canonical_token)) {
      set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                W_SEED_EPHEMERAL_GRAPH_FAILURE_PROVIDER, candidate_index,
                SIZE_MAX, SIZE_MAX, document->nodes[document->parse.root].raw_span);
      return false;
    }
  }
  if (plan->node_count >= input->max_sources) {
    set_error(error, W_SEED_EPHEMERAL_GRAPH_CAPACITY,
              W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT, candidate_index, SIZE_MAX,
              SIZE_MAX, document->nodes[document->parse.root].raw_span);
    return false;
  }
  if (plan->node_count >= plan->scratch->node_capacity) {
    set_error(error, W_SEED_EPHEMERAL_GRAPH_CAPACITY,
              W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT, candidate_index, SIZE_MAX,
              SIZE_MAX, document->nodes[document->parse.root].raw_span);
    return false;
  }
  if (plan->total_source_bytes > input->max_total_source_bytes ||
      document->source->bytes.length >
          input->max_total_source_bytes - plan->total_source_bytes) {
    set_error(error, W_SEED_EPHEMERAL_GRAPH_CAPACITY,
              W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT, candidate_index, SIZE_MAX,
              SIZE_MAX, document->nodes[document->parse.root].raw_span);
    return false;
  }
  const size_t new_index = plan->node_count;
  graph_node *node = &plan->scratch->nodes[new_index];
  node->candidate_index = candidate_index;
  node->depth = 0u;
  node->source_bytes = document->source->bytes.length;
  node->source_id = document->logical_source_id;
  node->module_id = document->module_id;
  node->local_module_name = document->local_module_name;
  (void)memcpy(node->digest, digest, sizeof(node->digest));
  plan->node_count += 1u;
  plan->total_source_bytes += document->source->bytes.length;
  if (node_index != NULL) *node_index = new_index;
  return true;
}

static bool process_plan(graph_plan *plan,
                         const w_seed_ephemeral_graph_input *input,
                         w_seed_frontend_text root_module_id,
                         graph_error *error) {
  size_t source_node = 0u;
  while (source_node < plan->node_count) {
    const graph_node *node = &plan->scratch->nodes[source_node];
    const w_seed_frontend_document *document =
        &input->documents[node->candidate_index];
    size_t origin_count = 0u;
    w_seed_span bad_span = empty_span(0u);
    w_seed_ephemeral_graph_status scan_status = W_SEED_EPHEMERAL_GRAPH_INVALID;
    if (!scan_document(document, plan->scratch->origins,
                       plan->scratch->origin_capacity, &origin_count,
                       &bad_span, &scan_status)) {
      set_error(error, scan_status, scan_status == W_SEED_EPHEMERAL_GRAPH_CAPACITY
                                      ? W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT
                                      : W_SEED_EPHEMERAL_GRAPH_FAILURE_SOURCE,
                node->candidate_index, source_node, SIZE_MAX, bad_span);
      return false;
    }
    for (size_t origin_index = 0u; origin_index < origin_count;
         origin_index += 1u) {
      const w_seed_module_origin *origin =
          &plan->scratch->origins[origin_index];
      if (origin->kind != W_SEED_MODULE_ORIGIN_IMPORT) continue;
      w_seed_frontend_text import_path = {
          (const char *)(document->source->bytes.data +
                         origin->module_path_span.start_byte),
          origin->module_path_span.end_byte - origin->module_path_span.start_byte};
      bool non_ascii = false;
      bool is_std = false;
      if (!module_path_shape(import_path, &is_std, &non_ascii)) {
        set_error(error, non_ascii ? W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED
                                   : W_SEED_EPHEMERAL_GRAPH_INVALID,
                  non_ascii ? W_SEED_EPHEMERAL_GRAPH_FAILURE_UNSUPPORTED_NFC
                            : W_SEED_EPHEMERAL_GRAPH_FAILURE_PATH,
                  node->candidate_index, source_node, origin_index,
                  origin->module_path_span);
        return false;
      }
      if (is_std) {
        set_error(error, W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED,
                  W_SEED_EPHEMERAL_GRAPH_FAILURE_STD_PROVIDER,
                  node->candidate_index, source_node, origin_index,
                  origin->module_path_span);
        return false;
      }
      size_t target_node = SIZE_MAX;
      if (import_matches_root(import_path, root_module_id)) {
        target_node = 0u;
      } else {
        size_t matches = 0u;
        const size_t candidate =
            candidate_for_import(input, import_path, &matches);
        if (matches == 0u) {
          set_error(error, W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED,
                    W_SEED_EPHEMERAL_GRAPH_FAILURE_MISSING_LOCAL,
                    node->candidate_index, source_node, origin_index,
                    origin->module_path_span);
          return false;
        }
        if (matches != 1u) {
          set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                    W_SEED_EPHEMERAL_GRAPH_FAILURE_IDENTITY, candidate,
                    source_node, origin_index, origin->module_path_span);
          return false;
        }
        if (!graph_find_candidate(plan, candidate, &target_node)) {
          w_seed_frontend_text expected_local = {NULL, 0u};
          if (!module_last_component(import_path, &expected_local)) {
            set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                      W_SEED_EPHEMERAL_GRAPH_FAILURE_PATH,
                      node->candidate_index, source_node, origin_index,
                      origin->module_path_span);
            return false;
          }
          if (!graph_add_node(plan, input, candidate,
                              (w_seed_frontend_text){
                                  (const char *)(input->documents[candidate]
                                                     .logical_source_id.data),
                                  input->documents[candidate]
                                      .logical_source_id.length},
                              import_path, expected_local, false, &target_node,
                              error)) {
            /* The candidate source ID is checked against the import below.
             * graph_add_node receives it only to keep all text caller-owned. */
            return false;
          }
          if (!source_id_equals_import(
                  input->documents[candidate].logical_source_id, import_path)) {
            set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                      W_SEED_EPHEMERAL_GRAPH_FAILURE_IDENTITY, candidate,
                      source_node, origin_index, origin->module_path_span);
            return false;
          }
        }
      }
      if (target_node == SIZE_MAX || target_node >= plan->node_count) {
        set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                  W_SEED_EPHEMERAL_GRAPH_FAILURE_INDEX,
                  node->candidate_index, source_node, origin_index,
                  origin->module_path_span);
        return false;
      }
      if (plan->edge_count >= input->max_edges) {
        set_error(error, W_SEED_EPHEMERAL_GRAPH_CAPACITY,
                  W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT,
                  node->candidate_index, source_node, origin_index,
                  origin->declaration_span);
        return false;
      }
      if (plan->edge_count >= plan->scratch->edge_capacity) {
        set_error(error, W_SEED_EPHEMERAL_GRAPH_CAPACITY,
                  W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT,
                  node->candidate_index, source_node, origin_index,
                  origin->declaration_span);
        return false;
      }
      graph_edge *edge = &plan->scratch->edges[plan->edge_count];
      edge->source_node = source_node;
      edge->target_node = target_node;
      edge->direct_import_ordinal = origin->direct_import_ordinal;
      edge->declaration_span = origin->declaration_span;
      edge->path_span = origin->module_path_span;
      edge->logical_origin = origin->kind;
      plan->edge_count += 1u;
    }
    source_node += 1u;
  }
  return true;
}

static w_seed_frontend_text edge_origin_view(
    const graph_plan *plan, const w_seed_ephemeral_graph_input *input,
    const graph_edge *edge) {
  const graph_node *source = &plan->scratch->nodes[edge->source_node];
  const w_seed_frontend_document *document =
      &input->documents[source->candidate_index];
  return (w_seed_frontend_text){
      (const char *)(document->source->bytes.data + edge->path_span.start_byte),
      edge->path_span.end_byte - edge->path_span.start_byte};
}

static int logical_edge_compare(
    const graph_plan *plan, const w_seed_ephemeral_graph_input *input,
    size_t left_index, size_t right_index) {
  const graph_edge *left = &plan->scratch->edges[left_index];
  const graph_edge *right = &plan->scratch->edges[right_index];
  const size_t left_source = plan->scratch->node_ordinals[left->source_node];
  const size_t right_source = plan->scratch->node_ordinals[right->source_node];
  if (left_source < right_source) return -1;
  if (left_source > right_source) return 1;
  const int origin_comparison =
      text_compare(edge_origin_view(plan, input, left),
                   edge_origin_view(plan, input, right));
  if (origin_comparison != 0) return origin_comparison;
  const size_t left_target = plan->scratch->node_ordinals[left->target_node];
  const size_t right_target = plan->scratch->node_ordinals[right->target_node];
  if (left_target < right_target) return -1;
  if (left_target > right_target) return 1;
  if (left->logical_origin < right->logical_origin) return -1;
  if (left->logical_origin > right->logical_origin) return 1;
  if (left->direct_import_ordinal < right->direct_import_ordinal) return -1;
  if (left->direct_import_ordinal > right->direct_import_ordinal) return 1;
  if (left_index < right_index) return -1;
  if (left_index > right_index) return 1;
  return 0;
}

/* Stable bottom-up mergesort keeps the public logical edge order bounded at
 * O(E log E). The resolved-order array is the caller-owned merge buffer and
 * is rebuilt after this pass. */
static void sort_logical_edges(
    graph_plan *plan, const w_seed_ephemeral_graph_input *input) {
  const size_t edge_count = plan->edge_count;
  size_t *from = plan->scratch->sorted_edges;
  size_t *to = plan->scratch->sorted_resolved_edges;
  for (size_t index = 0u; index < edge_count; index += 1u) from[index] = index;
  size_t width = 1u;
  while (width < edge_count) {
    size_t left = 0u;
    while (left < edge_count) {
      size_t middle = left + width;
      if (middle > edge_count) middle = edge_count;
      size_t right = middle + width;
      if (right > edge_count) right = edge_count;
      size_t first = left;
      size_t second = middle;
      size_t output = left;
      while (first < middle && second < right) {
        if (logical_edge_compare(plan, input, from[second], from[first]) < 0)
          to[output++] = from[second++];
        else
          to[output++] = from[first++];
      }
      while (first < middle) to[output++] = from[first++];
      while (second < right) to[output++] = from[second++];
      left = right;
    }
    size_t *swap = from;
    from = to;
    to = swap;
    if (width > edge_count / 2u) break;
    width *= 2u;
  }
  if (from != plan->scratch->sorted_edges)
    (void)memcpy(plan->scratch->sorted_edges, from,
                 edge_count * sizeof(*from));
}

static bool reconstruct_resolved_edges(graph_plan *plan, graph_error *error) {
  w_seed_ephemeral_graph_scratch *scratch = plan->scratch;
  for (size_t source = 0u; source < plan->node_count; source += 1u)
    scratch->indegree[source] = 0u;
  for (size_t index = 0u; index < plan->edge_count; index += 1u) {
    const graph_edge *edge = &scratch->edges[index];
    if (edge->source_node >= plan->node_count ||
        edge->target_node >= plan->node_count) {
      set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                W_SEED_EPHEMERAL_GRAPH_FAILURE_INDEX, SIZE_MAX,
                edge->source_node, index, edge->declaration_span);
      return false;
    }
    const size_t source_ordinal = scratch->node_ordinals[edge->source_node];
    if (source_ordinal >= plan->node_count ||
        scratch->indegree[source_ordinal] == UINT32_MAX) {
      set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                W_SEED_EPHEMERAL_GRAPH_FAILURE_ORDER, SIZE_MAX,
                edge->source_node, index, edge->declaration_span);
      return false;
    }
    scratch->indegree[source_ordinal] += 1u;
  }
  size_t offset = 0u;
  for (size_t source = 0u; source < plan->node_count; source += 1u) {
    if (offset > (size_t)UINT32_MAX ||
        scratch->indegree[source] >
            (uint32_t)((size_t)UINT32_MAX - offset)) {
      set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                W_SEED_EPHEMERAL_GRAPH_FAILURE_ORDER, SIZE_MAX, SIZE_MAX,
                SIZE_MAX, empty_span(0u));
      return false;
    }
    scratch->queue[source] = (uint32_t)offset;
    offset += (size_t)scratch->indegree[source];
  }
  if (offset != plan->edge_count) {
    set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
              W_SEED_EPHEMERAL_GRAPH_FAILURE_ORDER, SIZE_MAX, SIZE_MAX,
              SIZE_MAX, empty_span(0u));
    return false;
  }
  for (size_t index = 0u; index < plan->edge_count; index += 1u)
    scratch->sorted_resolved_edges[index] = SIZE_MAX;
  for (size_t index = 0u; index < plan->edge_count; index += 1u) {
    const graph_edge *edge = &scratch->edges[index];
    const size_t source_ordinal = scratch->node_ordinals[edge->source_node];
    if (edge->direct_import_ordinal >= scratch->indegree[source_ordinal]) {
      set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                W_SEED_EPHEMERAL_GRAPH_FAILURE_ORDER, SIZE_MAX,
                edge->source_node, index, edge->declaration_span);
      return false;
    }
    const size_t resolved_index =
        (size_t)scratch->queue[source_ordinal] +
        (size_t)edge->direct_import_ordinal;
    if (resolved_index >= plan->edge_count ||
        scratch->sorted_resolved_edges[resolved_index] != SIZE_MAX) {
      set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                W_SEED_EPHEMERAL_GRAPH_FAILURE_ORDER, SIZE_MAX,
                edge->source_node, index, edge->declaration_span);
      return false;
    }
    scratch->sorted_resolved_edges[resolved_index] = index;
  }
  for (size_t index = 0u; index < plan->edge_count; index += 1u) {
    if (scratch->sorted_resolved_edges[index] == SIZE_MAX) {
      set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                W_SEED_EPHEMERAL_GRAPH_FAILURE_ORDER, SIZE_MAX, SIZE_MAX,
                index, empty_span(0u));
      return false;
    }
  }
  return true;
}

static bool finalize_graph(graph_plan *plan,
                           const w_seed_ephemeral_graph_input *input,
                           graph_error *error) {
  w_seed_ephemeral_graph_scratch *scratch = plan->scratch;
  if (plan->node_count > scratch->indegree_capacity ||
      plan->node_count > scratch->queue_capacity ||
      plan->node_count > scratch->depths_capacity ||
      plan->node_count > scratch->sorted_nodes_capacity ||
      plan->node_count > scratch->node_ordinals_capacity ||
      plan->edge_count > scratch->sorted_edges_capacity ||
      plan->edge_count > scratch->sorted_resolved_edges_capacity) {
    set_error(error, W_SEED_EPHEMERAL_GRAPH_CAPACITY,
              W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT, SIZE_MAX, SIZE_MAX,
              SIZE_MAX, empty_span(0u));
    return false;
  }
  uint32_t *indegree = scratch->indegree;
  uint32_t *queue = scratch->queue;
  uint32_t *depths = scratch->depths;
  if (plan->node_count != 0u) {
    (void)memset(indegree, 0, plan->node_count * sizeof(*indegree));
    (void)memset(queue, 0, plan->node_count * sizeof(*queue));
    (void)memset(depths, 0, plan->node_count * sizeof(*depths));
  }
  size_t queue_count = 0u;
  size_t queue_cursor = 0u;
  for (size_t index = 0u; index < plan->edge_count; index += 1u) {
    const graph_edge *edge = &plan->scratch->edges[index];
    if (edge->source_node == edge->target_node ||
        edge->source_node >= plan->node_count ||
        edge->target_node >= plan->node_count ||
        indegree[edge->target_node] == UINT32_MAX) {
      set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
                W_SEED_EPHEMERAL_GRAPH_FAILURE_CYCLE, SIZE_MAX,
                edge->source_node, index, edge->declaration_span);
      return false;
    }
    indegree[edge->target_node] += 1u;
  }
  for (size_t index = 0u; index < plan->node_count; index += 1u) {
    if (indegree[index] == 0u) queue[queue_count++] = (uint32_t)index;
  }
  size_t processed = 0u;
  while (queue_cursor < queue_count) {
    const size_t source = queue[queue_cursor++];
    processed += 1u;
    for (size_t edge_index = 0u; edge_index < plan->edge_count; edge_index += 1u) {
      graph_edge *edge = &plan->scratch->edges[edge_index];
      if (edge->source_node != source) continue;
      const uint32_t candidate_depth = depths[source] + 1u;
      if (candidate_depth > depths[edge->target_node])
        depths[edge->target_node] = candidate_depth;
      indegree[edge->target_node] -= 1u;
      if (indegree[edge->target_node] == 0u)
        queue[queue_count++] = (uint32_t)edge->target_node;
    }
  }
  if (processed != plan->node_count) {
    set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
              W_SEED_EPHEMERAL_GRAPH_FAILURE_CYCLE, SIZE_MAX, SIZE_MAX,
              SIZE_MAX, empty_span(0u));
    return false;
  }
  for (size_t index = 0u; index < plan->node_count; index += 1u) {
    if ((size_t)depths[index] > input->max_depth) {
      set_error(error, W_SEED_EPHEMERAL_GRAPH_CAPACITY,
                W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT,
                plan->scratch->nodes[index].candidate_index, index, SIZE_MAX,
                empty_span(0u));
      return false;
    }
    plan->scratch->nodes[index].depth = depths[index];
  }
  plan->scratch->sorted_nodes[0u] = 0u;
  size_t sorted_count = 1u;
  for (size_t node = 1u; node < plan->node_count; node += 1u) {
    size_t position = sorted_count;
    while (position > 1u) {
      const size_t prior = plan->scratch->sorted_nodes[position - 1u];
      if (text_compare(plan->scratch->nodes[prior].source_id,
                       plan->scratch->nodes[node].source_id) <= 0)
        break;
      plan->scratch->sorted_nodes[position] = prior;
      position -= 1u;
    }
    plan->scratch->sorted_nodes[position] = node;
    sorted_count += 1u;
  }
  for (size_t ordinal = 0u; ordinal < plan->node_count; ordinal += 1u)
    plan->scratch->node_ordinals[plan->scratch->sorted_nodes[ordinal]] = ordinal;
  sort_logical_edges(plan, input);
  if (!reconstruct_resolved_edges(plan, error)) return false;
  return true;
}

static bool build_plan(const w_seed_ephemeral_graph_input *input,
                       graph_plan *plan, graph_error *error) {
  if (plan == NULL || error == NULL) return false;
  (void)memset(plan, 0, sizeof(*plan));
  set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
            W_SEED_EPHEMERAL_GRAPH_FAILURE_NONE, SIZE_MAX, SIZE_MAX, SIZE_MAX,
            empty_span(0u));
  if (!input_shape_valid(input)) {
    set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID, input_failure_kind(input),
              SIZE_MAX, SIZE_MAX, SIZE_MAX, empty_span(0u));
    return false;
  }
  plan->scratch = input->scratch;
  const w_seed_frontend_document *root =
      &input->documents[input->root_candidate_index];
  w_seed_span root_span = empty_span(0u);
  if (!source_and_document_shape(root, &root_span)) {
    set_error(error, W_SEED_EPHEMERAL_GRAPH_INVALID,
              W_SEED_EPHEMERAL_GRAPH_FAILURE_SOURCE,
              input->root_candidate_index, SIZE_MAX, SIZE_MAX, root_span);
    return false;
  }
  bool non_ascii = false;
  if (!root_source_id_shape(root->logical_source_id, &non_ascii)) {
    set_error(error, non_ascii ? W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED
                               : W_SEED_EPHEMERAL_GRAPH_INVALID,
              non_ascii ? W_SEED_EPHEMERAL_GRAPH_FAILURE_UNSUPPORTED_NFC
                        : W_SEED_EPHEMERAL_GRAPH_FAILURE_PATH,
              input->root_candidate_index, SIZE_MAX, SIZE_MAX, root_span);
    return false;
  }
  w_seed_module_scan_result root_scan_result;
  const w_seed_module_scan_status root_scan = w_seed_module_scan(
      root->source, root->nodes, root->parse.node_count, &root->parse,
      NULL, 0u, &root_scan_result);
  if (root_scan != W_SEED_MODULE_SCAN_OK &&
      root_scan != W_SEED_MODULE_SCAN_CAPACITY) {
    set_error(error, root_scan == W_SEED_MODULE_SCAN_UNSUPPORTED
                         ? W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED
                         : W_SEED_EPHEMERAL_GRAPH_INVALID,
              root_scan == W_SEED_MODULE_SCAN_UNSUPPORTED
                  ? W_SEED_EPHEMERAL_GRAPH_FAILURE_PATH
                  : W_SEED_EPHEMERAL_GRAPH_FAILURE_SOURCE,
              input->root_candidate_index, SIZE_MAX, SIZE_MAX, root_span);
    return false;
  }
  if (root_scan_result.required >
      (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) {
    set_error(error, W_SEED_EPHEMERAL_GRAPH_CAPACITY,
              W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT,
              input->root_candidate_index, SIZE_MAX, SIZE_MAX, root_span);
    return false;
  }
  w_seed_frontend_text root_module_id = {NULL, 0u};
  w_seed_frontend_text root_local_name = {NULL, 0u};
  if (root_scan_result.has_module_header_name) {
    root_module_id.data = (const char *)(root->source->bytes.data +
                                         root_scan_result.module_header_name_span
                                             .start_byte);
    root_module_id.length = root_scan_result.module_header_name_span.end_byte -
                            root_scan_result.module_header_name_span.start_byte;
  } else {
    root_module_id.data = root->logical_source_id.data;
    root_module_id.length = root->logical_source_id.length - 2u;
  }
  if (!module_path_shape(root_module_id, NULL, &non_ascii) || non_ascii ||
      !module_last_component(root_module_id, &root_local_name) ||
      !text_equal(root->module_id, root_module_id) ||
      !text_equal(root->local_module_name, root_local_name)) {
    set_error(error, non_ascii ? W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED
                               : W_SEED_EPHEMERAL_GRAPH_INVALID,
              non_ascii ? W_SEED_EPHEMERAL_GRAPH_FAILURE_UNSUPPORTED_NFC
                        : W_SEED_EPHEMERAL_GRAPH_FAILURE_IDENTITY,
              input->root_candidate_index, SIZE_MAX, SIZE_MAX, root_span);
    return false;
  }
  size_t root_node = SIZE_MAX;
  if (!graph_add_node(plan, input, input->root_candidate_index,
                      root->logical_source_id, root_module_id, root_local_name,
                      true, &root_node, error)) {
    return false;
  }
  if (root_node != 0u) return false;
  if (!process_plan(plan, input, root_module_id, error)) return false;
  return finalize_graph(plan, input, error);
}

static void copy_error_to_result(const graph_error *error,
                                 w_seed_ephemeral_graph_result *result) {
  if (error == NULL || result == NULL) return;
  result->status = error->status;
  result->failure = error->failure;
  result->candidate_index = error->candidate_index;
  result->document_ordinal = error->document_ordinal;
  result->edge_ordinal = error->edge_ordinal;
  result->span = error->span;
}

static bool output_capacity_valid(const w_seed_ephemeral_graph_output *output,
                                  const graph_plan *plan) {
  if (output == NULL || plan == NULL) return false;
  return (plan->node_count == 0u ||
          (output->inventory != NULL &&
           output->inventory_capacity >= plan->node_count)) &&
         (plan->edge_count == 0u ||
          (output->edges != NULL && output->edge_capacity >= plan->edge_count)) &&
         (plan->node_count == 0u ||
          (output->document_order != NULL &&
           output->document_order_capacity >= plan->node_count)) &&
         (plan->edge_count == 0u ||
          (output->resolved_imports != NULL &&
           output->resolved_import_capacity >= plan->edge_count));
}

static void publish_plan(const graph_plan *plan,
                         const w_seed_ephemeral_graph_input *input,
                         w_seed_ephemeral_graph_output *output) {
  for (size_t ordinal = 0u; ordinal < plan->node_count; ordinal += 1u) {
    const graph_node *node =
        &plan->scratch->nodes[plan->scratch->sorted_nodes[ordinal]];
    output->inventory[ordinal] = (w_seed_ephemeral_graph_inventory_item){
        node->source_id,       node->module_id, node->local_module_name,
        {0u},                   (uint32_t)node->candidate_index, node->depth};
    (void)memcpy(output->inventory[ordinal].digest, node->digest,
                 sizeof(node->digest));
    output->document_order[ordinal] = (uint32_t)node->candidate_index;
  }
  for (size_t ordinal = 0u; ordinal < plan->edge_count; ordinal += 1u) {
    const graph_edge *edge =
        &plan->scratch->edges[plan->scratch->sorted_edges[ordinal]];
    const uint32_t source_ordinal =
        (uint32_t)plan->scratch->node_ordinals[edge->source_node];
    const uint32_t target_ordinal =
        (uint32_t)plan->scratch->node_ordinals[edge->target_node];
    output->edges[ordinal] = (w_seed_ephemeral_graph_edge){
        source_ordinal, target_ordinal, edge->direct_import_ordinal,
        edge->declaration_span, edge->path_span, edge->logical_origin};
  }
  for (size_t ordinal = 0u; ordinal < plan->edge_count; ordinal += 1u) {
    const graph_edge *edge =
        &plan->scratch
             ->edges[plan->scratch->sorted_resolved_edges[ordinal]];
    const uint32_t source_ordinal =
        (uint32_t)plan->scratch->node_ordinals[edge->source_node];
    const uint32_t target_ordinal =
        (uint32_t)plan->scratch->node_ordinals[edge->target_node];
    output->resolved_imports[ordinal] = (w_seed_frontend_resolved_import){
        source_ordinal,
        edge->direct_import_ordinal,
        edge->declaration_span,
        W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT,
        target_ordinal};
  }
  (void)input;
}

w_seed_ephemeral_graph_status w_seed_ephemeral_graph_measure(
    const w_seed_ephemeral_graph_input *input,
    w_seed_ephemeral_graph_counts *counts,
    w_seed_ephemeral_graph_result *result) {
  clear_counts(counts);
  clear_result(result);
  if (input == NULL || counts == NULL || result == NULL) {
    if (result != NULL) {
      result->status = W_SEED_EPHEMERAL_GRAPH_INVALID;
      result->failure = W_SEED_EPHEMERAL_GRAPH_FAILURE_POINTER;
    }
    return W_SEED_EPHEMERAL_GRAPH_INVALID;
  }
  graph_plan plan;
  graph_error error;
  if (!build_plan(input, &plan, &error)) {
    copy_error_to_result(&error, result);
    result->required.sources = plan.node_count;
    result->required.edges = plan.edge_count;
    result->required.total_source_bytes = plan.total_source_bytes;
    *counts = result->required;
    return result->status;
  }
  counts->sources = plan.node_count;
  counts->edges = plan.edge_count;
  counts->total_source_bytes = plan.total_source_bytes;
  result->status = W_SEED_EPHEMERAL_GRAPH_OK;
  result->failure = W_SEED_EPHEMERAL_GRAPH_FAILURE_NONE;
  result->required = *counts;
  return result->status;
}

w_seed_ephemeral_graph_status w_seed_ephemeral_graph_write(
    const w_seed_ephemeral_graph_input *input,
    w_seed_ephemeral_graph_output *output,
    w_seed_ephemeral_graph_result *result) {
  clear_result(result);
  if (input == NULL || output == NULL || result == NULL) {
    if (result != NULL) result->failure = W_SEED_EPHEMERAL_GRAPH_FAILURE_POINTER;
    return W_SEED_EPHEMERAL_GRAPH_INVALID;
  }
  graph_plan plan;
  graph_error error;
  if (!build_plan(input, &plan, &error)) {
    copy_error_to_result(&error, result);
    result->required.sources = plan.node_count;
    result->required.edges = plan.edge_count;
    result->required.total_source_bytes = plan.total_source_bytes;
    return result->status;
  }
  result->required = (w_seed_ephemeral_graph_counts){
      plan.node_count, plan.edge_count, plan.total_source_bytes};
  if (!output_capacity_valid(output, &plan)) {
    result->status = W_SEED_EPHEMERAL_GRAPH_CAPACITY;
    result->failure = W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT;
    return result->status;
  }
  publish_plan(&plan, input, output);
  result->status = W_SEED_EPHEMERAL_GRAPH_OK;
  result->failure = W_SEED_EPHEMERAL_GRAPH_FAILURE_NONE;
  result->written = result->required;
  return result->status;
}
