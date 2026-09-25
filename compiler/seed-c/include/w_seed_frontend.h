#ifndef W_SEED_FRONTEND_H
#define W_SEED_FRONTEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_parser.h"
#include "w_seed_module_scan.h"
#include "w_seed_sha256.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal seed frontend. It is not a public W command or compiler driver. */
#define W_SEED_FRONTEND_SCHEMA_VERSION "w-seed-frontend-78"
#define W_SEED_FRONTEND_NONE UINT32_MAX
#define W_SEED_FRONTEND_NONE_SIZE SIZE_MAX
#define W_SEED_FRONTEND_MAX_CST_NODES 32768u
/* The const-inference scratch is indexed by the global declaration ordinal.
 * This is an explicit input ceiling, not a per-document promise. */
#define W_SEED_FRONTEND_MAX_CONST_DECLARATIONS \
  W_SEED_FRONTEND_MAX_CST_NODES
#define W_SEED_FRONTEND_MAX_NESTING 256u
/* Generic schema/application scratch is deliberately bounded below the CST
 * budget so the two dry/emit contexts remain safe on the seed's Windows
 * stack.  Crossing this ceiling is an UNSUPPORTED projection with a fact. */
#define W_SEED_FRONTEND_MAX_GENERIC_SLOTS 64u
#define W_SEED_FRONTEND_MAX_STATIC_LIST_ELEMENTS 4096u
#define W_SEED_FRONTEND_MAX_DOCUMENTS 256u
#define W_SEED_FRONTEND_MAX_EXTERNAL_MODULES 256u
#define W_SEED_FRONTEND_MAX_EXTERNAL_SYMBOLS 4096u
#define W_SEED_FRONTEND_MAX_EXTERNAL_PARAMETERS 4096u
#define W_SEED_FRONTEND_MAX_HOST_SYMBOLS 4096u
#define W_SEED_FRONTEND_MAX_HOST_PARAMETERS 4096u
#define W_SEED_FRONTEND_MAX_HOST_REQUIREMENTS 16u
/* Caller-owned execution-domain input is intentionally tiny.  It is a
 * binding table, not an ambient runtime catalogue. */
#define W_SEED_FRONTEND_MAX_DOMAINS 64u
#define W_SEED_FRONTEND_DOMAIN_IDENTITY ".domain"
/* D1 uses an explicit 64-bit target profile.  This is a semantic target
 * fact, not a query of the host compiler's size_t width; changing it changes
 * normalized usize types and therefore the frontend receipt key. */
#define W_SEED_FRONTEND_TARGET_USIZE_BITS 64u

typedef struct {
  const char *data;
  size_t length;
} w_seed_frontend_text;

typedef enum {
  W_SEED_FRONTEND_OK = 0,
  W_SEED_FRONTEND_DIAGNOSTICS,
  W_SEED_FRONTEND_UNSUPPORTED,
  W_SEED_FRONTEND_CAPACITY,
  W_SEED_FRONTEND_BARRIER,
  W_SEED_FRONTEND_INVALID,
} w_seed_frontend_status;

typedef enum {
  W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE = 0,
  W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE,
  W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
  W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL,
  W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL,
  W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL,
  W_SEED_FRONTEND_FACT_INVALID_ENTRY,
  /* Append-only Async0 semantic facts.  These are emitted for rejected
   * task-form expressions instead of leaving consumers to infer meaning from
   * source spelling. */
  W_SEED_FRONTEND_FACT_ASYNC_LAUNCH,
  W_SEED_FRONTEND_FACT_AWAIT,
  W_SEED_FRONTEND_FACT_TASK_ESCAPE,
  /* Rejected use of the non-reifiable current-execution yield facet. */
  W_SEED_FRONTEND_FACT_EXECUTION_YIELD,
  /* Rejected use of the bounded mandatory `.main` domain dispatch. */
  W_SEED_FRONTEND_FACT_SPAWN_MAIN_LAUNCH,
  /* Rejected use of an explicitly caller-bound parallel `.domain` dispatch.
   * This fact is distinct from the serial `.main` lane. */
  W_SEED_FRONTEND_FACT_SPAWN_PARALLEL_DOMAIN_LAUNCH,
  /* Rejected static submission of a kernel binding. */
  W_SEED_FRONTEND_FACT_SPAWN_ACCELERATED_DOMAIN_LAUNCH,
} w_seed_frontend_fact_kind;

typedef enum {
  W_SEED_FRONTEND_DOMAIN_MODE_SERIAL = 0,
  W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
} w_seed_frontend_domain_mode;

typedef enum {
  W_SEED_FRONTEND_DOMAIN_HOST = 0,
  W_SEED_FRONTEND_DOMAIN_ACCELERATED,
} w_seed_frontend_domain_kind;

typedef enum {
  W_SEED_FRONTEND_DOMAIN_CAPABILITY_NONE = 0u,
  W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL = 1u << 0,
  W_SEED_FRONTEND_DOMAIN_CAPABILITY_DEVICE = 1u << 1,
} w_seed_frontend_domain_capability;

/* A caller-owned exact execution-domain binding. The frontend never treats a
 * source identity as ambient or infers a binding from mode, capabilities, or
 * maximum. Accelerated bindings carry logical admission facts only, never a
 * provider, device, queue, artifact, or launch handle. */
typedef struct {
  w_seed_frontend_text name;
  w_seed_frontend_domain_kind kind;
  w_seed_frontend_domain_mode mode;
  uint32_t capabilities;
  uint32_t maximum;
} w_seed_frontend_domain;

typedef enum {
  W_SEED_FRONTEND_TYPE_INVALID = 0,
  W_SEED_FRONTEND_TYPE_UNIT,
  W_SEED_FRONTEND_TYPE_BOOL,
  W_SEED_FRONTEND_TYPE_STRING,
  W_SEED_FRONTEND_TYPE_BYTES,
  W_SEED_FRONTEND_TYPE_INTEGER,
  W_SEED_FRONTEND_TYPE_FLOAT,
  W_SEED_FRONTEND_TYPE_OPTION,
  W_SEED_FRONTEND_TYPE_NOMINAL,
  W_SEED_FRONTEND_TYPE_FUNCTION,
  W_SEED_FRONTEND_TYPE_UNKNOWN,
  /* Append-only nominal enum type. */
  W_SEED_FRONTEND_TYPE_ENUM,
  /* Append-only closed enum case-set type. */
  W_SEED_FRONTEND_TYPE_ENUM_SUBSET,
  /* Append-only compile-time ordered list and half-open range types. */
  W_SEED_FRONTEND_TYPE_STATIC_LIST,
  W_SEED_FRONTEND_TYPE_RANGE,
  /* Append-only opaque task handle.  The result type is carried by the
   * element_type relation on the normalized type record. */
  W_SEED_FRONTEND_TYPE_TASK,
  /* Bottom type for expressions that do not complete normally. */
  W_SEED_FRONTEND_TYPE_NEVER,
  /* Append-only fixed tuple value. */
  W_SEED_FRONTEND_TYPE_TUPLE,
} w_seed_frontend_type_kind;

typedef enum {
  W_SEED_FRONTEND_PANIC_CODE_INVALID = 0,
  W_SEED_FRONTEND_PANIC_CODE_EXPLICIT,
} w_seed_frontend_panic_code;

typedef enum {
  W_SEED_FRONTEND_DECL_STRUCT = 0,
  W_SEED_FRONTEND_DECL_TYPE,
  W_SEED_FRONTEND_DECL_ALIAS,
  W_SEED_FRONTEND_DECL_FUNCTION,
  W_SEED_FRONTEND_DECL_ENUM,
  /* Append-only module-level named const declaration. */
  W_SEED_FRONTEND_DECL_CONST,
} w_seed_frontend_decl_kind;

typedef enum {
  W_SEED_FRONTEND_EXPR_UNSUPPORTED = 0,
  W_SEED_FRONTEND_EXPR_IDENTIFIER,
  W_SEED_FRONTEND_EXPR_INTEGER,
  W_SEED_FRONTEND_EXPR_FLOAT,
  W_SEED_FRONTEND_EXPR_BOOL,
  W_SEED_FRONTEND_EXPR_STRING,
  W_SEED_FRONTEND_EXPR_BYTES,
  W_SEED_FRONTEND_EXPR_UNARY,
  W_SEED_FRONTEND_EXPR_BINARY,
  W_SEED_FRONTEND_EXPR_CALL,
  W_SEED_FRONTEND_EXPR_PARENTHESIS,
  /* Append-only closed-enum expressions. */
  W_SEED_FRONTEND_EXPR_ENUM_CASE,
  W_SEED_FRONTEND_EXPR_SWITCH,
  /* Append-only enum membership expression. */
  W_SEED_FRONTEND_EXPR_ENUM_MEMBERSHIP,
  /* Append-only member, index, and half-open range expressions. */
  W_SEED_FRONTEND_EXPR_MEMBER,
  W_SEED_FRONTEND_EXPR_INDEX,
  W_SEED_FRONTEND_EXPR_RANGE,
  /* Append-only ordered text/expression interpolation. */
  W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING,
  /* Append-only bounded scalar if expression.  left is the Bool condition,
   * right is the then arm, and else_expression is the else arm. */
  W_SEED_FRONTEND_EXPR_IF,
  /* Append-only local assignment. left is the mutable binding identifier,
   * right is the replacement value, and the expression type is Unit. */
  W_SEED_FRONTEND_EXPR_ASSIGNMENT,
  /* Append-only Async0 structured child launcher. left is exactly one root
   * CALL expression and the result type is an explicit Task record. */
  W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH,
  /* Append-only Async0 join. left is one Task binding identifier and the
   * result type is the launcher's result type. */
  W_SEED_FRONTEND_EXPR_AWAIT,
  /* Exact `await execution#yield()` suspension point.  This is one semantic
   * expression, not a member call and not a public Task value. */
  W_SEED_FRONTEND_EXPR_EXECUTION_YIELD,
  /* Exact `spawn<.main> localCall(...)`.  This is a mandatory serial-domain
   * dispatch, not an async initializer eligible for direct-call elision. */
  W_SEED_FRONTEND_EXPR_SPAWN_MAIN_LAUNCH,
  /* Exact `spawn<.domain> localCall(...)`.  The caller-owned domain input
   * record is retained by the placement fields on the expression. */
  W_SEED_FRONTEND_EXPR_SPAWN_PARALLEL_DOMAIN_LAUNCH,
  /* Exact `spawn<acceleratedDomain> kernel(...)`. The selected domain,
   * kernel module and binding are indexed on the expression. */
  W_SEED_FRONTEND_EXPR_SPAWN_ACCELERATED_DOMAIN_LAUNCH,
  /* Exact `try localCall(...)` propagation marker. left is the resolved
   * throwing CALL and propagated_error_enum is its concrete enum identity. */
  W_SEED_FRONTEND_EXPR_TRY,
  /* Explicit `panic(...)` terminator expression. */
  W_SEED_FRONTEND_EXPR_PANIC,
  /* Append-only exact implicit integer widening.  left is the source
   * expression; conversion_source_type and conversion_destination_type are
   * the canonical scalar identities retained for downstream lowering. */
  W_SEED_FRONTEND_EXPR_IMPLICIT_INTEGER_WIDEN,
  /* Append-only explicit `D(truncatingBits: source)` conversion.  It is a
   * distinct total bit-pattern conversion, not an implicit widening or call.
   * The shared conversion_* facts retain its source and destination types. */
  W_SEED_FRONTEND_EXPR_INTEGER_TRUNCATING_BITS,
  /* Append-only explicit `D(saturating: source)` conversion.  It clamps the
   * source mathematical value to the destination integer range. */
  W_SEED_FRONTEND_EXPR_INTEGER_SATURATING,
  /* Append-only exact numeric widening.  It keeps one typed child and the
   * canonical source/destination identities, whether selected contextually
   * or written with an exact `D(value)` conversion. */
  W_SEED_FRONTEND_EXPR_NUMERIC_WIDEN,
  /* Exact floating representation bridges. The source and result identities
   * are carried by conversion_source_type and conversion_destination_type;
   * neither operation performs floating-point arithmetic. */
  W_SEED_FRONTEND_EXPR_FLOAT_FROM_BITS =
      W_SEED_FRONTEND_EXPR_NUMERIC_WIDEN + 1,
  W_SEED_FRONTEND_EXPR_FLOAT_TO_BITS,
  /* Append-only partial integer conversion. Its value is owned by a plain
   * `try` expression that records the canonical NumericConversionError type. */
  W_SEED_FRONTEND_EXPR_INTEGER_EXACTLY,
  /* Append-only partial floating-to-fixed-integer conversion. Its value is
   * owned by a plain `try` expression and retains a closed rounding identity
   * plus the statically possible NumericConversionError cases. */
  W_SEED_FRONTEND_EXPR_FLOAT_TO_INTEGER_ROUNDING,
  /* Append-only ordered tuple value construction. Its children are in the
   * tuple_elements relation; it is never represented as CALL/PARENTHESIS. */
  W_SEED_FRONTEND_EXPR_TUPLE,
} w_seed_frontend_expr_kind;

/* Core rounding identities. These are semantic values, not source text or
 * user-defined enum cases. `NONE` is the absence sentinel on all unrelated
 * expression records. */
typedef enum {
  W_SEED_FRONTEND_ROUNDING_MODE_NONE = 0,
  W_SEED_FRONTEND_ROUNDING_MODE_NEAREST_EVEN,
  W_SEED_FRONTEND_ROUNDING_MODE_NEAREST_AWAY_FROM_ZERO,
  W_SEED_FRONTEND_ROUNDING_MODE_TOWARD_ZERO,
  W_SEED_FRONTEND_ROUNDING_MODE_TOWARD_POSITIVE,
  W_SEED_FRONTEND_ROUNDING_MODE_TOWARD_NEGATIVE,
} w_seed_frontend_rounding_mode;

/* Statically possible failure cases for a fallible numeric conversion. The
 * bitmask is intentionally append-only so later conversion families can add
 * facts without changing existing record meaning. */
typedef enum {
  W_SEED_FRONTEND_CONVERSION_ERROR_FACT_NONE = 0u,
  W_SEED_FRONTEND_CONVERSION_ERROR_FACT_NON_FINITE = 1u << 0,
  W_SEED_FRONTEND_CONVERSION_ERROR_FACT_OUT_OF_RANGE = 1u << 1,
} w_seed_frontend_conversion_error_fact;

typedef enum {
  W_SEED_FRONTEND_INTERPOLATION_TEXT = 0,
  W_SEED_FRONTEND_INTERPOLATION_EXPRESSION,
} w_seed_frontend_interpolation_segment_kind;

typedef struct {
  w_seed_frontend_interpolation_segment_kind kind;
  uint32_t owner_expression;
  uint32_t ordinal;
  w_seed_span span;
  /* TEXT owns a slice in const_bytes. EXPRESSION owns expression_index. */
  uint32_t expression_index;
  uint32_t const_byte_offset;
  uint32_t const_byte_count;
} w_seed_frontend_interpolation_segment;

typedef enum {
  W_SEED_FRONTEND_SWITCH_PATTERN_ENUM_CASE = 0,
  W_SEED_FRONTEND_SWITCH_PATTERN_WILDCARD,
  W_SEED_FRONTEND_SWITCH_PATTERN_LITERAL,
} w_seed_frontend_switch_pattern_kind;

typedef enum {
  W_SEED_FRONTEND_STMT_UNSUPPORTED = 0,
  W_SEED_FRONTEND_STMT_LET,
  W_SEED_FRONTEND_STMT_VAR,
  W_SEED_FRONTEND_STMT_RETURN,
  W_SEED_FRONTEND_STMT_IF,
  W_SEED_FRONTEND_STMT_EXPRESSION,
  W_SEED_FRONTEND_STMT_EXPECT,
  /* Append-only structured control statements for downstream const lowering. */
  W_SEED_FRONTEND_STMT_GUARD,
  W_SEED_FRONTEND_STMT_FOR,
  /* Append-only pre-test loop with a Bool condition and one child chain. */
  W_SEED_FRONTEND_STMT_WHILE,
  /* Append-only post-test loop with a Bool condition and one child chain. */
  W_SEED_FRONTEND_STMT_REPEAT,
  /* Append-only typed recoverable-error terminator. */
  W_SEED_FRONTEND_STMT_THROW,
  /* Append-only lexical cleanup registration. The only source-backed seed
   * form currently accepted is one synchronous direct local Unit call. */
  W_SEED_FRONTEND_STMT_DEFER,
  /* Append-only unlabeled loop control transfers. Their target is verified by
   * HIR from the enclosing bounded loop CFG. */
  W_SEED_FRONTEND_STMT_BREAK,
  W_SEED_FRONTEND_STMT_CONTINUE,
} w_seed_frontend_stmt_kind;

typedef struct {
  w_seed_frontend_text logical_source_id;
  w_seed_frontend_text module_id;
  /* Resolver-owned local component. This field is required and explicit. */
  w_seed_frontend_text local_module_name;
  const w_seed_source *source;
  const w_seed_cst_node *nodes;
  size_t node_count;
  w_seed_parse_result parse;
} w_seed_frontend_document;

typedef enum {
  W_SEED_FRONTEND_EXTERNAL_VALUE = 0,
  W_SEED_FRONTEND_EXTERNAL_TYPE,
} w_seed_frontend_external_kind;

typedef enum {
  W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY = 0,
  W_SEED_FRONTEND_LABEL_REQUIRED,
} w_seed_frontend_label_kind;

typedef struct {
  w_seed_frontend_text name;
  w_seed_frontend_text type;
  w_seed_frontend_label_kind label_kind;
} w_seed_frontend_external_parameter;

typedef struct {
  w_seed_frontend_text name;
  w_seed_frontend_external_kind kind;
  bool exported;
  const w_seed_frontend_external_parameter *parameters;
  size_t parameter_count;
  w_seed_frontend_text return_type;
  /* Append-only const capability flag. The default is false. */
  bool is_const;
  /* Empty for a free external symbol. A non-empty value names the exact
   * nominal receiver that owns this external member. */
  w_seed_frontend_text receiver_type;
} w_seed_frontend_external_symbol;

typedef struct {
  w_seed_frontend_text module_id;
  const w_seed_frontend_external_symbol *symbols;
  size_t symbol_count;
} w_seed_frontend_external_module;

/* A host-prelude is an explicit resolver input. It is separate from the
 * external import graph: a name in this table is not an imported module
 * symbol, and no implicit global prelude exists. Requirements are nominal
 * records, not a closed global capability catalogue. */
typedef struct {
  w_seed_frontend_text name;
} w_seed_frontend_host_requirement;

typedef struct {
  w_seed_frontend_text name;
  w_seed_frontend_external_kind kind;
  const w_seed_frontend_external_parameter *parameters;
  size_t parameter_count;
  w_seed_frontend_text return_type;
  bool is_const;
  const w_seed_frontend_host_requirement *requirements;
  size_t requirement_count;
} w_seed_frontend_host_prelude_symbol;

typedef struct {
  w_seed_frontend_text profile;
  const w_seed_frontend_host_prelude_symbol *symbols;
  size_t symbol_count;
} w_seed_frontend_host_prelude;

/* Append-only callee identity. A consumer must not infer provenance from a
 * missing function index: host-prelude and imported external symbols are
 * distinct identities. */
typedef enum {
  W_SEED_FRONTEND_CALLEE_NONE = 0,
  W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION,
  W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL,
  W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL,
  W_SEED_FRONTEND_CALLEE_KERNEL_BINDING,
  /* Append-only generated initializer for a local nominal struct. */
  W_SEED_FRONTEND_CALLEE_LOCAL_STRUCT_CONSTRUCTOR,
} w_seed_frontend_callee_kind;

/* Closed compiler-owned associated operations. These are semantic identities,
 * not module symbols or source rewrites. */
typedef enum {
  W_SEED_FRONTEND_BUILTIN_NONE = 0,
  /* Fixed-width integer wrapping policy. The historical U64 names remain
   * source-compatible aliases for these append-only numeric identities. */
  W_SEED_FRONTEND_BUILTIN_U64_WRAPPING_ADD,
  W_SEED_FRONTEND_BUILTIN_U64_WRAPPING_SUBTRACT,
  /* Fixed-width integer type-namespace receiver marker; not executable. */
  W_SEED_FRONTEND_BUILTIN_U64_RECEIVER,
  W_SEED_FRONTEND_BUILTIN_U64_WRAPPING_MULTIPLY,
  W_SEED_FRONTEND_BUILTIN_U64_WRAPPING_NEGATE,
  W_SEED_FRONTEND_BUILTIN_U64_WRAPPING_POWER,
  W_SEED_FRONTEND_BUILTIN_U64_WRAPPING_SHIFT_LEFT,
  /* Historical u64 identity for fixed-width integer maskedShiftLeft;
   * keep the numeric identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_MASKED_SHIFT_LEFT,
  /* Historical u64 identity for fixed-width integer maskedShiftRight;
   * keep the numeric identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_MASKED_SHIFT_RIGHT,
  /* Historical u64 identity for fixed-width integer logicalShiftRight;
   * keep the numeric identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_LOGICAL_SHIFT_RIGHT,
  /* Canonical u64.rotatedLeft. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_ROTATED_LEFT,
  /* Canonical u64.rotatedRight. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_ROTATED_RIGHT,
  /* Canonical u64.countOnes. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_COUNT_ONES,
  /* Canonical u64.countZeros. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_COUNT_ZEROS,
  /* Canonical u64.countLeadingZeros. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_COUNT_LEADING_ZEROS,
  /* Canonical u64.countTrailingZeros. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_COUNT_TRAILING_ZEROS,
  /* Canonical u64.reversedBits. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_REVERSED_BITS,
  /* Canonical u64.reversedBytes. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_REVERSED_BYTES,
  /* Canonical u64.saturatingAdd. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_SATURATING_ADD,
  /* Canonical u64.saturatingSubtract. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_SATURATING_SUBTRACT,
  /* Canonical u64.saturatingMultiply. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_SATURATING_MULTIPLY,
  /* Canonical u64.overflowingAdd. Its result is `(u64, Bool)`. */
  W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_ADD,
  /* Canonical u64.overflowingSubtract. Its result is `(u64, Bool)`. */
  W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_SUBTRACT,
  /* Canonical u64.overflowingMultiply. Its result is `(u64, Bool)`. */
  W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_MULTIPLY,
  /* Canonical u64.overflowingNegate. Its result is `(u64, Bool)`. */
  W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_NEGATE,
  /* Canonical u64.overflowingPower. Its result is `(u64, Bool)`. */
  W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_POWER,
  /* Canonical u64.saturatingNegate. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_SATURATING_NEGATE,
  /* Canonical u64.saturatingPower. Keep this identity append-only. */
  W_SEED_FRONTEND_BUILTIN_U64_SATURATING_POWER,
  /* Generic fixed-width integer wrapping policy.  These aliases deliberately
   * retain the original numeric identities: the receiver's signedness and
   * width are carried by the associated expression type, not by a
   * width-specific operation enum. */
  W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_ADD =
      W_SEED_FRONTEND_BUILTIN_U64_WRAPPING_ADD,
  W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_SUBTRACT =
      W_SEED_FRONTEND_BUILTIN_U64_WRAPPING_SUBTRACT,
  W_SEED_FRONTEND_BUILTIN_INTEGER_RECEIVER =
      W_SEED_FRONTEND_BUILTIN_U64_RECEIVER,
  W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_MULTIPLY =
      W_SEED_FRONTEND_BUILTIN_U64_WRAPPING_MULTIPLY,
  W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_NEGATE =
      W_SEED_FRONTEND_BUILTIN_U64_WRAPPING_NEGATE,
  W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_POWER =
      W_SEED_FRONTEND_BUILTIN_U64_WRAPPING_POWER,
  W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_SHIFT_LEFT =
      W_SEED_FRONTEND_BUILTIN_U64_WRAPPING_SHIFT_LEFT,
  /* Float namespace/member semantic identities.  Receiver width comes from
   * the exact associated expression type; the receiver marker is not itself
   * executable. These are appended after the existing operation range. */
  W_SEED_FRONTEND_BUILTIN_FLOAT_RECEIVER =
      W_SEED_FRONTEND_BUILTIN_U64_SATURATING_POWER + 1,
  W_SEED_FRONTEND_BUILTIN_FLOAT_FROM_BITS,
  W_SEED_FRONTEND_BUILTIN_FLOAT_TO_BITS,
} w_seed_frontend_builtin_operation;

typedef enum {
  W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT = 0,
  W_SEED_FRONTEND_RESOLVED_IMPORT_EXTERNAL_MODULE,
} w_seed_frontend_resolved_import_kind;

/* A resolver-owned edge binds one direct import to one exact target. The
 * frontend validates source order, spans, target bounds, and local cycles. */
typedef struct {
  uint32_t source_document_index;
  uint32_t direct_import_ordinal;
  w_seed_span import_declaration_span;
  w_seed_frontend_resolved_import_kind target_kind;
  uint32_t target_index;
} w_seed_frontend_resolved_import;

typedef struct {
  const w_seed_frontend_document *documents;
  size_t document_count;
  const w_seed_frontend_external_module *external_modules;
  size_t external_module_count;
  const w_seed_frontend_host_prelude *host_scope;
  bool import_resolution_complete;
  const w_seed_frontend_resolved_import *resolved_imports;
  size_t resolved_import_count;
  /* Append-only caller-owned exact domain bindings. */
  const w_seed_frontend_domain *domains;
  size_t domain_count;
} w_seed_frontend_input;

typedef struct {
  size_t modules;
  size_t imports;
  size_t import_items;
  size_t structs;
  size_t fields;
  size_t type_declarations;
  size_t aliases;
  size_t types;
  size_t functions;
  size_t parameters;
  size_t entries;
  size_t statements;
  size_t expressions;
  size_t interpolation_segments;
  size_t arguments;
  size_t symbols;
  size_t facts;
  size_t diagnostics;
  /* Typed D0 diagnostic carrier ranges. */
  size_t diagnostic_facts;
  size_t diagnostic_items;
  size_t diagnostic_labels;
  size_t receipt_bytes;
  /* Append-only enum declaration/case/payload counts. */
  size_t enums;
  size_t enum_cases;
  size_t enum_case_parameters;
  size_t switch_arms;
  size_t pattern_captures;
  size_t enum_subset_members;
  /* Append-only membership case records. */
  size_t enum_membership_cases;
  /* Append-only generic declaration parameter records. */
  size_t generic_parameters;
  /* Append-only generic type-application and frontend ConstValue records. */
  size_t generic_applications;
  size_t generic_arguments;
  size_t typed_const_expressions;
  size_t const_values;
  size_t const_elements;
  size_t const_bytes;
  /* Append-only module const declaration records. */
  size_t const_declarations;
  /* Append-only compiler-owned kernel-module and kernel-binding records. */
  size_t kernel_modules;
  size_t kernel_bindings;
  /* Append-only canonical tuple component and construction element records. */
  size_t tuple_components;
  size_t tuple_elements;
} w_seed_frontend_counts;

typedef struct {
  w_seed_frontend_text source_id;
  w_seed_frontend_text module_id;
  w_seed_frontend_text local_module_name;
  w_seed_span span;
  size_t document_index;
  uint32_t first_import;
  uint32_t import_count;
  uint32_t first_struct;
  uint32_t struct_count;
  uint32_t first_type_declaration;
  uint32_t type_declaration_count;
  uint32_t first_alias;
  uint32_t alias_count;
  uint32_t first_function;
  uint32_t function_count;
  uint32_t first_entry;
  uint32_t entry_count;
  uint32_t first_enum;
  uint32_t enum_count;
  /* Append-only module const declaration range. */
  uint32_t first_const_declaration;
  uint32_t const_declaration_count;
} w_seed_frontend_module;

typedef enum {
  W_SEED_FRONTEND_IMPORT_UNRESOLVED = 0,
  W_SEED_FRONTEND_IMPORT_LOCAL_DOCUMENT,
  W_SEED_FRONTEND_IMPORT_EXTERNAL_MODULE,
} w_seed_frontend_import_target_kind;

/* `kernel` is a projection of compiler-known identities. It is not an
 * ordinary value import and never creates runtime state. */
typedef enum {
  W_SEED_FRONTEND_IMPORT_ORDINARY = 0,
  W_SEED_FRONTEND_IMPORT_KERNEL,
} w_seed_frontend_import_kind;

typedef struct {
  /* The import path is source evidence. target_kind/index is the only
   * resolved identity used by frontend consumers. */
  uint32_t module_index;
  w_seed_frontend_import_kind kind;
  w_seed_frontend_text path;
  w_seed_frontend_text alias;
  w_seed_span span;
  uint32_t first_item;
  uint32_t item_count;
  uint32_t direct_import_ordinal;
  w_seed_frontend_import_target_kind target_kind;
  uint32_t target_index;
} w_seed_frontend_import;

typedef struct {
  /* `name`/`local_name` retain source provenance. For a grouped named kernel
   * import, these indices bind the canonical target `(module identity,
   * public label)`; ordinary items retain NONE in all three fields. */
  uint32_t module_index;
  w_seed_frontend_text name;
  w_seed_frontend_text local_name;
  w_seed_span span;
  uint32_t resolved_kernel_module_index;
  uint32_t resolved_kernel_binding_index;
  uint32_t resolved_kernel_function_index;
} w_seed_frontend_import_item;

typedef struct {
  uint32_t module_index;
  w_seed_frontend_text name;
  bool exported;
  w_seed_span span;
  uint32_t first_field;
  uint32_t field_count;
  /* Append-only generic declaration parameter range. */
  uint32_t first_generic_parameter;
  uint32_t generic_parameter_count;
} w_seed_frontend_struct;

typedef enum {
  W_SEED_FRONTEND_GENERIC_KIND_INVALID = 0,
  W_SEED_FRONTEND_GENERIC_KIND_TYPE,
  W_SEED_FRONTEND_GENERIC_KIND_VALUE,
} w_seed_frontend_generic_kind;

typedef enum {
  W_SEED_FRONTEND_GENERIC_DOMAIN_NONE = 0,
  W_SEED_FRONTEND_GENERIC_DOMAIN_INVALID,
  W_SEED_FRONTEND_GENERIC_DOMAIN_CONCRETE,
  W_SEED_FRONTEND_GENERIC_DOMAIN_DEPENDENT,
} w_seed_frontend_generic_domain_kind;

typedef enum {
  W_SEED_FRONTEND_GENERIC_REFINEMENT_NONE = 0,
  W_SEED_FRONTEND_GENERIC_REFINEMENT_PREDICATE,
  W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID,
} w_seed_frontend_generic_refinement_kind;

typedef enum {
  W_SEED_FRONTEND_GENERIC_SUBJECT_NONE = 0,
  W_SEED_FRONTEND_GENERIC_SUBJECT_MEMBER,
  W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID,
} w_seed_frontend_generic_subject_kind;

/* A normalized generic parameter belongs to a declaration head.  This
 * record contains declaration schema only. It does not contain a static
 * argument or a downstream const result. */
typedef struct {
  uint32_t module_index;
  w_seed_frontend_decl_kind owner_kind;
  uint32_t owner_index;
  uint32_t ordinal;
  /* Empty for positional-only parameters. */
  w_seed_frontend_text external_label;
  w_seed_frontend_text internal_name;
  w_seed_frontend_label_kind label_kind;
  w_seed_frontend_generic_kind kind;
  w_seed_span span;
  uint32_t domain_type;
  w_seed_frontend_generic_refinement_kind refinement_kind;
  uint32_t predicate_function_index;
  w_seed_span predicate_span;
  w_seed_span predicate_function_span;
  w_seed_frontend_generic_subject_kind subject_kind;
  /* Value-domain classification.  DEPENDENT links to a previous TYPE slot. */
  w_seed_frontend_generic_domain_kind domain_kind;
  uint32_t dependent_type_parameter_ordinal;
} w_seed_frontend_generic_parameter;

typedef struct {
  uint32_t module_index;
  uint32_t owner_struct;
  w_seed_frontend_text name;
  w_seed_span span;
  uint32_t type_index;
} w_seed_frontend_field;

typedef struct {
  uint32_t module_index;
  w_seed_frontend_text name;
  bool exported;
  w_seed_span span;
  w_seed_span generic_span;
  bool has_generic_parameters;
  uint32_t conformance_type;
  uint32_t first_case;
  uint32_t case_count;
  uint32_t type_index;
  w_seed_span conformance_span;
} w_seed_frontend_enum;

typedef struct {
  uint32_t module_index;
  uint32_t owner_enum;
  w_seed_frontend_text name;
  w_seed_span span;
  uint32_t first_payload;
  uint32_t payload_count;
} w_seed_frontend_enum_case;

typedef struct {
  uint32_t module_index;
  uint32_t owner_case;
  w_seed_frontend_text label;
  bool has_label;
  w_seed_span span;
  uint32_t type_index;
} w_seed_frontend_enum_case_parameter;

/* A caller-owned module const declaration.  The record contains source
 * ownership and typed initializer relations only.  It never stores a value. */
typedef struct {
  uint32_t module_index;
  w_seed_frontend_text name;
  bool exported;
  w_seed_span span;
  w_seed_span body_span;
  uint32_t declared_type;
  uint32_t initializer_expression;
  uint32_t symbol_index;
  bool has_explicit_type;
  bool lowerable;
  /* Append-only inferred or constrained semantic type.  NONE means that the
   * initializer did not produce a type record.  declared_type remains the
   * source annotation index and is never populated by inference. */
  uint32_t effective_type;
} w_seed_frontend_const_declaration;

/* Compiler-owned synthesis records for a module contract's `kernels` field.
 * They preserve semantic identities only. Provider, target, queue, pointer,
 * and physical ABI data belong to later lowering/provider layers. */
typedef struct {
  uint32_t module_index;
  w_seed_span span;
  uint32_t first_kernel;
  uint32_t kernel_count;
} w_seed_frontend_kernel_module;

typedef struct {
  uint32_t module_index;
  uint32_t owner_kernel_module;
  uint32_t ordinal;
  w_seed_frontend_text label;
  w_seed_span span;
  uint32_t function_index;
} w_seed_frontend_kernel_binding;

typedef struct {
  uint32_t module_index;
  w_seed_frontend_text name;
  bool exported;
  w_seed_span span;
  uint32_t type_index;
} w_seed_frontend_type_declaration;

typedef struct {
  uint32_t module_index;
  w_seed_frontend_text name;
  bool exported;
  w_seed_span span;
  uint32_t type_index;
} w_seed_frontend_alias;

typedef struct {
  w_seed_frontend_type_kind kind;
  w_seed_frontend_text spelling;
  w_seed_frontend_text nominal_name;
  w_seed_span span;
  bool is_signed;
  uint16_t bit_width;
  uint32_t element_type;
  uint32_t return_type;
  uint32_t first_parameter;
  uint32_t parameter_count;
  /* Kind-discriminated nominal identity: enum base for ENUM/SUBSET, or
   * local struct index for NOMINAL. Other kinds require NONE. */
  uint32_t enum_base_index;
  uint32_t first_subset_member;
  uint32_t subset_member_count;
  /* W_SEED_FRONTEND_NONE unless this root owns a generic application. */
  uint32_t generic_application_index;
  /* Append-only Task result relation.  It is populated only for
   * W_SEED_FRONTEND_TYPE_TASK and indexes the result type record. */
  uint32_t task_result_type;
  /* External nominal identity.  These fields are populated only when the
   * spelling is bound to an imported external TYPE; the pair indexes the
   * resolver-owned external module/symbol tables and is never a pointer. */
  uint32_t external_module_index;
  uint32_t external_symbol_index;
  /* Append-only ordered tuple component range; NONE/zero on non-tuples. */
  uint32_t first_tuple_component;
  uint32_t tuple_component_count;
} w_seed_frontend_type;

/* Ordered, source-backed component of a tuple type. `label` is empty for the
 * currently supported unlabeled tuple grammar and is reserved for the
 * all-labeled form without imposing a fixed tuple arity. */
typedef struct {
  uint32_t owner_type;
  uint32_t ordinal;
  w_seed_frontend_text label;
  uint32_t type_index;
  w_seed_span span;
} w_seed_frontend_tuple_component;

typedef struct {
  uint32_t owner_type;
  uint32_t enum_base_index;
  uint32_t enum_case_index;
  w_seed_span source_span;
} w_seed_frontend_enum_subset_member;

typedef struct {
  uint32_t module_index;
  uint32_t owner_expression;
  uint32_t enum_base_index;
  uint32_t enum_case_index;
  w_seed_span source_span;
} w_seed_frontend_enum_membership_case;

typedef struct {
  uint32_t module_index;
  uint32_t owner_function;
  w_seed_frontend_text name;
  w_seed_frontend_text label;
  w_seed_frontend_label_kind label_kind;
  w_seed_span span;
  uint32_t type_index;
} w_seed_frontend_parameter;

typedef struct {
  uint32_t module_index;
  w_seed_frontend_text name;
  bool exported;
  w_seed_span span;
  w_seed_span body_span;
  uint32_t first_parameter;
  uint32_t parameter_count;
  uint32_t return_type;
  /* NONE for nonthrowing functions; otherwise the concrete `throws E` type. */
  uint32_t error_type;
  uint32_t first_statement;
  uint32_t statement_count;
  /* Append-only const capability and D0 body support flags. */
  bool is_const;
  bool const_body_supported;
  /* Append-only function qualifiers. These are frontend facts, so downstream
   * consumers never inspect CST tokens or source text to reject a function. */
  bool is_async;
  bool is_throws;
  bool is_unsafe;
  bool has_borrow_clause;
  /* True only for the private function synthesized for `entry { ... }`. */
  bool is_anonymous_entry;
} w_seed_frontend_function;

typedef struct {
  uint32_t module_index;
  w_seed_frontend_text target;
  w_seed_span span;
  bool valid;
  /* `entry { ... }` owns the body directly and has no public target name. */
  bool is_body;
  /* Direct normalized relation to the function selected by this entry. */
  uint32_t target_function;
} w_seed_frontend_entry;

typedef struct {
  w_seed_frontend_stmt_kind kind;
  uint32_t module_index;
  uint32_t owner_function;
  w_seed_span span;
  uint32_t expression_index;
  uint32_t condition_expression;
  uint32_t first_child;
  uint32_t child_count;
  w_seed_frontend_text binding_name;
  uint32_t declared_type;
  /* Append-only normalized statement relations. */
  uint32_t next_sibling;
  uint32_t else_child;
  uint32_t range_lower_expression;
  uint32_t range_upper_expression;
  uint32_t loop_local_ordinal;
  /* Append-only effective type of a local binding initializer.  This is
   * distinct from declared_type, which records only a source annotation. */
  uint32_t effective_type;
  /* Structured while-label facts. The loop owns loop_label; a transfer keeps
   * its optional source label and the resolved enclosing loop statement. An
   * unlabeled transfer has an empty transfer_label but still has an explicit
   * transfer_target_statement. */
  w_seed_frontend_text loop_label;
  w_seed_frontend_text transfer_label;
  uint32_t transfer_target_statement;
} w_seed_frontend_statement;

typedef enum {
  W_SEED_FRONTEND_SYMBOL_MODULE = 0,
  W_SEED_FRONTEND_SYMBOL_STRUCT,
  W_SEED_FRONTEND_SYMBOL_TYPE,
  W_SEED_FRONTEND_SYMBOL_ALIAS,
  W_SEED_FRONTEND_SYMBOL_FUNCTION,
  W_SEED_FRONTEND_SYMBOL_FIELD,
  W_SEED_FRONTEND_SYMBOL_PARAMETER,
  W_SEED_FRONTEND_SYMBOL_BINDING,
  W_SEED_FRONTEND_SYMBOL_ENTRY,
  W_SEED_FRONTEND_SYMBOL_ENUM,
  W_SEED_FRONTEND_SYMBOL_ENUM_CASE,
  /* Append-only module-level named const symbol. */
  W_SEED_FRONTEND_SYMBOL_CONST,
} w_seed_frontend_symbol_kind;

typedef struct {
  w_seed_frontend_symbol_kind kind;
  uint32_t module_index;
  uint32_t owner_index;
  w_seed_frontend_text name;
  bool exported;
  w_seed_span span;
  uint32_t type_index;
} w_seed_frontend_symbol;

typedef struct {
  uint32_t module_index;
  /* The seed currently records the identifier callee as the lexical owner.
   * CALL.first_argument/argument_count is the authoritative call argument
   * range; this field is retained as an append-only legacy relation. */
  uint32_t owner_expression;
  w_seed_frontend_text label;
  w_seed_span span;
  uint32_t expression_index;
  /* Append-only ordinal: parameter for calls, local struct field for a
   * generated value initializer. */
  uint32_t resolved_parameter_ordinal;
} w_seed_frontend_argument;

typedef enum {
  W_SEED_FRONTEND_GENERIC_ARGUMENT_TYPE = 0,
  W_SEED_FRONTEND_GENERIC_ARGUMENT_VALUE,
} w_seed_frontend_generic_argument_kind;

/* Binding status only.  It never asserts predicate truth or a completed
 * specialization. */
typedef enum {
  W_SEED_FRONTEND_GENERIC_BINDING_INVALID = 0,
  W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED,
  W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST,
  W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE,
} w_seed_frontend_generic_binding_status;

typedef struct {
  uint32_t module_index;
  uint32_t owner_type;
  uint32_t head_struct;
  w_seed_frontend_text head_name;
  w_seed_span span;
  w_seed_span envelope_span;
  uint32_t first_argument;
  uint32_t argument_count;
  w_seed_frontend_generic_binding_status binding_status;
  /* True when a later const graph must evaluate a typed ConstExpr or
   * declared refinement. This seed sets it for declared refinements. */
  bool requires_const_evaluation;
} w_seed_frontend_generic_application;

typedef struct {
  uint32_t module_index;
  uint32_t owner_application;
  uint32_t source_ordinal;
  w_seed_span span;
  w_seed_frontend_text label;
  uint32_t parameter_index;
  uint32_t parameter_ordinal;
  w_seed_frontend_generic_argument_kind kind;
  uint32_t type_index;
  uint32_t const_value_index;
  w_seed_frontend_generic_binding_status binding_status;
  /* Append-only relation for a calculated generic value.  Immediate values
   * keep this at W_SEED_FRONTEND_NONE. */
  uint32_t typed_const_expression_index;
} w_seed_frontend_generic_argument;

/* A frontend-only, typed expression relation.  It records type and source
 * ownership for the later const graph; it never contains an evaluated value. */
typedef struct {
  uint32_t module_index;
  uint32_t owner_application;
  uint32_t argument_ordinal;
  uint32_t expression_index;
  w_seed_span span;
  uint32_t expected_type;
  uint32_t effective_type;
} w_seed_frontend_typed_const_expression;

typedef enum {
  W_SEED_FRONTEND_CONST_INVALID = 0,
  W_SEED_FRONTEND_CONST_BOOL,
  W_SEED_FRONTEND_CONST_INTEGER,
  W_SEED_FRONTEND_CONST_STRING,
  W_SEED_FRONTEND_CONST_ENUM_CASE,
  W_SEED_FRONTEND_CONST_STATIC_LIST,
} w_seed_frontend_const_value_kind;

typedef struct {
  w_seed_frontend_const_value_kind kind;
  uint32_t type_index;
  w_seed_span span;
  bool bool_value;
  bool integer_signed;
  uint16_t integer_bit_width;
  uint8_t integer_byte_count;
  uint8_t integer_bytes[16];
  uint32_t first_byte;
  uint32_t byte_count;
  uint32_t enum_base_index;
  uint32_t enum_case_index;
  uint32_t first_element;
  uint32_t element_count;
} w_seed_frontend_const_value;

typedef struct {
  uint32_t owner_value;
  uint32_t ordinal;
  uint32_t value_index;
  w_seed_span span;
} w_seed_frontend_const_element;

typedef struct {
  uint32_t module_index;
  uint32_t owner_expression;
  w_seed_frontend_switch_pattern_kind pattern_kind;
  uint32_t enum_index;
  uint32_t enum_case_index;
  w_seed_span pattern_span;
  uint32_t result_expression;
  w_seed_span span;
  bool supported;
  uint32_t first_capture;
  uint32_t capture_count;
} w_seed_frontend_switch_arm;

/* A switch payload capture is an explicit lexical relation to one enum-case
 * parameter.  It is never reconstructed from spelling downstream. */
typedef struct {
  uint32_t module_index;
  uint32_t owner_switch_arm;
  uint32_t ordinal;
  uint32_t parameter_ordinal;
  w_seed_frontend_text name;
  w_seed_span span;
  uint32_t type_index;
} w_seed_frontend_pattern_capture;

typedef struct {
  w_seed_frontend_expr_kind kind;
  uint32_t module_index;
  uint32_t owner_function;
  w_seed_frontend_text spelling;
  w_seed_frontend_text operator_text;
  w_seed_span span;
  uint32_t left;
  uint32_t right;
  uint32_t first_argument;
  uint32_t argument_count;
  uint32_t inferred_type;
  bool supported;
  /* Populated only for the append-only scalar conversion wrapper kinds.
   * Ordinary records retain the W_SEED_FRONTEND_NONE absence sentinel. */
  uint32_t conversion_source_type;
  uint32_t conversion_destination_type;
  /* Append-only enum/switch identity fields. */
  uint32_t enum_index;
  uint32_t enum_case_index;
  uint32_t first_switch_arm;
  uint32_t switch_arm_count;
  /* Append-only enum membership case range. */
  uint32_t first_membership_case;
  uint32_t membership_case_count;
  /* Append-only typed literal projections. Downstream const lowering consumes
   * these fields without reparsing source spelling. integer_value is a non-negative
   * magnitude in canonical little-endian order with unused high bytes zero. */
  bool has_bool_value;
  bool bool_value;
  bool has_integer_value;
  uint8_t integer_value[16];
  /* Canonical IEEE-754 bits for a supported f32/f64 literal.  Both formats
   * use the low bits of this carrier; an f32 literal has zero high bits.  The
   * frontend materializes once; downstream stages never reparse source
   * spelling or depend on the host floating representation. */
  bool has_float_value;
  uint64_t float_bits;
  /* Append-only ordinal. On MEMBER, a local nominal flat projection stores
   * its struct field ordinal; on identifier, the function parameter ordinal. */
  uint32_t resolved_parameter_ordinal;
  /* Discriminated by resolved_callee_kind: local function index, or local
   * struct index for LOCAL_STRUCT_CONSTRUCTOR. */
  uint32_t resolved_function_index;
  /* Append-only discriminated callee identity. The numeric fields are valid
   * only for their corresponding kind and are never pointer identities. */
  w_seed_frontend_callee_kind resolved_callee_kind;
  w_seed_frontend_builtin_operation builtin_operation;
  uint32_t resolved_host_symbol_index;
  uint32_t resolved_external_module_index;
  uint32_t resolved_external_symbol_index;
  uint32_t resolved_local_ordinal;
  /* Valid only for KERNEL_BINDING. These are semantic arena
   * indices, never provider/device/queue handles. */
  uint32_t resolved_kernel_module_index;
  uint32_t resolved_kernel_binding_index;
  w_seed_frontend_text member_name;
  /* Append-only source relation retained for ordinary module consts. Kernel
   * projections never populate this field. */
  uint32_t resolved_const_declaration;
  /* Append-only normalized simple String literal slice.  The offset is
   * W_SEED_FRONTEND_NONE for every other expression kind.  An empty String
   * uses a valid offset and a zero count. */
  uint32_t const_byte_offset;
  uint32_t const_byte_count;
  /* Append-only lexical relation for a local binding read.  The value is a
   * normalized statement index, never a source-text identity. */
  uint32_t resolved_binding_statement;
  /* Append-only lexical relation for a switch payload capture read. */
  uint32_t resolved_pattern_capture;
  /* Append-only ordered interpolation range. */
  uint32_t first_interpolation_segment;
  uint32_t interpolation_segment_count;
  /* Append-only scalar-if else-arm relation.  It is NONE for every other
   * expression kind. */
  uint32_t else_expression;
  /* Append-only Task relations. ASYNC_LAUNCH and SPAWN_MAIN_LAUNCH use
   * task_result_type and task_call_expression; AWAIT uses
   * task_binding_statement and task_result_type. */
  uint32_t task_result_type;
  uint32_t task_call_expression;
  uint32_t task_binding_statement;
  /* Present only for a CALL-based TRY. It identifies the exact local error
   * enum shared by the called function and lexical owner function. */
  uint32_t propagated_error_enum;
  /* Present only for an integer-exactly or float-to-integer-rounding TRY. It
   * identifies the canonical bare NumericConversionError nominal type, not a
   * local/external type. */
  uint32_t propagated_error_type;
  /* Present only for EXPR_PANIC. The message reuses the normalized String
   * literal's const_byte_offset/count; the panic is a terminator, never a
   * call. */
  w_seed_frontend_panic_code panic_code;
  /* Append-only explicit placement evidence. NONE/default is required on
   * every expression except the two explicit-domain launch kinds. */
  uint32_t domain_index;
  w_seed_frontend_domain_kind domain_kind;
  w_seed_frontend_domain_mode domain_mode;
  uint32_t domain_capabilities;
  uint32_t domain_maximum;
  /* Present only for NUMERIC_WIDEN. The receipt retains whether the shared
   * exact wrapper came from explicit D(value) syntax or context. */
  bool numeric_widen_is_explicit;
  /* Present only for FLOAT_TO_INTEGER_ROUNDING. The mode is a closed core
   * identity; possible_error_facts is a static type-level fact set, not a
   * claim about the particular source value. */
  w_seed_frontend_rounding_mode conversion_rounding_mode;
  uint32_t conversion_possible_error_facts;
  /* Append-only ordered tuple construction element range; NONE/zero unless
   * kind is W_SEED_FRONTEND_EXPR_TUPLE. */
  uint32_t first_tuple_element;
  uint32_t tuple_element_count;
} w_seed_frontend_expression;

/* Ordered child value of a tuple constructor. `label` is empty for the
 * currently supported unlabeled tuple grammar and is reserved for the
 * all-labeled form. */
typedef struct {
  uint32_t owner_expression;
  uint32_t ordinal;
  w_seed_frontend_text label;
  uint32_t expression_index;
  w_seed_span span;
} w_seed_frontend_tuple_element;

typedef enum {
  W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING = 0,
  W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER,
  W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY,
  W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET,
} w_seed_frontend_diagnostic_fact_kind;

typedef struct {
  w_seed_frontend_text key;
  /* STRING uses text. INTEGER uses integer_value. ARRAY and SET values use
   * ranges in the append-only diagnostic_items array. */
  w_seed_frontend_diagnostic_fact_kind kind;
  w_seed_frontend_text text;
  int64_t integer_value;
  uint32_t first_item;
  uint32_t item_count;
} w_seed_frontend_diagnostic_fact;

typedef struct {
  w_seed_frontend_text text;
} w_seed_frontend_diagnostic_item;

typedef struct {
  w_seed_frontend_text role;
  w_seed_span span;
  size_t document_index;
} w_seed_frontend_diagnostic_label;

typedef struct {
  w_seed_frontend_text code;
  w_seed_span primary;
  size_t document_index;
  uint32_t first_fact;
  uint32_t fact_count;
  uint32_t first_label;
  uint32_t label_count;
} w_seed_frontend_diagnostic;

typedef struct {
  w_seed_frontend_fact_kind kind;
  w_seed_frontend_text detail;
  w_seed_span span;
  size_t document_index;
} w_seed_frontend_fact;

typedef struct {
  w_seed_frontend_module *modules;
  size_t module_capacity;
  w_seed_frontend_import *imports;
  size_t import_capacity;
  w_seed_frontend_import_item *import_items;
  size_t import_item_capacity;
  w_seed_frontend_struct *structs;
  size_t struct_capacity;
  w_seed_frontend_field *fields;
  size_t field_capacity;
  w_seed_frontend_type_declaration *type_declarations;
  size_t type_declaration_capacity;
  w_seed_frontend_alias *aliases;
  size_t alias_capacity;
  w_seed_frontend_type *types;
  size_t type_capacity;
  w_seed_frontend_function *functions;
  size_t function_capacity;
  w_seed_frontend_parameter *parameters;
  size_t parameter_capacity;
  w_seed_frontend_argument *arguments;
  size_t argument_capacity;
  w_seed_frontend_entry *entries;
  size_t entry_capacity;
  w_seed_frontend_statement *statements;
  size_t statement_capacity;
  w_seed_frontend_expression *expressions;
  size_t expression_capacity;
  w_seed_frontend_interpolation_segment *interpolation_segments;
  size_t interpolation_segment_capacity;
  w_seed_frontend_symbol *symbols;
  size_t symbol_capacity;
  w_seed_frontend_fact *facts;
  size_t fact_capacity;
  w_seed_frontend_diagnostic *diagnostics;
  size_t diagnostic_capacity;
  w_seed_frontend_diagnostic_fact *diagnostic_facts;
  size_t diagnostic_fact_capacity;
  w_seed_frontend_diagnostic_item *diagnostic_items;
  size_t diagnostic_item_capacity;
  w_seed_frontend_diagnostic_label *diagnostic_labels;
  size_t diagnostic_label_capacity;
  uint8_t *receipt;
  size_t receipt_capacity;
  /* Append-only enum output arrays. */
  w_seed_frontend_enum *enums;
  size_t enum_capacity;
  w_seed_frontend_enum_case *enum_cases;
  size_t enum_case_capacity;
  w_seed_frontend_enum_case_parameter *enum_case_parameters;
  size_t enum_case_parameter_capacity;
  /* Append-only module const declaration output. */
  w_seed_frontend_const_declaration *const_declarations;
  size_t const_declaration_capacity;
  /* Append-only compiler-owned kernel-module synthesis records. */
  w_seed_frontend_kernel_module *kernel_modules;
  size_t kernel_module_capacity;
  w_seed_frontend_kernel_binding *kernel_bindings;
  size_t kernel_binding_capacity;
  /* Append-only switch-arm output arrays. */
  w_seed_frontend_switch_arm *switch_arms;
  size_t switch_arm_capacity;
  w_seed_frontend_pattern_capture *pattern_captures;
  size_t pattern_capture_capacity;
  /* Append-only normalized enum-subset member records. */
  w_seed_frontend_enum_subset_member *enum_subset_members;
  size_t enum_subset_member_capacity;
  /* Append-only enum membership case records. */
  w_seed_frontend_enum_membership_case *enum_membership_cases;
  size_t enum_membership_case_capacity;
  /* Append-only generic declaration parameter records. */
  w_seed_frontend_generic_parameter *generic_parameters;
  size_t generic_parameter_capacity;
  w_seed_frontend_generic_application *generic_applications;
  size_t generic_application_capacity;
  w_seed_frontend_generic_argument *generic_arguments;
  size_t generic_argument_capacity;
  w_seed_frontend_typed_const_expression *typed_const_expressions;
  size_t typed_const_expression_capacity;
  w_seed_frontend_const_value *const_values;
  size_t const_value_capacity;
  w_seed_frontend_const_element *const_elements;
  size_t const_element_capacity;
  uint8_t *const_bytes;
  size_t const_bytes_capacity;
  /* Append-only typed tuple relations. */
  w_seed_frontend_tuple_component *tuple_components;
  size_t tuple_component_capacity;
  w_seed_frontend_tuple_element *tuple_elements;
  size_t tuple_element_capacity;
} w_seed_frontend_output;

typedef struct {
  w_seed_frontend_status status;
  w_seed_frontend_counts required;
  w_seed_frontend_counts written;
  /* Append-only schema identity for consumers that receive records directly
   * instead of reparsing the frontend receipt. */
  w_seed_frontend_text schema_version;
  size_t barrier_document;
  w_seed_span barrier_span;
  size_t primary_diagnostic;
  size_t receipt_bytes;
} w_seed_frontend_result;

/* Measure all caller-owned output requirements without writing any output. */
w_seed_frontend_status w_seed_frontend_measure(
    const w_seed_frontend_input *input, w_seed_frontend_counts *counts,
    w_seed_frontend_result *result);

/* Normalize complete CST documents, resolve bounded symbols, and type-check. */
w_seed_frontend_status w_seed_frontend_run(
    const w_seed_frontend_input *input, w_seed_frontend_output *output,
    w_seed_frontend_result *result);

#ifdef __cplusplus
}
#endif

#endif
