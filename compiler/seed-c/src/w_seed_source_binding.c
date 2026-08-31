#include "w_seed_source_binding.h"

#include <string.h>

#include "w_seed_sha256.h"

enum { BINDING_RANGE_CAPACITY = 2048u };

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool present;
} binding_range;

typedef struct {
  const w_seed_ephemeral_graph_provider_facts *root_facts;
  const w_seed_ephemeral_graph_provider_facts *facts;
  size_t fact_count;
  const w_seed_source *root_source;
  uint8_t source_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES];
  uint8_t root_facts_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES];
} acquisition_view;

typedef struct {
  const w_seed_owner_guard *guard;
  const w_seed_manifest_backend *backend;
  uint64_t generation;
  uint8_t receipt_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES];
  uint8_t context_binding_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES];
  uint8_t candidate_binding_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES];
} manifest_view;

static w_seed_source_binding_result binding_result(
    w_seed_source_binding_status status, w_seed_source_binding_phase phase,
    w_seed_source_binding_lifecycle lifecycle, uint64_t generation) {
  return (w_seed_source_binding_result){status, phase, lifecycle, generation};
}

static bool text_valid(w_seed_frontend_text text) {
  return text.length == 0u || text.data != NULL;
}

static bool text_nonempty(w_seed_frontend_text text) {
  return text.length != 0u && text.data != NULL;
}

static bool text_equal(w_seed_frontend_text left, w_seed_frontend_text right) {
  return left.length == right.length &&
         (left.length == 0u || memcmp(left.data, right.data, left.length) == 0);
}

static bool bytes_nonzero(const uint8_t *bytes, size_t length) {
  if (bytes == NULL) return false;
  for (size_t index = 0u; index < length; index += 1u)
    if (bytes[index] != 0u) return true;
  return false;
}

static bool range_make(const void *pointer, size_t bytes, binding_range *range) {
  if (range == NULL) return false;
  *range = (binding_range){0u, 0u, false};
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

static bool range_overlap(binding_range left, binding_range right) {
  return left.present && right.present && left.start < right.end &&
         right.start < left.end;
}

static bool array_range(const void *pointer, size_t count, size_t element_size,
                        binding_range *range) {
  if (element_size != 0u && count > SIZE_MAX / element_size) return false;
  return range_make(pointer, count * element_size, range);
}

static bool output_disjoint_from_range(const binding_range *output,
                                       const void *pointer, size_t count,
                                       size_t element_size) {
  binding_range candidate;
  return output != NULL && array_range(pointer, count, element_size,
                                       &candidate) &&
         !range_overlap(*output, candidate);
}

static bool binding_shallow_disjoint(const w_seed_source_binding_input *input,
                                     const w_seed_source_binding *binding);

static bool input_envelope_bounds(const w_seed_source_binding_input *input);

/* The producer descriptors may intentionally alias their own borrowed views.
 * This pass only rejects an output binding that aliases any descriptor or
 * declared backing. It therefore does not duplicate producer alias policy. */
static bool binding_output_disjoint(const w_seed_source_binding_input *input,
                                    const w_seed_source_binding *binding) {
  if (input == NULL || binding == NULL) return false;
  /* Keep this helper safe when reused: no nested descriptor is followed until
   * the complete top-level object graph and its declared capacities pass the
   * shallow/range envelope. compose_impl repeats these inexpensive checks only
   * to preserve its more precise INVALID versus ALIAS result mapping. */
  if (!binding_shallow_disjoint(input, binding) ||
      !input_envelope_bounds(input))
    return false;
  binding_range output;
  if (!range_make(binding, sizeof(*binding), &output)) return false;
#define CHECK_OBJECT(pointer)                                                   \
  if (!output_disjoint_from_range(&output, (pointer), 1u, sizeof(*(pointer)))) \
    return false
#define CHECK_ARRAY(pointer, count, type)                                       \
  if (!output_disjoint_from_range(&output, (pointer), (count), sizeof(type)))  \
    return false

  CHECK_OBJECT(input);
  CHECK_OBJECT(input->acquisition.pipeline);
  CHECK_OBJECT(input->acquisition.result);
  CHECK_OBJECT(input->manifest.input);
  CHECK_OBJECT(input->manifest.program);
  CHECK_OBJECT(input->manifest.result);
  CHECK_OBJECT(input->manifest.verify_scratch);
  CHECK_OBJECT(input->link);

  const w_seed_acquisition_pipeline_input *pipeline =
      input->acquisition.pipeline;
  CHECK_OBJECT(pipeline->driver_input);
  CHECK_OBJECT(pipeline->scratch);
  CHECK_OBJECT(pipeline->output);
  CHECK_OBJECT(pipeline->storage);
  const w_seed_ephemeral_driver_input *driver_input = pipeline->driver_input;
  const w_seed_ephemeral_driver_scratch *driver_scratch = pipeline->scratch;
  const w_seed_ephemeral_driver_output *driver_output = pipeline->output;
  const w_seed_acquisition_storage *acquisition_storage = pipeline->storage;
  CHECK_ARRAY(driver_input->root_path.data, driver_input->root_path.length,
              uint8_t);
  CHECK_ARRAY(driver_input->root_source_id.data,
              driver_input->root_source_id.length, char);
  CHECK_ARRAY(driver_input->backend.context, pipeline->backend_context_size,
              uint8_t);
  CHECK_ARRAY(driver_scratch->slots, driver_scratch->slot_capacity,
              w_seed_ephemeral_driver_slot);
  CHECK_ARRAY(driver_scratch->requests, driver_scratch->request_capacity,
              w_seed_ephemeral_provider_request);
  CHECK_ARRAY(driver_scratch->lexer_frames, driver_scratch->lexer_frame_capacity,
              w_seed_lexer_frame);
  CHECK_ARRAY(driver_scratch->tokens, driver_scratch->token_capacity,
              w_seed_parse_token);
  CHECK_ARRAY(driver_scratch->parse_frames, driver_scratch->parse_frame_capacity,
              w_seed_parse_frame);
  CHECK_ARRAY(driver_scratch->issues, driver_scratch->issue_capacity,
              w_seed_parse_issue);
  CHECK_ARRAY(driver_scratch->origins, driver_scratch->origin_capacity,
              w_seed_module_origin);
  CHECK_ARRAY(driver_scratch->candidate_documents,
              driver_scratch->candidate_document_capacity,
              w_seed_frontend_document);
  CHECK_ARRAY(driver_scratch->candidate_facts,
              driver_scratch->candidate_fact_capacity,
              w_seed_ephemeral_graph_provider_facts);
  CHECK_OBJECT(driver_scratch->graph_scratch);
  const w_seed_ephemeral_graph_scratch *graph = driver_scratch->graph_scratch;
  CHECK_ARRAY(graph->nodes, graph->node_capacity,
              w_seed_ephemeral_graph_scratch_node);
  CHECK_ARRAY(graph->edges, graph->edge_capacity,
              w_seed_ephemeral_graph_scratch_edge);
  CHECK_ARRAY(graph->sorted_nodes, graph->sorted_nodes_capacity, size_t);
  CHECK_ARRAY(graph->node_ordinals, graph->node_ordinals_capacity, size_t);
  CHECK_ARRAY(graph->sorted_edges, graph->sorted_edges_capacity, size_t);
  CHECK_ARRAY(graph->sorted_resolved_edges,
              graph->sorted_resolved_edges_capacity, size_t);
  CHECK_ARRAY(graph->origins, graph->origin_capacity, w_seed_module_origin);
  CHECK_ARRAY(graph->indegree, graph->indegree_capacity, uint32_t);
  CHECK_ARRAY(graph->queue, graph->queue_capacity, uint32_t);
  CHECK_ARRAY(graph->depths, graph->depths_capacity, uint32_t);
  CHECK_ARRAY(driver_output->documents, driver_output->document_capacity,
              w_seed_frontend_document);
  CHECK_ARRAY(driver_output->graph.inventory,
              driver_output->graph.inventory_capacity,
              w_seed_ephemeral_graph_inventory_item);
  CHECK_ARRAY(driver_output->graph.edges, driver_output->graph.edge_capacity,
              w_seed_ephemeral_graph_edge);
  CHECK_ARRAY(driver_output->graph.document_order,
              driver_output->graph.document_order_capacity, uint32_t);
  CHECK_ARRAY(driver_output->graph.resolved_imports,
              driver_output->graph.resolved_import_capacity,
              w_seed_frontend_resolved_import);
  for (size_t index = 0u;
       index < (size_t)W_SEED_ACQUISITION_MAX_SOURCES; index += 1u) {
    CHECK_ARRAY(acquisition_storage->staging_bytes[index],
                acquisition_storage->staging_capacity[index], uint8_t);
    CHECK_ARRAY(acquisition_storage->revalidation_bytes[index],
                acquisition_storage->revalidation_capacity[index], uint8_t);
    CHECK_ARRAY(acquisition_storage->published_bytes[index],
                acquisition_storage->published_capacity[index], uint8_t);
    CHECK_ARRAY(acquisition_storage->nodes[index],
                acquisition_storage->node_capacity[index], w_seed_cst_node);
  }
  for (size_t index = 0u; index < driver_scratch->slot_capacity; index += 1u) {
    const w_seed_ephemeral_driver_slot *slot = &driver_scratch->slots[index];
    CHECK_ARRAY(slot->source_id_storage, slot->source_id_capacity, char);
    CHECK_ARRAY(slot->module_id_storage, slot->module_id_capacity, char);
    CHECK_ARRAY(slot->nodes, slot->node_capacity, w_seed_cst_node);
    CHECK_ARRAY(slot->source.bytes.data, slot->source.bytes.length, uint8_t);
    CHECK_ARRAY(slot->facts.provider_id.data, slot->facts.provider_id.length,
                char);
    CHECK_ARRAY(slot->facts.root_token.data, slot->facts.root_token.length,
                char);
    CHECK_ARRAY(slot->facts.source_provider_owner_token.data,
                slot->facts.source_provider_owner_token.length, char);
    CHECK_ARRAY(slot->facts.canonical_token.data,
                slot->facts.canonical_token.length, char);
  }
  for (size_t index = 0u; index < driver_scratch->request_capacity;
       index += 1u) {
    const w_seed_ephemeral_provider_request *request =
        &driver_scratch->requests[index];
    CHECK_ARRAY(request->source_id.data, request->source_id.length, char);
    CHECK_ARRAY(request->staging_bytes, request->staging_capacity, uint8_t);
    CHECK_ARRAY(request->revalidation_bytes, request->revalidation_capacity,
                uint8_t);
    CHECK_ARRAY(request->bytes, request->byte_capacity, uint8_t);
    CHECK_ARRAY(request->tokens.provider_id,
                request->tokens.provider_id_capacity, char);
    CHECK_ARRAY(request->tokens.root_token, request->tokens.root_token_capacity,
                char);
    CHECK_ARRAY(request->tokens.source_provider_owner_token,
                request->tokens.source_provider_owner_token_capacity, char);
    CHECK_ARRAY(request->tokens.canonical_token,
                request->tokens.canonical_token_capacity, char);
    CHECK_ARRAY(request->revalidation_tokens.provider_id,
                request->revalidation_tokens.provider_id_capacity, char);
    CHECK_ARRAY(request->revalidation_tokens.root_token,
                request->revalidation_tokens.root_token_capacity, char);
    CHECK_ARRAY(request->revalidation_tokens.source_provider_owner_token,
                request->revalidation_tokens.source_provider_owner_token_capacity,
                char);
    CHECK_ARRAY(request->revalidation_tokens.canonical_token,
                request->revalidation_tokens.canonical_token_capacity, char);
  }

  const w_seed_manifest_guarded_input *manifest_input =
      input->manifest.input;
  const w_seed_manifest_program *program = input->manifest.program;
  const w_seed_manifest_guarded_storage *manifest_storage =
      &manifest_input->storage;
  CHECK_OBJECT(manifest_input->guard);
  CHECK_OBJECT(manifest_input->backend);
  CHECK_OBJECT(&manifest_input->storage);
  CHECK_ARRAY(manifest_storage->read_slots,
              manifest_storage->read_slot_capacity,
              w_seed_manifest_read_slot);
  CHECK_ARRAY(manifest_storage->staged_sources,
              manifest_storage->staged_source_capacity,
              w_seed_manifest_source_input);
  for (size_t index = 0u; index < manifest_storage->read_slot_capacity;
       index += 1u) {
    const w_seed_manifest_read_slot *slot = &manifest_storage->read_slots[index];
    CHECK_ARRAY(slot->first_bytes, slot->first_capacity, uint8_t);
    CHECK_ARRAY(slot->second_bytes, slot->second_capacity, uint8_t);
  }
  for (size_t index = 0u; index < manifest_storage->staged_source_capacity;
       index += 1u) {
    const w_seed_manifest_source_input *source =
        &manifest_storage->staged_sources[index];
    CHECK_ARRAY(source->bytes.data, source->bytes.length, uint8_t);
  }
  CHECK_ARRAY(manifest_storage->staged.documents,
              manifest_storage->staged.document_capacity,
              w_seed_manifest_document);
  CHECK_ARRAY(manifest_storage->staged.roots,
              manifest_storage->staged.root_capacity, w_seed_manifest_root);
  CHECK_ARRAY(manifest_storage->staged.nodes,
              manifest_storage->staged.node_capacity, w_seed_manifest_node);
  CHECK_ARRAY(manifest_storage->staged.fields,
              manifest_storage->staged.field_capacity, w_seed_manifest_field);
  CHECK_ARRAY(manifest_storage->staged.edges,
              manifest_storage->staged.edge_capacity, w_seed_manifest_edge);
  CHECK_ARRAY(manifest_storage->staged.canonical_bytes,
              manifest_storage->staged.canonical_byte_capacity, uint8_t);
  CHECK_ARRAY(manifest_storage->published.documents,
              manifest_storage->published.document_capacity,
              w_seed_manifest_document);
  CHECK_ARRAY(manifest_storage->published.roots,
              manifest_storage->published.root_capacity, w_seed_manifest_root);
  CHECK_ARRAY(manifest_storage->published.nodes,
              manifest_storage->published.node_capacity, w_seed_manifest_node);
  CHECK_ARRAY(manifest_storage->published.fields,
              manifest_storage->published.field_capacity, w_seed_manifest_field);
  CHECK_ARRAY(manifest_storage->published.edges,
              manifest_storage->published.edge_capacity, w_seed_manifest_edge);
  CHECK_ARRAY(manifest_storage->published.canonical_bytes,
              manifest_storage->published.canonical_byte_capacity, uint8_t);
  const w_seed_manifest_scratch *verify_scratch =
      input->manifest.verify_scratch;
  CHECK_ARRAY(verify_scratch->name_slots, verify_scratch->name_slot_capacity,
              w_seed_manifest_name_slot);
  CHECK_ARRAY(verify_scratch->bytes, verify_scratch->byte_capacity, uint8_t);
  CHECK_ARRAY(program->documents, program->document_capacity,
              w_seed_manifest_document);
  CHECK_ARRAY(program->roots, program->root_capacity, w_seed_manifest_root);
  CHECK_ARRAY(program->nodes, program->node_capacity, w_seed_manifest_node);
  CHECK_ARRAY(program->fields, program->field_capacity, w_seed_manifest_field);
  CHECK_ARRAY(program->edges, program->edge_capacity, w_seed_manifest_edge);
  CHECK_ARRAY(program->canonical_bytes, program->canonical_byte_capacity,
              uint8_t);
  for (size_t index = 0u; index < program->document_count; index += 1u)
    CHECK_ARRAY(program->documents[index].source.data,
                program->documents[index].source.length, uint8_t);
  CHECK_OBJECT(manifest_input->guard);
  CHECK_OBJECT(manifest_input->backend);
  const w_seed_source_binding_link *link = input->link;
  CHECK_ARRAY(link->context, link->context_size, uint8_t);
#undef CHECK_OBJECT
#undef CHECK_ARRAY
  return true;
}

/* Prove every descriptor object before following one of its nested pointers.
 * A malformed pointer that overlaps the output is rejected at this shallow
 * boundary, so later capacity checks cannot read the object being written. */
static bool binding_shallow_disjoint(const w_seed_source_binding_input *input,
                                     const w_seed_source_binding *binding) {
  if (input == NULL || binding == NULL) return false;
  binding_range output;
  if (!range_make(binding, sizeof(*binding), &output)) return false;
#define CHECK_OBJECT(pointer)                                                   \
  if (!output_disjoint_from_range(&output, (pointer), 1u, sizeof(*(pointer)))) \
    return false
  CHECK_OBJECT(input);
  CHECK_OBJECT(input->acquisition.pipeline);
  CHECK_OBJECT(input->acquisition.result);
  CHECK_OBJECT(input->manifest.input);
  CHECK_OBJECT(input->manifest.program);
  CHECK_OBJECT(input->manifest.result);
  CHECK_OBJECT(input->manifest.verify_scratch);
  CHECK_OBJECT(input->link);
  const w_seed_acquisition_pipeline_input *pipeline =
      input->acquisition.pipeline;
  CHECK_OBJECT(pipeline->driver_input);
  CHECK_OBJECT(pipeline->scratch);
  CHECK_OBJECT(pipeline->output);
  CHECK_OBJECT(pipeline->storage);
  const w_seed_ephemeral_driver_scratch *scratch = pipeline->scratch;
  CHECK_OBJECT(scratch->graph_scratch);
  const w_seed_manifest_guarded_input *manifest_input = input->manifest.input;
  CHECK_OBJECT(manifest_input->guard);
  CHECK_OBJECT(manifest_input->backend);
  const w_seed_source_binding_link *link = input->link;
  if (!output_disjoint_from_range(&output, link->context, link->context_size,
                                  sizeof(uint8_t)))
    return false;
#undef CHECK_OBJECT
  return true;
}

static bool hash_u64(w_seed_sha256_state *state, uint64_t value) {
  if (state == NULL) return false;
  uint8_t bytes[8];
  for (size_t index = 0u; index < sizeof(bytes); index += 1u)
    bytes[index] = (uint8_t)(value >> (56u - (unsigned int)(index * 8u)));
  w_seed_sha256_update(state, bytes, sizeof(bytes));
  return true;
}

static bool hash_frame(w_seed_sha256_state *state, const char *tag,
                       const uint8_t *bytes, size_t length) {
  if (state == NULL || tag == NULL ||
      (length != 0u && bytes == NULL) || length > (size_t)UINT64_MAX)
    return false;
  const size_t tag_length = strlen(tag);
  if (tag_length > (size_t)UINT64_MAX || !hash_u64(state, (uint64_t)tag_length))
    return false;
  if (tag_length != 0u)
    w_seed_sha256_update(state, (const uint8_t *)tag, tag_length);
  if (!hash_u64(state, (uint64_t)length)) return false;
  if (length != 0u) w_seed_sha256_update(state, bytes, length);
  return true;
}

static bool hash_text(w_seed_sha256_state *state, const char *tag,
                      w_seed_frontend_text text) {
  return text_valid(text) &&
         hash_frame(state, tag, (const uint8_t *)text.data, text.length);
}

static bool digest_provider_facts(
    const w_seed_ephemeral_graph_provider_facts *facts,
    uint8_t digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES]) {
  if (facts == NULL || digest == NULL || !text_valid(facts->provider_id) ||
      !text_valid(facts->root_token) ||
      !text_valid(facts->source_provider_owner_token) ||
      !text_valid(facts->canonical_token))
    return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  if (!hash_text(&state, "provider", facts->provider_id) ||
      !hash_text(&state, "root", facts->root_token) ||
      !hash_text(&state, "owner", facts->source_provider_owner_token) ||
      !hash_text(&state, "canonical", facts->canonical_token) ||
      !hash_frame(&state, "opened", (const uint8_t *)&facts->opened,
                  sizeof(facts->opened)) ||
      !hash_frame(&state, "containment", (const uint8_t *)&facts->containment_inside,
                  sizeof(facts->containment_inside)) ||
      !hash_frame(&state, "symlink", (const uint8_t *)&facts->symlink,
                  sizeof(facts->symlink)) ||
      !hash_u64(&state, (uint64_t)facts->snapshot_before_byte_count) ||
      !hash_frame(&state, "before-digest", facts->snapshot_before_digest,
                  W_SEED_SOURCE_BINDING_DIGEST_BYTES) ||
      !hash_u64(&state, (uint64_t)facts->snapshot_after_byte_count) ||
      !hash_frame(&state, "after-digest", facts->snapshot_after_digest,
                  W_SEED_SOURCE_BINDING_DIGEST_BYTES))
    return false;
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool digest_manifest_bindings(
    const w_seed_manifest_program *program, size_t document_count,
    uint8_t context_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES],
    uint8_t candidate_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES]) {
  if (program == NULL || context_digest == NULL || candidate_digest == NULL ||
      document_count > program->document_count)
    return false;
  w_seed_sha256_state context_state;
  w_seed_sha256_state candidate_state;
  w_seed_sha256_init(&context_state);
  w_seed_sha256_init(&candidate_state);
  if (!hash_u64(&context_state, (uint64_t)document_count) ||
      !hash_u64(&candidate_state, (uint64_t)document_count))
    return false;
  for (size_t index = 0u; index < document_count; index += 1u) {
    const w_seed_manifest_document *document = &program->documents[index];
    if (!hash_u64(&context_state, document->generation) ||
        !hash_u64(&context_state, (uint64_t)document->candidate.directory_ordinal) ||
        !hash_u64(&context_state, (uint64_t)document->candidate.candidate_index) ||
        !hash_frame(&context_state, "context", document->context_binding,
                    W_SEED_SOURCE_BINDING_DIGEST_BYTES) ||
        !hash_u64(&candidate_state, document->generation) ||
        !hash_u64(&candidate_state,
                  (uint64_t)document->candidate.directory_ordinal) ||
        !hash_u64(&candidate_state,
                  (uint64_t)document->candidate.candidate_index) ||
        !hash_frame(&candidate_state, "candidate", document->candidate_binding,
                    W_SEED_SOURCE_BINDING_DIGEST_BYTES))
      return false;
  }
  w_seed_sha256_final(&context_state, context_digest);
  w_seed_sha256_final(&candidate_state, candidate_digest);
  return true;
}

static bool digest_binding(const w_seed_source_binding *binding,
                           uint8_t digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES]) {
  if (binding == NULL || digest == NULL) return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  if (!hash_frame(&state, "domain", (const uint8_t *)W_SEED_SOURCE_BINDING_DOMAIN_TAG,
                  sizeof(W_SEED_SOURCE_BINDING_DOMAIN_TAG) - 1u) ||
      !hash_frame(&state, "schema",
                  (const uint8_t *)W_SEED_SOURCE_BINDING_SCHEMA_VERSION,
                  sizeof(W_SEED_SOURCE_BINDING_SCHEMA_VERSION) - 1u) ||
      !hash_frame(&state, "acquisition-root-facts",
                  binding->acquisition_root_facts_digest,
                  W_SEED_SOURCE_BINDING_DIGEST_BYTES) ||
      !hash_frame(&state, "acquisition-source",
                  binding->acquisition_source_digest,
                  W_SEED_SOURCE_BINDING_DIGEST_BYTES) ||
      !hash_frame(&state, "manifest-receipt", binding->manifest_receipt_digest,
                  W_SEED_SOURCE_BINDING_DIGEST_BYTES) ||
      !hash_frame(&state, "manifest-context",
                  binding->manifest_context_binding_digest,
                  W_SEED_SOURCE_BINDING_DIGEST_BYTES) ||
      !hash_frame(&state, "manifest-candidate",
                  binding->manifest_candidate_binding_digest,
                  W_SEED_SOURCE_BINDING_DIGEST_BYTES) ||
      !hash_u64(&state, binding->guard_generation) ||
      !hash_frame(&state, "link", binding->link_digest,
                  W_SEED_SOURCE_BINDING_DIGEST_BYTES))
    return false;
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool acquisition_status_valid(
    const w_seed_acquisition_pipeline_result *result) {
  if (result == NULL || result->status != W_SEED_ACQUISITION_PIPELINE_OK ||
      result->driver_result.status != W_SEED_EPHEMERAL_DRIVER_OK ||
      result->driver_result.provider_status != W_SEED_EPHEMERAL_PROVIDER_OK ||
      result->driver_result.provider_result.status !=
          W_SEED_EPHEMERAL_PROVIDER_OK ||
      result->driver_result.provider_result.phase !=
          W_SEED_EPHEMERAL_PROVIDER_PHASE_COMMIT ||
      result->driver_result.provider_result.backend_status !=
          W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK ||
      result->driver_result.provider_result.request_index != SIZE_MAX ||
      result->driver_result.provider_result.capacity_field !=
          W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_NONE ||
      result->driver_result.provider_result.required_capacity != 0u ||
      result->driver_result.provider_result.required_byte_capacity != 0u ||
      result->driver_result.provider_result.observed_byte_count != 0u)
    return false;
  return result->driver_result.graph_status == W_SEED_EPHEMERAL_GRAPH_OK &&
         result->driver_result.graph_result.status ==
             W_SEED_EPHEMERAL_GRAPH_OK;
}

static bool acquisition_fact_shape(
    const w_seed_ephemeral_graph_provider_facts *facts,
    size_t maximum_token_bytes) {
  if (facts == NULL || !text_nonempty(facts->provider_id) ||
      !text_nonempty(facts->root_token) ||
      !text_nonempty(facts->source_provider_owner_token) ||
      !text_nonempty(facts->canonical_token) ||
      facts->provider_id.length > maximum_token_bytes ||
      facts->root_token.length > maximum_token_bytes ||
      facts->source_provider_owner_token.length > maximum_token_bytes ||
      facts->canonical_token.length > maximum_token_bytes || !facts->opened ||
      !facts->containment_inside ||
      (facts->symlink != W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE &&
       facts->symlink != W_SEED_EPHEMERAL_GRAPH_SYMLINK_INSIDE) ||
      facts->snapshot_before_byte_count != facts->snapshot_after_byte_count)
    return false;
  return true;
}

static bool validate_acquisition(
    const w_seed_source_binding_acquisition *input, acquisition_view *view) {
  if (input == NULL || view == NULL || input->pipeline == NULL ||
      input->result == NULL || !acquisition_status_valid(input->result))
    return false;
  const w_seed_acquisition_pipeline_input *pipeline = input->pipeline;
  const w_seed_ephemeral_driver_input *driver_input = pipeline->driver_input;
  const w_seed_ephemeral_driver_scratch *scratch = pipeline->scratch;
  const w_seed_ephemeral_driver_output *output = pipeline->output;
  const w_seed_acquisition_storage *storage = pipeline->storage;
  const w_seed_acquisition_pipeline_result *result = input->result;
  const size_t count = result->graph_written.sources;
  if (driver_input == NULL || scratch == NULL || output == NULL ||
      storage == NULL || count == 0u ||
      count > (size_t)W_SEED_ACQUISITION_MAX_SOURCES ||
      count != result->driver_result.graph_result.written.sources ||
      result->graph_written.edges != result->driver_result.graph_result.written.edges ||
      result->graph_written.total_source_bytes !=
          result->driver_result.graph_result.written.total_source_bytes ||
      result->document_count != count || output->document_count != count ||
      output->document_capacity < count || scratch->slot_capacity < count ||
      scratch->request_capacity < count ||
      scratch->candidate_fact_capacity < count || !storage->initialized ||
      storage->owner != storage || storage->pipeline_active ||
      pipeline->backend_context_size == 0u ||
      driver_input->backend.context == NULL ||
      driver_input->root_path.data == NULL || driver_input->root_path.length == 0u ||
      driver_input->root_source_id.data == NULL ||
      driver_input->root_source_id.length == 0u ||
      driver_input->root_path.length >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES ||
      driver_input->root_source_id.length >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES ||
      driver_input->backend.open_root == NULL ||
      driver_input->backend.open_source == NULL ||
      driver_input->backend.read_source == NULL ||
      driver_input->backend.revalidate_source == NULL ||
      driver_input->backend.close_source == NULL ||
      driver_input->backend.close_root == NULL ||
      driver_input->provider_limits.max_token_bytes == 0u ||
      driver_input->provider_limits.max_token_bytes >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES)
    return false;
  const w_seed_ephemeral_driver_slot *root_slot = &scratch->slots[0u];
  const w_seed_ephemeral_provider_request *root_request =
      &scratch->requests[0u];
  if (root_slot->source_id_storage == NULL ||
      root_slot->source_id_length != driver_input->root_source_id.length ||
      memcmp(root_slot->source_id_storage, driver_input->root_source_id.data,
             root_slot->source_id_length) != 0 ||
      !text_equal(root_request->source_id, driver_input->root_source_id) ||
      root_request->source != &root_slot->source ||
      root_request->facts != &root_slot->facts || root_slot->source.bytes.data == NULL ||
      root_slot->source.bytes.length == 0u)
    return false;
  if (count > scratch->candidate_document_capacity ||
      scratch->candidate_documents == NULL || scratch->candidate_facts == NULL ||
      scratch->slots == NULL || scratch->requests == NULL)
    return false;
  size_t total_bytes = 0u;
  for (size_t index = 0u; index < count; index += 1u) {
    const w_seed_ephemeral_driver_slot *slot = &scratch->slots[index];
    const w_seed_ephemeral_provider_request *request = &scratch->requests[index];
    const w_seed_ephemeral_graph_provider_facts *facts = &slot->facts;
    if (slot->source_id_storage == NULL || slot->source_id_length == 0u ||
        slot->source_id_length > driver_input->provider_limits.max_path_bytes ||
        !text_equal(request->source_id,
                    (w_seed_frontend_text){slot->source_id_storage,
                                           slot->source_id_length}) ||
        request->source != &slot->source || request->facts != facts ||
        !acquisition_fact_shape(
            facts, driver_input->provider_limits.max_token_bytes) ||
        memcmp(&scratch->candidate_facts[index], facts, sizeof(*facts)) != 0 ||
        !w_seed_source_init(slot->source.bytes, &(w_seed_source){0},
                            &(w_seed_source_error){0}))
      return false;
    uint8_t digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES];
    if (!w_seed_ephemeral_graph_source_digest(&slot->source, digest) ||
        memcmp(digest, facts->snapshot_after_digest, sizeof(digest)) != 0 ||
        facts->snapshot_before_byte_count != slot->source.bytes.length ||
        facts->snapshot_after_byte_count != slot->source.bytes.length ||
        total_bytes > (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES -
                           slot->source.bytes.length)
      return false;
    total_bytes += slot->source.bytes.length;
    if (index != 0u &&
        (!text_equal(facts->provider_id, root_slot->facts.provider_id) ||
         !text_equal(facts->root_token, root_slot->facts.root_token) ||
         !text_equal(facts->source_provider_owner_token,
                     root_slot->facts.source_provider_owner_token)))
      return false;
    for (size_t prior = 0u; prior < index; prior += 1u) {
      if (text_equal(facts->canonical_token,
                     scratch->slots[prior].facts.canonical_token))
        return false;
    }
  }
  if (total_bytes != result->graph_written.total_source_bytes ||
      !digest_provider_facts(&root_slot->facts, view->root_facts_digest) ||
      !w_seed_ephemeral_graph_source_digest(&root_slot->source,
                                            view->source_digest))
    return false;
  view->root_facts = &root_slot->facts;
  view->facts = scratch->candidate_facts;
  view->fact_count = count;
  view->root_source = &root_slot->source;
  return true;
}

static bool validate_manifest(
    const w_seed_source_binding_manifest *input, manifest_view *view) {
  if (input == NULL || view == NULL || input->input == NULL ||
      input->program == NULL || input->result == NULL ||
      input->verify_scratch == NULL)
    return false;
  const w_seed_manifest_guarded_input *guarded = input->input;
  const w_seed_owner_guard *guard = guarded->guard;
  const w_seed_manifest_backend *backend = guarded->backend;
  const w_seed_manifest_program *program = input->program;
  const w_seed_manifest_result *result = input->result;
  if (guard == NULL || backend == NULL || guard->owner != guard ||
      !guard->session_live || guard->lifecycle != W_SEED_OWNER_GUARD_LIVE_RECONFIRMED ||
      guard->generation == 0u || guard->candidate_count == 0u ||
      guard->directory_count == 0u ||
      guard->disposition != W_SEED_OWNER_GUARD_CANDIDATES_RECONFIRMED ||
      backend->owner != backend || backend->guard != guard ||
      backend->context == NULL || backend->context_size == 0u ||
      backend->context != guard->backend.context ||
      backend->context_size != guard->backend_context_size ||
      backend->generation != guard->generation || backend->read_candidate == NULL ||
      result->status != W_SEED_MANIFEST_OK ||
      result->phase != W_SEED_MANIFEST_PHASE_COMMIT ||
      result->backend_status != W_SEED_MANIFEST_BACKEND_OK ||
      result->backend_phase != W_SEED_MANIFEST_BACKEND_PHASE_CLOSE ||
      result->owner_guard_status != W_SEED_OWNER_GUARD_OK ||
      !result->owner_guard_revalidate_called || result->written.documents == 0u ||
      result->written.documents != result->required.documents ||
      result->written.documents != guard->candidate_count ||
      program->document_count != (size_t)result->written.documents ||
      program->document_capacity < program->document_count ||
      !bytes_nonzero(result->receipt_digest,
                     W_SEED_SOURCE_BINDING_DIGEST_BYTES) ||
      memcmp(result->schema, W_SEED_MANIFEST_SCHEMA_VERSION,
             sizeof(result->schema)) != 0)
    return false;
  for (size_t index = 0u; index < program->document_count; index += 1u) {
    const w_seed_manifest_document *document = &program->documents[index];
    if (document->binding_kind != W_SEED_MANIFEST_BINDING_OWNER_GUARD ||
        document->generation != guard->generation ||
        document->candidate.generation != guard->generation ||
        document->candidate.candidate_index != index ||
        document->candidate.directory_ordinal >= guard->directory_count ||
        !bytes_nonzero(document->context_binding,
                       W_SEED_SOURCE_BINDING_DIGEST_BYTES) ||
        !bytes_nonzero(document->candidate_binding,
                       W_SEED_SOURCE_BINDING_DIGEST_BYTES))
      return false;
  }
  if (!w_seed_manifest_guarded_verify(program, result, input->verify_scratch) ||
      !digest_manifest_bindings(program, program->document_count,
                                view->context_binding_digest,
                                view->candidate_binding_digest))
    return false;
  view->guard = guard;
  view->backend = backend;
  view->generation = guard->generation;
  (void)memcpy(view->receipt_digest, result->receipt_digest,
               sizeof(view->receipt_digest));
  return true;
}

static w_seed_source_binding_status map_link_status(
    w_seed_source_binding_link_status status) {
  switch (status) {
    case W_SEED_SOURCE_BINDING_LINK_INVALID:
      return W_SEED_SOURCE_BINDING_INVALID;
    case W_SEED_SOURCE_BINDING_LINK_MISMATCH:
      return W_SEED_SOURCE_BINDING_MISMATCH;
    case W_SEED_SOURCE_BINDING_LINK_MUTATED:
      return W_SEED_SOURCE_BINDING_MUTATED;
    case W_SEED_SOURCE_BINDING_LINK_BOUNDARY:
      return W_SEED_SOURCE_BINDING_BOUNDARY;
    case W_SEED_SOURCE_BINDING_LINK_UNSUPPORTED:
      return W_SEED_SOURCE_BINDING_UNSUPPORTED;
    case W_SEED_SOURCE_BINDING_LINK_IO:
      return W_SEED_SOURCE_BINDING_IO;
    case W_SEED_SOURCE_BINDING_LINK_OK:
    default:
      return W_SEED_SOURCE_BINDING_FAULT;
  }
}

static bool input_envelope_bounds(const w_seed_source_binding_input *input) {
  if (input == NULL || input->acquisition.pipeline == NULL ||
      input->manifest.input == NULL || input->manifest.program == NULL ||
      input->manifest.verify_scratch == NULL || input->link == NULL)
    return false;
  const w_seed_acquisition_pipeline_input *pipeline =
      input->acquisition.pipeline;
  const w_seed_acquisition_pipeline_result *acquisition_result =
      input->acquisition.result;
  const w_seed_manifest_guarded_input *manifest_input = input->manifest.input;
  const w_seed_ephemeral_driver_input *driver_input = pipeline->driver_input;
  const w_seed_ephemeral_driver_scratch *scratch = pipeline->scratch;
  const w_seed_ephemeral_driver_output *output = pipeline->output;
  const w_seed_acquisition_storage *acquisition_storage = pipeline->storage;
  const w_seed_manifest_program *program = input->manifest.program;
  const w_seed_manifest_scratch *verify_scratch = input->manifest.verify_scratch;
  const w_seed_manifest_guarded_storage *storage = &manifest_input->storage;
  const w_seed_manifest_output *staged = &storage->staged;
  const w_seed_manifest_output *published = &storage->published;
  const w_seed_ephemeral_graph_scratch *graph = scratch->graph_scratch;
  binding_range ignored;
  if (!range_make(driver_input, sizeof(*driver_input), &ignored) ||
      !range_make(acquisition_result, sizeof(*acquisition_result), &ignored) ||
      !range_make(scratch, sizeof(*scratch), &ignored) ||
      !range_make(output, sizeof(*output), &ignored) ||
      !range_make(acquisition_storage, sizeof(*acquisition_storage), &ignored) ||
      !range_make(manifest_input->guard, sizeof(*manifest_input->guard),
                  &ignored) ||
      !range_make(manifest_input->backend, sizeof(*manifest_input->backend),
                  &ignored) ||
      !range_make(verify_scratch, sizeof(*verify_scratch), &ignored) ||
      !range_make(scratch == NULL ? NULL : scratch->graph_scratch,
                  sizeof(*scratch->graph_scratch), &ignored))
    return false;
  if (scratch == NULL || output == NULL || acquisition_storage == NULL ||
      scratch->slots == NULL || scratch->requests == NULL ||
      scratch->graph_scratch == NULL ||
      driver_input == NULL ||
      driver_input->backend.context == NULL ||
      pipeline->backend_context_size == 0u ||
      driver_input->provider_limits.max_sources == 0u ||
      driver_input->provider_limits.max_sources >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES ||
      driver_input->provider_limits.max_source_bytes == 0u ||
      driver_input->provider_limits.max_source_bytes >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES ||
      driver_input->provider_limits.max_total_source_bytes == 0u ||
      driver_input->provider_limits.max_total_source_bytes >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOTAL_SOURCE_BYTES ||
      driver_input->provider_limits.max_path_bytes == 0u ||
      driver_input->provider_limits.max_path_bytes >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES ||
      driver_input->provider_limits.max_token_bytes == 0u ||
      driver_input->provider_limits.max_token_bytes >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES ||
      scratch->slot_capacity >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES ||
      scratch->request_capacity >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES ||
      scratch->candidate_document_capacity >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES ||
      scratch->candidate_fact_capacity >
          (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES ||
      scratch->origin_capacity > (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      output->document_capacity > (size_t)W_SEED_FRONTEND_MAX_DOCUMENTS ||
      output->graph.inventory_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      output->graph.edge_capacity > (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      output->graph.document_order_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      output->graph.resolved_import_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      scratch->lexer_frame_capacity > (size_t)W_SEED_FRONTEND_MAX_CST_NODES ||
      scratch->token_capacity >
          (size_t)(4u * W_SEED_FRONTEND_MAX_CST_NODES) ||
      scratch->parse_frame_capacity > (size_t)W_SEED_FRONTEND_MAX_CST_NODES ||
      scratch->issue_capacity > (size_t)W_SEED_FRONTEND_MAX_CST_NODES ||
      scratch->graph_scratch->node_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      scratch->graph_scratch->edge_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      scratch->graph_scratch->sorted_nodes_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      scratch->graph_scratch->node_ordinals_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      scratch->graph_scratch->sorted_edges_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      scratch->graph_scratch->sorted_resolved_edges_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      scratch->graph_scratch->origin_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      scratch->graph_scratch->indegree_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      scratch->graph_scratch->queue_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      scratch->graph_scratch->depths_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      storage->read_slot_capacity > (size_t)W_SEED_MANIFEST_MAX_DOCUMENTS ||
      storage->staged_source_capacity > (size_t)W_SEED_MANIFEST_MAX_DOCUMENTS ||
      program->document_capacity > (size_t)W_SEED_MANIFEST_MAX_DOCUMENTS ||
      program->root_capacity >
          (size_t)W_SEED_MANIFEST_MAX_DOCUMENTS *
              (size_t)W_SEED_MANIFEST_MAX_ROOTS_PER_DOCUMENT ||
      program->node_capacity > (size_t)W_SEED_MANIFEST_MAX_STRUCTURAL_NODES ||
      program->field_capacity > (size_t)W_SEED_MANIFEST_MAX_STRUCTURAL_NODES ||
      program->edge_capacity > (size_t)W_SEED_MANIFEST_MAX_STRUCTURAL_NODES ||
      program->canonical_byte_capacity >
          (size_t)W_SEED_MANIFEST_MAX_CANONICAL_BYTES ||
      verify_scratch->name_slot_capacity >
          (size_t)W_SEED_MANIFEST_MAX_STRUCTURAL_NODES ||
      verify_scratch->byte_capacity >
          (size_t)W_SEED_MANIFEST_MAX_DECODED_SCALAR_BYTES +
              (size_t)W_SEED_MANIFEST_SCALAR_SCRATCH_OVERHEAD)
    return false;
  if (!array_range(driver_input->root_path.data, driver_input->root_path.length,
                   sizeof(*driver_input->root_path.data), &ignored) ||
      !array_range(driver_input->root_source_id.data,
                   driver_input->root_source_id.length,
                   sizeof(*driver_input->root_source_id.data), &ignored) ||
      !array_range(driver_input->backend.context, pipeline->backend_context_size,
                   sizeof(uint8_t), &ignored) ||
      output->document_count > output->document_capacity ||
      output->document_count >
          (size_t)W_SEED_FRONTEND_MAX_DOCUMENTS)
    return false;
  if (!array_range(scratch->slots, scratch->slot_capacity,
                   sizeof(*scratch->slots), &ignored) ||
      !array_range(scratch->requests, scratch->request_capacity,
                   sizeof(*scratch->requests), &ignored) ||
      !array_range(scratch->lexer_frames, scratch->lexer_frame_capacity,
                   sizeof(*scratch->lexer_frames), &ignored) ||
      !array_range(scratch->tokens, scratch->token_capacity,
                   sizeof(*scratch->tokens), &ignored) ||
      !array_range(scratch->parse_frames, scratch->parse_frame_capacity,
                   sizeof(*scratch->parse_frames), &ignored) ||
      !array_range(scratch->issues, scratch->issue_capacity,
                   sizeof(*scratch->issues), &ignored) ||
      !array_range(scratch->origins, scratch->origin_capacity,
                   sizeof(*scratch->origins), &ignored) ||
      !array_range(scratch->candidate_documents,
                   scratch->candidate_document_capacity,
                   sizeof(*scratch->candidate_documents), &ignored) ||
      !array_range(scratch->candidate_facts, scratch->candidate_fact_capacity,
                   sizeof(*scratch->candidate_facts), &ignored) ||
      !array_range(output->documents, output->document_capacity,
                   sizeof(*output->documents), &ignored) ||
      !array_range(output->graph.inventory, output->graph.inventory_capacity,
                   sizeof(*output->graph.inventory), &ignored) ||
      !array_range(output->graph.edges, output->graph.edge_capacity,
                   sizeof(*output->graph.edges), &ignored) ||
      !array_range(output->graph.document_order,
                   output->graph.document_order_capacity,
                   sizeof(*output->graph.document_order), &ignored) ||
      !array_range(output->graph.resolved_imports,
                   output->graph.resolved_import_capacity,
                   sizeof(*output->graph.resolved_imports), &ignored) ||
      !array_range(graph->nodes, graph->node_capacity, sizeof(*graph->nodes),
                   &ignored) ||
      !array_range(graph->edges, graph->edge_capacity, sizeof(*graph->edges),
                   &ignored) ||
      !array_range(graph->sorted_nodes, graph->sorted_nodes_capacity,
                   sizeof(*graph->sorted_nodes), &ignored) ||
      !array_range(graph->node_ordinals, graph->node_ordinals_capacity,
                   sizeof(*graph->node_ordinals), &ignored) ||
      !array_range(graph->sorted_edges, graph->sorted_edges_capacity,
                   sizeof(*graph->sorted_edges), &ignored) ||
      !array_range(graph->sorted_resolved_edges,
                   graph->sorted_resolved_edges_capacity,
                   sizeof(*graph->sorted_resolved_edges), &ignored) ||
      !array_range(graph->origins, graph->origin_capacity,
                   sizeof(*graph->origins), &ignored) ||
      !array_range(graph->indegree, graph->indegree_capacity,
                   sizeof(*graph->indegree), &ignored) ||
      !array_range(graph->queue, graph->queue_capacity, sizeof(*graph->queue),
                   &ignored) ||
      !array_range(graph->depths, graph->depths_capacity,
                   sizeof(*graph->depths), &ignored))
    return false;
  if (!array_range(storage->read_slots, storage->read_slot_capacity,
                   sizeof(*storage->read_slots), &ignored) ||
      !array_range(storage->staged_sources, storage->staged_source_capacity,
                   sizeof(*storage->staged_sources), &ignored) ||
      !array_range(staged->documents, staged->document_capacity,
                   sizeof(*staged->documents), &ignored) ||
      !array_range(staged->roots, staged->root_capacity, sizeof(*staged->roots),
                   &ignored) ||
      !array_range(staged->nodes, staged->node_capacity, sizeof(*staged->nodes),
                   &ignored) ||
      !array_range(staged->fields, staged->field_capacity,
                   sizeof(*staged->fields), &ignored) ||
      !array_range(staged->edges, staged->edge_capacity, sizeof(*staged->edges),
                   &ignored) ||
      !array_range(staged->canonical_bytes, staged->canonical_byte_capacity,
                   sizeof(*staged->canonical_bytes), &ignored) ||
      !array_range(published->documents, published->document_capacity,
                   sizeof(*published->documents), &ignored) ||
      !array_range(published->roots, published->root_capacity,
                   sizeof(*published->roots), &ignored) ||
      !array_range(published->nodes, published->node_capacity,
                   sizeof(*published->nodes), &ignored) ||
      !array_range(published->fields, published->field_capacity,
                   sizeof(*published->fields), &ignored) ||
      !array_range(published->edges, published->edge_capacity,
                   sizeof(*published->edges), &ignored) ||
      !array_range(published->canonical_bytes,
                   published->canonical_byte_capacity,
                   sizeof(*published->canonical_bytes), &ignored) ||
      !array_range(verify_scratch->name_slots,
                   verify_scratch->name_slot_capacity,
                   sizeof(*verify_scratch->name_slots), &ignored) ||
      !array_range(verify_scratch->bytes, verify_scratch->byte_capacity,
                   sizeof(*verify_scratch->bytes), &ignored) ||
      !array_range(program->documents, program->document_capacity,
                   sizeof(*program->documents), &ignored) ||
      !array_range(program->roots, program->root_capacity,
                   sizeof(*program->roots), &ignored) ||
      !array_range(program->nodes, program->node_capacity,
                   sizeof(*program->nodes), &ignored) ||
      !array_range(program->fields, program->field_capacity,
                   sizeof(*program->fields), &ignored) ||
      !array_range(program->edges, program->edge_capacity,
                   sizeof(*program->edges), &ignored) ||
      !array_range(program->canonical_bytes, program->canonical_byte_capacity,
                   sizeof(*program->canonical_bytes), &ignored) ||
      program->document_count > program->document_capacity)
    return false;
  for (size_t index = 0u;
       index < (size_t)W_SEED_ACQUISITION_MAX_SOURCES; index += 1u) {
    if (acquisition_storage->staging_capacity[index] >
            (size_t)W_SEED_ACQUISITION_MAX_SOURCE_BYTES ||
        acquisition_storage->revalidation_capacity[index] >
            (size_t)W_SEED_ACQUISITION_MAX_SOURCE_BYTES ||
        acquisition_storage->published_capacity[index] >
            (size_t)W_SEED_ACQUISITION_MAX_SOURCE_BYTES ||
        acquisition_storage->node_capacity[index] >
            (size_t)W_SEED_ACQUISITION_MAX_NODES)
      return false;
  }
  const size_t root_capacity_limit =
      (size_t)W_SEED_MANIFEST_MAX_DOCUMENTS *
      (size_t)W_SEED_MANIFEST_MAX_ROOTS_PER_DOCUMENT;
  if (staged->document_capacity > (size_t)W_SEED_MANIFEST_MAX_DOCUMENTS ||
      staged->root_capacity > root_capacity_limit ||
      staged->node_capacity > (size_t)W_SEED_MANIFEST_MAX_STRUCTURAL_NODES ||
      staged->field_capacity > (size_t)W_SEED_MANIFEST_MAX_STRUCTURAL_NODES ||
      staged->edge_capacity > (size_t)W_SEED_MANIFEST_MAX_STRUCTURAL_NODES ||
      staged->canonical_byte_capacity >
          (size_t)W_SEED_MANIFEST_MAX_CANONICAL_BYTES ||
      published->document_capacity >
          (size_t)W_SEED_MANIFEST_MAX_DOCUMENTS ||
      published->root_capacity > root_capacity_limit ||
      published->node_capacity >
          (size_t)W_SEED_MANIFEST_MAX_STRUCTURAL_NODES ||
      published->field_capacity >
          (size_t)W_SEED_MANIFEST_MAX_STRUCTURAL_NODES ||
      published->edge_capacity >
          (size_t)W_SEED_MANIFEST_MAX_STRUCTURAL_NODES ||
      published->canonical_byte_capacity >
          (size_t)W_SEED_MANIFEST_MAX_CANONICAL_BYTES)
    return false;
  for (size_t index = 0u; index < scratch->slot_capacity; index += 1u) {
    if (scratch->slots[index].source_id_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES ||
        scratch->slots[index].module_id_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES ||
        scratch->slots[index].node_capacity >
            (size_t)W_SEED_FRONTEND_MAX_CST_NODES)
      return false;
  }
  for (size_t index = 0u; index < scratch->request_capacity; index += 1u) {
    const w_seed_ephemeral_provider_request *request = &scratch->requests[index];
    if (!array_range(request->source_id.data, request->source_id.length,
                     sizeof(*request->source_id.data), &ignored) ||
        request->staging_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES ||
        request->revalidation_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES ||
        request->byte_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES ||
        request->tokens.provider_id_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES ||
        request->tokens.root_token_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES ||
        request->tokens.source_provider_owner_token_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES ||
        request->tokens.canonical_token_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES ||
        request->revalidation_tokens.provider_id_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES ||
        request->revalidation_tokens.root_token_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES ||
        request->revalidation_tokens.source_provider_owner_token_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES ||
        request->revalidation_tokens.canonical_token_capacity >
            (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES)
      return false;
    if (!array_range(request->staging_bytes, request->staging_capacity,
                     sizeof(*request->staging_bytes), &ignored) ||
        !array_range(request->revalidation_bytes,
                     request->revalidation_capacity,
                     sizeof(*request->revalidation_bytes), &ignored) ||
        !array_range(request->bytes, request->byte_capacity,
                     sizeof(*request->bytes), &ignored) ||
        !array_range(request->tokens.provider_id,
                     request->tokens.provider_id_capacity,
                     sizeof(*request->tokens.provider_id), &ignored) ||
        !array_range(request->tokens.root_token,
                     request->tokens.root_token_capacity,
                     sizeof(*request->tokens.root_token), &ignored) ||
        !array_range(request->tokens.source_provider_owner_token,
                     request->tokens.source_provider_owner_token_capacity,
                     sizeof(*request->tokens.source_provider_owner_token),
                     &ignored) ||
        !array_range(request->tokens.canonical_token,
                     request->tokens.canonical_token_capacity,
                     sizeof(*request->tokens.canonical_token), &ignored) ||
        !array_range(request->revalidation_tokens.provider_id,
                     request->revalidation_tokens.provider_id_capacity,
                     sizeof(*request->revalidation_tokens.provider_id),
                     &ignored) ||
        !array_range(request->revalidation_tokens.root_token,
                     request->revalidation_tokens.root_token_capacity,
                     sizeof(*request->revalidation_tokens.root_token),
                     &ignored) ||
        !array_range(request->revalidation_tokens.source_provider_owner_token,
                     request->revalidation_tokens
                         .source_provider_owner_token_capacity,
                     sizeof(*request->revalidation_tokens
                                  .source_provider_owner_token),
                     &ignored) ||
        !array_range(request->revalidation_tokens.canonical_token,
                     request->revalidation_tokens.canonical_token_capacity,
                     sizeof(*request->revalidation_tokens.canonical_token),
                     &ignored))
      return false;
  }
  for (size_t index = 0u; index < scratch->slot_capacity; index += 1u) {
    const w_seed_ephemeral_driver_slot *slot = &scratch->slots[index];
    if (slot->source.bytes.length >
            driver_input->provider_limits.max_source_bytes ||
        slot->source_id_length > driver_input->provider_limits.max_path_bytes ||
        slot->facts.provider_id.length >
            driver_input->provider_limits.max_token_bytes ||
        slot->facts.root_token.length >
            driver_input->provider_limits.max_token_bytes ||
        slot->facts.source_provider_owner_token.length >
            driver_input->provider_limits.max_token_bytes ||
        slot->facts.canonical_token.length >
            driver_input->provider_limits.max_token_bytes ||
        !array_range(slot->source.bytes.data, slot->source.bytes.length,
                     sizeof(*slot->source.bytes.data), &ignored) ||
        !array_range(slot->facts.provider_id.data,
                     slot->facts.provider_id.length,
                     sizeof(*slot->facts.provider_id.data), &ignored) ||
        !array_range(slot->facts.root_token.data, slot->facts.root_token.length,
                     sizeof(*slot->facts.root_token.data), &ignored) ||
        !array_range(slot->facts.source_provider_owner_token.data,
                     slot->facts.source_provider_owner_token.length,
                     sizeof(*slot->facts.source_provider_owner_token.data),
                     &ignored) ||
        !array_range(slot->facts.canonical_token.data,
                     slot->facts.canonical_token.length,
                     sizeof(*slot->facts.canonical_token.data), &ignored))
      return false;
  }
  return true;
}

static w_seed_source_binding_result compose_impl(
    const w_seed_source_binding_input *input, w_seed_source_binding *binding) {
  if (input == NULL || binding == NULL ||
      (uintptr_t)binding % (uintptr_t)_Alignof(w_seed_source_binding) != 0u)
    return binding_result(W_SEED_SOURCE_BINDING_INVALID,
                          W_SEED_SOURCE_BINDING_PHASE_VALIDATE,
                          W_SEED_SOURCE_BINDING_ZERO, 0u);
  if (!binding_shallow_disjoint(input, binding))
    return binding_result(W_SEED_SOURCE_BINDING_ALIAS,
                          W_SEED_SOURCE_BINDING_PHASE_VALIDATE,
                          W_SEED_SOURCE_BINDING_ZERO, 0u);
  if (!input_envelope_bounds(input))
    return binding_result(W_SEED_SOURCE_BINDING_INVALID,
                          W_SEED_SOURCE_BINDING_PHASE_VALIDATE,
                          W_SEED_SOURCE_BINDING_ZERO, 0u);
  if (!binding_output_disjoint(input, binding))
    return binding_result(W_SEED_SOURCE_BINDING_ALIAS,
                          W_SEED_SOURCE_BINDING_PHASE_VALIDATE,
                          W_SEED_SOURCE_BINDING_ZERO, 0u);

  acquisition_view acquisition = {0};
  if (!validate_acquisition(&input->acquisition, &acquisition))
    return binding_result(W_SEED_SOURCE_BINDING_INVALID,
                          W_SEED_SOURCE_BINDING_PHASE_ACQUISITION,
                          W_SEED_SOURCE_BINDING_ZERO, 0u);
  manifest_view manifest = {0};
  if (!validate_manifest(&input->manifest, &manifest))
    return binding_result(W_SEED_SOURCE_BINDING_MUTATED,
                          W_SEED_SOURCE_BINDING_PHASE_MANIFEST,
                          W_SEED_SOURCE_BINDING_ACQUISITION_VALIDATED, 0u);
  const w_seed_source_binding_link *link = input->link;
  if (link->owner != link || link->context == NULL || link->context_size == 0u ||
      link->context != manifest.backend->context ||
      link->context_size != manifest.backend->context_size ||
      link->compose == NULL)
    return binding_result(W_SEED_SOURCE_BINDING_INVALID,
                          W_SEED_SOURCE_BINDING_PHASE_LINK,
                          W_SEED_SOURCE_BINDING_MANIFEST_VALIDATED,
                          manifest.generation);
  const w_seed_source_binding_link_input link_input = {
      acquisition.facts, acquisition.fact_count, 0u, manifest.generation};
  const w_seed_source_binding_link_result link_result =
      link->compose(link, &link_input);
  if (link_result.status != W_SEED_SOURCE_BINDING_LINK_OK ||
      link_result.phase != W_SEED_SOURCE_BINDING_LINK_PHASE_COMMIT ||
      !bytes_nonzero(link_result.link_digest,
                     W_SEED_SOURCE_BINDING_DIGEST_BYTES)) {
    const w_seed_source_binding_status status =
        link_result.status == W_SEED_SOURCE_BINDING_LINK_OK
            ? W_SEED_SOURCE_BINDING_FAULT
            : map_link_status(link_result.status);
    return binding_result(status, W_SEED_SOURCE_BINDING_PHASE_LINK,
                          W_SEED_SOURCE_BINDING_MANIFEST_VALIDATED,
                          manifest.generation);
  }
  w_seed_source_binding candidate = {0};
  (void)memset(&candidate, 0, sizeof(candidate));
  candidate.owner = binding;
  candidate.lifecycle = W_SEED_SOURCE_BINDING_BOUND;
  candidate.acquisition_pipeline = input->acquisition.pipeline;
  candidate.acquisition_result = input->acquisition.result;
  candidate.manifest_input = input->manifest.input;
  candidate.manifest_program = input->manifest.program;
  candidate.manifest_result = input->manifest.result;
  candidate.manifest_verify_scratch = input->manifest.verify_scratch;
  candidate.link = link;
  candidate.guard_generation = manifest.generation;
  (void)memcpy(candidate.acquisition_root_facts_digest,
               acquisition.root_facts_digest,
               sizeof(candidate.acquisition_root_facts_digest));
  (void)memcpy(candidate.acquisition_source_digest, acquisition.source_digest,
               sizeof(candidate.acquisition_source_digest));
  (void)memcpy(candidate.manifest_receipt_digest, manifest.receipt_digest,
               sizeof(candidate.manifest_receipt_digest));
  (void)memcpy(candidate.manifest_context_binding_digest,
               manifest.context_binding_digest,
               sizeof(candidate.manifest_context_binding_digest));
  (void)memcpy(candidate.manifest_candidate_binding_digest,
               manifest.candidate_binding_digest,
               sizeof(candidate.manifest_candidate_binding_digest));
  (void)memcpy(candidate.link_digest, link_result.link_digest,
               sizeof(candidate.link_digest));
  (void)memcpy(candidate.schema, W_SEED_SOURCE_BINDING_SCHEMA_VERSION,
               sizeof(candidate.schema));
  if (!digest_binding(&candidate, candidate.binding_digest))
    return binding_result(W_SEED_SOURCE_BINDING_FAULT,
                          W_SEED_SOURCE_BINDING_PHASE_COMMIT,
                          W_SEED_SOURCE_BINDING_LINKED, manifest.generation);
  (void)memcpy(binding, &candidate, sizeof(candidate));
  return binding_result(W_SEED_SOURCE_BINDING_OK,
                        W_SEED_SOURCE_BINDING_PHASE_COMMIT,
                        W_SEED_SOURCE_BINDING_BOUND, manifest.generation);
}

w_seed_source_binding_result w_seed_source_binding_compose(
    const w_seed_source_binding_input *input, w_seed_source_binding *binding) {
  return compose_impl(input, binding);
}

static bool source_binding_fields_equal(const w_seed_source_binding *left,
                                        const w_seed_source_binding *right) {
  if (left == NULL || right == NULL) return false;
  return left->owner == right->owner &&
         left->lifecycle == right->lifecycle &&
         left->acquisition_pipeline == right->acquisition_pipeline &&
         left->acquisition_result == right->acquisition_result &&
         left->manifest_input == right->manifest_input &&
         left->manifest_program == right->manifest_program &&
         left->manifest_result == right->manifest_result &&
         left->manifest_verify_scratch == right->manifest_verify_scratch &&
         left->link == right->link &&
         left->guard_generation == right->guard_generation &&
         memcmp(left->acquisition_root_facts_digest,
                right->acquisition_root_facts_digest,
                sizeof(left->acquisition_root_facts_digest)) == 0 &&
         memcmp(left->acquisition_source_digest, right->acquisition_source_digest,
                sizeof(left->acquisition_source_digest)) == 0 &&
         memcmp(left->manifest_receipt_digest, right->manifest_receipt_digest,
                sizeof(left->manifest_receipt_digest)) == 0 &&
         memcmp(left->manifest_context_binding_digest,
                right->manifest_context_binding_digest,
                sizeof(left->manifest_context_binding_digest)) == 0 &&
         memcmp(left->manifest_candidate_binding_digest,
                right->manifest_candidate_binding_digest,
                sizeof(left->manifest_candidate_binding_digest)) == 0 &&
         memcmp(left->link_digest, right->link_digest,
                sizeof(left->link_digest)) == 0 &&
         memcmp(left->binding_digest, right->binding_digest,
                sizeof(left->binding_digest)) == 0 &&
         memcmp(left->schema, right->schema, sizeof(left->schema)) == 0;
}

bool w_seed_source_binding_verify(const w_seed_source_binding *binding) {
  if (binding == NULL || binding->owner != binding ||
      binding->lifecycle != W_SEED_SOURCE_BINDING_BOUND ||
      binding->acquisition_pipeline == NULL ||
      binding->acquisition_result == NULL || binding->manifest_input == NULL ||
      binding->manifest_program == NULL || binding->manifest_result == NULL ||
      binding->manifest_verify_scratch == NULL || binding->link == NULL)
    return false;
  const w_seed_source_binding_input input = {
      {binding->acquisition_pipeline, binding->acquisition_result},
      {binding->manifest_input, binding->manifest_program,
       binding->manifest_result, binding->manifest_verify_scratch},
      binding->link};
  w_seed_source_binding candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  const w_seed_source_binding_result result =
      compose_impl(&input, &candidate);
  if (result.status != W_SEED_SOURCE_BINDING_OK ||
      candidate.owner != &candidate)
    return false;
  candidate.owner = binding;
  return source_binding_fields_equal(&candidate, binding);
}
