#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#endif

#include "w_seed_manifest.h"
#include "w_seed_manifest_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if !defined(__linux__)

int main(void) {
  (void)fprintf(stderr, "w_seed_manifest_linux_gate: Linux required\n");
  return 1;
}

#else

enum {
  GATE_PATH_CAPACITY = W_SEED_OWNER_GUARD_MAX_PATH_BYTES + 1u,
  GATE_DOCUMENT_CAPACITY = 512u,
  GATE_AGGREGATE_CAPACITY = 2048u,
  GATE_STRUCTURAL_CAPACITY = 128u,
  GATE_CANONICAL_CAPACITY = 512u,
  GATE_CANDIDATE_COUNT = 2u,
};

typedef struct {
  char root[GATE_PATH_CAPACITY];
  char a[GATE_PATH_CAPACITY];
  char b[GATE_PATH_CAPACITY];
  char source[GATE_PATH_CAPACITY];
  char nested[GATE_PATH_CAPACITY];
  char workspace[GATE_PATH_CAPACITY];
  char replacement[GATE_PATH_CAPACITY];
  char anchor[GATE_PATH_CAPACITY];
  int base_fd;
} gate_fixture;

typedef struct {
  w_seed_owner_guard_linux_context context;
  w_seed_owner_guard_backend backend;
  w_seed_owner_guard_observation staged[W_SEED_OWNER_GUARD_MAX_LEVELS];
  w_seed_owner_guard_observation revalidation[W_SEED_OWNER_GUARD_MAX_LEVELS];
  w_seed_owner_guard_candidate_ref candidates[W_SEED_OWNER_GUARD_MAX_LEVELS];
  w_seed_owner_guard_input input;
  w_seed_owner_guard guard;
} gate_owner_session;

typedef struct {
  size_t document_count;
  size_t candidate_index[GATE_CANDIDATE_COUNT];
} gate_success_summary;

typedef enum {
  GATE_MUTATION_NONE = 0,
  GATE_MUTATION_BYTES,
  GATE_MUTATION_REPLACEMENT,
  GATE_MUTATION_BINDING,
} gate_mutation;

static w_seed_manifest_name_slot
    gate_name_slots[GATE_STRUCTURAL_CAPACITY];
static uint8_t gate_scratch_bytes[GATE_DOCUMENT_CAPACITY +
                                  W_SEED_MANIFEST_SCALAR_SCRATCH_OVERHEAD];
static uint8_t gate_first_bytes[GATE_CANDIDATE_COUNT][GATE_DOCUMENT_CAPACITY];
static uint8_t gate_second_bytes[GATE_CANDIDATE_COUNT][GATE_DOCUMENT_CAPACITY];
static w_seed_manifest_read_slot gate_read_slots[GATE_CANDIDATE_COUNT];
static w_seed_manifest_source_input
    gate_staged_sources[GATE_CANDIDATE_COUNT];
static w_seed_manifest_document gate_staged_documents[GATE_CANDIDATE_COUNT];
static w_seed_manifest_root gate_staged_roots[4u];
static w_seed_manifest_node gate_staged_nodes[16u];
static w_seed_manifest_field gate_staged_fields[8u];
static w_seed_manifest_edge gate_staged_edges[8u];
static uint8_t gate_staged_canonical[GATE_CANONICAL_CAPACITY];
static w_seed_manifest_document
    gate_published_documents[GATE_CANDIDATE_COUNT];
static w_seed_manifest_root gate_published_roots[4u];
static w_seed_manifest_node gate_published_nodes[16u];
static w_seed_manifest_field gate_published_fields[8u];
static w_seed_manifest_edge gate_published_edges[8u];
static uint8_t gate_published_canonical[GATE_CANONICAL_CAPACITY];

static gate_mutation gate_mutation_mode;
static size_t gate_read_calls;
static bool gate_mutation_done;
static char gate_nested_path[GATE_PATH_CAPACITY];
static char gate_replacement_path[GATE_PATH_CAPACITY];
static w_seed_manifest_backend gate_native_backend;

static bool gate_fail(const char *message) {
  if (message == NULL) message = "unknown failure";
  (void)fprintf(stderr, "w_seed_manifest_linux_gate: %s\n", message);
  return false;
}

static bool gate_path_join(char *destination, const char *directory,
                           const char *leaf) {
  if (destination == NULL || directory == NULL || leaf == NULL) return false;
  const size_t directory_length = strlen(directory);
  const size_t leaf_length = strlen(leaf);
  if (directory_length == 0u || leaf_length == 0u ||
      directory_length >= (size_t)GATE_PATH_CAPACITY ||
      leaf_length > (size_t)GATE_PATH_CAPACITY - directory_length - 2u)
    return false;
  (void)memcpy(destination, directory, directory_length);
  destination[directory_length] = '/';
  (void)memcpy(destination + directory_length + 1u, leaf, leaf_length);
  destination[directory_length + leaf_length + 1u] = '\0';
  return true;
}

static bool gate_unlink_if_present(const char *path) {
  if (path == NULL) return false;
  if (unlink(path) == 0) return true;
  return errno == ENOENT;
}

static bool gate_write_file(const char *path, const uint8_t *bytes,
                            size_t length) {
  if (path == NULL || (length != 0u && bytes == NULL)) return false;
  const int file = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (file < 0) return false;
  size_t offset = 0u;
  bool success = true;
  while (offset < length) {
    const ssize_t written = write(file, bytes + offset, length - offset);
    if (written <= (ssize_t)0) {
      success = false;
      break;
    }
    offset += (size_t)written;
  }
  if (close(file) != 0) success = false;
  return success && offset == length;
}

static bool gate_make_directory(const char *path) {
  if (path == NULL) return false;
  if (mkdir(path, 0700) == 0) return true;
  if (errno != EEXIST) return false;
  const int directory = open(path, O_PATH | O_DIRECTORY | O_CLOEXEC);
  if (directory < 0) return false;
  return close(directory) == 0;
}

static bool gate_fixture_paths(gate_fixture *fixture, const char *root) {
  if (fixture == NULL || root == NULL || root[0] == '\0' ||
      strlen(root) >= (size_t)GATE_PATH_CAPACITY)
    return false;
  (void)memset(fixture, 0, sizeof(*fixture));
  if (!gate_path_join(fixture->a, root, "a") ||
      !gate_path_join(fixture->b, fixture->a, "b") ||
      !gate_path_join(fixture->source, fixture->b, "source.w") ||
      !gate_path_join(fixture->nested, fixture->b, "build.w") ||
      !gate_path_join(fixture->workspace, root, "build.w") ||
      !gate_path_join(fixture->replacement, fixture->b, "replacement.w") ||
      !gate_path_join(fixture->anchor, fixture->b, "build.w.anchor"))
    return false;
  (void)memcpy(fixture->root, root, strlen(root) + 1u);
  return true;
}

static bool gate_fixture_stable(gate_fixture *fixture) {
  static const uint8_t source[] = "source\n";
  static const uint8_t nested[] = "package { value: 1 }\n";
  static const uint8_t workspace[] = "workspace { value: 2 }\n";
  if (fixture == NULL || !gate_make_directory(fixture->root) ||
      !gate_make_directory(fixture->a) || !gate_make_directory(fixture->b) ||
      !gate_write_file(fixture->source, source, sizeof(source) - 1u) ||
      !gate_write_file(fixture->nested, nested, sizeof(nested) - 1u) ||
      !gate_write_file(fixture->workspace, workspace, sizeof(workspace) - 1u) ||
      !gate_unlink_if_present(fixture->replacement) ||
      !gate_unlink_if_present(fixture->anchor) ||
      link(fixture->nested, fixture->anchor) != 0)
    return false;
  return true;
}

static bool gate_fixture_restore_anchor(const gate_fixture *fixture) {
  if (fixture == NULL || !gate_unlink_if_present(fixture->nested))
    return false;
  return link(fixture->anchor, fixture->nested) == 0;
}

static void gate_fixture_close(gate_fixture *fixture) {
  if (fixture == NULL) return;
  if (fixture->base_fd >= 0) {
    (void)close(fixture->base_fd);
    fixture->base_fd = -1;
  }
}

static bool gate_fixture_open(gate_fixture *fixture) {
  if (fixture == NULL) return false;
  fixture->base_fd = open(fixture->root, O_PATH | O_DIRECTORY | O_CLOEXEC);
  return fixture->base_fd >= 0;
}

static w_seed_manifest_limits gate_limits(void) {
  w_seed_manifest_limits limits = w_seed_manifest_default_limits();
  limits.max_document_bytes = GATE_DOCUMENT_CAPACITY;
  limits.max_aggregate_bytes = GATE_AGGREGATE_CAPACITY;
  limits.max_structural_nodes = GATE_STRUCTURAL_CAPACITY;
  limits.max_roots_per_document = W_SEED_MANIFEST_MAX_ROOTS_PER_DOCUMENT;
  limits.max_documents = GATE_CANDIDATE_COUNT;
  limits.max_scalar_source_bytes = GATE_DOCUMENT_CAPACITY;
  limits.max_number_digits = GATE_DOCUMENT_CAPACITY;
  limits.max_decoded_scalar_bytes = GATE_DOCUMENT_CAPACITY;
  limits.max_canonical_bytes = GATE_CANONICAL_CAPACITY;
  limits.max_work_units = UINT64_C(1000000);
  return limits;
}

static bool gate_owner_begin(gate_owner_session *session,
                             const gate_fixture *fixture) {
  static const uint8_t source_path[] = "a/b/source.w";
  if (session == NULL || fixture == NULL ||
      !w_seed_owner_guard_linux_init(&session->context, fixture->base_fd)) {
    return false;
  }
  if (!session->context.native_supported ||
      !w_seed_owner_guard_linux_backend(&session->context,
                                        &session->backend)) {
    return false;
  }
  session->input = (w_seed_owner_guard_input){
      {source_path, sizeof(source_path) - 1u},
      W_SEED_OWNER_GUARD_MAX_LEVELS,
      {session->staged, W_SEED_OWNER_GUARD_MAX_LEVELS,
       session->revalidation, W_SEED_OWNER_GUARD_MAX_LEVELS,
       session->candidates, W_SEED_OWNER_GUARD_MAX_LEVELS},
      session->backend,
      sizeof(session->context)};
  (void)memset(&session->guard, 0, sizeof(session->guard));
  w_seed_owner_guard_result result;
  (void)memset(&result, 0, sizeof(result));
  const w_seed_owner_guard_status status =
      w_seed_owner_guard_begin(&session->input, &session->guard, &result);
  if (status != W_SEED_OWNER_GUARD_OK ||
      session->guard.candidate_count != GATE_CANDIDATE_COUNT)
    return false;
  w_seed_owner_guard_view view;
  (void)memset(&view, 0, sizeof(view));
  if (!w_seed_owner_guard_get_view(&session->guard, &view) ||
      view.candidate_count != GATE_CANDIDATE_COUNT ||
      view.candidates[0].candidate_index != 0u ||
      view.candidates[1].candidate_index != 1u ||
      view.candidates[0].directory_ordinal != 0u ||
      view.candidates[1].directory_ordinal != 2u)
    return false;
  return true;
}

static bool gate_mutate_after_first_wave(void) {
  static const uint8_t changed[] = "package { value: 3 }\n";
  static const uint8_t replacement[] = "package { value: 4 }\n";
  if (gate_mutation_done || gate_mutation_mode == GATE_MUTATION_NONE)
    return true;
  bool success = true;
  if (gate_mutation_mode == GATE_MUTATION_BYTES)
    success = gate_write_file(gate_nested_path, changed, sizeof(changed) - 1u);
  else if (gate_mutation_mode == GATE_MUTATION_REPLACEMENT)
    success = gate_write_file(gate_replacement_path, replacement,
                              sizeof(replacement) - 1u) &&
              rename(gate_replacement_path, gate_nested_path) == 0;
  gate_mutation_done = success;
  return success;
}

static w_seed_manifest_backend_result gate_read_candidate(
    const void *context, uint64_t generation,
    w_seed_owner_guard_candidate_ref candidate, uint8_t *bytes,
    size_t byte_capacity, size_t byte_limit) {
  w_seed_manifest_backend_result result = gate_native_backend.read_candidate(
      gate_native_backend.context, generation, candidate, bytes, byte_capacity,
      byte_limit);
  if (result.status == W_SEED_MANIFEST_BACKEND_OK) {
    gate_read_calls += 1u;
    if (gate_read_calls == GATE_CANDIDATE_COUNT &&
        gate_mutation_mode != GATE_MUTATION_BINDING &&
        !gate_mutate_after_first_wave()) {
      result.status = W_SEED_MANIFEST_BACKEND_FAULT;
      result.phase = W_SEED_MANIFEST_BACKEND_PHASE_CLOSE;
      result.byte_count = 0u;
      result.required_byte_capacity = 0u;
      (void)memset(result.source_digest, 0, sizeof(result.source_digest));
      (void)memset(result.context_binding, 0, sizeof(result.context_binding));
      (void)memset(result.candidate_binding, 0,
                   sizeof(result.candidate_binding));
    }
    if (gate_mutation_mode == GATE_MUTATION_BINDING &&
        gate_read_calls > GATE_CANDIDATE_COUNT)
      result.candidate_binding[0] ^= 0x01u;
  }
  (void)context;
  return result;
}

static void gate_clear_manifest_storage(void) {
  (void)memset(gate_first_bytes, 0, sizeof(gate_first_bytes));
  (void)memset(gate_second_bytes, 0, sizeof(gate_second_bytes));
  (void)memset(gate_staged_sources, 0, sizeof(gate_staged_sources));
  (void)memset(gate_staged_documents, 0, sizeof(gate_staged_documents));
  (void)memset(gate_staged_roots, 0, sizeof(gate_staged_roots));
  (void)memset(gate_staged_nodes, 0, sizeof(gate_staged_nodes));
  (void)memset(gate_staged_fields, 0, sizeof(gate_staged_fields));
  (void)memset(gate_staged_edges, 0, sizeof(gate_staged_edges));
  (void)memset(gate_staged_canonical, 0, sizeof(gate_staged_canonical));
  (void)memset(gate_published_documents, 0xa5,
               sizeof(gate_published_documents));
  (void)memset(gate_published_roots, 0xa5, sizeof(gate_published_roots));
  (void)memset(gate_published_nodes, 0xa5, sizeof(gate_published_nodes));
  (void)memset(gate_published_fields, 0xa5, sizeof(gate_published_fields));
  (void)memset(gate_published_edges, 0xa5, sizeof(gate_published_edges));
  (void)memset(gate_published_canonical, 0xa5,
               sizeof(gate_published_canonical));
  (void)memset(gate_read_slots, 0, sizeof(gate_read_slots));
  for (size_t index = 0u; index < GATE_CANDIDATE_COUNT; index += 1u)
    gate_read_slots[index] = (w_seed_manifest_read_slot){
        gate_first_bytes[index], GATE_DOCUMENT_CAPACITY,
        gate_second_bytes[index], GATE_DOCUMENT_CAPACITY};
}

static w_seed_manifest_guarded_input gate_manifest_input(
    w_seed_owner_guard *guard, const w_seed_manifest_backend *backend) {
  const w_seed_manifest_output staged = {
      gate_staged_documents, GATE_CANDIDATE_COUNT, gate_staged_roots, 4u,
      gate_staged_nodes, 16u, gate_staged_fields, 8u, gate_staged_edges, 8u,
      gate_staged_canonical, sizeof(gate_staged_canonical)};
  const w_seed_manifest_output published = {
      gate_published_documents, GATE_CANDIDATE_COUNT, gate_published_roots,
      4u, gate_published_nodes, 16u, gate_published_fields, 8u,
      gate_published_edges, 8u, gate_published_canonical,
      sizeof(gate_published_canonical)};
  return (w_seed_manifest_guarded_input){
      guard,
      backend,
      gate_limits(),
      {gate_read_slots, GATE_CANDIDATE_COUNT, gate_staged_sources,
       GATE_CANDIDATE_COUNT,
       {gate_name_slots, GATE_STRUCTURAL_CAPACITY, gate_scratch_bytes,
        sizeof(gate_scratch_bytes)},
       staged,
       published}};
}

static bool gate_published_unchanged(
    const w_seed_manifest_program *program,
    const uint8_t program_snapshot[sizeof(w_seed_manifest_program)],
    const uint8_t documents_snapshot[sizeof(gate_published_documents)],
    const uint8_t roots_snapshot[sizeof(gate_published_roots)],
    const uint8_t nodes_snapshot[sizeof(gate_published_nodes)],
    const uint8_t fields_snapshot[sizeof(gate_published_fields)],
    const uint8_t edges_snapshot[sizeof(gate_published_edges)],
    const uint8_t canonical_snapshot[sizeof(gate_published_canonical)]) {
  return program != NULL &&
         memcmp(program, program_snapshot, sizeof(*program)) == 0 &&
         memcmp(gate_published_documents, documents_snapshot,
                sizeof(gate_published_documents)) == 0 &&
         memcmp(gate_published_roots, roots_snapshot,
                sizeof(gate_published_roots)) == 0 &&
         memcmp(gate_published_nodes, nodes_snapshot,
                sizeof(gate_published_nodes)) == 0 &&
         memcmp(gate_published_fields, fields_snapshot,
                sizeof(gate_published_fields)) == 0 &&
         memcmp(gate_published_edges, edges_snapshot,
                sizeof(gate_published_edges)) == 0 &&
         memcmp(gate_published_canonical, canonical_snapshot,
                sizeof(gate_published_canonical)) == 0;
}

static bool gate_run_case(gate_fixture *fixture, gate_mutation mutation,
                          w_seed_manifest_result *success_result,
                          gate_success_summary *success_summary) {
  if (fixture == NULL ||
      !gate_fixture_stable(fixture) ||
      !gate_fixture_open(fixture))
    return false;
  gate_owner_session owner;
  (void)memset(&owner, 0, sizeof(owner));
  owner.context.base_dir_fd = -1;
  if (!gate_owner_begin(&owner, fixture)) {
    w_seed_owner_guard_destroy(&owner.guard);
    gate_fixture_close(fixture);
    return false;
  }
  w_seed_manifest_backend native_backend;
  if (!w_seed_manifest_linux_backend(&owner.guard, &owner.context,
                                     &native_backend)) {
    w_seed_owner_guard_destroy(&owner.guard);
    gate_fixture_close(fixture);
    return false;
  }
  gate_native_backend = native_backend;
  gate_mutation_mode = mutation;
  gate_read_calls = 0u;
  gate_mutation_done = false;
  (void)memcpy(gate_nested_path, fixture->nested, sizeof(gate_nested_path));
  (void)memcpy(gate_replacement_path, fixture->replacement,
               sizeof(gate_replacement_path));
  gate_clear_manifest_storage();

  w_seed_manifest_backend manifest_backend = native_backend;
  manifest_backend.owner = &manifest_backend;
  manifest_backend.read_candidate = gate_read_candidate;
  w_seed_manifest_guarded_input input =
      gate_manifest_input(&owner.guard, &manifest_backend);
  w_seed_manifest_program program;
  (void)memset(&program, 0, sizeof(program));
  const w_seed_manifest_program program_snapshot = program;
  uint8_t documents_snapshot[sizeof(gate_published_documents)];
  uint8_t roots_snapshot[sizeof(gate_published_roots)];
  uint8_t nodes_snapshot[sizeof(gate_published_nodes)];
  uint8_t fields_snapshot[sizeof(gate_published_fields)];
  uint8_t edges_snapshot[sizeof(gate_published_edges)];
  uint8_t canonical_snapshot[sizeof(gate_published_canonical)];
  (void)memcpy(documents_snapshot, gate_published_documents,
               sizeof(documents_snapshot));
  (void)memcpy(roots_snapshot, gate_published_roots, sizeof(roots_snapshot));
  (void)memcpy(nodes_snapshot, gate_published_nodes, sizeof(nodes_snapshot));
  (void)memcpy(fields_snapshot, gate_published_fields,
               sizeof(fields_snapshot));
  (void)memcpy(edges_snapshot, gate_published_edges, sizeof(edges_snapshot));
  (void)memcpy(canonical_snapshot, gate_published_canonical,
               sizeof(canonical_snapshot));

  const w_seed_manifest_result result =
      w_seed_manifest_guarded_run(&input, &program);
  bool success = true;
  if (mutation == GATE_MUTATION_NONE) {
    success = result.status == W_SEED_MANIFEST_OK &&
              result.phase == W_SEED_MANIFEST_PHASE_COMMIT &&
              result.owner_guard_revalidate_called &&
              gate_read_calls == 4u && program.document_count == 2u &&
              program.documents[0].candidate.candidate_index == 0u &&
              program.documents[1].candidate.candidate_index == 1u &&
              program.documents[0].source.data == gate_second_bytes[0] &&
              program.documents[1].source.data == gate_second_bytes[1];
    if (success_result != NULL) *success_result = result;
    if (success && success_summary != NULL) {
      /* Only scalar evidence crosses teardown; no program/source pointer escapes. */
      success_summary->document_count = program.document_count;
      success_summary->candidate_index[0] =
          program.documents[0].candidate.candidate_index;
      success_summary->candidate_index[1] =
          program.documents[1].candidate.candidate_index;
    }
  } else {
    success = result.status == W_SEED_MANIFEST_MUTATED &&
              result.owner_guard_revalidate_called &&
              gate_published_unchanged(&program,
                                       (const uint8_t *)&program_snapshot,
                                       documents_snapshot, roots_snapshot,
                                       nodes_snapshot, fields_snapshot,
                                       edges_snapshot, canonical_snapshot);
  }
  if (mutation == GATE_MUTATION_REPLACEMENT) {
    if (!gate_fixture_restore_anchor(fixture)) success = false;
  }
  w_seed_owner_guard_destroy(&owner.guard);
  gate_fixture_close(fixture);
  return success;
}

static bool gate_hex(const uint8_t digest[W_SEED_MANIFEST_DIGEST_BYTES],
                     char destination[W_SEED_MANIFEST_DIGEST_BYTES * 2u +
                                      1u]) {
  static const char digits[] = "0123456789abcdef";
  if (digest == NULL || destination == NULL) return false;
  for (size_t index = 0u; index < W_SEED_MANIFEST_DIGEST_BYTES; index += 1u) {
    destination[index * 2u] = digits[digest[index] >> 4u];
    destination[index * 2u + 1u] = digits[digest[index] & 0x0fu];
  }
  destination[W_SEED_MANIFEST_DIGEST_BYTES * 2u] = '\0';
  return true;
}

int main(int argc, char **argv) {
  if (argc != 2 || argv[1] == NULL) {
    (void)fprintf(stderr,
                 "w_seed_manifest_linux_gate: expected fixture directory\n");
    return 1;
  }
  gate_fixture fixture;
  if (!gate_fixture_paths(&fixture, argv[1]))
    return gate_fail("invalid fixture path") ? 0 : 1;
  fixture.base_fd = -1;
  w_seed_manifest_result success_result;
  gate_success_summary success_summary;
  (void)memset(&success_result, 0, sizeof(success_result));
  (void)memset(&success_summary, 0, sizeof(success_summary));
  if (!gate_run_case(&fixture, GATE_MUTATION_NONE, &success_result,
                     &success_summary)) {
    gate_fixture_close(&fixture);
    return gate_fail("stable wave check failed") ? 0 : 1;
  }
  if (!gate_run_case(&fixture, GATE_MUTATION_BYTES, NULL, NULL)) {
    gate_fixture_close(&fixture);
    return gate_fail("byte mutation check failed") ? 0 : 1;
  }
  if (!gate_run_case(&fixture, GATE_MUTATION_REPLACEMENT, NULL, NULL)) {
    gate_fixture_close(&fixture);
    return gate_fail("replacement check failed") ? 0 : 1;
  }
  if (!gate_run_case(&fixture, GATE_MUTATION_BINDING, NULL, NULL)) {
    gate_fixture_close(&fixture);
    return gate_fail("binding check failed") ? 0 : 1;
  }
  char semantic[W_SEED_MANIFEST_DIGEST_BYTES * 2u + 1u];
  char provenance[W_SEED_MANIFEST_DIGEST_BYTES * 2u + 1u];
  char receipt[W_SEED_MANIFEST_DIGEST_BYTES * 2u + 1u];
  if (!gate_hex(success_result.semantic_digest, semantic) ||
      !gate_hex(success_result.provenance_digest, provenance) ||
      !gate_hex(success_result.receipt_digest, receipt))
    return gate_fail("digest formatting failed") ? 0 : 1;
  (void)printf("w_seed_manifest_linux_gate: candidates=%zu order=%zu,%zu "
               "semantic=%s provenance=%s receipt=%s\n",
               success_summary.document_count,
               success_summary.candidate_index[0],
               success_summary.candidate_index[1],
               semantic, provenance, receipt);
  return 0;
}

#endif
