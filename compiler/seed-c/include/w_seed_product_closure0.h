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
#define W_SEED_PRODUCT_CLOSURE0_SCHEMA_VERSION "w-seed-product-closure0-1"
#define W_SEED_PRODUCT_CLOSURE0_NONE UINT32_MAX
#define W_SEED_PRODUCT_CLOSURE0_DIGEST_BYTES 32u
#define W_SEED_PRODUCT_CLOSURE0_MAX_MODULES 32u
#define W_SEED_PRODUCT_CLOSURE0_MAX_FUNCTIONS 128u
#define W_SEED_PRODUCT_CLOSURE0_MAX_IDENTITIES 256u
#define W_SEED_PRODUCT_CLOSURE0_MAX_TYPES 4u
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

/* The root is intentionally explicit.  ProductClosure0 currently admits
 * only the one verifier-published .default entry at source entry index zero.
 * All fields are source HIR indices, not dense closure ordinals. */
typedef struct {
  uint32_t entry_index;
  uint32_t module_index;
  uint32_t function_index;
  uint32_t identity_index;
} w_seed_product_closure0_root;

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
} w_seed_product_closure0_output;

typedef struct {
  w_seed_product_closure0_status status;
  w_seed_product_closure0_counts required;
  w_seed_product_closure0_counts written;
  w_seed_product_closure0_failure failure;
  w_seed_product_closure0_root root;
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
