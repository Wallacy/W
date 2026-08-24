#ifndef W_SEED_GENERIC_VALIDATION_H
#define W_SEED_GENERIC_VALIDATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_constir.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal seed-C generic predicate validation.  This is not a W interface
 * and it does not create a final specialization or a type identity. */
#define W_SEED_GENERIC_VALIDATION_SCHEMA_VERSION "w-seed-generic-validation-2"
#define W_SEED_GENERIC_VALIDATION_FINGERPRINT_SCHEMA_VERSION \
  "w-seed-generic-fingerprint-1"
#define W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES 32u
#define W_SEED_GENERIC_VALIDATION_MAX_DEPTH 256u
#define W_SEED_GENERIC_VALIDATION_MAX_PREDICATES \
  W_SEED_FRONTEND_MAX_GENERIC_SLOTS
#define W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_RECORDS 64u
#define W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES 4096u
#define W_SEED_GENERIC_VALIDATION_MAX_REJECTION_TRACE_ITEMS 1u
#define W_SEED_GENERIC_VALIDATION_FALLBACK_BYTES 15u

typedef enum {
  W_SEED_GENERIC_VALIDATION_VERIFIED = 0,
  W_SEED_GENERIC_VALIDATION_REJECTED,
  W_SEED_GENERIC_VALIDATION_UNSUPPORTED,
  W_SEED_GENERIC_VALIDATION_INVALID,
  /* A well-formed predicate ran but the evaluator preserved a runtime
   * diagnostic such as W-CONST-0003 or W-CONST-0006. */
  W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED,
  /* Caller-owned evidence or conversion storage is too small. */
  W_SEED_GENERIC_VALIDATION_CAPACITY,
} w_seed_generic_validation_state;

typedef enum {
  W_SEED_GENERIC_VALIDATION_FAILURE_NONE = 0,
  W_SEED_GENERIC_VALIDATION_FAILURE_PREDICATE_FALSE,
  W_SEED_GENERIC_VALIDATION_FAILURE_BINDING,
  W_SEED_GENERIC_VALIDATION_FAILURE_VALUE,
  W_SEED_GENERIC_VALIDATION_FAILURE_FUNCTION,
  W_SEED_GENERIC_VALIDATION_FAILURE_CAPACITY,
  W_SEED_GENERIC_VALIDATION_FAILURE_EVALUATOR_DIAGNOSTIC,
  W_SEED_GENERIC_VALIDATION_FAILURE_RESULT_TYPE,
  W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT,
} w_seed_generic_validation_failure;

/* A fingerprint is a local, versioned evidence projection.  It is not a
 * TypeId, cache key, schema ID, ABI key, or authority for equality. */
typedef enum {
  W_SEED_GENERIC_VALIDATION_FINGERPRINT_NOT_AVAILABLE = 0,
  W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE,
  W_SEED_GENERIC_VALIDATION_FINGERPRINT_UNSUPPORTED,
} w_seed_generic_validation_fingerprint_state;

typedef enum {
  W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT = 0,
  W_SEED_GENERIC_VALIDATION_RECEIPT_PREDICATE,
} w_seed_generic_validation_receipt_kind;

/* One causal evaluation record.  The record is written only after all input
 * relations and output capacities pass the preflight. */
typedef struct {
  w_seed_generic_validation_receipt_kind kind;
  uint32_t generic_argument_index;
  uint32_t argument_const_value_index;
  uint32_t typed_const_expression_index;
  w_seed_span argument_span;
  uint32_t predicate_parameter_index;
  uint32_t predicate_function_index;
  w_seed_span predicate_span;
  w_seed_span predicate_function_span;
  w_seed_constir_eval_result evaluation;
  /* The value produced by a calculated argument or predicate. */
  w_seed_constir_value eval_value;
  bool result_is_bool;
  bool bool_value;
} w_seed_generic_validation_receipt;

/* Minimum caller-owned facts for W-CONST-0004.  D1 uses the exact fallback
 * strings because the current evaluator does not preserve execution edges. */
typedef struct {
  uint32_t application_index;
  uint32_t head_struct_index;
  w_seed_frontend_text head_name;
  uint32_t generic_argument_index;
  uint32_t argument_const_value_index;
  uint32_t typed_const_expression_index;
  w_seed_span argument_span;
  uint32_t predicate_function_index;
  w_seed_span predicate_span;
  w_seed_span predicate_function_span;
  w_seed_frontend_text failure;
  w_seed_frontend_text rejection_trace
      [W_SEED_GENERIC_VALIDATION_MAX_REJECTION_TRACE_ITEMS];
  size_t rejection_trace_count;
} w_seed_generic_validation_rejection;

typedef struct {
  const w_seed_frontend_output *frontend_output;
  const w_seed_frontend_result *frontend_result;
  const w_seed_constir_program *constir_program;
  uint32_t application_index;
  w_seed_constir_quota quota;
  w_seed_constir_eval_workspace *eval_workspace;
  /* The arena is caller-owned.  A list parent points into this array. */
  w_seed_constir_value *conversion_values;
  size_t conversion_value_capacity;
  /* Caller-owned UTF-8 evidence bytes.  D1 writes one shared fallback item
   * of W_SEED_GENERIC_VALIDATION_FALLBACK_BYTES bytes. */
  uint8_t *evidence_bytes;
  size_t evidence_byte_capacity;
  w_seed_generic_validation_receipt *receipts;
  size_t receipt_capacity;
} w_seed_generic_validation_input;

typedef struct {
  w_seed_generic_validation_state state;
  w_seed_generic_validation_failure failure;
  uint32_t application_index;
  uint32_t head_struct_index;
  size_t predicate_count;
  size_t computed_argument_count;
  size_t receipts_written;
  w_seed_constir_diagnostic_code diagnostic;
  w_seed_span diagnostic_span;
  /* For a runtime evaluator fault, this is the exact causal result. */
  w_seed_constir_eval_result evaluation;
  w_seed_generic_validation_rejection rejection;
  w_seed_generic_validation_fingerprint_state fingerprint_state;
  uint8_t fingerprint_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES];
} w_seed_generic_validation_result;

/* Validate one normalized local generic application.  The function performs
 * all relation and capacity checks before it calls the ConstIR evaluator. */
w_seed_generic_validation_state w_seed_generic_validation_run(
    const w_seed_generic_validation_input *input,
    w_seed_generic_validation_result *result);

/* Stable text projections for probes and diagnostics. */
const char *w_seed_generic_validation_state_name(
    w_seed_generic_validation_state state);
const char *w_seed_generic_validation_failure_name(
    w_seed_generic_validation_failure failure);
const char *w_seed_generic_validation_fingerprint_state_name(
    w_seed_generic_validation_fingerprint_state state);

#ifdef __cplusplus
}
#endif

#endif
