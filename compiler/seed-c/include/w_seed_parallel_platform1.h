#ifndef W_SEED_PARALLEL_PLATFORM1_H
#define W_SEED_PARALLEL_PLATFORM1_H

#include "w_seed_parallel_provider0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PLATFORM1 is a private physical provider primitive.  It is not a public
 * Task ABI, scheduler, worker pool, cancellation contract, or runtime. */

typedef enum {
  W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS = 1,
  W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR,
  W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED,
  W_SEED_PARALLEL_PLATFORM1_COMPLETION_PANIC,
} w_seed_parallel_platform1_completion_kind;

/* Private stable panic categories. They are a boundary signal, never an Error
 * case, TaskOutcome payload, or recoverable result. */
typedef enum {
  W_SEED_PARALLEL_PLATFORM1_PANIC_NONE = 0,
  W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT,
  W_SEED_PARALLEL_PLATFORM1_PANIC_BOUNDS,
  W_SEED_PARALLEL_PLATFORM1_PANIC_OVERFLOW,
  W_SEED_PARALLEL_PLATFORM1_PANIC_DIVISION_BY_ZERO,
  W_SEED_PARALLEL_PLATFORM1_PANIC_OUT_OF_MEMORY,
  W_SEED_PARALLEL_PLATFORM1_PANIC_INTERNAL_CONTRACT,
} w_seed_parallel_platform1_panic_code;

/* Physical-provider status is shared by the private PLATFORM1 declaration
 * and its implementation. Keeping the closed status type here prevents a
 * public prototype from depending on a source-only header. */
typedef enum {
  W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK = 0,
  W_SEED_PARALLEL_PROVIDER0_PLATFORM_UNSUPPORTED,
  W_SEED_PARALLEL_PROVIDER0_PLATFORM_PROVIDER_FAILURE,
  W_SEED_PARALLEL_PROVIDER0_PLATFORM_TASK_FAILURE,
} w_seed_parallel_provider0_platform_status;

#define W_SEED_PARALLEL_PLATFORM1_CANCEL_FAIL_FAST 1u
#define W_SEED_PARALLEL_PLATFORM1_CANCEL_PANIC_BOUNDARY 2u

/* The payload is valid only for its tag.  Unused fields must be zero. */
typedef struct {
  w_seed_parallel_platform1_completion_kind kind;
  int64_t success_value;
  uint32_t error_code;
  uint32_t cancel_reason;
  /* Provider-typed case ordinal.  The private PLATFORM1 primitive does not
   * interpret this value.  A typed binding must match it to verified HIR. */
  uint32_t error_case_ordinal;
  w_seed_parallel_platform1_panic_code panic_code;
} w_seed_parallel_platform1_completion;

typedef bool (*w_seed_parallel_platform1_task_fn)(
    void *context, size_t task_index,
    w_seed_parallel_platform1_completion *completion);

typedef struct {
  w_seed_parallel_platform1_task_fn invoke;
  void *context;
  /* The callback context is opaque to PLATFORM1, but its caller-owned
   * extent is explicit for alias and lifetime barriers.  A non-null context
   * must have a non-zero extent.  A null context must have extent zero. */
  size_t context_bytes;
} w_seed_parallel_platform1_job;

/* These facts describe one physical PLATFORM1 execution.  They are not a
 * provider authentication token.  A caller must not promote this receipt to
 * a cryptographic origin proof without a separate trusted authority. */
typedef struct {
  uint32_t started_count;
  uint32_t settled_count;
  uint32_t canceled_before_start_count;
  uint32_t maximum_active;
  uint32_t cancellation_source_index;
  bool cancellation_requested;
  uint32_t panic_source_index;
  w_seed_parallel_platform1_panic_code panic_code;
  bool panic_requested;
} w_seed_parallel_platform1_receipt;

bool w_seed_parallel_platform1_receipt_equal(
    const w_seed_parallel_platform1_receipt *left,
    const w_seed_parallel_platform1_receipt *right);

w_seed_parallel_provider0_platform_status
w_seed_parallel_platform1_execute(
    const w_seed_parallel_platform1_job *job, size_t job_count,
    uint32_t provider_capacity,
    w_seed_parallel_platform1_completion *completions,
    w_seed_parallel_platform1_receipt *receipt,
    w_seed_parallel_provider0_kind *provider_kind);

#ifdef __cplusplus
}
#endif

#endif
