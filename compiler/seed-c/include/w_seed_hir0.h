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
#define W_SEED_HIR0_SCHEMA_VERSION "w-seed-hir0-3"
#define W_SEED_HIR0_NONE UINT32_MAX
#define W_SEED_HIR0_MAX_TEXT_BYTES (64u * 1024u)
#define W_SEED_HIR0_MAX_VALUE_BYTES (64u * 1024u)
#define W_SEED_HIR0_MAX_RECEIPT_BYTES 256u

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
  W_SEED_HIR0_VALUE_CONST_I64,
  W_SEED_HIR0_VALUE_CONST_BOOL,
  W_SEED_HIR0_VALUE_BINARY_I64,
  W_SEED_HIR0_VALUE_INTERPOLATED_STRING,
} w_seed_hir0_value_kind;

typedef enum {
  W_SEED_HIR0_VALUE_OWNER_ARGUMENT = 0,
  W_SEED_HIR0_VALUE_OWNER_BINARY,
  W_SEED_HIR0_VALUE_OWNER_INTERPOLATION_SEGMENT,
} w_seed_hir0_value_owner_kind;

typedef enum {
  W_SEED_HIR0_BINARY_ADD = 0,
  W_SEED_HIR0_BINARY_SUBTRACT,
  W_SEED_HIR0_BINARY_MULTIPLY,
  W_SEED_HIR0_BINARY_DIVIDE,
  W_SEED_HIR0_BINARY_REMAINDER,
} w_seed_hir0_binary_operator;

typedef enum {
  W_SEED_HIR0_INTERPOLATION_TEXT = 0,
  W_SEED_HIR0_INTERPOLATION_VALUE,
} w_seed_hir0_interpolation_segment_kind;

typedef enum {
  W_SEED_HIR0_TERMINATOR_RETURN_UNIT = 0,
} w_seed_hir0_terminator_kind;

typedef enum {
  W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY = 0,
} w_seed_hir0_requirement_owner_kind;

typedef struct {
  uint32_t offset;
  uint32_t count;
} w_seed_hir0_text;

typedef struct {
  w_seed_hir0_identity_kind kind;
  uint32_t owner_module;
  uint32_t target_index;
  w_seed_hir0_text name;
  /* Only host-prelude identities use these ranges. Other identities must
   * contain W_SEED_HIR0_NONE and zero counts. */
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
} w_seed_hir0_type;

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
  /* HIR0 has a single linear block per function. next_block is reserved for
   * a later CFG schema and must remain W_SEED_HIR0_NONE in this schema. */
  uint32_t next_block;
} w_seed_hir0_block;

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
  uint32_t byte_offset;
  uint32_t byte_count;
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
  uint32_t ordinal;
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
  uint32_t left_value;
  uint32_t right_value;
  uint32_t first_interpolation_segment;
  uint32_t interpolation_segment_count;
  w_seed_hir0_binary_operator binary_operator;
  int64_t integer_value;
  bool bool_value;
  uint32_t byte_offset;
  uint32_t byte_count;
  w_seed_span source_span;
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
} w_seed_hir0_entry;

typedef struct {
  size_t modules;
  size_t identities;
  size_t types;
  size_t functions;
  size_t parameters;
  size_t blocks;
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
  const w_seed_hir0_function *functions;
  size_t function_count;
  size_t function_capacity;
  const w_seed_hir0_parameter *parameters;
  size_t parameter_count;
  size_t parameter_capacity;
  const w_seed_hir0_block *blocks;
  size_t block_count;
  size_t block_capacity;
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
} w_seed_hir0_program;

typedef struct {
  w_seed_hir0_module *modules;
  size_t module_capacity;
  w_seed_hir0_identity *identities;
  size_t identity_capacity;
  w_seed_hir0_type *types;
  size_t type_capacity;
  w_seed_hir0_function *functions;
  size_t function_capacity;
  w_seed_hir0_parameter *parameters;
  size_t parameter_capacity;
  w_seed_hir0_block *blocks;
  size_t block_capacity;
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
