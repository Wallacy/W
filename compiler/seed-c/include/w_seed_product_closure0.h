#ifndef W_SEED_PRODUCT_CLOSURE0_H
#define W_SEED_PRODUCT_CLOSURE0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_hir0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ProductClosure0 is an internal, bounded projection over a verified HIR0
 * program.  It does not copy HIR records and never owns input storage.  The
 * caller owns every output array and receives source-index to dense-closure
 * remaps (W_SEED_PRODUCT_CLOSURE0_NONE for omitted records). */
#define W_SEED_PRODUCT_CLOSURE0_SCHEMA_VERSION "w-seed-product-closure0-7"
#define W_SEED_PRODUCT_CLOSURE0_NONE UINT32_MAX
#define W_SEED_PRODUCT_CLOSURE0_DIGEST_BYTES 32u
#define W_SEED_PRODUCT_CLOSURE0_MAX_MODULES 32u
#define W_SEED_PRODUCT_CLOSURE0_MAX_FUNCTIONS 128u
#define W_SEED_PRODUCT_CLOSURE0_MAX_IDENTITIES 256u
#define W_SEED_PRODUCT_CLOSURE0_MAX_TYPES 128u
#define W_SEED_PRODUCT_CLOSURE0_MAX_ENUMS 32u
#define W_SEED_PRODUCT_CLOSURE0_MAX_ENUM_CASES 512u
#define W_SEED_PRODUCT_CLOSURE0_MAX_VALUES 4096u
#define W_SEED_PRODUCT_CLOSURE0_MAX_REQUIREMENTS 512u
#define W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_MODULES 32u
#define W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_SYMBOLS 256u
#define W_SEED_PRODUCT_CLOSURE0_MAX_PARAMETERS 1024u
#define W_SEED_PRODUCT_CLOSURE0_MAX_BLOCKS 2048u
#define W_SEED_PRODUCT_CLOSURE0_MAX_BLOCK_ARGUMENTS 2048u
#define W_SEED_PRODUCT_CLOSURE0_MAX_EDGE_ARGUMENTS 4096u
#define W_SEED_PRODUCT_CLOSURE0_MAX_INSTRUCTIONS 4096u
#define W_SEED_PRODUCT_CLOSURE0_MAX_BINDINGS 2048u
#define W_SEED_PRODUCT_CLOSURE0_MAX_CALLS 2048u
#define W_SEED_PRODUCT_CLOSURE0_MAX_HOST_PARAMETERS 256u
#define W_SEED_PRODUCT_CLOSURE0_MAX_ARGUMENTS 4096u
#define W_SEED_PRODUCT_CLOSURE0_MAX_INTERPOLATION_SEGMENTS 4096u
#define W_SEED_PRODUCT_CLOSURE0_MAX_TERMINATORS 2048u
#define W_SEED_PRODUCT_CLOSURE0_MAX_DEPTH 256u

typedef enum {
  W_SEED_PRODUCT_CLOSURE0_OK = 0,
  W_SEED_PRODUCT_CLOSURE0_CAPACITY,
  W_SEED_PRODUCT_CLOSURE0_INVALID,
  W_SEED_PRODUCT_CLOSURE0_UNSUPPORTED,
} w_seed_product_closure0_status;

typedef enum {
  W_SEED_PRODUCT_CLOSURE0_FAILURE_NONE = 0,
  W_SEED_PRODUCT_CLOSURE0_FAILURE_POINTER,
  W_SEED_PRODUCT_CLOSURE0_FAILURE_HIR,
  W_SEED_PRODUCT_CLOSURE0_FAILURE_ROOT,
  W_SEED_PRODUCT_CLOSURE0_FAILURE_LIMIT,
  W_SEED_PRODUCT_CLOSURE0_FAILURE_INDEX,
  W_SEED_PRODUCT_CLOSURE0_FAILURE_OWNERSHIP,
  W_SEED_PRODUCT_CLOSURE0_FAILURE_UNSUPPORTED,
  W_SEED_PRODUCT_CLOSURE0_FAILURE_CYCLE,
} w_seed_product_closure0_failure;

typedef struct {
  const w_seed_hir0_program *program;
  const w_seed_hir0_result *hir_result;
} w_seed_product_closure0_input;

typedef enum {
  W_SEED_PRODUCT_CLOSURE0_OUTCOME_NONE = 0,
  W_SEED_PRODUCT_CLOSURE0_OUTCOME_NORMAL,
  W_SEED_PRODUCT_CLOSURE0_OUTCOME_TYPED_THROW,
} w_seed_product_closure0_outcome_kind;

/* Root identity and root outcome are separate facts. All indexes are source
 * HIR indexes, not dense closure ordinals. */
typedef struct {
  uint32_t entry_index;
  uint32_t module_index;
  uint32_t function_index;
  uint32_t identity_index;
  uint32_t target_identity_index;
  w_seed_hir0_entry_adapter_kind adapter_kind;
  w_seed_hir0_entry_cleanup_kind cleanup_obligation;
  uint32_t first_cleanup_owner_parameter;
  uint32_t cleanup_owner_parameter_count;
  /* Explicit verified release order. For the typed-error root this is
   * Context then Arguments, reverse of the HIR owner-parameter range. */
  uint32_t cleanup_release_parameter_count;
  uint32_t cleanup_release_parameters[2];
} w_seed_product_closure0_root;

/* A typed throw is an outcome fact, never a replacement for the entry's
 * normal return type or an alias for panic/status. */
typedef struct {
  w_seed_product_closure0_outcome_kind kind;
  /* The split terminator and its explicit successor are both published.  A
   * normal outcome names the target successor; a typed outcome names the
   * error successor. */
  uint32_t source_terminator_index;
  uint32_t terminator_index;
  uint32_t successor_block_index;
  uint32_t successor_argument_index;
  uint32_t successor_argument_count;
  uint32_t value_index;
  uint32_t result_type_index;
  uint32_t error_type_index;
  uint32_t error_enum_index;
  uint32_t error_case_index;
} w_seed_product_closure0_outcome;

typedef struct {
  size_t modules;
  size_t functions;
  size_t identities;
  size_t types;
  size_t values;
  size_t requirements;
  size_t external_modules;
  size_t external_symbols;
  size_t reachable_modules;
  size_t omitted_modules;
  size_t reachable_functions;
  size_t omitted_functions;
  size_t reachable_identities;
  size_t reachable_types;
  size_t reachable_values;
  size_t reachable_requirements;
  size_t reachable_external_modules;
  size_t reachable_external_symbols;
  size_t reachable_checked_fault_operations;
  /* Remap arrays are source-indexed, so these requirements are the complete
   * source array lengths rather than just the number of retained records. */
  size_t module_remap;
  size_t function_remap;
  size_t identity_remap;
  size_t type_remap;
  size_t value_remap;
  size_t requirement_remap;
  size_t external_module_remap;
  size_t external_symbol_remap;
} w_seed_product_closure0_counts;

typedef struct {
  uint32_t source_index;
  uint32_t closure_index;
  uint32_t owner_function;
  uint32_t type_index;
  w_seed_hir0_value_kind kind;
} w_seed_product_closure0_value_fact;

typedef struct {
  uint32_t source_index;
  uint32_t closure_index;
  uint32_t owner_module;
  w_seed_hir0_type_kind kind;
} w_seed_product_closure0_type_fact;

typedef struct {
  uint32_t source_index;
  uint32_t closure_index;
  uint32_t owner_identity;
} w_seed_product_closure0_requirement_fact;

/* A checked operation is an independently published arithmetic-fault edge
 * fact. Source indices are retained for adapter use; owner/type/operator are
 * re-derived from verified HIR during ProductClosure verification. */
typedef struct {
  uint32_t source_value_index;
  uint32_t owner_function_index;
  uint32_t type_index;
  w_seed_hir0_binary_operator binary_operator;
} w_seed_product_closure0_checked_fault_operation;

/* This relation is separate from NumericConversionError: the conversion's
 * typed failure is status 1, while each reachable checked integer operation
 * reached after the conversion's normal edge maps to status 2. */
typedef struct {
  bool present;
  uint32_t source_conversion_terminator_index;
  uint32_t normal_successor_block_index;
  uint32_t conversion_failure_status;
  uint32_t arithmetic_failure_status;
  size_t operation_count;
  uint8_t operation_digest[W_SEED_PRODUCT_CLOSURE0_DIGEST_BYTES];
} w_seed_product_closure0_checked_fault_relation;

typedef struct {
  /* Retained and omitted source indices are always in ascending declaration
   * order.  A non-empty projection requires a non-NULL array with sufficient
   * capacity; empty projections may use NULL/zero. */
  uint32_t *reachable_modules;
  size_t reachable_module_capacity;
  uint32_t *omitted_modules;
  size_t omitted_module_capacity;
  uint32_t *reachable_functions;
  size_t reachable_function_capacity;
  uint32_t *omitted_functions;
  size_t omitted_function_capacity;
  uint32_t *reachable_identities;
  size_t reachable_identity_capacity;
  uint32_t *reachable_types;
  size_t reachable_type_capacity;
  uint32_t *reachable_values;
  size_t reachable_value_capacity;
  uint32_t *reachable_requirements;
  size_t reachable_requirement_capacity;
  uint32_t *reachable_external_modules;
  size_t reachable_external_module_capacity;
  uint32_t *reachable_external_symbols;
  size_t reachable_external_symbol_capacity;

  /* Each remap is indexed by the corresponding source HIR index. */
  uint32_t *module_remap;
  size_t module_remap_capacity;
  uint32_t *function_remap;
  size_t function_remap_capacity;
  uint32_t *identity_remap;
  size_t identity_remap_capacity;
  uint32_t *type_remap;
  size_t type_remap_capacity;
  uint32_t *value_remap;
  size_t value_remap_capacity;
  uint32_t *requirement_remap;
  size_t requirement_remap_capacity;
  uint32_t *external_module_remap;
  size_t external_module_remap_capacity;
  uint32_t *external_symbol_remap;
  size_t external_symbol_remap_capacity;

  /* Optional caller-owned fact projections.  They are useful to adapters
   * which need the retained value/type/requirement kind without following a
   * source-index map. */
  w_seed_product_closure0_value_fact *value_facts;
  size_t value_fact_capacity;
  w_seed_product_closure0_type_fact *type_facts;
  size_t type_fact_capacity;
  w_seed_product_closure0_requirement_fact *requirement_facts;
  size_t requirement_fact_capacity;
  w_seed_product_closure0_checked_fault_operation *checked_fault_operations;
  size_t checked_fault_operation_capacity;
} w_seed_product_closure0_output;

typedef struct {
  w_seed_product_closure0_status status;
  w_seed_product_closure0_counts required;
  w_seed_product_closure0_counts written;
  w_seed_product_closure0_failure failure;
  w_seed_product_closure0_root root;
  /* The success and typed-error paths remain separate facts. `outcome` is
   * retained as the typed-error compatibility channel: numeric splits alias
   * it to out_of_range_outcome. Float rounding additionally publishes the
   * distinct non-finite path. */
  w_seed_product_closure0_outcome normal_outcome;
  w_seed_product_closure0_outcome outcome;
  w_seed_product_closure0_outcome non_finite_outcome;
  w_seed_product_closure0_outcome out_of_range_outcome;
  w_seed_product_closure0_checked_fault_relation checked_fault_relation;
  uint8_t reachable_semantic_digest[W_SEED_PRODUCT_CLOSURE0_DIGEST_BYTES];
} w_seed_product_closure0_result;

/* Measure a complete verified-HIR product closure without writing output.
 * Counts and result are unchanged on failure. */
w_seed_product_closure0_status w_seed_product_closure0_measure(
    const w_seed_product_closure0_input *input,
    w_seed_product_closure0_counts *counts,
    w_seed_product_closure0_result *result);

/* Build and publish the closure projection transactionally.  HIR verification
 * runs before reachability; any malformed dead record therefore rejects the
 * whole operation.  Output arrays, counts, and result remain unchanged on
 * every failure. */
w_seed_product_closure0_status w_seed_product_closure0_run(
    const w_seed_product_closure0_input *input,
    const w_seed_product_closure0_output *output,
    w_seed_product_closure0_result *result);

/* Verify a previously published projection against the complete HIR and its
 * canonical reachable digest. */
bool w_seed_product_closure0_verify(
    const w_seed_product_closure0_input *input,
    const w_seed_product_closure0_output *output,
    const w_seed_product_closure0_result *result);

/* Independent adapter cross-check.  expected_reachable_functions is source
 * indexed and must describe the exact closure; the function verifies HIR
 * first and then compares the independently computed root closure. */
bool w_seed_product_closure0_cross_check_functions(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    const bool *expected_reachable_functions,
    size_t expected_capacity);

#ifdef __cplusplus
}
#endif

#endif
