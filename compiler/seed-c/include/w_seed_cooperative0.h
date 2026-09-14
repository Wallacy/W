#ifndef W_SEED_COOPERATIVE0_H
#define W_SEED_COOPERATIVE0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_hir0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cooperative0 is an explicitly requested bounded compiler execution oracle.
 * It models one finite single-thread schedule for exactly two verified scalar
 * siblings; it is not a generated Task runtime, async-I/O facility, or
 * parallel worker pool.  The receipt identifies this boundary explicitly. */
#define W_SEED_COOPERATIVE0_SCHEMA_VERSION "w-seed-cooperative0-2"
#define W_SEED_COOPERATIVE0_NONE UINT32_MAX
#define W_SEED_COOPERATIVE0_MAX_TASKS W_SEED_HIR0_COOPERATIVE_ORACLE_MAX_TASKS
#define W_SEED_COOPERATIVE0_MAX_YIELDS_PER_TASK \
  W_SEED_HIR0_COOPERATIVE_MAX_YIELDS_PER_TASK
#define W_SEED_COOPERATIVE0_MAX_FUNCTIONS \
  W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS
#define W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS 128u
#define W_SEED_COOPERATIVE0_MAX_FRAME_PARAMETERS 16u
#define W_SEED_COOPERATIVE0_MAX_QUEUE 2u
#define W_SEED_COOPERATIVE0_MAX_TRACE_EVENTS 64u
#define W_SEED_COOPERATIVE0_MAX_STDOUT_BYTES (16u * 1024u)
#define W_SEED_COOPERATIVE0_MAX_ARTIFACT_BYTES (32u * 1024u)

typedef enum {
  W_SEED_COOPERATIVE0_OK = 0,
  W_SEED_COOPERATIVE0_INVALID,
  W_SEED_COOPERATIVE0_UNSUPPORTED,
  W_SEED_COOPERATIVE0_CAPACITY,
  W_SEED_COOPERATIVE0_ALIAS,
} w_seed_cooperative0_status;

typedef enum {
  W_SEED_COOPERATIVE0_EVENT_RESERVE = 0,
  W_SEED_COOPERATIVE0_EVENT_PUBLISH,
  W_SEED_COOPERATIVE0_EVENT_DISPATCH,
  W_SEED_COOPERATIVE0_EVENT_RESUME,
  W_SEED_COOPERATIVE0_EVENT_YIELD,
  W_SEED_COOPERATIVE0_EVENT_SETTLE,
  W_SEED_COOPERATIVE0_EVENT_CLEANUP,
  W_SEED_COOPERATIVE0_EVENT_OUTCOME_COMMIT,
  W_SEED_COOPERATIVE0_EVENT_JOIN,
  W_SEED_COOPERATIVE0_EVENT_RELEASE,
} w_seed_cooperative0_event_kind;

typedef enum {
  W_SEED_COOPERATIVE0_VALUE_NONE = 0,
  W_SEED_COOPERATIVE0_VALUE_I64,
  W_SEED_COOPERATIVE0_VALUE_BOOL,
  W_SEED_COOPERATIVE0_VALUE_TEXT,
} w_seed_cooperative0_value_kind;

typedef enum {
  /* A plan is an admission proof. Its frames are pristine and may be
   * independently verified before execution. */
  W_SEED_COOPERATIVE0_PLAN_INITIAL = 0,
} w_seed_cooperative0_plan_phase;

typedef enum {
  /* A final state is published only with a complete transition receipt. */
  W_SEED_COOPERATIVE0_STATE_EXECUTED = 1,
} w_seed_cooperative0_state_phase;

/* Target-neutral source descriptor for the compiler-host oracle bridge.  It
 * intentionally carries no backend target or artifact selector. */
typedef struct {
  const char *path;
  size_t path_length;
  w_seed_frontend_text logical_source_id;
} w_seed_cooperative0_input;

/* A frame value is an inline carrier.  TEXT is only a view into verified HIR
 * bytes; no runtime object identity, reference count, or allocation is
 * present. */
typedef struct {
  w_seed_cooperative0_value_kind kind;
  int64_t integer;
  bool boolean;
  uint32_t byte_offset;
  uint32_t byte_count;
} w_seed_cooperative0_value;

typedef struct {
  uint32_t task_id;
  uint32_t call_index;
  uint32_t function_index;
  uint32_t launch_binding;
  uint32_t join_binding;
  uint32_t frame_index;
  uint32_t yield_count;
  uint32_t lexical_ordinal;
} w_seed_cooperative0_task_proof;

typedef struct {
  uint32_t function_index;
  uint32_t next_instruction;
  uint32_t first_instruction;
  uint32_t instruction_count;
  uint32_t parameter_count;
  uint32_t binding_count;
  uint32_t yield_count;
  bool settled;
  bool cleaned;
  bool outcome_committed;
  uint8_t reserved[5];
  w_seed_cooperative0_value parameters[W_SEED_COOPERATIVE0_MAX_FRAME_PARAMETERS];
  w_seed_cooperative0_value bindings[W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS];
  bool binding_initialized[W_SEED_COOPERATIVE0_MAX_FRAME_BINDINGS];
  w_seed_cooperative0_value result;
} w_seed_cooperative0_frame;

typedef struct {
  uint32_t sequence;
  w_seed_cooperative0_event_kind kind;
  uint32_t task_id;
  uint32_t frame_slot;
  uint32_t pc;
  uint32_t next_pc;
  uint32_t yield_ordinal;
  uint32_t queue_before_count;
  uint32_t queue_before[W_SEED_COOPERATIVE0_MAX_QUEUE];
  uint32_t queue_after_count;
  uint32_t queue_after[W_SEED_COOPERATIVE0_MAX_QUEUE];
  /* Digest of the complete fixed frame state at this transition. It binds
   * pc/yield progress to the inline live values without exposing an object. */
  uint8_t frame_digest[32];
  /* Nonzero only for settle/cleanup/outcome_commit/join/release.  It binds
   * the lifecycle observation to the value produced by the frame. */
  uint8_t outcome_digest[32];
} w_seed_cooperative0_trace_event;

/* The plan is a fixed caller-owned proof/result record.  It contains the
 * resumable frames used by execution so tests can inspect that this path has
 * no heap/OS-thread carrier hidden behind a Task symbol. */
typedef struct {
  char schema[sizeof(W_SEED_COOPERATIVE0_SCHEMA_VERSION)];
  uint32_t phase;
  uint32_t task_count;
  uint32_t frame_count;
  uint32_t yield_count;
  uint32_t trace_event_count;
  uint8_t hir_semantic_digest[32];
  w_seed_hir0_execution_profile execution_profile;
  uint8_t reserved[3];
  w_seed_cooperative0_task_proof tasks[W_SEED_COOPERATIVE0_MAX_TASKS];
  w_seed_cooperative0_frame frames[W_SEED_COOPERATIVE0_MAX_TASKS];
} w_seed_cooperative0_plan;

typedef struct {
  char schema[sizeof(W_SEED_COOPERATIVE0_SCHEMA_VERSION)];
  uint32_t phase;
  uint32_t task_count;
  uint32_t frame_count;
  uint32_t trace_event_count;
  uint8_t hir_semantic_digest[32];
  w_seed_hir0_execution_profile execution_profile;
  uint8_t reserved[3];
  w_seed_cooperative0_frame frames[W_SEED_COOPERATIVE0_MAX_TASKS];
} w_seed_cooperative0_execution_state;

typedef struct {
  size_t artifact_bytes;
  size_t stdout_bytes;
  size_t trace_events;
} w_seed_cooperative0_counts;

typedef struct {
  uint8_t *artifact;
  size_t artifact_capacity;
  uint8_t *stdout_bytes;
  size_t stdout_capacity;
  w_seed_cooperative0_trace_event *trace;
  size_t trace_capacity;
} w_seed_cooperative0_output;

typedef struct {
  w_seed_cooperative0_status status;
  w_seed_cooperative0_counts required;
  w_seed_cooperative0_counts written;
  char schema[sizeof(W_SEED_COOPERATIVE0_SCHEMA_VERSION)];
  uint8_t hir_semantic_digest[32];
  w_seed_hir0_execution_profile execution_profile;
  uint8_t reserved[3];
  uint8_t artifact_digest[32];
  uint8_t stdout_digest[32];
  w_seed_cooperative0_plan plan;
  w_seed_cooperative0_execution_state final_state;
} w_seed_cooperative0_result;

w_seed_cooperative0_status w_seed_cooperative0_measure(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    w_seed_cooperative0_counts *counts, w_seed_cooperative0_result *result);

w_seed_cooperative0_status w_seed_cooperative0_run(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative0_output *output,
    w_seed_cooperative0_result *result);

/* Independently validate a physical plan against verified HIR.  This is
 * intentionally public so a test or host can reject forged frame, task,
 * yield, join, order, or count relations without trusting the executor. */
bool w_seed_cooperative0_verify_plan(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative0_plan *plan);

/* Validate the complete actual transition stream independently of the
 * executor. The plan remains initial; execution state is supplied separately
 * so a verifier cannot mistake a running/final frame for an admission frame. */
bool w_seed_cooperative0_verify_trace(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative0_plan *plan,
    const w_seed_cooperative0_trace_event *trace, size_t trace_count);

bool w_seed_cooperative0_verify_execution(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative0_plan *plan,
    const w_seed_cooperative0_trace_event *trace, size_t trace_count,
    const w_seed_cooperative0_execution_state *final_state);

/* Read-only verification of a published output/result pair.  This binds the
 * initial plan, final state, actual trace, artifact bytes, and stdout bytes;
 * it independently recomputes both output digests and rejects aliases. */
bool w_seed_cooperative0_verify_output(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative0_output *output,
    const w_seed_cooperative0_result *result);

#ifdef __cplusplus
}
#endif

#endif
