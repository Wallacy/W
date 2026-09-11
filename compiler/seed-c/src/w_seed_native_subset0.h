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
#define W_SEED_NATIVE_SUBSET0_MAX_VALUES 128u
#define W_SEED_NATIVE_SUBSET0_MAX_INTERPOLATION_SEGMENTS 64u
#define W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS 8u
#define W_SEED_NATIVE_SUBSET0_MAX_PARAMETERS 16u
/* Matches Native0's statement-derived HIR block capacity. */
#define W_SEED_NATIVE_SUBSET0_MAX_BLOCKS 392u

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

/* Bounded multi-function native subset. Source argument order remains in HIR;
 * parameter_ordinal supplies the declaration/ABI mapping. Unit functions may
 * contain the verified forward-only structured CFG, including nested
 * diamonds. The call graph is acyclic and branch output uses the maximum
 * mutually-exclusive path. */
typedef struct {
  const w_seed_hir0_entry *entry;
  size_t function_count;
  size_t parameter_count;
  size_t instruction_count;
  size_t binding_count;
  size_t call_count;
  size_t maximum_stdout_bytes;
  bool has_interpolation;
  bool has_bool;
  bool has_local_calls;
  bool has_cfg;
  bool has_mutable_bindings;
} w_seed_native_subset0_program;

/* The process handler selection is deliberately separate from the executable
 * print subset. Its borrowed parameters describe the already-verified root
 * owners; the emitter uses the cleanup range in reverse initialization order. */
typedef struct {
  const w_seed_hir0_entry *entry;
  const w_seed_hir0_function *function;
  const w_seed_hir0_parameter *arguments_parameter;
  const w_seed_hir0_parameter *context_parameter;
  uint32_t arguments_parameter_ordinal;
  uint32_t context_parameter_ordinal;
} w_seed_native_subset0_process;

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

w_seed_native_subset0_status w_seed_native_subset0_select_program(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_program *selection);

/* Select only the complete HIR16 native-process owner handler. This does not
 * inspect source spelling and never admits an unverified or partial HIR. */
w_seed_native_subset0_status w_seed_native_subset0_select_process(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_process *selection);

/* Select only the complete public process-input witness. This is a separate
 * boundary from the private owner-only handler because the public witness
 * carries the verified Arguments.isEmpty branch and print calls. */
w_seed_native_subset0_status
w_seed_native_subset0_select_process_executable(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_process *selection);

#ifdef __cplusplus
}
#endif

#endif
