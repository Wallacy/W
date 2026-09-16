#ifndef W_SEED_PARALLEL_PANIC_BINDING1_H
#define W_SEED_PARALLEL_PANIC_BINDING1_H

#include "w_seed_hir0.h"
#include "w_seed_parallel_invocation1.h"
#include "w_seed_parallel_local_provider1.h"
#include "w_seed_parallel_platform1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PARPANIC1 is a private compiler-lifecycle bridge. It consumes a verified
 * source-selected PARINV1 plan and one local PLATFORM1 provider authority.
 * It is not PanicEvent, a public Task ABI, a runtime, or cleanup evidence. */
#define W_SEED_PARALLEL_PANIC_BINDING1_SCHEMA_VERSION \
  "w-seed-parallel-panic-binding1-1"

typedef enum {
  W_SEED_PARALLEL_PANIC_BINDING1_OK = 0,
  W_SEED_PARALLEL_PANIC_BINDING1_INVALID,
  W_SEED_PARALLEL_PANIC_BINDING1_UNSUPPORTED,
  W_SEED_PARALLEL_PANIC_BINDING1_CAPACITY,
  W_SEED_PARALLEL_PANIC_BINDING1_ALIAS,
  W_SEED_PARALLEL_PANIC_BINDING1_HIR,
  W_SEED_PARALLEL_PANIC_BINDING1_AUTHORITY,
  W_SEED_PARALLEL_PANIC_BINDING1_PROVIDER_FAILURE,
  W_SEED_PARALLEL_PANIC_BINDING1_TASK_FAILURE,
  W_SEED_PARALLEL_PANIC_BINDING1_NO_PANIC,
  W_SEED_PARALLEL_PANIC_BINDING1_FORGERY,
} w_seed_parallel_panic_binding1_status;

typedef struct {
  size_t source_id_bytes;
  size_t module_id_bytes;
  size_t message_bytes;
} w_seed_parallel_panic_binding1_counts;

/* All pointers in this input are immutable producer records. The generation
 * is a caller-owned provenance fact and is never part of semantic identity. */
typedef struct {
  const w_seed_hir0_program *hir_program;
  const w_seed_hir0_result *hir_result;
  const w_seed_parallel_selection1_program *selection;
  const w_seed_parallel_selection1_result *selection_result;
  const w_seed_parallel_invocation1_program *invocation;
  const w_seed_parallel_invocation1_result *invocation_result;
  const w_seed_parallel_local_provider1_authority *provider_authority;
  uint32_t provider_capacity;
  uint32_t generation;
} w_seed_parallel_panic_binding1_input;

/* PLATFORM1 completion records and its receipt are caller-owned physical
 * scratch. They may change on a failed provider call. The published signal,
 * copied bytes, and result remain unchanged on every failed PARPANIC1 call. */
typedef struct {
  w_seed_parallel_platform1_completion *completions;
  size_t completion_capacity;
  w_seed_parallel_platform1_receipt *receipt;
  w_seed_parallel_provider0_kind *provider_kind;
} w_seed_parallel_panic_binding1_workspace;

/* This record is a private panic signal, not a semantic value result. Its
 * byte pointers refer only to the caller-owned buffers in the output record.
 * The copied bytes and identity fields remain usable after HIR/frontend
 * teardown. The producer digests permit a live producer set to be checked. */
typedef struct {
  uint32_t source_task_index;
  uint32_t lexical_index;
  uint32_t call_index;
  uint32_t function_index;
  uint32_t module_index;
  uint32_t source_expression;
  uint32_t terminator_index;
  uint32_t message_value_index;
  w_seed_hir0_panic_code panic_code;
  bool semantic_value_published;
  w_seed_span function_span;
  w_seed_span call_span;
  w_seed_span terminator_span;
  size_t source_length;
  uint8_t source_sha256[32];
  const uint8_t *source_id_bytes;
  size_t source_id_byte_count;
  const uint8_t *module_id_bytes;
  size_t module_id_byte_count;
  const uint8_t *message_bytes;
  size_t message_byte_count;
  w_seed_parallel_provider0_kind provider_kind;
  uint32_t provider_capacity;
  uint32_t generation;
  w_seed_parallel_local_provider1_receipt authority_receipt;
  w_seed_parallel_platform1_receipt platform_receipt;
  uint8_t hir_semantic_digest[32];
  uint8_t hir_provenance_digest[32];
  uint8_t selection_semantic_digest[32];
  uint8_t invocation_semantic_digest[32];
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
} w_seed_parallel_panic_binding1_signal;

typedef struct {
  uint8_t *source_id_bytes;
  size_t source_id_capacity;
  uint8_t *module_id_bytes;
  size_t module_id_capacity;
  uint8_t *message_bytes;
  size_t message_capacity;
  w_seed_parallel_panic_binding1_signal *signal;
} w_seed_parallel_panic_binding1_output;

typedef struct {
  w_seed_parallel_panic_binding1_status status;
  w_seed_parallel_panic_binding1_counts required;
  w_seed_parallel_panic_binding1_counts written;
  char schema[sizeof(W_SEED_PARALLEL_PANIC_BINDING1_SCHEMA_VERSION)];
  uint32_t task_count;
  uint32_t panic_count;
  uint32_t source_task_index;
  uint32_t generation;
  uint8_t hir_semantic_digest[32];
  uint8_t hir_provenance_digest[32];
  uint8_t selection_semantic_digest[32];
  uint8_t invocation_semantic_digest[32];
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
} w_seed_parallel_panic_binding1_result;

w_seed_parallel_panic_binding1_status
w_seed_parallel_panic_binding1_measure(
    const w_seed_parallel_panic_binding1_input *input,
    w_seed_parallel_panic_binding1_counts *counts,
    w_seed_parallel_panic_binding1_result *result);

w_seed_parallel_panic_binding1_status w_seed_parallel_panic_binding1_run(
    const w_seed_parallel_panic_binding1_input *input,
    const w_seed_parallel_panic_binding1_workspace *workspace,
    const w_seed_parallel_panic_binding1_output *output,
    w_seed_parallel_panic_binding1_result *result);

/* Verify the physical completion workspace and the published copied signal.
 * Verification performs no provider call and does not mutate any argument. */
bool w_seed_parallel_panic_binding1_verify(
    const w_seed_parallel_panic_binding1_input *input,
    const w_seed_parallel_panic_binding1_workspace *workspace,
    const w_seed_parallel_panic_binding1_output *output,
    const w_seed_parallel_panic_binding1_result *result);

#ifdef __cplusplus
}
#endif

#endif
