#ifndef W_SEED_NATIVE_SUBSET0_H
#define W_SEED_NATIVE_SUBSET0_H

#include <stddef.h>
#include <stdint.h>

#include "w_seed_hir0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This private selector is the single boundary for the native print subset.
 * It consumes only a verified HIR0 program and exposes borrowed record and
 * payload views. It does not perform textual lookup or copy caller storage. */
#define W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD 256u

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

w_seed_native_subset0_status w_seed_native_subset0_select(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_native_subset0_selection *selection);

#ifdef __cplusplus
}
#endif

#endif
