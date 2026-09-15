#ifndef W_SEED_TASK_LIFECYCLE0_H
#define W_SEED_TASK_LIFECYCLE0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * TASKLIFE0 is a target-neutral semantic kernel.  Its records are caller
 * owned and have fixed storage.  They describe a checked logical trace.  They
 * are not task frames, scheduler records, runtime objects, or an ABI.
 *
 * Four is an evidence-only seed ceiling for this implementation.  It is not a
 * W language limit and it is not an ABI limit.
 */
#define W_SEED_TASK_LIFECYCLE0_SCHEMA_VERSION \
  "w-seed-task-lifecycle0-1"
#define W_SEED_TASK_LIFECYCLE0_MAX_TASKS 4u
#define W_SEED_TASK_LIFECYCLE0_MAX_EVENTS 128u
#define W_SEED_TASK_LIFECYCLE0_NONE UINT32_MAX

typedef enum {
  W_SEED_TASK_LIFECYCLE0_OK = 0,
  W_SEED_TASK_LIFECYCLE0_INVALID_ARGUMENT,
  W_SEED_TASK_LIFECYCLE0_MALFORMED,
  W_SEED_TASK_LIFECYCLE0_CAPACITY,
  W_SEED_TASK_LIFECYCLE0_DUPLICATE,
  W_SEED_TASK_LIFECYCLE0_MISSING,
  W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER,
  W_SEED_TASK_LIFECYCLE0_STALE_GENERATION,
  W_SEED_TASK_LIFECYCLE0_NONMONOTONIC_CANCELLATION,
  W_SEED_TASK_LIFECYCLE0_JOIN_ORDER,
  W_SEED_TASK_LIFECYCLE0_RELEASE_ORDER,
  W_SEED_TASK_LIFECYCLE0_ORPHAN,
  W_SEED_TASK_LIFECYCLE0_REORDERED,
  W_SEED_TASK_LIFECYCLE0_INVALID_OUTCOME,
  W_SEED_TASK_LIFECYCLE0_ALIAS,
} w_seed_task_lifecycle0_status;

typedef enum {
  W_SEED_TASK_LIFECYCLE0_TASK_UNINITIALIZED = 0,
  W_SEED_TASK_LIFECYCLE0_TASK_RESERVED,
  W_SEED_TASK_LIFECYCLE0_TASK_PUBLISHED,
  W_SEED_TASK_LIFECYCLE0_TASK_ACTIVE,
  W_SEED_TASK_LIFECYCLE0_TASK_READY,
  W_SEED_TASK_LIFECYCLE0_TASK_SUSPENDED,
  W_SEED_TASK_LIFECYCLE0_TASK_BODY_SETTLED,
  W_SEED_TASK_LIFECYCLE0_TASK_CLEANUP,
  W_SEED_TASK_LIFECYCLE0_TASK_OUTCOME_COMMITTED,
  W_SEED_TASK_LIFECYCLE0_TASK_JOINED,
  W_SEED_TASK_LIFECYCLE0_TASK_RELEASED,
} w_seed_task_lifecycle0_task_state;

typedef enum {
  W_SEED_TASK_LIFECYCLE0_SCOPE_UNINITIALIZED = 0,
  W_SEED_TASK_LIFECYCLE0_SCOPE_OPEN,
  W_SEED_TASK_LIFECYCLE0_SCOPE_CANCELLATION_REQUESTED,
  W_SEED_TASK_LIFECYCLE0_SCOPE_DRAINING,
  W_SEED_TASK_LIFECYCLE0_SCOPE_CHILDREN_DRAINED,
  W_SEED_TASK_LIFECYCLE0_SCOPE_OUTCOME_COMMITTED,
  W_SEED_TASK_LIFECYCLE0_SCOPE_JOINED,
  W_SEED_TASK_LIFECYCLE0_SCOPE_RETAINED,
} w_seed_task_lifecycle0_scope_state;

typedef enum {
  W_SEED_TASK_LIFECYCLE0_OUTCOME_NONE = 0,
  W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS,
  W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR,
  W_SEED_TASK_LIFECYCLE0_OUTCOME_CANCELED,
} w_seed_task_lifecycle0_outcome_kind;

/* Error values are closed in this seed.  Cancellation is not an error value. */
typedef enum {
  W_SEED_TASK_LIFECYCLE0_ERROR_NONE = 0,
  W_SEED_TASK_LIFECYCLE0_ERROR_BODY = 1,
  W_SEED_TASK_LIFECYCLE0_ERROR_CLEANUP = 2,
  W_SEED_TASK_LIFECYCLE0_ERROR_SCOPE = 3,
  W_SEED_TASK_LIFECYCLE0_ERROR_TEST = 4,
} w_seed_task_lifecycle0_error_code;

#define W_SEED_TASK_LIFECYCLE0_ERROR_FIRST \
  W_SEED_TASK_LIFECYCLE0_ERROR_BODY
#define W_SEED_TASK_LIFECYCLE0_ERROR_LAST \
  W_SEED_TASK_LIFECYCLE0_ERROR_TEST

typedef enum {
  W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_NONE = 0,
  W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_ERROR_FAIL_FAST = 1,
  W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_USER_REQUEST = 2,
  W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_SHUTDOWN = 3,
  W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_SUPERSEDED = 4,
} w_seed_task_lifecycle0_cancel_reason;

/* This is the complete cancellation payload.  It contains no pointer. */
typedef struct {
  uint32_t generation;
  uint32_t request_sequence;
  uint32_t source_index;
  w_seed_task_lifecycle0_cancel_reason reason;
} w_seed_task_lifecycle0_cancellation_snapshot;

/* The payload is valid only for its tag.  Unused fields must be zero. */
typedef struct {
  w_seed_task_lifecycle0_outcome_kind kind;
  int64_t success_value;
  w_seed_task_lifecycle0_error_code error_code;
  w_seed_task_lifecycle0_cancellation_snapshot canceled;
} w_seed_task_lifecycle0_outcome;

/* The body outcome is a candidate.  A cancellation request before settlement
 * replaces it with canceled(snapshot).  A request after settlement is late
 * and does not replace the settled candidate. */
typedef struct {
  uint32_t task_id;
  uint32_t lexical_index;
  uint32_t generation;
  w_seed_task_lifecycle0_outcome body_outcome;
} w_seed_task_lifecycle0_task_spec;

typedef enum {
  W_SEED_TASK_LIFECYCLE0_EVENT_NONE = 0,
  W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_OPEN,
  W_SEED_TASK_LIFECYCLE0_EVENT_TASK_RESERVED,
  W_SEED_TASK_LIFECYCLE0_EVENT_TASK_PUBLISHED,
  W_SEED_TASK_LIFECYCLE0_EVENT_TASK_ACTIVE,
  W_SEED_TASK_LIFECYCLE0_EVENT_TASK_READY,
  W_SEED_TASK_LIFECYCLE0_EVENT_TASK_SUSPENDED,
  W_SEED_TASK_LIFECYCLE0_EVENT_TASK_BODY_SETTLED,
  W_SEED_TASK_LIFECYCLE0_EVENT_TASK_CLEANUP,
  W_SEED_TASK_LIFECYCLE0_EVENT_TASK_OUTCOME_COMMITTED,
  W_SEED_TASK_LIFECYCLE0_EVENT_TASK_JOINED,
  W_SEED_TASK_LIFECYCLE0_EVENT_TASK_RELEASED,
  W_SEED_TASK_LIFECYCLE0_EVENT_TASK_CANCELLATION_REQUESTED,
  W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CANCELLATION_REQUESTED,
  W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_DRAINING,
  W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CHILDREN_DRAINED,
  W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_OUTCOME_COMMITTED,
  W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_JOINED,
  W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_RETAINED,
} w_seed_task_lifecycle0_event_kind;

/* Every event carries its own sequence and generation.  This makes stale and
 * reordered traces rejectable without a runtime pointer or global registry. */
typedef struct {
  w_seed_task_lifecycle0_event_kind kind;
  uint32_t sequence;
  uint32_t target_index;
  uint32_t generation;
  uint32_t source_index;
  w_seed_task_lifecycle0_cancel_reason reason;
  w_seed_task_lifecycle0_outcome outcome;
  w_seed_task_lifecycle0_cancellation_snapshot snapshot;
} w_seed_task_lifecycle0_event;

/* All input is a fixed caller-owned transaction.  The event list is the
 * explicit logical trace consumed by run/measure and re-derived by verify. */
typedef struct {
  char schema[sizeof(W_SEED_TASK_LIFECYCLE0_SCHEMA_VERSION)];
  uint32_t scope_generation;
  uint32_t task_count;
  w_seed_task_lifecycle0_task_spec
      tasks[W_SEED_TASK_LIFECYCLE0_MAX_TASKS];
  uint32_t event_count;
  w_seed_task_lifecycle0_event
      events[W_SEED_TASK_LIFECYCLE0_MAX_EVENTS];
} w_seed_task_lifecycle0_transaction;

typedef struct {
  uint32_t task_id;
  uint32_t lexical_index;
  uint32_t generation;
  w_seed_task_lifecycle0_task_state state;
  uint8_t cancellation_requested;
  uint8_t settled_before_cancellation;
  uint16_t reserved;
  uint32_t transition_count;
  w_seed_task_lifecycle0_cancellation_snapshot cancellation;
  w_seed_task_lifecycle0_outcome candidate;
  w_seed_task_lifecycle0_outcome outcome;
} w_seed_task_lifecycle0_task_record;

typedef struct {
  w_seed_task_lifecycle0_scope_state state;
  uint8_t cancellation_requested;
  uint8_t reserved[3];
  uint32_t transition_count;
  uint32_t primary_error_task;
  w_seed_task_lifecycle0_cancellation_snapshot cancellation;
  w_seed_task_lifecycle0_outcome outcome;
} w_seed_task_lifecycle0_scope_record;

/* Full run result.  The copied trace is part of the exact transaction
 * snapshot.  No field contains target, scheduler, pointer, or ABI data. */
typedef struct {
  char schema[sizeof(W_SEED_TASK_LIFECYCLE0_SCHEMA_VERSION)];
  uint32_t scope_generation;
  uint32_t task_count;
  uint32_t event_count;
  uint32_t transition_count;
  uint32_t primary_error_task;
  w_seed_task_lifecycle0_scope_record scope;
  w_seed_task_lifecycle0_task_record
      tasks[W_SEED_TASK_LIFECYCLE0_MAX_TASKS];
  w_seed_task_lifecycle0_event
      trace[W_SEED_TASK_LIFECYCLE0_MAX_EVENTS];
  uint64_t transaction_digest;
} w_seed_task_lifecycle0_result;

/* Measure is a smaller exact snapshot.  It is independently regenerated from
 * the same transaction and is not a caller-supplied count or outcome claim. */
typedef struct {
  char schema[sizeof(W_SEED_TASK_LIFECYCLE0_SCHEMA_VERSION)];
  uint32_t scope_generation;
  uint32_t task_count;
  uint32_t event_count;
  uint32_t transition_count;
  uint32_t primary_error_task;
  w_seed_task_lifecycle0_scope_record scope;
  w_seed_task_lifecycle0_task_record
      tasks[W_SEED_TASK_LIFECYCLE0_MAX_TASKS];
  uint64_t transaction_digest;
} w_seed_task_lifecycle0_measurement;

/* Both operations are all-or-nothing.  A failure leaves the destination
 * byte-for-byte unchanged. */
w_seed_task_lifecycle0_status w_seed_task_lifecycle0_run(
    const w_seed_task_lifecycle0_transaction *transaction,
    w_seed_task_lifecycle0_result *result);

w_seed_task_lifecycle0_status w_seed_task_lifecycle0_measure(
    const w_seed_task_lifecycle0_transaction *transaction,
    w_seed_task_lifecycle0_measurement *measurement);

/* Verify independently replays the transaction and compares every result
 * field, including the copied trace and digest. */
bool w_seed_task_lifecycle0_verify(
    const w_seed_task_lifecycle0_transaction *transaction,
    const w_seed_task_lifecycle0_result *result);

bool w_seed_task_lifecycle0_verify_measurement(
    const w_seed_task_lifecycle0_transaction *transaction,
    const w_seed_task_lifecycle0_measurement *measurement);

/*
 * TASKLIFE1 removes TASKLIFE0's embedded storage ceilings.  The semantic
 * counts remain u32 so they are stable on 32- and 64-bit targets; size_t is
 * used only for physical caller-owned capacities.  None of these records is
 * a runtime task object or task ABI.
 */
#define W_SEED_TASK_LIFECYCLE1_SCHEMA_VERSION \
  "w-seed-task-lifecycle1-1"

typedef struct {
  char schema[sizeof(W_SEED_TASK_LIFECYCLE1_SCHEMA_VERSION)];
  uint32_t scope_generation;
  uint32_t task_count;
  const w_seed_task_lifecycle0_task_spec *tasks;
  uint32_t event_count;
  const w_seed_task_lifecycle0_event *events;
} w_seed_task_lifecycle1_transaction;

typedef struct {
  w_seed_task_lifecycle0_task_record *tasks;
  size_t task_capacity;
} w_seed_task_lifecycle1_workspace;

typedef struct {
  uint32_t task_count;
  uint32_t event_count;
} w_seed_task_lifecycle1_counts;

typedef struct {
  w_seed_task_lifecycle0_task_record *tasks;
  size_t task_capacity;
  w_seed_task_lifecycle0_event *trace;
  size_t event_capacity;
} w_seed_task_lifecycle1_output;

typedef struct {
  char schema[sizeof(W_SEED_TASK_LIFECYCLE1_SCHEMA_VERSION)];
  uint32_t scope_generation;
  uint32_t task_count;
  uint32_t event_count;
  uint32_t transition_count;
  uint32_t primary_error_task;
  w_seed_task_lifecycle0_scope_record scope;
  uint64_t transaction_digest;
} w_seed_task_lifecycle1_result;

/* Workspace is scratch and may change on failure.  Counts, output, and result
 * are transactional and remain byte-for-byte unchanged on failure. */
w_seed_task_lifecycle0_status w_seed_task_lifecycle1_measure(
    const w_seed_task_lifecycle1_transaction *transaction,
    w_seed_task_lifecycle1_workspace workspace,
    w_seed_task_lifecycle1_counts *counts,
    w_seed_task_lifecycle1_result *result);

w_seed_task_lifecycle0_status w_seed_task_lifecycle1_run(
    const w_seed_task_lifecycle1_transaction *transaction,
    w_seed_task_lifecycle1_workspace workspace,
    w_seed_task_lifecycle1_output output,
    w_seed_task_lifecycle1_result *result);

bool w_seed_task_lifecycle1_verify(
    const w_seed_task_lifecycle1_transaction *transaction,
    w_seed_task_lifecycle1_workspace workspace,
    const w_seed_task_lifecycle1_output *output,
    const w_seed_task_lifecycle1_result *result);

#ifdef __cplusplus
}
#endif

#endif
