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
#define W_SEED_GENERIC_VALIDATION_SCHEMA_VERSION "w-seed-generic-validation-8"
#define W_SEED_GENERIC_VALIDATION_FINGERPRINT_SCHEMA_VERSION \
  "w-seed-generic-fingerprint-1"
#define W_SEED_GENERIC_VALIDATION_SPECIALIZATION_SCHEMA_VERSION \
  "w-seed-generic-specialization-2"
#define W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES 32u
#define W_SEED_GENERIC_VALIDATION_SPECIALIZATION_DIGEST_BYTES 32u
#define W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES 32u
#define W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_AUTHORITY_BYTES 4096u
#define W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PACKAGE_BYTES 127u
#define W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PACKAGE_COMPONENT_BYTES 63u
#define W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS 64u
#define W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_SEGMENT_BYTES 512u
#define W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_OWNER_CHAIN 32u
#define W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_NAME_BYTES 512u
#define W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES 16384u
/* The builder publishes at most MAX_PREIMAGE_BYTES.  The parser accepts a
 * larger bounded framing window so it can distinguish a complete,
 * over-ceiling receipt (UNSUPPORTED) from a truncated one (INVALID).  A
 * structurally parsed receipt, including an UNSUPPORTED one, still requires
 * a matching SHA-256 digest; only INVALID framing skips the hash. */
#define W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PARSE_PREIMAGE_BYTES \
  65536u
#define W_SEED_GENERIC_VALIDATION_MAX_DEPTH 256u
#define W_SEED_GENERIC_VALIDATION_MAX_PREDICATES \
  W_SEED_FRONTEND_MAX_GENERIC_SLOTS
#define W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_RECORDS 64u
#define W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES 4096u
#define W_SEED_GENERIC_VALIDATION_MAX_REJECTION_TRACE_ITEMS 1u
#define W_SEED_GENERIC_VALIDATION_FALLBACK_BYTES 15u
#define W_SEED_GENERIC_VALIDATION_MAX_CONST_DEPENDENCIES 256u
#define W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH \
  (W_SEED_GENERIC_VALIDATION_MAX_CONST_DEPENDENCIES + 1u)

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
  /* The well-formed D4 dependency graph exceeded its 256-dependency ceiling. */
  W_SEED_GENERIC_VALIDATION_FAILURE_DEPENDENCY_LIMIT,
} w_seed_generic_validation_failure;

/* A fingerprint is a local, versioned evidence projection.  It is not a
 * TypeId, cache key, schema ID, ABI key, or authority for equality. */
typedef enum {
  W_SEED_GENERIC_VALIDATION_FINGERPRINT_NOT_AVAILABLE = 0,
  W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE,
  W_SEED_GENERIC_VALIDATION_FINGERPRINT_UNSUPPORTED,
} w_seed_generic_validation_fingerprint_state;

/* The semantic specialization preimage is caller-owned evidence.  It is not
 * a cache recipe, TypeId, or persistent identifier. */
typedef enum {
  W_SEED_GENERIC_VALIDATION_SPECIALIZATION_NOT_AVAILABLE = 0,
  W_SEED_GENERIC_VALIDATION_SPECIALIZATION_AVAILABLE,
  W_SEED_GENERIC_VALIDATION_SPECIALIZATION_UNSUPPORTED,
  W_SEED_GENERIC_VALIDATION_SPECIALIZATION_CAPACITY,
  /* Validation can be VERIFIED without a publishable nominal identity when
   * the resolver did not supply the origin receipt. */
  W_SEED_GENERIC_VALIDATION_SPECIALIZATION_IDENTITY_REQUIRED,
} w_seed_generic_validation_specialization_state;

/* The resolver supplies the authenticated authority preimage.  This seed
 * does not resolve registries, Git, or local authorities.  Kind values are
 * deliberately independent of the frontend enum ordinals: zero is reserved
 * for malformed input and the canonical local struct kind is 0x01. */
typedef enum {
  W_SEED_GENERIC_NOMINAL_DECLARATION_INVALID = 0,
  W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT = 1,
  W_SEED_GENERIC_NOMINAL_DECLARATION_TYPE = 2,
  W_SEED_GENERIC_NOMINAL_DECLARATION_OBJECT = 3,
  W_SEED_GENERIC_NOMINAL_DECLARATION_ENUM = 4,
  W_SEED_GENERIC_NOMINAL_DECLARATION_PROTOCOL = 5,
  W_SEED_GENERIC_NOMINAL_DECLARATION_SERVICE = 6,
} w_seed_generic_nominal_declaration_kind;

typedef struct {
  uint8_t kind;
  w_seed_frontend_text name;
} w_seed_generic_nominal_owner;

/* Normalized facts used by the caller-owned origin builder.  Package names
 * use the exact two-component ASCII scoped-package grammar.  Module segments,
 * owner names, and the declaration name use the bounded ASCII W identifier
 * grammar.  Valid UTF-8 outside that subset is UNSUPPORTED until the resolver
 * supplies NFC-normalized facts; malformed UTF-8 and language-invalid ASCII
 * are INVALID.  Facts and their backing arrays must remain immutable and
 * disjoint from the output during measure/write. */
typedef struct {
  const uint8_t *authority_preimage;
  size_t authority_preimage_length;
  w_seed_frontend_text scoped_package_name;
  const w_seed_frontend_text *module_path_segments;
  size_t module_path_segment_count;
  w_seed_generic_nominal_declaration_kind declaration_kind;
  const w_seed_generic_nominal_owner *owner_chain;
  size_t owner_chain_count;
  w_seed_frontend_text declared_name;
} w_seed_generic_nominal_origin;

typedef enum {
  W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID = 0,
  W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE,
  W_SEED_GENERIC_NOMINAL_ORIGIN_UNSUPPORTED,
  W_SEED_GENERIC_NOMINAL_ORIGIN_CAPACITY,
} w_seed_generic_nominal_origin_state;

typedef struct {
  w_seed_generic_nominal_origin_state state;
  size_t bytes_written;
  size_t bytes_required;
  uint8_t digest[W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES];
} w_seed_generic_nominal_origin_result;

/* A receipt view includes only caller-owned bytes and the frontend relation
 * needed to bind those bytes to one application head.  The receipt itself
 * carries authority, package, module path, kind, owners, and name. */
typedef struct {
  const uint8_t *preimage;
  size_t preimage_length;
  const uint8_t *digest;
  uint32_t frontend_module_index;
  uint32_t frontend_head_struct_index;
} w_seed_generic_nominal_origin_view;

typedef struct {
  const uint8_t *preimage;
  size_t preimage_length;
  const uint8_t *digest;
} w_seed_generic_specialization_view;

typedef enum {
  W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT = 0,
  W_SEED_GENERIC_VALIDATION_RECEIPT_PREDICATE,
} w_seed_generic_validation_receipt_kind;

/* One causal evaluation record.  Normal validation writes a record after the
 * caller-owned capacities pass preflight.  D4 cycle preflight runs before
 * capacity and can publish only CONST_ARGUMENT when receipt storage is
 * sufficient; it never writes beyond caller-owned storage. */
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
  /* Effective type of the generic argument represented by this receipt:
   * CONST_ARGUMENT uses the value type; PREDICATE uses the input/domain type.
   * The predicate eval_value remains Bool.  A cycle receipt uses the typed
   * argument effective type even though no value was evaluated. */
  uint32_t effective_type;
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
  /* Optional authenticated nominal-origin receipt for the application head.
   * NULL is permitted for semantic validation, but specialization identity
   * then remains IDENTITY_REQUIRED. */
  const w_seed_generic_nominal_origin_view *nominal_origin;
  /* Caller-owned semantic specialization preimage output.  A NULL pointer
   * with non-zero capacity is invalid input.  The buffer must be disjoint
   * from frontend, ConstIR, conversion, evidence, receipt, and result
   * storage, and all input storage must remain unchanged between the measure
   * and write passes. */
  uint8_t *specialization_preimage;
  size_t specialization_preimage_capacity;
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
  w_seed_generic_validation_specialization_state specialization_state;
  size_t specialization_bytes_written;
  size_t specialization_bytes_required;
  uint8_t specialization_digest[
      W_SEED_GENERIC_VALIDATION_SPECIALIZATION_DIGEST_BYTES];
  /* Closed causal path for W-CONST-0002.  Entries use ConstIR function
   * indices.  The first entry is repeated at the end. */
  uint32_t const_cycle_path[W_SEED_GENERIC_VALIDATION_MAX_CONST_CYCLE_PATH];
  size_t const_cycle_path_length;
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
const char *w_seed_generic_validation_specialization_state_name(
    w_seed_generic_validation_specialization_state state);

/* Caller-owned nominal-origin receipt builder.  `measure` never writes an
 * output buffer and returns the exact byte count plus its SHA-256 accelerator.
 * `write` measures again before writing, so a short buffer is left untouched.
 * Input facts, text arrays, and any origin view remain immutable and disjoint
 * from output/result storage across both passes.  Neither function
 * authenticates a registry/Git/local authority. */
w_seed_generic_nominal_origin_state w_seed_generic_nominal_origin_measure(
    const w_seed_generic_nominal_origin *origin,
    w_seed_generic_nominal_origin_result *result);
w_seed_generic_nominal_origin_state w_seed_generic_nominal_origin_write(
    const w_seed_generic_nominal_origin *origin, uint8_t *output,
    size_t output_capacity, w_seed_generic_nominal_origin_result *result);

/* Validate the complete receipt framing, digest, and canonical fields.  This
 * is a structural/integrity check only; resolver authorization remains an
 * input fact owned by the caller. */
bool w_seed_generic_nominal_origin_view_valid(
    const w_seed_generic_nominal_origin_view *view);
bool w_seed_generic_nominal_origin_equal(
    const w_seed_generic_nominal_origin_view *left,
    const w_seed_generic_nominal_origin_view *right);

/* Compare semantic specialization views with collision-safe full-byte
 * equality.  A matching digest never replaces the preimage comparison. */
bool w_seed_generic_specialization_equal(
    const w_seed_generic_specialization_view *left,
    const w_seed_generic_specialization_view *right);

#ifdef __cplusplus
}
#endif

#endif
