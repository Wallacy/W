#ifndef W_SEED_PARALLEL_ELISION0_H
#define W_SEED_PARALLEL_ELISION0_H

#include <stdbool.h>
#include <stdint.h>

#include "w_seed_parallel_selection0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PARELIDE0 is a target-neutral, caller-owned legality certificate for one
 * deliberately narrow optimization candidate. It does not choose an
 * optimization, estimate cost, rewrite HIR, or claim that a provider/domain
 * is unobservable. A later target policy must independently establish those
 * facts before replacing physical launch/join with a direct call. */
#define W_SEED_PARALLEL_ELISION0_SCHEMA_VERSION \
  "w-seed-parallel-elision0-1"
#define W_SEED_PARALLEL_ELISION0_MAX_ARGUMENTS 16u

typedef enum {
  W_SEED_PARALLEL_ELISION0_OK = 0,
  W_SEED_PARALLEL_ELISION0_INVALID,
  W_SEED_PARALLEL_ELISION0_UNSUPPORTED,
} w_seed_parallel_elision0_status;

typedef enum {
  W_SEED_PARALLEL_ELISION0_FACT_SINGLE_TASK = 1u << 0,
  W_SEED_PARALLEL_ELISION0_FACT_IMMEDIATE_JOIN = 1u << 1,
  W_SEED_PARALLEL_ELISION0_FACT_CLOSED_PURE_GRAPH = 1u << 2,
  W_SEED_PARALLEL_ELISION0_FACT_NEVER_SUSPENDS = 1u << 3,
  W_SEED_PARALLEL_ELISION0_FACT_NONTHROWING = 1u << 4,
  W_SEED_PARALLEL_ELISION0_FACT_UNIQUE_TASK_CONSUMER = 1u << 5,
  W_SEED_PARALLEL_ELISION0_FACT_SCALAR_ABI = 1u << 6,
} w_seed_parallel_elision0_fact;

#define W_SEED_PARALLEL_ELISION0_REQUIRED_FACTS                         \
  (W_SEED_PARALLEL_ELISION0_FACT_SINGLE_TASK |                         \
   W_SEED_PARALLEL_ELISION0_FACT_IMMEDIATE_JOIN |                      \
   W_SEED_PARALLEL_ELISION0_FACT_CLOSED_PURE_GRAPH |                   \
   W_SEED_PARALLEL_ELISION0_FACT_NEVER_SUSPENDS |                      \
   W_SEED_PARALLEL_ELISION0_FACT_NONTHROWING |                         \
   W_SEED_PARALLEL_ELISION0_FACT_UNIQUE_TASK_CONSUMER |                \
   W_SEED_PARALLEL_ELISION0_FACT_SCALAR_ABI)

typedef struct {
  char schema[sizeof(W_SEED_PARALLEL_ELISION0_SCHEMA_VERSION)];
  uint32_t root_function_index;
  uint32_t task_call_index;
  uint32_t task_function_index;
  uint32_t launch_binding_index;
  uint32_t join_binding_index;
  uint32_t launch_instruction_index;
  uint32_t join_instruction_index;
  uint32_t reachable_function_count;
  uint32_t proof_facts;
  uint32_t function_count;
  uint32_t instruction_count;
  uint32_t binding_count;
  uint32_t call_count;
  uint32_t value_count;
  uint8_t reserved[4];
  uint8_t hir_semantic_digest[32];
} w_seed_parallel_elision0_certificate;

/* Publication is transactional. INVALID and UNSUPPORTED leave certificate
 * untouched. Verification rederives every byte from independently verified
 * HIR and PARSEL0. */
w_seed_parallel_elision0_status w_seed_parallel_elision0_certify(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    w_seed_parallel_elision0_certificate *certificate);

bool w_seed_parallel_elision0_verify(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_parallel_elision0_certificate *certificate);

#ifdef __cplusplus
}
#endif

#endif
