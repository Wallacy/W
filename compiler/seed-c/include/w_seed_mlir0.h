#ifndef W_SEED_MLIR0_H
#define W_SEED_MLIR0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_cooperative_selection0.h"
#include "w_seed_hir0.h"
#include "w_seed_parallel_invocation0.h"
#include "w_seed_parallel_invocation1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal seed-only MLIR0 terminal adapter. It consumes a verified HIR0
 * program, selects the fixed native subset (including exact float
 * representation transfers), and emits textual builtin plus LLVM dialect
 * MLIR for one fixed target. The artifact is recipe-private and does not
 * allocate or execute. */
#define W_SEED_MLIR0_SCHEMA_VERSION "w-seed-mlir0-61"
#define W_SEED_MLIR0_WINDOWS_SCHEMA_VERSION "w-seed-mlir0-windows-47"
#define W_SEED_MLIR0_TARGET_TRIPLE_LINUX "x86_64-unknown-linux-gnu"
#define W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS "x86_64-pc-windows-msvc"
#define W_SEED_MLIR0_PROCESS_SCHEMA_VERSION \
  "w-seed-mlir0-process-handler-1"
#define W_SEED_MLIR0_PROCESS_EXECUTABLE_SCHEMA_VERSION \
  "w-seed-mlir0-process-executable-6"
/* Target-neutral M2 cooperative product emission is a scalar MLIR core.  It
 * has no target triple, data-layout, runtime, or process-entry contract. */
#define W_SEED_MLIR0_COOPERATIVE_SCHEMA_VERSION \
  "w-seed-mlir0-cooperative-2"
#define W_SEED_MLIR0_COOPERATIVE_EXECUTABLE_SCHEMA_VERSION \
  "w-seed-mlir0-cooperative-executable-2"
/* A linkable target-neutral task-entry module. It contains no process entry,
 * scheduler, provider, runtime ownership, or target ABI. */
#define W_SEED_MLIR0_PARALLEL_ENTRY_SCHEMA_VERSION \
  "w-seed-mlir0-parallel-entry-2"
#define W_SEED_MLIR1_PARALLEL_ENTRY_RESULT_SCHEMA_VERSION \
  "w-seed-mlir1-parallel-entry-result-1"
/* A composed process/parallel artifact is deliberately a separate contract
 * from both the ordinary process executable and the target-neutral task
 * entries.  It is linkable MLIR: the process adapter and provider own the
 * unresolved launch/join symbols, while the HIR task remains target-private. */
#define W_SEED_MLIR0_PROCESS_PARALLEL_SCHEMA_VERSION \
  "w-seed-mlir0-process-parallel-2"
/* Private compiler-lifecycle probes for one HIR40 synchronous typed invoke
 * and its HIR41 one-cleanup extension. The carrier is an optimizer-visible
 * two-field LLVM aggregate, not a W ABI: field 0 is an i1 outcome (0 =
 * success, 1 = the only Failure.denied case), and field 1 is the i64 normal
 * payload (zero on the error path). */
#define W_SEED_MLIR0_TYPED_PROPAGATION_SCHEMA_VERSION \
  "w-seed-mlir0-typed-propagation-1"
#define W_SEED_MLIR0_TYPED_CLEANUP_SCHEMA_VERSION \
  "w-seed-mlir0-typed-cleanup-1"
#define W_SEED_MLIR0_TYPED_PROPAGATION_CARRIER_FIELDS 2u
/* Private compiler-lifecycle artifact for the bounded integer exactly
 * conversion CFG. The outcome bit identifies NumericConversionError.outOfRange
 * and the destination payload is zero on that error arm; this is not a W ABI. */
#define W_SEED_MLIR0_INTEGER_EXACTLY_SCHEMA_VERSION \
  "w-seed-mlir0-integer-exactly-1"
#define W_SEED_MLIR0_INTEGER_EXACTLY_CARRIER_FIELDS 2u
/* Private compiler-lifecycle artifact for the bounded f32/f64-to-integer
 * rounding CFG. Status is a private i2: 0 success, 1 nonFinite, 2 outOfRange.
 * The conversion is emitted only in the finite, proven-in-range block. */
#define W_SEED_MLIR0_FLOAT_TO_INTEGER_ROUNDING_SCHEMA_VERSION \
  "w-seed-mlir0-float-to-integer-rounding-1"
#define W_SEED_MLIR0_FLOAT_TO_INTEGER_ROUNDING_CARRIER_FIELDS 2u
#define W_SEED_MLIR0_FLOAT_TO_INTEGER_ROUNDING_OUTCOME_BITS 2u
/* The unsuffixed aliases retain the byte-for-byte Linux seed contract. */
#define W_SEED_MLIR0_TARGET_TRIPLE W_SEED_MLIR0_TARGET_TRIPLE_LINUX
/* The dynamic seed artifact covers the bounded NativeSubset0 value and
 * interpolation tables, 4096 output bytes, and the fixed LLVM-dialect
 * skeleton. This is compiler workspace, not generated executable payload. */
#define W_SEED_MLIR0_MAX_BYTES 262144u

/* These are bounded seed evidence leaves, not the W target universe.  The
 * platform catalog owns the open target matrix; adding local evidence must not
 * exclude another applicable LLVM target. */
typedef enum {
  W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU = 0,
  W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC = 1,
  W_SEED_MLIR0_TARGET_UNSUPPORTED = 2,
} w_seed_mlir0_target_kind;

typedef struct {
  w_seed_mlir0_target_kind kind;
} w_seed_mlir0_target;

/* The executable artifact remains the zero value so existing callers and
 * bytes are unchanged. The handler artifact is a private opaque-owner
 * handler for supported targets; it has no CRT entry point or I/O. The
 * process executable is a separate public adapter so the private handler
 * contract remains byte-compatible. */
typedef enum {
  W_SEED_MLIR0_ARTIFACT_EXECUTABLE = 0,
  W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER = 1,
  W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE = 2,
  /* Target-specific process projection of the separately verified,
   * target-neutral cooperative core. */
  W_SEED_MLIR0_ARTIFACT_COOPERATIVE_EXECUTABLE = 3,
} w_seed_mlir0_artifact_kind;

typedef struct {
  const w_seed_hir0_program *program;
  const w_seed_hir0_result *hir_result;
  w_seed_mlir0_artifact_kind artifact_kind;
} w_seed_mlir0_input;

typedef enum {
  W_SEED_MLIR0_OK = 0,
  W_SEED_MLIR0_UNSUPPORTED,
  W_SEED_MLIR0_INVALID_HIR,
  W_SEED_MLIR0_CAPACITY,
  W_SEED_MLIR0_ALIAS,
} w_seed_mlir0_status;

typedef struct {
  size_t mlir_bytes;
} w_seed_mlir0_counts;

typedef struct {
  w_seed_mlir0_status status;
  w_seed_mlir0_counts required;
  w_seed_mlir0_counts written;
  uint8_t mlir_sha256[32];
} w_seed_mlir0_result;

typedef struct {
  uint8_t *bytes;
  size_t capacity;
} w_seed_mlir0_output;

/* M2 keeps the fixed state-machine facts in a separate record from the
 * target-sensitive MLIR0 artifact result.  The record is caller-owned and
 * contains no frame address, scheduler handle, or ABI field. */
typedef struct {
  size_t mlir_bytes;
  uint32_t frame_count;
  uint32_t queue_capacity;
  uint32_t yield_count;
  int64_t result_value;
} w_seed_mlir0_cooperative_counts;

typedef struct {
  w_seed_mlir0_status status;
  w_seed_mlir0_cooperative_counts required;
  w_seed_mlir0_cooperative_counts written;
  uint8_t mlir_sha256[32];
} w_seed_mlir0_cooperative_result;

typedef struct {
  uint8_t *bytes;
  size_t capacity;
} w_seed_mlir0_cooperative_output;

typedef struct {
  size_t mlir_bytes;
  uint32_t task_count;
  uint32_t runtime_argument_count;
  uint32_t reachable_function_count;
} w_seed_mlir0_parallel_entry_counts;

typedef struct {
  w_seed_mlir0_status status;
  w_seed_mlir0_parallel_entry_counts required;
  w_seed_mlir0_parallel_entry_counts written;
  uint8_t hir_semantic_digest[32];
  uint8_t mlir_sha256[32];
} w_seed_mlir0_parallel_entry_result;

typedef struct {
  uint8_t *bytes;
  size_t capacity;
} w_seed_mlir0_parallel_entry_output;

/* PARMLIR1 preserves the target-neutral MLIR text contract above while
 * replacing PARSEL0/PARINV0's fixed storage with measured caller-owned
 * relations. The extra digests bind the artifact to both producer proofs. */
typedef struct {
  w_seed_mlir0_status status;
  w_seed_mlir0_parallel_entry_counts required;
  w_seed_mlir0_parallel_entry_counts written;
  char schema[sizeof(W_SEED_MLIR1_PARALLEL_ENTRY_RESULT_SCHEMA_VERSION)];
  uint8_t hir_semantic_digest[32];
  uint8_t selection_semantic_digest[32];
  uint8_t invocation_semantic_digest[32];
  uint8_t mlir_sha256[32];
} w_seed_mlir1_parallel_entry_result;

typedef struct {
  size_t mlir_bytes;
  uint32_t task_count;
  uint32_t root_function_index;
  uint32_t runtime_argument_count;
  uint32_t reachable_function_count;
  uint32_t launch_count;
  uint32_t join_count;
} w_seed_mlir0_process_parallel_counts;

typedef struct {
  w_seed_mlir0_status status;
  w_seed_mlir0_process_parallel_counts required;
  w_seed_mlir0_process_parallel_counts written;
  uint8_t hir_semantic_digest[32];
  uint8_t mlir_sha256[32];
} w_seed_mlir0_process_parallel_result;

typedef struct {
  uint8_t *bytes;
  size_t capacity;
} w_seed_mlir0_process_parallel_output;

typedef struct {
  size_t mlir_bytes;
  /* The private artifact materializes leaf and relay, plus clean only for the
   * HIR41 variant. The inline entry body is not emitted as a process root. */
  uint32_t function_count;
  uint32_t invoke_count;
  /* Zero for the HIR40 artifact and one for the HIR41 cleanup artifact. */
  uint32_t cleanup_count;
  /* The carrier is exactly {i1 outcome, i64 payload}. */
  uint32_t carrier_field_count;
} w_seed_mlir0_typed_propagation_counts;

typedef struct {
  w_seed_mlir0_status status;
  w_seed_mlir0_typed_propagation_counts required;
  w_seed_mlir0_typed_propagation_counts written;
  uint8_t hir_semantic_digest[32];
  uint8_t mlir_sha256[32];
} w_seed_mlir0_typed_propagation_result;

typedef struct {
  uint8_t *bytes;
  size_t capacity;
} w_seed_mlir0_typed_propagation_output;

typedef struct {
  size_t mlir_bytes;
  uint16_t source_bit_width;
  uint16_t destination_bit_width;
  uint32_t representability_predicate_count;
  uint32_t typed_branch_count;
  uint32_t carrier_field_count;
  bool source_is_signed;
  bool destination_is_signed;
} w_seed_mlir0_integer_exactly_counts;

typedef struct {
  w_seed_mlir0_status status;
  w_seed_mlir0_integer_exactly_counts required;
  w_seed_mlir0_integer_exactly_counts written;
  uint8_t hir_semantic_digest[32];
  uint8_t mlir_sha256[32];
} w_seed_mlir0_integer_exactly_result;

typedef struct {
  uint8_t *bytes;
  size_t capacity;
} w_seed_mlir0_integer_exactly_output;

typedef struct {
  size_t mlir_bytes;
  uint16_t source_bit_width;
  uint16_t destination_bit_width;
  uint32_t range_predicate_count;
  uint32_t typed_branch_count;
  uint32_t outcome_count;
  uint32_t carrier_field_count;
  uint32_t outcome_bit_width;
  bool destination_is_signed;
  w_seed_hir0_rounding_mode rounding_mode;
} w_seed_mlir0_float_to_integer_rounding_counts;

typedef struct {
  w_seed_mlir0_status status;
  w_seed_mlir0_float_to_integer_rounding_counts required;
  w_seed_mlir0_float_to_integer_rounding_counts written;
  uint8_t hir_semantic_digest[32];
  uint8_t mlir_sha256[32];
} w_seed_mlir0_float_to_integer_rounding_result;

typedef struct {
  uint8_t *bytes;
  size_t capacity;
} w_seed_mlir0_float_to_integer_rounding_output;

/* Return true only for the explicit Linux or Windows target schemas. */
bool w_seed_mlir0_target_is_supported(const w_seed_mlir0_target *target);

/* Measure one deterministic MLIR artifact without writing caller-owned output. */
w_seed_mlir0_status w_seed_mlir0_measure(
    const w_seed_mlir0_input *input, const w_seed_mlir0_target *target,
    w_seed_mlir0_counts *counts, w_seed_mlir0_result *result);

/* Emit one exact MLIR artifact. Every failure leaves caller-owned output and
 * result records unchanged. The bytes have no implicit NUL terminator. */
w_seed_mlir0_status w_seed_mlir0_emit(
    const w_seed_mlir0_input *input, const w_seed_mlir0_target *target,
    const w_seed_mlir0_output *output, w_seed_mlir0_result *result);

/* Target-neutral M1 boundary for a verified HIR0 cooperative selection. The
 * selector admits only its narrower one-block subset and records admission
 * facts; no MLIR state machine is emitted. */
w_seed_mlir0_status w_seed_mlir0_select_cooperative(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    w_seed_cooperative_selection0 *selection);

bool w_seed_mlir0_verify_cooperative_selection(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative_selection0 *selection);

/* M2 target-neutral product core.  The emitter consumes a separately
 * verified HIR0 program and selection proof.  It emits scalar structured
 * control flow only.  No WRT or host process adapter is selected here. */
w_seed_mlir0_status w_seed_mlir0_measure_cooperative(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative_selection0 *selection,
    w_seed_mlir0_cooperative_counts *counts,
    w_seed_mlir0_cooperative_result *result);

w_seed_mlir0_status w_seed_mlir0_emit_cooperative(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative_selection0 *selection,
    const w_seed_mlir0_cooperative_output *output,
    w_seed_mlir0_cooperative_result *result);

/* Recheck a complete emitted core without changing caller-owned storage. */
bool w_seed_mlir0_verify_cooperative_emission(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative_selection0 *selection, const uint8_t *artifact,
    size_t artifact_bytes, const w_seed_mlir0_cooperative_result *result);

/* Emit one target-neutral, linkable function per verified parallel task. Each
 * entry accepts its task's signed-i64 arguments in declaration order; no
 * source argument is baked into the wrapper. Only the task functions and
 * their transitive local helper closure are materialized. Entry names are
 * canonical `w_seed_parallel_task_<ordinal>` symbols. */
w_seed_mlir0_status w_seed_mlir0_measure_parallel_entries(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_parallel_invocation0_plan *invocation,
    w_seed_mlir0_parallel_entry_counts *counts,
    w_seed_mlir0_parallel_entry_result *result);

w_seed_mlir0_status w_seed_mlir0_emit_parallel_entries(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_parallel_invocation0_plan *invocation,
    const w_seed_mlir0_parallel_entry_output *output,
    w_seed_mlir0_parallel_entry_result *result);

bool w_seed_mlir0_verify_parallel_entries(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_parallel_invocation0_plan *invocation,
    const uint8_t *artifact, size_t artifact_bytes,
    const w_seed_mlir0_parallel_entry_result *result);

/* Measured counterpart of the compatibility entry points above. Compatible
 * inputs produce byte-identical MLIR; task and argument counts are supplied by
 * PARSEL1/PARINV1 rather than bounded arrays in the proof records. */
w_seed_mlir0_status w_seed_mlir1_measure_parallel_entries(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_program *invocation,
    const w_seed_parallel_invocation1_result *invocation_result,
    w_seed_mlir0_parallel_entry_counts *counts,
    w_seed_mlir1_parallel_entry_result *result);

w_seed_mlir0_status w_seed_mlir1_emit_parallel_entries(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_program *invocation,
    const w_seed_parallel_invocation1_result *invocation_result,
    const w_seed_mlir0_parallel_entry_output *output,
    w_seed_mlir1_parallel_entry_result *result);

bool w_seed_mlir1_verify_parallel_entries(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_program *invocation,
    const w_seed_parallel_invocation1_result *invocation_result,
    const uint8_t *artifact, size_t artifact_bytes,
    const w_seed_mlir1_parallel_entry_result *result);

/* Compose the verified W-1595 process root with the verified W-1596
 * PARSEL0/PARINV0 task relation.  The artifact is intentionally linkable,
 * not executable yet: the provider implements the target-specific
 * launch/join symbols.  The root contains no baked prelude or task value. */
w_seed_mlir0_status w_seed_mlir0_measure_process_parallel(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_mlir0_target *target,
    w_seed_mlir0_process_parallel_counts *counts,
    w_seed_mlir0_process_parallel_result *result);

w_seed_mlir0_status w_seed_mlir0_emit_process_parallel(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_mlir0_target *target,
    const w_seed_mlir0_process_parallel_output *output,
    w_seed_mlir0_process_parallel_result *result);

bool w_seed_mlir0_verify_process_parallel(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_mlir0_target *target, const uint8_t *artifact,
    size_t artifact_bytes, const w_seed_mlir0_process_parallel_result *result);

/* Emit either the exact private `leaf`/`relay` HIR40 propagation witness or
 * its HIR41 `clean` extension. Both artifacts use the private two-field
 * carrier described above and have no unwind edge, Task, heap, process root,
 * or public ABI. */
w_seed_mlir0_status w_seed_mlir0_measure_typed_propagation(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_mlir0_target *target,
    w_seed_mlir0_typed_propagation_counts *counts,
    w_seed_mlir0_typed_propagation_result *result);

w_seed_mlir0_status w_seed_mlir0_emit_typed_propagation(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_mlir0_target *target,
    const w_seed_mlir0_typed_propagation_output *output,
    w_seed_mlir0_typed_propagation_result *result);

bool w_seed_mlir0_verify_typed_propagation(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_mlir0_target *target, const uint8_t *artifact,
    size_t artifact_bytes,
    const w_seed_mlir0_typed_propagation_result *result);

/* Emit one private LLVM-dialect conversion function from the closed HIR
 * exactly-conversion CFG. Range predicates use the logical source type before
 * truncation/extension, and both typed outcomes remain explicit control-flow
 * successors. No process root, public error ABI, heap, or CRT is implied. */
w_seed_mlir0_status w_seed_mlir0_measure_integer_exactly(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_mlir0_target *target,
    w_seed_mlir0_integer_exactly_counts *counts,
    w_seed_mlir0_integer_exactly_result *result);

w_seed_mlir0_status w_seed_mlir0_emit_integer_exactly(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_mlir0_target *target,
    const w_seed_mlir0_integer_exactly_output *output,
    w_seed_mlir0_integer_exactly_result *result);

bool w_seed_mlir0_verify_integer_exactly(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_mlir0_target *target, const uint8_t *artifact,
    size_t artifact_bytes,
    const w_seed_mlir0_integer_exactly_result *result);

w_seed_mlir0_status w_seed_mlir0_measure_float_to_integer_rounding(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_mlir0_target *target,
    w_seed_mlir0_float_to_integer_rounding_counts *counts,
    w_seed_mlir0_float_to_integer_rounding_result *result);

w_seed_mlir0_status w_seed_mlir0_emit_float_to_integer_rounding(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_mlir0_target *target,
    const w_seed_mlir0_float_to_integer_rounding_output *output,
    w_seed_mlir0_float_to_integer_rounding_result *result);

bool w_seed_mlir0_verify_float_to_integer_rounding(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_mlir0_target *target, const uint8_t *artifact,
    size_t artifact_bytes,
    const w_seed_mlir0_float_to_integer_rounding_result *result);

#ifdef __cplusplus
}
#endif

#endif
