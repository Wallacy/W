#ifndef W_SEED_MLIR0_H
#define W_SEED_MLIR0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_cooperative_selection0.h"
#include "w_seed_hir0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal seed-only MLIR0 terminal adapter. It consumes a verified HIR0
 * program, selects the fixed native print subset, and emits textual builtin
 * plus LLVM dialect MLIR for one fixed target. The artifact is recipe-private
 * and does not allocate or execute. */
#define W_SEED_MLIR0_SCHEMA_VERSION "w-seed-mlir0-17"
#define W_SEED_MLIR0_WINDOWS_SCHEMA_VERSION "w-seed-mlir0-windows-8"
#define W_SEED_MLIR0_TARGET_TRIPLE_LINUX "x86_64-unknown-linux-gnu"
#define W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS "x86_64-pc-windows-msvc"
#define W_SEED_MLIR0_PROCESS_SCHEMA_VERSION \
  "w-seed-mlir0-process-handler-1"
#define W_SEED_MLIR0_PROCESS_EXECUTABLE_SCHEMA_VERSION \
  "w-seed-mlir0-process-executable-1"
/* Target-neutral M2 cooperative product emission is a scalar MLIR core.  It
 * has no target triple, data-layout, runtime, or process-entry contract. */
#define W_SEED_MLIR0_COOPERATIVE_SCHEMA_VERSION \
  "w-seed-mlir0-cooperative-1"
#define W_SEED_MLIR0_COOPERATIVE_EXECUTABLE_SCHEMA_VERSION \
  "w-seed-mlir0-cooperative-executable-1"
/* The unsuffixed aliases retain the byte-for-byte Linux seed contract. */
#define W_SEED_MLIR0_TARGET_TRIPLE W_SEED_MLIR0_TARGET_TRIPLE_LINUX
/* The dynamic seed artifact is bounded by 64 HIR values, 64 interpolation
 * segments, 4096 output bytes, and the fixed LLVM-dialect skeleton. */
#define W_SEED_MLIR0_MAX_BYTES 196608u

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

/* Target-neutral M1 boundary for a verified HIR34 cooperative selection. The
 * selector admits only its narrower one-block subset and records admission
 * facts; no MLIR state machine is emitted. */
w_seed_mlir0_status w_seed_mlir0_select_cooperative(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    w_seed_cooperative_selection0 *selection);

bool w_seed_mlir0_verify_cooperative_selection(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_cooperative_selection0 *selection);

/* M2 target-neutral product core.  The emitter consumes a separately
 * verified HIR34 program and selection proof.  It emits scalar structured
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

#ifdef __cplusplus
}
#endif

#endif
