#ifndef W_SEED_PARALLEL_TYPED_BINDING1_H
#define W_SEED_PARALLEL_TYPED_BINDING1_H

#include "w_seed_hir0.h"
#include "w_seed_parallel_local_provider1.h"
#include "w_seed_parallel_platform1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This is a separate measured transaction for one typed physical result.  It
 * does not widen PARPROV1 or PARLIFE1 and does not define a public Task or
 * Result ABI. */
#define W_SEED_PARALLEL_TYPED_BINDING1_SCHEMA_VERSION \
  "w-seed-parallel-typed-binding1-1"
/* The seed witness intentionally covers exactly two lexical children.  This
 * is not a source-language, scheduler, ABI, or target limit. */
#define W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT 2u
#define W_SEED_PARALLEL_TYPED_BINDING1_PROVIDER_PROFILE_BYTES 64u
#define W_SEED_PARALLEL_TYPED_BINDING1_PROVIDER_IDENTITY_BYTES 64u

/* The first physical witness is deliberately target-specific.  This enum is
 * evidence for a compiler component, not a product target selector. */
typedef enum {
  W_SEED_PARALLEL_TYPED_BINDING1_TARGET_NONE = 0,
  W_SEED_PARALLEL_TYPED_BINDING1_TARGET_WINDOWS_AMD64 = 1,
} w_seed_parallel_typed_binding1_target_kind;

/* PLATFORM1 execution is an integrity and provenance boundary. The local
 * compiler authority below is not a cryptographic provider-origin proof;
 * external attestation can add stronger provenance without changing semantic
 * task records. */
typedef enum {
  W_SEED_PARALLEL_TYPED_BINDING1_ASSURANCE_NONE = 0,
  W_SEED_PARALLEL_TYPED_BINDING1_ASSURANCE_EXECUTION_INTEGRITY = 1,
  W_SEED_PARALLEL_TYPED_BINDING1_ASSURANCE_STATIC_LOCAL_PROVIDER = 2,
} w_seed_parallel_typed_binding1_assurance_kind;

typedef enum {
  W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_NONE = 0,
  W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_SUCCESS,
  W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_ERROR,
} w_seed_parallel_typed_binding1_outcome_kind;

typedef enum {
  W_SEED_PARALLEL_TYPED_BINDING1_OK = 0,
  W_SEED_PARALLEL_TYPED_BINDING1_INVALID,
  W_SEED_PARALLEL_TYPED_BINDING1_UNSUPPORTED,
  W_SEED_PARALLEL_TYPED_BINDING1_CAPACITY,
  W_SEED_PARALLEL_TYPED_BINDING1_ALIAS,
  W_SEED_PARALLEL_TYPED_BINDING1_HIR,
  W_SEED_PARALLEL_TYPED_BINDING1_AUTHORITY,
  W_SEED_PARALLEL_TYPED_BINDING1_PROVIDER_FAILURE,
  W_SEED_PARALLEL_TYPED_BINDING1_TASK_FAILURE,
  W_SEED_PARALLEL_TYPED_BINDING1_CANCELED,
  W_SEED_PARALLEL_TYPED_BINDING1_PANIC,
  W_SEED_PARALLEL_TYPED_BINDING1_FORGERY,
} w_seed_parallel_typed_binding1_status;

/* These are fixed caller-owned bytes.  The implementation accepts only the
 * canonical Windows PLATFORM1 profile and identity below. */
#define W_SEED_PARALLEL_TYPED_BINDING1_WINDOWS_PROFILE \
  "windows-kernel32-platform1@1"
#define W_SEED_PARALLEL_TYPED_BINDING1_WINDOWS_IDENTITY \
  "w-seed-platform1-windows-kernel32"

typedef struct {
  char profile[W_SEED_PARALLEL_TYPED_BINDING1_PROVIDER_PROFILE_BYTES];
  char identity[W_SEED_PARALLEL_TYPED_BINDING1_PROVIDER_IDENTITY_BYTES];
  w_seed_parallel_typed_binding1_target_kind target;
} w_seed_parallel_typed_binding1_provider;

/* A task retains lexical identity and the HIR call which the provider body
 * represents.  The exact initial witness has two entries. */
typedef struct {
  uint32_t lexical_index;
  uint32_t call_index;
} w_seed_parallel_typed_binding1_task;

/* The HIR error identity is derived from one verified typed INVOKE and its
 * throwing callee.  The case ordinal is matched to a provider completion's
 * explicit error_case_ordinal field, never inferred from error_code. */
typedef struct {
  uint32_t invoke_terminator;
  uint32_t call_index;
  uint32_t callee_function;
  uint32_t error_type;
  uint32_t enum_index;
  uint32_t enum_case_index;
  uint32_t case_ordinal;
  uint32_t case_tag;
  uint8_t digest[32];
} w_seed_parallel_typed_binding1_error_identity;

/* Semantic task record.  Provider capacity, worker counts, generation, and
 * receipt facts are excluded from its digest. */
typedef struct {
  uint32_t lexical_index;
  uint32_t call_index;
  uint32_t callee_function;
  w_seed_parallel_typed_binding1_outcome_kind outcome;
  int64_t success_value;
  uint32_t error_type;
  uint32_t error_enum_index;
  uint32_t error_case_index;
  uint32_t error_case_ordinal;
  uint32_t error_case_tag;
  uint8_t error_identity_digest[32];
} w_seed_parallel_typed_binding1_record;

typedef struct {
  size_t completions;
  size_t records;
} w_seed_parallel_typed_binding1_counts;

/* PLATFORM1 writes these caller-owned scratch values.  The binding stages
 * them locally and publishes them here only after all semantic checks pass. */
typedef struct {
  w_seed_parallel_platform1_completion *completions;
  size_t completion_capacity;
  w_seed_parallel_platform1_receipt *receipt;
  w_seed_parallel_provider0_kind *provider_kind;
} w_seed_parallel_typed_binding1_workspace;

/* This signal is a private PARBIND1 handoff.  It is not an Error case,
 * TaskOutcome, Result carrier, public ABI, or source-language event. */
typedef struct w_seed_parallel_typed_binding1_panic_signal {
  uint32_t source_index;
  uint32_t lexical_index;
  uint32_t call_index;
  w_seed_parallel_platform1_panic_code panic_code;
  bool semantic_result_published;
  uint8_t hir_semantic_digest[32];
  uint8_t error_identity_digest[32];
  uint32_t started_count;
  uint32_t settled_count;
  uint32_t canceled_before_start_count;
  uint32_t maximum_active;
  uint32_t cancellation_source_index;
  bool cancellation_requested;
  uint32_t panic_source_index;
  bool panic_requested;
} w_seed_parallel_typed_binding1_panic_signal;

typedef struct {
  w_seed_parallel_typed_binding1_record *records;
  size_t record_capacity;
} w_seed_parallel_typed_binding1_output;

typedef struct {
  w_seed_parallel_typed_binding1_provider provider;
  w_seed_parallel_local_provider1_receipt local_authority;
  w_seed_parallel_provider0_kind provider_kind;
  uint32_t provider_capacity;
  uint32_t generation;
  w_seed_parallel_platform1_receipt upstream_receipt;
  w_seed_parallel_typed_binding1_assurance_kind assurance;
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
} w_seed_parallel_typed_binding1_provenance;

typedef struct {
  w_seed_parallel_typed_binding1_status status;
  w_seed_parallel_typed_binding1_counts required;
  w_seed_parallel_typed_binding1_counts written;
  char schema[sizeof(W_SEED_PARALLEL_TYPED_BINDING1_SCHEMA_VERSION)];
  uint32_t task_count;
  uint32_t invoke_terminator;
  uint32_t generation;
  uint32_t primary_error_task;
  w_seed_parallel_typed_binding1_outcome_kind scope_outcome;
  uint8_t hir_semantic_digest[32];
  w_seed_parallel_typed_binding1_error_identity error_identity;
  uint8_t semantic_digest[32];
  w_seed_parallel_typed_binding1_provenance provenance;
} w_seed_parallel_typed_binding1_result;

typedef struct {
  const w_seed_hir0_program *hir_program;
  const w_seed_hir0_result *hir_result;
  uint32_t invoke_terminator;
  const w_seed_parallel_typed_binding1_task *tasks;
  size_t task_count;
  size_t task_capacity;
  w_seed_parallel_platform1_job provider_job;
  const w_seed_parallel_local_provider1_authority *provider_authority;
  uint32_t provider_capacity;
  uint32_t generation;
  w_seed_parallel_typed_binding1_provider provider;
} w_seed_parallel_typed_binding1_input;

w_seed_parallel_typed_binding1_status w_seed_parallel_typed_binding1_measure(
    const w_seed_parallel_typed_binding1_input *input,
    w_seed_parallel_typed_binding1_counts *counts,
    w_seed_parallel_typed_binding1_result *result);

w_seed_parallel_typed_binding1_status w_seed_parallel_typed_binding1_run(
    const w_seed_parallel_typed_binding1_input *input,
    const w_seed_parallel_typed_binding1_workspace *workspace,
    const w_seed_parallel_typed_binding1_output *output,
    w_seed_parallel_typed_binding1_result *result);

bool w_seed_parallel_typed_binding1_verify(
    const w_seed_parallel_typed_binding1_input *input,
    const w_seed_parallel_typed_binding1_workspace *workspace,
    const w_seed_parallel_typed_binding1_output *output,
    const w_seed_parallel_typed_binding1_result *result);

/* Private boundary-only operation.  It executes the already verified
 * PLATFORM1 witness once, validates the complete physical receipt, and only
 * then publishes a panic signal.  OK means that the signal was published;
 * every other status leaves signal untouched.  This is deliberately
 * separate from the ordinary run path so a non-OK PANIC cannot look like a
 * partially committed binding transaction. */
w_seed_parallel_typed_binding1_status
w_seed_parallel_typed_binding1_panic_run(
    const w_seed_parallel_typed_binding1_input *input,
    w_seed_parallel_typed_binding1_panic_signal *signal);

#ifdef __cplusplus
}
#endif

#endif
