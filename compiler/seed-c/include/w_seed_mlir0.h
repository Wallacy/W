#ifndef W_SEED_MLIR0_H
#define W_SEED_MLIR0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_hir0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal seed-only MLIR0 terminal adapter. It consumes a verified HIR0
 * program, selects the fixed native print subset, and emits textual builtin
 * plus LLVM dialect MLIR for one fixed target. The artifact is recipe-private
 * and does not allocate or execute. */
#define W_SEED_MLIR0_SCHEMA_VERSION "w-seed-mlir0-8"
#define W_SEED_MLIR0_TARGET_TRIPLE "x86_64-unknown-linux-gnu"
/* The dynamic seed artifact is bounded by 64 HIR values, 64 interpolation
 * segments, 4096 output bytes, and the fixed LLVM-dialect skeleton. */
#define W_SEED_MLIR0_MAX_BYTES 196608u

typedef enum {
  W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU = 0,
  W_SEED_MLIR0_TARGET_UNSUPPORTED = 1,
} w_seed_mlir0_target_kind;

typedef struct {
  w_seed_mlir0_target_kind kind;
} w_seed_mlir0_target;

typedef struct {
  const w_seed_hir0_program *program;
  const w_seed_hir0_result *hir_result;
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

/* Return true only for the one target represented by this schema. */
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

#ifdef __cplusplus
}
#endif

#endif
