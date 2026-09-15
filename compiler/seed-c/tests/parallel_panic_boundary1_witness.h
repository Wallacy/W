#ifndef W_SEED_PARALLEL_PANIC_BOUNDARY1_WITNESS_H
#define W_SEED_PARALLEL_PANIC_BOUNDARY1_WITNESS_H

#include "w_seed_parallel_panic_boundary1.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_SOURCE = 4096,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_LEXER_FRAMES = 256,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_TOKENS = 2048,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_NODES = 4096,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_PARSE_FRAMES = 2048,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ISSUES = 128,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_MODULES = 8,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_IMPORTS = 8,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_IMPORT_ITEMS = 8,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_STRUCTS = 4,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_FIELDS = 8,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ENUMS = 8,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ENUM_CASES = 32,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ENUM_CASE_PARAMETERS = 16,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_SWITCH_ARMS = 32,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_TYPES = 32,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_FUNCTIONS = 8,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_PARAMETERS = 32,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ENTRIES = 4,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_STATEMENTS = 256,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_EXPRESSIONS = 256,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ARGUMENTS = 32,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_INTERPOLATION_SEGMENTS = 16,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_SYMBOLS = 32,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_FACTS = 16,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_DIAGNOSTICS = 8,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_RECEIPT = 65536,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_IDENTITIES = 32,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS = 512,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_TEXT = 4096,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_VALUES = 4096,
  W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECEIPT =
      W_SEED_HIR0_MAX_RECEIPT_BYTES,
};

typedef struct {
  w_seed_parallel_platform1_completion requested[2];
  uint32_t calls[2];
  bool fail;
} w_seed_parallel_panic_boundary1_provider_context;

typedef struct {
  uint8_t source_bytes[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_SOURCE];
  size_t source_length;
  w_seed_source source;
  w_seed_lexer_frame lexer_frames[
      W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_LEXER_FRAMES];
  w_seed_parse_token tokens[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_TOKENS];
  w_seed_cst_node nodes[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_NODES];
  w_seed_parse_frame parse_frames[
      W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_PARSE_FRAMES];
  w_seed_parse_issue issues[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ISSUES];
  w_seed_parser parser;
  w_seed_parse_result parse;
  w_seed_frontend_document document;
  w_seed_frontend_input frontend_input;
  w_seed_frontend_module modules[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_MODULES];
  w_seed_frontend_import imports[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_IMPORTS];
  w_seed_frontend_import_item
      import_items[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_IMPORT_ITEMS];
  w_seed_frontend_struct structs[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_STRUCTS];
  w_seed_frontend_field fields[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_FIELDS];
  w_seed_frontend_enum enums[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ENUMS];
  w_seed_frontend_enum_case
      enum_cases[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ENUM_CASES];
  w_seed_frontend_enum_case_parameter enum_case_parameters[
      W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ENUM_CASE_PARAMETERS];
  w_seed_frontend_enum_subset_member enum_subset_members[
      W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_frontend_type_declaration type_declarations[
      W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_STRUCTS];
  w_seed_frontend_alias aliases[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_STRUCTS];
  w_seed_frontend_type types[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_TYPES];
  w_seed_frontend_function
      functions[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_FUNCTIONS];
  w_seed_frontend_parameter
      parameters[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_PARAMETERS];
  w_seed_frontend_entry entries[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ENTRIES];
  w_seed_frontend_statement
      statements[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_STATEMENTS];
  w_seed_frontend_expression
      expressions[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_EXPRESSIONS];
  w_seed_frontend_argument
      arguments[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ARGUMENTS];
  w_seed_frontend_switch_arm
      switch_arms[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_SWITCH_ARMS];
  w_seed_frontend_pattern_capture
      pattern_captures[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_SWITCH_ARMS];
  w_seed_frontend_interpolation_segment interpolation_segments[
      W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_INTERPOLATION_SEGMENTS];
  w_seed_frontend_symbol
      symbols[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_SYMBOLS];
  w_seed_frontend_fact facts[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_FACTS];
  w_seed_frontend_diagnostic
      diagnostics[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_DIAGNOSTICS];
  w_seed_frontend_diagnostic_fact
      diagnostic_facts[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_DIAGNOSTICS * 5];
  w_seed_frontend_diagnostic_item
      diagnostic_items[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_DIAGNOSTICS * 4];
  w_seed_frontend_diagnostic_label
      diagnostic_labels[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_DIAGNOSTICS * 2];
  w_seed_frontend_host_requirement host_requirements[2];
  w_seed_frontend_external_parameter host_parameters[2];
  w_seed_frontend_host_prelude_symbol host_symbols[2];
  w_seed_frontend_host_prelude host_scope;
  uint8_t const_bytes[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_SOURCE];
  uint8_t frontend_receipt[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_RECEIPT];
  w_seed_frontend_output frontend_output;
  w_seed_frontend_result frontend_result;
  w_seed_hir0_module hir_modules[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_identity
      hir_identities[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_IDENTITIES];
  w_seed_hir0_type hir_types[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_enum hir_enums[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_enum_case
      hir_enum_cases[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_enum_case_parameter
      hir_enum_case_parameters[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_enum_subset_member
      hir_enum_subset_members[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_function
      hir_functions[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_parameter
      hir_parameters[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_block hir_blocks[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_block_argument
      hir_block_arguments[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_edge_argument
      hir_edge_arguments[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_switch_edge
      hir_switch_edges[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_switch_capture
      hir_switch_captures[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_instruction
      hir_instructions[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_binding
      hir_bindings[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_call hir_calls[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_host_parameter
      hir_host_parameters[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_argument
      hir_arguments[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_enum_payload
      hir_enum_payloads[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_requirement
      hir_requirements[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_value hir_values[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_interpolation_segment
      hir_interpolation_segments[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_terminator
      hir_terminators[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_entry hir_entries[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  w_seed_hir0_external_module hir_external_modules[2];
  w_seed_hir0_external_symbol hir_external_symbols[8];
  w_seed_hir0_cleanup
      hir_cleanups[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS];
  uint8_t hir_text[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_TEXT];
  uint8_t hir_value_bytes[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_VALUES];
  uint8_t hir_receipt[W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECEIPT];
  w_seed_hir0_output hir_output;
  w_seed_hir0_result hir_result;
  w_seed_hir0_counts hir_counts;
  w_seed_hir0_program hir_program;
  w_seed_parallel_panic_boundary1_provider_context provider_context;
  w_seed_parallel_typed_binding1_task tasks[2];
  w_seed_parallel_typed_binding1_provider provider;
  w_seed_parallel_local_provider1_authority authority;
  w_seed_parallel_typed_binding1_input typed_input;
} w_seed_parallel_panic_boundary1_witness;

bool w_seed_parallel_panic_boundary1_witness_init(
    w_seed_parallel_panic_boundary1_witness *witness);

bool w_seed_parallel_panic_boundary1_provider_invoke(
    void *raw, size_t task_index,
    w_seed_parallel_platform1_completion *completion);

#ifdef __cplusplus
}
#endif

#endif
