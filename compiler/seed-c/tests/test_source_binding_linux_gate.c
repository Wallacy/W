#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#endif

#include "w_seed_acquisition.h"
#include "w_seed_ephemeral_provider_linux.h"
#include "w_seed_manifest_linux.h"
#include "w_seed_owner_guard_linux.h"
#include "w_seed_source_binding_linux.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if !defined(__linux__)

int main(void) {
  (void)fprintf(stderr, "w_seed_source_binding_linux_gate: Linux required\n");
  return 1;
}

#else

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

enum {
  GATE_PATH_CAPACITY = 512u,
  GATE_SOURCE_CAPACITY = 4096u,
  GATE_TOKEN_CAPACITY = 128u,
  GATE_ACQ_SOURCES = 4u,
  GATE_ACQ_LEXER_FRAMES = 256u,
  GATE_ACQ_TOKENS = 2048u,
  GATE_ACQ_PARSE_FRAMES = 512u,
  GATE_ACQ_ISSUES = 128u,
  GATE_ACQ_ORIGINS = 64u,
  GATE_ACQ_EDGES = 4u,
  GATE_MAN_CANDIDATES = 2u,
  GATE_MAN_DOCUMENT_BYTES = 4096u,
  GATE_MAN_STRUCTURAL = 512u,
  GATE_MAN_CANONICAL = 1024u,
  GATE_MAN_AGGREGATE = 8192u,
  GATE_MAN_NODES = 128u,
  GATE_MAN_FIELDS = 128u,
  GATE_MAN_EDGES = 128u,
};

typedef struct {
  char root[GATE_PATH_CAPACITY];
  char a[GATE_PATH_CAPACITY];
  char b[GATE_PATH_CAPACITY];
  char source[GATE_PATH_CAPACITY];
  char same_parent_source[GATE_PATH_CAPACITY];
  char other_root[GATE_PATH_CAPACITY];
  char other_a[GATE_PATH_CAPACITY];
  char other_b[GATE_PATH_CAPACITY];
  char other_parent_source[GATE_PATH_CAPACITY];
  char nested_build[GATE_PATH_CAPACITY];
  char root_build[GATE_PATH_CAPACITY];
  char source_relative[GATE_PATH_CAPACITY];
  char same_parent_relative[GATE_PATH_CAPACITY];
  char other_parent_relative[GATE_PATH_CAPACITY];
  int base_fd;
} gate_fixture;

typedef struct {
  char root_path[GATE_PATH_CAPACITY];
  char root_source_id[GATE_PATH_CAPACITY];
  char source_ids[GATE_ACQ_SOURCES][GATE_PATH_CAPACITY];
  char module_ids[GATE_ACQ_SOURCES][GATE_PATH_CAPACITY];
  char provider[GATE_ACQ_SOURCES][GATE_TOKEN_CAPACITY];
  char root_token[GATE_ACQ_SOURCES][GATE_TOKEN_CAPACITY];
  char owner_token[GATE_ACQ_SOURCES][GATE_TOKEN_CAPACITY];
  char canonical_token[GATE_ACQ_SOURCES][GATE_TOKEN_CAPACITY];
  char revalidation_provider[GATE_ACQ_SOURCES][GATE_TOKEN_CAPACITY];
  char revalidation_root_token[GATE_ACQ_SOURCES][GATE_TOKEN_CAPACITY];
  char revalidation_owner_token[GATE_ACQ_SOURCES][GATE_TOKEN_CAPACITY];
  char revalidation_canonical_token[GATE_ACQ_SOURCES][GATE_TOKEN_CAPACITY];
  w_seed_cst_node nodes[GATE_ACQ_SOURCES][1024u];
  w_seed_ephemeral_driver_slot slots[GATE_ACQ_SOURCES];
  w_seed_ephemeral_provider_request requests[GATE_ACQ_SOURCES];
  w_seed_lexer_frame lexer_frames[GATE_ACQ_LEXER_FRAMES];
  w_seed_parse_token tokens[GATE_ACQ_TOKENS];
  w_seed_parse_frame parse_frames[GATE_ACQ_PARSE_FRAMES];
  w_seed_parse_issue issues[GATE_ACQ_ISSUES];
  w_seed_module_origin origins[GATE_ACQ_ORIGINS];
  w_seed_frontend_document candidate_documents[GATE_ACQ_SOURCES];
  w_seed_ephemeral_graph_provider_facts candidate_facts[GATE_ACQ_SOURCES];
  w_seed_ephemeral_graph_scratch_node graph_nodes[GATE_ACQ_SOURCES];
  w_seed_ephemeral_graph_scratch_edge graph_edges[GATE_ACQ_EDGES];
  size_t sorted_nodes[GATE_ACQ_SOURCES];
  size_t node_ordinals[GATE_ACQ_SOURCES];
  size_t sorted_edges[GATE_ACQ_EDGES];
  size_t sorted_resolved_edges[GATE_ACQ_EDGES];
  w_seed_module_origin graph_origins[GATE_ACQ_EDGES];
  uint32_t indegree[GATE_ACQ_SOURCES];
  uint32_t queue[GATE_ACQ_SOURCES];
  uint32_t depths[GATE_ACQ_SOURCES];
  w_seed_frontend_document documents[GATE_ACQ_SOURCES];
  w_seed_ephemeral_graph_inventory_item inventory[GATE_ACQ_SOURCES];
  w_seed_ephemeral_graph_edge edges[GATE_ACQ_EDGES];
  uint32_t document_order[GATE_ACQ_SOURCES];
  w_seed_frontend_resolved_import resolved[GATE_ACQ_EDGES];
  w_seed_ephemeral_graph_scratch graph_scratch;
  w_seed_ephemeral_driver_scratch scratch;
  w_seed_ephemeral_driver_input driver_input;
  w_seed_ephemeral_driver_output output;
  w_seed_acquisition_storage storage;
  w_seed_acquisition_pipeline_input pipeline;
  w_seed_acquisition_pipeline_result result;
} gate_acq_fixture;

typedef struct {
  w_seed_owner_guard_linux_context context;
  w_seed_owner_guard_backend owner_backend;
  w_seed_owner_guard_observation staged_observations[
      W_SEED_OWNER_GUARD_MAX_LEVELS];
  w_seed_owner_guard_observation revalidation_observations[
      W_SEED_OWNER_GUARD_MAX_LEVELS];
  w_seed_owner_guard_candidate_ref candidates[W_SEED_OWNER_GUARD_MAX_LEVELS];
  w_seed_owner_guard_input owner_input;
  w_seed_owner_guard guard;

  w_seed_manifest_read_slot read_slots[GATE_MAN_CANDIDATES];
  uint8_t first_bytes[GATE_MAN_CANDIDATES][GATE_MAN_DOCUMENT_BYTES];
  uint8_t second_bytes[GATE_MAN_CANDIDATES][GATE_MAN_DOCUMENT_BYTES];
  w_seed_manifest_source_input staged_sources[GATE_MAN_CANDIDATES];
  w_seed_manifest_name_slot name_slots[GATE_MAN_STRUCTURAL];
  uint8_t scratch_bytes[GATE_MAN_DOCUMENT_BYTES +
                        W_SEED_MANIFEST_SCALAR_SCRATCH_OVERHEAD];
  w_seed_manifest_document staged_documents[GATE_MAN_CANDIDATES];
  w_seed_manifest_root staged_roots[4u];
  w_seed_manifest_node staged_nodes[GATE_MAN_NODES];
  w_seed_manifest_field staged_fields[GATE_MAN_FIELDS];
  w_seed_manifest_edge staged_edges[GATE_MAN_EDGES];
  uint8_t staged_canonical[GATE_MAN_CANONICAL];
  w_seed_manifest_document published_documents[GATE_MAN_CANDIDATES];
  w_seed_manifest_root published_roots[4u];
  w_seed_manifest_node published_nodes[GATE_MAN_NODES];
  w_seed_manifest_field published_fields[GATE_MAN_FIELDS];
  w_seed_manifest_edge published_edges[GATE_MAN_EDGES];
  uint8_t published_canonical[GATE_MAN_CANONICAL];
  w_seed_manifest_backend native_backend;
  w_seed_manifest_backend manifest_backend;
  w_seed_manifest_guarded_input guarded_input;
  w_seed_manifest_program program;
  w_seed_manifest_result result;
  w_seed_source_binding_link link;
} gate_session;

static gate_acq_fixture gate_original_acq;
static gate_acq_fixture gate_alternate_acq;
static gate_session gate_live_session;

#define GATE_CHECK(condition)                                                  \
  do {                                                                         \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "w_seed_source_binding_linux_gate: %s (%s:%d)\n", \
                    #condition, __FILE__, __LINE__);                           \
      return false;                                                            \
    }                                                                          \
  } while (0)

static bool gate_copy(char *destination, size_t capacity, const char *source) {
  if (destination == NULL || source == NULL) return false;
  const size_t length = strlen(source);
  if (length >= capacity) return false;
  (void)memcpy(destination, source, length + 1u);
  return true;
}

static bool gate_join(char *destination, size_t capacity, const char *directory,
                      const char *leaf) {
  if (destination == NULL || directory == NULL || leaf == NULL) return false;
  const size_t directory_length = strlen(directory);
  const size_t leaf_length = strlen(leaf);
  if (directory_length > capacity || leaf_length > capacity ||
      directory_length + 1u + leaf_length >= capacity)
    return false;
  (void)memcpy(destination, directory, directory_length);
  destination[directory_length] = '/';
  (void)memcpy(destination + directory_length + 1u, leaf, leaf_length + 1u);
  return true;
}

static bool gate_write(const char *path, const uint8_t *bytes, size_t length) {
  if (path == NULL || (length != 0u && bytes == NULL)) return false;
  const int file = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (file < 0) return false;
  size_t offset = 0u;
  bool success = true;
  while (offset < length) {
    const ssize_t written = write(file, bytes + offset, length - offset);
    if (written < (ssize_t)0) {
      if (errno == EINTR) continue;
      success = false;
      break;
    }
    if (written == (ssize_t)0) {
      success = false;
      break;
    }
    offset += (size_t)written;
  }
  if (close(file) != 0) success = false;
  return success && offset == length;
}

static bool gate_directory(const char *path) {
  if (path == NULL) return false;
  if (mkdir(path, 0700) == 0) return true;
  if (errno != EEXIST) return false;
  struct stat value;
  return stat(path, &value) == 0 && S_ISDIR(value.st_mode);
}

static bool gate_unlink(const char *path) {
  if (path == NULL) return false;
  return unlink(path) == 0 || errno == ENOENT;
}

static bool gate_fixture_paths(gate_fixture *fixture) {
  if (fixture == NULL) return false;
  (void)memset(fixture, 0, sizeof(*fixture));
  fixture->base_fd = -1;
  char directory_template[] = "/tmp/w-seed-source-binding-XXXXXX";
  char *directory = mkdtemp(directory_template);
  if (directory == NULL || !gate_copy(fixture->root, sizeof(fixture->root),
                                       directory))
    return false;
  if (!gate_join(fixture->a, sizeof(fixture->a), fixture->root, "a") ||
      !gate_join(fixture->b, sizeof(fixture->b), fixture->a, "b") ||
      !gate_join(fixture->source, sizeof(fixture->source), fixture->b,
                 "source.w") ||
      !gate_join(fixture->same_parent_source, sizeof(fixture->same_parent_source),
                 fixture->b, "last-light.w") ||
      !gate_join(fixture->other_root, sizeof(fixture->other_root), fixture->root,
                 "other") ||
      !gate_join(fixture->other_a, sizeof(fixture->other_a), fixture->other_root,
                 "a") ||
      !gate_join(fixture->other_b, sizeof(fixture->other_b), fixture->other_a,
                 "b") ||
      !gate_join(fixture->other_parent_source,
                 sizeof(fixture->other_parent_source), fixture->other_b,
                 "source.w") ||
      !gate_join(fixture->nested_build, sizeof(fixture->nested_build),
                 fixture->b, "build.w") ||
      !gate_join(fixture->root_build, sizeof(fixture->root_build), fixture->root,
                 "build.w") ||
      !gate_copy(fixture->source_relative, sizeof(fixture->source_relative),
                 "a/b/source.w") ||
      !gate_copy(fixture->same_parent_relative,
                 sizeof(fixture->same_parent_relative), "a/b/last-light.w") ||
      !gate_copy(fixture->other_parent_relative,
                 sizeof(fixture->other_parent_relative),
                 "other/a/b/source.w"))
    return false;
  return true;
}

static bool gate_fixture_create(gate_fixture *fixture) {
  static const uint8_t source[] =
      "module restaurant;\n"
      "fn lastLight(): i64 { return 42 }\n";
  static const uint8_t nested_build[] = "package { value: 1 }\n";
  static const uint8_t root_build[] = "workspace { value: 2 }\n";
  if (fixture == NULL || !gate_directory(fixture->root) ||
      !gate_directory(fixture->a) || !gate_directory(fixture->b) ||
      !gate_directory(fixture->other_root) ||
      !gate_directory(fixture->other_a) ||
      !gate_directory(fixture->other_b) ||
      !gate_write(fixture->source, source, sizeof(source) - 1u) ||
      !gate_write(fixture->same_parent_source, source, sizeof(source) - 1u) ||
      !gate_write(fixture->other_parent_source, source, sizeof(source) - 1u) ||
      !gate_write(fixture->nested_build, nested_build, sizeof(nested_build) - 1u) ||
      !gate_write(fixture->root_build, root_build, sizeof(root_build) - 1u))
    return false;
  fixture->base_fd = open(fixture->root, O_PATH | O_DIRECTORY | O_CLOEXEC);
  return fixture->base_fd >= 0;
}

static void gate_fixture_destroy(gate_fixture *fixture) {
  if (fixture == NULL) return;
  if (fixture->base_fd >= 0) {
    (void)close(fixture->base_fd);
    fixture->base_fd = -1;
  }
  (void)gate_unlink(fixture->source);
  (void)gate_unlink(fixture->same_parent_source);
  (void)gate_unlink(fixture->other_parent_source);
  (void)gate_unlink(fixture->nested_build);
  (void)gate_unlink(fixture->root_build);
  (void)rmdir(fixture->b);
  (void)rmdir(fixture->a);
  (void)rmdir(fixture->other_b);
  (void)rmdir(fixture->other_a);
  (void)rmdir(fixture->other_root);
  (void)rmdir(fixture->root);
}

static bool gate_acq_init(gate_acq_fixture *fixture, const char *root_path,
                          const char *root_source_id,
                          const w_seed_ephemeral_provider_backend *backend) {
  if (fixture == NULL || root_path == NULL || root_source_id == NULL ||
      backend == NULL || backend->context == NULL)
    return false;
  (void)memset(fixture, 0, sizeof(*fixture));
  if (!gate_copy(fixture->root_path, sizeof(fixture->root_path), root_path) ||
      !gate_copy(fixture->root_source_id, sizeof(fixture->root_source_id),
                 root_source_id))
    return false;
  for (size_t index = 0u; index < GATE_ACQ_SOURCES; index += 1u) {
    fixture->slots[index].source_id_storage = fixture->source_ids[index];
    fixture->slots[index].source_id_capacity = GATE_PATH_CAPACITY;
    fixture->slots[index].module_id_storage = fixture->module_ids[index];
    fixture->slots[index].module_id_capacity = GATE_PATH_CAPACITY;
    fixture->slots[index].nodes = fixture->nodes[index];
    fixture->slots[index].node_capacity = 1024u;
    fixture->requests[index].tokens = (w_seed_ephemeral_provider_token_buffers){
        fixture->provider[index], GATE_TOKEN_CAPACITY,
        fixture->root_token[index], GATE_TOKEN_CAPACITY,
        fixture->owner_token[index], GATE_TOKEN_CAPACITY,
        fixture->canonical_token[index], GATE_TOKEN_CAPACITY};
    fixture->requests[index].revalidation_tokens =
        (w_seed_ephemeral_provider_token_buffers){
            fixture->revalidation_provider[index], GATE_TOKEN_CAPACITY,
            fixture->revalidation_root_token[index], GATE_TOKEN_CAPACITY,
            fixture->revalidation_owner_token[index], GATE_TOKEN_CAPACITY,
            fixture->revalidation_canonical_token[index], GATE_TOKEN_CAPACITY};
  }
  fixture->graph_scratch = (w_seed_ephemeral_graph_scratch){
      fixture->graph_nodes, GATE_ACQ_SOURCES, fixture->graph_edges,
      GATE_ACQ_EDGES, fixture->sorted_nodes, GATE_ACQ_SOURCES,
      fixture->node_ordinals, GATE_ACQ_SOURCES, fixture->sorted_edges,
      GATE_ACQ_EDGES, fixture->sorted_resolved_edges, GATE_ACQ_EDGES,
      fixture->graph_origins, GATE_ACQ_EDGES, fixture->indegree,
      GATE_ACQ_SOURCES, fixture->queue, GATE_ACQ_SOURCES, fixture->depths,
      GATE_ACQ_SOURCES};
  fixture->scratch = (w_seed_ephemeral_driver_scratch){
      fixture->slots, GATE_ACQ_SOURCES, fixture->requests, GATE_ACQ_SOURCES,
      fixture->lexer_frames, GATE_ACQ_LEXER_FRAMES, fixture->tokens,
      GATE_ACQ_TOKENS, fixture->parse_frames, GATE_ACQ_PARSE_FRAMES,
      fixture->issues, GATE_ACQ_ISSUES, fixture->origins, GATE_ACQ_ORIGINS,
      fixture->candidate_documents, GATE_ACQ_SOURCES, fixture->candidate_facts,
      GATE_ACQ_SOURCES, &fixture->graph_scratch};
  fixture->output = (w_seed_ephemeral_driver_output){
      {fixture->inventory, GATE_ACQ_SOURCES, fixture->edges, GATE_ACQ_EDGES,
       fixture->document_order, GATE_ACQ_SOURCES, fixture->resolved,
       GATE_ACQ_EDGES},
      fixture->documents, GATE_ACQ_SOURCES, 0u};
  fixture->driver_input = (w_seed_ephemeral_driver_input){
      {(const uint8_t *)fixture->root_path, strlen(fixture->root_path)},
      {fixture->root_source_id, strlen(fixture->root_source_id)},
      {GATE_ACQ_SOURCES, GATE_SOURCE_CAPACITY,
       GATE_SOURCE_CAPACITY * GATE_ACQ_SOURCES, GATE_PATH_CAPACITY,
       GATE_TOKEN_CAPACITY},
      GATE_ACQ_EDGES,
      W_SEED_EPHEMERAL_GRAPH_MAX_DEPTH,
      {65536u, 256u},
      *backend};
  fixture->pipeline = (w_seed_acquisition_pipeline_input){
      &fixture->driver_input, &fixture->scratch, &fixture->output,
      &fixture->storage, sizeof(w_seed_ephemeral_provider_linux_context)};
  return w_seed_acquisition_storage_init(&fixture->storage);
}

static bool gate_acq_run(gate_acq_fixture *fixture) {
  if (fixture == NULL) return false;
  (void)memset(&fixture->result, 0, sizeof(fixture->result));
  const w_seed_acquisition_pipeline_status status =
      w_seed_acquisition_pipeline_run(&fixture->pipeline, &fixture->result);
  return status == W_SEED_ACQUISITION_PIPELINE_OK &&
         fixture->result.status == W_SEED_ACQUISITION_PIPELINE_OK &&
         fixture->result.document_count == 1u &&
         fixture->result.graph_written.sources == 1u &&
         fixture->result.graph_written.edges == 0u &&
         fixture->output.document_count == 1u;
}

static w_seed_manifest_limits gate_manifest_limits(void) {
  w_seed_manifest_limits limits = w_seed_manifest_default_limits();
  limits.max_document_bytes = GATE_MAN_DOCUMENT_BYTES;
  limits.max_aggregate_bytes = GATE_MAN_AGGREGATE;
  limits.max_structural_nodes = GATE_MAN_STRUCTURAL;
  limits.max_roots_per_document = W_SEED_MANIFEST_MAX_ROOTS_PER_DOCUMENT;
  limits.max_documents = GATE_MAN_CANDIDATES;
  limits.max_scalar_source_bytes = GATE_MAN_DOCUMENT_BYTES;
  limits.max_number_digits = GATE_MAN_DOCUMENT_BYTES;
  limits.max_decoded_scalar_bytes = GATE_MAN_DOCUMENT_BYTES;
  limits.max_canonical_bytes = GATE_MAN_CANONICAL;
  limits.max_work_units = UINT64_C(1000000);
  return limits;
}

static bool gate_session_begin(gate_session *session,
                               const gate_fixture *fixture) {
  if (session == NULL || fixture == NULL ||
      !w_seed_owner_guard_linux_init(&session->context, fixture->base_fd) ||
      !session->context.native_supported ||
      !w_seed_owner_guard_linux_backend(&session->context,
                                        &session->owner_backend))
    return false;
  session->owner_input = (w_seed_owner_guard_input){
      {(const uint8_t *)fixture->source_relative,
       strlen(fixture->source_relative)},
      W_SEED_OWNER_GUARD_MAX_LEVELS,
      {session->staged_observations, W_SEED_OWNER_GUARD_MAX_LEVELS,
       session->revalidation_observations, W_SEED_OWNER_GUARD_MAX_LEVELS,
       session->candidates, W_SEED_OWNER_GUARD_MAX_LEVELS},
      session->owner_backend,
      sizeof(session->context)};
  w_seed_owner_guard_result owner_result;
  (void)memset(&session->guard, 0, sizeof(session->guard));
  (void)memset(&owner_result, 0, sizeof(owner_result));
  if (w_seed_owner_guard_begin(&session->owner_input, &session->guard,
                               &owner_result) != W_SEED_OWNER_GUARD_OK ||
      session->guard.candidate_count != GATE_MAN_CANDIDATES ||
      session->guard.lifecycle != W_SEED_OWNER_GUARD_LIVE_OBSERVED)
    return false;

  if (!w_seed_manifest_linux_backend(&session->guard, &session->context,
                                     &session->native_backend))
    return false;
  session->manifest_backend = session->native_backend;
  session->manifest_backend.owner = &session->manifest_backend;
  for (size_t index = 0u; index < GATE_MAN_CANDIDATES; index += 1u)
    session->read_slots[index] = (w_seed_manifest_read_slot){
        session->first_bytes[index], GATE_MAN_DOCUMENT_BYTES,
        session->second_bytes[index], GATE_MAN_DOCUMENT_BYTES};
  const w_seed_manifest_output staged = {
      session->staged_documents, GATE_MAN_CANDIDATES, session->staged_roots,
      4u, session->staged_nodes, GATE_MAN_NODES, session->staged_fields,
      GATE_MAN_FIELDS, session->staged_edges, GATE_MAN_EDGES,
      session->staged_canonical, GATE_MAN_CANONICAL};
  const w_seed_manifest_output published = {
      session->published_documents, GATE_MAN_CANDIDATES,
      session->published_roots, 4u, session->published_nodes, GATE_MAN_NODES,
      session->published_fields, GATE_MAN_FIELDS, session->published_edges,
      GATE_MAN_EDGES, session->published_canonical, GATE_MAN_CANONICAL};
  session->guarded_input = (w_seed_manifest_guarded_input){
      &session->guard,
      &session->manifest_backend,
      gate_manifest_limits(),
      {session->read_slots, GATE_MAN_CANDIDATES, session->staged_sources,
       GATE_MAN_CANDIDATES,
       {session->name_slots, GATE_MAN_STRUCTURAL, session->scratch_bytes,
        sizeof(session->scratch_bytes)},
       staged,
       published}};
  (void)memset(&session->program, 0, sizeof(session->program));
  session->result = w_seed_manifest_guarded_run(&session->guarded_input,
                                                 &session->program);
  if (session->result.status != W_SEED_MANIFEST_OK ||
      session->result.phase != W_SEED_MANIFEST_PHASE_COMMIT ||
      !session->result.owner_guard_revalidate_called ||
      session->program.document_count != GATE_MAN_CANDIDATES ||
      session->guard.lifecycle != W_SEED_OWNER_GUARD_LIVE_RECONFIRMED ||
      session->guard.generation == 0u)
    return false;
  return w_seed_source_binding_linux_link(&session->context,
                                          &session->link);
}

static void gate_session_destroy(gate_session *session) {
  if (session == NULL) return;
  w_seed_owner_guard_destroy(&session->guard);
}

static w_seed_source_binding_input gate_binding_input(
    const gate_acq_fixture *acquisition, const gate_session *session) {
  return (w_seed_source_binding_input){
      {&acquisition->pipeline, &acquisition->result},
      {&session->guarded_input, &session->program, &session->result,
       &session->guarded_input.storage.scratch},
      &session->link};
}

static w_seed_source_binding_result gate_compose_once(
    const gate_acq_fixture *acquisition, const gate_session *session,
    w_seed_source_binding *binding) {
  w_seed_source_binding_result result;
  {
    w_seed_source_binding_input input =
        gate_binding_input(acquisition, session);
    result = w_seed_source_binding_compose(&input, binding);
  }
  return result;
}

static bool gate_binding_evidence_equal(const w_seed_source_binding *left,
                                        const w_seed_source_binding *right) {
  if (left == NULL || right == NULL || left->lifecycle != right->lifecycle ||
      left->guard_generation != right->guard_generation ||
      memcmp(left->schema, right->schema, sizeof(left->schema)) != 0)
    return false;
  return memcmp(left->acquisition_root_facts_digest,
                right->acquisition_root_facts_digest,
                sizeof(left->acquisition_root_facts_digest)) == 0 &&
         memcmp(left->acquisition_source_digest,
                right->acquisition_source_digest,
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
                sizeof(left->binding_digest)) == 0;
}

static bool gate_expect_status(const w_seed_source_binding_input *input,
                               w_seed_source_binding_status expected) {
  if (input == NULL) return false;
  w_seed_source_binding binding;
  (void)memset(&binding, 0xA5, sizeof(binding));
  uint8_t before[sizeof(binding)];
  (void)memcpy(before, &binding, sizeof(before));
  const w_seed_source_binding_result result =
      w_seed_source_binding_compose(input, &binding);
  return result.status == expected &&
         memcmp(before, &binding, sizeof(binding)) == 0;
}

static bool gate_success_and_determinism(const gate_acq_fixture *acquisition,
                                         const gate_session *session) {
  w_seed_source_binding first;
  w_seed_source_binding second;
  (void)memset(&first, 0, sizeof(first));
  (void)memset(&second, 0, sizeof(second));
  const w_seed_source_binding_result first_result =
      gate_compose_once(acquisition, session, &first);
  GATE_CHECK(first_result.status == W_SEED_SOURCE_BINDING_OK);
  GATE_CHECK(first_result.lifecycle == W_SEED_SOURCE_BINDING_BOUND);
  GATE_CHECK(w_seed_source_binding_verify(&first));
  const w_seed_source_binding_result second_result =
      gate_compose_once(acquisition, session, &second);
  GATE_CHECK(second_result.status == W_SEED_SOURCE_BINDING_OK);
  GATE_CHECK(w_seed_source_binding_verify(&second));
  GATE_CHECK(gate_binding_evidence_equal(&first, &second));
  GATE_CHECK(first.guard_generation == session->guard.generation);
  return true;
}

static bool gate_forged_provider_and_tokens(gate_acq_fixture *acquisition,
                                             const gate_session *session) {
  w_seed_source_binding_input input =
      gate_binding_input(acquisition, session);
  const char saved_provider = acquisition->provider[0u][0u];
  acquisition->provider[0u][0u] = 'x';
  GATE_CHECK(gate_expect_status(&input, W_SEED_SOURCE_BINDING_UNSUPPORTED));
  acquisition->provider[0u][0u] = saved_provider;

  char *root_token = acquisition->root_token[0u];
  const char saved_root = root_token[1u];
  root_token[1u] = saved_root == 'f' ? 'e' : 'f';
  GATE_CHECK(gate_expect_status(&input, W_SEED_SOURCE_BINDING_MISMATCH));
  root_token[1u] = saved_root;

  char *owner_token = acquisition->owner_token[0u];
  const char saved_owner = owner_token[1u];
  owner_token[1u] = saved_owner == 'f' ? 'e' : 'f';
  GATE_CHECK(gate_expect_status(&input, W_SEED_SOURCE_BINDING_MISMATCH));
  owner_token[1u] = saved_owner;

  char *canonical_token = acquisition->canonical_token[0u];
  const char saved_canonical = canonical_token[1u];
  canonical_token[1u] = saved_canonical == 'f' ? 'e' : 'f';
  GATE_CHECK(gate_expect_status(&input, W_SEED_SOURCE_BINDING_MISMATCH));
  canonical_token[1u] = saved_canonical;
  return true;
}

static bool gate_forged_manifest(const gate_acq_fixture *acquisition,
                                 const gate_session *session) {
  w_seed_source_binding_input input =
      gate_binding_input(acquisition, session);
  w_seed_manifest_document forged_documents[GATE_MAN_CANDIDATES];
  (void)memcpy(forged_documents, session->program.documents,
               sizeof(forged_documents));
  forged_documents[0u].candidate.candidate_index = 1u;
  w_seed_manifest_program forged_program = session->program;
  forged_program.documents = forged_documents;
  input.manifest.program = &forged_program;
  GATE_CHECK(gate_expect_status(&input, W_SEED_SOURCE_BINDING_MUTATED));

  w_seed_manifest_result forged_result = session->result;
  forged_result.receipt_digest[0u] ^= 0x01u;
  input = gate_binding_input(acquisition, session);
  input.manifest.result = &forged_result;
  GATE_CHECK(gate_expect_status(&input, W_SEED_SOURCE_BINDING_MUTATED));
  return true;
}

static bool gate_run(gate_fixture *fixture) {
  w_seed_ephemeral_provider_linux_context provider_context;
  w_seed_ephemeral_provider_backend provider_backend;
  GATE_CHECK(fixture != NULL);
  GATE_CHECK(w_seed_ephemeral_provider_linux_init(&provider_context,
                                                   fixture->base_fd));
  GATE_CHECK(provider_context.openat2_supported);
  GATE_CHECK(w_seed_ephemeral_provider_linux_backend(&provider_context,
                                                      &provider_backend));
  GATE_CHECK(gate_acq_init(&gate_original_acq, fixture->source_relative,
                           "source.w", &provider_backend));
  GATE_CHECK(gate_acq_run(&gate_original_acq));
  (void)memset(&gate_live_session, 0, sizeof(gate_live_session));
  GATE_CHECK(gate_session_begin(&gate_live_session, fixture));
  GATE_CHECK(gate_success_and_determinism(&gate_original_acq,
                                          &gate_live_session));

  GATE_CHECK(gate_acq_init(&gate_alternate_acq, fixture->same_parent_relative,
                           "source.w", &provider_backend));
  GATE_CHECK(gate_acq_run(&gate_alternate_acq));
  w_seed_source_binding_input input =
      gate_binding_input(&gate_alternate_acq, &gate_live_session);
  GATE_CHECK(gate_expect_status(&input, W_SEED_SOURCE_BINDING_MISMATCH));
  w_seed_acquisition_storage_destroy(&gate_alternate_acq.storage);

  GATE_CHECK(gate_acq_init(&gate_alternate_acq,
                           fixture->other_parent_relative, "source.w",
                           &provider_backend));
  GATE_CHECK(gate_acq_run(&gate_alternate_acq));
  input = gate_binding_input(&gate_alternate_acq, &gate_live_session);
  GATE_CHECK(gate_expect_status(&input, W_SEED_SOURCE_BINDING_MISMATCH));
  w_seed_acquisition_storage_destroy(&gate_alternate_acq.storage);

  GATE_CHECK(gate_forged_provider_and_tokens(&gate_original_acq,
                                              &gate_live_session));
  GATE_CHECK(gate_forged_manifest(&gate_original_acq, &gate_live_session));

  w_seed_acquisition_pipeline_result forged_acquisition =
      gate_original_acq.result;
  forged_acquisition.status = W_SEED_ACQUISITION_PIPELINE_INVALID;
  input = gate_binding_input(&gate_original_acq, &gate_live_session);
  input.acquisition.result = &forged_acquisition;
  GATE_CHECK(gate_expect_status(&input, W_SEED_SOURCE_BINDING_INVALID));

  const uint64_t active_generation =
      gate_live_session.context.active_generation;
  gate_live_session.context.active_generation = active_generation + 1u;
  input = gate_binding_input(&gate_original_acq, &gate_live_session);
  GATE_CHECK(gate_expect_status(&input, W_SEED_SOURCE_BINDING_MUTATED));
  gate_live_session.context.active_generation = active_generation;

  w_seed_owner_guard_destroy(&gate_live_session.guard);
  input = gate_binding_input(&gate_original_acq, &gate_live_session);
  GATE_CHECK(gate_expect_status(&input, W_SEED_SOURCE_BINDING_MUTATED));
  return true;
}

int main(void) {
  gate_fixture fixture;
  (void)memset(&fixture, 0, sizeof(fixture));
  fixture.base_fd = -1;
  (void)memset(&gate_original_acq, 0, sizeof(gate_original_acq));
  (void)memset(&gate_alternate_acq, 0, sizeof(gate_alternate_acq));
  (void)memset(&gate_live_session, 0, sizeof(gate_live_session));
  if (!gate_fixture_paths(&fixture) || !gate_fixture_create(&fixture) ||
      !gate_run(&fixture)) {
    gate_session_destroy(&gate_live_session);
    w_seed_acquisition_storage_destroy(&gate_original_acq.storage);
    w_seed_acquisition_storage_destroy(&gate_alternate_acq.storage);
    gate_fixture_destroy(&fixture);
    return 1;
  }
  gate_session_destroy(&gate_live_session);
  w_seed_acquisition_storage_destroy(&gate_original_acq.storage);
  w_seed_acquisition_storage_destroy(&gate_alternate_acq.storage);
  gate_fixture_destroy(&fixture);
  (void)puts("w_seed_source_binding_linux_gate: ok");
  return 0;
}

#endif
