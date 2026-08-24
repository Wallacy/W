#ifndef W_SEED_CONSTIR_H
#define W_SEED_CONSTIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_frontend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal seed-C ConstIR D1. This is not an importable W interface. */
#define W_SEED_CONSTIR_SCHEMA_VERSION "w-seed-constir-2"
#define W_SEED_CONSTIR_NONE UINT32_MAX
#define W_SEED_CONSTIR_INTEGER_BYTES 16u
#define W_SEED_CONSTIR_MAX_PARAMETERS 256u
/* Deterministic implementation ceilings.  SIZE_MAX call quota requests the
 * same bounded policy; it does not remove these limits. */
#define W_SEED_CONSTIR_MAX_CALL_DEPTH 256u
#define W_SEED_CONSTIR_MAX_EVAL_DEPTH 1024u
/* D1 validates borrowed StaticList values before execution.  This ceiling
 * bounds that caller-owned scan independently of the step quota. */
#define W_SEED_CONSTIR_MAX_STATIC_LIST_ELEMENTS 4096u

typedef enum {
  W_SEED_CONSTIR_OK = 0,
  W_SEED_CONSTIR_CAPACITY,
  W_SEED_CONSTIR_INVALID,
} w_seed_constir_status;

typedef enum {
  W_SEED_CONSTIR_NODE_INVALID = 0,
  W_SEED_CONSTIR_NODE_BOOL,
  W_SEED_CONSTIR_NODE_INTEGER,
  W_SEED_CONSTIR_NODE_ENUM_CASE,
  W_SEED_CONSTIR_NODE_PARAMETER,
  W_SEED_CONSTIR_NODE_UNARY,
  W_SEED_CONSTIR_NODE_BINARY,
  W_SEED_CONSTIR_NODE_CALL,
  W_SEED_CONSTIR_NODE_SWITCH,
  W_SEED_CONSTIR_NODE_MEMBERSHIP,
  W_SEED_CONSTIR_NODE_LOCAL,
  W_SEED_CONSTIR_NODE_STATIC_LIST_COUNT,
  W_SEED_CONSTIR_NODE_STATIC_LIST_INDEX,
} w_seed_constir_node_kind;

typedef enum {
  W_SEED_CONSTIR_STATEMENT_INVALID = 0,
  W_SEED_CONSTIR_STATEMENT_RETURN,
  W_SEED_CONSTIR_STATEMENT_GUARD,
  W_SEED_CONSTIR_STATEMENT_IF,
  W_SEED_CONSTIR_STATEMENT_FOR_RANGE,
} w_seed_constir_statement_kind;

typedef enum {
  W_SEED_CONSTIR_OPERATOR_INVALID = 0,
  W_SEED_CONSTIR_OPERATOR_NOT,
  W_SEED_CONSTIR_OPERATOR_NEGATE,
  W_SEED_CONSTIR_OPERATOR_ADD,
  W_SEED_CONSTIR_OPERATOR_SUBTRACT,
  W_SEED_CONSTIR_OPERATOR_MULTIPLY,
  W_SEED_CONSTIR_OPERATOR_DIVIDE,
  W_SEED_CONSTIR_OPERATOR_REMAINDER,
  W_SEED_CONSTIR_OPERATOR_EQUAL,
  W_SEED_CONSTIR_OPERATOR_NOT_EQUAL,
  W_SEED_CONSTIR_OPERATOR_LESS,
  W_SEED_CONSTIR_OPERATOR_LESS_EQUAL,
  W_SEED_CONSTIR_OPERATOR_GREATER,
  W_SEED_CONSTIR_OPERATOR_GREATER_EQUAL,
  W_SEED_CONSTIR_OPERATOR_AND,
  W_SEED_CONSTIR_OPERATOR_OR,
  W_SEED_CONSTIR_OPERATOR_SHIFT_LEFT,
  W_SEED_CONSTIR_OPERATOR_SHIFT_RIGHT,
  W_SEED_CONSTIR_OPERATOR_BIT_AND,
  W_SEED_CONSTIR_OPERATOR_BIT_OR,
  W_SEED_CONSTIR_OPERATOR_BIT_XOR,
  W_SEED_CONSTIR_OPERATOR_POWER,
} w_seed_constir_operator;

typedef enum {
  W_SEED_CONSTIR_DIAGNOSTIC_NONE = 0,
  W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0001,
  W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003,
  W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006,
  /* Published by the caller-owned generic predicate boundary. */
  W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0004,
} w_seed_constir_diagnostic_code;

typedef enum {
  W_SEED_CONSTIR_VALUE_INVALID = 0,
  W_SEED_CONSTIR_VALUE_BOOL,
  W_SEED_CONSTIR_VALUE_INTEGER,
  W_SEED_CONSTIR_VALUE_ENUM,
  W_SEED_CONSTIR_VALUE_STATIC_LIST,
} w_seed_constir_value_kind;

typedef struct {
  const w_seed_frontend_input *frontend_input;
  const w_seed_frontend_output *frontend_output;
  const w_seed_frontend_result *frontend_result;
} w_seed_constir_input;

typedef struct {
  size_t functions;
  size_t parameters;
  size_t nodes;
  size_t call_arguments;
  size_t switch_arms;
  size_t membership_cases;
  size_t statements;
  size_t locals;
  size_t diagnostics;
  size_t receipt_bytes;
} w_seed_constir_counts;

typedef struct {
  w_seed_constir_node_kind kind;
  uint32_t owner_function;
  uint32_t frontend_expression;
  uint32_t type_index;
  w_seed_frontend_type_kind type_kind;
  bool type_is_signed;
  uint16_t type_bit_width;
  uint32_t enum_base_index;
  uint32_t enum_case_index;
  w_seed_span source_span;
  uint32_t left;
  uint32_t right;
  uint32_t parameter_ordinal;
  uint32_t call_target_function;
  uint32_t first_call_argument;
  uint32_t call_argument_count;
  uint32_t first_switch_arm;
  uint32_t switch_arm_count;
  uint32_t first_membership_case;
  uint32_t membership_case_count;
  uint32_t element_type_index;
  uint32_t local_ordinal;
  w_seed_constir_operator normalized_operator;
  bool bool_value;
  /* Canonical little-endian two's-complement sign extension for signed
   * integers and zero extension for unsigned integers, limited to 128 bits. */
  uint8_t integer_value[W_SEED_CONSTIR_INTEGER_BYTES];
} w_seed_constir_node;

typedef struct {
  uint32_t owner_node;
  uint32_t parameter_ordinal;
  uint32_t node_index;
  w_seed_span source_span;
} w_seed_constir_call_argument;

typedef struct {
  uint32_t owner_function;
  uint32_t ordinal;
  uint32_t frontend_parameter;
  uint32_t type_index;
  w_seed_frontend_type_kind type_kind;
  bool type_is_signed;
  uint16_t type_bit_width;
  uint32_t enum_base_index;
  w_seed_span source_span;
} w_seed_constir_parameter;

typedef struct {
  uint32_t owner_node;
  w_seed_frontend_switch_pattern_kind pattern_kind;
  uint32_t enum_base_index;
  uint32_t enum_case_index;
  uint32_t result_node;
  w_seed_span pattern_span;
  w_seed_span source_span;
} w_seed_constir_switch_arm;

typedef struct {
  uint32_t owner_node;
  uint32_t enum_base_index;
  uint32_t enum_case_index;
  w_seed_span source_span;
} w_seed_constir_membership_case;

typedef struct {
  w_seed_constir_statement_kind kind;
  uint32_t owner_function;
  w_seed_span source_span;
  uint32_t expression_node;
  uint32_t condition_node;
  uint32_t first_child;
  uint32_t child_count;
  uint32_t else_child;
  uint32_t next_sibling;
  uint32_t lower_node;
  uint32_t upper_node;
  uint32_t local_ordinal;
  uint32_t local_type_index;
  w_seed_frontend_type_kind local_type_kind;
  bool local_type_is_signed;
  uint16_t local_type_bit_width;
  uint8_t half_open;
} w_seed_constir_statement;

typedef struct {
  uint32_t owner_function;
  uint32_t ordinal;
  uint32_t type_index;
  w_seed_frontend_type_kind type_kind;
  bool type_is_signed;
  uint16_t type_bit_width;
  uint32_t element_type_index;
  w_seed_span source_span;
} w_seed_constir_local;

typedef struct {
  w_seed_constir_diagnostic_code code;
  uint32_t owner_function;
  uint32_t frontend_expression;
  w_seed_span source_span;
} w_seed_constir_diagnostic;

typedef struct {
  uint32_t frontend_function;
  bool lowerable;
  w_seed_span source_span;
  w_seed_span body_span;
  uint32_t first_parameter;
  uint32_t parameter_count;
  uint32_t first_node;
  uint32_t node_count;
  uint32_t root_node;
  uint32_t first_statement;
  uint32_t statement_count;
  uint32_t root_statement;
  uint32_t first_local;
  uint32_t local_count;
  uint32_t diagnostic_index;
  uint8_t body_digest[32];
} w_seed_constir_function;

typedef struct {
  w_seed_constir_function *functions;
  size_t function_capacity;
  w_seed_constir_parameter *parameters;
  size_t parameter_capacity;
  w_seed_constir_node *nodes;
  size_t node_capacity;
  w_seed_constir_call_argument *call_arguments;
  size_t call_argument_capacity;
  w_seed_constir_switch_arm *switch_arms;
  size_t switch_arm_capacity;
  w_seed_constir_membership_case *membership_cases;
  size_t membership_case_capacity;
  w_seed_constir_statement *statements;
  size_t statement_capacity;
  w_seed_constir_local *locals;
  size_t local_capacity;
  w_seed_constir_diagnostic *diagnostics;
  size_t diagnostic_capacity;
  uint8_t *receipt;
  size_t receipt_capacity;
} w_seed_constir_output;

typedef struct {
  w_seed_constir_status status;
  w_seed_constir_counts required;
  w_seed_constir_counts written;
  size_t barrier_function;
  w_seed_span barrier_span;
} w_seed_constir_result;

/* Measure all caller-owned ConstIR output requirements without writing output. */
w_seed_constir_status w_seed_constir_measure(
    const w_seed_constir_input *input, w_seed_constir_counts *counts,
    w_seed_constir_result *result);

/* Lower complete frontend output. Capacity failure leaves every output sentinel. */
w_seed_constir_status w_seed_constir_run(
    const w_seed_constir_input *input, w_seed_constir_output *output,
    w_seed_constir_result *result);

typedef struct w_seed_constir_value {
  w_seed_constir_value_kind kind;
  uint32_t type_index;
  w_seed_frontend_type_kind type_kind;
  bool type_is_signed;
  uint16_t type_bit_width;
  uint32_t enum_base_index;
  uint32_t enum_case_index;
  uint32_t element_type_index;
  const struct w_seed_constir_value *elements;
  size_t element_count;
  bool bool_value;
  /* Canonical little-endian two's-complement sign extension for signed
   * integers and zero extension for unsigned integers, limited to 128 bits. */
  uint8_t integer_value[W_SEED_CONSTIR_INTEGER_BYTES];
} w_seed_constir_value;

typedef struct {
  size_t steps;
  size_t heap_bytes;
  /* Active entry depth is one.  A finite value above
   * W_SEED_CONSTIR_MAX_CALL_DEPTH is INVALID; SIZE_MAX means bounded by the
   * implementation ceiling. */
  size_t call_depth;
  size_t result_bytes;
} w_seed_constir_quota;

typedef struct {
  w_seed_constir_value values[W_SEED_CONSTIR_MAX_PARAMETERS];
  w_seed_constir_value locals[W_SEED_CONSTIR_MAX_PARAMETERS];
} w_seed_constir_eval_frame;

typedef struct {
  w_seed_constir_eval_frame *frames;
  size_t frame_capacity;
} w_seed_constir_eval_workspace;

typedef struct {
  w_seed_constir_status status;
  w_seed_constir_diagnostic_code diagnostic;
  w_seed_span diagnostic_span;
  size_t consumed_steps;
  size_t consumed_heap_bytes;
  size_t consumed_call_depth;
  size_t consumed_result_bytes;
  size_t quota_limit;
} w_seed_constir_eval_result;

typedef struct {
  const w_seed_constir_function *functions;
  size_t function_count;
  const w_seed_constir_parameter *parameters;
  size_t parameter_count;
  const w_seed_constir_node *nodes;
  size_t node_count;
  const w_seed_constir_call_argument *call_arguments;
  size_t call_argument_count;
  const w_seed_constir_switch_arm *switch_arms;
  size_t switch_arm_count;
  const w_seed_constir_membership_case *membership_cases;
  size_t membership_case_count;
  const w_seed_frontend_output *frontend_output;
  const w_seed_frontend_result *frontend_result;
  const w_seed_constir_statement *statements;
  size_t statement_count;
  const w_seed_constir_local *locals;
  size_t local_count;
} w_seed_constir_program;

typedef struct {
  uint32_t function_index;
  const w_seed_constir_value *arguments;
  size_t argument_count;
} w_seed_constir_invocation;

/* Read-only structural preflight used by downstream caller-owned passes.
 * This wrapper shares the evaluator's canonical validator. */
bool w_seed_constir_validate_program(const w_seed_constir_program *program);

/* Validate a batch of invocations with one program preflight and no
 * evaluator quota consumption. */
bool w_seed_constir_validate_invocations(
    const w_seed_constir_program *program,
    const w_seed_constir_invocation *invocations, size_t invocation_count);

/* Validate only invocation relations after the caller has completed the
 * canonical program preflight. */
bool w_seed_constir_validate_invocations_in_validated_program(
    const w_seed_constir_program *program,
    const w_seed_constir_invocation *invocations, size_t invocation_count);

/* Validate one invocation without consuming evaluator quota or writing a
 * result.  This shares the program and parameter/value checks used by the
 * evaluator. */
bool w_seed_constir_validate_invocation(
    const w_seed_constir_program *program, uint32_t function_index,
    const w_seed_constir_value *arguments, size_t argument_count);

/* Evaluate one lowerable function with typed arguments and deterministic quotas. */
w_seed_constir_status w_seed_constir_evaluate(
    const w_seed_constir_program *program, uint32_t function_index,
    const w_seed_constir_value *arguments, size_t argument_count,
    w_seed_constir_quota quota, w_seed_constir_eval_workspace *workspace,
    w_seed_constir_value *value, w_seed_constir_eval_result *result);

/* Helpers create canonical typed scalar arguments without host layout. */
bool w_seed_constir_value_bool(uint32_t type_index, bool value,
                               w_seed_constir_value *out);
bool w_seed_constir_value_integer(uint32_t type_index,
                                  w_seed_frontend_type_kind type_kind,
                                  bool is_signed, uint16_t bit_width,
                                  const uint8_t bytes[W_SEED_CONSTIR_INTEGER_BYTES],
                                  w_seed_constir_value *out);
bool w_seed_constir_value_enum(uint32_t type_index, uint32_t enum_base_index,
                               uint32_t enum_case_index,
                               w_seed_constir_value *out);
bool w_seed_constir_value_static_list(
    uint32_t type_index, uint32_t element_type_index,
    const w_seed_constir_value *elements, size_t element_count,
    w_seed_constir_value *out);

#ifdef __cplusplus
}
#endif

#endif
