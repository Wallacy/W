#ifndef W_SEED_SOURCE_BINDING_H
#define W_SEED_SOURCE_BINDING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_acquisition.h"
#include "w_seed_manifest.h"

#ifdef __cplusplus
extern "C" {
#endif

/* BND0 is an internal borrowed composition boundary. It proves that one
 * complete ACQ0 result and one complete MAN0 guarded result refer to the same
 * source session before a later owner or workspace policy can run. It does
 * not validate a schema, select an owner, open a CLI, or create a snapshot. */
#define W_SEED_SOURCE_BINDING_SCHEMA_VERSION "w-seed-bnd0-1"
#define W_SEED_SOURCE_BINDING_DOMAIN_TAG "w.seed.bnd0.binding/1"
#define W_SEED_SOURCE_BINDING_DIGEST_BYTES 32u

typedef enum {
  W_SEED_SOURCE_BINDING_OK = 0,
  W_SEED_SOURCE_BINDING_INVALID,
  W_SEED_SOURCE_BINDING_MISMATCH,
  W_SEED_SOURCE_BINDING_MUTATED,
  W_SEED_SOURCE_BINDING_BOUNDARY,
  W_SEED_SOURCE_BINDING_ALIAS,
  W_SEED_SOURCE_BINDING_UNSUPPORTED,
  W_SEED_SOURCE_BINDING_IO,
  W_SEED_SOURCE_BINDING_FAULT,
} w_seed_source_binding_status;

typedef enum {
  W_SEED_SOURCE_BINDING_PHASE_NONE = 0,
  W_SEED_SOURCE_BINDING_PHASE_VALIDATE,
  W_SEED_SOURCE_BINDING_PHASE_ACQUISITION,
  W_SEED_SOURCE_BINDING_PHASE_MANIFEST,
  W_SEED_SOURCE_BINDING_PHASE_LINK,
  W_SEED_SOURCE_BINDING_PHASE_COMMIT,
} w_seed_source_binding_phase;

typedef enum {
  W_SEED_SOURCE_BINDING_ZERO = 0,
  W_SEED_SOURCE_BINDING_ACQUISITION_VALIDATED,
  W_SEED_SOURCE_BINDING_MANIFEST_VALIDATED,
  W_SEED_SOURCE_BINDING_LINKED,
  W_SEED_SOURCE_BINDING_BOUND,
} w_seed_source_binding_lifecycle;

typedef enum {
  W_SEED_SOURCE_BINDING_LINK_OK = 0,
  W_SEED_SOURCE_BINDING_LINK_INVALID,
  W_SEED_SOURCE_BINDING_LINK_MISMATCH,
  W_SEED_SOURCE_BINDING_LINK_MUTATED,
  W_SEED_SOURCE_BINDING_LINK_BOUNDARY,
  W_SEED_SOURCE_BINDING_LINK_UNSUPPORTED,
  W_SEED_SOURCE_BINDING_LINK_IO,
} w_seed_source_binding_link_status;

typedef enum {
  W_SEED_SOURCE_BINDING_LINK_PHASE_NONE = 0,
  W_SEED_SOURCE_BINDING_LINK_PHASE_VALIDATE,
  W_SEED_SOURCE_BINDING_LINK_PHASE_PROVIDER,
  W_SEED_SOURCE_BINDING_LINK_PHASE_OWNER,
  W_SEED_SOURCE_BINDING_LINK_PHASE_COMPARE,
  W_SEED_SOURCE_BINDING_LINK_PHASE_COMMIT,
} w_seed_source_binding_link_phase;

/* A link sees the complete ACQ-owned facts array. It receives no schema or
 * owner-selection input. The link descriptor supplies its adapter-private
 * owner context and must be self-owned. */
typedef struct {
  const w_seed_ephemeral_graph_provider_facts *facts;
  size_t fact_count;
  size_t root_fact_index;
  uint64_t guard_generation;
} w_seed_source_binding_link_input;

typedef struct {
  w_seed_source_binding_link_status status;
  w_seed_source_binding_link_phase phase;
  uint8_t link_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES];
} w_seed_source_binding_link_result;

struct w_seed_source_binding_link;
typedef w_seed_source_binding_link_result (*w_seed_source_binding_link_fn)(
    const struct w_seed_source_binding_link *link,
    const w_seed_source_binding_link_input *input);

/* The context range is borrowed, synchronous, adapter-private storage. A
 * callback must not retain it, mutate it, allocate, or start asynchronous
 * work. The core trusts a configured C callback; this boundary does not claim
 * to defend against a hostile callback implementation. */
typedef struct w_seed_source_binding_link {
  const struct w_seed_source_binding_link *owner;
  const void *context;
  size_t context_size;
  w_seed_source_binding_link_fn compose;
} w_seed_source_binding_link;

/* These wrappers deliberately carry the full producer descriptors and
 * terminal reports. A digest or a pair of loose fact pointers is not enough
 * to cross BND0. The producer objects and all their declared backings must
 * stay immutable and live while a binding is borrowed. */
typedef struct {
  const w_seed_acquisition_pipeline_input *pipeline;
  const w_seed_acquisition_pipeline_result *result;
} w_seed_source_binding_acquisition;

typedef struct {
  const w_seed_manifest_guarded_input *input;
  const w_seed_manifest_program *program;
  const w_seed_manifest_result *result;
  /* w_seed_manifest_verify reparses into this explicit caller-owned scratch.
   * Its byte/name-slot backings may change during compose/verify. */
  const w_seed_manifest_scratch *verify_scratch;
} w_seed_source_binding_manifest;

typedef struct {
  w_seed_source_binding_acquisition acquisition;
  w_seed_source_binding_manifest manifest;
  const w_seed_source_binding_link *link;
} w_seed_source_binding_input;

typedef struct {
  w_seed_source_binding_status status;
  w_seed_source_binding_phase phase;
  w_seed_source_binding_lifecycle lifecycle;
  uint64_t guard_generation;
} w_seed_source_binding_result;

/* This fixed-size object is caller-owned and non-copyable by its owner-self
 * invariant. It borrows every pointer below; it owns no handles or backing and
 * is neither a lease nor a global snapshot. A successful compose publishes it
 * only after every check and digest has completed. Any non-OK compose leaves
 * the caller's object bitwise unchanged. */
typedef struct w_seed_source_binding {
  const struct w_seed_source_binding *owner;
  w_seed_source_binding_lifecycle lifecycle;
  const w_seed_acquisition_pipeline_input *acquisition_pipeline;
  const w_seed_acquisition_pipeline_result *acquisition_result;
  const w_seed_manifest_guarded_input *manifest_input;
  const w_seed_manifest_program *manifest_program;
  const w_seed_manifest_result *manifest_result;
  const w_seed_manifest_scratch *manifest_verify_scratch;
  const w_seed_source_binding_link *link;
  uint64_t guard_generation;
  uint8_t acquisition_root_facts_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES];
  uint8_t acquisition_source_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES];
  uint8_t manifest_receipt_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES];
  uint8_t manifest_context_binding_digest[
      W_SEED_SOURCE_BINDING_DIGEST_BYTES];
  uint8_t manifest_candidate_binding_digest[
      W_SEED_SOURCE_BINDING_DIGEST_BYTES];
  uint8_t link_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES];
  uint8_t binding_digest[W_SEED_SOURCE_BINDING_DIGEST_BYTES];
  char schema[sizeof(W_SEED_SOURCE_BINDING_SCHEMA_VERSION)];
} w_seed_source_binding;

/* Compose ACQ0 -> MAN0 -> adapter link with no heap and no partial output. */
w_seed_source_binding_result w_seed_source_binding_compose(
    const w_seed_source_binding_input *input, w_seed_source_binding *binding);

/* Recheck the borrowed relation without writing the binding. The explicit
 * MAN0 verify scratch is the only producer backing that may be reused. */
bool w_seed_source_binding_verify(const w_seed_source_binding *binding);

#ifdef __cplusplus
}
#endif

#endif
