#ifndef W_SEED_ACCELERATED_PROVIDER0_H
#define W_SEED_ACCELERATED_PROVIDER0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_accelerated_request0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ACCPROV0 is a private compiler-lifecycle boundary. It consumes exactly one
 * verified ACCREQ0 and one caller-owned native artifact. It is not a public
 * runtime, Task ABI, scheduler, provider registry, or binary-authentication
 * mechanism. Provider-specific handles and pointers may exist only in the
 * provider callback context; they never cross this record boundary. */
#define W_SEED_ACCELERATED_PROVIDER0_SCHEMA_VERSION \
  "w-seed-accelerated-provider0-1"
#define W_SEED_ACCELERATED_PROVIDER0_ARTIFACT_RECEIPT_SCHEMA_VERSION \
  "w-seed-accelerated-native-artifact0-1"
#define W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES 32u
#define W_SEED_ACCELERATED_PROVIDER0_MAX_PHASES 8u
#define W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES 96u
#define W_SEED_ACCELERATED_PROVIDER0_MAX_NATIVE_ARTIFACT_BYTES (16u * 1024u * 1024u)

typedef enum {
  W_SEED_ACCELERATED_PROVIDER0_OK = 0,
  W_SEED_ACCELERATED_PROVIDER0_INVALID_ARGUMENT,
  W_SEED_ACCELERATED_PROVIDER0_INVALID_REQUEST,
  W_SEED_ACCELERATED_PROVIDER0_INVALID_ARTIFACT,
  W_SEED_ACCELERATED_PROVIDER0_INVALID_RECEIPT,
  W_SEED_ACCELERATED_PROVIDER0_INVALID_AUTHORITY,
  W_SEED_ACCELERATED_PROVIDER0_INVALID_STATE,
  W_SEED_ACCELERATED_PROVIDER0_UNSUPPORTED,
  W_SEED_ACCELERATED_PROVIDER0_CAPACITY,
  W_SEED_ACCELERATED_PROVIDER0_ALIAS,
  W_SEED_ACCELERATED_PROVIDER0_PROVIDER_FAILURE,
  W_SEED_ACCELERATED_PROVIDER0_DEVICE_LOST,
  W_SEED_ACCELERATED_PROVIDER0_STALE_GENERATION,
  W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH,
  W_SEED_ACCELERATED_PROVIDER0_RESULT_MISMATCH,
  W_SEED_ACCELERATED_PROVIDER0_CLEANUP_UNCERTAIN,
} w_seed_accelerated_provider0_status;

typedef enum {
  W_SEED_ACCELERATED_PROVIDER0_PHASE_NONE = 0,
  W_SEED_ACCELERATED_PROVIDER0_PHASE_STAGED,
  W_SEED_ACCELERATED_PROVIDER0_PHASE_SUBMITTED,
  W_SEED_ACCELERATED_PROVIDER0_PHASE_DEVICE_RUNNING,
  W_SEED_ACCELERATED_PROVIDER0_PHASE_BODY_SETTLED,
  W_SEED_ACCELERATED_PROVIDER0_PHASE_PROVIDER_DRAINED,
  W_SEED_ACCELERATED_PROVIDER0_PHASE_CLEANUP,
  W_SEED_ACCELERATED_PROVIDER0_PHASE_OUTCOME_COMMITTED,
  W_SEED_ACCELERATED_PROVIDER0_PHASE_JOINED,
} w_seed_accelerated_provider0_phase;

/* Provider callback statuses are physical facts. The core maps them to the
 * bounded ACCPROV0 status without exposing raw provider status semantically. */
typedef enum {
  W_SEED_ACCELERATED_PROVIDER0_CALLBACK_OK = 0,
  W_SEED_ACCELERATED_PROVIDER0_CALLBACK_UNSUPPORTED,
  W_SEED_ACCELERATED_PROVIDER0_CALLBACK_FAILURE,
  W_SEED_ACCELERATED_PROVIDER0_CALLBACK_DEVICE_LOST,
  W_SEED_ACCELERATED_PROVIDER0_CALLBACK_STALE_GENERATION,
  W_SEED_ACCELERATED_PROVIDER0_CALLBACK_PROTOCOL_MISMATCH,
} w_seed_accelerated_provider0_callback_status;

typedef struct {
  const char *data;
  size_t bytes;
} w_seed_accelerated_provider0_text;

/* This receipt is caller-supplied and must be independently reproducible from
 * the ACCREQ0 device artifact and the native artifact bytes. Its link digest
 * binds the request artifact digest, target, provider ABI class, and native
 * artifact digest. No provider instance or physical handle is represented. */
typedef struct {
  char schema[sizeof(
      W_SEED_ACCELERATED_PROVIDER0_ARTIFACT_RECEIPT_SCHEMA_VERSION)];
  uint8_t request_device_artifact_digest
      [W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
  uint8_t target_digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
  uint8_t provider_abi_class_digest
      [W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
  uint8_t native_artifact_digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
  uint8_t link_digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
} w_seed_accelerated_provider0_artifact_receipt;

typedef struct {
  const uint8_t *bytes;
  size_t byte_count;
  w_seed_accelerated_provider0_text target;
  w_seed_accelerated_provider0_text provider_abi_class;
  const w_seed_accelerated_provider0_artifact_receipt *receipt;
} w_seed_accelerated_provider0_native_artifact;

typedef struct {
  w_seed_accelerated_provider0_callback_status status;
  uint32_t raw_status;
  uint32_t generation;
  int32_t result_value;
  bool effect_started;
  bool body_settled;
  bool provider_drained;
  bool cleanup_succeeded;
  char device[W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
  char queue[W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
  char generation_text[W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
} w_seed_accelerated_provider0_callback_event;

typedef bool (*w_seed_accelerated_provider0_stage_fn)(
    void *context, const w_seed_accelerated_request0_program *request_program,
    const w_seed_accelerated_request0_result *request_result,
    const w_seed_accelerated_provider0_native_artifact *artifact,
    w_seed_accelerated_provider0_callback_event *event);

typedef bool (*w_seed_accelerated_provider0_submit_fn)(
    void *context, w_seed_accelerated_provider0_callback_event *event);

typedef bool (*w_seed_accelerated_provider0_join_fn)(
    void *context, w_seed_accelerated_provider0_callback_event *event);

typedef bool (*w_seed_accelerated_provider0_cleanup_fn)(
    void *context, w_seed_accelerated_provider0_callback_event *event);

typedef struct {
  w_seed_accelerated_provider0_stage_fn stage;
  w_seed_accelerated_provider0_submit_fn submit;
  w_seed_accelerated_provider0_join_fn join;
  w_seed_accelerated_provider0_cleanup_fn cleanup;
} w_seed_accelerated_provider0_vtable;

/* A process-local authority is only a private compiler-owned binding. The
 * seal prevents accidental construction of an unbound token in this process;
 * it is explicitly not a cryptographic provider attestation. */
typedef struct {
  uintptr_t private_seal;
  const w_seed_accelerated_provider0_vtable *vtable;
  void *context;
  size_t context_bytes;
  uint32_t generation;
  char implementation_identity[
      W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
} w_seed_accelerated_provider0_authority;

/* Physical provenance is separate from semantic output. Fixed arrays make the
 * receipt caller-owned and self-contained without hidden host allocation. */
typedef struct {
  char schema[sizeof(W_SEED_ACCELERATED_PROVIDER0_SCHEMA_VERSION)];
  uint32_t phase_count;
  w_seed_accelerated_provider0_phase
      phases[W_SEED_ACCELERATED_PROVIDER0_MAX_PHASES];
  char provider_abi_class[W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
  char target[W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
  char implementation_identity[
      W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
  char device[W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
  char queue[W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
  char generation[W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
  uint8_t request_semantic_digest
      [W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
  uint8_t request_device_artifact_digest
      [W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
  uint8_t native_artifact_digest
      [W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
  uint32_t raw_status;
  bool stage_called;
  bool submit_called;
  bool join_called;
  bool effect_uncertain;
  bool body_settled;
  bool provider_drained;
  bool cleanup_attempted;
  bool cleanup_succeeded;
  uint8_t reserved[5];
  uint8_t provenance_digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
} w_seed_accelerated_provider0_receipt;

/* Semantic output contains no provider or physical identity. On failure the
 * output remains untouched; no semantic result is published. */
typedef struct {
  char schema[sizeof(W_SEED_ACCELERATED_PROVIDER0_SCHEMA_VERSION)];
  uint32_t result_bytes;
  uint16_t result_bit_width;
  bool result_is_signed;
  bool success;
  int32_t value;
  uint32_t phase_count;
  w_seed_accelerated_provider0_phase
      phases[W_SEED_ACCELERATED_PROVIDER0_MAX_PHASES];
  uint8_t request_semantic_digest
      [W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
  uint8_t semantic_digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
} w_seed_accelerated_provider0_outcome;

/* State is caller-owned scratch. It deliberately retains only opaque
 * provider callbacks/context plus verified producer pointers until destroy;
 * it is not serializable or semantic output. begin() preflights and binds the
 * outcome and receipt addresses; every later step must use those exact
 * addresses. A terminal state cannot run or destroy again, preventing
 * retry/double-cleanup after uncertain effects. */
typedef struct {
  w_seed_accelerated_provider0_phase phase;
  bool terminal;
  bool stage_called;
  bool submit_called;
  bool join_called;
  bool effect_started;
  bool effect_uncertain;
  bool cleanup_attempted;
  bool cleanup_succeeded;
  bool stage_succeeded;
  w_seed_accelerated_provider0_status failure_status;
  uint32_t raw_status;
  int32_t result_value;
  uint32_t expected_generation;
  const w_seed_accelerated_provider0_authority *authority;
  const w_seed_accelerated_request0_program *request_program;
  const w_seed_accelerated_request0_result *request_result;
  const w_seed_accelerated_provider0_native_artifact *artifact;
  uint8_t request_semantic_digest
      [W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
  uint8_t request_device_artifact_digest
      [W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
  uint8_t native_artifact_digest
      [W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES];
  char device[W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
  char queue[W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
  char generation[W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
  /* The begin preflight binds both caller-owned output addresses. Every
   * subsequent step must receive these exact addresses; no new writable range
   * may be introduced after a provider effect. */
  w_seed_accelerated_provider0_outcome *outcome;
  w_seed_accelerated_provider0_receipt *receipt;
} w_seed_accelerated_provider0_state;

typedef struct {
  const w_seed_accelerated_request0_program *request_program;
  const w_seed_accelerated_request0_result *request_result;
  const w_seed_accelerated_provider0_native_artifact *artifact;
  const w_seed_accelerated_provider0_authority *authority;
} w_seed_accelerated_provider0_input;

bool w_seed_accelerated_provider0_authority_open(
    const w_seed_accelerated_provider0_vtable *vtable, void *context,
    size_t context_bytes, w_seed_accelerated_provider0_text implementation,
    uint32_t generation, w_seed_accelerated_provider0_authority *authority);

bool w_seed_accelerated_provider0_authority_verify(
    const w_seed_accelerated_provider0_authority *authority);

w_seed_accelerated_provider0_status w_seed_accelerated_provider0_begin(
    const w_seed_accelerated_provider0_input *input,
    w_seed_accelerated_provider0_state *state,
    /* outcome and receipt are both preflighted here, even when execution will
     * ultimately fail; destroy() must receive these same addresses. */
    w_seed_accelerated_provider0_outcome *outcome,
    w_seed_accelerated_provider0_receipt *receipt);

w_seed_accelerated_provider0_status w_seed_accelerated_provider0_submit(
    w_seed_accelerated_provider0_state *state,
    w_seed_accelerated_provider0_receipt *receipt);

w_seed_accelerated_provider0_status w_seed_accelerated_provider0_join(
    w_seed_accelerated_provider0_state *state,
    w_seed_accelerated_provider0_receipt *receipt);

/* outcome is the exact address bound by begin(); it is required even on a
 * failed run so cleanup cannot be paired with a newly introduced output. */
w_seed_accelerated_provider0_status w_seed_accelerated_provider0_destroy(
    w_seed_accelerated_provider0_state *state,
    w_seed_accelerated_provider0_outcome *outcome,
    w_seed_accelerated_provider0_receipt *receipt);

w_seed_accelerated_provider0_status w_seed_accelerated_provider0_run(
    const w_seed_accelerated_provider0_input *input,
    w_seed_accelerated_provider0_state *state,
    w_seed_accelerated_provider0_outcome *outcome,
    w_seed_accelerated_provider0_receipt *receipt);

bool w_seed_accelerated_provider0_verify_outcome(
    const w_seed_accelerated_provider0_input *input,
    const w_seed_accelerated_provider0_outcome *outcome);

bool w_seed_accelerated_provider0_verify_receipt(
    const w_seed_accelerated_provider0_input *input,
    const w_seed_accelerated_provider0_receipt *receipt);

#ifdef __cplusplus
}
#endif

#endif
