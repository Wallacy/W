#ifndef W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_H
#define W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_H

#include "w_seed_parallel_panic_lifecycle1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PANICHOSTREG1 is a private, Windows x64 host-side witness above
 * PANICLIFE1. It owns one anonymous event created by CreateEventW and
 * observes one CloseHandle result. It is not PANICBOUNDARY1, a general
 * resource registry, a runtime, a public PanicEvent, or user cleanup. */
#define W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_SCHEMA_VERSION \
  "w-seed-parallel-panic-host-registry1-1"
#define W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_WINDOWS_AMD64 1u
#define W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT 2u
#define W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_PRIMARY_NONE UINT32_MAX

typedef enum {
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK = 0,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_INVALID,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_UNSUPPORTED,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_CAPACITY,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_ALIAS,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_AUTHORITY,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_UPSTREAM,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_NO_PANIC,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RELEASE_BLOCKED,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_CREATE_FAILED,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RELEASE_UNCERTAIN,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_FORGERY,
} w_seed_parallel_panic_host_registry1_status;

typedef enum {
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_ASSURANCE_NONE = 0,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_ASSURANCE_STATIC_LOCAL_BINDING = 1,
} w_seed_parallel_panic_host_registry1_assurance;

typedef enum {
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RESOURCE_NONE = 0,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RESOURCE_EVENT,
} w_seed_parallel_panic_host_registry1_resource_kind;

typedef enum {
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_EMPTY = 0,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_REGISTERED,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_UNCERTAIN,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_DESTROYED,
} w_seed_parallel_panic_host_registry1_state;

typedef enum {
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_NONE = 0,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_RESOURCE_REGISTERED,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_RESOURCE_CLOSE_COMMITTED,
} w_seed_parallel_panic_host_registry1_event_kind;

/* This seam is for deterministic C tests. PRE_CLOSE_FAILURE does not call
 * CloseHandle and leaves the registry owner responsible for destroy().
 * POST_CLOSE_RESULT_UNCERTAIN calls the real CloseHandle once, then records
 * an intentionally uncertain result without retrying. */
typedef enum {
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_NONE = 0,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_PRE_CLOSE_FAILURE,
  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_POST_CLOSE_RESULT_UNCERTAIN,
} w_seed_parallel_panic_host_registry1_test_fault;

typedef struct {
  char schema[sizeof(W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_SCHEMA_VERSION)];
  uint32_t target;
  uint32_t generation;
  w_seed_parallel_panic_host_registry1_resource_kind resource_kind;
  w_seed_parallel_panic_host_registry1_assurance assurance;
  uint8_t contract_digest[32];
} w_seed_parallel_panic_host_registry1_authority_receipt;

typedef struct {
  /* The seal and self pointer are process-local authentication state. Neither
   * is serialized or copied into a result or digest. */
  uintptr_t private_seal;
  const void *self;
  w_seed_parallel_panic_host_registry1_authority_receipt receipt;
} w_seed_parallel_panic_host_registry1_authority;

typedef struct {
  const w_seed_parallel_panic_lifecycle1_input *lifecycle_input;
  const w_seed_parallel_panic_lifecycle1_output *lifecycle_output;
  const w_seed_parallel_panic_lifecycle1_result *lifecycle_result;
} w_seed_parallel_panic_host_registry1_input;

typedef struct {
  size_t records;
  size_t events;
} w_seed_parallel_panic_host_registry1_counts;

/* The semantic record contains only the one resource transition and its
 * ordered event trace. It contains no handle, address, process, source, or
 * provider identity. */
typedef struct {
  w_seed_parallel_panic_host_registry1_resource_kind resource_kind;
  w_seed_parallel_panic_host_registry1_state from_state;
  w_seed_parallel_panic_host_registry1_state to_state;
  uint32_t registered_count;
  uint32_t released_count;
  uint32_t event_count;
  uint8_t semantic_digest[32];
} w_seed_parallel_panic_host_registry1_record;

typedef struct {
  uint32_t sequence;
  w_seed_parallel_panic_host_registry1_event_kind kind;
} w_seed_parallel_panic_host_registry1_event;

typedef struct {
  w_seed_parallel_panic_host_registry1_record *record;
  size_t record_capacity;
  w_seed_parallel_panic_host_registry1_event *events;
  size_t event_capacity;
} w_seed_parallel_panic_host_registry1_output;

/* The result is a pointer-free provenance receipt. The raw HANDLE is
 * deliberately absent; CloseHandle success is the only physical close fact
 * claimed by this slice. */
typedef struct {
  w_seed_parallel_panic_host_registry1_status status;
  w_seed_parallel_panic_host_registry1_counts required;
  w_seed_parallel_panic_host_registry1_counts written;
  char schema[sizeof(W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_SCHEMA_VERSION)];
  w_seed_parallel_panic_host_registry1_resource_kind resource_kind;
  w_seed_parallel_panic_host_registry1_state final_state;
  uint32_t event_count;
  uint32_t primary_source_index;
  uint32_t panic_count;
  uint32_t generation;
  uint32_t provider_capacity;
  w_seed_parallel_provider0_kind provider_kind;
  w_seed_parallel_panic_host_registry1_authority_receipt authority_receipt;
  w_seed_parallel_platform1_receipt platform_receipt;
  uint8_t panic_semantic_digest[32];
  uint8_t panic_provenance_digest[32];
  uint8_t hir_semantic_digest[32];
  uint8_t hir_provenance_digest[32];
  uint8_t selection_semantic_digest[32];
  uint8_t invocation_semantic_digest[32];
  uint8_t lifecycle_semantic_digest[32];
  uint8_t lifecycle_provenance_digest[32];
  bool close_attempted;
  bool close_succeeded;
  uint8_t provenance_digest[32];
} w_seed_parallel_panic_host_registry1_result;

/* Caller-owned registry storage. The private event value is never returned,
 * duplicated, or copied into output/result. Callers must zero-initialize it
 * before open_event; only the component may transition its state. */
typedef struct {
  const void *self;
  const w_seed_parallel_panic_host_registry1_authority *authority;
  uint32_t expected_primary;
  uint32_t expected_generation;
  w_seed_parallel_panic_host_registry1_state state;
  w_seed_parallel_panic_host_registry1_test_fault test_fault;
  uint32_t close_attempt_count;
  uint32_t close_success_count;
  uintptr_t private_event_handle;
} w_seed_parallel_panic_host_registry1_registry;

bool w_seed_parallel_panic_host_registry1_open(
    uint32_t target, w_seed_parallel_panic_host_registry1_authority *authority);

bool w_seed_parallel_panic_host_registry1_authority_verify(
    const w_seed_parallel_panic_host_registry1_authority *authority);

bool w_seed_parallel_panic_host_registry1_authority_receipt_equal(
    const w_seed_parallel_panic_host_registry1_authority_receipt *left,
    const w_seed_parallel_panic_host_registry1_authority_receipt *right);

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_open_event(
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_registry *registry,
    uint32_t expected_primary, uint32_t expected_generation);

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_measure(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const w_seed_parallel_panic_host_registry1_registry *registry,
    w_seed_parallel_panic_host_registry1_counts *counts,
    w_seed_parallel_panic_host_registry1_result *result);

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_release(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_registry *registry,
    const w_seed_parallel_panic_host_registry1_output *output,
    w_seed_parallel_panic_host_registry1_result *result);

/* `run` is the conventional bounded-component spelling for the side-effecting
 * release operation. It has exactly the same all-or-nothing contract. */
w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_run(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_registry *registry,
    const w_seed_parallel_panic_host_registry1_output *output,
    w_seed_parallel_panic_host_registry1_result *result);

bool w_seed_parallel_panic_host_registry1_verify(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const w_seed_parallel_panic_host_registry1_registry *registry,
    const w_seed_parallel_panic_host_registry1_output *output,
    const w_seed_parallel_panic_host_registry1_result *result);

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_destroy(
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_registry *registry);

/* Test-only fault injection; production callers should leave the seam at
 * NONE. It never exposes the internal HANDLE. */
w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_test_set_fault(
    w_seed_parallel_panic_host_registry1_registry *registry,
    w_seed_parallel_panic_host_registry1_test_fault fault);

#ifdef __cplusplus
}
#endif

#endif
