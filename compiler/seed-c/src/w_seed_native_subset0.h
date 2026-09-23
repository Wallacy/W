#ifndef W_SEED_NATIVE_SUBSET0_H
#define W_SEED_NATIVE_SUBSET0_H

#include <stddef.h>
#include <stdint.h>

#include "w_seed_cooperative_selection0.h"
#include "w_seed_hir0.h"
#include "w_seed_parallel_selection0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* These bounds define the private native sequence subset. */
#define W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD 256u
#define W_SEED_NATIVE_SUBSET0_MAX_INSTRUCTIONS 128u
#define W_SEED_NATIVE_SUBSET0_MAX_CALLS 32u
#define W_SEED_NATIVE_SUBSET0_MAX_BINDINGS 128u
#define W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES 4096u
#define W_SEED_NATIVE_SUBSET0_MAX_VALUES 512u
#define W_SEED_NATIVE_SUBSET0_MAX_INTERPOLATION_SEGMENTS 128u
#define W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS 16u
#define W_SEED_NATIVE_SUBSET0_MAX_MODULES 32u
#define W_SEED_NATIVE_SUBSET0_MAX_PARAMETERS 32u
/* Matches Native0's 16 functions + 3 blocks per 128 statements. */
#define W_SEED_NATIVE_SUBSET0_MAX_BLOCKS 400u

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
  size_t module_count;
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
  bool has_enum_switch;
  bool has_mutable_bindings;
  /* True only when the selected entry call graph contains a verified panic
   * terminator.  Panic text remains HIR evidence; it is not an output plan. */
  bool has_reachable_panic;
  bool natural_loop_functions[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS];
  /* Verified bounded i64-carrier CFG facts for cyclic typed LLVM CFG
   * emission. Keep separate from the structured natural-loop projection. */
  bool verified_i64_loop_cfg_functions[
      W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS];
  /* Verified five-block post-test repeat facts.  This is intentionally
   * separate from the pre-test natural-loop projection consumed by MLIR0. */
  bool post_test_loop_functions[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS];
} w_seed_native_subset0_program;

/* The process handler selection is deliberately separate from the executable
 * print subset. Its borrowed parameters describe the already-verified root
 * owners; the emitter uses the cleanup range in reverse initialization order. */
typedef struct {
  const w_seed_hir0_entry *entry;
  const w_seed_hir0_function *function;
  const w_seed_hir0_parameter *arguments_parameter;
  const w_seed_hir0_parameter *context_parameter;
  /* Dense HIR indices are carried explicitly so the MLIR adapter can keep
   * the source-declared entry/parameter order without assuming ordinal zero
   * or a particular external-symbol layout. */
  uint32_t function_index;
  uint32_t arguments_symbol_index;
  uint32_t context_symbol_index;
  uint32_t exit_code_symbol_index;
  uint32_t is_empty_symbol_index;
  uint32_t count_symbol_index;
  uint32_t success_symbol_index;
  uint32_t failure_symbol_index;
  uint32_t arguments_parameter_ordinal;
  uint32_t context_parameter_ordinal;
  size_t maximum_stdout_bytes;
  /* The restricted native-process typed-error adapter admits one verified
   * integer-exactly split.  These are borrowed HIR facts; the adapter maps
   * the NumericConversionError arm to private status 1 and never exposes the
   * error payload as a process ABI. */
  const w_seed_hir0_value *exact_source_value;
  const w_seed_hir0_terminator *exact_conversion;
  const w_seed_hir0_terminator *exact_normal_return;
  const w_seed_hir0_terminator *exact_error_throw;
  uint32_t exact_split_block_index;
  uint32_t exact_normal_block_index;
  uint32_t exact_error_block_index;
  uint32_t exact_source_type_index;
  uint32_t exact_destination_type_index;
  uint32_t exact_error_type_index;
  uint16_t exact_source_bit_width;
  uint16_t exact_destination_bit_width;
  bool exact_source_is_signed;
  bool exact_destination_is_signed;
  /* Logical usize has target-owned width.  NativeSubset0 preserves that fact
   * instead of pretending it is a portable u64; MLIR0 resolves it only after
   * accepting a concrete target. */
  bool exact_source_is_target_usize;
  bool has_integer_exactly;
  /* The bounded native-process rounding root is a separate typed-error
   * relation.  These borrowed HIR facts preserve the source constant, the
   * three typed successor roles, and the closed rounding/type facts without
   * implying that the process emitter can execute this route yet. */
  const w_seed_hir0_value *rounding_source_value;
  const w_seed_hir0_terminator *rounding_conversion;
  const w_seed_hir0_terminator *rounding_normal_return;
  const w_seed_hir0_terminator *rounding_non_finite_throw;
  const w_seed_hir0_terminator *rounding_out_of_range_throw;
  uint32_t rounding_split_block_index;
  uint32_t rounding_normal_block_index;
  uint32_t rounding_non_finite_block_index;
  uint32_t rounding_out_of_range_block_index;
  uint32_t rounding_source_type_index;
  uint32_t rounding_destination_type_index;
  uint32_t rounding_error_type_index;
  uint16_t rounding_source_bit_width;
  uint16_t rounding_destination_bit_width;
  bool rounding_destination_is_signed;
  w_seed_hir0_rounding_mode rounding_mode;
  bool has_float_to_integer_rounding;
  /* True only when the selected process entry call graph contains a verified
   * panic terminator.  The message is validated but never emitted. */
  bool has_reachable_panic;
  bool natural_loop_functions[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS];
  bool post_test_loop_functions[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS];
} w_seed_native_subset0_process;

/* Private compiler-lifecycle selector for the exact HIR40 synchronous typed
 * propagation witness and its HIR41 one-cleanup extension. The optional
 * cleanup fields are all-null/all-NONE for HIR40. The selector publishes
 * borrowed records only. It does not define a public throwing ABI or a
 * process exit policy. */
typedef struct {
  const w_seed_hir0_entry *entry;
  const w_seed_hir0_function *leaf_function;
  const w_seed_hir0_function *relay_function;
  const w_seed_hir0_terminator *leaf_throw;
  const w_seed_hir0_terminator *invoke;
  const w_seed_hir0_terminator *normal_return;
  const w_seed_hir0_terminator *error_throw;
  const w_seed_hir0_cleanup *cleanup;
  const w_seed_hir0_function *cleanup_function;
  const w_seed_hir0_call *normal_cleanup_call;
  const w_seed_hir0_call *error_cleanup_call;
  uint32_t entry_function_index;
  uint32_t leaf_function_index;
  uint32_t relay_function_index;
  uint32_t cleanup_function_index;
  uint32_t invoke_block_index;
  uint32_t normal_block_index;
  uint32_t error_block_index;
  uint32_t error_type_index;
  uint32_t error_enum_index;
  uint32_t error_case_index;
} w_seed_native_subset0_typed_propagation;

/* Private NativeSubset0 admission for the bounded `try D(exactly: source)`
 * integer conversion CFG.  This record keeps the source's logical width and
 * signedness separate from the destination until MLIR has emitted the range
 * predicates.  It is not a call selection and does not define a root ABI. */
typedef struct {
  const w_seed_hir0_entry *entry;
  const w_seed_hir0_function *function;
  const w_seed_hir0_parameter *source_parameter;
  const w_seed_hir0_value *source_value;
  const w_seed_hir0_terminator *conversion;
  const w_seed_hir0_block_argument *normal_argument;
  const w_seed_hir0_block_argument *error_argument;
  const w_seed_hir0_terminator *normal_return;
  const w_seed_hir0_terminator *error_throw;
  uint32_t entry_index;
  uint32_t function_index;
  uint32_t entry_function_index;
  uint32_t split_block_index;
  uint32_t normal_block_index;
  uint32_t error_block_index;
  uint32_t source_type_index;
  uint32_t destination_type_index;
  uint32_t error_type_index;
  uint16_t source_bit_width;
  uint16_t destination_bit_width;
  bool source_is_signed;
  bool destination_is_signed;
} w_seed_native_subset0_integer_exactly;

/* Private NativeSubset0 admission for the bounded
 * `try D(rounding: source, mode: .policy)` CFG. The three successors retain
 * their semantic roles: normal payload, NumericConversionError.nonFinite,
 * and NumericConversionError.outOfRange. This record is not a root ABI. */
typedef struct {
  const w_seed_hir0_entry *entry;
  const w_seed_hir0_function *function;
  const w_seed_hir0_parameter *source_parameter;
  const w_seed_hir0_value *source_value;
  const w_seed_hir0_terminator *conversion;
  const w_seed_hir0_block_argument *normal_argument;
  const w_seed_hir0_block_argument *non_finite_argument;
  const w_seed_hir0_block_argument *out_of_range_argument;
  const w_seed_hir0_terminator *normal_return;
  const w_seed_hir0_terminator *non_finite_throw;
  const w_seed_hir0_terminator *out_of_range_throw;
  uint32_t entry_index;
  uint32_t function_index;
  uint32_t entry_function_index;
  uint32_t split_block_index;
  uint32_t normal_block_index;
  uint32_t non_finite_block_index;
  uint32_t out_of_range_block_index;
  uint32_t source_type_index;
  uint32_t destination_type_index;
  uint32_t error_type_index;
  uint16_t source_bit_width;
  uint16_t destination_bit_width;
  bool destination_is_signed;
  w_seed_hir0_rounding_mode rounding_mode;
} w_seed_native_subset0_float_to_integer_rounding;

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

/* Select the same complete process-input witness while admitting exactly one
 * independently verified PARSEL0 parallel dispatch in its root body. The
 * ordinary process-executable selector remains direct-call-only. */
w_seed_native_subset0_status
w_seed_native_subset0_select_process_parallel(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *parallel_selection,
    w_seed_native_subset0_process *selection);

/* Select only the exact private one-case typed propagation witness, with
 * either no cleanup (HIR40) or the one proven cleanup (HIR41). This route is
 * separate from select_program and cannot create an executable. */
w_seed_native_subset0_status
w_seed_native_subset0_select_typed_propagation(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_typed_propagation *selection);

/* Re-derive the private selection from verified HIR without trusting copied
 * pointers or indices in the supplied selection. */
bool w_seed_native_subset0_verify_typed_propagation(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const w_seed_native_subset0_typed_propagation *selection);

/* Select one closed conversion-only HIR function and its neutral empty entry.
 * The selected CFG still branches to typed success and error blocks. */
w_seed_native_subset0_status
w_seed_native_subset0_select_integer_exactly(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_integer_exactly *selection);

/* Independently rederive the exact-conversion admission from verified HIR. */
bool w_seed_native_subset0_verify_integer_exactly(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const w_seed_native_subset0_integer_exactly *selection);

/* Select one closed float-to-integer conversion function and its neutral
 * empty entry. The selected HIR keeps all three typed successors explicit. */
w_seed_native_subset0_status
w_seed_native_subset0_select_float_to_integer_rounding(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_float_to_integer_rounding *selection);

/* Independently rederive every field of the bounded rounding selection. */
bool w_seed_native_subset0_verify_float_to_integer_rounding(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const w_seed_native_subset0_float_to_integer_rounding *selection);

/* Select the target-neutral closed cooperative product shape. This is only
 * an admission record for a future emitter; it does not emit or execute. */
w_seed_native_subset0_status w_seed_native_subset0_select_cooperative(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_cooperative_selection0 *selection);

/* Independently rederive every cooperative selection field from verified HIR.
 * No selector-produced pointer or field is trusted. */
bool w_seed_native_subset0_verify_cooperative(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const w_seed_cooperative_selection0 *selection);

#ifdef __cplusplus
}
#endif

#endif
