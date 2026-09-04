#ifndef W_SEED_NATIVE0_H
#define W_SEED_NATIVE0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_frontend.h"
#include "w_seed_hir0.h"
#include "w_seed_mlir0.h"
#include "w_seed_parser.h"
#include "w_seed_source.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Native0 is the bounded source-to-MLIR0 adapter used by the seed gate. It
 * reads one explicit file path, retains no heap state, and uses only storage
 * supplied by its caller. */
#define W_SEED_NATIVE0_SCHEMA_VERSION "w-seed-native0-2"
#define W_SEED_NATIVE0_MAX_SOURCE_BYTES 4096u
#define W_SEED_NATIVE0_MAX_PATH_BYTES 4096u
#define W_SEED_NATIVE0_MAX_SOURCE_ID_BYTES 4096u

enum {
  W_SEED_NATIVE0_LEXER_FRAMES = 256,
  W_SEED_NATIVE0_TOKENS = 2048,
  W_SEED_NATIVE0_NODES = 4096,
  W_SEED_NATIVE0_PARSE_FRAMES = 2048,
  W_SEED_NATIVE0_ISSUES = 128,
  W_SEED_NATIVE0_MODULES = 8,
  W_SEED_NATIVE0_IMPORTS = 16,
  W_SEED_NATIVE0_IMPORT_ITEMS = 16,
  W_SEED_NATIVE0_STRUCTS = 8,
  W_SEED_NATIVE0_FIELDS = 16,
  W_SEED_NATIVE0_TYPES = 32,
  W_SEED_NATIVE0_FUNCTIONS = 8,
  W_SEED_NATIVE0_PARAMETERS = 16,
  W_SEED_NATIVE0_ENTRIES = 8,
  W_SEED_NATIVE0_STATEMENTS = 64,
  W_SEED_NATIVE0_EXPRESSIONS = 128,
  W_SEED_NATIVE0_ARGUMENTS = 64,
  W_SEED_NATIVE0_INTERPOLATION_SEGMENTS = 128,
  W_SEED_NATIVE0_SYMBOLS = 64,
  W_SEED_NATIVE0_FACTS = 64,
  W_SEED_NATIVE0_DIAGNOSTICS = 32,
  W_SEED_NATIVE0_HIR_IDENTITIES = 32,
  W_SEED_NATIVE0_HIR_RECORDS = 32,
  W_SEED_NATIVE0_HIR_TEXT = 4096,
  W_SEED_NATIVE0_HIR_VALUES = 4096,
  W_SEED_NATIVE0_HIR_RECEIPT = 256,
  W_SEED_NATIVE0_FRONTEND_RECEIPT = 65536,
};

typedef enum {
  W_SEED_NATIVE0_OK = 0,
  W_SEED_NATIVE0_SOURCE,
  W_SEED_NATIVE0_PARSE,
  W_SEED_NATIVE0_FRONTEND,
  W_SEED_NATIVE0_HIR,
  W_SEED_NATIVE0_MLIR,
  W_SEED_NATIVE0_CAPACITY,
  W_SEED_NATIVE0_UNSUPPORTED,
  W_SEED_NATIVE0_INVALID,
} w_seed_native0_status;

/* The path length includes neither the terminating NUL nor any implicit
 * storage. The identity is the logical source identity used in all frontend
 * documents; Native0 never substitutes a hard-coded identity. */
typedef struct {
  const char *path;
  size_t path_length;
  w_seed_frontend_text logical_source_id;
  w_seed_mlir0_target target;
} w_seed_native0_input;

typedef w_seed_mlir0_output w_seed_native0_output;

/* A result is published only after the complete source-to-MLIR operation
 * succeeds. Every failure leaves this record and the output bytes unchanged. */
typedef struct {
  w_seed_native0_status status;
  size_t source_bytes;
  w_seed_mlir0_result mlir;
} w_seed_native0_result;

/* All parser, frontend, and HIR arenas are explicit fields so ownership and
 * bounds remain visible to the caller. The adapter itself has no static or
 * dynamically allocated working storage. */
typedef struct {
  uint8_t source_bytes[W_SEED_NATIVE0_MAX_SOURCE_BYTES + 1u];
  size_t source_length;
  w_seed_source source;
  w_seed_lexer_frame lexer_frames[W_SEED_NATIVE0_LEXER_FRAMES];
  w_seed_parse_token tokens[W_SEED_NATIVE0_TOKENS];
  w_seed_cst_node nodes[W_SEED_NATIVE0_NODES];
  w_seed_parse_frame parse_frames[W_SEED_NATIVE0_PARSE_FRAMES];
  w_seed_parse_issue issues[W_SEED_NATIVE0_ISSUES];
  w_seed_parser parser;
  w_seed_parse_result parse;
  w_seed_frontend_document document;
  w_seed_frontend_input input;
  w_seed_frontend_module modules[W_SEED_NATIVE0_MODULES];
  w_seed_frontend_import imports[W_SEED_NATIVE0_IMPORTS];
  w_seed_frontend_import_item import_items[W_SEED_NATIVE0_IMPORT_ITEMS];
  w_seed_frontend_struct structs[W_SEED_NATIVE0_STRUCTS];
  w_seed_frontend_field fields[W_SEED_NATIVE0_FIELDS];
  w_seed_frontend_type_declaration type_declarations[W_SEED_NATIVE0_STRUCTS];
  w_seed_frontend_alias aliases[W_SEED_NATIVE0_STRUCTS];
  w_seed_frontend_type types[W_SEED_NATIVE0_TYPES];
  w_seed_frontend_function functions[W_SEED_NATIVE0_FUNCTIONS];
  w_seed_frontend_parameter parameters[W_SEED_NATIVE0_PARAMETERS];
  w_seed_frontend_entry entries[W_SEED_NATIVE0_ENTRIES];
  w_seed_frontend_statement statements[W_SEED_NATIVE0_STATEMENTS];
  w_seed_frontend_expression expressions[W_SEED_NATIVE0_EXPRESSIONS];
  w_seed_frontend_argument arguments[W_SEED_NATIVE0_ARGUMENTS];
  w_seed_frontend_interpolation_segment
      interpolation_segments[W_SEED_NATIVE0_INTERPOLATION_SEGMENTS];
  w_seed_frontend_symbol symbols[W_SEED_NATIVE0_SYMBOLS];
  w_seed_frontend_fact facts[W_SEED_NATIVE0_FACTS];
  w_seed_frontend_diagnostic diagnostics[W_SEED_NATIVE0_DIAGNOSTICS];
  w_seed_frontend_diagnostic_fact
      diagnostic_facts[W_SEED_NATIVE0_DIAGNOSTICS * 5u];
  w_seed_frontend_diagnostic_item
      diagnostic_items[W_SEED_NATIVE0_DIAGNOSTICS * 4u];
  w_seed_frontend_diagnostic_label
      diagnostic_labels[W_SEED_NATIVE0_DIAGNOSTICS * 2u];
  w_seed_frontend_host_requirement host_requirements[2];
  w_seed_frontend_external_parameter host_parameters[2];
  w_seed_frontend_host_prelude_symbol host_symbols[2];
  w_seed_frontend_host_prelude host_scope;
  uint8_t const_bytes[W_SEED_NATIVE0_MAX_SOURCE_BYTES];
  uint8_t frontend_receipt[W_SEED_NATIVE0_FRONTEND_RECEIPT];
  w_seed_frontend_output output;
  w_seed_frontend_result frontend_result;
  w_seed_hir0_module hir_modules[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_identity hir_identities[W_SEED_NATIVE0_HIR_IDENTITIES];
  w_seed_hir0_type hir_types[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_function hir_functions[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_parameter hir_parameters[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_block hir_blocks[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_instruction hir_instructions[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_binding hir_bindings[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_call hir_calls[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_host_parameter hir_host_parameters[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_argument hir_arguments[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_requirement hir_requirements[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_value hir_values[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_interpolation_segment
      hir_interpolation_segments[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_terminator hir_terminators[W_SEED_NATIVE0_HIR_RECORDS];
  w_seed_hir0_entry hir_entries[W_SEED_NATIVE0_HIR_RECORDS];
  uint8_t hir_text[W_SEED_NATIVE0_HIR_TEXT];
  uint8_t hir_value_bytes[W_SEED_NATIVE0_HIR_VALUES];
  uint8_t hir_receipt[W_SEED_NATIVE0_HIR_RECEIPT];
  w_seed_hir0_output hir_output;
  w_seed_hir0_result hir_result;
  w_seed_hir0_program hir_program;
} w_seed_native0_storage;

/* Run source acquisition, parsing, frontend normalization, verified HIR0
 * lowering, and direct MLIR0 emission. No HLO0/HLO1 stage is called. */
w_seed_native0_status w_seed_native0_run(
    const w_seed_native0_input *input, w_seed_native0_storage *storage,
    const w_seed_native0_output *output, w_seed_native0_result *result);

#ifdef __cplusplus
}
#endif

#endif
