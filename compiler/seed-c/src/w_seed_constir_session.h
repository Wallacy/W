#ifndef W_SEED_CONSTIR_SESSION_H
#define W_SEED_CONSTIR_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "w_seed_constir.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Private seed-compiler state. This header is not installed or exposed to W. */
typedef struct {
  uint32_t declaration;
  w_seed_constir_value value;
  uint8_t state;
} w_seed_constir_session_entry;

typedef struct {
  w_seed_constir_session_entry entries[
      W_SEED_CONSTIR_MAX_CONST_MEMO_ENTRIES];
  size_t count;
} w_seed_constir_session;

void w_seed_constir_session_init(w_seed_constir_session *session);

w_seed_constir_status w_seed_constir_evaluate_in_session(
    const w_seed_constir_program *program, uint32_t function_index,
    const w_seed_constir_value *arguments, size_t argument_count,
    w_seed_constir_quota quota, w_seed_constir_eval_workspace *workspace,
    w_seed_constir_session *session, w_seed_constir_value *value,
    w_seed_constir_eval_result *result);

#ifdef __cplusplus
}
#endif

#endif
