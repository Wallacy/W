#ifndef W_SEED_NATIVE_SUBSET0_H
#define W_SEED_NATIVE_SUBSET0_H

#include <stddef.h>
#include <stdint.h>

#include "w_seed_hir0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* These bounds define the private native sequence subset. */
#define W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD 256u
#define W_SEED_NATIVE_SUBSET0_MAX_INSTRUCTIONS 32u
#define W_SEED_NATIVE_SUBSET0_MAX_CALLS 32u
#define W_SEED_NATIVE_SUBSET0_MAX_BINDINGS 32u
#define W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES 4096u
#define W_SEED_NATIVE_SUBSET0_MAX_VALUES 64u
#define W_SEED_NATIVE_SUBSET0_MAX_INTERPOLATION_SEGMENTS 64u

typedef enum {
  W_SEED_NATIVE_SUBSET0_OK = 0,
  W_SEED_NATIVE_SUBSET0_UNSUPPORTED,
  W_SEED_NATIVE_SUBSET0_INVALID,
} w_seed_native_subset0_status;

typedef struct {
  const w_seed_hir0_entry *entry;
  const w_seed_hir0_function *function;
  const w_seed_hir0_block *block;
  const w_seed_hir0_instruction *instruction;
  const w_seed_hir0_binding *binding;
  const w_seed_hir0_call *call;
  const w_seed_hir0_identity *callee;
  const w_seed_hir0_requirement *requirement;
  const w_seed_hir0_argument *argument;
  const w_seed_hir0_value *value;
  const uint8_t *payload;
  size_t payload_bytes;
} w_seed_native_subset0_selection;

/* This private selector is the single boundary for the native print subset.
 * It consumes only a verified HIR0 program and exposes borrowed record and
 * payload views. It does not perform textual lookup or copy caller storage. */
typedef struct {
  const w_seed_hir0_instruction *instruction;
  const w_seed_hir0_call *call;
  const w_seed_hir0_identity *callee;
  const w_seed_hir0_requirement *requirement;
  const w_seed_hir0_argument *argument;
  const w_seed_hir0_value *value;
  const uint8_t *payload;
  size_t payload_bytes;
  bool is_interpolated;
} w_seed_native_subset0_call_selection;

typedef struct {
  const w_seed_hir0_entry *entry;
  const w_seed_hir0_function *function;
  const w_seed_hir0_block *block;
  const w_seed_hir0_binding *bindings[
      W_SEED_NATIVE_SUBSET0_MAX_BINDINGS];
  w_seed_native_subset0_call_selection calls[
      W_SEED_NATIVE_SUBSET0_MAX_CALLS];
  size_t instruction_count;
  size_t binding_count;
  size_t call_count;
  size_t stdout_bytes;
  size_t maximum_stdout_bytes;
  bool has_interpolation;
} w_seed_native_subset0_sequence;

w_seed_native_subset0_status w_seed_native_subset0_select(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_selection *selection);

/* Select the bounded linear sequence used only by MLIR0. HLO0, HLO1 and RUN0
 * retain the single-print selector above. */
w_seed_native_subset0_status w_seed_native_subset0_select_sequence(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_sequence *sequence);

#ifdef __cplusplus
}
#endif

#endif
