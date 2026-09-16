#ifndef W_SEED_PARALLEL_PANIC_LIFECYCLE1_H
#define W_SEED_PARALLEL_PANIC_LIFECYCLE1_H

#include "w_seed_parallel_panic_binding1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PANICLIFE1 is a private, target-neutral decision bridge above PARPANIC1.
 * It does not construct a TaskOutcome, call a provider, run a scheduler, or
 * perform the later PANICBOUNDARY1 or resource-registry operation. */
#define W_SEED_PARALLEL_PANIC_LIFECYCLE1_SCHEMA_VERSION \
  "w-seed-parallel-panic-lifecycle1-1"
#define W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT 3u

typedef enum {
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_OK = 0,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_INVALID,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_CAPACITY,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_ALIAS,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_UPSTREAM,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_NO_PANIC,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_FORGERY,
} w_seed_parallel_panic_lifecycle1_status;

typedef enum {
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_STATE_NONE = 0,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_STATE_BOUNDARY_TERMINATION_REQUIRED,
} w_seed_parallel_panic_lifecycle1_state;

typedef enum {
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_NORMAL_OUTCOME_NONE = 0,
} w_seed_parallel_panic_lifecycle1_normal_outcome;

typedef enum {
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_BOUNDARY_ACTION_NONE = 0,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_BOUNDARY_ACTION_TERMINATE_FAULT_BOUNDARY,
} w_seed_parallel_panic_lifecycle1_boundary_action;

typedef enum {
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_CLEANUP_OWNER_NONE = 0,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_CLEANUP_OWNER_BOUNDARY_HOST,
} w_seed_parallel_panic_lifecycle1_cleanup_owner;

typedef enum {
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_USER_CLEANUP_NONE = 0,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_USER_CLEANUP_NOT_CLAIMED,
} w_seed_parallel_panic_lifecycle1_user_cleanup;

typedef enum {
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_RESOURCE_REGISTRY_NONE = 0,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_RESOURCE_REGISTRY_NOT_EVALUATED,
} w_seed_parallel_panic_lifecycle1_resource_registry;

typedef enum {
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_NONE = 0,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_PANIC_OBSERVED,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_NORMAL_OUTCOME_PUBLICATION_FORBIDDEN,
  W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_FAULT_BOUNDARY_TERMINATION_REQUIRED,
} w_seed_parallel_panic_lifecycle1_event_kind;

typedef struct {
  size_t message_bytes;
  size_t events;
} w_seed_parallel_panic_lifecycle1_counts;

/* PANICLIFE1 consumes the complete verified PARPANIC1 view. No provider call
 * is made by this component. The upstream workspace remains an input because
 * its physical receipt is part of independent PARPANIC1 verification. */
typedef struct {
  const w_seed_parallel_panic_binding1_input *panic_input;
  const w_seed_parallel_panic_binding1_workspace *panic_workspace;
  const w_seed_parallel_panic_binding1_output *panic_output;
  const w_seed_parallel_panic_binding1_result *panic_result;
} w_seed_parallel_panic_lifecycle1_input;

/* The decision is the semantic output. Its only variable bytes are copied to
 * the message buffer owned by the caller of PANICLIFE1. Its pointer must point
 * exactly at that output buffer. */
typedef struct {
  w_seed_hir0_panic_code panic_code;
  const uint8_t *message_bytes;
  size_t message_byte_count;
  w_seed_parallel_panic_lifecycle1_state state;
  w_seed_parallel_panic_lifecycle1_normal_outcome normal_outcome;
  w_seed_parallel_panic_lifecycle1_boundary_action boundary_action;
  w_seed_parallel_panic_lifecycle1_cleanup_owner cleanup_owner;
  w_seed_parallel_panic_lifecycle1_user_cleanup user_cleanup;
  w_seed_parallel_panic_lifecycle1_resource_registry resource_registry;
  uint32_t event_count;
  uint8_t semantic_digest[32];
} w_seed_parallel_panic_lifecycle1_decision;

/* Events carry no task, source, provider, or resource facts. The exact three
 * sequence/kind pairs are the complete PANICLIFE1 semantic event trace. */
typedef struct {
  uint32_t sequence;
  w_seed_parallel_panic_lifecycle1_event_kind kind;
} w_seed_parallel_panic_lifecycle1_event;

typedef struct {
  uint8_t *message_bytes;
  size_t message_capacity;
  w_seed_parallel_panic_lifecycle1_event *events;
  size_t event_capacity;
  w_seed_parallel_panic_lifecycle1_decision *decision;
} w_seed_parallel_panic_lifecycle1_output;

/* The result is the provenance receipt. It copies the fixed upstream identity
 * and physical facts, while its source/module byte pointers remain views into
 * the verified PARPANIC1 output. Independent verify therefore requires the
 * live upstream producer graph. */
typedef struct {
  w_seed_parallel_panic_lifecycle1_status status;
  w_seed_parallel_panic_lifecycle1_counts required;
  w_seed_parallel_panic_lifecycle1_counts written;
  char schema[sizeof(W_SEED_PARALLEL_PANIC_LIFECYCLE1_SCHEMA_VERSION)];
  uint32_t event_count;
  uint32_t panic_count;
  uint32_t source_task_index;
  uint32_t lexical_index;
  uint32_t call_index;
  uint32_t function_index;
  uint32_t module_index;
  uint32_t source_expression;
  uint32_t terminator_index;
  uint32_t message_value_index;
  w_seed_hir0_panic_code panic_code;
  w_seed_span function_span;
  w_seed_span call_span;
  w_seed_span terminator_span;
  size_t source_length;
  uint8_t source_sha256[32];
  const uint8_t *source_id_bytes;
  size_t source_id_byte_count;
  const uint8_t *module_id_bytes;
  size_t module_id_byte_count;
  uint8_t hir_semantic_digest[32];
  uint8_t hir_provenance_digest[32];
  uint8_t selection_semantic_digest[32];
  uint8_t invocation_semantic_digest[32];
  w_seed_parallel_provider0_kind provider_kind;
  uint32_t provider_capacity;
  uint32_t generation;
  w_seed_parallel_local_provider1_receipt authority_receipt;
  w_seed_parallel_platform1_receipt platform_receipt;
  uint8_t provenance_digest[32];
} w_seed_parallel_panic_lifecycle1_result;

w_seed_parallel_panic_lifecycle1_status
w_seed_parallel_panic_lifecycle1_measure(
    const w_seed_parallel_panic_lifecycle1_input *input,
    w_seed_parallel_panic_lifecycle1_counts *counts,
    w_seed_parallel_panic_lifecycle1_result *result);

w_seed_parallel_panic_lifecycle1_status
w_seed_parallel_panic_lifecycle1_run(
    const w_seed_parallel_panic_lifecycle1_input *input,
    const w_seed_parallel_panic_lifecycle1_output *output,
    w_seed_parallel_panic_lifecycle1_result *result);

/* Verify does not call a provider and does not mutate input, output, or
 * result. It revalidates PARPANIC1 and the complete three-event decision. */
bool w_seed_parallel_panic_lifecycle1_verify(
    const w_seed_parallel_panic_lifecycle1_input *input,
    const w_seed_parallel_panic_lifecycle1_output *output,
    const w_seed_parallel_panic_lifecycle1_result *result);

#ifdef __cplusplus
}
#endif

#endif
