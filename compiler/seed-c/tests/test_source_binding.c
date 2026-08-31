#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "w_seed_sha256.h"
#include "w_seed_ephemeral_provider_linux.h"
#include "w_seed_source_binding.h"
#include "w_seed_source_binding_linux.h"

#define CHECK(value)                                                            \
  do {                                                                          \
    if (!(value)) {                                                             \
      (void)fprintf(stderr, "source binding check failed at line %d\n",       \
                    __LINE__);                                                 \
      return EXIT_FAILURE;                                                      \
    }                                                                           \
  } while (0)

enum {
  UNIT_MAN_DOCUMENT_BYTES = 128u,
  UNIT_MAN_STRUCTURAL_NODES = 64u,
  UNIT_MAN_CANONICAL_BYTES = 256u,
  UNIT_MAN_SCRATCH_BYTES = 512u,
  UNIT_ACQ_BYTES = 64u,
  UNIT_ACQ_TOKEN_BYTES = 80u,
};

static const uint8_t unit_manifest_bytes[] = "package { value: 1 }\n";
static const uint8_t unit_acquisition_bytes[] = "source bytes\n";
static const char unit_source_id[] = "root";
static const char unit_root_path[] = "root-request";
static const char unit_provider_id[] = "fake-provider-v2";
static const char unit_root_token[] = "fake-root-token";
static const char unit_owner_token[] = "fake-owner-token";
static const char unit_canonical_token[] = "fake-canonical-token";

typedef struct {
  const uint8_t *bytes;
  size_t length;
} unit_manifest_context;

typedef struct {
  uint32_t marker;
} unit_acquisition_context;

typedef enum {
  UNIT_LINK_OK = 0,
  UNIT_LINK_INVALID,
  UNIT_LINK_MISMATCH,
  UNIT_LINK_MUTATED,
  UNIT_LINK_BOUNDARY,
  UNIT_LINK_UNSUPPORTED,
  UNIT_LINK_IO,
} unit_link_mode;

static unit_link_mode unit_link_mode_value;

static w_seed_frontend_text unit_text(const char *data) {
  return (w_seed_frontend_text){data, strlen(data)};
}

static w_seed_ephemeral_provider_backend_status unit_open_root(
    void *context, w_seed_byte_view root_path,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *root_handle,
    w_seed_ephemeral_provider_handle *root_source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  (void)context;
  (void)root_path;
  (void)tokens;
  (void)root_handle;
  (void)root_source_handle;
  (void)observation;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
}

static w_seed_ephemeral_provider_backend_status unit_open_source(
    void *context, w_seed_ephemeral_provider_handle root_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  (void)context;
  (void)root_handle;
  (void)source_id;
  (void)tokens;
  (void)source_handle;
  (void)observation;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
}

static w_seed_ephemeral_provider_backend_status unit_read_source(
    void *context, w_seed_ephemeral_provider_handle source_handle,
    uint8_t *bytes, size_t capacity, size_t *written) {
  (void)context;
  (void)source_handle;
  (void)bytes;
  (void)capacity;
  (void)written;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
}

static w_seed_ephemeral_provider_backend_status unit_revalidate_source(
    void *context, w_seed_ephemeral_provider_handle root_handle,
    w_seed_ephemeral_provider_handle source_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_observation *observation, uint8_t *bytes,
    size_t capacity, size_t *written) {
  (void)context;
  (void)root_handle;
  (void)source_handle;
  (void)source_id;
  (void)tokens;
  (void)observation;
  (void)bytes;
  (void)capacity;
  (void)written;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
}

static void unit_close_source(void *context,
                              w_seed_ephemeral_provider_handle source_handle) {
  (void)context;
  (void)source_handle;
}

static void unit_close_root(void *context,
                            w_seed_ephemeral_provider_handle root_handle) {
  (void)context;
  (void)root_handle;
}

static w_seed_owner_guard_backend_result unit_owner_begin(
    void *context, w_seed_byte_view source_path,
    w_seed_owner_guard_observation *observations, size_t observation_capacity) {
  (void)context;
  if (source_path.length != sizeof("source") - 1u ||
      memcmp(source_path.data, "source", source_path.length) != 0 ||
      observations == NULL || observation_capacity < 1u)
    return (w_seed_owner_guard_backend_result){
        W_SEED_OWNER_GUARD_BACKEND_INVALID,
        W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u};
  observations[0] = (w_seed_owner_guard_observation){0u, 0u, true};
  return (w_seed_owner_guard_backend_result){
      W_SEED_OWNER_GUARD_BACKEND_OK,
      W_SEED_OWNER_GUARD_BACKEND_PHASE_COMMIT,
      0u, 0u, 41u, 1u, 1u};
}

static w_seed_owner_guard_backend_result unit_owner_revalidate(
    void *context, uint64_t generation,
    w_seed_owner_guard_observation *observations, size_t observation_capacity) {
  (void)context;
  if (generation != 41u || observations == NULL || observation_capacity < 1u)
    return (w_seed_owner_guard_backend_result){
        W_SEED_OWNER_GUARD_BACKEND_INVALID,
        W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, generation, 0u, 0u};
  observations[0] = (w_seed_owner_guard_observation){0u, 0u, true};
  return (w_seed_owner_guard_backend_result){
      W_SEED_OWNER_GUARD_BACKEND_OK,
      W_SEED_OWNER_GUARD_BACKEND_PHASE_COMMIT,
      0u, 0u, generation, 1u, 1u};
}

static void unit_owner_abort(void *context) { (void)context; }

static void unit_owner_destroy(void *context, uint64_t generation) {
  (void)context;
  (void)generation;
}

static void unit_u32_be(uint8_t bytes[4], uint32_t value) {
  bytes[0] = (uint8_t)(value >> 24u);
  bytes[1] = (uint8_t)(value >> 16u);
  bytes[2] = (uint8_t)(value >> 8u);
  bytes[3] = (uint8_t)value;
}

static void unit_u64_be(uint8_t bytes[8], uint64_t value) {
  for (size_t index = 0u; index < 8u; index += 1u)
    bytes[7u - index] = (uint8_t)(value >> (index * 8u));
}

static void unit_manifest_source_digest(
    const uint8_t *bytes, size_t length,
    uint8_t digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES]) {
  static const char tag[] = W_SEED_MANIFEST_DOCUMENT_SOURCE_TAG;
  uint8_t tag_length[4];
  uint8_t number[8];
  unit_u32_be(tag_length, (uint32_t)(sizeof(tag) - 1u));
  unit_u64_be(number, (uint64_t)length);
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, tag_length, sizeof(tag_length));
  w_seed_sha256_update(&state, (const uint8_t *)tag, sizeof(tag) - 1u);
  w_seed_sha256_update(&state, number, sizeof(number));
  w_seed_sha256_update(&state, bytes, length);
  w_seed_sha256_final(&state, digest);
}

static w_seed_manifest_backend_result unit_manifest_read(
    const void *context_value, uint64_t generation,
    w_seed_owner_guard_candidate_ref candidate, uint8_t *bytes,
    size_t byte_capacity, size_t byte_limit) {
  (void)byte_limit;
  const unit_manifest_context *context =
      (const unit_manifest_context *)context_value;
  if (context == NULL || generation != 41u || candidate.generation != 41u ||
      candidate.directory_ordinal != 0u || candidate.candidate_index != 0u)
    return (w_seed_manifest_backend_result){
        W_SEED_MANIFEST_BACKEND_INVALID,
        W_SEED_MANIFEST_BACKEND_PHASE_VALIDATE,
        generation,
        candidate,
        0u,
        0u,
        {0},
        {0},
        {0}};
  if (context->length > byte_capacity)
    return (w_seed_manifest_backend_result){
        W_SEED_MANIFEST_BACKEND_CAPACITY,
        W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_EOF,
        generation,
        candidate,
        0u,
        context->length,
        {0},
        {0},
        {0}};
  (void)memcpy(bytes, context->bytes, context->length);
  w_seed_manifest_backend_result result = {
      W_SEED_MANIFEST_BACKEND_OK,
      W_SEED_MANIFEST_BACKEND_PHASE_CLOSE,
      generation,
      candidate,
      context->length,
      context->length,
      {0},
      {0},
      {0}};
  unit_manifest_source_digest(bytes, context->length, result.source_digest);
  result.context_binding[0] = 0x11u;
  result.candidate_binding[0] = 0x22u;
  return result;
}

static w_seed_source_binding_link_result unit_link_compose(
    const w_seed_source_binding_link *link,
    const w_seed_source_binding_link_input *input) {
  w_seed_source_binding_link_result result = {0};
  if (link == NULL || input == NULL || link->owner != link ||
      link->context == NULL || input->facts == NULL || input->fact_count == 0u ||
      input->root_fact_index != 0u) {
    result.status = W_SEED_SOURCE_BINDING_LINK_INVALID;
    result.phase = W_SEED_SOURCE_BINDING_LINK_PHASE_VALIDATE;
    return result;
  }
  switch (unit_link_mode_value) {
    case UNIT_LINK_INVALID:
      result.status = W_SEED_SOURCE_BINDING_LINK_INVALID;
      result.phase = W_SEED_SOURCE_BINDING_LINK_PHASE_PROVIDER;
      return result;
    case UNIT_LINK_MISMATCH:
      result.status = W_SEED_SOURCE_BINDING_LINK_MISMATCH;
      result.phase = W_SEED_SOURCE_BINDING_LINK_PHASE_COMPARE;
      return result;
    case UNIT_LINK_MUTATED:
      result.status = W_SEED_SOURCE_BINDING_LINK_MUTATED;
      result.phase = W_SEED_SOURCE_BINDING_LINK_PHASE_COMPARE;
      return result;
    case UNIT_LINK_BOUNDARY:
      result.status = W_SEED_SOURCE_BINDING_LINK_BOUNDARY;
      result.phase = W_SEED_SOURCE_BINDING_LINK_PHASE_OWNER;
      return result;
    case UNIT_LINK_UNSUPPORTED:
      result.status = W_SEED_SOURCE_BINDING_LINK_UNSUPPORTED;
      result.phase = W_SEED_SOURCE_BINDING_LINK_PHASE_PROVIDER;
      return result;
    case UNIT_LINK_IO:
      result.status = W_SEED_SOURCE_BINDING_LINK_IO;
      result.phase = W_SEED_SOURCE_BINDING_LINK_PHASE_OWNER;
      return result;
    case UNIT_LINK_OK:
    default:
      break;
  }
  result.status = W_SEED_SOURCE_BINDING_LINK_OK;
  result.phase = W_SEED_SOURCE_BINDING_LINK_PHASE_COMMIT;
  for (size_t index = 0u; index < sizeof(result.link_digest); index += 1u)
    result.link_digest[index] = (uint8_t)(0xa0u + (uint8_t)index);
  return result;
}

typedef struct {
  unit_acquisition_context acquisition_context;
  unit_manifest_context manifest_context;
  w_seed_acquisition_storage acquisition_storage;
  w_seed_ephemeral_driver_input driver_input;
  w_seed_ephemeral_driver_scratch driver_scratch;
  w_seed_ephemeral_driver_output driver_output;
  w_seed_acquisition_pipeline_input acquisition_pipeline;
  w_seed_acquisition_pipeline_result acquisition_result;
  w_seed_ephemeral_driver_slot slots[1];
  w_seed_ephemeral_provider_request requests[1];
  w_seed_frontend_document candidate_documents[1];
  w_seed_ephemeral_graph_provider_facts candidate_facts[1];
  w_seed_ephemeral_graph_scratch graph_scratch;
  w_seed_frontend_document output_documents[1];
  char source_id_storage[sizeof(unit_source_id)];
  char provider_storage[UNIT_ACQ_TOKEN_BYTES];
  char root_token_storage[UNIT_ACQ_TOKEN_BYTES];
  char owner_token_storage[UNIT_ACQ_TOKEN_BYTES];
  char canonical_token_storage[UNIT_ACQ_TOKEN_BYTES];
  char revalidation_provider_storage[UNIT_ACQ_TOKEN_BYTES];
  char revalidation_root_storage[UNIT_ACQ_TOKEN_BYTES];
  char revalidation_owner_storage[UNIT_ACQ_TOKEN_BYTES];
  char revalidation_canonical_storage[UNIT_ACQ_TOKEN_BYTES];
  uint8_t source_bytes[UNIT_ACQ_BYTES];
  w_seed_owner_guard guard;
  w_seed_owner_guard_observation owner_staged[1];
  w_seed_owner_guard_observation owner_revalidation[1];
  w_seed_owner_guard_candidate_ref owner_candidates[1];
  w_seed_manifest_backend manifest_backend;
  w_seed_manifest_guarded_input manifest_input;
  w_seed_manifest_read_slot manifest_slots[1];
  w_seed_manifest_source_input manifest_sources[1];
  w_seed_manifest_document staged_documents[1];
  w_seed_manifest_root staged_roots[2];
  w_seed_manifest_node staged_nodes[UNIT_MAN_STRUCTURAL_NODES];
  w_seed_manifest_field staged_fields[UNIT_MAN_STRUCTURAL_NODES];
  w_seed_manifest_edge staged_edges[UNIT_MAN_STRUCTURAL_NODES];
  uint8_t staged_canonical[UNIT_MAN_CANONICAL_BYTES];
  w_seed_manifest_document published_documents[1];
  w_seed_manifest_root published_roots[2];
  w_seed_manifest_node published_nodes[UNIT_MAN_STRUCTURAL_NODES];
  w_seed_manifest_field published_fields[UNIT_MAN_STRUCTURAL_NODES];
  w_seed_manifest_edge published_edges[UNIT_MAN_STRUCTURAL_NODES];
  uint8_t published_canonical[UNIT_MAN_CANONICAL_BYTES];
  w_seed_manifest_name_slot manifest_name_slots[UNIT_MAN_STRUCTURAL_NODES];
  uint8_t manifest_scratch_bytes[UNIT_MAN_SCRATCH_BYTES];
  uint8_t manifest_first[UNIT_MAN_DOCUMENT_BYTES];
  uint8_t manifest_second[UNIT_MAN_DOCUMENT_BYTES];
  w_seed_manifest_limits manifest_limits;
  w_seed_manifest_program manifest_program;
  w_seed_manifest_result manifest_result;
  w_seed_source_binding_link link;
  w_seed_source_binding_input binding_input;
} unit_fixture;

static bool unit_fixture_prepare(unit_fixture *fixture) {
  if (fixture == NULL) return false;
  (void)memset(fixture, 0, sizeof(*fixture));
  fixture->manifest_context.bytes = unit_manifest_bytes;
  fixture->manifest_context.length = sizeof(unit_manifest_bytes) - 1u;
  fixture->acquisition_context.marker = 7u;
  if (!w_seed_acquisition_storage_init(&fixture->acquisition_storage))
    return false;

  w_seed_ephemeral_driver_slot *slot = &fixture->slots[0];
  (void)memcpy(fixture->source_id_storage, unit_source_id,
               sizeof(unit_source_id));
  slot->source_id_storage = fixture->source_id_storage;
  slot->source_id_capacity = sizeof(fixture->source_id_storage);
  slot->source_id_length = sizeof(unit_source_id) - 1u;
  slot->module_id_storage = NULL;
  slot->module_id_capacity = 0u;
  slot->module_id_length = 0u;
  w_seed_source_error source_error;
  (void)memcpy(fixture->source_bytes, unit_acquisition_bytes,
               sizeof(unit_acquisition_bytes));
  if (!w_seed_source_init(
          (w_seed_byte_view){fixture->source_bytes,
                             sizeof(unit_acquisition_bytes) - 1u},
          &slot->source, &source_error))
    return false;
  slot->facts.provider_id =
      (w_seed_frontend_text){fixture->provider_storage,
                             sizeof(unit_provider_id) - 1u};
  slot->facts.root_token =
      (w_seed_frontend_text){fixture->root_token_storage,
                             sizeof(unit_root_token) - 1u};
  slot->facts.source_provider_owner_token =
      (w_seed_frontend_text){fixture->owner_token_storage,
                             sizeof(unit_owner_token) - 1u};
  slot->facts.canonical_token =
      (w_seed_frontend_text){fixture->canonical_token_storage,
                             sizeof(unit_canonical_token) - 1u};
  (void)memcpy(fixture->provider_storage, unit_provider_id,
               sizeof(unit_provider_id));
  (void)memcpy(fixture->root_token_storage, unit_root_token,
               sizeof(unit_root_token));
  (void)memcpy(fixture->owner_token_storage, unit_owner_token,
               sizeof(unit_owner_token));
  (void)memcpy(fixture->canonical_token_storage, unit_canonical_token,
               sizeof(unit_canonical_token));
  slot->facts.opened = true;
  slot->facts.containment_inside = true;
  slot->facts.symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE;
  slot->facts.snapshot_before_byte_count = slot->source.bytes.length;
  slot->facts.snapshot_after_byte_count = slot->source.bytes.length;
  if (!w_seed_ephemeral_graph_source_digest(
          &slot->source, slot->facts.snapshot_before_digest))
    return false;
  (void)memcpy(slot->facts.snapshot_after_digest,
               slot->facts.snapshot_before_digest,
               sizeof(slot->facts.snapshot_after_digest));
  fixture->candidate_facts[0] = slot->facts;
  fixture->requests[0].source_id =
      (w_seed_frontend_text){fixture->source_id_storage,
                             sizeof(unit_source_id) - 1u};
  fixture->requests[0].source = &slot->source;
  fixture->requests[0].facts = &slot->facts;
  fixture->requests[0].tokens = (w_seed_ephemeral_provider_token_buffers){
      fixture->provider_storage, sizeof(fixture->provider_storage),
      fixture->root_token_storage, sizeof(fixture->root_token_storage),
      fixture->owner_token_storage, sizeof(fixture->owner_token_storage),
      fixture->canonical_token_storage, sizeof(fixture->canonical_token_storage)};
  fixture->requests[0].revalidation_tokens =
      (w_seed_ephemeral_provider_token_buffers){
          fixture->revalidation_provider_storage,
          sizeof(fixture->revalidation_provider_storage),
          fixture->revalidation_root_storage,
          sizeof(fixture->revalidation_root_storage),
          fixture->revalidation_owner_storage,
          sizeof(fixture->revalidation_owner_storage),
          fixture->revalidation_canonical_storage,
          sizeof(fixture->revalidation_canonical_storage)};
  fixture->driver_scratch = (w_seed_ephemeral_driver_scratch){
      fixture->slots,
      1u,
      fixture->requests,
      1u,
      NULL,
      0u,
      NULL,
      0u,
      NULL,
      0u,
      NULL,
      0u,
      NULL,
      0u,
      fixture->candidate_documents,
      1u,
      fixture->candidate_facts,
      1u,
      &fixture->graph_scratch};
  fixture->driver_output = (w_seed_ephemeral_driver_output){
      {NULL, 0u, NULL, 0u, NULL, 0u, NULL, 0u},
      fixture->output_documents,
      1u,
      1u};
  fixture->output_documents[0].logical_source_id = fixture->requests[0].source_id;
  fixture->output_documents[0].source = &slot->source;
  (void)memset(&fixture->driver_input, 0, sizeof(fixture->driver_input));
  fixture->driver_input.root_path =
      (w_seed_byte_view){(const uint8_t *)unit_root_path,
                         sizeof(unit_root_path) - 1u};
  fixture->driver_input.root_source_id = fixture->requests[0].source_id;
  fixture->driver_input.provider_limits =
      (w_seed_ephemeral_provider_limits){
          1u, UNIT_ACQ_BYTES, UNIT_ACQ_BYTES, 64u, UNIT_ACQ_TOKEN_BYTES};
  fixture->driver_input.backend.context = &fixture->acquisition_context;
  fixture->driver_input.backend.open_root = unit_open_root;
  fixture->driver_input.backend.open_source = unit_open_source;
  fixture->driver_input.backend.read_source = unit_read_source;
  fixture->driver_input.backend.revalidate_source = unit_revalidate_source;
  fixture->driver_input.backend.close_source = unit_close_source;
  fixture->driver_input.backend.close_root = unit_close_root;
  fixture->acquisition_pipeline = (w_seed_acquisition_pipeline_input){
      &fixture->driver_input,
      &fixture->driver_scratch,
      &fixture->driver_output,
      &fixture->acquisition_storage,
      sizeof(fixture->acquisition_context)};
  fixture->acquisition_result.status = W_SEED_ACQUISITION_PIPELINE_OK;
  fixture->acquisition_result.attempts = 1u;
  fixture->acquisition_result.driver_result.status =
      W_SEED_EPHEMERAL_DRIVER_OK;
  fixture->acquisition_result.driver_result.phase =
      W_SEED_EPHEMERAL_DRIVER_PHASE_COMMIT;
  fixture->acquisition_result.driver_result.provider_status =
      W_SEED_EPHEMERAL_PROVIDER_OK;
  fixture->acquisition_result.driver_result.provider_result =
      (w_seed_ephemeral_provider_result){
          W_SEED_EPHEMERAL_PROVIDER_OK,
          W_SEED_EPHEMERAL_PROVIDER_FAILURE_NONE,
          W_SEED_EPHEMERAL_PROVIDER_PHASE_COMMIT,
          SIZE_MAX,
          slot->source.bytes.length,
          W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_NONE,
          0u,
          0u,
          0u,
          W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK};
  fixture->acquisition_result.driver_result.graph_status =
      W_SEED_EPHEMERAL_GRAPH_OK;
  fixture->acquisition_result.driver_result.graph_result.status =
      W_SEED_EPHEMERAL_GRAPH_OK;
  fixture->acquisition_result.driver_result.graph_result.written =
      (w_seed_ephemeral_graph_counts){1u, 0u, slot->source.bytes.length};
  fixture->acquisition_result.document_count = 1u;
  fixture->acquisition_result.graph_written =
      (w_seed_ephemeral_graph_counts){1u, 0u, slot->source.bytes.length};

  w_seed_owner_guard_backend owner_backend = {
      &fixture->manifest_context,
      unit_owner_begin,
      unit_owner_revalidate,
      unit_owner_abort,
      unit_owner_destroy};
  const w_seed_owner_guard_input owner_input = {
      {(const uint8_t *)"source", sizeof("source") - 1u},
      1u,
      {fixture->owner_staged, 1u, fixture->owner_revalidation, 1u,
       fixture->owner_candidates, 1u},
      owner_backend,
      sizeof(fixture->manifest_context)};
  w_seed_owner_guard_result owner_result;
  (void)memset(&fixture->guard, 0, sizeof(fixture->guard));
  if (w_seed_owner_guard_begin(&owner_input, &fixture->guard, &owner_result) !=
      W_SEED_OWNER_GUARD_OK)
    return false;

  fixture->manifest_limits = w_seed_manifest_default_limits();
  fixture->manifest_limits.max_document_bytes = UNIT_MAN_DOCUMENT_BYTES;
  fixture->manifest_limits.max_aggregate_bytes = 512u;
  fixture->manifest_limits.max_nesting = 16u;
  fixture->manifest_limits.max_structural_nodes = UNIT_MAN_STRUCTURAL_NODES;
  fixture->manifest_limits.max_roots_per_document = 2u;
  fixture->manifest_limits.max_documents = 1u;
  fixture->manifest_limits.max_scalar_source_bytes = UNIT_MAN_DOCUMENT_BYTES;
  fixture->manifest_limits.max_number_digits = 32u;
  fixture->manifest_limits.max_decoded_scalar_bytes = 128u;
  fixture->manifest_limits.max_canonical_bytes = UNIT_MAN_CANONICAL_BYTES;
  fixture->manifest_limits.max_work_units = UINT64_C(1048576);
  fixture->manifest_slots[0] = (w_seed_manifest_read_slot){
      fixture->manifest_first, sizeof(fixture->manifest_first),
      fixture->manifest_second, sizeof(fixture->manifest_second)};
  const w_seed_manifest_output staged = {
      fixture->staged_documents,
      1u,
      fixture->staged_roots,
      2u,
      fixture->staged_nodes,
      UNIT_MAN_STRUCTURAL_NODES,
      fixture->staged_fields,
      UNIT_MAN_STRUCTURAL_NODES,
      fixture->staged_edges,
      UNIT_MAN_STRUCTURAL_NODES,
      fixture->staged_canonical,
      sizeof(fixture->staged_canonical)};
  const w_seed_manifest_output published = {
      fixture->published_documents,
      1u,
      fixture->published_roots,
      2u,
      fixture->published_nodes,
      UNIT_MAN_STRUCTURAL_NODES,
      fixture->published_fields,
      UNIT_MAN_STRUCTURAL_NODES,
      fixture->published_edges,
      UNIT_MAN_STRUCTURAL_NODES,
      fixture->published_canonical,
      sizeof(fixture->published_canonical)};
  fixture->manifest_backend = (w_seed_manifest_backend){
      NULL,
      &fixture->guard,
      &fixture->manifest_context,
      sizeof(fixture->manifest_context),
      41u,
      unit_manifest_read};
  fixture->manifest_backend.owner = &fixture->manifest_backend;
  fixture->manifest_input = (w_seed_manifest_guarded_input){
      &fixture->guard,
      &fixture->manifest_backend,
      fixture->manifest_limits,
      {fixture->manifest_slots,
       1u,
       fixture->manifest_sources,
       1u,
       {fixture->manifest_name_slots, UNIT_MAN_STRUCTURAL_NODES,
        fixture->manifest_scratch_bytes, sizeof(fixture->manifest_scratch_bytes)},
       staged,
       published}};
  fixture->manifest_result = w_seed_manifest_guarded_run(
      &fixture->manifest_input, &fixture->manifest_program);
  if (fixture->manifest_result.status != W_SEED_MANIFEST_OK)
    return false;
  fixture->link = (w_seed_source_binding_link){
      NULL,
      &fixture->manifest_context,
      sizeof(fixture->manifest_context),
      unit_link_compose};
  fixture->link.owner = &fixture->link;
  fixture->binding_input = (w_seed_source_binding_input){
      {&fixture->acquisition_pipeline, &fixture->acquisition_result},
      {&fixture->manifest_input,
       &fixture->manifest_program,
       &fixture->manifest_result,
       &fixture->manifest_input.storage.scratch},
      &fixture->link};
  return fixture->guard.lifecycle == W_SEED_OWNER_GUARD_LIVE_RECONFIRMED &&
         fixture->guard.disposition ==
             W_SEED_OWNER_GUARD_CANDIDATES_RECONFIRMED;
}

static bool unit_compose_failure_preserves(unit_fixture *fixture,
                                           w_seed_source_binding_status expected) {
  w_seed_source_binding binding;
  (void)memset(&binding, 0xa5, sizeof(binding));
  const w_seed_source_binding before = binding;
  const w_seed_source_binding_result result =
      w_seed_source_binding_compose(&fixture->binding_input, &binding);
  return result.status == expected &&
         memcmp(&binding, &before, sizeof(binding)) == 0;
}

static int run_core_tests(void) {
  unit_fixture fixture;
  CHECK(unit_fixture_prepare(&fixture));
  unit_link_mode_value = UNIT_LINK_OK;
  w_seed_source_binding binding;
  (void)memset(&binding, 0xa5, sizeof(binding));
  {
    w_seed_source_binding_input stack_wrapper = fixture.binding_input;
    const w_seed_source_binding_result result =
        w_seed_source_binding_compose(&stack_wrapper, &binding);
    CHECK(result.status == W_SEED_SOURCE_BINDING_OK);
    CHECK(result.phase == W_SEED_SOURCE_BINDING_PHASE_COMMIT);
    CHECK(result.lifecycle == W_SEED_SOURCE_BINDING_BOUND);
    CHECK(result.guard_generation == 41u);
  }
  CHECK(binding.owner == &binding);
  CHECK(binding.lifecycle == W_SEED_SOURCE_BINDING_BOUND);
  CHECK(memcmp(binding.schema, W_SEED_SOURCE_BINDING_SCHEMA_VERSION,
               sizeof(binding.schema)) == 0);
  CHECK(memcmp(binding.link_digest, (uint8_t[32]){
                                     0xa0u, 0xa1u, 0xa2u, 0xa3u, 0xa4u,
                                     0xa5u, 0xa6u, 0xa7u, 0xa8u, 0xa9u,
                                     0xaau, 0xabu, 0xacu, 0xadu, 0xaeu,
                                     0xafu, 0xb0u, 0xb1u, 0xb2u, 0xb3u,
                                     0xb4u, 0xb5u, 0xb6u, 0xb7u, 0xb8u,
                                     0xb9u, 0xbau, 0xbbu, 0xbcu, 0xbdu,
                                     0xbeu, 0xbfu},
               sizeof(binding.link_digest)) == 0);
  CHECK(w_seed_source_binding_verify(&binding));
  /* verify reconstructs its own wrapper after the original wrapper's scope. */
  {
    w_seed_source_binding_input stack_wrapper = fixture.binding_input;
    CHECK(stack_wrapper.link == &fixture.link);
  }
  CHECK(w_seed_source_binding_verify(&binding));
  w_seed_source_binding copied = binding;
  CHECK(!w_seed_source_binding_verify(&copied));

  /* Verify must reject forged descriptor pointers without comparing padding. */
  {
    w_seed_acquisition_pipeline_input copied_pipeline =
        *binding.acquisition_pipeline;
    copied_pipeline.driver_input = NULL;
    w_seed_source_binding forged = binding;
    forged.acquisition_pipeline = &copied_pipeline;
    CHECK(!w_seed_source_binding_verify(&forged));
  }

  /* Every digest family and the schema are part of the portable relation. */
  {
    w_seed_source_binding forged = binding;
    forged.acquisition_root_facts_digest[0] ^= 0x01u;
    CHECK(!w_seed_source_binding_verify(&forged));
  }
  {
    w_seed_source_binding forged = binding;
    forged.acquisition_source_digest[0] ^= 0x01u;
    CHECK(!w_seed_source_binding_verify(&forged));
  }
  {
    w_seed_source_binding forged = binding;
    forged.manifest_receipt_digest[0] ^= 0x01u;
    CHECK(!w_seed_source_binding_verify(&forged));
  }
  {
    w_seed_source_binding forged = binding;
    forged.manifest_context_binding_digest[0] ^= 0x01u;
    CHECK(!w_seed_source_binding_verify(&forged));
  }
  {
    w_seed_source_binding forged = binding;
    forged.manifest_candidate_binding_digest[0] ^= 0x01u;
    CHECK(!w_seed_source_binding_verify(&forged));
  }
  {
    w_seed_source_binding forged = binding;
    forged.link_digest[0] ^= 0x01u;
    CHECK(!w_seed_source_binding_verify(&forged));
  }
  {
    w_seed_source_binding forged = binding;
    forged.binding_digest[0] ^= 0x01u;
    CHECK(!w_seed_source_binding_verify(&forged));
  }
  {
    w_seed_source_binding forged = binding;
    forged.schema[0] ^= 0x01;
    CHECK(!w_seed_source_binding_verify(&forged));
  }

  const unit_link_mode modes[] = {UNIT_LINK_INVALID, UNIT_LINK_MISMATCH,
                                  UNIT_LINK_MUTATED, UNIT_LINK_BOUNDARY,
                                  UNIT_LINK_UNSUPPORTED, UNIT_LINK_IO};
  const w_seed_source_binding_status statuses[] = {
      W_SEED_SOURCE_BINDING_INVALID,
      W_SEED_SOURCE_BINDING_MISMATCH,
      W_SEED_SOURCE_BINDING_MUTATED,
      W_SEED_SOURCE_BINDING_BOUNDARY,
      W_SEED_SOURCE_BINDING_UNSUPPORTED,
      W_SEED_SOURCE_BINDING_IO};
  for (size_t index = 0u; index < sizeof(modes) / sizeof(modes[0]); index += 1u) {
    unit_link_mode_value = modes[index];
    CHECK(unit_compose_failure_preserves(&fixture, statuses[index]));
  }
  unit_link_mode_value = UNIT_LINK_OK;

  fixture.manifest_result.receipt_digest[0] ^= 0x01u;
  CHECK(unit_compose_failure_preserves(&fixture,
                                       W_SEED_SOURCE_BINDING_MUTATED));
  fixture.manifest_result.receipt_digest[0] ^= 0x01u;
  fixture.published_documents[0].candidate.candidate_index = 1u;
  CHECK(unit_compose_failure_preserves(&fixture,
                                       W_SEED_SOURCE_BINDING_MUTATED));
  fixture.published_documents[0].candidate.candidate_index = 0u;
  fixture.manifest_backend.generation = 40u;
  CHECK(unit_compose_failure_preserves(&fixture,
                                       W_SEED_SOURCE_BINDING_MUTATED));
  fixture.manifest_backend.generation = 41u;
  fixture.acquisition_result.status = W_SEED_ACQUISITION_PIPELINE_INVALID;
  CHECK(unit_compose_failure_preserves(&fixture,
                                       W_SEED_SOURCE_BINDING_INVALID));
  fixture.acquisition_result.status = W_SEED_ACQUISITION_PIPELINE_OK;
  fixture.requests[0].source_id = unit_text("other");
  CHECK(unit_compose_failure_preserves(&fixture,
                                       W_SEED_SOURCE_BINDING_INVALID));
  fixture.requests[0].source_id = fixture.driver_input.root_source_id;

  fixture.graph_scratch.node_capacity = SIZE_MAX;
  CHECK(unit_compose_failure_preserves(&fixture,
                                       W_SEED_SOURCE_BINDING_INVALID));
  fixture.graph_scratch.node_capacity = 0u;
  fixture.slots[0].facts.provider_id.length = SIZE_MAX;
  CHECK(unit_compose_failure_preserves(&fixture,
                                       W_SEED_SOURCE_BINDING_INVALID));
  fixture.slots[0].facts.provider_id = unit_text(unit_provider_id);
  CHECK(unit_compose_failure_preserves(&fixture,
                                       W_SEED_SOURCE_BINDING_INVALID));
  /* The previous call must have failed before any output write; restore the
   * exact provider view and prove the stable binding remains independently. */
  fixture.slots[0].facts.provider_id =
      (w_seed_frontend_text){fixture.provider_storage,
                             sizeof(unit_provider_id) - 1u};
  CHECK(w_seed_source_binding_verify(&binding));
  uint8_t input_snapshot[sizeof(fixture.binding_input)];
  (void)memcpy(input_snapshot, &fixture.binding_input, sizeof(input_snapshot));
  CHECK(w_seed_source_binding_compose(
            &fixture.binding_input,
            (w_seed_source_binding *)(void *)&fixture.binding_input)
            .status == W_SEED_SOURCE_BINDING_ALIAS);
  CHECK(memcmp(&fixture.binding_input, input_snapshot, sizeof(input_snapshot)) ==
        0);

  w_seed_owner_guard_destroy(&fixture.guard);
  w_seed_acquisition_storage_destroy(&fixture.acquisition_storage);
  return EXIT_SUCCESS;
}

#if defined(__linux__)

static void unit_linux_token(char *destination, char prefix, uint64_t mount_id,
                             uint64_t device_major, uint64_t device_minor,
                             uint64_t inode) {
  static const char hex[] = "0123456789abcdef";
  const uint64_t values[] = {mount_id, device_major, device_minor, inode};
  destination[0] = prefix;
  size_t cursor = 1u;
  for (size_t field = 0u; field < 4u; field += 1u) {
    for (size_t nibble = 0u; nibble < 16u; nibble += 1u)
      destination[cursor + nibble] =
          hex[(values[field] >> (60u - (unsigned int)(nibble * 4u))) & 0xfu];
    cursor += 16u;
    if (field != 3u) destination[cursor++] = '-';
  }
}

static int run_linux_link_tests(void) {
  w_seed_owner_guard_linux_context context;
  (void)memset(&context, 0, sizeof(context));
  context.initialized = true;
  context.native_supported = true;
  context.session_live = true;
  context.active_generation = 41u;
  context.level_count = 1u;
  context.candidate_count = 1u;
  context.source_slot = 1u;
  context.directory_slots[0] = 0u;
  context.slots[0] = (w_seed_owner_guard_linux_slot){
      10, {7u, 8u, 9u, 100u}, W_SEED_OWNER_GUARD_LINUX_SLOT_DIRECTORY, true};
  context.slots[1] = (w_seed_owner_guard_linux_slot){
      11, {7u, 8u, 9u, 200u}, W_SEED_OWNER_GUARD_LINUX_SLOT_SOURCE, true};
  char provider[sizeof(W_SEED_EPHEMERAL_PROVIDER_LINUX_V2_ID)];
  char root[W_SEED_EPHEMERAL_PROVIDER_LINUX_V2_TOKEN_BYTES];
  char owner[W_SEED_EPHEMERAL_PROVIDER_LINUX_V2_TOKEN_BYTES];
  char canonical[W_SEED_EPHEMERAL_PROVIDER_LINUX_V2_TOKEN_BYTES];
  (void)memcpy(provider, W_SEED_EPHEMERAL_PROVIDER_LINUX_V2_ID,
               sizeof(provider));
  unit_linux_token(root, 'r', 7u, 8u, 9u, 100u);
  unit_linux_token(owner, 'o', 7u, 8u, 9u, 100u);
  unit_linux_token(canonical, 'c', 7u, 8u, 9u, 200u);
  w_seed_ephemeral_graph_provider_facts facts;
  (void)memset(&facts, 0, sizeof(facts));
  facts.provider_id = (w_seed_frontend_text){provider, sizeof(provider) - 1u};
  facts.root_token = (w_seed_frontend_text){root, sizeof(root)};
  facts.source_provider_owner_token =
      (w_seed_frontend_text){owner, sizeof(owner)};
  facts.canonical_token = (w_seed_frontend_text){canonical, sizeof(canonical)};
  facts.opened = true;
  facts.containment_inside = true;
  facts.symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE;
  facts.snapshot_before_byte_count = 1u;
  facts.snapshot_before_digest[0] = 1u;
  facts.snapshot_after_byte_count = 1u;
  facts.snapshot_after_digest[0] = 1u;
  w_seed_source_binding_link link;
  CHECK(w_seed_source_binding_linux_link(&context, &link));
  const w_seed_source_binding_link_input input = {&facts, 1u, 0u, 41u};
  w_seed_source_binding_link_result result = link.compose(&link, &input);
  CHECK(result.status == W_SEED_SOURCE_BINDING_LINK_OK);
  CHECK(result.phase == W_SEED_SOURCE_BINDING_LINK_PHASE_COMMIT);
  CHECK(result.link_digest[0] != 0u);

  context.active_generation = 42u;
  result = link.compose(&link, &input);
  CHECK(result.status == W_SEED_SOURCE_BINDING_LINK_MUTATED);
  CHECK(result.phase == W_SEED_SOURCE_BINDING_LINK_PHASE_OWNER);
  context.active_generation = 41u;

  facts.provider_id = unit_text("linux-openat2-v1");
  result = link.compose(&link, &input);
  CHECK(result.status == W_SEED_SOURCE_BINDING_LINK_UNSUPPORTED);
  facts.provider_id = (w_seed_frontend_text){provider, sizeof(provider) - 1u};
  root[1] = 'f';
  result = link.compose(&link, &input);
  CHECK(result.status == W_SEED_SOURCE_BINDING_LINK_MISMATCH);
  unit_linux_token(root, 'r', 7u, 8u, 9u, 100u);
  unit_linux_token(owner, 'o', 7u, 8u, 9u, 101u);
  result = link.compose(&link, &input);
  CHECK(result.status == W_SEED_SOURCE_BINDING_LINK_MISMATCH);
  unit_linux_token(owner, 'o', 7u, 8u, 9u, 100u);
  unit_linux_token(canonical, 'c', 7u, 8u, 9u, 201u);
  result = link.compose(&link, &input);
  CHECK(result.status == W_SEED_SOURCE_BINDING_LINK_MISMATCH);
  unit_linux_token(canonical, 'c', 7u, 8u, 9u, 200u);
  facts.provider_id = unit_text("linux-openat2-v2-forged");
  result = link.compose(&link, &input);
  CHECK(result.status == W_SEED_SOURCE_BINDING_LINK_UNSUPPORTED);
  return EXIT_SUCCESS;
}

#else

static int run_linux_link_tests(void) {
  w_seed_owner_guard_linux_context context;
  w_seed_source_binding_link link;
  (void)memset(&context, 0, sizeof(context));
  CHECK(w_seed_source_binding_linux_link(&context, &link));
  w_seed_ephemeral_graph_provider_facts facts = {0};
  const w_seed_source_binding_link_input input = {&facts, 1u, 0u, 41u};
  const w_seed_source_binding_link_result result = link.compose(&link, &input);
  CHECK(result.status == W_SEED_SOURCE_BINDING_LINK_UNSUPPORTED);
  CHECK(result.phase == W_SEED_SOURCE_BINDING_LINK_PHASE_VALIDATE);
  return EXIT_SUCCESS;
}

#endif

int main(void) {
  if (run_core_tests() != EXIT_SUCCESS) return EXIT_FAILURE;
  if (run_linux_link_tests() != EXIT_SUCCESS) return EXIT_FAILURE;
  (void)puts("source-binding unit: ok");
  return EXIT_SUCCESS;
}
