#ifndef W_SEED_FRONTEND_H
#define W_SEED_FRONTEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_parser.h"
#include "w_seed_sha256.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal seed frontend. It is not a public W command or compiler driver. */
#define W_SEED_FRONTEND_SCHEMA_VERSION "w-seed-frontend-2"
#define W_SEED_FRONTEND_NONE UINT32_MAX
#define W_SEED_FRONTEND_NONE_SIZE SIZE_MAX
#define W_SEED_FRONTEND_MAX_CST_NODES 8192u
#define W_SEED_FRONTEND_MAX_NESTING 256u
#define W_SEED_FRONTEND_MAX_DOCUMENTS 256u
#define W_SEED_FRONTEND_MAX_EXTERNAL_MODULES 256u
#define W_SEED_FRONTEND_MAX_EXTERNAL_SYMBOLS 4096u
#define W_SEED_FRONTEND_MAX_EXTERNAL_PARAMETERS 4096u
/* D1 uses an explicit 64-bit target profile.  This is a semantic target
 * fact, not a query of the host compiler's size_t width; changing it changes
 * normalized usize types and therefore the frontend/ConstIR receipt key. */
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
} w_seed_frontend_fact_kind;

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
} w_seed_frontend_type_kind;

typedef enum {
  W_SEED_FRONTEND_DECL_STRUCT = 0,
  W_SEED_FRONTEND_DECL_TYPE,
  W_SEED_FRONTEND_DECL_ALIAS,
  W_SEED_FRONTEND_DECL_FUNCTION,
  W_SEED_FRONTEND_DECL_ENUM,
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
} w_seed_frontend_expr_kind;

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
  /* Append-only structured control statements for ConstIR. */
  W_SEED_FRONTEND_STMT_GUARD,
  W_SEED_FRONTEND_STMT_FOR,
} w_seed_frontend_stmt_kind;

typedef struct {
  w_seed_frontend_text logical_source_id;
  w_seed_frontend_text module_id;
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
  W_SEED_FRONTEND_LABEL_NAMED_REQUIRED,
  W_SEED_FRONTEND_LABEL_EXTERNAL_REQUIRED,
  W_SEED_FRONTEND_LABEL_OPTIONAL,
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
} w_seed_frontend_external_symbol;

typedef struct {
  w_seed_frontend_text module_id;
  const w_seed_frontend_external_symbol *symbols;
  size_t symbol_count;
} w_seed_frontend_external_module;

typedef struct {
  const w_seed_frontend_document *documents;
  size_t document_count;
  const w_seed_frontend_external_module *external_modules;
  size_t external_module_count;
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
  size_t arguments;
  size_t symbols;
  size_t facts;
  size_t diagnostics;
  size_t receipt_bytes;
  /* Append-only enum declaration/case/payload counts. */
  size_t enums;
  size_t enum_cases;
  size_t enum_case_parameters;
  size_t switch_arms;
  size_t enum_subset_members;
  /* Append-only membership case records. */
  size_t enum_membership_cases;
  /* Append-only generic declaration parameter records. */
  size_t generic_parameters;
} w_seed_frontend_counts;

typedef struct {
  w_seed_frontend_text source_id;
  w_seed_frontend_text module_id;
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
} w_seed_frontend_module;

typedef struct {
  uint32_t module_index;
  w_seed_frontend_text path;
  w_seed_frontend_text alias;
  w_seed_span span;
  uint32_t first_item;
  uint32_t item_count;
} w_seed_frontend_import;

typedef struct {
  uint32_t module_index;
  w_seed_frontend_text name;
  w_seed_frontend_text local_name;
  w_seed_span span;
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
 * record contains declaration schema only.  It does not contain a static
 * argument or a ConstIR result. */
typedef struct {
  uint32_t module_index;
  w_seed_frontend_decl_kind owner_kind;
  uint32_t owner_index;
  uint32_t ordinal;
  /* Empty for positional and optional-label parameters. */
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
  /* Append-only closed-enum identity and normalized case-set fields. */
  uint32_t enum_base_index;
  uint32_t first_subset_member;
  uint32_t subset_member_count;
} w_seed_frontend_type;

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
  uint32_t first_statement;
  uint32_t statement_count;
  /* Append-only const capability and D0 body support flags. */
  bool is_const;
  bool const_body_supported;
} w_seed_frontend_function;

typedef struct {
  uint32_t module_index;
  w_seed_frontend_text target;
  w_seed_span span;
  bool valid;
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
  uint32_t owner_expression;
  w_seed_frontend_text label;
  w_seed_span span;
  uint32_t expression_index;
  /* Append-only frontend resolution fact for ConstIR call lowering. */
  uint32_t resolved_parameter_ordinal;
} w_seed_frontend_argument;

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
} w_seed_frontend_switch_arm;

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
  /* Append-only enum/switch identity fields. */
  uint32_t enum_index;
  uint32_t enum_case_index;
  uint32_t first_switch_arm;
  uint32_t switch_arm_count;
  /* Append-only enum membership case range. */
  uint32_t first_membership_case;
  uint32_t membership_case_count;
  /* Append-only typed literal projections. ConstIR consumes these fields
   * without reparsing source spelling. integer_value is a non-negative
   * magnitude in canonical little-endian order with unused high bytes zero. */
  bool has_bool_value;
  bool bool_value;
  bool has_integer_value;
  uint8_t integer_value[16];
  /* Append-only frontend resolution facts for ConstIR lowering. */
  uint32_t resolved_parameter_ordinal;
  uint32_t resolved_function_index;
  uint32_t resolved_local_ordinal;
  w_seed_frontend_text member_name;
} w_seed_frontend_expression;

typedef enum {
  W_SEED_FRONTEND_DIAGNOSTIC_SEMANTIC = 0,
  W_SEED_FRONTEND_DIAGNOSTIC_TYPE,
  W_SEED_FRONTEND_DIAGNOSTIC_LABEL,
} w_seed_frontend_diagnostic_kind;

typedef struct {
  w_seed_frontend_diagnostic_kind kind;
  w_seed_frontend_text code;
  w_seed_frontend_text actual;
  w_seed_frontend_text expected;
  w_seed_frontend_text declaration;
  w_seed_frontend_text label;
  w_seed_frontend_text accepted_forms;
  w_seed_span primary;
  size_t document_index;
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
  w_seed_frontend_symbol *symbols;
  size_t symbol_capacity;
  w_seed_frontend_fact *facts;
  size_t fact_capacity;
  w_seed_frontend_diagnostic *diagnostics;
  size_t diagnostic_capacity;
  uint8_t *receipt;
  size_t receipt_capacity;
  /* Append-only enum output arrays. */
  w_seed_frontend_enum *enums;
  size_t enum_capacity;
  w_seed_frontend_enum_case *enum_cases;
  size_t enum_case_capacity;
  w_seed_frontend_enum_case_parameter *enum_case_parameters;
  size_t enum_case_parameter_capacity;
  /* Append-only switch-arm output arrays. */
  w_seed_frontend_switch_arm *switch_arms;
  size_t switch_arm_capacity;
  /* Append-only normalized enum-subset member records. */
  w_seed_frontend_enum_subset_member *enum_subset_members;
  size_t enum_subset_member_capacity;
  /* Append-only enum membership case records. */
  w_seed_frontend_enum_membership_case *enum_membership_cases;
  size_t enum_membership_case_capacity;
  /* Append-only generic declaration parameter records. */
  w_seed_frontend_generic_parameter *generic_parameters;
  size_t generic_parameter_capacity;
} w_seed_frontend_output;

typedef struct {
  w_seed_frontend_status status;
  w_seed_frontend_counts required;
  w_seed_frontend_counts written;
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
