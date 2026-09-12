#ifndef W_SEED_HIR0_H
#define W_SEED_HIR0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_frontend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* HIR0 is a closed, bounded, caller-owned intermediate representation for the
 * verified-HIR-backed first executable seed subset. It owns copied names and
 * constant bytes. It does not retain frontend pointers and it does not
 * allocate. */
#define W_SEED_HIR0_SCHEMA_VERSION "w-seed-hir0-23"
#define W_SEED_HIR0_NONE UINT32_MAX
#define W_SEED_HIR0_MAX_NESTING 64u
#define W_SEED_HIR0_MAX_TEXT_BYTES (64u * 1024u)
#define W_SEED_HIR0_MAX_VALUE_BYTES (64u * 1024u)
#define W_SEED_HIR0_MAX_RECEIPT_BYTES 320u

typedef enum {
  W_SEED_HIR0_OK = 0,
  W_SEED_HIR0_FRONTEND,
  W_SEED_HIR0_UNSUPPORTED,
  W_SEED_HIR0_CAPACITY,
  W_SEED_HIR0_INVALID,
} w_seed_hir0_status;

typedef enum {
  W_SEED_HIR0_TYPE_UNIT = 0,
  W_SEED_HIR0_TYPE_STRING,
  W_SEED_HIR0_TYPE_I64,
  W_SEED_HIR0_TYPE_BOOL,
  /* Append-only nominal type with resolver-owned external identity. */
  W_SEED_HIR0_TYPE_NOMINAL,
  /* A caller-owned local closed enum declaration. */
  W_SEED_HIR0_TYPE_ENUM,
} w_seed_hir0_type_kind;

typedef enum {
  W_SEED_HIR0_IDENTITY_MODULE = 0,
  W_SEED_HIR0_IDENTITY_FUNCTION,
  W_SEED_HIR0_IDENTITY_ENTRY,
  W_SEED_HIR0_IDENTITY_HOST_PRELUDE,
} w_seed_hir0_identity_kind;

typedef enum {
  W_SEED_HIR0_INSTRUCTION_CALL = 0,
  W_SEED_HIR0_INSTRUCTION_BINDING,
} w_seed_hir0_instruction_kind;

typedef enum {
  W_SEED_HIR0_LABEL_POSITIONAL_ONLY = 0,
  W_SEED_HIR0_LABEL_REQUIRED,
} w_seed_hir0_label_kind;

typedef enum {
  W_SEED_HIR0_VALUE_CONST_STRING = 0,
  W_SEED_HIR0_VALUE_BINDING_READ,
  W_SEED_HIR0_VALUE_PARAMETER_READ,
  W_SEED_HIR0_VALUE_CONST_I64,
  W_SEED_HIR0_VALUE_CONST_BOOL,
  /* Both operands are i64. Arithmetic returns i64; comparisons return Bool. */
  W_SEED_HIR0_VALUE_BINARY_I64,
  W_SEED_HIR0_VALUE_INTERPOLATED_STRING,
  /* Result of one prior local CALL instruction in the same block. */
  W_SEED_HIR0_VALUE_CALL_RESULT,
  W_SEED_HIR0_VALUE_UNARY_BOOL,
  W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ,
  /* A member of the bounded external ExitCode enum. */
  W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE,
  /* A closed external property read. The receiver is left_value and the
   * external identity/member_name pair names the property. */
  W_SEED_HIR0_VALUE_EXTERNAL_MEMBER,
  /* Checked signed-i64 unary negation. */
  W_SEED_HIR0_VALUE_UNARY_I64,
  /* A case of a caller-owned local enum. HIR0 values remain payloadless until
   * constructor payload relations are added. */
  W_SEED_HIR0_VALUE_ENUM_CASE,
} w_seed_hir0_value_kind;

typedef enum {
  W_SEED_HIR0_VALUE_OWNER_ARGUMENT = 0,
  W_SEED_HIR0_VALUE_OWNER_BINDING,
  W_SEED_HIR0_VALUE_OWNER_BINARY,
  W_SEED_HIR0_VALUE_OWNER_INTERPOLATION_SEGMENT,
  W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
  W_SEED_HIR0_VALUE_OWNER_UNARY,
  /* Append-only owners for the closed process external-value children. */
  W_SEED_HIR0_VALUE_OWNER_EXTERNAL_MEMBER,
  W_SEED_HIR0_VALUE_OWNER_EXTERNAL_ENUM_CASE,
} w_seed_hir0_value_owner_kind;

typedef enum {
  W_SEED_HIR0_BINARY_ADD = 0,
  W_SEED_HIR0_BINARY_SUBTRACT,
  W_SEED_HIR0_BINARY_MULTIPLY,
  W_SEED_HIR0_BINARY_DIVIDE,
  W_SEED_HIR0_BINARY_REMAINDER,
  W_SEED_HIR0_BINARY_EQUAL,
  W_SEED_HIR0_BINARY_NOT_EQUAL,
  W_SEED_HIR0_BINARY_LESS,
  W_SEED_HIR0_BINARY_LESS_EQUAL,
  W_SEED_HIR0_BINARY_GREATER,
  W_SEED_HIR0_BINARY_GREATER_EQUAL,
} w_seed_hir0_binary_operator;

typedef enum {
  W_SEED_HIR0_UNARY_NOT = 0,
  W_SEED_HIR0_UNARY_NEGATE,
} w_seed_hir0_unary_operator;

typedef enum {
  W_SEED_HIR0_LOGICAL_NONE = 0,
  W_SEED_HIR0_LOGICAL_AND,
  W_SEED_HIR0_LOGICAL_OR,
} w_seed_hir0_logical_operator;

typedef enum {
  W_SEED_HIR0_INTERPOLATION_TEXT = 0,
  W_SEED_HIR0_INTERPOLATION_VALUE,
} w_seed_hir0_interpolation_segment_kind;

typedef enum {
  W_SEED_HIR0_TERMINATOR_RETURN_UNIT = 0,
  W_SEED_HIR0_TERMINATOR_RETURN_VALUE,
  W_SEED_HIR0_TERMINATOR_BRANCH,
  W_SEED_HIR0_TERMINATOR_JUMP,
  /* Closed local payloadless-enum exhaustive dispatch. */
  W_SEED_HIR0_TERMINATOR_SWITCH_ENUM,
} w_seed_hir0_terminator_kind;

typedef enum {
  W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY = 0,
} w_seed_hir0_requirement_owner_kind;

typedef enum {
  W_SEED_HIR0_EXTERNAL_VALUE = 0,
  W_SEED_HIR0_EXTERNAL_TYPE,
} w_seed_hir0_external_kind;

/* External parameters are closed ABI facts, not an open call-signature
 * promise. The only non-empty contract currently represented is the bounded
 * std.process.ExitCode.failure(code: i64) constructor. */
typedef enum {
  W_SEED_HIR0_EXTERNAL_PARAMETER_NONE = 0,
  W_SEED_HIR0_EXTERNAL_PARAMETER_PROCESS_FAILURE_I64,
} w_seed_hir0_external_parameter_abi;

typedef enum {
  /* Existing/default zero-argument Unit entry adapter. */
  W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT = 0,
  /* Bounded native-process handler adapter. */
  W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS,
} w_seed_hir0_entry_adapter_kind;

/* A function's common entry suspension summary.  An explicit async
 * declaration publishes MAY even when its separate direct-entry proof is
 * available. */
typedef enum {
  W_SEED_HIR0_SUSPENSION_NEVER = 0,
  W_SEED_HIR0_SUSPENSION_MAY,
} w_seed_hir0_suspension_kind;

/* Explicit W-1484 direct-entry facet.  This is a proof result, not a product
 * profile selection or an optimizer reachability fact. */
typedef enum {
  W_SEED_HIR0_DIRECT_ENTRY_ABSENT = 0,
  W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE,
} w_seed_hir0_direct_entry_kind;

/* HIR-owned lifecycle facts are deliberately closed.  UNKNOWN is retained
 * for strings, opaque nominals, and any future type; it is never a permission
 * to treat an owner as a scalar. */
typedef enum {
  W_SEED_HIR0_LIFECYCLE_UNKNOWN = 0,
  W_SEED_HIR0_LIFECYCLE_VALUE_COPY,
  W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER,
} w_seed_hir0_lifecycle_kind;

/* A release contract is a compiler-owned, versioned external ABI fact.  The
 * process value is limited to the exact closed std.process@1 records and does
 * not claim that the provider has been implemented or executed. */
typedef enum {
  W_SEED_HIR0_RELEASE_CONTRACT_NONE = 0,
  W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN,
  W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE,
} w_seed_hir0_release_contract_kind;

/* Normal-return cleanup obligations are separate from root drain/reclamation
 * performed by the native adapter outside the handler. */
typedef enum {
  W_SEED_HIR0_ENTRY_CLEANUP_NONE = 0,
  W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS,
} w_seed_hir0_entry_cleanup_kind;

typedef struct {
  uint32_t offset;
  uint32_t count;
} w_seed_hir0_text;

typedef struct {
  w_seed_hir0_identity_kind kind;
  uint32_t owner_module;
  uint32_t target_index;
  w_seed_hir0_text name;
  /* Function identities use the function-parameter range. Host-prelude
   * identities use the host-parameter range. Other identities contain
   * W_SEED_HIR0_NONE and zero counts. */
  uint32_t first_parameter;
  uint32_t parameter_count;
  uint32_t first_requirement;
  uint32_t requirement_count;
  uint32_t return_type;
  bool is_const;
  /* Only HOST_PRELUDE identities use profile. */
  w_seed_hir0_text profile;
} w_seed_hir0_identity;

typedef struct {
  w_seed_hir0_type_kind kind;
  uint32_t owner_module;
  w_seed_hir0_text name;
  /* Present only for TYPE_NOMINAL. The pair indexes caller-copied external
   * records and is independent of source-local aliases. */
  uint32_t external_module_index;
  uint32_t external_symbol_index;
  /* Present only for TYPE_ENUM and indexes the caller-owned enum record. */
  uint32_t enum_index;
  /* Independently rederived from the closed external identity contract. */
  w_seed_hir0_lifecycle_kind lifecycle;
  w_seed_hir0_release_contract_kind release_contract;
} w_seed_hir0_type;

/* Local enum declarations, cases, and case parameters are copied into
 * HIR-owned records. The ranges are lexical and dense; tag is the stable case
 * ordinal. A payloadless enum therefore pays for no payload records. */
typedef struct {
  uint32_t module_index;
  uint32_t type_index;
  w_seed_hir0_text name;
  uint32_t first_case;
  uint32_t case_count;
  w_seed_span source_span;
} w_seed_hir0_enum;

typedef struct {
  uint32_t owner_enum;
  uint32_t ordinal;
  uint32_t tag;
  uint32_t first_payload;
  uint32_t payload_count;
  w_seed_hir0_text name;
  w_seed_span source_span;
} w_seed_hir0_enum_case;

typedef struct {
  uint32_t owner_case;
  uint32_t ordinal;
  uint32_t type_index;
  w_seed_hir0_text label;
  bool has_label;
  w_seed_span source_span;
} w_seed_hir0_enum_case_parameter;

typedef struct {
  uint32_t module_index;
  w_seed_hir0_text module_id;
  uint32_t first_symbol;
  uint32_t symbol_count;
} w_seed_hir0_external_module;

typedef struct {
  uint32_t module_index;
  uint32_t ordinal;
  w_seed_hir0_text name;
  w_seed_hir0_external_kind kind;
  bool exported;
  bool is_const;
  w_seed_hir0_text receiver_type;
  w_seed_hir0_text return_type;
  /* HIR16 accepts only the closed parameter ABI above. */
  uint32_t parameter_count;
  w_seed_hir0_external_parameter_abi parameter_abi;
} w_seed_hir0_external_symbol;

typedef struct {
  uint32_t module_index;
  uint32_t identity_index;
  w_seed_hir0_text source_id;
  w_seed_hir0_text module_id;
  w_seed_hir0_text local_module_name;
  w_seed_span source_span;
  size_t source_length;
  uint8_t source_sha256[32];
  uint32_t first_function;
  uint32_t function_count;
  uint32_t first_entry;
  uint32_t entry_count;
} w_seed_hir0_module;

typedef struct {
  uint32_t module_index;
  uint32_t identity_index;
  w_seed_hir0_text name;
  bool exported;
  w_seed_span source_span;
  w_seed_span body_span;
  uint32_t return_type;
  uint32_t first_parameter;
  uint32_t parameter_count;
  uint32_t first_block;
  uint32_t block_count;
  bool is_const;
  bool is_async;
  bool is_throws;
  bool is_unsafe;
  bool has_borrow_clause;
  /* True only for the private function synthesized from `entry { ... }`. */
  bool is_anonymous_entry;
  /* Append-only W-1484 facts.  `suspension` is the common entry summary;
   * `direct_entry` is independently rederived from the complete body. */
  w_seed_hir0_suspension_kind suspension;
  w_seed_hir0_direct_entry_kind direct_entry;
} w_seed_hir0_function;

typedef struct {
  uint32_t owner_function;
  uint32_t ordinal;
  uint32_t type_index;
  w_seed_hir0_text name;
  w_seed_hir0_text label;
  w_seed_hir0_label_kind label_kind;
  w_seed_span source_span;
} w_seed_hir0_parameter;

typedef struct {
  uint32_t owner_function;
  uint32_t ordinal;
  uint32_t first_instruction;
  uint32_t instruction_count;
  uint32_t terminator_index;
  w_seed_span source_span;
  /* next_block is reserved. CFG edges exist only in terminators. */
  uint32_t next_block;
  uint32_t first_block_argument;
  uint32_t block_argument_count;
} w_seed_hir0_block;

typedef struct {
  uint32_t owner_block;
  uint32_t ordinal;
  uint32_t type_index;
  w_seed_span source_span;
} w_seed_hir0_block_argument;

/* A jump's operands are explicit edge arguments.  Records are contiguous in
 * terminator order and pair with the destination block arguments by ordinal.
 * Values remain independently indexed, so nested value trees need not be
 * laid out in root order. */
typedef struct {
  uint32_t owner_terminator;
  uint32_t owner_block;
  uint32_t ordinal;
  uint32_t value_index;
  uint32_t type_index;
  w_seed_span source_span;
} w_seed_hir0_edge_argument;

/* Dense, canonical declaration-order case edges owned by a SWITCH_ENUM
 * terminator.  The enum and
 * case identities are retained independently of the carrier tag so a
 * verifier can prove that dispatch coverage is exact. */
typedef struct {
  uint32_t owner_terminator;
  uint32_t ordinal;
  uint32_t enum_index;
  uint32_t enum_case_index;
  uint32_t target_block;
  w_seed_span source_span;
} w_seed_hir0_switch_edge;

typedef struct {
  w_seed_hir0_instruction_kind kind;
  uint32_t owner_block;
  uint32_t ordinal;
  uint32_t call_index;
  uint32_t binding_index;
  uint32_t result_type;
  w_seed_span source_span;
} w_seed_hir0_instruction;

typedef struct {
  uint32_t owner_instruction;
  uint32_t owner_block;
  uint32_t ordinal;
  uint32_t type_index;
  w_seed_hir0_text name;
  bool is_mutable;
  /* Every declaration is its own source_binding.  A later SSA version points
   * back to the original mutable declaration; no runtime storage is implied. */
  uint32_t source_binding;
  /* NONE on a declaration; otherwise a preceding SSA version of the same
   * source binding. Lowering emits the latest predecessor. */
  uint32_t previous_version;
  /* NONE on the current version; otherwise the unique following version.
   * Bidirectional edges keep independent verification linear. */
  uint32_t next_version;
  uint32_t initializer_value;
  w_seed_span source_span;
} w_seed_hir0_binding;

typedef struct {
  uint32_t owner_identity;
  uint32_t ordinal;
  uint32_t type_index;
  w_seed_hir0_text name;
  w_seed_hir0_text label;
  w_seed_hir0_label_kind label_kind;
} w_seed_hir0_host_parameter;

typedef struct {
  w_seed_hir0_requirement_owner_kind owner_kind;
  uint32_t owner_index;
  uint32_t ordinal;
  w_seed_hir0_text name;
} w_seed_hir0_requirement;

typedef struct {
  uint32_t owner_instruction;
  uint32_t owner_block;
  uint32_t ordinal;
  uint32_t callee_identity;
  uint32_t first_argument;
  uint32_t argument_count;
  uint32_t first_requirement;
  uint32_t requirement_count;
  uint32_t result_type;
  w_seed_span source_span;
} w_seed_hir0_call;

typedef struct {
  uint32_t owner_call;
  /* ordinal preserves source evaluation order. parameter_ordinal identifies
   * the declaration/ABI slot. They can differ for named arguments. */
  uint32_t ordinal;
  uint32_t parameter_ordinal;
  uint32_t value_index;
  uint32_t type_index;
  w_seed_hir0_text label;
  w_seed_hir0_label_kind label_kind;
  w_seed_span source_span;
} w_seed_hir0_argument;

typedef struct {
  w_seed_hir0_value_kind kind;
  w_seed_hir0_value_owner_kind owner_kind;
  uint32_t owner_index;
  uint32_t owner_ordinal;
  uint32_t type_index;
  uint32_t binding_index;
  uint32_t parameter_index;
  uint32_t call_index;
  uint32_t left_value;
  uint32_t right_value;
  uint32_t first_interpolation_segment;
  uint32_t interpolation_segment_count;
  w_seed_hir0_binary_operator binary_operator;
  w_seed_hir0_unary_operator unary_operator;
  uint32_t block_argument_index;
  int64_t integer_value;
  bool bool_value;
  uint32_t byte_offset;
  uint32_t byte_count;
  w_seed_span source_span;
  /* Present only for external member/enum values. The pair identifies the
   * caller-copied external symbol and member_name carries its canonical name.
   * For VALUE_EXTERNAL_ENUM_CASE, left_value is the explicit payload child for
   * the closed failure constructor and NONE for success. */
  uint32_t external_module_index;
  uint32_t external_symbol_index;
  w_seed_hir0_text member_name;
  /* Present only for VALUE_ENUM_CASE. */
  uint32_t enum_index;
  uint32_t enum_case_index;
} w_seed_hir0_value;

typedef struct {
  w_seed_hir0_interpolation_segment_kind kind;
  uint32_t owner_value;
  uint32_t ordinal;
  uint32_t value_index;
  uint32_t byte_offset;
  uint32_t byte_count;
  w_seed_span source_span;
} w_seed_hir0_interpolation_segment;

typedef struct {
  uint32_t owner_block;
  w_seed_hir0_terminator_kind kind;
  uint32_t ordinal;
  uint32_t value_index;
  uint32_t result_type;
  /* BRANCH uses target_block and else_block. JUMP uses target_block and
   * requires else_block to be W_SEED_HIR0_NONE. RETURN uses neither field. */
  uint32_t target_block;
  uint32_t else_block;
  uint32_t first_edge_argument;
  uint32_t edge_argument_count;
  w_seed_hir0_logical_operator logical_operator;
  /* SWITCH_ENUM uses value_index as its subject and records the nominal enum
   * plus its internal carrier width and dense edge range here. */
  uint32_t switch_enum_index;
  uint32_t first_switch_edge;
  uint32_t switch_edge_count;
  uint32_t switch_carrier_width;
  w_seed_span source_span;
} w_seed_hir0_terminator;

typedef struct {
  uint32_t module_index;
  uint32_t identity_index;
  uint32_t target_function;
  uint32_t target_identity;
  w_seed_hir0_text target_name;
  w_seed_hir0_text slot;
  w_seed_span source_span;
  /* True when this descriptor owns an inline short-entry body. */
  bool is_body;
  /* Explicit adapter compatibility. Consumers must not infer this from
   * function names or signature spelling. */
  w_seed_hir0_entry_adapter_kind adapter_kind;
  /* The handler releases each listed root owner exactly once on the supported
   * normal return. Root drain and memory reclamation remain outside HIR. */
  w_seed_hir0_entry_cleanup_kind cleanup_obligation;
  uint32_t first_cleanup_owner_parameter;
  uint32_t cleanup_owner_parameter_count;
} w_seed_hir0_entry;

typedef struct {
  size_t modules;
  size_t identities;
  size_t types;
  size_t functions;
  size_t parameters;
  size_t blocks;
  size_t block_arguments;
  size_t edge_arguments;
  size_t switch_edges;
  size_t instructions;
  size_t bindings;
  size_t calls;
  size_t host_parameters;
  size_t arguments;
  size_t requirements;
  size_t values;
  size_t interpolation_segments;
  size_t terminators;
  size_t entries;
  size_t text_bytes;
  size_t value_bytes;
  size_t receipt_bytes;
  size_t external_modules;
  size_t external_symbols;
  size_t enums;
  size_t enum_cases;
  size_t enum_case_parameters;
} w_seed_hir0_counts;

/* A program carries capacities so the verifier can reject a truncated or
 * forged caller-owned record set before it follows any relation. */
typedef struct {
  const w_seed_hir0_module *modules;
  size_t module_count;
  size_t module_capacity;
  const w_seed_hir0_identity *identities;
  size_t identity_count;
  size_t identity_capacity;
  const w_seed_hir0_type *types;
  size_t type_count;
  size_t type_capacity;
  const w_seed_hir0_enum *enums;
  size_t enum_count;
  size_t enum_capacity;
  const w_seed_hir0_enum_case *enum_cases;
  size_t enum_case_count;
  size_t enum_case_capacity;
  const w_seed_hir0_enum_case_parameter *enum_case_parameters;
  size_t enum_case_parameter_count;
  size_t enum_case_parameter_capacity;
  const w_seed_hir0_function *functions;
  size_t function_count;
  size_t function_capacity;
  const w_seed_hir0_parameter *parameters;
  size_t parameter_count;
  size_t parameter_capacity;
  const w_seed_hir0_block *blocks;
  size_t block_count;
  size_t block_capacity;
  const w_seed_hir0_block_argument *block_arguments;
  size_t block_argument_count;
  size_t block_argument_capacity;
  const w_seed_hir0_edge_argument *edge_arguments;
  size_t edge_argument_count;
  size_t edge_argument_capacity;
  const w_seed_hir0_switch_edge *switch_edges;
  size_t switch_edge_count;
  size_t switch_edge_capacity;
  const w_seed_hir0_instruction *instructions;
  size_t instruction_count;
  size_t instruction_capacity;
  const w_seed_hir0_binding *bindings;
  size_t binding_count;
  size_t binding_capacity;
  const w_seed_hir0_call *calls;
  size_t call_count;
  size_t call_capacity;
  const w_seed_hir0_host_parameter *host_parameters;
  size_t host_parameter_count;
  size_t host_parameter_capacity;
  const w_seed_hir0_argument *arguments;
  size_t argument_count;
  size_t argument_capacity;
  const w_seed_hir0_requirement *requirements;
  size_t requirement_count;
  size_t requirement_capacity;
  const w_seed_hir0_value *values;
  size_t value_count;
  size_t value_capacity;
  const w_seed_hir0_interpolation_segment *interpolation_segments;
  size_t interpolation_segment_count;
  size_t interpolation_segment_capacity;
  const w_seed_hir0_terminator *terminators;
  size_t terminator_count;
  size_t terminator_capacity;
  const w_seed_hir0_entry *entries;
  size_t entry_count;
  size_t entry_capacity;
  const uint8_t *text_bytes;
  size_t text_byte_count;
  size_t text_byte_capacity;
  const uint8_t *value_bytes;
  size_t value_byte_count;
  size_t value_byte_capacity;
  const uint8_t *receipt;
  size_t receipt_count;
  size_t receipt_capacity;
  const w_seed_hir0_external_module *external_modules;
  size_t external_module_count;
  size_t external_module_capacity;
  const w_seed_hir0_external_symbol *external_symbols;
  size_t external_symbol_count;
  size_t external_symbol_capacity;
} w_seed_hir0_program;

typedef struct {
  w_seed_hir0_module *modules;
  size_t module_capacity;
  w_seed_hir0_identity *identities;
  size_t identity_capacity;
  w_seed_hir0_type *types;
  size_t type_capacity;
  w_seed_hir0_enum *enums;
  size_t enum_capacity;
  w_seed_hir0_enum_case *enum_cases;
  size_t enum_case_capacity;
  w_seed_hir0_enum_case_parameter *enum_case_parameters;
  size_t enum_case_parameter_capacity;
  w_seed_hir0_function *functions;
  size_t function_capacity;
  w_seed_hir0_parameter *parameters;
  size_t parameter_capacity;
  w_seed_hir0_block *blocks;
  size_t block_capacity;
  w_seed_hir0_block_argument *block_arguments;
  size_t block_argument_capacity;
  w_seed_hir0_edge_argument *edge_arguments;
  size_t edge_argument_capacity;
  w_seed_hir0_switch_edge *switch_edges;
  size_t switch_edge_capacity;
  w_seed_hir0_instruction *instructions;
  size_t instruction_capacity;
  w_seed_hir0_binding *bindings;
  size_t binding_capacity;
  w_seed_hir0_call *calls;
  size_t call_capacity;
  w_seed_hir0_host_parameter *host_parameters;
  size_t host_parameter_capacity;
  w_seed_hir0_argument *arguments;
  size_t argument_capacity;
  w_seed_hir0_requirement *requirements;
  size_t requirement_capacity;
  w_seed_hir0_value *values;
  size_t value_capacity;
  w_seed_hir0_interpolation_segment *interpolation_segments;
  size_t interpolation_segment_capacity;
  w_seed_hir0_terminator *terminators;
  size_t terminator_capacity;
  w_seed_hir0_entry *entries;
  size_t entry_capacity;
  uint8_t *text_bytes;
  size_t text_byte_capacity;
  uint8_t *value_bytes;
  size_t value_byte_capacity;
  uint8_t *receipt;
  size_t receipt_capacity;
  w_seed_hir0_external_module *external_modules;
  size_t external_module_capacity;
  w_seed_hir0_external_symbol *external_symbols;
  size_t external_symbol_capacity;
} w_seed_hir0_output;

typedef struct {
  w_seed_hir0_status status;
  w_seed_hir0_counts required;
  w_seed_hir0_counts written;
  char schema[sizeof(W_SEED_HIR0_SCHEMA_VERSION)];
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
} w_seed_hir0_result;

/* Frontend records are consumed once and all semantic payload is copied into
 * the HIR output. The frontend is never needed by a verified HIR0 program. */
typedef struct {
  const w_seed_frontend_input *frontend_input;
  const w_seed_frontend_output *frontend_output;
  const w_seed_frontend_result *frontend_result;
} w_seed_hir0_input;

w_seed_hir0_status w_seed_hir0_measure(const w_seed_hir0_input *input,
                                       w_seed_hir0_counts *counts,
                                       w_seed_hir0_result *result);

w_seed_hir0_status w_seed_hir0_run(const w_seed_hir0_input *input,
                                   w_seed_hir0_output *output,
                                   w_seed_hir0_result *result);

/* Verify the complete immutable program, including capacities, ownership,
 * relation ranges, schema receipt, and a field-by-field digest. */
bool w_seed_hir0_verify(const w_seed_hir0_program *program,
                        const w_seed_hir0_result *result);

/* Convert a successful output/result pair into a read-only program view. */
bool w_seed_hir0_program_from_output(const w_seed_hir0_output *output,
                                     const w_seed_hir0_result *result,
                                     w_seed_hir0_program *program);

#ifdef __cplusplus
}
#endif

#endif
