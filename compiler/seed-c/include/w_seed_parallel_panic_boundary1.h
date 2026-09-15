#ifndef W_SEED_PARALLEL_PANIC_BOUNDARY1_H
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_H

#include "w_seed_parallel_typed_binding1.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PANICBOUNDARY1 is a private compiler-lifecycle witness.  It consumes the
 * verified PARBIND1 panic signal in one Windows x64 root child process.  The
 * receipt claims only root-child termination and join.  It makes no
 * descendant-tree completion claim.  It is not a public process, Task,
 * runtime, source syntax, or W ABI contract. */
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_SCHEMA_VERSION \
  "w-seed-parallel-panic-boundary1-1"
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_MAGIC 0x57504231u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROTOCOL_VERSION 1u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TYPE_PANIC 1u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_HEADER_BYTES 32u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SCHEMA_BYTES 48u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES 288u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_DEFAULT_TIMEOUT_MS 2000u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_MAX_TIMEOUT_MS 10000u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_TERMINATION_EXIT_CODE 0xe1u

/* The wire is encoded explicitly as big-endian fields.  These offsets are
 * protocol constants, not C structure layout or a public ABI.  Header fields
 * make the frame self-identifying and reject stale, duplicate, partial, or
 * trailing output before a receipt can be committed. */
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_MAGIC_OFFSET 0u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_VERSION_OFFSET 4u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TYPE_OFFSET 8u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_LENGTH_OFFSET 12u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_NONCE_OFFSET 16u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SEQUENCE_OFFSET 24u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_RESERVED_OFFSET 28u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SCHEMA_OFFSET 32u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_OFFSET 80u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TARGET_OFFSET 84u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVIDER_CAPACITY_OFFSET 88u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVIDER_KIND_OFFSET 92u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_GENERATION_OFFSET 96u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TASK_COUNT_OFFSET 100u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SOURCE_INDEX_OFFSET 104u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_LEXICAL_INDEX_OFFSET 108u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CALL_INDEX_OFFSET 112u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PANIC_CODE_OFFSET 116u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SEMANTIC_PUBLISHED_OFFSET 120u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CHILD_PID_OFFSET 124u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STARTED_COUNT_OFFSET 128u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SETTLED_COUNT_OFFSET 132u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CANCELED_COUNT_OFFSET 136u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_MAXIMUM_ACTIVE_OFFSET 140u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CANCEL_SOURCE_OFFSET 144u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CANCEL_REQUESTED_OFFSET 148u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PANIC_SOURCE_OFFSET 152u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PANIC_REQUESTED_OFFSET 156u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_HIR_DIGEST_OFFSET 160u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_ERROR_DIGEST_OFFSET 192u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_AUTHORITY_DIGEST_OFFSET 224u
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVENANCE_DIGEST_OFFSET 256u

typedef enum {
  W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_NONE = 0,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_EARLY_EXIT,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_PARTIAL_OUTPUT,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_MALFORMED,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_TIMEOUT,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_NON_PANIC,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_TRAILING_OUTPUT,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_STALE_NONCE,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_STALE_DIGEST,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_STALE_GENERATION,
} w_seed_parallel_panic_boundary1_fault;

typedef enum {
  W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_NONE = 0,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_PANIC = 1,
} w_seed_parallel_panic_boundary1_wire_status;

typedef enum {
  W_SEED_PARALLEL_PANIC_BOUNDARY1_OK = 0,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_INVALID,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_UNSUPPORTED,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_CAPACITY,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_ALIAS,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_HIR,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_AUTHORITY,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_PARBIND,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_NON_PANIC,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_EARLY_EXIT,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_PARTIAL,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TIMEOUT,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_NOT_LIVE,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEARDOWN,
} w_seed_parallel_panic_boundary1_status;

typedef struct {
  const w_seed_parallel_typed_binding1_input *typed_input;
  const char *helper_path;
  uint32_t expected_panic_code;
  uint32_t timeout_ms;
  uint64_t invocation_nonce;
  w_seed_parallel_panic_boundary1_fault helper_fault;
} w_seed_parallel_panic_boundary1_input;

typedef struct {
  char schema[sizeof(W_SEED_PARALLEL_PANIC_BOUNDARY1_SCHEMA_VERSION)];
  uint32_t protocol_version;
  uint32_t frame_type;
  uint32_t frame_length;
  uint64_t invocation_nonce;
  uint32_t sequence;
  w_seed_parallel_panic_boundary1_wire_status wire_status;
  uint32_t target;
  uint32_t provider_capacity;
  uint32_t provider_kind;
  uint32_t generation;
  uint32_t task_count;
  uint32_t panic_source_index;
  uint32_t panic_source_lexical_index;
  uint32_t panic_source_call_index;
  w_seed_parallel_platform1_panic_code panic_code;
  bool semantic_result_published;
  uint32_t child_pid;
  uint32_t started_count;
  uint32_t settled_count;
  uint32_t canceled_before_start_count;
  uint32_t maximum_active;
  uint32_t cancellation_source_index;
  bool cancellation_requested;
  uint32_t panic_receipt_source_index;
  bool panic_requested;
  uint8_t hir_semantic_digest[32];
  uint8_t error_identity_digest[32];
  uint8_t authority_digest[32];
  uint8_t wire_provenance_digest[32];
  uint32_t child_exit_code;
  bool child_was_live_at_teardown;
  bool child_terminated;
  bool child_joined;
  bool handles_closed;
  uint8_t provenance_digest[32];
} w_seed_parallel_panic_boundary1_receipt;

typedef struct {
  w_seed_parallel_panic_boundary1_receipt *receipt;
  size_t receipt_capacity;
} w_seed_parallel_panic_boundary1_output;

/* This encoder is a private helper/test seam.  The helper is a trusted
 * private witness artifact for this seed test.  The unkeyed SHA-256 wire
 * digest provides correlation and integrity only.  It is not authentication
 * or provider-origin proof.  The seam is shared by the helper executable and
 * parent adapter so the inherited protocol has one definition. */
bool w_seed_parallel_panic_boundary1_encode_wire(
    const w_seed_parallel_typed_binding1_input *typed_input,
    const w_seed_parallel_typed_binding1_result *measured,
    const w_seed_parallel_typed_binding1_panic_signal *signal,
    uint32_t child_pid, uint64_t invocation_nonce,
    uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES]);

w_seed_parallel_panic_boundary1_status
w_seed_parallel_panic_boundary1_run(
    const w_seed_parallel_panic_boundary1_input *input,
    const w_seed_parallel_panic_boundary1_output *output);

bool w_seed_parallel_panic_boundary1_verify(
    const w_seed_parallel_panic_boundary1_input *input,
    const w_seed_parallel_panic_boundary1_receipt *receipt);

#ifdef __cplusplus
}
#endif

#endif
