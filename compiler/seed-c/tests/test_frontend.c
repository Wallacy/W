#include "w_seed_frontend.h"
#include "w_seed_gpu_module.h"
#include "w_seed_gpu0_projection.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <fenv.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "frontend check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                       \
      return false;                                                             \
    }                                                                           \
  } while (0)

enum {
  TEST_NODES = 2048,
  TEST_TOKENS = 1024,
  TEST_FRAMES = 1024,
  TEST_LEX_FRAMES = 512,
  TEST_ISSUES = 64,
  TEST_MODULES = 8,
  TEST_IMPORTS = 32,
  TEST_IMPORT_ITEMS = 32,
  TEST_STRUCTS = 16,
  TEST_GENERIC_PARAMETERS = 64,
  TEST_GENERIC_APPLICATIONS = 64,
  TEST_GENERIC_ARGUMENTS = 256,
  TEST_TYPED_CONST_EXPRESSIONS = 256,
  TEST_CONST_VALUES = 512,
  TEST_CONST_ELEMENTS = 512,
  TEST_CONST_BYTES = 8192,
  TEST_TUPLE_COMPONENTS = TEST_NODES * 4,
  TEST_TUPLE_ELEMENTS = TEST_NODES * 4,
  TEST_ENUMS = 16,
  TEST_ENUM_CASES = 128,
  TEST_ENUM_CASE_PARAMETERS = 256,
  TEST_ENUM_SUBSET_MEMBERS = 256,
  TEST_FIELDS = 64,
  TEST_DECLARATIONS = 32,
  TEST_KERNEL_MODULES = 16,
  TEST_KERNEL_BINDINGS = 64,
  TEST_TYPES = 128,
  TEST_FUNCTIONS = 32,
  TEST_PARAMETERS = 128,
  TEST_ENTRIES = 16,
  TEST_STATEMENTS = 256,
  TEST_EXPRESSIONS = 1024,
  TEST_INTERPOLATION_SEGMENTS = 1024,
  TEST_ARGUMENTS = 256,
  TEST_SWITCH_ARMS = 256,
  TEST_PATTERN_CAPTURES = 256,
  TEST_ENUM_MEMBERSHIP_CASES = 1024,
  TEST_SYMBOLS = 512,
  TEST_FACTS = 512,
  TEST_DIAGNOSTICS = 128,
  TEST_DIAGNOSTIC_FACTS = TEST_DIAGNOSTICS * 5,
  TEST_DIAGNOSTIC_ITEMS = TEST_DIAGNOSTICS * 4,
  TEST_DIAGNOSTIC_LABELS = TEST_DIAGNOSTICS * 2,
  TEST_RECEIPT = 128 * 1024,
};

typedef struct {
  w_seed_source source;
  w_seed_lexer_frame lexer_frames[TEST_LEX_FRAMES];
  w_seed_parse_token tokens[TEST_TOKENS];
  w_seed_cst_node nodes[TEST_NODES];
  w_seed_parse_frame frames[TEST_FRAMES];
  w_seed_parse_issue issues[TEST_ISSUES];
  w_seed_parser parser;
  w_seed_parse_result parse;
  w_seed_frontend_document document;
  w_seed_frontend_input input;
  w_seed_frontend_resolved_import resolved_imports[TEST_IMPORTS];
  w_seed_frontend_module modules[TEST_MODULES];
  w_seed_frontend_import imports[TEST_IMPORTS];
  w_seed_frontend_import_item import_items[TEST_IMPORT_ITEMS];
  w_seed_frontend_struct structs[TEST_STRUCTS];
  w_seed_frontend_generic_parameter
      generic_parameters[TEST_GENERIC_PARAMETERS];
  w_seed_frontend_generic_application
      generic_applications[TEST_GENERIC_APPLICATIONS];
  w_seed_frontend_generic_argument generic_arguments[TEST_GENERIC_ARGUMENTS];
  w_seed_frontend_typed_const_expression
      typed_const_expressions[TEST_TYPED_CONST_EXPRESSIONS];
  w_seed_frontend_const_value const_values[TEST_CONST_VALUES];
  w_seed_frontend_const_element const_elements[TEST_CONST_ELEMENTS];
  uint8_t const_bytes[TEST_CONST_BYTES];
  w_seed_frontend_enum enums[TEST_ENUMS];
  w_seed_frontend_enum_case enum_cases[TEST_ENUM_CASES];
  w_seed_frontend_enum_case_parameter
      enum_case_parameters[TEST_ENUM_CASE_PARAMETERS];
  w_seed_frontend_enum_subset_member
      enum_subset_members[TEST_ENUM_SUBSET_MEMBERS];
  w_seed_frontend_field fields[TEST_FIELDS];
  w_seed_frontend_type_declaration type_declarations[TEST_DECLARATIONS];
  w_seed_frontend_alias aliases[TEST_DECLARATIONS];
  w_seed_frontend_const_declaration const_declarations[TEST_DECLARATIONS];
  w_seed_frontend_kernel_module
      kernel_modules[TEST_KERNEL_MODULES];
  w_seed_frontend_kernel_binding
      kernel_bindings[TEST_KERNEL_BINDINGS];
  w_seed_frontend_type types[TEST_TYPES];
  w_seed_frontend_tuple_component tuple_components[TEST_TUPLE_COMPONENTS];
  w_seed_frontend_tuple_element tuple_elements[TEST_TUPLE_ELEMENTS];
  w_seed_frontend_function functions[TEST_FUNCTIONS];
  w_seed_frontend_parameter parameters[TEST_PARAMETERS];
  w_seed_frontend_entry entries[TEST_ENTRIES];
  w_seed_frontend_statement statements[TEST_STATEMENTS];
  w_seed_frontend_expression expressions[TEST_EXPRESSIONS];
  w_seed_frontend_interpolation_segment
      interpolation_segments[TEST_INTERPOLATION_SEGMENTS];
  w_seed_frontend_argument arguments[TEST_ARGUMENTS];
  w_seed_frontend_switch_arm switch_arms[TEST_SWITCH_ARMS];
  w_seed_frontend_pattern_capture pattern_captures[TEST_PATTERN_CAPTURES];
  w_seed_frontend_enum_membership_case
      enum_membership_cases[TEST_ENUM_MEMBERSHIP_CASES];
  w_seed_frontend_symbol symbols[TEST_SYMBOLS];
  w_seed_frontend_fact facts[TEST_FACTS];
  w_seed_frontend_diagnostic diagnostics[TEST_DIAGNOSTICS];
  w_seed_frontend_diagnostic_fact
      diagnostic_facts[TEST_DIAGNOSTIC_FACTS];
  w_seed_frontend_diagnostic_item
      diagnostic_items[TEST_DIAGNOSTIC_ITEMS];
  w_seed_frontend_diagnostic_label
      diagnostic_labels[TEST_DIAGNOSTIC_LABELS];
  w_seed_frontend_external_parameter external_parameters[8];
  w_seed_frontend_external_symbol external_symbols[8];
  w_seed_frontend_external_module external_modules[2];
  w_seed_frontend_host_requirement host_requirements[2];
  w_seed_frontend_external_parameter host_parameters[2];
  w_seed_frontend_host_prelude_symbol host_symbols[2];
  w_seed_frontend_host_prelude host_scope;
  w_seed_frontend_domain domains[2];
  uint8_t receipt[TEST_RECEIPT];
  w_seed_frontend_output output;
  w_seed_frontend_result result;
} fixture;

static bool fixture_run_with_print_host(fixture *value, const char *source);

static fixture fixture_a;
static fixture fixture_b;
static fixture fixture_condition;
static fixture fixture_narrowing;
static fixture fixture_label;
static fixture fixture_literal;
static fixture fixture_recovered;
static fixture fixture_capacity;
static fixture fixture_duplicate;
static fixture fixture_unresolved;
static fixture fixture_external;
static fixture fixture_generic;
static fixture fixture_callback;
static fixture fixture_collision;
static fixture fixture_const;
static fixture fixture_host;
static char long_source[8192];
static fixture fixture_scalar_if;
static fixture fixture_mutation;
static fixture fixture_async;

typedef struct {
  w_seed_gpu_module_record modules[TEST_KERNEL_MODULES];
  w_seed_gpu_module_kernel kernels[TEST_KERNEL_BINDINGS];
  uint8_t text[8192];
  uint8_t receipt[TEST_RECEIPT];
} gpu_module_storage;

typedef struct {
  w_seed_gpu0_function functions[W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY + 2u];
  w_seed_gpu0_operation operations[W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY + 1u];
  char text[64];
} gpu0_projection_storage;

typedef struct {
  uint8_t host_artifact[W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY];
  uint8_t device_artifact[W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY];
  int32_t device_result;
  int32_t host_result;
  w_seed_gpu0_result result;
} gpu0_run_storage;

static gpu_module_storage gpu_storage;
static gpu_module_storage gpu_capacity_storage;
static gpu_module_storage gpu_non42_storage;

static bool all_bytes_equal(const void *data, size_t size, uint8_t value) {
  if (size == 0) return true;
  if (data == NULL) return false;
  const uint8_t *bytes = (const uint8_t *)data;
  for (size_t index = 0; index < size; index += 1) {
    if (bytes[index] != value) return false;
  }
  return true;
}

static void fixture_fill_output(fixture *fixture_value, uint8_t value) {
  (void)memset(fixture_value->modules, value, sizeof(fixture_value->modules));
  (void)memset(fixture_value->imports, value, sizeof(fixture_value->imports));
  (void)memset(fixture_value->import_items, value,
               sizeof(fixture_value->import_items));
  (void)memset(fixture_value->structs, value, sizeof(fixture_value->structs));
  (void)memset(fixture_value->generic_parameters, value,
               sizeof(fixture_value->generic_parameters));
  (void)memset(fixture_value->generic_applications, value,
               sizeof(fixture_value->generic_applications));
  (void)memset(fixture_value->generic_arguments, value,
               sizeof(fixture_value->generic_arguments));
  (void)memset(fixture_value->typed_const_expressions, value,
               sizeof(fixture_value->typed_const_expressions));
  (void)memset(fixture_value->const_values, value,
               sizeof(fixture_value->const_values));
  (void)memset(fixture_value->const_elements, value,
               sizeof(fixture_value->const_elements));
  (void)memset(fixture_value->const_bytes, value,
               sizeof(fixture_value->const_bytes));
  (void)memset(fixture_value->enums, value, sizeof(fixture_value->enums));
  (void)memset(fixture_value->enum_cases, value,
               sizeof(fixture_value->enum_cases));
  (void)memset(fixture_value->enum_case_parameters, value,
               sizeof(fixture_value->enum_case_parameters));
  (void)memset(fixture_value->enum_subset_members, value,
               sizeof(fixture_value->enum_subset_members));
  (void)memset(fixture_value->fields, value, sizeof(fixture_value->fields));
  (void)memset(fixture_value->type_declarations, value,
               sizeof(fixture_value->type_declarations));
  (void)memset(fixture_value->aliases, value, sizeof(fixture_value->aliases));
  (void)memset(fixture_value->const_declarations, value,
               sizeof(fixture_value->const_declarations));
  (void)memset(fixture_value->kernel_modules, value,
               sizeof(fixture_value->kernel_modules));
  (void)memset(fixture_value->kernel_bindings, value,
               sizeof(fixture_value->kernel_bindings));
  (void)memset(fixture_value->types, value, sizeof(fixture_value->types));
  (void)memset(fixture_value->tuple_components, value,
               sizeof(fixture_value->tuple_components));
  (void)memset(fixture_value->tuple_elements, value,
               sizeof(fixture_value->tuple_elements));
  (void)memset(fixture_value->functions, value,
               sizeof(fixture_value->functions));
  (void)memset(fixture_value->parameters, value,
               sizeof(fixture_value->parameters));
  (void)memset(fixture_value->entries, value, sizeof(fixture_value->entries));
  (void)memset(fixture_value->statements, value,
               sizeof(fixture_value->statements));
  (void)memset(fixture_value->expressions, value,
               sizeof(fixture_value->expressions));
  (void)memset(fixture_value->interpolation_segments, value,
               sizeof(fixture_value->interpolation_segments));
  (void)memset(fixture_value->arguments, value,
               sizeof(fixture_value->arguments));
  (void)memset(fixture_value->switch_arms, value,
               sizeof(fixture_value->switch_arms));
  (void)memset(fixture_value->pattern_captures, value,
               sizeof(fixture_value->pattern_captures));
  (void)memset(fixture_value->enum_membership_cases, value,
               sizeof(fixture_value->enum_membership_cases));
  (void)memset(fixture_value->symbols, value, sizeof(fixture_value->symbols));
  (void)memset(fixture_value->facts, value, sizeof(fixture_value->facts));
  (void)memset(fixture_value->diagnostics, value,
               sizeof(fixture_value->diagnostics));
  (void)memset(fixture_value->diagnostic_facts, value,
               sizeof(fixture_value->diagnostic_facts));
  (void)memset(fixture_value->diagnostic_items, value,
               sizeof(fixture_value->diagnostic_items));
  (void)memset(fixture_value->diagnostic_labels, value,
               sizeof(fixture_value->diagnostic_labels));
  (void)memset(fixture_value->receipt, value, sizeof(fixture_value->receipt));
}

static bool fixture_output_is(const fixture *fixture_value, uint8_t value,
                              bool modules_present) {
  return (!modules_present ||
          all_bytes_equal(fixture_value->modules, sizeof(fixture_value->modules),
                          value)) &&
         all_bytes_equal(fixture_value->imports, sizeof(fixture_value->imports),
                         value) &&
         all_bytes_equal(fixture_value->import_items,
                         sizeof(fixture_value->import_items), value) &&
         all_bytes_equal(fixture_value->structs, sizeof(fixture_value->structs),
                         value) &&
         all_bytes_equal(fixture_value->generic_parameters,
                         sizeof(fixture_value->generic_parameters), value) &&
         all_bytes_equal(fixture_value->generic_applications,
                         sizeof(fixture_value->generic_applications), value) &&
         all_bytes_equal(fixture_value->generic_arguments,
                         sizeof(fixture_value->generic_arguments), value) &&
         all_bytes_equal(fixture_value->typed_const_expressions,
                         sizeof(fixture_value->typed_const_expressions), value) &&
         all_bytes_equal(fixture_value->const_values,
                         sizeof(fixture_value->const_values), value) &&
         all_bytes_equal(fixture_value->const_elements,
                         sizeof(fixture_value->const_elements), value) &&
         all_bytes_equal(fixture_value->const_bytes,
                         sizeof(fixture_value->const_bytes), value) &&
         all_bytes_equal(fixture_value->enums, sizeof(fixture_value->enums),
                         value) &&
         all_bytes_equal(fixture_value->enum_cases,
                         sizeof(fixture_value->enum_cases), value) &&
         all_bytes_equal(fixture_value->enum_case_parameters,
                         sizeof(fixture_value->enum_case_parameters), value) &&
         all_bytes_equal(fixture_value->fields, sizeof(fixture_value->fields),
                         value) &&
         all_bytes_equal(fixture_value->type_declarations,
                         sizeof(fixture_value->type_declarations), value) &&
          all_bytes_equal(fixture_value->aliases, sizeof(fixture_value->aliases),
                          value) &&
          all_bytes_equal(fixture_value->const_declarations,
                          sizeof(fixture_value->const_declarations), value) &&
          all_bytes_equal(fixture_value->kernel_modules,
                          sizeof(fixture_value->kernel_modules), value) &&
          all_bytes_equal(fixture_value->kernel_bindings,
                          sizeof(fixture_value->kernel_bindings), value) &&
         all_bytes_equal(fixture_value->types, sizeof(fixture_value->types),
                         value) &&
         all_bytes_equal(fixture_value->tuple_components,
                         sizeof(fixture_value->tuple_components), value) &&
         all_bytes_equal(fixture_value->tuple_elements,
                         sizeof(fixture_value->tuple_elements), value) &&
         all_bytes_equal(fixture_value->functions,
                         sizeof(fixture_value->functions), value) &&
         all_bytes_equal(fixture_value->parameters,
                         sizeof(fixture_value->parameters), value) &&
         all_bytes_equal(fixture_value->entries, sizeof(fixture_value->entries),
                         value) &&
         all_bytes_equal(fixture_value->statements,
                         sizeof(fixture_value->statements), value) &&
         all_bytes_equal(fixture_value->expressions,
                         sizeof(fixture_value->expressions), value) &&
         all_bytes_equal(fixture_value->interpolation_segments,
                         sizeof(fixture_value->interpolation_segments), value) &&
         all_bytes_equal(fixture_value->arguments,
                         sizeof(fixture_value->arguments), value) &&
         all_bytes_equal(fixture_value->switch_arms,
                         sizeof(fixture_value->switch_arms), value) &&
         all_bytes_equal(fixture_value->pattern_captures,
                         sizeof(fixture_value->pattern_captures), value) &&
         all_bytes_equal(fixture_value->enum_membership_cases,
                         sizeof(fixture_value->enum_membership_cases), value) &&
         all_bytes_equal(fixture_value->enum_subset_members,
                         sizeof(fixture_value->enum_subset_members), value) &&
         all_bytes_equal(fixture_value->symbols, sizeof(fixture_value->symbols),
                         value) &&
         all_bytes_equal(fixture_value->facts, sizeof(fixture_value->facts),
                         value) &&
         all_bytes_equal(fixture_value->diagnostics,
                         sizeof(fixture_value->diagnostics), value) &&
         all_bytes_equal(fixture_value->diagnostic_facts,
                         sizeof(fixture_value->diagnostic_facts), value) &&
         all_bytes_equal(fixture_value->diagnostic_items,
                         sizeof(fixture_value->diagnostic_items), value) &&
         all_bytes_equal(fixture_value->diagnostic_labels,
                         sizeof(fixture_value->diagnostic_labels), value) &&
         all_bytes_equal(fixture_value->receipt, sizeof(fixture_value->receipt),
                         value);
}

static bool fixture_parse(fixture *fixture_value, const char *text) {
  const w_seed_byte_view bytes = {(const uint8_t *)text, strlen(text)};
  w_seed_source_error source_error;
  CHECK(w_seed_source_init(bytes, &fixture_value->source, &source_error));
  w_seed_lex_error lex_error;
  CHECK(w_seed_parser_init(
      &fixture_value->source, (w_seed_span){0, bytes.length},
      (w_seed_foreign_limits){65536u, 256u}, fixture_value->lexer_frames,
      TEST_LEX_FRAMES, fixture_value->tokens, TEST_TOKENS,
      fixture_value->nodes, TEST_NODES, fixture_value->frames, TEST_FRAMES,
                          fixture_value->issues, TEST_ISSUES,
                          &fixture_value->parser, &lex_error));
  CHECK(w_seed_parser_parse(&fixture_value->parser, &fixture_value->parse));
  fixture_value->document.logical_source_id = (w_seed_frontend_text){"test", 4};
  fixture_value->document.module_id = (w_seed_frontend_text){"test", 4};
  fixture_value->document.local_module_name =
      fixture_value->document.module_id;
  w_seed_module_scan_result scan_result;
  const w_seed_module_scan_status scan_status = w_seed_module_scan(
      &fixture_value->source, fixture_value->nodes,
      fixture_value->parse.node_count, &fixture_value->parse, NULL, 0u,
      &scan_result);
  if ((scan_status == W_SEED_MODULE_SCAN_OK ||
       scan_status == W_SEED_MODULE_SCAN_CAPACITY) &&
      scan_result.has_module_header_name) {
    fixture_value->document.local_module_name = (w_seed_frontend_text){
        (const char *)fixture_value->source.bytes.data +
            scan_result.module_header_name_span.start_byte,
        scan_result.module_header_name_span.end_byte -
            scan_result.module_header_name_span.start_byte};
  }
  fixture_value->document.source = &fixture_value->source;
  fixture_value->document.nodes = fixture_value->nodes;
  fixture_value->document.node_count = fixture_value->parse.node_count;
  fixture_value->document.parse = fixture_value->parse;
  fixture_value->input.documents = &fixture_value->document;
  fixture_value->input.document_count = 1;
  fixture_value->input.external_modules = NULL;
  fixture_value->input.external_module_count = 0;
  fixture_value->input.host_scope = NULL;
  fixture_value->input.import_resolution_complete = false;
  fixture_value->input.resolved_imports = NULL;
  fixture_value->input.resolved_import_count = 0u;
  fixture_value->input.domains = NULL;
  fixture_value->input.domain_count = 0u;
  fixture_fill_output(fixture_value, 0);
  fixture_value->output = (w_seed_frontend_output){
      .modules = fixture_value->modules,
      .module_capacity = TEST_MODULES,
      .imports = fixture_value->imports,
      .import_capacity = TEST_IMPORTS,
      .import_items = fixture_value->import_items,
      .import_item_capacity = TEST_IMPORT_ITEMS,
      .structs = fixture_value->structs,
      .struct_capacity = TEST_STRUCTS,
      .generic_parameters = fixture_value->generic_parameters,
      .generic_parameter_capacity = TEST_GENERIC_PARAMETERS,
      .generic_applications = fixture_value->generic_applications,
      .generic_application_capacity = TEST_GENERIC_APPLICATIONS,
      .generic_arguments = fixture_value->generic_arguments,
      .generic_argument_capacity = TEST_GENERIC_ARGUMENTS,
      .typed_const_expressions = fixture_value->typed_const_expressions,
      .typed_const_expression_capacity = TEST_TYPED_CONST_EXPRESSIONS,
      .const_values = fixture_value->const_values,
      .const_value_capacity = TEST_CONST_VALUES,
      .const_elements = fixture_value->const_elements,
      .const_element_capacity = TEST_CONST_ELEMENTS,
      .const_bytes = fixture_value->const_bytes,
      .const_bytes_capacity = TEST_CONST_BYTES,
      .enums = fixture_value->enums,
      .enum_capacity = TEST_ENUMS,
      .enum_cases = fixture_value->enum_cases,
      .enum_case_capacity = TEST_ENUM_CASES,
      .enum_case_parameters = fixture_value->enum_case_parameters,
      .enum_case_parameter_capacity = TEST_ENUM_CASE_PARAMETERS,
      .enum_subset_members = fixture_value->enum_subset_members,
      .enum_subset_member_capacity = TEST_ENUM_SUBSET_MEMBERS,
      .fields = fixture_value->fields,
      .field_capacity = TEST_FIELDS,
      .type_declarations = fixture_value->type_declarations,
      .type_declaration_capacity = TEST_DECLARATIONS,
      .aliases = fixture_value->aliases,
      .alias_capacity = TEST_DECLARATIONS,
      .const_declarations = fixture_value->const_declarations,
      .const_declaration_capacity = TEST_DECLARATIONS,
      .kernel_modules = fixture_value->kernel_modules,
      .kernel_module_capacity = TEST_KERNEL_MODULES,
      .kernel_bindings = fixture_value->kernel_bindings,
      .kernel_binding_capacity = TEST_KERNEL_BINDINGS,
      .types = fixture_value->types,
      .type_capacity = TEST_TYPES,
      .tuple_components = fixture_value->tuple_components,
      .tuple_component_capacity = TEST_TUPLE_COMPONENTS,
      .tuple_elements = fixture_value->tuple_elements,
      .tuple_element_capacity = TEST_TUPLE_ELEMENTS,
      .functions = fixture_value->functions,
      .function_capacity = TEST_FUNCTIONS,
      .parameters = fixture_value->parameters,
      .parameter_capacity = TEST_PARAMETERS,
      .arguments = fixture_value->arguments,
      .argument_capacity = TEST_ARGUMENTS,
      .switch_arms = fixture_value->switch_arms,
      .switch_arm_capacity = TEST_SWITCH_ARMS,
      .pattern_captures = fixture_value->pattern_captures,
      .pattern_capture_capacity = TEST_PATTERN_CAPTURES,
      .enum_membership_cases = fixture_value->enum_membership_cases,
      .enum_membership_case_capacity = TEST_ENUM_MEMBERSHIP_CASES,
      .entries = fixture_value->entries,
      .entry_capacity = TEST_ENTRIES,
      .statements = fixture_value->statements,
      .statement_capacity = TEST_STATEMENTS,
      .expressions = fixture_value->expressions,
      .expression_capacity = TEST_EXPRESSIONS,
      .interpolation_segments = fixture_value->interpolation_segments,
      .interpolation_segment_capacity = TEST_INTERPOLATION_SEGMENTS,
      .symbols = fixture_value->symbols,
      .symbol_capacity = TEST_SYMBOLS,
      .facts = fixture_value->facts,
      .fact_capacity = TEST_FACTS,
      .diagnostics = fixture_value->diagnostics,
      .diagnostic_capacity = TEST_DIAGNOSTICS,
      .diagnostic_facts = fixture_value->diagnostic_facts,
      .diagnostic_fact_capacity = TEST_DIAGNOSTIC_FACTS,
      .diagnostic_items = fixture_value->diagnostic_items,
      .diagnostic_item_capacity = TEST_DIAGNOSTIC_ITEMS,
      .diagnostic_labels = fixture_value->diagnostic_labels,
      .diagnostic_label_capacity = TEST_DIAGNOSTIC_LABELS,
      .receipt = fixture_value->receipt,
      .receipt_capacity = TEST_RECEIPT,
  };
  return true;
}

static bool fixture_run(fixture *fixture_value, const char *text) {
  CHECK(fixture_parse(fixture_value, text));
  (void)w_seed_frontend_run(&fixture_value->input, &fixture_value->output,
                            &fixture_value->result);
  return true;
}

static void fixture_configure_domain(fixture *fixture_value,
                                     w_seed_frontend_domain_mode mode,
                                     uint32_t capabilities) {
  fixture_value->domains[0] = (w_seed_frontend_domain){
      .name = (w_seed_frontend_text){W_SEED_FRONTEND_DOMAIN_IDENTITY,
                                     sizeof(W_SEED_FRONTEND_DOMAIN_IDENTITY) -
                                         1u},
      .mode = mode,
      .capabilities = capabilities};
  fixture_value->input.domains = fixture_value->domains;
  fixture_value->input.domain_count = 1u;
}

static void fixture_configure_accelerated_domain(fixture *fixture_value,
                                                 uint32_t maximum) {
  fixture_value->domains[0] = (w_seed_frontend_domain){
      .name = (w_seed_frontend_text){".inference", 10u},
      .kind = W_SEED_FRONTEND_DOMAIN_ACCELERATED,
      .mode = W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
      .capabilities = W_SEED_FRONTEND_DOMAIN_CAPABILITY_DEVICE,
      .maximum = maximum};
  fixture_value->input.domains = fixture_value->domains;
  fixture_value->input.domain_count = 1u;
}

static bool fixture_run_with_domain(fixture *fixture_value, const char *text,
                                    w_seed_frontend_domain_mode mode,
                                    uint32_t capabilities) {
  if (!fixture_parse(fixture_value, text)) return false;
  fixture_configure_domain(fixture_value, mode, capabilities);
  (void)w_seed_frontend_run(&fixture_value->input, &fixture_value->output,
                            &fixture_value->result);
  return true;
}

static bool fixture_resolve_external_imports(fixture *fixture_value) {
  if (fixture_value == NULL || fixture_value->input.document_count != 1u)
    return false;
  w_seed_module_origin origins[TEST_IMPORTS];
  w_seed_module_scan_result scan_result;
  if (w_seed_module_scan(&fixture_value->source, fixture_value->nodes,
                         fixture_value->parse.node_count,
                         &fixture_value->parse, origins, TEST_IMPORTS,
                         &scan_result) != W_SEED_MODULE_SCAN_OK) {
    return false;
  }
  for (size_t origin_index = 0u; origin_index < scan_result.written;
       origin_index += 1u) {
    const w_seed_span path_span = origins[origin_index].module_path_span;
    const w_seed_frontend_text path = {
        (const char *)fixture_value->source.bytes.data + path_span.start_byte,
        path_span.end_byte - path_span.start_byte};
    size_t target_index = SIZE_MAX;
    for (size_t module_index = 0u;
         module_index < fixture_value->input.external_module_count;
         module_index += 1u) {
      const w_seed_frontend_text candidate =
          fixture_value->input.external_modules[module_index].module_id;
      if (candidate.length == path.length &&
          memcmp(candidate.data, path.data, path.length) == 0) {
        target_index = module_index;
        break;
      }
    }
    if (target_index == SIZE_MAX) return false;
    fixture_value->resolved_imports[origin_index] =
        (w_seed_frontend_resolved_import){
            .source_document_index = 0u,
            .direct_import_ordinal = origins[origin_index].direct_import_ordinal,
            .import_declaration_span = origins[origin_index].declaration_span,
            .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_EXTERNAL_MODULE,
            .target_index = (uint32_t)target_index};
  }
  fixture_value->input.import_resolution_complete = true;
  fixture_value->input.resolved_imports = fixture_value->resolved_imports;
  fixture_value->input.resolved_import_count = scan_result.written;
  return true;
}

static void fixture_configure_arguments_external(fixture *fixture_value) {
  fixture_value->external_parameters[0] =
      (w_seed_frontend_external_parameter){
          .name = (w_seed_frontend_text){"flag", 4u},
          .type = (w_seed_frontend_text){"String", 6u},
          .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  fixture_value->external_symbols[0] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"Arguments", 9u},
          .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
          .exported = true,
          .parameters = NULL,
          .parameter_count = 0u,
          .return_type = (w_seed_frontend_text){"Arguments", 9u},
          .is_const = false,
          .receiver_type = (w_seed_frontend_text){NULL, 0u}};
  fixture_value->external_symbols[1] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"contains", 8u},
          .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
          .exported = true,
          .parameters = fixture_value->external_parameters,
          .parameter_count = 1u,
          .return_type = (w_seed_frontend_text){"Bool", 4u},
          .is_const = false,
          .receiver_type = (w_seed_frontend_text){"Arguments", 9u}};
  fixture_value->external_modules[0] =
      (w_seed_frontend_external_module){
          .module_id = (w_seed_frontend_text){"std.process", 11u},
          .symbols = fixture_value->external_symbols,
          .symbol_count = 2u};
  fixture_value->input.external_modules = fixture_value->external_modules;
  fixture_value->input.external_module_count = 1u;
}

static void fixture_configure_process_abi_external(fixture *fixture_value) {
  fixture_value->external_symbols[0] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"Arguments", 9u},
          .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
          .exported = true,
          .parameters = NULL,
          .parameter_count = 0u,
          .return_type = (w_seed_frontend_text){"Arguments", 9u},
          .is_const = false,
          .receiver_type = (w_seed_frontend_text){NULL, 0u}};
  fixture_value->external_symbols[1] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"Context", 7u},
          .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
          .exported = true,
          .parameters = NULL,
          .parameter_count = 0u,
          .return_type = (w_seed_frontend_text){"Context", 7u},
          .is_const = false,
          .receiver_type = (w_seed_frontend_text){NULL, 0u}};
  fixture_value->external_symbols[2] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"ExitCode", 8u},
          .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
          .exported = true,
          .parameters = NULL,
          .parameter_count = 0u,
          .return_type = (w_seed_frontend_text){"ExitCode", 8u},
          .is_const = false,
          .receiver_type = (w_seed_frontend_text){NULL, 0u}};
  fixture_value->external_symbols[3] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"success", 7u},
          .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
          .exported = true,
          .parameters = NULL,
          .parameter_count = 0u,
          .return_type = (w_seed_frontend_text){"ExitCode", 8u},
          .is_const = true,
          .receiver_type = (w_seed_frontend_text){"ExitCode", 8u}};
  fixture_value->external_parameters[0] =
      (w_seed_frontend_external_parameter){
          .name = (w_seed_frontend_text){"code", 4u},
          .type = (w_seed_frontend_text){"i32", 3u},
          .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  fixture_value->external_symbols[4] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"failure", 7u},
          .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
          .exported = true,
          .parameters = fixture_value->external_parameters,
          .parameter_count = 1u,
          .return_type = (w_seed_frontend_text){"ExitCode", 8u},
          .is_const = true,
          .receiver_type = (w_seed_frontend_text){"ExitCode", 8u}};
  fixture_value->external_symbols[5] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"isEmpty", 7u},
          .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
          .exported = true,
          .parameters = NULL,
          .parameter_count = 0u,
          .return_type = (w_seed_frontend_text){"Bool", 4u},
          .is_const = true,
          .receiver_type = (w_seed_frontend_text){"Arguments", 9u}};
  fixture_value->external_modules[0] =
      (w_seed_frontend_external_module){
          .module_id = (w_seed_frontend_text){"std.process", 11u},
          .symbols = fixture_value->external_symbols,
          .symbol_count = 6u};
  fixture_value->input.external_modules = fixture_value->external_modules;
  fixture_value->input.external_module_count = 1u;
}

static bool counts_equal(const w_seed_frontend_counts *left,
                         const w_seed_frontend_counts *right) {
  return left->modules == right->modules && left->imports == right->imports &&
         left->import_items == right->import_items &&
         left->structs == right->structs && left->fields == right->fields &&
         left->generic_parameters == right->generic_parameters &&
         left->generic_applications == right->generic_applications &&
         left->generic_arguments == right->generic_arguments &&
         left->typed_const_expressions == right->typed_const_expressions &&
         left->const_values == right->const_values &&
         left->const_elements == right->const_elements &&
         left->const_bytes == right->const_bytes &&
         left->tuple_components == right->tuple_components &&
         left->tuple_elements == right->tuple_elements &&
         left->enums == right->enums &&
         left->enum_cases == right->enum_cases &&
         left->enum_case_parameters == right->enum_case_parameters &&
         left->enum_subset_members == right->enum_subset_members &&
         left->type_declarations == right->type_declarations &&
         left->aliases == right->aliases && left->types == right->types &&
         left->const_declarations == right->const_declarations &&
         left->functions == right->functions &&
         left->parameters == right->parameters &&
         left->entries == right->entries &&
         left->statements == right->statements &&
         left->expressions == right->expressions &&
         left->interpolation_segments == right->interpolation_segments &&
         left->arguments == right->arguments && left->symbols == right->symbols &&
         left->switch_arms == right->switch_arms &&
         left->pattern_captures == right->pattern_captures &&
         left->enum_membership_cases == right->enum_membership_cases &&
         left->facts == right->facts &&
         left->diagnostics == right->diagnostics &&
         left->diagnostic_facts == right->diagnostic_facts &&
         left->diagnostic_items == right->diagnostic_items &&
         left->diagnostic_labels == right->diagnostic_labels &&
         left->receipt_bytes == right->receipt_bytes;
}

static bool has_fact(const fixture *fixture_value,
                     w_seed_frontend_fact_kind kind);

static bool has_fact(const fixture *fixture_value,
                     w_seed_frontend_fact_kind kind) {
  for (size_t index = 0; index < fixture_value->result.written.facts;
       index += 1) {
    if (fixture_value->facts[index].kind == kind) return true;
  }
  return false;
}

static bool has_parse_issue(const fixture *fixture_value,
                            w_seed_parse_issue_kind kind) {
  if (fixture_value == NULL) return false;
  for (size_t index = 0u; index < fixture_value->parse.issue_count;
       index += 1u) {
    if (fixture_value->issues[index].kind == kind) return true;
  }
  return false;
}

static bool receipt_contains(const fixture *fixture_value, const char *needle,
                             size_t needle_length) {
  if (needle_length == 0) return true;
  for (size_t start = 0;
       start + needle_length <= fixture_value->result.receipt_bytes; start += 1) {
    if (memcmp(fixture_value->receipt + start, needle, needle_length) == 0) {
      return true;
    }
  }
  return false;
}

static bool has_diagnostic(const fixture *fixture_value, const char *code) {
  if (fixture_value == NULL || code == NULL) return false;
  const size_t length = strlen(code);
  for (size_t index = 0; index < fixture_value->result.written.diagnostics;
       index += 1) {
    const w_seed_frontend_diagnostic *diagnostic =
        &fixture_value->diagnostics[index];
    if (diagnostic->code.length == length &&
        memcmp(diagnostic->code.data, code, length) == 0) {
      return true;
    }
  }
  return false;
}

static bool frontend_text_is(w_seed_frontend_text text, const char *literal) {
  if (literal == NULL) return false;
  const size_t length = strlen(literal);
  return text.length == length &&
         (length == 0u || (text.data != NULL &&
                           memcmp(text.data, literal, length) == 0));
}

static bool fixture_span_text_is(const fixture *fixture_value,
                                 size_t document_index, w_seed_span span,
                                 const char *literal) {
  if (fixture_value == NULL || literal == NULL ||
      fixture_value->input.documents == NULL ||
      document_index >= fixture_value->input.document_count ||
      fixture_value->input.documents[document_index].source == NULL ||
      span.start_byte > span.end_byte) {
    return false;
  }
  const w_seed_source *source =
      fixture_value->input.documents[document_index].source;
  const size_t length = strlen(literal);
  return span.end_byte - span.start_byte == length &&
         span.end_byte <= source->bytes.length &&
         (length == 0u ||
          memcmp(source->bytes.data + span.start_byte, literal, length) == 0);
}

static bool fixture_type_records_equal(const fixture *fixture_value,
                                       uint32_t left_index,
                                       uint32_t right_index, size_t depth) {
  if (fixture_value == NULL ||
      left_index >= fixture_value->result.written.types ||
      right_index >= fixture_value->result.written.types ||
      depth >= W_SEED_FRONTEND_MAX_NESTING)
    return false;
  const w_seed_frontend_type *left = &fixture_value->types[left_index];
  const w_seed_frontend_type *right = &fixture_value->types[right_index];
  if (left->kind != right->kind) return false;
  if (left->kind == W_SEED_FRONTEND_TYPE_TUPLE) {
    if (left->tuple_component_count == 0u ||
        left->tuple_component_count != right->tuple_component_count ||
        left->first_tuple_component == W_SEED_FRONTEND_NONE ||
        right->first_tuple_component == W_SEED_FRONTEND_NONE ||
        (size_t)left->first_tuple_component + left->tuple_component_count >
            fixture_value->result.written.tuple_components ||
        (size_t)right->first_tuple_component + right->tuple_component_count >
            fixture_value->result.written.tuple_components)
      return false;
    for (uint32_t ordinal = 0u; ordinal < left->tuple_component_count;
         ordinal += 1u) {
      const w_seed_frontend_tuple_component *left_component =
          &fixture_value->tuple_components[left->first_tuple_component +
                                           ordinal];
      const w_seed_frontend_tuple_component *right_component =
          &fixture_value->tuple_components[right->first_tuple_component +
                                           ordinal];
      if (left_component->ordinal != ordinal ||
          right_component->ordinal != ordinal ||
          left_component->owner_type != left_index ||
          right_component->owner_type != right_index ||
          left_component->label.length != right_component->label.length ||
          (left_component->label.length != 0u &&
           (left_component->label.data == NULL ||
            right_component->label.data == NULL ||
            memcmp(left_component->label.data, right_component->label.data,
                   left_component->label.length) != 0)) ||
          !fixture_type_records_equal(fixture_value,
                                      left_component->type_index,
                                      right_component->type_index, depth + 1u))
        return false;
    }
    return true;
  }
  if (left->kind == W_SEED_FRONTEND_TYPE_INTEGER)
    return left->is_signed == right->is_signed &&
           left->bit_width == right->bit_width;
  if (left->kind == W_SEED_FRONTEND_TYPE_FLOAT)
    return left->bit_width == right->bit_width;
  if (left->external_module_index != W_SEED_FRONTEND_NONE ||
      right->external_module_index != W_SEED_FRONTEND_NONE ||
      left->external_symbol_index != W_SEED_FRONTEND_NONE ||
      right->external_symbol_index != W_SEED_FRONTEND_NONE)
    return left->external_module_index == right->external_module_index &&
           left->external_symbol_index == right->external_symbol_index;
  return left->spelling.length == right->spelling.length &&
         (left->spelling.length == 0u ||
          (left->spelling.data != NULL && right->spelling.data != NULL &&
           memcmp(left->spelling.data, right->spelling.data,
                  left->spelling.length) == 0));
}

static const w_seed_frontend_diagnostic *diagnostic_for_code(
    const fixture *fixture_value, const char *code) {
  if (fixture_value == NULL || code == NULL) return NULL;
  const size_t length = strlen(code);
  for (size_t index = 0u;
       index < fixture_value->result.written.diagnostics; index += 1u) {
    const w_seed_frontend_diagnostic *diagnostic =
        &fixture_value->diagnostics[index];
    if (diagnostic->code.length == length &&
        memcmp(diagnostic->code.data, code, length) == 0)
      return diagnostic;
  }
  return NULL;
}

static const w_seed_frontend_diagnostic *diagnostic_for_code_occurrence(
    const fixture *fixture_value, const char *code, size_t occurrence) {
  if (fixture_value == NULL || code == NULL) return NULL;
  const size_t length = strlen(code);
  size_t seen = 0u;
  for (size_t index = 0u;
       index < fixture_value->result.written.diagnostics; index += 1u) {
    const w_seed_frontend_diagnostic *diagnostic =
        &fixture_value->diagnostics[index];
    if (diagnostic->code.length != length ||
        memcmp(diagnostic->code.data, code, length) != 0)
      continue;
    if (seen == occurrence) return diagnostic;
    seen += 1u;
  }
  return NULL;
}

static bool diagnostic_record_ranges_are_valid(
    const fixture *fixture_value,
    const w_seed_frontend_diagnostic *diagnostic) {
  if (fixture_value == NULL || diagnostic == NULL ||
      diagnostic->document_index >= fixture_value->input.document_count ||
      fixture_value->input.documents == NULL)
    return false;
  const w_seed_frontend_document *document =
      &fixture_value->input.documents[diagnostic->document_index];
  if (document->source == NULL ||
      diagnostic->primary.start_byte > diagnostic->primary.end_byte ||
      diagnostic->primary.end_byte > document->source->bytes.length ||
      (size_t)diagnostic->first_fact + diagnostic->fact_count >
          fixture_value->result.written.diagnostic_facts ||
      (size_t)diagnostic->first_label + diagnostic->label_count >
          fixture_value->result.written.diagnostic_labels)
    return false;
  for (size_t offset = 0u; offset < diagnostic->label_count; offset += 1u) {
    const w_seed_frontend_diagnostic_label *label =
        &fixture_value->diagnostic_labels[diagnostic->first_label + offset];
    if (label->document_index >= fixture_value->input.document_count ||
        label->role.length == 0u || label->role.data == NULL ||
        label->span.start_byte > label->span.end_byte ||
        label->span.end_byte >
            fixture_value->input.documents[label->document_index]
                .source->bytes.length)
      return false;
  }
  return true;
}

static const w_seed_frontend_diagnostic_fact *diagnostic_fact_for(
    const fixture *fixture_value, const w_seed_frontend_diagnostic *diagnostic,
    size_t offset) {
  if (!diagnostic_record_ranges_are_valid(fixture_value, diagnostic) ||
      offset >= diagnostic->fact_count)
    return NULL;
  return &fixture_value->diagnostic_facts[diagnostic->first_fact + offset];
}

static bool diagnostic_fact_string_is(
    const fixture *fixture_value, const w_seed_frontend_diagnostic *diagnostic,
    size_t offset, const char *key, const char *value) {
  const w_seed_frontend_diagnostic_fact *fact =
      diagnostic_fact_for(fixture_value, diagnostic, offset);
  return fact != NULL && fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING &&
         frontend_text_is(fact->key, key) && frontend_text_is(fact->text, value);
}

static bool diagnostic_fact_array_is(
    const fixture *fixture_value, const w_seed_frontend_diagnostic *diagnostic,
    size_t offset, const char *key, const char *const *values,
    size_t value_count) {
  const w_seed_frontend_diagnostic_fact *fact =
      diagnostic_fact_for(fixture_value, diagnostic, offset);
  if (fact == NULL || fact->kind != W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY ||
      !frontend_text_is(fact->key, key) ||
      fact->item_count != value_count)
    return false;
  if (value_count == 0u)
    return fact->first_item == W_SEED_FRONTEND_NONE;
  if (fact->first_item == W_SEED_FRONTEND_NONE ||
      (size_t)fact->first_item + fact->item_count >
          fixture_value->result.written.diagnostic_items)
    return false;
  for (size_t item = 0u; item < value_count; item += 1u) {
    if (!frontend_text_is(
            fixture_value->diagnostic_items[fact->first_item + item].text,
            values[item]))
      return false;
  }
  return true;
}

static bool diagnostic_fact_set_is(
    const fixture *fixture_value, const w_seed_frontend_diagnostic *diagnostic,
    size_t offset, const char *key, const char *const *values,
    size_t value_count) {
  const w_seed_frontend_diagnostic_fact *fact =
      diagnostic_fact_for(fixture_value, diagnostic, offset);
  if (fact == NULL || fact->kind != W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET ||
      !frontend_text_is(fact->key, key) ||
      fact->item_count != value_count)
    return false;
  if (value_count == 0u)
    return fact->first_item == W_SEED_FRONTEND_NONE;
  if (fact->first_item == W_SEED_FRONTEND_NONE ||
      (size_t)fact->first_item + fact->item_count >
          fixture_value->result.written.diagnostic_items)
    return false;
  for (size_t item = 0u; item < value_count; item += 1u) {
    if (!frontend_text_is(
            fixture_value->diagnostic_items[fact->first_item + item].text,
            values[item]))
      return false;
  }
  return true;
}

static bool diagnostic_fact_integer_is(
    const fixture *fixture_value, const w_seed_frontend_diagnostic *diagnostic,
    size_t offset, const char *key, int64_t value) {
  const w_seed_frontend_diagnostic_fact *fact =
      diagnostic_fact_for(fixture_value, diagnostic, offset);
  return fact != NULL && fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER &&
         frontend_text_is(fact->key, key) && fact->integer_value == value &&
         fact->item_count == 0u && fact->first_item == W_SEED_FRONTEND_NONE;
}

static bool diagnostic_label_role_is(
    const fixture *fixture_value, const w_seed_frontend_diagnostic *diagnostic,
    size_t offset, const char *role) {
  if (!diagnostic_record_ranges_are_valid(fixture_value, diagnostic) ||
      offset >= diagnostic->label_count || role == NULL)
    return false;
  return frontend_text_is(
      fixture_value
          ->diagnostic_labels[diagnostic->first_label + offset]
          .role,
      role);
}

static bool test_declarations_and_determinism(void) {
  static const char source[] =
      "module demo\n"
      "import { ext } from dep\n"
      "export struct Pair { let left: u8 let right: u16 }\n"
      "export type Number = u32\n"
      "export alias Flag = Bool?\n"
      "export fn add(left: u8, right: u16): u16 { return left + right }\n"
      "entry(add)\n";
  fixture *first = &fixture_a;
  fixture *second = &fixture_b;
  CHECK(fixture_run(first, source));
  CHECK(first->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(first->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(first->result.written.modules == 1);
  CHECK(first->result.written.imports == 1);
  CHECK(first->result.written.structs == 1);
  CHECK(first->result.written.fields == 2);
  CHECK(first->result.written.functions == 1);
  CHECK(first->result.written.entries == 1);
  CHECK(first->result.written.parameters == 2);
  CHECK(first->result.written.receipt_bytes != 0);
  w_seed_frontend_counts measured_counts;
  w_seed_frontend_result measured_result;
  CHECK(w_seed_frontend_measure(&first->input, &measured_counts,
                                &measured_result) == W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&measured_counts, &first->result.required));
  CHECK(counts_equal(&first->result.required, &first->result.written));
  CHECK(measured_result.required.receipt_bytes ==
        first->result.required.receipt_bytes);
  CHECK(first->modules[0].module_id.length == 4);
  CHECK(first->functions[0].name.length == 3);
  CHECK(first->aliases[0].type_index != W_SEED_FRONTEND_NONE);
  CHECK(first->types[first->aliases[0].type_index].kind ==
        W_SEED_FRONTEND_TYPE_OPTION);
  CHECK(first->types[first->aliases[0].type_index].element_type !=
        W_SEED_FRONTEND_NONE);
  CHECK(fixture_run(second, source));
  CHECK(second->result.status == first->result.status);
  CHECK(second->result.receipt_bytes == first->result.receipt_bytes);
  CHECK(memcmp(first->receipt, second->receipt, first->result.receipt_bytes) == 0);
  return true;
}

static bool test_enums_and_payloads(void) {
  static const char source[] =
      "export enum Outcome: Error {\n"
      "  ready\n"
      "  delayed(Duration)\n"
      "  failed(reason: Failure, code: u16)\n"
      "}\n";
  fixture *value = &fixture_a;
  CHECK(fixture_run(value, source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  CHECK(value->result.written.enums == 1);
  CHECK(value->result.written.enum_cases == 3);
  CHECK(value->result.written.enum_case_parameters == 3);
  CHECK(value->modules[0].first_enum == 0 && value->modules[0].enum_count == 1);
  CHECK(value->enums[0].type_index != W_SEED_FRONTEND_NONE);
  CHECK(value->types[value->enums[0].type_index].kind ==
        W_SEED_FRONTEND_TYPE_ENUM);
  CHECK(value->enums[0].conformance_type != W_SEED_FRONTEND_NONE);
  CHECK(value->enum_cases[0].owner_enum == 0 &&
        value->enum_cases[1].owner_enum == 0 &&
        value->enum_cases[2].owner_enum == 0);
  CHECK(value->enum_cases[1].payload_count == 1);
  CHECK(value->enum_case_parameters[0].owner_case == 1 &&
        value->enum_case_parameters[1].owner_case == 2 &&
        value->enum_case_parameters[2].owner_case == 2);
  CHECK(value->enum_case_parameters[0].has_label == false);
  CHECK(value->enum_case_parameters[1].has_label == true);
  CHECK(value->enum_case_parameters[1].label.length == 6);
  static const char *const case_names[] = {"ready", "delayed", "failed"};
  size_t case_symbol_count = 0;
  for (size_t symbol = 0; symbol < value->result.written.symbols; symbol += 1) {
    const w_seed_frontend_symbol *record = &value->symbols[symbol];
    if (record->kind != W_SEED_FRONTEND_SYMBOL_ENUM_CASE) continue;
    CHECK(case_symbol_count < 3);
    CHECK(record->owner_index == case_symbol_count);
    CHECK(record->type_index == value->enums[0].type_index);
    const size_t expected_length = strlen(case_names[case_symbol_count]);
    CHECK(record->name.length == expected_length &&
          memcmp(record->name.data, case_names[case_symbol_count],
                 expected_length) == 0);
    case_symbol_count += 1;
  }
  CHECK(case_symbol_count == 3);
  fixture *repeat = &fixture_b;
  CHECK(fixture_run(repeat, source));
  CHECK(repeat->result.status == value->result.status);
  CHECK(counts_equal(&repeat->result.written, &value->result.written));
  CHECK(repeat->result.receipt_bytes == value->result.receipt_bytes);
  CHECK(memcmp(repeat->receipt, value->receipt, value->result.receipt_bytes) ==
        0);

  fixture *duplicate = &fixture_duplicate;
  CHECK(fixture_run(duplicate, "enum E { same same }\n"));
  CHECK(duplicate->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(duplicate->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(duplicate->result.written.enums == 1 &&
        duplicate->result.written.enum_cases == 2);
  CHECK(has_fact(duplicate, W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL));

  fixture *generic = &fixture_generic;
  CHECK(fixture_run(generic, "enum Box<T> { value(T) }\n"));
  CHECK(generic->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(generic->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(generic, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE));

  fixture *callback = &fixture_callback;
  CHECK(fixture_run(
      callback,
      "enum Callbacks { positional(fn(u32): Bool) "
      "labeled(handler: fn(u32): Bool) }\n"));
  CHECK(callback->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(callback->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(callback->result.written.enums == 1 &&
        callback->result.written.enum_cases == 2 &&
        callback->result.written.enum_case_parameters == 2);
  CHECK(callback->enum_cases[0].owner_enum == 0 &&
        callback->enum_cases[1].owner_enum == 0);
  CHECK(callback->enum_case_parameters[0].owner_case == 0 &&
        callback->enum_case_parameters[1].owner_case == 1);
  CHECK(callback->enum_case_parameters[0].has_label == false &&
        callback->enum_case_parameters[1].has_label == true);
  CHECK(callback->enum_case_parameters[0].label.length == 0 &&
        callback->enum_case_parameters[1].label.length == 7);
  CHECK(has_fact(callback, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE));

  fixture *collision = &fixture_collision;
  CHECK(fixture_run(collision, "enum E { first }\nstruct E {}\n"));
  CHECK(collision->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(collision->result.written.enums == 1 &&
        collision->result.written.structs == 1 &&
        collision->result.written.enum_cases == 1);
  CHECK(has_fact(collision, W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL));
  CHECK(fixture_run(collision, "enum E { first }\nfn E(){}\n"));
  CHECK(collision->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(collision->result.written.enums == 1 &&
        collision->result.written.functions == 1 &&
        collision->result.written.enum_cases == 1);
  CHECK(has_fact(collision, W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL));
  return true;
}

static bool test_typed_throw_projection(void) {
  static const char source[] =
      "enum Failure: Error { denied }\n"
      "fn fail(): () throws Failure { throw .denied }\n"
      "entry { }\n";
  fixture *value = &fixture_a;
  CHECK(fixture_run(value, source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  CHECK(value->result.written.enums == 1u &&
        value->result.written.functions == 2u &&
        value->result.written.statements == 1u);
  const w_seed_frontend_function *function = &value->functions[0];
  CHECK(function->is_throws &&
        function->error_type != W_SEED_FRONTEND_NONE &&
        function->error_type < value->result.written.types &&
        value->types[function->error_type].kind == W_SEED_FRONTEND_TYPE_ENUM &&
        value->types[function->error_type].enum_base_index == 0u);
  CHECK(value->enums[0].conformance_type != W_SEED_FRONTEND_NONE &&
        frontend_text_is(value->types[value->enums[0].conformance_type].spelling,
                         "Error"));
  CHECK(value->statements[0].kind == W_SEED_FRONTEND_STMT_THROW &&
        value->statements[0].expression_index != W_SEED_FRONTEND_NONE);
  const w_seed_frontend_expression *error =
      &value->expressions[value->statements[0].expression_index];
  CHECK(error->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE);
  CHECK(error->inferred_type != W_SEED_FRONTEND_NONE &&
        error->inferred_type < value->result.written.types);
  CHECK(value->types[error->inferred_type].kind == W_SEED_FRONTEND_TYPE_ENUM &&
        value->types[error->inferred_type].enum_base_index == 0u);
  CHECK(error->enum_index == 0u && error->enum_case_index == 0u);

  static const char propagation_source[] =
      "enum Failure: Error { denied }\n"
      "fn leaf(): i64 throws Failure { throw .denied }\n"
      "fn relay(): i64 throws Failure { return try leaf() }\n"
      "entry { }\n";
  CHECK(fixture_run(value, propagation_source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  const w_seed_frontend_expression *try_expression = NULL;
  for (size_t index = 0u; index < value->result.written.expressions; index += 1u) {
    if (value->expressions[index].kind == W_SEED_FRONTEND_EXPR_TRY) {
      CHECK(try_expression == NULL);
      try_expression = &value->expressions[index];
    }
  }
  CHECK(try_expression != NULL && try_expression->supported &&
        try_expression->left != W_SEED_FRONTEND_NONE &&
        try_expression->left < value->result.written.expressions &&
        try_expression->propagated_error_enum == 0u &&
        try_expression->inferred_type < value->result.written.types &&
        value->types[try_expression->inferred_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        value->types[try_expression->inferred_type].is_signed &&
        value->types[try_expression->inferred_type].bit_width == 64u &&
        frontend_text_is(try_expression->operator_text, "try"));
  const w_seed_frontend_expression *propagated_call =
      &value->expressions[try_expression->left];
  CHECK(propagated_call->kind == W_SEED_FRONTEND_EXPR_CALL &&
        propagated_call->supported &&
        propagated_call->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION &&
        propagated_call->resolved_function_index == 0u &&
        propagated_call->inferred_type == try_expression->inferred_type);
  CHECK(receipt_contains(
      value, "try-expression=", strlen("try-expression=")));

  fixture *invalid = &fixture_b;
  CHECK(fixture_run(invalid,
                    "enum Failure: Error { denied } "
                    "fn fail(): () { throw .denied } entry { }"));
  CHECK(invalid->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(invalid->statements[0].kind == W_SEED_FRONTEND_STMT_UNSUPPORTED);

  CHECK(fixture_run(
      invalid,
      "enum Failure: Error { denied } "
      "fn leaf(): i64 throws Failure { throw .denied } "
      "fn relay(): i64 throws Failure { return leaf() } entry { }"));
  CHECK(invalid->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  bool rejected_unmarked_call = false;
  for (size_t index = 0u; index < invalid->result.written.expressions; index += 1u) {
    const w_seed_frontend_expression *candidate = &invalid->expressions[index];
    if (candidate->kind == W_SEED_FRONTEND_EXPR_CALL &&
        candidate->resolved_function_index == 0u) {
      rejected_unmarked_call = !candidate->supported;
    }
  }
  CHECK(rejected_unmarked_call);

  CHECK(fixture_run(
      invalid,
      "enum SourceFailure: Error { denied } "
      "enum RelayFailure: Error { rejected } "
      "fn leaf(): i64 throws SourceFailure { throw .denied } "
      "fn relay(): i64 throws RelayFailure { return try leaf() } entry { }"));
  CHECK(invalid->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(invalid, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));

  CHECK(fixture_run(
      invalid,
      "enum Failure: Error { denied } "
      "fn leaf(): i64 throws Failure { throw .denied } "
      "fn relay(): i64 { return try leaf() } entry { }"));
  CHECK(invalid->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(invalid, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));

  CHECK(fixture_run(
      invalid,
      "enum Failure: Error { denied } fn leaf(): i64 { return 1 } "
      "fn relay(): i64 throws Failure { return try leaf() } entry { }"));
  CHECK(invalid->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(invalid, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));

  CHECK(fixture_run(
      invalid,
      "enum Failure: Error { denied } "
      "fn leaf(): i64 throws Failure { throw .denied } "
      "fn relay(): i64 throws Failure { return try? leaf() } entry { }"));
  CHECK(invalid->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(invalid, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));

  CHECK(fixture_run(
      invalid,
      "enum Failure: Error { denied } "
      "async fn leaf(): i64 throws Failure { throw .denied } "
      "fn relay(): i64 throws Failure { return try leaf() } entry { }"));
  CHECK(invalid->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(invalid, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  return true;
}

static bool test_synchronous_defer_projection(void) {
  static const char source[] =
      "enum Failure: Error { denied }\n"
      "fn clean() { }\n"
      "fn leaf(): i64 throws Failure { throw .denied }\n"
      "fn relay(): i64 throws Failure {\n"
      "  defer { clean() }\n"
      "  return try leaf()\n"
      "}\n"
      "entry { }\n";
  fixture *value = &fixture_a;
  CHECK(fixture_run(value, source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  uint32_t clean_function = W_SEED_FRONTEND_NONE;
  uint32_t relay_function = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.functions; index += 1u) {
    if (frontend_text_is(value->functions[index].name, "clean"))
      clean_function = (uint32_t)index;
    if (frontend_text_is(value->functions[index].name, "relay"))
      relay_function = (uint32_t)index;
  }
  CHECK(clean_function != W_SEED_FRONTEND_NONE &&
        relay_function != W_SEED_FRONTEND_NONE);
  uint32_t cleanup_index = W_SEED_FRONTEND_NONE;
  const w_seed_frontend_function *relay = &value->functions[relay_function];
  for (size_t offset = 0u; offset < relay->statement_count; offset += 1u) {
    const uint32_t index = relay->first_statement + (uint32_t)offset;
    if (value->statements[index].kind == W_SEED_FRONTEND_STMT_DEFER) {
      CHECK(cleanup_index == W_SEED_FRONTEND_NONE);
      cleanup_index = index;
    }
  }
  CHECK(cleanup_index != W_SEED_FRONTEND_NONE);
  const w_seed_frontend_statement *cleanup = &value->statements[cleanup_index];
  CHECK(cleanup->expression_index == W_SEED_FRONTEND_NONE &&
        cleanup->first_child != W_SEED_FRONTEND_NONE &&
        cleanup->child_count == 1u);
  const w_seed_frontend_statement *body =
      &value->statements[cleanup->first_child];
  CHECK(body->kind == W_SEED_FRONTEND_STMT_EXPRESSION &&
        body->next_sibling == W_SEED_FRONTEND_NONE &&
        body->expression_index != W_SEED_FRONTEND_NONE);
  const w_seed_frontend_expression *call =
      &value->expressions[body->expression_index];
  CHECK(call->kind == W_SEED_FRONTEND_EXPR_CALL && call->supported &&
        call->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION &&
        call->resolved_function_index == clean_function &&
        call->argument_count == 0u &&
        call->first_argument <= value->result.written.arguments);

  static const char *const rejected[] = {
      "fn clean(){} fn f(){defer async {clean()}} entry{}",
      "fn clean(){} fn f(){if true {defer {clean()}}} entry{}",
      "fn clean(){} fn f(){defer {clean()} defer {clean()}} entry{}",
      "async fn clean(){} fn f(){defer {clean()}} entry{}",
      "enum E: Error { failed } fn clean() throws E {throw .failed} "
      "fn f(){defer {clean()}} entry{}",
      "fn clean(value:i64){} fn f(){defer {clean()}} entry{}",
      "fn clean():i64{return 1} fn f(){defer {clean()}} entry{}",
      "enum E: Error { failed } fn leaf():i64 throws E {throw .failed} "
      "fn clean(){} fn f():i64 throws E{return try leaf();defer {clean()}} "
      "entry{}",
  };
  fixture *invalid = &fixture_b;
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    CHECK(fixture_run(invalid, rejected[index]));
    CHECK(invalid->parse.status == W_SEED_PARSE_COMPLETE);
    CHECK(invalid->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  }

  CHECK(fixture_parse(invalid, "fn f(){defer {cleanup()}} entry{}"));
  invalid->host_symbols[0] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"cleanup", 7u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = NULL,
      .requirement_count = 0u};
  invalid->host_scope = (w_seed_frontend_host_prelude){
      .profile = (w_seed_frontend_text){"native-process@1", 16u},
      .symbols = invalid->host_symbols,
      .symbol_count = 1u};
  invalid->input.host_scope = &invalid->host_scope;
  (void)w_seed_frontend_run(&invalid->input, &invalid->output,
                            &invalid->result);
  CHECK(invalid->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  return true;
}

static bool test_semantic_diagnostics(void) {
  fixture *condition = &fixture_condition;
  CHECK(fixture_run(condition,
                    "fn f(): () { if 1 { return } }\nentry(f)\n"));
  CHECK(condition->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(condition->result.written.diagnostics >= 1);
  CHECK(condition->diagnostics[0].code.length == 10 &&
        memcmp(condition->diagnostics[0].code.data, "W-SEM-0001", 10) == 0);
  const w_seed_frontend_diagnostic *semantic =
      diagnostic_for_code(condition, "W-SEM-0001");
  CHECK(semantic != NULL && semantic->fact_count == 2u &&
        semantic->label_count == 0u &&
        diagnostic_record_ranges_are_valid(condition, semantic));
  CHECK(diagnostic_fact_string_is(condition, semantic, 0u, "actual", "1"));
  CHECK(diagnostic_fact_string_is(condition, semantic, 1u, "expected", "Bool"));

  fixture *narrowing = &fixture_narrowing;
  CHECK(fixture_run(narrowing,
                    "fn f(value: u32): u16 { return value }\nentry(f)\n"));
  bool saw_narrowing = false;
  for (size_t index = 0; index < narrowing->result.written.diagnostics; index += 1) {
    if (narrowing->diagnostics[index].code.length == 11 &&
        memcmp(narrowing->diagnostics[index].code.data, "W-TYPE-0122", 11) == 0)
      saw_narrowing = true;
  }
  CHECK(saw_narrowing);

  fixture *label = &fixture_label;
  CHECK(fixture_run(label,
                    "fn callee(value: u32): u32 { return value }\n"
                    "fn f(): u32 { return callee(other: 1) }\nentry(f)\n"));
  bool saw_label = false;
  for (size_t index = 0; index < label->result.written.diagnostics; index += 1) {
    if (label->diagnostics[index].code.length == 12 &&
        memcmp(label->diagnostics[index].code.data, "W-LABEL-0005", 12) == 0)
      saw_label = true;
  }
  CHECK(saw_label);
  const w_seed_frontend_diagnostic *unknown_label =
      diagnostic_for_code(label, "W-LABEL-0005");
  static const char *const value_form[] = {"value"};
  CHECK(unknown_label != NULL && unknown_label->fact_count == 3u &&
        unknown_label->label_count == 0u &&
        diagnostic_record_ranges_are_valid(label, unknown_label));
  CHECK(diagnostic_fact_array_is(label, unknown_label, 0u, "acceptedForms",
                                 value_form, 1u));
  CHECK(diagnostic_fact_string_is(label, unknown_label, 1u, "declaration",
                                  "callee"));
  CHECK(diagnostic_fact_string_is(label, unknown_label, 2u, "label", "other"));
  fixture *call_type = &fixture_label;
  CHECK(fixture_run(call_type,
                    "fn callee(value: u32): u32 { return value }\n"
                    "fn f(): u32 { return callee(value: true) }\nentry(f)\n"));
  CHECK(call_type->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(call_type, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  CHECK(fixture_run(call_type,
                    "fn inspect(named: Bool): Bool { return named }\n"
                    "fn f(): Bool { return inspect(named: true) }\nentry(f)\n"));
  CHECK(call_type->result.status == W_SEED_FRONTEND_OK);
  CHECK(fixture_run(call_type,
                    "fn inspect(value: Bool): Bool { return value }\n"
                    "fn f(): Bool { return inspect(value: true) }\nentry(f)\n"));
  CHECK(call_type->result.status == W_SEED_FRONTEND_OK);

  fixture *literal = &fixture_literal;
  CHECK(fixture_run(literal, "fn f(): u32 { return 1 }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_OK);
  CHECK(fixture_run(literal, "fn f(): u16 { return 250_u8 }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_OK);
  CHECK(fixture_run(literal,
                    "fn f(): u16 { return 70_000_u32 }\nentry(f)\n"));
  bool saw_literal_narrowing = false;
  for (size_t index = 0; index < literal->result.written.diagnostics;
       index += 1) {
    if (literal->diagnostics[index].code.length == 11 &&
        memcmp(literal->diagnostics[index].code.data, "W-TYPE-0122", 11) == 0) {
      saw_literal_narrowing = true;
    }
  }
  CHECK(saw_literal_narrowing);
  CHECK(fixture_run(literal,
                    "fn f(): u32 { return 0xdead_u32 }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_OK);
  CHECK(fixture_run(literal,
                    "fn f(): u16 { return 0o755_u16 + 0b1111_0000_u16 }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_OK);
  CHECK(fixture_run(literal,
                    "fn f(): u8 { return 999_u8 }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(literal, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  CHECK(fixture_run(literal,
                    "fn f(): f32 { return 16777217 }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(literal->result.written.diagnostics != 0);
  CHECK(fixture_run(literal,
                    "fn f(): u32 { return missing }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(literal, W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL));
  CHECK(fixture_run(literal, "fn f(): () { missing }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(literal, W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL));
  CHECK(fixture_run(literal,
                    "fn f(): u32 { let x: u32 = 1 return x }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_OK);
  CHECK(fixture_run(literal,
                    "fn f(): u32 { let x: u32 = true return x }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(fixture_run(literal,
                    "fn f(): u32 { return x let x: u32 = 1 }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(literal, W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL));
  CHECK(fixture_run(literal,
                    "fn f(): u32 { if true { let x: u32 = 1 } return x }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(literal, W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL));
  CHECK(fixture_run(literal, "fn f(): u32 { return }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(fixture_run(literal, "fn f(): u7 { return 1 }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(literal, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE));
  CHECK(fixture_run(literal,
                    "fn f(): u32 { return 1 << 2 }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(literal, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  CHECK(fixture_run(literal,
                    "fn f(): u32 { return 1 ?? 2 }\nentry(f)\n"));
  CHECK(literal->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  return true;
}

static bool append_many_source(char *destination, size_t capacity,
                               size_t *length, const char *text) {
  if (destination == NULL || length == NULL || text == NULL) return false;
  const size_t text_length = strlen(text);
  if (*length >= capacity || text_length >= capacity - *length) return false;
  (void)memcpy(destination + *length, text, text_length);
  *length += text_length;
  destination[*length] = '\0';
  return true;
}

static bool append_many_piece(char *destination, size_t capacity,
                              size_t *length, size_t index,
                              const char *prefix, const char *suffix) {
  char piece[64];
  const int written = snprintf(piece, sizeof(piece), "%s%llu%s", prefix,
                               (unsigned long long)index, suffix);
  if (written < 0 || (size_t)written >= sizeof(piece)) return false;
  return append_many_source(destination, capacity, length, piece);
}

static bool append_kernel_contract_field(char *destination, size_t capacity,
                                         size_t *length, size_t index,
                                         bool last) {
  char piece[64];
  const int written = snprintf(
      piece, sizeof(piece), "k%llu: f%llu%s", (unsigned long long)index,
      (unsigned long long)index, last ? "" : ", ");
  if (written < 0 || (size_t)written >= sizeof(piece)) return false;
  return append_many_source(destination, capacity, length, piece);
}

static bool scalar_if_frontend_shape(const fixture *value,
                                     size_t expected_count) {
  if (value == NULL) return false;
  size_t if_count = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_IF) continue;
    if_count += 1u;
    CHECK(expression->supported);
    CHECK(expression->left < value->result.written.expressions &&
          expression->right < value->result.written.expressions &&
          expression->else_expression < value->result.written.expressions);
    CHECK(expression->inferred_type < value->result.written.types);
    const w_seed_frontend_expression *condition =
        &value->expressions[expression->left];
    const w_seed_frontend_expression *then_arm =
        &value->expressions[expression->right];
    const w_seed_frontend_expression *else_arm =
        &value->expressions[expression->else_expression];
    CHECK(condition->inferred_type < value->result.written.types &&
          value->types[condition->inferred_type].kind ==
              W_SEED_FRONTEND_TYPE_BOOL);
    CHECK(then_arm->inferred_type < value->result.written.types &&
          else_arm->inferred_type < value->result.written.types);
    const w_seed_frontend_type *then_type =
        &value->types[then_arm->inferred_type];
    const w_seed_frontend_type *else_type =
        &value->types[else_arm->inferred_type];
    const w_seed_frontend_type *result_type =
        &value->types[expression->inferred_type];
    CHECK(then_type->kind == else_type->kind &&
          then_type->kind == result_type->kind);
    if (then_type->kind == W_SEED_FRONTEND_TYPE_INTEGER) {
      CHECK(then_type->is_signed == else_type->is_signed &&
            then_type->bit_width == else_type->bit_width &&
            then_type->is_signed == result_type->is_signed &&
            then_type->bit_width == result_type->bit_width &&
            then_type->is_signed && then_type->bit_width == 64u);
    } else if (then_type->kind == W_SEED_FRONTEND_TYPE_FLOAT) {
      CHECK(then_type->bit_width == else_type->bit_width &&
            then_type->bit_width == result_type->bit_width &&
            (then_type->bit_width == 32u || then_type->bit_width == 64u));
    } else {
      CHECK(then_type->kind == W_SEED_FRONTEND_TYPE_BOOL);
    }
  }
  CHECK(if_count == expected_count);
  return true;
}

static bool scalar_if_unsupported(fixture *value, const char *source) {
  CHECK(fixture_run(value, source));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(value->result.written.diagnostics == 0u);
  CHECK(has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  return true;
}

static bool test_scalar_if_frontend_subset(void) {
  fixture *value = &fixture_scalar_if;
  static const char i64_source[] =
      "fn serve(isOpen: Bool, seats: i64): i64 { "
      "let next = if isOpen { seats + 1 } else { seats - 1 } "
      "return if isOpen { next } else { seats } }\n"
      "entry(serve)\n";
  CHECK(fixture_run(value, i64_source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK);
  CHECK(value->result.written.diagnostics == 0u &&
        value->result.written.facts == 0u);
  CHECK(scalar_if_frontend_shape(value, 2u));
  w_seed_frontend_counts measured_counts;
  w_seed_frontend_result measured_result;
  CHECK(w_seed_frontend_measure(&value->input, &measured_counts,
                                &measured_result) == W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&measured_counts, &value->result.required));
  CHECK(measured_result.required.receipt_bytes ==
        value->result.required.receipt_bytes);

  static const char bool_source[] =
      "fn choose(isOpen: Bool): Bool { "
      "let state = if isOpen { true } else { false } "
      "return if isOpen { state } else { false } }\n"
      "entry(choose)\n";
  CHECK(fixture_run(value, bool_source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK);
  CHECK(scalar_if_frontend_shape(value, 2u));

  static const char literal_source[] =
      "fn literal(isOpen: Bool): i64 { "
      "let seats = if isOpen { 5 } else { 2 } return seats }\n"
      "entry(literal)\n";
  CHECK(fixture_run(value, literal_source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK);
  CHECK(scalar_if_frontend_shape(value, 1u));
  bool saw_integer_literal = false;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_INTEGER) continue;
    saw_integer_literal = true;
    CHECK(expression->has_integer_value);
  }
  CHECK(saw_integer_literal);

  CHECK(fixture_parse(value,
                      "fn missing(isOpen: Bool): i64 { "
                      "return if isOpen { 1 } }\nentry(missing)\n"));
  CHECK(value->parse.status != W_SEED_PARSE_COMPLETE &&
        has_parse_issue(value, W_SEED_PARSE_ISSUE_VALUE_IF_MISSING_ELSE));

  CHECK(fixture_run(value,
                    "fn nonBool(): i64 { return if 1 { 2 } else { 3 } }\n"
                    "entry(nonBool)\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        value->result.written.diagnostics == 1u &&
        has_diagnostic(value, "W-SEM-0001") &&
        !has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));

  CHECK(fixture_run(value,
                    "fn mismatch(isOpen: Bool): i64 { "
                    "return if isOpen { 2 } else { false } }\n"
                    "entry(mismatch)\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        value->result.written.diagnostics == 1u &&
        has_diagnostic(value, "W-TYPE-0120") &&
        !has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));

  CHECK(fixture_run(
      value,
      "fn nested(): i64 { return if true { if false { 1 } else { 2 } } "
      "else { 3 } }\nentry(nested)\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.diagnostics == 0u &&
        value->result.written.facts == 0u);
  CHECK(scalar_if_frontend_shape(value, 2u));
  static const char float_source[] =
      "fn choose64(select: Bool): f64 { return if select { "
      "2.5_f64 } else { 3.25_f64 } }\n"
      "fn choose32(select: Bool): f32 { return if select { "
      "1.0_f32 } else { 2.0_f32 } }\n"
      "entry { }\n";
  CHECK(fixture_run(value, float_source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  CHECK(value->result.written.diagnostics == 0u &&
        value->result.written.facts == 0u);
  CHECK(scalar_if_frontend_shape(value, 2u));
  CHECK(fixture_run(
      value,
      "fn mismatch(select: Bool): f64 { return if select { "
      "2.5_f64 } else { 2.0_f32 } }\nentry { }\n"));
  CHECK(value->result.status != W_SEED_FRONTEND_OK &&
        (has_diagnostic(value, "W-TYPE-0120") ||
         has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION)));
  CHECK(fixture_parse(
      value,
      "fn missingFloat(select: Bool): f64 { return if select { "
      "2.5_f64 } }\nentry { }\n"));
  CHECK(value->parse.status != W_SEED_PARSE_COMPLETE &&
        has_parse_issue(value, W_SEED_PARSE_ISSUE_VALUE_IF_MISSING_ELSE));
  CHECK(scalar_if_unsupported(
      value,
      "fn text(): String { return if true { \"a\" } else { \"b\" } }\n"
      "entry(text)\n"));
  CHECK(scalar_if_unsupported(
      value,
      "fn value(): i64 { return 1 }\n"
      "fn callArm(): i64 { return if true { value() } else { 2 } }\n"
      "entry(callArm)\n"));
  CHECK(scalar_if_unsupported(
      value,
      "fn effect(seats: i64): i64 { "
      "return if true { seats = 1 } else { seats } }\n"
      "entry(effect)\n"));
  return true;
}

static bool test_enum_subsets(void) {
  static const char source[] =
      "enum Stage { accepted reserving preparing serving }\n"
      "alias FullStage = Stage<[.serving, .accepted, .preparing, .reserving]>\n"
      "alias WorkStage = Stage<[Stage.serving, .preparing]>\n"
      "fn asBase(_ stage: WorkStage): Stage { return stage }\n"
      "fn asSuperset(_ stage: WorkStage): FullStage { return stage }\n"
      "fn call(_ stage: WorkStage): FullStage { return asSuperset(stage) }\n"
      "fn caseValue(): WorkStage { return .preparing }\n"
      "fn label(_ stage: WorkStage): String { return switch stage { "
      "case .preparing: \"P\" case .serving: \"S\" } }\n";
  fixture *value = &fixture_a;
  CHECK(fixture_run(value, source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  CHECK(value->result.written.enums == 1);
  CHECK(value->result.written.aliases == 2);
  CHECK(value->result.written.enum_subset_members > 0);

  const w_seed_frontend_type *full =
      &value->types[value->aliases[0].type_index];
  const w_seed_frontend_type *work =
      &value->types[value->aliases[1].type_index];
  CHECK(full->kind == W_SEED_FRONTEND_TYPE_ENUM);
  CHECK(full->enum_base_index == 0 &&
        full->first_subset_member == W_SEED_FRONTEND_NONE &&
        full->subset_member_count == 0);
  CHECK(work->kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET);
  CHECK(work->enum_base_index == 0 && work->subset_member_count == 2);
  CHECK(work->first_subset_member == 0);
  size_t stage_identifier_count = 0;
  size_t subset_case_count = 0;
  for (size_t expression_index = 0;
       expression_index < value->result.written.expressions;
       expression_index += 1) {
    const w_seed_frontend_expression *expression =
        &value->expressions[expression_index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        expression->spelling.length == 5 &&
        memcmp(expression->spelling.data, "stage", 5) == 0) {
      CHECK(expression->inferred_type != W_SEED_FRONTEND_NONE);
      CHECK(expression->inferred_type < value->result.written.types);
      CHECK(value->types[expression->inferred_type].kind ==
            W_SEED_FRONTEND_TYPE_ENUM_SUBSET);
      CHECK(value->types[expression->inferred_type].enum_base_index == 0);
      CHECK(expression->inferred_type == value->aliases[1].type_index);
      stage_identifier_count += 1;
    }
    if (expression->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE) {
      CHECK(expression->inferred_type != W_SEED_FRONTEND_NONE);
      CHECK(expression->inferred_type < value->result.written.types);
      CHECK(value->types[expression->inferred_type].kind ==
            W_SEED_FRONTEND_TYPE_ENUM_SUBSET);
      CHECK(expression->inferred_type == value->aliases[1].type_index);
      subset_case_count += 1;
    }
  }
  CHECK(stage_identifier_count >= 4);
  CHECK(subset_case_count >= 1);
  for (size_t type_index = 0; type_index < value->result.written.types;
       type_index += 1) {
    const w_seed_frontend_type *subset = &value->types[type_index];
    if (subset->subset_member_count == 0) continue;
    CHECK((size_t)subset->first_subset_member +
              subset->subset_member_count <=
          value->result.written.enum_subset_members);
    uint32_t previous_case = W_SEED_FRONTEND_NONE;
    for (uint32_t member_offset = 0; member_offset < subset->subset_member_count;
         member_offset += 1) {
      const w_seed_frontend_enum_subset_member *member =
          &value->enum_subset_members[subset->first_subset_member +
                                      member_offset];
      CHECK(member->owner_type == type_index);
      CHECK(member->enum_base_index == 0);
      CHECK(member->enum_case_index < 4);
      CHECK(previous_case == W_SEED_FRONTEND_NONE ||
            previous_case < member->enum_case_index);
      previous_case = member->enum_case_index;
    }
  }
  CHECK(receipt_contains(value, "enum-subset-member=2|enum=0|case=2",
                         strlen("enum-subset-member=2|enum=0|case=2")));

  w_seed_frontend_counts measured_counts;
  w_seed_frontend_result measured_result;
  CHECK(w_seed_frontend_measure(&value->input, &measured_counts,
                                &measured_result) == W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&measured_counts, &value->result.required));
  CHECK(measured_counts.enum_subset_members ==
        value->result.written.enum_subset_members);

  fixture *repeat = &fixture_b;
  CHECK(fixture_run(repeat, source));
  CHECK(repeat->result.status == value->result.status);
  CHECK(counts_equal(&repeat->result.written, &value->result.written));
  CHECK(repeat->result.receipt_bytes == value->result.receipt_bytes);
  CHECK(memcmp(repeat->receipt, value->receipt, value->result.receipt_bytes) ==
        0);

  CHECK(fixture_run(
      value,
      "enum Stage { accepted preparing }\n"
      "enum Other { accepted }\n"
      "alias WorkStage = Stage<[.preparing]>\n"
      "fn wrong(): WorkStage { return Other.accepted }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(value, "W-TYPE-0121"));
  CHECK(fixture_run(value,
                    "enum Generic<T> { value(T) }\n"
                    "alias Bad = Generic<[.value]>\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE));
  CHECK(fixture_run(value,
                    "enum E { a b }\n"
                    "alias Same = E<[.a]>\n"
                    "alias Same = E<[.b]>\n"
                    "fn ambiguous(): Same { return .a }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(value, W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL));
  for (size_t expression_index = 0;
       expression_index < value->result.written.expressions;
       expression_index += 1) {
    CHECK(value->expressions[expression_index].kind !=
          W_SEED_FRONTEND_EXPR_ENUM_CASE);
  }

  /* A case-set member array is an explicit capacity surface.  Exhausting it
   * must stop before emit and leave every caller-owned buffer at its sentinel. */
  static char many_source[8192];
  size_t many_length = 0;
  CHECK(append_many_source(many_source, sizeof(many_source), &many_length,
                           "enum Many { "));
  for (size_t index = 0; index < 66; index += 1)
    CHECK(append_many_piece(many_source, sizeof(many_source), &many_length,
                            index, "c", " "));
  CHECK(append_many_source(many_source, sizeof(many_source), &many_length,
                           "}\n"
                           "alias ManySubset = Many<["));
  for (size_t index = 0; index < 65; index += 1) {
    const char *separator = index + 1u < 65u ? ", " : "";
    CHECK(append_many_piece(many_source, sizeof(many_source), &many_length,
                            index, ".c", separator));
  }
  CHECK(append_many_source(many_source, sizeof(many_source), &many_length,
                           "]>\n"));
  fixture *capacity = &fixture_capacity;
  CHECK(fixture_parse(capacity, many_source));
  const uint8_t sentinel = 0xa5u;
  fixture_fill_output(capacity, sentinel);
  capacity->output.enum_subset_member_capacity = 0;
  capacity->output.enum_subset_members = NULL;
  (void)w_seed_frontend_run(&capacity->input, &capacity->output,
                            &capacity->result);
  CHECK(capacity->result.status == W_SEED_FRONTEND_CAPACITY);
  CHECK(capacity->result.required.enum_subset_members == 65);
  CHECK(fixture_output_is(capacity, sentinel, true));
  return true;
}

static bool test_enum_values_constructors_and_switches(void) {
  static const char values_source[] =
      "enum Stage { accepted reserving preparing serving completed cancelled }\n"
      "enum DomainError { invalidTransition(from: Stage, to: Stage) }\n"
      "fn acceptStage(_ value: Stage): Stage { return value }\n"
      "fn shortValue(): Stage { return .preparing }\n"
      "fn qualifiedValue(): Stage { return Stage.preparing }\n"
      "fn localCall(): Stage { return acceptStage(.preparing) }\n"
      "fn makeError(from: Stage, to: Stage): DomainError { "
      "return .invalidTransition(from: from, to: to) }\n";
  fixture *values = &fixture_a;
  CHECK(fixture_run(values, values_source));
  CHECK(values->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(values->result.status == W_SEED_FRONTEND_OK);
  CHECK(values->result.written.enums == 2);
  CHECK(values->result.written.enum_cases == 7);
  CHECK(values->result.written.enum_case_parameters == 2);
  CHECK(values->result.written.arguments == 3);
  CHECK(values->result.written.diagnostics == 0);
  CHECK(values->result.required.receipt_bytes == values->result.written.receipt_bytes);
  CHECK(values->enums[0].type_index != W_SEED_FRONTEND_NONE &&
        values->enums[1].type_index != W_SEED_FRONTEND_NONE);
  size_t enum_value_count = 0;
  size_t constructor_count = 0;
  for (size_t index = 0; index < values->result.written.expressions; index += 1) {
    const w_seed_frontend_expression *expression = &values->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE) {
      CHECK(expression->enum_index != W_SEED_FRONTEND_NONE);
      CHECK(expression->enum_case_index != W_SEED_FRONTEND_NONE);
      CHECK(expression->enum_index < 2);
      CHECK(expression->inferred_type ==
                values->enums[expression->enum_index].type_index ||
            expression->inferred_type == W_SEED_FRONTEND_NONE);
      enum_value_count += 1;
    }
    if (expression->kind == W_SEED_FRONTEND_EXPR_CALL &&
        expression->left != W_SEED_FRONTEND_NONE &&
        expression->left < values->result.written.expressions &&
        values->expressions[expression->left].kind ==
            W_SEED_FRONTEND_EXPR_ENUM_CASE) {
      CHECK(expression->argument_count == 2);
      CHECK(values->expressions[expression->left].enum_index == 1);
      CHECK(values->expressions[expression->left].enum_case_index == 6);
      CHECK(expression->inferred_type == values->enums[1].type_index ||
            expression->inferred_type == W_SEED_FRONTEND_NONE);
      constructor_count += 1;
    }
  }
  CHECK(enum_value_count == 4);
  CHECK(constructor_count == 1);
  CHECK(values->arguments[1].label.length == 4 &&
        memcmp(values->arguments[1].label.data, "from", 4) == 0);
  CHECK(values->arguments[2].label.length == 2 &&
        memcmp(values->arguments[2].label.data, "to", 2) == 0);
  CHECK(values->arguments[1].resolved_parameter_ordinal == 0u &&
        values->arguments[2].resolved_parameter_ordinal == 1u);
  CHECK(fixture_run(values,
                    "enum Stage { ready }\n"
                    "fn value(): Stage { return .ready }\n"));
  CHECK(values->result.status == W_SEED_FRONTEND_OK);

  CHECK(fixture_run(values,
                    "enum Stage { accepted reserving preparing }\n"
                    "enum DomainError { invalidTransition(from: Stage, to: Stage) }\n"
                    "fn f(from: Stage, to: Stage): DomainError { "
                    "return .invalidTransition(to: to, from: from) }\n"));
  CHECK(values->result.status == W_SEED_FRONTEND_OK &&
        values->result.written.arguments == 2u &&
        values->arguments[0].resolved_parameter_ordinal == 1u &&
        values->arguments[1].resolved_parameter_ordinal == 0u);
  CHECK(fixture_run(values,
                    "enum Stage { accepted reserving preparing }\n"
                    "enum DomainError { invalidTransition(from: Stage, to: Stage) }\n"
                    "fn f(from: Stage, to: Stage): DomainError { "
                    "return .invalidTransition(from: from, from: to) }\n"));
  CHECK(values->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(values, "W-LABEL-0006"));
  const w_seed_frontend_diagnostic *duplicate_label =
      diagnostic_for_code(values, "W-LABEL-0006");
  CHECK(duplicate_label != NULL && duplicate_label->fact_count == 3u &&
        duplicate_label->label_count == 0u &&
        diagnostic_record_ranges_are_valid(values, duplicate_label));
  CHECK(diagnostic_fact_string_is(values, duplicate_label, 0u, "declaration",
                                  "invalidTransition"));
  CHECK(diagnostic_fact_string_is(values, duplicate_label, 1u, "label", "from"));
  CHECK(diagnostic_fact_string_is(values, duplicate_label, 2u, "slot", "from"));
  CHECK(fixture_run(values,
                    "enum Stage { accepted reserving preparing }\n"
                    "enum DomainError { invalidTransition(from: Stage, to: Stage) }\n"
                    "fn f(from: Stage, to: Stage): DomainError { "
                    "return .invalidTransition(from: from) }\n"));
  CHECK(values->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(values, "W-LABEL-0005"));
  CHECK(fixture_run(values,
                    "enum Numeric { value(value: u8) }\n"
                    "fn f(): Numeric { return .value(value: 300_u16) }\n"));
  CHECK(values->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(values, "W-TYPE-0122"));
  CHECK(fixture_run(values,
                    "enum Stage { ready }\n"
                    "fn a(): Stage { return .ready() }\n"
                    "fn b(): Stage { return Stage.ready() }\n"));
  CHECK(values->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(values->result.written.diagnostics == 2);
  CHECK(has_diagnostic(values, "W-LABEL-0005"));

  fixture *short_case = &fixture_b;
  CHECK(fixture_run(short_case,
                    "enum Stage { accepted preparing }\n"
                    "fn f(): Stage { let value = .preparing return value }\n"));
  CHECK(short_case->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(short_case, "W-MATCH-0003"));
  const w_seed_frontend_diagnostic *short_member =
      diagnostic_for_code(short_case, "W-MATCH-0003");
  CHECK(short_member != NULL && short_member->fact_count == 3u &&
        short_member->label_count == 0u &&
        diagnostic_record_ranges_are_valid(short_case, short_member));
  CHECK(diagnostic_fact_string_is(short_case, short_member, 0u, "context",
                                  "short-enum"));
  CHECK(diagnostic_fact_string_is(short_case, short_member, 1u, "expectedType",
                                  "none"));
  CHECK(diagnostic_fact_string_is(short_case, short_member, 2u, "member",
                                  "preparing"));
  CHECK(fixture_run(short_case,
                    "enum Stage { accepted preparing }\n"
                    "fn f(): Stage { return .missing }\n"));
  CHECK(short_case->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(short_case, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));

  static const char switch_source[] =
      "enum Stage { accepted reserving preparing }\n"
      "fn label(stage: Stage): String { return switch stage { "
      "case .accepted: \"A\" case Stage.reserving: \"R\" case _: \"P\" } }\n";
  fixture *switch_value = &fixture_condition;
  CHECK(fixture_run(switch_value, switch_source));
  CHECK(switch_value->result.status == W_SEED_FRONTEND_OK);
  CHECK(switch_value->result.written.switch_arms == 3);
  CHECK(switch_value->result.required.switch_arms == 3);
  CHECK(switch_value->result.required.switch_arms ==
        switch_value->result.written.switch_arms);
  w_seed_frontend_counts switch_measured_counts;
  w_seed_frontend_result switch_measured_result;
  CHECK(w_seed_frontend_measure(&switch_value->input, &switch_measured_counts,
                                &switch_measured_result) == W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&switch_measured_counts, &switch_value->result.required));
  CHECK(switch_measured_result.required.receipt_bytes ==
        switch_value->result.required.receipt_bytes);
  CHECK(switch_value->expressions[1].kind == W_SEED_FRONTEND_EXPR_SWITCH);
  CHECK(switch_value->expressions[1].first_switch_arm == 0 &&
        switch_value->expressions[1].switch_arm_count == 3);
  CHECK(switch_value->switch_arms[0].owner_expression == 1 &&
        switch_value->switch_arms[0].pattern_kind ==
            W_SEED_FRONTEND_SWITCH_PATTERN_ENUM_CASE &&
        switch_value->switch_arms[0].enum_index == 0 &&
        switch_value->switch_arms[0].enum_case_index == 0 &&
        switch_value->switch_arms[0].supported);
  CHECK(switch_value->switch_arms[1].enum_index == 0 &&
        switch_value->switch_arms[1].enum_case_index == 1 &&
        switch_value->switch_arms[1].supported);
  CHECK(switch_value->switch_arms[2].pattern_kind ==
            W_SEED_FRONTEND_SWITCH_PATTERN_WILDCARD &&
        switch_value->switch_arms[2].enum_index == 0 &&
        switch_value->switch_arms[2].enum_case_index == W_SEED_FRONTEND_NONE &&
        switch_value->switch_arms[2].supported);
  CHECK(receipt_contains(
      switch_value, "switch-arm=0|owner=1|pattern=0|enum=0|case=0",
      strlen("switch-arm=0|owner=1|pattern=0|enum=0|case=0")));
  fixture *switch_repeat = &fixture_narrowing;
  CHECK(fixture_run(switch_repeat, switch_source));
  CHECK(switch_repeat->result.receipt_bytes == switch_value->result.receipt_bytes);
  CHECK(memcmp(switch_repeat->receipt, switch_value->receipt,
               switch_value->result.receipt_bytes) == 0);

  CHECK(fixture_run(
      switch_value,
      "enum Course { starter main(price: i64) dessert }\n"
      "fn price(course: Course): i64 { return switch course { "
      "case .starter: 10 case .main(price: let amount): amount "
      "case .dessert: 20 } }\n"));
  CHECK(switch_value->result.status == W_SEED_FRONTEND_OK);
  CHECK(switch_value->result.written.switch_arms == 3u &&
        switch_value->result.written.pattern_captures == 1u);
  CHECK(switch_value->switch_arms[0].capture_count == 0u &&
        switch_value->switch_arms[1].first_capture == 0u &&
        switch_value->switch_arms[1].capture_count == 1u &&
        switch_value->switch_arms[2].capture_count == 0u);
  CHECK(switch_value->pattern_captures[0].owner_switch_arm == 1u &&
        switch_value->pattern_captures[0].ordinal == 0u &&
        switch_value->pattern_captures[0].parameter_ordinal == 0u &&
        frontend_text_is(switch_value->pattern_captures[0].name, "amount"));
  bool saw_capture_read = false;
  for (size_t expression = 0u;
       expression < switch_value->result.written.expressions;
       expression += 1u) {
    if (switch_value->expressions[expression].kind ==
            W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        frontend_text_is(switch_value->expressions[expression].spelling,
                         "amount")) {
      CHECK(switch_value->expressions[expression].resolved_pattern_capture ==
            0u);
      saw_capture_read = true;
    }
  }
  CHECK(saw_capture_read);
  CHECK(receipt_contains(switch_value, "pattern-capture=0|owner=1|ordinal=0",
                         strlen("pattern-capture=0|owner=1|ordinal=0")));
  CHECK(fixture_run(
      switch_repeat,
      "enum Course { starter main(price: i64) dessert }\n"
      "fn price(course: Course): i64 { return switch course { "
      "case .starter: 10 case .main(price: let amount): amount "
      "case .dessert: 20 } }\n"));
  CHECK(switch_repeat->result.status == W_SEED_FRONTEND_OK &&
        switch_repeat->result.receipt_bytes ==
            switch_value->result.receipt_bytes &&
        memcmp(switch_repeat->receipt, switch_value->receipt,
               switch_value->result.receipt_bytes) == 0);

  CHECK(fixture_run(
      switch_value,
      "enum Course { starter main(price: i64) dessert }\n"
      "fn price(course: Course): i64 { return switch course { "
      "case .starter: 10 case .main(other: let amount): amount "
      "case .dessert: 20 } }\n"));
  CHECK(switch_value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(switch_value,
                 W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));

  CHECK(fixture_run(switch_value,
                    "enum Stage { accepted reserving preparing }\n"
                    "fn label(stage: Stage): String { return switch stage { "
                    "case .accepted: \"A\" case .accepted: \"A2\" "
                    "case .reserving: \"R\" case .preparing: \"P\" } }\n"));
  CHECK(switch_value->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(switch_value, "W-MATCH-0002"));
  const w_seed_frontend_diagnostic *duplicate_pattern =
      diagnostic_for_code(switch_value, "W-MATCH-0002");
  CHECK(duplicate_pattern != NULL && duplicate_pattern->fact_count == 3u &&
        duplicate_pattern->label_count == 2u &&
        diagnostic_record_ranges_are_valid(switch_value, duplicate_pattern));
  CHECK(diagnostic_fact_string_is(switch_value, duplicate_pattern, 0u,
                                  "coveredBy", "accepted"));
  CHECK(diagnostic_fact_string_is(switch_value, duplicate_pattern, 1u,
                                  "pattern", ".accepted"));
  CHECK(diagnostic_fact_string_is(switch_value, duplicate_pattern, 2u,
                                  "subjectType", "Stage"));
  CHECK(diagnostic_label_role_is(switch_value, duplicate_pattern, 0u,
                                 "covered-case"));
  CHECK(diagnostic_label_role_is(switch_value, duplicate_pattern, 1u,
                                 "match-subject"));
  CHECK(fixture_run(switch_value,
                    "enum Stage { accepted reserving preparing }\n"
                    "fn label(stage: Stage): String { return switch stage { "
                    "case _: \"all\" case .accepted: \"A\" } }\n"));
  CHECK(switch_value->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(switch_value, "W-MATCH-0002"));
  CHECK(fixture_run(switch_value,
                    "enum Stage { accepted reserving preparing }\n"
                    "fn label(stage: Stage): String { return switch stage { "
                    "case .accepted: \"A\" case .reserving: 1 "
                    "case .preparing: \"P\" } }\n"));
  CHECK(switch_value->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(switch_value, "W-TYPE-0120"));

  CHECK(fixture_run(switch_value,
                    "enum Stage { accepted reserving preparing }\n"
                    "fn narrow(stage: Stage): u8 { return switch stage { "
                    "case .accepted: 1_u16 case .reserving: 2_u16 "
                    "case .preparing: 3_u16 } }\n"));
  CHECK(switch_value->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(switch_value, "W-TYPE-0122"));
  CHECK(!has_diagnostic(switch_value, "W-TYPE-0120"));
  CHECK(fixture_run(switch_value,
                    "enum Stage { accepted reserving preparing }\n"
                    "fn widen(stage: Stage): u16 { return switch stage { "
                    "case .accepted: 1_u8 case .reserving: 2_u8 "
                    "case .preparing: 3_u8 } }\n"));
  CHECK(switch_value->result.status == W_SEED_FRONTEND_OK);
  CHECK(fixture_run(switch_value,
                    "enum Stage { accepted reserving preparing }\n"
                    "fn postfix(stage: Stage): String { return switch stage { "
                    "case .accepted: \"A\" case .reserving: \"R\" "
                    "case .preparing: \"P\" }.length }\n"
                    "fn binary(stage: Stage): String { return switch stage { "
                    "case .accepted: \"A\" case .reserving: \"R\" "
                    "case .preparing: \"P\" } + \"x\" }\n"));
  CHECK(switch_value->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(switch_value->result.written.switch_arms == 0);
  CHECK(has_fact(switch_value,
                 W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));

  size_t many_length = 0;
  CHECK(append_many_source(long_source, sizeof(long_source), &many_length,
                           "enum Many { "));
  for (size_t index = 0; index < 70; index += 1) {
    CHECK(append_many_piece(long_source, sizeof(long_source), &many_length,
                            index, "case", index + 1u < 70u ? " " : ""));
  }
  CHECK(append_many_source(long_source, sizeof(long_source), &many_length,
                           " }\nfn all(value: Many): String { return switch value { "));
  for (size_t index = 0; index < 70; index += 1) {
    CHECK(append_many_piece(long_source, sizeof(long_source), &many_length,
                            index, "case .case", ": \"x\" "));
  }
  CHECK(append_many_source(long_source, sizeof(long_source), &many_length,
                           "} }\n"));
  fixture *many = &fixture_callback;
  CHECK(fixture_run(many, long_source));
  CHECK(many->result.status == W_SEED_FRONTEND_OK);
  CHECK(many->result.written.enum_cases == 70 &&
        many->result.written.switch_arms == 70);
  CHECK(many->result.written.diagnostics == 0);

  size_t missing_length = 0;
  CHECK(append_many_source(long_source, sizeof(long_source), &missing_length,
                           "enum Many { "));
  for (size_t index = 0; index < 70; index += 1) {
    CHECK(append_many_piece(long_source, sizeof(long_source), &missing_length,
                            index, "case", index + 1u < 70u ? " " : ""));
  }
  CHECK(append_many_source(long_source, sizeof(long_source), &missing_length,
                           " }\nfn missing(value: Many): String { return switch value { "));
  for (size_t index = 0; index < 69; index += 1) {
    CHECK(append_many_piece(long_source, sizeof(long_source), &missing_length,
                            index, "case .case", ": \"x\" "));
  }
  CHECK(append_many_source(long_source, sizeof(long_source), &missing_length,
                           "} }\n"));
  CHECK(fixture_run(many, long_source));
  CHECK(many->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(many, "W-MATCH-0001"));
  CHECK(many->result.written.diagnostics == 1u);
  const w_seed_frontend_diagnostic *missing_many =
      diagnostic_for_code(many, "W-MATCH-0001");
  static const char *const missing_cases[] = {"case69"};
  CHECK(missing_many != NULL && missing_many->fact_count == 2u &&
        missing_many->label_count == 1u &&
        diagnostic_record_ranges_are_valid(many, missing_many));
  CHECK(diagnostic_fact_set_is(many, missing_many, 0u, "missingCases",
                               missing_cases, 1u));
  CHECK(diagnostic_fact_string_is(many, missing_many, 1u, "subjectType",
                                  "Many"));
  CHECK(diagnostic_label_role_is(many, missing_many, 0u, "match-subject"));

  CHECK(fixture_run(switch_value,
                    "enum Stage { accepted reserving preparing }\n"
                    "fn missing(stage: Stage): String { return switch stage { "
                    "case .preparing: \"P\" } }\n"));
  CHECK(switch_value->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(switch_value->result.written.diagnostics == 1u);
  const w_seed_frontend_diagnostic *aggregated_missing =
      diagnostic_for_code(switch_value, "W-MATCH-0001");
  static const char *const sorted_missing[] = {"accepted", "reserving"};
  CHECK(aggregated_missing != NULL && aggregated_missing->fact_count == 2u &&
        aggregated_missing->label_count == 1u &&
        diagnostic_record_ranges_are_valid(switch_value, aggregated_missing));
  CHECK(diagnostic_fact_set_is(switch_value, aggregated_missing, 0u,
                               "missingCases", sorted_missing, 2u));
  CHECK(diagnostic_fact_string_is(switch_value, aggregated_missing, 1u,
                                  "subjectType", "Stage"));
  CHECK(diagnostic_label_role_is(switch_value, aggregated_missing, 0u,
                                 "match-subject"));
  return true;
}

static bool test_const_and_membership(void) {
  static const char source[] =
      "enum Stage { accepted reserving preparing serving }\n"
      "alias WorkStage = Stage<[.preparing, .serving]>\n"
      "const fn isWork(_ stage: Stage): Bool { return stage in "
      "(Stage.serving, .preparing) }\n"
      "const fn subset(_ stage: WorkStage): Bool { return stage in "
      "(.accepted, .preparing) }\n"
      "const fn calls(_ stage: Stage): Bool { return isWork(stage) }\n"
      "fn ordinary(_ stage: Stage): Bool { return stage in (.accepted) }\n";
  fixture *value = &fixture_const;
  CHECK(fixture_run(value, source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  CHECK(value->result.written.functions == 4);
  CHECK(value->result.written.enum_membership_cases == 5);
  CHECK(value->functions[0].is_const && value->functions[0].const_body_supported);
  CHECK(value->functions[1].is_const && value->functions[1].const_body_supported);
  CHECK(value->functions[2].is_const && value->functions[2].const_body_supported);
  CHECK(!value->functions[3].is_const && !value->functions[3].const_body_supported);

  size_t membership_count = 0;
  for (size_t index = 0; index < value->result.written.expressions; index += 1) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_ENUM_MEMBERSHIP) continue;
    CHECK(expression->supported);
    CHECK(expression->inferred_type != W_SEED_FRONTEND_NONE);
    CHECK(value->types[expression->inferred_type].kind ==
          W_SEED_FRONTEND_TYPE_BOOL);
    CHECK(expression->first_membership_case != W_SEED_FRONTEND_NONE);
    CHECK(expression->membership_case_count != 0);
    CHECK((size_t)expression->first_membership_case +
              expression->membership_case_count <=
          value->result.written.enum_membership_cases);
    uint32_t previous_case = W_SEED_FRONTEND_NONE;
    for (uint32_t offset = 0; offset < expression->membership_case_count;
         offset += 1) {
      const w_seed_frontend_enum_membership_case *record =
          &value->enum_membership_cases[expression->first_membership_case +
                                        offset];
      CHECK(record->owner_expression == index);
      CHECK(record->enum_base_index == 0);
      CHECK(previous_case == W_SEED_FRONTEND_NONE ||
            previous_case < record->enum_case_index);
      previous_case = record->enum_case_index;
    }
    membership_count += 1;
  }
  CHECK(membership_count == 3);
  CHECK(receipt_contains(value, "enum-membership-case=0|owner=1|enum=0|case=2",
                         strlen("enum-membership-case=0|owner=1|enum=0|case=2")));
  w_seed_frontend_counts measured_counts;
  w_seed_frontend_result measured_result;
  CHECK(w_seed_frontend_measure(&value->input, &measured_counts,
                                &measured_result) == W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&measured_counts, &value->result.required));
  CHECK(measured_counts.enum_membership_cases ==
        value->result.written.enum_membership_cases);
  fixture *repeat = &fixture_b;
  CHECK(fixture_run(repeat, source));
  CHECK(repeat->result.status == value->result.status);
  CHECK(repeat->result.receipt_bytes == value->result.receipt_bytes);
  CHECK(memcmp(repeat->receipt, value->receipt, value->result.receipt_bytes) ==
        0);

  /* A subset may list a base case outside its subset.  Membership remains a
   * Bool expression and does not reject that case at normalization time. */
  CHECK(fixture_run(value,
                    "enum Stage { accepted preparing serving }\n"
                    "alias Work = Stage<[.preparing, .serving]>\n"
                    "const fn f(stage: Work): Bool { return stage in "
                    "(.accepted, .preparing) }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  CHECK(value->result.written.enum_membership_cases == 2);

  static const char *const invalid_sources[] = {
      "enum Stage { accepted preparing }\n"
      "fn f(stage: Stage): Bool { return stage in () }\n",
      "enum Stage { accepted preparing }\n"
      "fn f(stage: Stage): Bool { return stage in (.accepted, .accepted) }\n",
      "enum Stage { accepted preparing }\n"
      "fn f(stage: Stage): Bool { return stage in (.missing) }\n",
      "enum Stage { accepted preparing }\n"
      "fn f(stage: Stage): Bool { return stage in (.accepted .preparing) }\n",
      "enum Stage { accepted preparing }\n"
      "enum Other { nope }\n"
      "fn f(stage: Stage): Bool { return stage in (Other.nope) }\n",
  };
  for (size_t index = 0;
       index < sizeof(invalid_sources) / sizeof(invalid_sources[0]); index += 1) {
    CHECK(fixture_run(value, invalid_sources[index]));
    CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED);
    CHECK(value->result.written.enum_membership_cases == 0);
    CHECK(has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
    CHECK(!has_diagnostic(value, "W-MATCH-0001"));
    CHECK(!has_diagnostic(value, "W-MATCH-0002"));
  }

  /* The bounded list has no 64-case shortcut. */
  static char many_source[8192];
  size_t many_length = 0;
  CHECK(append_many_source(many_source, sizeof(many_source), &many_length,
                           "enum Many { "));
  for (size_t index = 0; index < 70; index += 1)
    CHECK(append_many_piece(many_source, sizeof(many_source), &many_length,
                            index, "case", " "));
  CHECK(append_many_source(many_source, sizeof(many_source), &many_length,
                           "}\nfn f(value: Many): Bool { return value in ("));
  for (size_t index = 0; index < 70; index += 1)
    CHECK(append_many_piece(many_source, sizeof(many_source), &many_length,
                            index, ".case", index + 1u < 70u ? ", " : ""));
  CHECK(append_many_source(many_source, sizeof(many_source), &many_length,
                           ") }\n"));
  CHECK(fixture_run(value, many_source));
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  CHECK(value->result.written.enum_membership_cases == 70);
  CHECK(value->expressions[1].kind == W_SEED_FRONTEND_EXPR_ENUM_MEMBERSHIP);
  CHECK(value->expressions[1].membership_case_count == 70);
  const uint8_t sentinel = 0xa5u;
  CHECK(fixture_parse(&fixture_capacity, many_source));
  fixture_fill_output(&fixture_capacity, sentinel);
  fixture_capacity.output.enum_membership_case_capacity = 0;
  fixture_capacity.output.enum_membership_cases = NULL;
  (void)w_seed_frontend_run(&fixture_capacity.input, &fixture_capacity.output,
                            &fixture_capacity.result);
  CHECK(fixture_capacity.result.status == W_SEED_FRONTEND_CAPACITY);
  CHECK(fixture_capacity.result.required.enum_membership_cases == 70);
  CHECK(fixture_output_is(&fixture_capacity, sentinel, true));

  CHECK(fixture_run(value,
                    "fn normal(): Bool { return true }\n"
                    "const fn bad(): Bool { return normal() }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(value->result.written.diagnostics == 1);
  CHECK(has_diagnostic(value, "W-CONST-0001"));
  CHECK(value->functions[1].is_const && !value->functions[1].const_body_supported);
  CHECK(value->diagnostics[0].primary.start_byte <
        value->diagnostics[0].primary.end_byte);
  const w_seed_frontend_diagnostic *const_call =
      diagnostic_for_code(value, "W-CONST-0001");
  static const char *const const_call_chain[] = {"bad", "normal"};
  CHECK(const_call != NULL && const_call->fact_count == 4u &&
        const_call->label_count == 1u &&
        diagnostic_record_ranges_are_valid(value, const_call));
  CHECK(diagnostic_fact_array_is(value, const_call, 0u, "callChain",
                                 const_call_chain, 2u));
  CHECK(diagnostic_fact_string_is(value, const_call, 1u, "operation",
                                  "call"));
  CHECK(diagnostic_fact_string_is(value, const_call, 2u, "reason",
                                  "not const-safe"));
  CHECK(diagnostic_fact_string_is(value, const_call, 3u, "symbol",
                                  "normal"));
  CHECK(diagnostic_label_role_is(value, const_call, 0u, "const-owner"));
  CHECK(value->diagnostic_labels[const_call->first_label].document_index == 0u &&
        value->diagnostic_labels[const_call->first_label].span.start_byte ==
            value->functions[1].span.start_byte &&
        value->diagnostic_labels[const_call->first_label].span.end_byte ==
            value->functions[1].span.end_byte);

  CHECK(fixture_run(value,
                    "const fn bad(): Bool { return true.foo }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(value->result.written.diagnostics == 1);
  CHECK(has_diagnostic(value, "W-CONST-0001"));
  CHECK(has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  CHECK(!value->functions[0].const_body_supported);

  CHECK(fixture_parse(&fixture_external,
                      "import { ext } from extdep\n"
                      "const fn f(): Bool { return ext() }\n"));
  fixture_external.external_symbols[0] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"ext", 3},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0,
      .return_type = (w_seed_frontend_text){"Bool", 4},
      .is_const = false,
  };
  fixture_external.external_modules[0] = (w_seed_frontend_external_module){
      .module_id = (w_seed_frontend_text){"extdep", 6},
      .symbols = fixture_external.external_symbols,
      .symbol_count = 1,
  };
  fixture_external.input.external_modules = fixture_external.external_modules;
  fixture_external.input.external_module_count = 1;
  CHECK(fixture_resolve_external_imports(&fixture_external));
  (void)w_seed_frontend_run(&fixture_external.input, &fixture_external.output,
                            &fixture_external.result);
  CHECK(fixture_external.result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(fixture_external.result.written.diagnostics == 1);
  CHECK(has_diagnostic(&fixture_external, "W-CONST-0001"));
  fixture_external.external_symbols[0].is_const = true;
  (void)w_seed_frontend_run(&fixture_external.input, &fixture_external.output,
                            &fixture_external.result);
  CHECK(fixture_external.result.status == W_SEED_FRONTEND_OK);
  CHECK(fixture_external.functions[0].is_const &&
        fixture_external.functions[0].const_body_supported);
  return true;
}

static bool test_host_scope_and_callee_identity(void) {
  fixture *value = &fixture_host;
  static const char source[] =
      "fn main(): () { print(\"Hello, world!\") }\n"
      "entry(main)\n";
  CHECK(fixture_parse(value, source));
  value->host_requirements[0] = (w_seed_frontend_host_requirement){
      .name = (w_seed_frontend_text){"Console", 7u}};
  value->host_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  value->host_symbols[0] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"print", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = value->host_parameters,
      .parameter_count = 1u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = value->host_requirements,
      .requirement_count = 1u};
  value->host_scope = (w_seed_frontend_host_prelude){
      .profile = (w_seed_frontend_text){"native-process@1", 16u},
      .symbols = value->host_symbols,
      .symbol_count = 1u};
  value->input.host_scope = &value->host_scope;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  CHECK(value->result.written.functions == 1u &&
        value->functions[0].name.length == 4u &&
        !value->functions[0].is_async && !value->functions[0].is_throws &&
        !value->functions[0].is_unsafe &&
        !value->functions[0].has_borrow_clause);
  uint32_t call_index = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    if (value->expressions[index].kind == W_SEED_FRONTEND_EXPR_CALL) {
      CHECK(call_index == W_SEED_FRONTEND_NONE);
      call_index = (uint32_t)index;
    }
  }
  CHECK(call_index != W_SEED_FRONTEND_NONE);
  const w_seed_frontend_expression *call = &value->expressions[call_index];
  CHECK(call->supported && call->left != W_SEED_FRONTEND_NONE &&
        call->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL &&
        call->resolved_host_symbol_index == 0u &&
        call->resolved_external_module_index == W_SEED_FRONTEND_NONE &&
        call->resolved_external_symbol_index == W_SEED_FRONTEND_NONE &&
        call->resolved_function_index == W_SEED_FRONTEND_NONE);
  CHECK((size_t)call->left < value->result.written.expressions);
  const w_seed_frontend_expression *callee = &value->expressions[call->left];
  CHECK(callee->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        callee->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL &&
        callee->resolved_host_symbol_index == 0u &&
        callee->resolved_external_module_index == W_SEED_FRONTEND_NONE &&
        callee->resolved_external_symbol_index == W_SEED_FRONTEND_NONE);
  CHECK(call->argument_count == 1u && call->first_argument == 0u);
  CHECK(value->arguments[0].resolved_parameter_ordinal == 0u);
  CHECK((size_t)value->arguments[0].expression_index <
        value->result.written.expressions);
  const w_seed_frontend_expression *literal =
      &value->expressions[value->arguments[0].expression_index];
  CHECK(literal->kind == W_SEED_FRONTEND_EXPR_STRING && literal->supported &&
        literal->const_byte_offset != W_SEED_FRONTEND_NONE &&
        literal->const_byte_count == 13u &&
        memcmp(value->const_bytes + literal->const_byte_offset,
               "Hello, world!", 13u) == 0);
  CHECK(receipt_contains(value, "host-scope=16:6e61746976652d70726f636573734031",
                         strlen("host-scope=16:6e61746976652d70726f636573734031")));
  CHECK(receipt_contains(value, "host-requirement=0|0|7:436f6e736f6c65",
                         strlen("host-requirement=0|0|7:436f6e736f6c65")));
  CHECK(receipt_contains(
      value,
      "|async=0|throws=0|error-type=4294967295|unsafe=0|borrows=0|anonymous=0\n",
      strlen("|async=0|throws=0|error-type=4294967295|unsafe=0|borrows=0|anonymous=0\n")));
  w_seed_frontend_counts measured;
  w_seed_frontend_result measured_result;
  CHECK(w_seed_frontend_measure(&value->input, &measured, &measured_result) ==
        W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&measured, &value->result.required));
  const size_t receipt_bytes = value->result.receipt_bytes;
  uint8_t receipt_copy[TEST_RECEIPT];
  CHECK(receipt_bytes <= sizeof(receipt_copy));
  (void)memcpy(receipt_copy, value->receipt, receipt_bytes);
  (void)w_seed_frontend_run(&value->input, &value->output, &value->result);
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.receipt_bytes == receipt_bytes &&
        memcmp(value->receipt, receipt_copy, receipt_bytes) == 0);

  value->input.host_scope = NULL;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_UNSUPPORTED);
  value->input.host_scope = &value->host_scope;

  fixture *external = &fixture_external;
  CHECK(fixture_parse(external,
                      "import { print } from extdep\n"
                      "fn main(): () { print(\"Hello, world!\") }\n"
                      "entry(main)\n"));
  external->external_symbols[0] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"print", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = external->external_parameters,
      .parameter_count = 1u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false};
  external->external_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  external->external_modules[0] = (w_seed_frontend_external_module){
      .module_id = (w_seed_frontend_text){"extdep", 6u},
      .symbols = external->external_symbols,
      .symbol_count = 1u};
  external->input.external_modules = external->external_modules;
  external->input.external_module_count = 1u;
  external->host_requirements[0] = value->host_requirements[0];
  external->host_parameters[0] = value->host_parameters[0];
  external->host_symbols[0] = value->host_symbols[0];
  external->host_symbols[0].parameters = external->host_parameters;
  external->host_symbols[0].requirements = external->host_requirements;
  external->host_scope = value->host_scope;
  external->host_scope.symbols = external->host_symbols;
  external->input.host_scope = &external->host_scope;
  CHECK(fixture_resolve_external_imports(external));
  CHECK(w_seed_frontend_run(&external->input, &external->output,
                            &external->result) == W_SEED_FRONTEND_OK);
  call_index = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < external->result.written.expressions;
       index += 1u) {
    if (external->expressions[index].kind == W_SEED_FRONTEND_EXPR_CALL)
      call_index = (uint32_t)index;
  }
  CHECK(call_index != W_SEED_FRONTEND_NONE);
  call = &external->expressions[call_index];
  CHECK(call->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL &&
        call->resolved_external_module_index == 0u &&
        call->resolved_external_symbol_index == 0u &&
        call->resolved_host_symbol_index == W_SEED_FRONTEND_NONE);
  CHECK((size_t)call->left < external->result.written.expressions);
  callee = &external->expressions[call->left];
  CHECK(callee->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL &&
        callee->resolved_external_module_index == 0u &&
        callee->resolved_external_symbol_index == 0u);

  CHECK(fixture_parse(value,
                      "fn print(_ message: String): () {}\n"
                      "fn main(): () { print(\"Hello, world!\") }\n"
                      "entry(main)\n"));
  value->input.host_scope = &value->host_scope;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  call_index = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    if (value->expressions[index].kind == W_SEED_FRONTEND_EXPR_CALL)
      call_index = (uint32_t)index;
  }
  CHECK(call_index != W_SEED_FRONTEND_NONE);
  CHECK(value->expressions[call_index].resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION &&
        value->expressions[call_index].resolved_function_index == 0u);
  return true;
}

static bool test_external_nominal_member_resolution(void) {
  fixture *value = &fixture_external;
  static const char positive[] =
      "import std.process\n"
      "fn run(args: Arguments): Bool { return args.contains(\"--closed\") }\n"
      "entry(run)\n";
  CHECK(fixture_parse(value, positive));
  fixture_configure_arguments_external(value);
  CHECK(fixture_resolve_external_imports(value));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.written.imports == 1u &&
        value->result.written.functions == 1u &&
        value->result.written.parameters == 1u &&
        value->result.written.arguments == 1u);

  uint32_t receiver_index = W_SEED_FRONTEND_NONE;
  uint32_t member_index = W_SEED_FRONTEND_NONE;
  uint32_t call_index = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression =
        &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_MEMBER) {
      CHECK(member_index == W_SEED_FRONTEND_NONE);
      member_index = (uint32_t)index;
      receiver_index = expression->left;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_CALL) {
      CHECK(call_index == W_SEED_FRONTEND_NONE);
      call_index = (uint32_t)index;
    }
  }
  CHECK(receiver_index != W_SEED_FRONTEND_NONE &&
        member_index != W_SEED_FRONTEND_NONE &&
        call_index != W_SEED_FRONTEND_NONE);
  CHECK((size_t)receiver_index < value->result.written.expressions &&
        (size_t)member_index < value->result.written.expressions &&
        (size_t)call_index < value->result.written.expressions);
  const w_seed_frontend_expression *receiver =
      &value->expressions[receiver_index];
  const w_seed_frontend_expression *member = &value->expressions[member_index];
  const w_seed_frontend_expression *call = &value->expressions[call_index];
  CHECK(receiver->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        receiver->supported && receiver->resolved_parameter_ordinal == 0u &&
        receiver->resolved_binding_statement == W_SEED_FRONTEND_NONE &&
        receiver->inferred_type != W_SEED_FRONTEND_NONE);
  CHECK(value->types[receiver->inferred_type].kind ==
            W_SEED_FRONTEND_TYPE_NOMINAL &&
        frontend_text_is(value->types[receiver->inferred_type].nominal_name,
                          "Arguments"));
  CHECK(member->left == receiver_index && member->supported &&
        frontend_text_is(member->member_name, "contains") &&
        member->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL &&
        member->resolved_external_module_index == 0u &&
        member->resolved_external_symbol_index == 1u);
  CHECK(call->left == member_index && call->supported &&
        call->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL &&
        call->resolved_external_module_index == 0u &&
        call->resolved_external_symbol_index == 1u &&
        call->first_argument == 0u && call->argument_count == 1u);
  CHECK(value->arguments[0].owner_expression == member_index &&
        value->arguments[0].resolved_parameter_ordinal == 0u &&
        value->arguments[0].expression_index != W_SEED_FRONTEND_NONE);
  CHECK(receipt_contains(value, "|receiver=0:",
                         strlen("|receiver=0:")));

  w_seed_frontend_counts measured;
  w_seed_frontend_result measured_result;
  CHECK(w_seed_frontend_measure(&value->input, &measured, &measured_result) ==
        W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&measured, &value->result.required) &&
        measured_result.required.receipt_bytes == value->result.receipt_bytes);
  const size_t receipt_bytes = value->result.receipt_bytes;
  uint8_t receipt_copy[TEST_RECEIPT];
  CHECK(receipt_bytes <= sizeof(receipt_copy));
  (void)memcpy(receipt_copy, value->receipt, receipt_bytes);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.receipt_bytes == receipt_bytes &&
        memcmp(value->receipt, receipt_copy, receipt_bytes) == 0);

  /* A type/member must not become visible without the direct import. */
  CHECK(fixture_parse(
      value,
      "fn run(args: Arguments): Bool { return args.contains(\"--closed\") }\n"
      "entry(run)\n"));
  fixture_configure_arguments_external(value);
  CHECK(fixture_resolve_external_imports(value));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));

  /* The member identity is not a free function in a bare import. */
  CHECK(fixture_parse(
      value,
      "import std.process\n"
      "fn run(args: Arguments): Bool { return contains(\"--closed\") }\n"
      "entry(run)\n"));
  fixture_configure_arguments_external(value);
  CHECK(fixture_resolve_external_imports(value));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_UNSUPPORTED);

  CHECK(fixture_parse(
      value,
      "import std.process\n"
      "fn run(text: String): Bool { return text.contains(\"--closed\") }\n"
      "entry(run)\n"));
  fixture_configure_arguments_external(value);
  CHECK(fixture_resolve_external_imports(value));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_UNSUPPORTED);

  CHECK(fixture_parse(
      value,
      "import std.process\n"
      "fn run(args: Arguments): Bool { return args.unknown(\"--closed\") }\n"
      "entry(run)\n"));
  fixture_configure_arguments_external(value);
  CHECK(fixture_resolve_external_imports(value));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_UNSUPPORTED);

  static const char *const bad_calls[] = {
      "args.contains(true)",
      "args.contains()",
      "args.contains(flag: \"--closed\")",
  };
  for (size_t case_index = 0u;
       case_index < sizeof(bad_calls) / sizeof(bad_calls[0]);
       case_index += 1u) {
    char source[256];
    const int written = snprintf(
        source, sizeof(source),
        "import std.process\n"
        "fn run(args: Arguments): Bool { return %s }\n"
        "entry(run)\n",
        bad_calls[case_index]);
    CHECK(written > 0 && (size_t)written < sizeof(source));
    CHECK(fixture_parse(value, source));
    fixture_configure_arguments_external(value);
    CHECK(fixture_resolve_external_imports(value));
    const w_seed_frontend_status bad_call_status =
        w_seed_frontend_run(&value->input, &value->output, &value->result);
    CHECK(bad_call_status ==
          (case_index == 2u ? W_SEED_FRONTEND_DIAGNOSTICS
                            : W_SEED_FRONTEND_UNSUPPORTED));
  }

  /* A forged owner spelling is invalid ABI, while malformed text is rejected
   * before the caller-owned output can be touched. */
  CHECK(fixture_parse(
      value,
      "import std.process\n"
      "fn run(args: Arguments): Bool { return args.contains(\"--closed\") }\n"
      "entry(run)\n"));
  fixture_configure_arguments_external(value);
  fixture_fill_output(value, 0xa5u);
  value->external_symbols[1].receiver_type =
      (w_seed_frontend_text){"Other", 5u};
  CHECK(fixture_resolve_external_imports(value));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_INVALID);
  CHECK(fixture_output_is(value, 0xa5u, true));
  value->external_symbols[1].receiver_type =
      (w_seed_frontend_text){"Arguments", 9u};
  fixture_fill_output(value, 0xa5u);
  value->external_symbols[1].receiver_type =
      (w_seed_frontend_text){NULL, 1u};
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_INVALID);
  CHECK(fixture_output_is(value, 0xa5u, true));
  return true;
}

static bool test_module_named_consts(void) {
  static const char named_source[] =
      "export const ultimateAnswer: i64 = 6 * 7\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { let named: Box<(ultimateAnswer)> }\n";
  fixture *value = &fixture_const;
  CHECK(fixture_run(value, named_source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.const_declarations == 1u &&
        value->modules[0].first_const_declaration == 0u &&
        value->modules[0].const_declaration_count == 1u);
  const w_seed_frontend_const_declaration *declaration =
      &value->const_declarations[0];
  CHECK(declaration->module_index == 0u && declaration->exported &&
        declaration->name.length == 14u &&
        memcmp(declaration->name.data, "ultimateAnswer", 14u) == 0 &&
        declaration->span.start_byte == 0u && declaration->body_span.start_byte >
            declaration->span.start_byte &&
        declaration->initializer_expression != W_SEED_FRONTEND_NONE &&
        declaration->has_explicit_type && declaration->lowerable &&
        declaration->effective_type == declaration->declared_type &&
        declaration->symbol_index != W_SEED_FRONTEND_NONE);
  CHECK(value->types[declaration->declared_type].kind ==
        W_SEED_FRONTEND_TYPE_INTEGER &&
        value->types[declaration->declared_type].bit_width == 64u);
  bool saw_named_relation = false;
  for (size_t index = 0u; index < value->result.written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        expression->resolved_const_declaration == 0u) {
      CHECK(expression->supported);
      saw_named_relation = true;
    }
  }
  CHECK(saw_named_relation);
  CHECK(receipt_contains(value, "const-declaration=0|", 19u));
  bool saw_const_symbol = false;
  for (size_t index = 0u; index < value->result.written.symbols; index += 1u) {
    const w_seed_frontend_symbol *symbol = &value->symbols[index];
    if (symbol->kind == W_SEED_FRONTEND_SYMBOL_CONST) {
      CHECK(symbol->owner_index == 0u && symbol->exported);
      saw_const_symbol = true;
    }
  }
  CHECK(saw_const_symbol);

  CHECK(fixture_run(value,
                    "const duplicate: i64 = 42\n"
                    "const duplicate: i64 = 42\n"
                    "struct Box<_ value: i64> {}\n"
                    "struct Use { let value: Box<(duplicate)> }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL));

  CHECK(fixture_run(value,
                    "const answer = 42\n"
                    "struct Box<_ value: i64> {}\n"
                    "struct Use { let value: Box<(answer)> }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  CHECK(value->const_declarations[0].declared_type ==
        W_SEED_FRONTEND_NONE &&
        value->const_declarations[0].effective_type != W_SEED_FRONTEND_NONE &&
        value->types[value->const_declarations[0].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        value->types[value->const_declarations[0].effective_type].bit_width ==
            64u);

  CHECK(fixture_run(value,
                    "const boolLiteral = true\n"
                    "const boolExpression = boolLiteral == true\n"
                    "const fixed = 7_u16\n"
                    "const propagated = fixed\n"
                    "const forward = target + 1\n"
                    "const target = 1_u32\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.const_declarations == 6u);
  CHECK(!value->const_declarations[0].has_explicit_type &&
        value->const_declarations[0].declared_type == W_SEED_FRONTEND_NONE &&
        value->types[value->const_declarations[0].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_BOOL);
  CHECK(!value->const_declarations[1].has_explicit_type &&
        value->const_declarations[1].declared_type == W_SEED_FRONTEND_NONE &&
        value->types[value->const_declarations[1].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_BOOL);
  CHECK(!value->const_declarations[2].has_explicit_type &&
        value->const_declarations[2].declared_type == W_SEED_FRONTEND_NONE &&
        value->types[value->const_declarations[2].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        !value->types[value->const_declarations[2].effective_type].is_signed &&
        value->types[value->const_declarations[2].effective_type].bit_width ==
            16u);
  CHECK(!value->const_declarations[3].has_explicit_type &&
        value->const_declarations[3].declared_type == W_SEED_FRONTEND_NONE &&
        value->types[value->const_declarations[3].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        !value->types[value->const_declarations[3].effective_type].is_signed &&
        value->types[value->const_declarations[3].effective_type].bit_width ==
            16u);
  CHECK(!value->const_declarations[4].has_explicit_type &&
        value->const_declarations[4].declared_type == W_SEED_FRONTEND_NONE &&
        value->types[value->const_declarations[4].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        !value->types[value->const_declarations[4].effective_type].is_signed &&
        value->types[value->const_declarations[4].effective_type].bit_width ==
            32u);
  CHECK(!value->const_declarations[5].has_explicit_type &&
        value->const_declarations[5].declared_type == W_SEED_FRONTEND_NONE &&
        value->types[value->const_declarations[5].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        !value->types[value->const_declarations[5].effective_type].is_signed &&
        value->types[value->const_declarations[5].effective_type].bit_width ==
            32u);

  CHECK(fixture_run(value,
                    "const answer: i64 = true\n"
                    "struct Box<_ value: i64> {}\n"
                    "struct Use { let value: Box<(answer)> }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(value, "W-SEM-0001"));

  CHECK(fixture_run(value,
                    "import { answer } from other\n"
                    "struct Box<_ value: i64> {}\n"
                    "struct Use { let value: Box<(answer)> }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL));

  CHECK(fixture_run(value,
                    "const anchor: i64 = 42\n"
                    "struct Box<_ value: i64> {}\n"
                    "struct Use { let value: Box<(missing)> }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(value, "W-SEM-0001") &&
        value->generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_INVALID);

  CHECK(fixture_run(value,
                    "const missing: i64 = absent\n"
                    "struct Box<_ value: i64> {}\n"
                    "struct Use { let value: Box<(missing)> }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(value, "W-SEM-0001"));

  CHECK(fixture_run(value,
                    "const duration: PhysicalDuration = 10<si.s>\n"
                    "struct Use {}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        value->result.written.diagnostics == 0u &&
        (has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE) ||
         has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION)));

  CHECK(fixture_run(value,
                    "const size: usize = 1<iec.MiB>\n"
                    "struct Use {}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        value->result.written.diagnostics == 0u &&
        has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));

  CHECK(fixture_parse(&fixture_capacity, named_source));
  const uint8_t sentinel = 0xa5u;
  fixture_fill_output(&fixture_capacity, sentinel);
  fixture_capacity.output.const_declarations = NULL;
  fixture_capacity.output.const_declaration_capacity = 0u;
  (void)w_seed_frontend_run(&fixture_capacity.input, &fixture_capacity.output,
                            &fixture_capacity.result);
  CHECK(fixture_capacity.result.status == W_SEED_FRONTEND_CAPACITY &&
        fixture_capacity.result.required.const_declarations == 1u &&
        fixture_output_is(&fixture_capacity, sentinel, true));
  return true;
}

static void fixture_configure_print_host(fixture *value) {
  value->host_requirements[0] = (w_seed_frontend_host_requirement){
      .name = (w_seed_frontend_text){"Console", 7u}};
  value->host_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  value->host_symbols[0] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"print", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = value->host_parameters,
      .parameter_count = 1u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = value->host_requirements,
      .requirement_count = 1u};
  value->host_scope = (w_seed_frontend_host_prelude){
      .profile = (w_seed_frontend_text){"native-process@1", 16u},
      .symbols = value->host_symbols,
      .symbol_count = 1u};
  value->input.host_scope = &value->host_scope;
}

static bool test_short_entry_frontend(void) {
  fixture *value = &fixture_literal;
  CHECK(fixture_parse(value,
                      "entry { let message = \"Hello\" print(message) }\n"));
  fixture_configure_print_host(value);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  CHECK(value->result.written.functions == 1u &&
        value->result.written.entries == 1u &&
        value->result.written.statements == 2u &&
        value->result.written.symbols == 4u);
  const w_seed_frontend_function *function = &value->functions[0];
  const w_seed_frontend_entry *entry = &value->entries[0];
  CHECK(function->is_anonymous_entry &&
        frontend_text_is(function->name, "<entry.default>") &&
        function->parameter_count == 0u &&
        function->return_type < value->result.written.types &&
        value->types[function->return_type].kind == W_SEED_FRONTEND_TYPE_UNIT);
  CHECK(entry->valid && entry->is_body && entry->target.length == 0u &&
        entry->target_function == 0u);
  CHECK(value->symbols[1].kind == W_SEED_FRONTEND_SYMBOL_BINDING &&
        value->symbols[2].kind == W_SEED_FRONTEND_SYMBOL_FUNCTION &&
        value->symbols[3].kind == W_SEED_FRONTEND_SYMBOL_ENTRY &&
        value->symbols[3].name.length == 0u);
  CHECK(receipt_contains(value, "|anonymous=1\n",
                         strlen("|anonymous=1\n")));

  w_seed_frontend_counts measured;
  w_seed_frontend_result measured_result;
  CHECK(w_seed_frontend_measure(&value->input, &measured, &measured_result) ==
        W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&measured, &value->result.required));
  CHECK(counts_equal(&value->result.required, &value->result.written));

  CHECK(fixture_parse(value, "fn run() { print(\"Hello\") }\nentry(run)\n"));
  fixture_configure_print_host(value);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.written.functions == 1u &&
        !value->functions[0].is_anonymous_entry &&
        !value->entries[0].is_body && value->entries[0].target_function == 0u &&
        frontend_text_is(value->entries[0].target, "run"));
  return true;
}

static bool test_local_binding_resolution(void) {
  static const char source[] =
      "fn main() { let message = \"Table 42 remains open\" "
      "print(message) }\nentry(main)\n";
  fixture *value = &fixture_literal;
  CHECK(fixture_parse(value, source));
  fixture_configure_print_host(value);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        frontend_text_is(value->result.schema_version,
                         W_SEED_FRONTEND_SCHEMA_VERSION) &&
        value->result.written.statements == 2u);
  const w_seed_frontend_statement *binding = &value->statements[0];
  CHECK(binding->kind == W_SEED_FRONTEND_STMT_LET &&
        frontend_text_is(binding->binding_name, "message") &&
        binding->declared_type == W_SEED_FRONTEND_NONE &&
        binding->effective_type != W_SEED_FRONTEND_NONE &&
        (size_t)binding->effective_type < value->result.written.types &&
        value->types[binding->effective_type].kind ==
            W_SEED_FRONTEND_TYPE_STRING);
  CHECK(binding->expression_index != W_SEED_FRONTEND_NONE &&
        (size_t)binding->expression_index < value->result.written.expressions &&
        value->expressions[binding->expression_index].kind ==
            W_SEED_FRONTEND_EXPR_STRING &&
        value->expressions[binding->expression_index].inferred_type ==
            binding->effective_type);
  uint32_t binding_symbol = W_SEED_FRONTEND_NONE;
  uint32_t message_expression = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.symbols; index += 1u) {
    if (value->symbols[index].kind == W_SEED_FRONTEND_SYMBOL_BINDING) {
      CHECK(binding_symbol == W_SEED_FRONTEND_NONE);
      binding_symbol = (uint32_t)index;
      CHECK(value->symbols[index].owner_index == 0u &&
            value->symbols[index].type_index == binding->effective_type);
    }
  }
  for (size_t index = 0u; index < value->result.written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        frontend_text_is(expression->spelling, "message")) {
      CHECK(message_expression == W_SEED_FRONTEND_NONE);
      message_expression = (uint32_t)index;
      CHECK(expression->supported &&
            expression->inferred_type == binding->effective_type &&
            expression->resolved_binding_statement == 0u);
    }
  }
  CHECK(binding_symbol != W_SEED_FRONTEND_NONE &&
        message_expression != W_SEED_FRONTEND_NONE &&
        receipt_contains(value, "schema=" W_SEED_FRONTEND_SCHEMA_VERSION "\n",
                         strlen("schema=" W_SEED_FRONTEND_SCHEMA_VERSION "\n")));

  fixture *trivia = &fixture_a;
  CHECK(fixture_parse(
      trivia,
      "// leading\nfn main() { let message = \"Table 42 remains open\"\n"
      "  // call\n  print ( message ) }\nentry ( main )\n"));
  fixture_configure_print_host(trivia);
  CHECK(w_seed_frontend_run(&trivia->input, &trivia->output,
                            &trivia->result) == W_SEED_FRONTEND_OK);
  CHECK(trivia->statements[0].kind == binding->kind &&
        frontend_text_is(trivia->statements[0].binding_name, "message") &&
        trivia->statements[0].effective_type != W_SEED_FRONTEND_NONE &&
        trivia->types[trivia->statements[0].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_STRING);
  for (size_t index = 0u; index < trivia->result.written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &trivia->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        frontend_text_is(expression->spelling, "message"))
      CHECK(expression->resolved_binding_statement == 0u);
  }

  fixture *forward = &fixture_condition;
  CHECK(fixture_parse(
      forward,
      "fn main() { print(message) let message = \"Table 42 remains open\" }\n"
      "entry(main)\n"));
  fixture_configure_print_host(forward);
  CHECK(w_seed_frontend_run(&forward->input, &forward->output,
                            &forward->result) == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(forward, W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL));
  for (size_t index = 0u; index < forward->result.written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &forward->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        frontend_text_is(expression->spelling, "message"))
      CHECK(expression->resolved_binding_statement == W_SEED_FRONTEND_NONE);
  }

  fixture *duplicate = &fixture_narrowing;
  CHECK(fixture_parse(
      duplicate,
      "fn main() { let message = \"a\" let message = \"b\" "
      "print(message) }\nentry(main)\n"));
  fixture_configure_print_host(duplicate);
  (void)w_seed_frontend_run(&duplicate->input, &duplicate->output,
                            &duplicate->result);
  for (size_t index = 0u; index < duplicate->result.written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &duplicate->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        frontend_text_is(expression->spelling, "message"))
      CHECK(expression->resolved_binding_statement == W_SEED_FRONTEND_NONE);
  }

  fixture *nested = &fixture_label;
  CHECK(fixture_parse(
      nested,
      "fn main() { let message = \"a\" if true { let message = \"b\" "
      "print(message) } }\nentry(main)\n"));
  fixture_configure_print_host(nested);
  (void)w_seed_frontend_run(&nested->input, &nested->output, &nested->result);
  bool nested_message_seen = false;
  for (size_t index = 0u; index < nested->result.written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &nested->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        frontend_text_is(expression->spelling, "message"))
      CHECK(!nested_message_seen &&
            expression->resolved_binding_statement == 2u &&
            (nested_message_seen = true));
  }
  CHECK(nested_message_seen);
  return true;
}

static bool test_multidocument_const_ordinals(void) {
  static fixture first;
  static fixture second;
  static const char first_source[] =
      "const first = 1\n"
      "struct First {}\n";
  static const char second_source[] =
      "const second = 2\n"
      "struct Second {}\n";
  static const char first_module[] = "first-module";
  static const char second_module[] = "second-module";
  CHECK(fixture_parse(&first, first_source));
  CHECK(fixture_parse(&second, second_source));
  first.document.logical_source_id =
      (w_seed_frontend_text){first_module, sizeof(first_module) - 1u};
  first.document.module_id = first.document.logical_source_id;
  first.document.local_module_name = first.document.module_id;
  second.document.logical_source_id =
      (w_seed_frontend_text){second_module, sizeof(second_module) - 1u};
  second.document.module_id = second.document.logical_source_id;
  second.document.local_module_name = second.document.module_id;
  w_seed_frontend_document documents[2] = {first.document, second.document};
  first.input.documents = documents;
  first.input.document_count = 2u;
  first.input.external_modules = NULL;
  first.input.external_module_count = 0u;
  CHECK(w_seed_frontend_run(&first.input, &first.output, &first.result) ==
        W_SEED_FRONTEND_OK);
  CHECK(first.result.written.const_declarations == 2u &&
        first.result.written.modules == 2u &&
        first.modules[0].first_const_declaration == 0u &&
        first.modules[0].const_declaration_count == 1u &&
        first.modules[1].first_const_declaration == 1u &&
        first.modules[1].const_declaration_count == 1u &&
        first.const_declarations[0].module_index == 0u &&
        first.const_declarations[1].module_index == 1u &&
        first.const_declarations[0].declared_type ==
            W_SEED_FRONTEND_NONE &&
        first.const_declarations[1].declared_type ==
            W_SEED_FRONTEND_NONE &&
        first.const_declarations[0].effective_type !=
            W_SEED_FRONTEND_NONE &&
        first.const_declarations[1].effective_type !=
            W_SEED_FRONTEND_NONE &&
        first.types[first.const_declarations[0].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        first.types[first.const_declarations[1].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER);
  return true;
}

static bool test_multidocument_predicate_owner(void) {
  static fixture root;
  static fixture helper;
  static const char root_source[] =
      "import { isValid } from helper\n"
      "struct Invalid<_ value: usize<(isValid(.member))>> {}\n";
  static const char helper_source[] =
      "module helper\n"
      "export fn isValid(value: usize): Bool { return true }\n";
  CHECK(fixture_parse(&root, root_source));
  CHECK(fixture_parse(&helper, helper_source));
  root.document.logical_source_id = (w_seed_frontend_text){"root", 4u};
  root.document.module_id = root.document.logical_source_id;
  root.document.local_module_name = root.document.module_id;
  helper.document.logical_source_id =
      (w_seed_frontend_text){"helper", 6u};
  helper.document.module_id = helper.document.logical_source_id;
  helper.document.local_module_name = helper.document.module_id;

  w_seed_module_origin origins[TEST_IMPORTS];
  w_seed_module_scan_result scan_result;
  CHECK(w_seed_module_scan(&root.source, root.nodes, root.parse.node_count,
                           &root.parse, origins, TEST_IMPORTS, &scan_result) ==
            W_SEED_MODULE_SCAN_OK &&
        scan_result.written == 1u);
  root.resolved_imports[0] = (w_seed_frontend_resolved_import){
      .source_document_index = 0u,
      .direct_import_ordinal = origins[0].direct_import_ordinal,
      .import_declaration_span = origins[0].declaration_span,
      .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT,
      .target_index = 1u};
  w_seed_frontend_document documents[2] = {root.document, helper.document};
  root.input.documents = documents;
  root.input.document_count = 2u;
  root.input.external_modules = NULL;
  root.input.external_module_count = 0u;
  root.input.import_resolution_complete = true;
  root.input.resolved_imports = root.resolved_imports;
  root.input.resolved_import_count = 1u;
  fixture_fill_output(&root, 0u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
        W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(&root, "W-CONST-0001"));
  const w_seed_frontend_diagnostic *diagnostic =
      diagnostic_for_code(&root, "W-CONST-0001");
  CHECK(diagnostic != NULL && diagnostic->fact_count == 4u &&
        diagnostic->label_count == 1u &&
        diagnostic_record_ranges_are_valid(&root, diagnostic));
  const w_seed_frontend_diagnostic_label *label =
      &root.diagnostic_labels[diagnostic->first_label];
  uint32_t helper_function_node = W_SEED_CST_NONE;
  for (size_t index = 0u; index < helper.parse.node_count; index += 1u) {
    if (helper.nodes[index].kind == W_SEED_CST_FUNCTION) {
      helper_function_node = (uint32_t)index;
      break;
    }
  }
  CHECK(helper_function_node != W_SEED_CST_NONE &&
        label->document_index == 1u &&
        label->span.start_byte == helper.nodes[helper_function_node]
                                        .raw_span.start_byte &&
        label->span.end_byte == helper.nodes[helper_function_node]
                                      .raw_span.end_byte &&
        root.input.documents[1].source == helper.document.source);
  return true;
}

static bool test_resolved_import_edges_and_identity(void) {
  static fixture root;
  static fixture child;
  static fixture redirected;
  static const char root_source[] =
      "import { value } from kitchen.menu\n"
      "fn use(): i64 { return value() }\n";
  static const char child_source[] =
      "module menu\n"
      "export fn value(): i64 { return 42 }\n";
  static const char redirected_source[] =
      "export fn value(): i64 { return 7 }\n";
  CHECK(fixture_parse(&root, root_source));
  CHECK(fixture_parse(&child, child_source));
  CHECK(fixture_parse(&redirected, redirected_source));
  root.document.logical_source_id = (w_seed_frontend_text){"root", 4};
  root.document.module_id = (w_seed_frontend_text){"kitchen.root", 12};
  root.document.local_module_name = (w_seed_frontend_text){"root", 4};
  child.document.logical_source_id = (w_seed_frontend_text){"menu", 4};
  child.document.module_id = (w_seed_frontend_text){"kitchen.menu", 12};
  child.document.local_module_name = (w_seed_frontend_text){"menu", 4};
  redirected.document.logical_source_id =
      (w_seed_frontend_text){"redirected", 10};
  redirected.document.module_id = (w_seed_frontend_text){"other.menu", 10};
  redirected.document.local_module_name =
      (w_seed_frontend_text){"menu", 4};
  w_seed_module_origin origins[1];
  w_seed_module_scan_result scan_result;
  CHECK(w_seed_module_scan(&root.source, root.nodes, root.parse.node_count,
                           &root.parse, origins, 1u, &scan_result) ==
        W_SEED_MODULE_SCAN_OK &&
        scan_result.written == 1u);
  root.resolved_imports[0] = (w_seed_frontend_resolved_import){
      .source_document_index = 0u,
      .direct_import_ordinal = 0u,
      .import_declaration_span = origins[0].declaration_span,
      .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT,
      .target_index = 1u};
  w_seed_frontend_document documents[3] = {
      root.document, child.document, redirected.document};
  root.input.documents = documents;
  root.input.document_count = 3u;
  root.input.external_modules = NULL;
  root.input.external_module_count = 0u;
  root.input.import_resolution_complete = true;
  root.input.resolved_imports = root.resolved_imports;
  root.input.resolved_import_count = 1u;
  fixture_fill_output(&root, 0u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
        W_SEED_FRONTEND_OK);
  CHECK(root.result.written.modules == 3u && root.result.written.imports == 1u &&
        root.modules[0].module_id.length == 12u &&
        memcmp(root.modules[0].module_id.data, "kitchen.root", 12u) == 0 &&
        root.modules[1].module_id.length == 12u &&
        memcmp(root.modules[1].module_id.data, "kitchen.menu", 12u) == 0 &&
        root.modules[1].local_module_name.length == 4u &&
        memcmp(root.modules[1].local_module_name.data, "menu", 4u) == 0);
  CHECK(root.imports[0].target_kind ==
            W_SEED_FRONTEND_IMPORT_LOCAL_DOCUMENT &&
        root.imports[0].target_index == 1u &&
        root.imports[0].direct_import_ordinal == 0u);
  CHECK(receipt_contains(&root, "module=12:6b69746368656e2e6d656e75|local=4:6d656e75",
                         strlen("module=12:6b69746368656e2e6d656e75|local=4:6d656e75")));
  bool saw_redirectable_call = false;
  for (size_t index = 0u; index < root.result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &root.expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_CALL &&
        expression->resolved_function_index != W_SEED_FRONTEND_NONE) {
      const w_seed_frontend_function *function =
          &root.functions[expression->resolved_function_index];
      if (function->module_index == 1u && function->name.length == 5u &&
          memcmp(function->name.data, "value", 5u) == 0) {
        saw_redirectable_call = true;
      }
    }
  }
  CHECK(saw_redirectable_call);
  const size_t first_receipt_bytes = root.result.receipt_bytes;
  uint8_t first_receipt[TEST_RECEIPT];
  (void)memcpy(first_receipt, root.receipt, first_receipt_bytes);
  root.resolved_imports[0].target_index = 2u;
  fixture_fill_output(&root, 0u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
        W_SEED_FRONTEND_OK);
  CHECK(root.imports[0].target_index == 2u &&
        (root.result.receipt_bytes != first_receipt_bytes ||
         memcmp(first_receipt, root.receipt, first_receipt_bytes) != 0));

  /* Resolution is an explicit all-or-nothing input transaction. */
  root.input.import_resolution_complete = false;
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
        W_SEED_FRONTEND_INVALID);
  root.input.import_resolution_complete = true;
  root.input.resolved_import_count = 0u;
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
        W_SEED_FRONTEND_INVALID);
  root.input.resolved_import_count = 1u;
  root.resolved_imports[0].source_document_index = 1u;
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
        W_SEED_FRONTEND_INVALID);
  root.resolved_imports[0].source_document_index = 0u;
  root.resolved_imports[0].direct_import_ordinal = 1u;
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
        W_SEED_FRONTEND_INVALID);
  root.resolved_imports[0].direct_import_ordinal = 0u;
  root.resolved_imports[0].import_declaration_span.start_byte += 1u;
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
        W_SEED_FRONTEND_INVALID);
  root.resolved_imports[0].import_declaration_span = origins[0].declaration_span;
  root.resolved_imports[0].target_index = 3u;
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
        W_SEED_FRONTEND_INVALID);
  root.resolved_imports[0].target_index = 0u;
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
        W_SEED_FRONTEND_INVALID);
  root.resolved_imports[0].target_index = 1u;
  documents[1].local_module_name = (w_seed_frontend_text){"wrong", 5};
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
        W_SEED_FRONTEND_INVALID);
  documents[1].local_module_name = (w_seed_frontend_text){"menu", 4};
  documents[1].local_module_name = (w_seed_frontend_text){NULL, 0u};
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
        W_SEED_FRONTEND_INVALID && fixture_output_is(&root, 0xa5u, true));
  documents[1].local_module_name = (w_seed_frontend_text){"menu", 4};

  static fixture cycle_first;
  static fixture cycle_second;
  static const char cycle_first_source[] =
      "import { value } from cycle.second\n";
  static const char cycle_second_source[] =
      "import { value } from cycle.first\n";
  CHECK(fixture_parse(&cycle_first, cycle_first_source));
  CHECK(fixture_parse(&cycle_second, cycle_second_source));
  cycle_first.document.module_id = (w_seed_frontend_text){"cycle.first", 11};
  cycle_first.document.local_module_name =
      (w_seed_frontend_text){"first", 5};
  cycle_second.document.module_id =
      (w_seed_frontend_text){"cycle.second", 12};
  cycle_second.document.local_module_name =
      (w_seed_frontend_text){"second", 6};
  w_seed_module_origin cycle_origins[2];
  w_seed_module_scan_result cycle_scan;
  CHECK(w_seed_module_scan(&cycle_first.source, cycle_first.nodes,
                           cycle_first.parse.node_count, &cycle_first.parse,
                           &cycle_origins[0], 1u, &cycle_scan) ==
        W_SEED_MODULE_SCAN_OK);
  CHECK(w_seed_module_scan(&cycle_second.source, cycle_second.nodes,
                           cycle_second.parse.node_count, &cycle_second.parse,
                           &cycle_origins[1], 1u, &cycle_scan) ==
        W_SEED_MODULE_SCAN_OK);
  w_seed_frontend_document cycle_documents[2] = {cycle_first.document,
                                                  cycle_second.document};
  cycle_first.resolved_imports[0] = (w_seed_frontend_resolved_import){
      .source_document_index = 0u,
      .direct_import_ordinal = 0u,
      .import_declaration_span = cycle_origins[0].declaration_span,
      .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT,
      .target_index = 1u};
  cycle_first.resolved_imports[1] = (w_seed_frontend_resolved_import){
      .source_document_index = 1u,
      .direct_import_ordinal = 0u,
      .import_declaration_span = cycle_origins[1].declaration_span,
      .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT,
      .target_index = 0u};
  cycle_first.input.documents = cycle_documents;
  cycle_first.input.document_count = 2u;
  cycle_first.input.import_resolution_complete = true;
  cycle_first.input.resolved_imports = cycle_first.resolved_imports;
  cycle_first.input.resolved_import_count = 2u;
  fixture_fill_output(&cycle_first, 0xa5u);
  CHECK(w_seed_frontend_run(&cycle_first.input, &cycle_first.output,
                            &cycle_first.result) == W_SEED_FRONTEND_INVALID);
  return true;
}

static bool test_resolved_import_edge_validation(void) {
  static fixture root;
  static fixture first;
  static fixture second;
  static const char root_source[] =
      "import { value } from first\n"
      "import { other } from second\n";
  static const char first_source[] =
      "export fn value(): i64 { return 1 }\n";
  static const char second_source[] =
      "export fn other(): i64 { return 2 }\n";
  CHECK(fixture_parse(&root, root_source));
  CHECK(fixture_parse(&first, first_source));
  CHECK(fixture_parse(&second, second_source));
  root.document.logical_source_id = (w_seed_frontend_text){"root", 4};
  root.document.module_id = (w_seed_frontend_text){"pkg.root", 8};
  root.document.local_module_name = (w_seed_frontend_text){"root", 4};
  first.document.logical_source_id = (w_seed_frontend_text){"first", 5};
  first.document.module_id = (w_seed_frontend_text){"pkg.first", 9};
  first.document.local_module_name = (w_seed_frontend_text){"first", 5};
  second.document.logical_source_id = (w_seed_frontend_text){"second", 6};
  second.document.module_id = (w_seed_frontend_text){"pkg.second", 10};
  second.document.local_module_name = (w_seed_frontend_text){"second", 6};
  w_seed_module_origin origins[2];
  w_seed_module_scan_result scan_result;
  CHECK(w_seed_module_scan(&root.source, root.nodes, root.parse.node_count,
                           &root.parse, origins, 2u, &scan_result) ==
        W_SEED_MODULE_SCAN_OK &&
        scan_result.written == 2u);
  root.resolved_imports[0] = (w_seed_frontend_resolved_import){
      .source_document_index = 0u,
      .direct_import_ordinal = 0u,
      .import_declaration_span = origins[0].declaration_span,
      .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT,
      .target_index = 1u};
  root.resolved_imports[1] = (w_seed_frontend_resolved_import){
      .source_document_index = 0u,
      .direct_import_ordinal = 1u,
      .import_declaration_span = origins[1].declaration_span,
      .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT,
      .target_index = 2u};
  w_seed_frontend_document documents[3] = {root.document, first.document,
                                           second.document};
  root.input.documents = documents;
  root.input.document_count = 3u;
  root.input.external_modules = NULL;
  root.input.external_module_count = 0u;
  root.input.import_resolution_complete = true;
  root.input.resolved_imports = root.resolved_imports;
  root.input.resolved_import_count = 2u;
  fixture_fill_output(&root, 0u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
            W_SEED_FRONTEND_OK &&
        root.result.written.imports == 2u);

  root.input.resolved_import_count = 3u;
  root.resolved_imports[2] = root.resolved_imports[1];
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
            W_SEED_FRONTEND_INVALID &&
        fixture_output_is(&root, 0xa5u, true));
  root.input.resolved_import_count = 2u;
  root.resolved_imports[1] = root.resolved_imports[0];
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
            W_SEED_FRONTEND_INVALID &&
        fixture_output_is(&root, 0xa5u, true));
  root.resolved_imports[1] = (w_seed_frontend_resolved_import){
      .source_document_index = 0u,
      .direct_import_ordinal = 1u,
      .import_declaration_span = origins[1].declaration_span,
      .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT,
      .target_index = 2u};
  w_seed_frontend_resolved_import swapped = root.resolved_imports[0];
  root.resolved_imports[0] = root.resolved_imports[1];
  root.resolved_imports[1] = swapped;
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
            W_SEED_FRONTEND_INVALID &&
        fixture_output_is(&root, 0xa5u, true));
  swapped = root.resolved_imports[0];
  root.resolved_imports[0] = root.resolved_imports[1];
  root.resolved_imports[1] = swapped;
  root.resolved_imports[0].target_kind =
      (w_seed_frontend_resolved_import_kind)99;
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
            W_SEED_FRONTEND_INVALID &&
        fixture_output_is(&root, 0xa5u, true));
  root.resolved_imports[0].target_kind =
      W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT;
  root.resolved_imports[0].target_index = 1u;
  root.resolved_imports[0].target_kind =
      W_SEED_FRONTEND_RESOLVED_IMPORT_EXTERNAL_MODULE;
  root.resolved_imports[0].target_index = 0u;
  fixture_fill_output(&root, 0xa5u);
  CHECK(w_seed_frontend_run(&root.input, &root.output, &root.result) ==
            W_SEED_FRONTEND_INVALID &&
        fixture_output_is(&root, 0xa5u, true));
  return true;
}

static bool test_implicit_integer_widening_frontend(void) {
  static const char source[] =
      "fn ret(value: u8): i16 { return value }\n"
      "fn widen(value: i16): i16 { return value }\n"
      "fn add(left: u8, right: i16): i16 { return left + right }\n"
      "fn caller(): i16 { let local: i16 = 1_u8 return widen(value: 2_u8) }\n"
      "fn runtime(value: u8): i16 { let local: i16 = value "
      "return widen(value: value) }\n"
      "fn assign(value: u8): i16 { var result: i16 = 0 result = value "
      "return result }\n"
      "entry(caller)\n";
  fixture *value = &fixture_a;
  CHECK(fixture_run(value, source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(value->result.status == W_SEED_FRONTEND_OK);

  size_t widening_count = 0u;
  bool saw_return = false;
  bool saw_binary = false;
  bool saw_binding = false;
  bool saw_argument = false;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind !=
        W_SEED_FRONTEND_EXPR_IMPLICIT_INTEGER_WIDEN)
      continue;
    widening_count += 1u;
    CHECK(expression->supported && expression->left != W_SEED_FRONTEND_NONE &&
          expression->right == W_SEED_FRONTEND_NONE &&
          expression->conversion_source_type != W_SEED_FRONTEND_NONE &&
          expression->conversion_destination_type != W_SEED_FRONTEND_NONE &&
          expression->conversion_source_type < value->result.written.types &&
          expression->conversion_destination_type < value->result.written.types &&
          expression->inferred_type == expression->conversion_destination_type);
    const w_seed_frontend_expression *source_expression =
        &value->expressions[expression->left];
    CHECK(source_expression->inferred_type ==
              expression->conversion_source_type &&
          value->types[expression->conversion_source_type].kind ==
              W_SEED_FRONTEND_TYPE_INTEGER &&
          value->types[expression->conversion_destination_type].kind ==
              W_SEED_FRONTEND_TYPE_INTEGER &&
          value->types[expression->conversion_source_type].bit_width <
              value->types[expression->conversion_destination_type].bit_width &&
          (!value->types[expression->conversion_source_type].is_signed ||
           value->types[expression->conversion_destination_type].is_signed));
    if (source_expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER)
      saw_return = true;
    if (source_expression->kind == W_SEED_FRONTEND_EXPR_INTEGER)
      saw_binding = true;
    for (size_t parent_index = 0u;
         parent_index < value->result.written.expressions; parent_index += 1u) {
      const w_seed_frontend_expression *parent =
          &value->expressions[parent_index];
      if ((parent->left == (uint32_t)index ||
           parent->right == (uint32_t)index) &&
          parent->kind == W_SEED_FRONTEND_EXPR_BINARY)
        saw_binary = true;
    }
    for (size_t argument_index = 0u;
         argument_index < value->result.written.arguments; argument_index += 1u) {
      const w_seed_frontend_argument *argument =
          &value->arguments[argument_index];
      if (argument->expression_index == (uint32_t)index &&
          argument->owner_expression < value->result.written.expressions) {
        for (size_t call_index = 0u;
             call_index < value->result.written.expressions; call_index += 1u) {
          const w_seed_frontend_expression *call =
              &value->expressions[call_index];
          if (call->kind == W_SEED_FRONTEND_EXPR_CALL &&
              call->first_argument != W_SEED_FRONTEND_NONE &&
              argument_index >= call->first_argument &&
              argument_index <
                  (size_t)call->first_argument + call->argument_count)
            saw_argument = true;
        }
      }
    }
  }
  CHECK(widening_count == 7u && saw_return && saw_binary && saw_binding &&
        saw_argument);

  static const struct {
    const char *from;
    const char *to;
  } positive_matrix[] = {
      {"i8", "i16"}, {"i8", "i32"}, {"i8", "i64"},
      {"i16", "i32"}, {"i16", "i64"}, {"i32", "i64"},
      {"u8", "u16"}, {"u8", "u32"}, {"u8", "u64"},
      {"u16", "u32"}, {"u16", "u64"}, {"u32", "u64"},
      {"u8", "i16"}, {"u8", "i32"}, {"u8", "i64"},
      {"u16", "i32"}, {"u16", "i64"}, {"u32", "i64"},
      {"u8", "Int"}, {"u8", "UInt"}, {"i8", "Int"},
  };
  char matrix_source[256];
  for (size_t case_index = 0u;
       case_index < sizeof(positive_matrix) / sizeof(positive_matrix[0]);
       case_index += 1u) {
    const int written = snprintf(
        matrix_source, sizeof(matrix_source),
        "fn f(value: %s): %s { return value } entry(f)\n",
        positive_matrix[case_index].from, positive_matrix[case_index].to);
    CHECK(written > 0 && (size_t)written < sizeof(matrix_source));
    CHECK(fixture_run(value, matrix_source));
    CHECK(value->result.status == W_SEED_FRONTEND_OK);
    const w_seed_frontend_expression *wrapper = NULL;
    size_t wrappers = 0u;
    for (size_t expression_index = 0u;
         expression_index < value->result.written.expressions;
         expression_index += 1u) {
      const w_seed_frontend_expression *candidate =
          &value->expressions[expression_index];
      if (candidate->kind ==
          W_SEED_FRONTEND_EXPR_IMPLICIT_INTEGER_WIDEN) {
        wrapper = candidate;
        wrappers += 1u;
      }
    }
    CHECK(wrappers == 1u && wrapper != NULL &&
          wrapper->conversion_source_type < value->result.written.types &&
          wrapper->conversion_destination_type < value->result.written.types);
    const w_seed_frontend_type *source_type =
        &value->types[wrapper->conversion_source_type];
    const w_seed_frontend_type *destination_type =
        &value->types[wrapper->conversion_destination_type];
    CHECK(source_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
          destination_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
          source_type->bit_width < destination_type->bit_width &&
          (!source_type->is_signed || destination_type->is_signed));
  }

  CHECK(fixture_run(value, "fn ok(): u8 { return 255 } entry(ok)\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  for (size_t expression_index = 0u;
       expression_index < value->result.written.expressions;
       expression_index += 1u)
    CHECK(value->expressions[expression_index].kind !=
          W_SEED_FRONTEND_EXPR_IMPLICIT_INTEGER_WIDEN);

  fixture *invalid = &fixture_b;
  CHECK(fixture_run(invalid,
                    "fn tooLarge(): u8 { return 256 } entry(tooLarge)\n"));
  CHECK(invalid->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(invalid, "W-TYPE-0122"));
  CHECK(fixture_run(invalid,
                    "fn negative(): u8 { return -1 } entry(negative)\n"));
  CHECK(invalid->result.status != W_SEED_FRONTEND_OK);
  CHECK(fixture_run(
      invalid, "fn f(value: u16): u8 { return value }\nentry(f)\n"));
  CHECK(invalid->result.status == W_SEED_FRONTEND_DIAGNOSTICS ||
        invalid->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_diagnostic(invalid, "W-TYPE-0122"));
  for (size_t index = 0u; index < invalid->result.written.expressions;
       index += 1u)
    CHECK(invalid->expressions[index].kind !=
          W_SEED_FRONTEND_EXPR_IMPLICIT_INTEGER_WIDEN);

  CHECK(fixture_run(
      invalid, "fn f(value: i16): u16 { return value }\nentry(f)\n"));
  CHECK(invalid->result.status == W_SEED_FRONTEND_DIAGNOSTICS ||
        invalid->result.status == W_SEED_FRONTEND_UNSUPPORTED);

  static const char *const rejected_matrix[] = {
      "fn f(value: i16): u32 { return value } entry(f)\n",
      "fn f(value: u16): i16 { return value } entry(f)\n",
      "fn f(left: i8, right: u8): i16 { return left + right } entry(f)\n",
      "fn f(value: i16): i8 { return value } entry(f)\n",
      "fn f(value: i8): u16 { return value } entry(f)\n",
      "fn f(value: Int): UInt { return value } entry(f)\n",
      "fn f(value: UInt): Int { return value } entry(f)\n",
      "fn f(value: u8): i8 { return value } entry(f)\n",
      "fn f(value: u32): i32 { return value } entry(f)\n",
      "fn f(value: u8): usize { return value } entry(f)\n",
  };
  for (size_t case_index = 0u;
       case_index < sizeof(rejected_matrix) / sizeof(rejected_matrix[0]);
       case_index += 1u) {
    CHECK(fixture_run(invalid, rejected_matrix[case_index]));
    CHECK(invalid->result.status != W_SEED_FRONTEND_OK);
    CHECK(has_diagnostic(invalid, "W-TYPE-0122"));
    for (size_t expression_index = 0u;
         expression_index < invalid->result.written.expressions;
         expression_index += 1u)
      CHECK(invalid->expressions[expression_index].kind !=
            W_SEED_FRONTEND_EXPR_IMPLICIT_INTEGER_WIDEN);
  }
  return true;
}

static bool test_explicit_integer_truncating_bits_frontend(void) {
  static const char CONTEXTS[] =
      "fn returnValue(value: i16): i8 { return i8(truncatingBits: value) }\n"
      "fn bindingValue(value: i8): i16 { let result: i16 = "
      "i16(truncatingBits: value) return result }\n"
      "fn sink(value: i8): i8 { return value }\n"
      "fn argumentValue(value: i16): i8 { return sink(value: "
      "i8(truncatingBits: value)) }\n"
      "fn equalWidth(value: u8): i8 { return i8(truncatingBits: value) }\n"
      "entry(returnValue)\n";
  fixture *value = &fixture_a;
  CHECK(fixture_run(value, CONTEXTS));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  size_t conversion_count = 0u;
  bool saw_return = false;
  bool saw_binding = false;
  bool saw_argument = false;
  bool saw_equal_width_reinterpretation = false;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind !=
        W_SEED_FRONTEND_EXPR_INTEGER_TRUNCATING_BITS)
      continue;
    conversion_count += 1u;
    CHECK(expression->supported &&
          expression->left != W_SEED_FRONTEND_NONE &&
          expression->left < value->result.written.expressions &&
          expression->right == W_SEED_FRONTEND_NONE &&
          expression->first_argument == W_SEED_FRONTEND_NONE &&
          expression->argument_count == 0u &&
          expression->conversion_source_type < value->result.written.types &&
          expression->conversion_destination_type <
              value->result.written.types &&
          expression->inferred_type ==
              expression->conversion_destination_type);
    const w_seed_frontend_expression *source =
        &value->expressions[expression->left];
    CHECK(source->inferred_type == expression->conversion_source_type &&
          value->types[expression->conversion_source_type].kind ==
              W_SEED_FRONTEND_TYPE_INTEGER &&
          value->types[expression->conversion_destination_type].kind ==
              W_SEED_FRONTEND_TYPE_INTEGER);
    saw_return |= source->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
                  value->types[expression->conversion_destination_type]
                          .bit_width == 8u;
    saw_binding |= source->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
                   value->types[expression->conversion_destination_type]
                           .bit_width == 16u;
    for (size_t argument = 0u;
         argument < value->result.written.arguments; argument += 1u) {
      if (value->arguments[argument].expression_index == index)
        saw_argument = true;
    }
    saw_equal_width_reinterpretation |=
        value->types[expression->conversion_source_type].bit_width == 8u &&
        value->types[expression->conversion_destination_type].bit_width ==
            8u &&
        value->types[expression->conversion_source_type].is_signed !=
            value->types[expression->conversion_destination_type].is_signed;
  }
  CHECK(conversion_count == 4u && saw_return && saw_binding && saw_argument &&
        saw_equal_width_reinterpretation);

  typedef struct {
    const char *name;
    bool is_signed;
    uint16_t bit_width;
  } integer_type_case;
  static const integer_type_case INTEGER_TYPES[] = {
      {"i8", true, 8u},     {"u8", false, 8u},
      {"i16", true, 16u},  {"u16", false, 16u},
      {"i32", true, 32u},  {"u32", false, 32u},
      {"i64", true, 64u},  {"u64", false, 64u},
      {"Int", true, 64u},  {"UInt", false, 64u},
  };
  char matrix_source[256];
  fixture *matrix = &fixture_b;
  for (size_t source_index = 0u;
       source_index < sizeof(INTEGER_TYPES) / sizeof(INTEGER_TYPES[0]);
       source_index += 1u) {
    for (size_t destination_index = 0u;
         destination_index <
         sizeof(INTEGER_TYPES) / sizeof(INTEGER_TYPES[0]);
         destination_index += 1u) {
      const int written = snprintf(
          matrix_source, sizeof(matrix_source),
          "fn f(value: %s): %s { return %s(truncatingBits: value) } "
          "entry(f)\n",
          INTEGER_TYPES[source_index].name,
          INTEGER_TYPES[destination_index].name,
          INTEGER_TYPES[destination_index].name);
      CHECK(written > 0 && (size_t)written < sizeof(matrix_source));
      CHECK(fixture_run(matrix, matrix_source));
      CHECK(matrix->result.status == W_SEED_FRONTEND_OK);
      const w_seed_frontend_expression *wrapper = NULL;
      size_t wrappers = 0u;
      for (size_t expression = 0u;
           expression < matrix->result.written.expressions;
           expression += 1u) {
        const w_seed_frontend_expression *candidate =
            &matrix->expressions[expression];
        if (candidate->kind ==
            W_SEED_FRONTEND_EXPR_INTEGER_TRUNCATING_BITS) {
          wrapper = candidate;
          wrappers += 1u;
        }
      }
      CHECK(wrappers == 1u && wrapper != NULL &&
            wrapper->conversion_source_type < matrix->result.written.types &&
            wrapper->conversion_destination_type <
                matrix->result.written.types);
      const w_seed_frontend_type *source_type =
          &matrix->types[wrapper->conversion_source_type];
      const w_seed_frontend_type *destination_type =
          &matrix->types[wrapper->conversion_destination_type];
      CHECK(source_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            source_type->is_signed == INTEGER_TYPES[source_index].is_signed &&
            source_type->bit_width == INTEGER_TYPES[source_index].bit_width &&
            destination_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            destination_type->is_signed ==
                INTEGER_TYPES[destination_index].is_signed &&
            destination_type->bit_width ==
                INTEGER_TYPES[destination_index].bit_width);
    }
  }

  static const char *const REJECTED[] = {
      "fn f(value: i16): i8 { return i8(value: value) } entry(f)\n",
      "fn f(value: i16): i8 { return i8(value) } entry(f)\n",
      "fn f(value: i16): i8 { return i8() } entry(f)\n",
      "fn f(value: i16): i8 { return i8(truncatingBits: value, "
      "truncatingBits: value) } entry(f)\n",
      "fn f(value: Bool): i8 { return i8(truncatingBits: value) } "
      "entry(f)\n",
      "fn f(value: i16): f64 { return f64(truncatingBits: value) } "
      "entry(f)\n",
      "fn f(value: i16): usize { return usize(truncatingBits: value) } "
      "entry(f)\n",
      "fn f(value: i16): UnknownInteger { return "
      "UnknownInteger(truncatingBits: value) } entry(f)\n",
      "fn f(value: i16): i128 { return i128(truncatingBits: value) } "
      "entry(f)\n",
      "fn f(value: usize): i8 { return i8(truncatingBits: value) } "
      "entry(f)\n",
      "fn f(value: f64): i8 { return i8(truncatingBits: value) } "
      "entry(f)\n",
  };
  for (size_t index = 0u; index < sizeof(REJECTED) / sizeof(REJECTED[0]);
       index += 1u) {
    CHECK(fixture_run(matrix, REJECTED[index]));
    CHECK(matrix->result.status != W_SEED_FRONTEND_OK);
    for (size_t expression = 0u;
         expression < matrix->result.written.expressions;
         expression += 1u)
      CHECK(matrix->expressions[expression].kind !=
            W_SEED_FRONTEND_EXPR_INTEGER_TRUNCATING_BITS);
  }
  return true;
}

static bool test_explicit_integer_exactly_frontend(void) {
  typedef struct {
    const char *name;
    bool is_signed;
    uint16_t bit_width;
  } integer_type_case;
  static const integer_type_case INTEGER_TYPES[] = {
      {"i8", true, 8u},     {"u8", false, 8u},
      {"i16", true, 16u},   {"u16", false, 16u},
      {"i32", true, 32u},   {"u32", false, 32u},
      {"i64", true, 64u},   {"u64", false, 64u},
      {"Int", true, 64u},   {"UInt", false, 64u},
  };
  char matrix_source[320];
  fixture *matrix = &fixture_b;
  for (size_t source_index = 0u;
       source_index < sizeof(INTEGER_TYPES) / sizeof(INTEGER_TYPES[0]);
       source_index += 1u) {
    for (size_t destination_index = 0u;
         destination_index <
         sizeof(INTEGER_TYPES) / sizeof(INTEGER_TYPES[0]);
         destination_index += 1u) {
      const int written = snprintf(
          matrix_source, sizeof(matrix_source),
          "fn convert(value: %s): %s throws NumericConversionError { "
          "return try %s(exactly: value) } entry { }\n",
          INTEGER_TYPES[source_index].name,
          INTEGER_TYPES[destination_index].name,
          INTEGER_TYPES[destination_index].name);
      CHECK(written > 0 && (size_t)written < sizeof(matrix_source));
      CHECK(fixture_run(matrix, matrix_source));
      CHECK(matrix->parse.status == W_SEED_PARSE_COMPLETE &&
            matrix->result.status == W_SEED_FRONTEND_OK &&
            counts_equal(&matrix->result.required,
                         &matrix->result.written));
      const w_seed_frontend_expression *conversion = NULL;
      const w_seed_frontend_expression *try_expression = NULL;
      size_t conversions = 0u;
      size_t tries = 0u;
      for (size_t expression = 0u;
           expression < matrix->result.written.expressions;
           expression += 1u) {
        const w_seed_frontend_expression *candidate =
            &matrix->expressions[expression];
        if (candidate->kind == W_SEED_FRONTEND_EXPR_INTEGER_EXACTLY) {
          conversion = candidate;
          conversions += 1u;
        } else if (candidate->kind == W_SEED_FRONTEND_EXPR_TRY) {
          try_expression = candidate;
          tries += 1u;
        }
      }
      CHECK(conversions == 1u && tries == 1u && conversion != NULL &&
            try_expression != NULL && conversion->supported &&
            try_expression->supported && try_expression->left !=
                W_SEED_FRONTEND_NONE &&
            &matrix->expressions[try_expression->left] == conversion &&
            try_expression->propagated_error_enum == W_SEED_FRONTEND_NONE &&
            try_expression->propagated_error_type <
                matrix->result.written.types &&
            conversion->conversion_source_type <
                matrix->result.written.types &&
            conversion->conversion_destination_type <
                matrix->result.written.types &&
            conversion->inferred_type ==
                conversion->conversion_destination_type &&
            try_expression->inferred_type == conversion->inferred_type &&
            frontend_text_is(try_expression->operator_text, "try"));
      const w_seed_frontend_type *source_type =
          &matrix->types[conversion->conversion_source_type];
      const w_seed_frontend_type *destination_type =
          &matrix->types[conversion->conversion_destination_type];
      const w_seed_frontend_type *error_type =
          &matrix->types[try_expression->propagated_error_type];
      CHECK(source_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            source_type->is_signed == INTEGER_TYPES[source_index].is_signed &&
            source_type->bit_width == INTEGER_TYPES[source_index].bit_width &&
            destination_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            destination_type->is_signed ==
                INTEGER_TYPES[destination_index].is_signed &&
            destination_type->bit_width ==
                INTEGER_TYPES[destination_index].bit_width &&
            error_type->kind == W_SEED_FRONTEND_TYPE_NOMINAL &&
            frontend_text_is(error_type->spelling,
                             "NumericConversionError") &&
            error_type->external_module_index == W_SEED_FRONTEND_NONE &&
            error_type->external_symbol_index == W_SEED_FRONTEND_NONE);
    }
  }

  fixture *rejected = &fixture_a;
  static const char *const REJECTED[] = {
      "fn f(value: i16): i8 { return i8(exactly: value) } entry { }\n",
      "fn f(value: i16): i8 { return try i8(exactly: value) } entry { }\n",
      "fn f(value: i16): i8 throws NumericConversionError { return "
      "try? i8(exactly: value) } entry { }\n",
      "fn f(value: i16): i8 throws NumericConversionError { return "
      "try i8(exact: value) } entry { }\n",
      "fn f(value: i16): i8 throws NumericConversionError { return "
      "try i8(exactly: value, exactly: value) } entry { }\n",
      "fn f(value: i16): i8 throws NumericConversionError { return "
      "try i8(exactly: value, other: 0_i16) } entry { }\n",
      "enum Failure: Error { denied } fn f(value: i16): i8 throws Failure { "
      "return try i8(exactly: value) } entry { }\n",
      "enum NumericConversionError: Error { outOfRange } fn f(value: i16): "
      "i8 throws NumericConversionError { return try i8(exactly: value) } "
      "entry { }\n",
      "struct NumericConversionError {} fn f(value: i16): i8 throws "
      "NumericConversionError { return try i8(exactly: value) } entry { }\n",
      "fn f(value: i16): f64 throws NumericConversionError { return try "
      "f64(exactly: value) } entry { }\n",
      "fn f(value: i16): i128 throws NumericConversionError { return try "
      "i128(exactly: value) } entry { }\n",
      "fn f(value: usize): i8 throws NumericConversionError { return try "
      "i8(exactly: value) } entry { }\n",
  };
  for (size_t index = 0u;
       index < sizeof(REJECTED) / sizeof(REJECTED[0]); index += 1u) {
    CHECK(fixture_run(rejected, REJECTED[index]));
    CHECK(rejected->result.status != W_SEED_FRONTEND_OK);
    for (size_t expression = 0u;
         expression < rejected->result.written.expressions;
         expression += 1u)
      CHECK(rejected->expressions[expression].kind !=
            W_SEED_FRONTEND_EXPR_INTEGER_EXACTLY ||
            !rejected->expressions[expression].supported);
  }
  return true;
}

static bool test_float_bits_frontend(void) {
  static const char SOURCE[] =
      "fn fromF32(bits: u32): f32 { return f32.fromBits(bits) }\n"
      "fn toF32(value: f32): u32 { return value.toBits() }\n"
      "fn fromF64(bits: u64): f64 { return f64.fromBits(bits) }\n"
      "fn toF64(value: f64): u64 { return value.toBits() }\n"
      "entry(fromF32)\n";
  fixture *value = &fixture_a;
  CHECK(fixture_run(value, SOURCE));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  w_seed_frontend_counts measured_counts;
  w_seed_frontend_result measured_result;
  CHECK(w_seed_frontend_measure(&value->input, &measured_counts,
                                &measured_result) == W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&measured_counts, &value->result.required) &&
        measured_result.required.receipt_bytes ==
            value->result.receipt_bytes &&
        receipt_contains(value, "float-bits-conversion=",
                        strlen("float-bits-conversion=")));

  size_t from_bits_count = 0u;
  size_t to_bits_count = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    const bool from_bits =
        expression->kind == W_SEED_FRONTEND_EXPR_FLOAT_FROM_BITS;
    const bool to_bits = expression->kind == W_SEED_FRONTEND_EXPR_FLOAT_TO_BITS;
    if (!from_bits && !to_bits) continue;
    from_bits_count += from_bits ? 1u : 0u;
    to_bits_count += to_bits ? 1u : 0u;
    CHECK(expression->supported &&
          expression->left != W_SEED_FRONTEND_NONE &&
          expression->left < value->result.written.expressions &&
          expression->right == W_SEED_FRONTEND_NONE &&
          expression->first_argument == W_SEED_FRONTEND_NONE &&
          expression->argument_count == 0u &&
          expression->conversion_source_type < value->result.written.types &&
          expression->conversion_destination_type <
              value->result.written.types &&
          expression->inferred_type ==
              expression->conversion_destination_type &&
          !expression->has_float_value && !expression->has_integer_value);
    const w_seed_frontend_expression *source =
        &value->expressions[expression->left];
    const w_seed_frontend_type *source_type =
        &value->types[expression->conversion_source_type];
    const w_seed_frontend_type *destination_type =
        &value->types[expression->conversion_destination_type];
    CHECK(source->inferred_type == expression->conversion_source_type &&
          !source->has_float_value && !source->has_integer_value);
    if (from_bits) {
      CHECK(source_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            !source_type->is_signed &&
            (source_type->bit_width == 32u ||
             source_type->bit_width == 64u) &&
            destination_type->kind == W_SEED_FRONTEND_TYPE_FLOAT &&
            destination_type->bit_width == source_type->bit_width);
    } else {
      CHECK(source_type->kind == W_SEED_FRONTEND_TYPE_FLOAT &&
            (source_type->bit_width == 32u ||
             source_type->bit_width == 64u) &&
            destination_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            !destination_type->is_signed &&
            destination_type->bit_width == source_type->bit_width);
    }
  }
  CHECK(from_bits_count == 2u && to_bits_count == 2u);

  const size_t canonical_receipt_bytes = value->result.receipt_bytes;
  uint8_t canonical_receipt[TEST_RECEIPT];
  CHECK(canonical_receipt_bytes <= sizeof(canonical_receipt));
  (void)memcpy(canonical_receipt, value->receipt, canonical_receipt_bytes);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.receipt_bytes == canonical_receipt_bytes &&
        memcmp(value->receipt, canonical_receipt, canonical_receipt_bytes) ==
            0);

  /* The exact bridge expressions participate in the frontend preflight:
   * a short output buffer must not expose partial expression records. */
  const size_t expression_capacity = value->output.expression_capacity;
  CHECK(value->result.written.expressions < expression_capacity);
  fixture_fill_output(value, 0xa5u);
  value->output.expression_capacity = value->result.written.expressions - 1u;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_CAPACITY);
  CHECK(fixture_output_is(value, 0xa5u, true));
  value->output.expression_capacity = expression_capacity;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);

  static const char *const REJECTED[] = {
      "fn f(bits: u64): f32 { return f32.fromBits(bits) } entry(f)\n",
      "fn f(bits: u32): f64 { return f64.fromBits(bits) } entry(f)\n",
      "fn f(bits: i32): f32 { return f32.fromBits(bits) } entry(f)\n",
      "fn f(bits: u32): f32 { return f32.fromBits(bits: bits) } entry(f)\n",
      "fn f(): f32 { return f32.fromBits() } entry(f)\n",
      "fn f(bits: u32, extra: u32): f32 { return "
      "f32.fromBits(bits, extra) } entry(f)\n",
      "fn f(value: f32): f32 { return value.toBits(0_u32) } entry(f)\n",
      "fn f(value: i32): u32 { return value.toBits() } entry(f)\n",
      "fn f(value: f32): f32 { return value.fromBits(0_u32) } entry(f)\n",
      "fn f(bits: u32): u32 { return f32.toBits() } entry(f)\n",
      "fn f(f32: u32, bits: u32): f32 { return f32.fromBits(bits) } "
      "entry(f)\n",
  };
  fixture *invalid = &fixture_b;
  for (size_t index = 0u; index < sizeof(REJECTED) / sizeof(REJECTED[0]);
       index += 1u) {
    CHECK(fixture_run(invalid, REJECTED[index]));
    CHECK(invalid->result.status != W_SEED_FRONTEND_OK &&
          has_fact(invalid, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
    for (size_t expression = 0u;
         expression < invalid->result.written.expressions; expression += 1u)
      CHECK(invalid->expressions[expression].kind !=
                W_SEED_FRONTEND_EXPR_FLOAT_FROM_BITS &&
            invalid->expressions[expression].kind !=
                W_SEED_FRONTEND_EXPR_FLOAT_TO_BITS);
  }
  return true;
}

static bool test_explicit_integer_saturating_frontend(void) {
  typedef struct {
    const char *name;
    bool is_signed;
    uint16_t bit_width;
  } integer_type_case;
  static const integer_type_case INTEGER_TYPES[] = {
      {"i8", true, 8u},     {"u8", false, 8u},
      {"i16", true, 16u},  {"u16", false, 16u},
      {"i32", true, 32u},  {"u32", false, 32u},
      {"i64", true, 64u},  {"u64", false, 64u},
      {"Int", true, 64u},  {"UInt", false, 64u},
  };
  char source[256];
  fixture *matrix = &fixture_b;
  for (size_t source_index = 0u;
       source_index < sizeof(INTEGER_TYPES) / sizeof(INTEGER_TYPES[0]);
       source_index += 1u) {
    for (size_t destination_index = 0u;
         destination_index <
         sizeof(INTEGER_TYPES) / sizeof(INTEGER_TYPES[0]);
         destination_index += 1u) {
      const int written = snprintf(
          source, sizeof(source),
          "fn f(value: %s): %s { return %s(saturating: value) } "
          "entry(f)\n",
          INTEGER_TYPES[source_index].name,
          INTEGER_TYPES[destination_index].name,
          INTEGER_TYPES[destination_index].name);
      CHECK(written > 0 && (size_t)written < sizeof(source));
      CHECK(fixture_run(matrix, source));
      CHECK(matrix->parse.status == W_SEED_PARSE_COMPLETE &&
            matrix->result.status == W_SEED_FRONTEND_OK &&
            counts_equal(&matrix->result.required,
                         &matrix->result.written));
      const w_seed_frontend_expression *wrapper = NULL;
      size_t wrappers = 0u;
      for (size_t expression = 0u;
           expression < matrix->result.written.expressions;
           expression += 1u) {
        const w_seed_frontend_expression *candidate =
            &matrix->expressions[expression];
        if (candidate->kind == W_SEED_FRONTEND_EXPR_INTEGER_SATURATING) {
          wrapper = candidate;
          wrappers += 1u;
        }
      }
      CHECK(wrappers == 1u && wrapper != NULL && wrapper->supported &&
            wrapper->left < matrix->result.written.expressions &&
            wrapper->right == W_SEED_FRONTEND_NONE &&
            wrapper->first_argument == W_SEED_FRONTEND_NONE &&
            wrapper->argument_count == 0u &&
            wrapper->conversion_source_type < matrix->result.written.types &&
            wrapper->conversion_destination_type <
                matrix->result.written.types &&
            wrapper->inferred_type == wrapper->conversion_destination_type);
      const w_seed_frontend_type *source_type =
          &matrix->types[wrapper->conversion_source_type];
      const w_seed_frontend_type *destination_type =
          &matrix->types[wrapper->conversion_destination_type];
      CHECK(source_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            source_type->is_signed == INTEGER_TYPES[source_index].is_signed &&
            source_type->bit_width == INTEGER_TYPES[source_index].bit_width &&
            destination_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            destination_type->is_signed ==
                INTEGER_TYPES[destination_index].is_signed &&
            destination_type->bit_width ==
                INTEGER_TYPES[destination_index].bit_width &&
            matrix->expressions[wrapper->left].inferred_type ==
                wrapper->conversion_source_type);
    }
  }

  /* Conversion sources are independently verified expressions.  In
   * particular, an unsuffixed literal still defaults to i64 rather than
   * inheriting the narrower i8 destination context. */
  static const struct {
    const char *source;
    w_seed_frontend_expr_kind source_kind;
    bool is_signed;
    uint16_t bit_width;
  } VERIFIED_SOURCES[] = {
      {"fn f(): i8 { return i8(saturating: 250_u16) } entry(f)\n",
       W_SEED_FRONTEND_EXPR_INTEGER, false, 16u},
      {"fn f(): i8 { return i8(saturating: 120_i16 + 8_i16) } entry(f)\n",
       W_SEED_FRONTEND_EXPR_BINARY, true, 16u},
      {"fn f(): i8 { return i8(saturating: 127) } entry(f)\n",
       W_SEED_FRONTEND_EXPR_INTEGER, true, 64u},
      {"fn f(): i8 { return i8(saturating: 64 + 63) } entry(f)\n",
       W_SEED_FRONTEND_EXPR_BINARY, true, 64u},
  };
  for (size_t index = 0u;
       index < sizeof(VERIFIED_SOURCES) / sizeof(VERIFIED_SOURCES[0]);
       index += 1u) {
    CHECK(fixture_run(matrix, VERIFIED_SOURCES[index].source));
    CHECK(matrix->parse.status == W_SEED_PARSE_COMPLETE &&
          matrix->result.status == W_SEED_FRONTEND_OK &&
          counts_equal(&matrix->result.required,
                       &matrix->result.written));
    const w_seed_frontend_expression *wrapper = NULL;
    size_t wrappers = 0u;
    for (size_t expression = 0u;
         expression < matrix->result.written.expressions;
         expression += 1u) {
      const w_seed_frontend_expression *candidate =
          &matrix->expressions[expression];
      if (candidate->kind == W_SEED_FRONTEND_EXPR_INTEGER_SATURATING) {
        wrapper = candidate;
        wrappers += 1u;
      }
    }
    CHECK(wrappers == 1u && wrapper != NULL && wrapper->supported &&
          wrapper->left < matrix->result.written.expressions &&
          wrapper->right == W_SEED_FRONTEND_NONE &&
          wrapper->conversion_source_type < matrix->result.written.types &&
          wrapper->conversion_destination_type <
              matrix->result.written.types &&
          wrapper->inferred_type == wrapper->conversion_destination_type);
    const w_seed_frontend_expression *source_expression =
        &matrix->expressions[wrapper->left];
    const w_seed_frontend_type *source_type =
        &matrix->types[wrapper->conversion_source_type];
    const w_seed_frontend_type *destination_type =
        &matrix->types[wrapper->conversion_destination_type];
    CHECK(source_expression->kind == VERIFIED_SOURCES[index].source_kind &&
          source_expression->inferred_type == wrapper->conversion_source_type &&
          source_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
          source_type->is_signed == VERIFIED_SOURCES[index].is_signed &&
          source_type->bit_width == VERIFIED_SOURCES[index].bit_width &&
          destination_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
          destination_type->is_signed && destination_type->bit_width == 8u);
    if (source_expression->kind == W_SEED_FRONTEND_EXPR_BINARY) {
      CHECK(source_expression->left < matrix->result.written.expressions &&
            source_expression->right < matrix->result.written.expressions &&
            matrix->expressions[source_expression->left].inferred_type ==
                wrapper->conversion_source_type &&
            matrix->expressions[source_expression->right].inferred_type ==
                wrapper->conversion_source_type);
    }
  }

  static const char *const REJECTED[] = {
      "fn f(value: i16): i8 { return i8(saturating: value, nan: .zero) } "
      "entry(f)\n",
      "fn f(value: i16): i8 { return i8(saturating: value, "
      "saturating: value) } entry(f)\n",
      "fn f(value: i16): i8 { return i8(saturating: value, other: value) } "
      "entry(f)\n",
      "fn f(value: i16): i8 { return i8(saturating: 1_i16, value: value) } "
      "entry(f)\n",
      "fn f(value: i16): i8 { return i8(saturating:) } entry(f)\n",
      "fn f(value: Bool): i8 { return i8(saturating: value) } entry(f)\n",
      "fn f(value: f64): i8 { return i8(saturating: value) } entry(f)\n",
      "fn f(value: i16): f64 { return f64(saturating: value) } entry(f)\n",
      "fn f(value: usize): i8 { return i8(saturating: value) } entry(f)\n",
      "fn f(value: i16): usize { return usize(saturating: value) } entry(f)\n",
      "fn f(value: isize): i8 { return i8(saturating: value) } entry(f)\n",
      "fn f(value: i16): isize { return isize(saturating: value) } entry(f)\n",
      "fn f(value: i128): i8 { return i8(saturating: value) } entry(f)\n",
      "fn f(value: i16): u128 { return u128(saturating: value) } entry(f)\n",
      "fn f(value: i16): UnknownInteger { return "
      "UnknownInteger(saturating: value) } entry(f)\n",
      "fn f(value: i16): i8 { return i8(checked: value) } entry(f)\n",
      "fn f(value: i16): i8 { return i8() } entry(f)\n",
  };
  for (size_t index = 0u; index < sizeof(REJECTED) / sizeof(REJECTED[0]);
       index += 1u) {
    CHECK(fixture_run(matrix, REJECTED[index]));
    CHECK(matrix->result.status != W_SEED_FRONTEND_OK);
    bool retained_saturating_label = false;
    for (size_t expression = 0u;
         expression < matrix->result.written.expressions;
         expression += 1u) {
      CHECK(matrix->expressions[expression].kind !=
            W_SEED_FRONTEND_EXPR_INTEGER_SATURATING);
      if (matrix->expressions[expression].kind ==
              W_SEED_FRONTEND_EXPR_UNSUPPORTED &&
          frontend_text_is(matrix->expressions[expression].operator_text,
                           "saturating"))
        retained_saturating_label = true;
    }
    if (index == 0u) CHECK(retained_saturating_label);
  }
  static const char TRY_SATURATING[] =
      "fn f(value: i16): i8 { return try i8(saturating: value) } "
      "entry(f)\n";
  CHECK(fixture_run(matrix, TRY_SATURATING));
  CHECK(matrix->result.status != W_SEED_FRONTEND_OK);
  bool saw_unsupported_try = false;
  for (size_t index = 0u; index < matrix->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression =
        &matrix->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_TRY) {
      CHECK(!expression->supported &&
            expression->left < matrix->result.written.expressions &&
            matrix->expressions[expression->left].kind ==
                W_SEED_FRONTEND_EXPR_INTEGER_SATURATING);
      saw_unsupported_try = true;
    }
  }
  CHECK(saw_unsupported_try);
  return true;
}

static bool test_float_integer_rounding_frontend(void) {
  typedef struct {
    const char *spelling;
    bool is_signed;
    uint16_t bit_width;
  } integer_case;
  static const integer_case DESTINATIONS[] = {
      {"i8", true, 8u},     {"u8", false, 8u},
      {"i16", true, 16u},   {"u16", false, 16u},
      {"i32", true, 32u},   {"u32", false, 32u},
      {"i64", true, 64u},   {"u64", false, 64u},
      {"Int", true, 64u},   {"UInt", false, 64u},
  };
  static const struct {
    const char *spelling;
    w_seed_frontend_rounding_mode mode;
  } MODES[] = {
      {"nearestEven", W_SEED_FRONTEND_ROUNDING_MODE_NEAREST_EVEN},
      {"nearestAwayFromZero",
       W_SEED_FRONTEND_ROUNDING_MODE_NEAREST_AWAY_FROM_ZERO},
      {"towardZero", W_SEED_FRONTEND_ROUNDING_MODE_TOWARD_ZERO},
      {"towardPositive", W_SEED_FRONTEND_ROUNDING_MODE_TOWARD_POSITIVE},
      {"towardNegative", W_SEED_FRONTEND_ROUNDING_MODE_TOWARD_NEGATIVE},
  };
  static const struct {
    const char *spelling;
    uint16_t bit_width;
  } SOURCES[] = {{"f32", 32u}, {"f64", 64u}};
  fixture *matrix = &fixture_literal;
  char source[512];
  for (size_t source_index = 0u;
       source_index < sizeof(SOURCES) / sizeof(SOURCES[0]);
       source_index += 1u) {
    for (size_t destination_index = 0u;
         destination_index <
             sizeof(DESTINATIONS) / sizeof(DESTINATIONS[0]);
         destination_index += 1u) {
      for (size_t mode_index = 0u;
           mode_index < sizeof(MODES) / sizeof(MODES[0]); mode_index += 1u) {
        const bool mode_first = (mode_index & 1u) != 0u;
        const int written = snprintf(
            source, sizeof(source),
            mode_first
                ? "fn convert(value: %s): %s throws NumericConversionError { "
                  "return try %s(mode: .%s, rounding: value) } entry { }\n"
                : "fn convert(value: %s): %s throws NumericConversionError { "
                  "return try %s(rounding: value, mode: .%s) } entry { }\n",
            SOURCES[source_index].spelling,
            DESTINATIONS[destination_index].spelling,
            DESTINATIONS[destination_index].spelling,
            MODES[mode_index].spelling);
        CHECK(written > 0 && (size_t)written < sizeof(source));
        CHECK(fixture_run(matrix, source));
        CHECK(matrix->parse.status == W_SEED_PARSE_COMPLETE &&
              matrix->result.status == W_SEED_FRONTEND_OK &&
              counts_equal(&matrix->result.required,
                           &matrix->result.written));
        const w_seed_frontend_expression *conversion = NULL;
        const w_seed_frontend_expression *try_expression = NULL;
        size_t conversion_count = 0u;
        size_t try_count = 0u;
        for (size_t expression_index = 0u;
             expression_index < matrix->result.written.expressions;
             expression_index += 1u) {
          const w_seed_frontend_expression *expression =
              &matrix->expressions[expression_index];
          if (expression->kind ==
              W_SEED_FRONTEND_EXPR_FLOAT_TO_INTEGER_ROUNDING) {
            conversion = expression;
            conversion_count += 1u;
          } else if (expression->kind == W_SEED_FRONTEND_EXPR_TRY) {
            try_expression = expression;
            try_count += 1u;
          }
        }
        CHECK(conversion_count == 1u && try_count == 1u &&
              conversion != NULL && try_expression != NULL &&
              conversion->supported && try_expression->supported &&
              try_expression->left < matrix->result.written.expressions &&
              &matrix->expressions[try_expression->left] == conversion &&
              try_expression->propagated_error_enum ==
                  W_SEED_FRONTEND_NONE &&
              try_expression->propagated_error_type <
                  matrix->result.written.types &&
              conversion->conversion_source_type <
                  matrix->result.written.types &&
              conversion->conversion_destination_type <
                  matrix->result.written.types &&
              conversion->conversion_rounding_mode == MODES[mode_index].mode &&
              conversion->conversion_possible_error_facts ==
                  (W_SEED_FRONTEND_CONVERSION_ERROR_FACT_NON_FINITE |
                   W_SEED_FRONTEND_CONVERSION_ERROR_FACT_OUT_OF_RANGE));
        const w_seed_frontend_type *source_type =
            &matrix->types[conversion->conversion_source_type];
        const w_seed_frontend_type *destination_type =
            &matrix->types[conversion->conversion_destination_type];
        CHECK(source_type->kind == W_SEED_FRONTEND_TYPE_FLOAT &&
              source_type->bit_width == SOURCES[source_index].bit_width &&
              destination_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
              destination_type->is_signed ==
                  DESTINATIONS[destination_index].is_signed &&
              destination_type->bit_width ==
                  DESTINATIONS[destination_index].bit_width &&
              conversion->inferred_type ==
                  conversion->conversion_destination_type &&
              try_expression->inferred_type == conversion->inferred_type);
        CHECK(receipt_contains(
            matrix, "float-integer-rounding=",
            sizeof("float-integer-rounding=") - 1u));
      }
    }
  }

  static const char *const REJECTED[] = {
      "fn f(value: f32): i8 throws NumericConversionError { return "
      "i8(rounding: value, mode: .nearestEven) } entry { }\n",
      "fn f(value: f32): i8 throws NumericConversionError { return "
      "try? i8(rounding: value, mode: .nearestEven) } entry { }\n",
      "fn f(value: f32): i8 { return try i8(rounding: value, mode: "
      ".nearestEven) } entry { }\n",
      "fn f(value: f32): i8 throws Failure { return try i8(rounding: "
      "value, mode: .nearestEven) } entry { }\n",
      "fn f(value: f32): i8 throws NumericConversionError { return try "
      "i8(rounding: value) } entry { }\n",
      "fn f(value: f32): i8 throws NumericConversionError { return try "
      "i8(rounding: value, mode: .nearestEven, mode: .towardZero) } "
      "entry { }\n",
      "fn f(value: f32): i8 throws NumericConversionError { return try "
      "i8(rounding: value, other: .nearestEven) } entry { }\n",
      "fn f(value: f32): i8 throws NumericConversionError { return try "
      "i8(rounding: value, mode: nearestEven) } entry { }\n",
      "fn f(value: f32): i8 throws NumericConversionError { return try "
      "i8(rounding: value, mode: .unknown) } entry { }\n",
      "fn f(value: f32): i8 throws NumericConversionError { return try "
      "i8(rounding: value, mode: .nearestEven, extra: value) } entry { }\n",
      "fn f(value: i32): i8 throws NumericConversionError { return try "
      "i8(rounding: value, mode: .nearestEven) } entry { }\n",
      "fn f(value: f32): f64 throws NumericConversionError { return try "
      "f64(rounding: value, mode: .nearestEven) } entry { }\n",
      "fn f(value: f32): usize throws NumericConversionError { return try "
      "usize(rounding: value, mode: .nearestEven) } entry { }\n",
      "fn f(value: f32): i128 throws NumericConversionError { return try "
      "i128(rounding: value, mode: .nearestEven) } entry { }\n",
  };
  for (size_t index = 0u;
       index < sizeof(REJECTED) / sizeof(REJECTED[0]); index += 1u) {
    CHECK(fixture_run(matrix, REJECTED[index]));
    CHECK(matrix->result.status != W_SEED_FRONTEND_OK);
    for (size_t expression_index = 0u;
         expression_index < matrix->result.written.expressions;
         expression_index += 1u) {
      CHECK(matrix->expressions[expression_index].kind !=
                W_SEED_FRONTEND_EXPR_FLOAT_TO_INTEGER_ROUNDING ||
            !matrix->expressions[expression_index].supported);
    }
  }
  return true;
}

static bool test_graph_facts_and_external_stub(void) {
  fixture *duplicate = &fixture_duplicate;
  CHECK(fixture_run(duplicate,
                    "fn f(): () { return }\n"
                    "fn f(): () { return }\nentry(f)\n"));
  CHECK(duplicate->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(duplicate,
                 W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL));

  fixture *unresolved = &fixture_unresolved;
  CHECK(fixture_run(unresolved,
                    "import { missing } from absent\n"
                    "fn f(): () { return }\nentry(f)\n"));
  CHECK(unresolved->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(unresolved,
                 W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL));

  fixture *external = &fixture_external;
  CHECK(fixture_parse(external,
                      "import { ext } from extdep\n"
                      "fn f(): u32 { return ext(1) }\nentry(f)\n"));
  external->external_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"value", 5},
      .type = (w_seed_frontend_text){"u32", 3},
  };
  external->external_symbols[0] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"ext", 3},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = external->external_parameters,
      .parameter_count = 1,
      .return_type = (w_seed_frontend_text){"u32", 3},
  };
  external->external_modules[0] = (w_seed_frontend_external_module){
      .module_id = (w_seed_frontend_text){"extdep", 6},
      .symbols = external->external_symbols,
      .symbol_count = 1,
  };
  external->input.external_modules = external->external_modules;
  external->input.external_module_count = 1;
  CHECK(fixture_resolve_external_imports(external));
  (void)w_seed_frontend_run(&external->input, &external->output,
                            &external->result);
  CHECK(external->result.status == W_SEED_FRONTEND_OK);
  CHECK(!has_fact(external,
                  W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL));
  static uint8_t external_receipt[TEST_RECEIPT];
  const size_t first_receipt_bytes = external->result.receipt_bytes;
  (void)memcpy(external_receipt, external->receipt, first_receipt_bytes);
  external->external_symbols[0].return_type =
      (w_seed_frontend_text){"Bool", 4};
  (void)w_seed_frontend_run(&external->input, &external->output,
                            &external->result);
  CHECK(external->result.receipt_bytes != first_receipt_bytes ||
        memcmp(external_receipt, external->receipt, first_receipt_bytes) != 0);
  CHECK(external->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  external->external_symbols[0].kind =
      (w_seed_frontend_external_kind)99;
  CHECK(w_seed_frontend_run(&external->input, &external->output,
                            &external->result) == W_SEED_FRONTEND_INVALID);
  external->external_symbols[0].kind = W_SEED_FRONTEND_EXTERNAL_VALUE;
  external->external_parameters[0].label_kind =
      (w_seed_frontend_label_kind)99;
  CHECK(w_seed_frontend_run(&external->input, &external->output,
                            &external->result) == W_SEED_FRONTEND_INVALID);
  external->external_parameters[0].label_kind =
      W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY;
  external->external_modules[1] = (w_seed_frontend_external_module){
      .module_id = (w_seed_frontend_text){"extdep", 6},
      .symbols = NULL,
      .symbol_count = 0,
  };
  external->input.external_module_count = 2;
  CHECK(w_seed_frontend_run(&external->input, &external->output,
                            &external->result) == W_SEED_FRONTEND_INVALID);
  external->input.external_module_count = 1;
  external->external_modules[0].symbols = external->external_symbols;
  external->external_modules[0].symbol_count = 2;
  external->external_symbols[1] = external->external_symbols[0];
  CHECK(w_seed_frontend_run(&external->input, &external->output,
                            &external->result) == W_SEED_FRONTEND_INVALID);
  external->external_modules[0].symbol_count = 1;
  external->external_modules[0].module_id =
      (w_seed_frontend_text){"test", 4};
  CHECK(w_seed_frontend_run(&external->input, &external->output,
                            &external->result) == W_SEED_FRONTEND_INVALID);

  CHECK(fixture_parse(
      external,
      "import { externalFn } from extdep\n"
      "enum Stage { ready }\n"
      "fn f(): u32 { return externalFn(.ready) }\n"));
  external->external_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"value", 5},
      .type = (w_seed_frontend_text){"Stage", 5},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY,
  };
  external->external_symbols[0] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"externalFn", 10},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = external->external_parameters,
      .parameter_count = 1,
      .return_type = (w_seed_frontend_text){"u32", 3},
  };
  external->external_modules[0] = (w_seed_frontend_external_module){
      .module_id = (w_seed_frontend_text){"extdep", 6},
      .symbols = external->external_symbols,
      .symbol_count = 1,
  };
  external->input.external_modules = external->external_modules;
  external->input.external_module_count = 1;
  CHECK(fixture_resolve_external_imports(external));
  (void)w_seed_frontend_run(&external->input, &external->output,
                            &external->result);
  CHECK(external->result.status == W_SEED_FRONTEND_OK);
  CHECK(external->result.written.diagnostics == 0);
  CHECK(external->result.written.arguments == 1);

  /* External stubs carry only the nominal alias spelling.  The local alias
   * declaration must still recover its enum identity and case-set at the
   * call boundary. */
  CHECK(fixture_parse(
      external,
      "import { externalSubset } from extdep\n"
      "enum Stage { accepted preparing serving }\n"
      "alias WorkStage = Stage<[.preparing, .serving]>\n"
      "fn good(stage: WorkStage): WorkStage { return externalSubset(stage) }\n"
      "fn bad(): WorkStage { return externalSubset(.accepted) }\n"));
  external->external_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"stage", 5},
      .type = (w_seed_frontend_text){"WorkStage", 9},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY,
  };
  external->external_symbols[0] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"externalSubset", 14},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = external->external_parameters,
      .parameter_count = 1,
      .return_type = (w_seed_frontend_text){"WorkStage", 9},
  };
  external->external_modules[0] = (w_seed_frontend_external_module){
      .module_id = (w_seed_frontend_text){"extdep", 6},
      .symbols = external->external_symbols,
      .symbol_count = 1,
  };
  external->input.external_modules = external->external_modules;
  external->input.external_module_count = 1;
  CHECK(fixture_resolve_external_imports(external));
  (void)w_seed_frontend_run(&external->input, &external->output,
                            &external->result);
  CHECK(external->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(external, "W-TYPE-0121"));
  return true;
}

static bool test_receipt_encoding_and_long_fields(void) {
  const char prefix[] = "fn f(): () { return [1 | 2, ";
  const char ending[] = "] }\nentry(f)\n";
  size_t length = 0;
  (void)memcpy(long_source + length, prefix, sizeof(prefix) - 1u);
  length += sizeof(prefix) - 1u;
  for (size_t index = 0; index < 128u; index += 1) {
    long_source[length] = '1';
    length += 1;
    long_source[length] = ',';
    length += 1;
  }
  (void)memcpy(long_source + length, ending, sizeof(ending) - 1u);
  length += sizeof(ending) - 1u;
  long_source[length] = '\0';
  CHECK(length < sizeof(long_source));

  fixture *long_fixture = &fixture_a;
  CHECK(fixture_run(long_fixture, long_source));
  CHECK(long_fixture->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(long_fixture,
                 W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  CHECK(long_fixture->result.receipt_bytes > 256u);
  /* The pipe in the unsupported span is encoded as hex, not as an unescaped
   * field delimiter. */
  CHECK(receipt_contains(long_fixture, "7c", 2));
  return true;
}

static bool test_generic_schema(void) {
  static const char stage_source[] =
      "export enum ServiceStage { accepted completed }\n"
      "const fn isValidStagePath(stages: StaticList<ServiceStage>): Bool { "
      "return true }\n"
      "struct StagePath<_ stages: StaticList<ServiceStage>"
      "<(isValidStagePath(.member))>> { let orderId: u64 }\n";
  fixture *stage = &fixture_a;
  CHECK(fixture_run(stage, stage_source));
  CHECK(stage->result.status == W_SEED_FRONTEND_OK);
  CHECK(stage->result.written.generic_parameters == 1u);
  CHECK(stage->structs[0].first_generic_parameter == 0u &&
        stage->structs[0].generic_parameter_count == 1u);
  const w_seed_frontend_generic_parameter *stage_parameter =
      &stage->generic_parameters[0];
  CHECK(stage_parameter->owner_kind == W_SEED_FRONTEND_DECL_STRUCT &&
        stage_parameter->owner_index == 0u &&
        stage_parameter->ordinal == 0u);
  CHECK(stage_parameter->external_label.length == 0u &&
        stage_parameter->internal_name.length == 6u &&
        memcmp(stage_parameter->internal_name.data, "stages", 6u) == 0);
  CHECK(stage_parameter->kind == W_SEED_FRONTEND_GENERIC_KIND_VALUE &&
        stage_parameter->label_kind ==
            W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
        stage_parameter->domain_type != W_SEED_FRONTEND_NONE);
  CHECK(stage->types[stage_parameter->domain_type].kind ==
        W_SEED_FRONTEND_TYPE_STATIC_LIST);
  CHECK(stage->types[stage_parameter->domain_type].element_type !=
        W_SEED_FRONTEND_NONE);
  CHECK(stage->types[stage->types[stage_parameter->domain_type].element_type]
            .kind == W_SEED_FRONTEND_TYPE_ENUM);
  CHECK(stage_parameter->refinement_kind ==
            W_SEED_FRONTEND_GENERIC_REFINEMENT_PREDICATE &&
        stage_parameter->subject_kind ==
            W_SEED_FRONTEND_GENERIC_SUBJECT_MEMBER &&
        stage_parameter->predicate_function_index == 0u &&
        stage_parameter->predicate_function_span.start_byte !=
            stage_parameter->predicate_function_span.end_byte);
  CHECK(!has_fact(stage, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE));
  CHECK(receipt_contains(stage, "generic-parameter=", 18u));

  fixture *matrix = &fixture_b;
  CHECK(fixture_run(matrix,
                    "struct Matrix<Element, rows: usize, columns: usize> {}\n"));
  CHECK(matrix->result.status == W_SEED_FRONTEND_OK &&
        matrix->result.written.generic_parameters == 3u);
  CHECK(matrix->generic_parameters[0].kind ==
            W_SEED_FRONTEND_GENERIC_KIND_TYPE &&
        matrix->generic_parameters[0].label_kind ==
            W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
        matrix->generic_parameters[0].external_label.length == 0u &&
        matrix->generic_parameters[0].domain_type == W_SEED_FRONTEND_NONE);
  CHECK(matrix->generic_parameters[1].kind ==
            W_SEED_FRONTEND_GENERIC_KIND_VALUE &&
        matrix->generic_parameters[1].label_kind ==
            W_SEED_FRONTEND_LABEL_REQUIRED);
  CHECK(matrix->generic_parameters[2].kind ==
            W_SEED_FRONTEND_GENERIC_KIND_VALUE &&
        matrix->generic_parameters[2].label_kind ==
            W_SEED_FRONTEND_LABEL_REQUIRED &&
        matrix->generic_parameters[1].external_label.length == 4u &&
        memcmp(matrix->generic_parameters[1].external_label.data, "rows",
               4u) == 0);

  fixture *labels = &fixture_label;
  CHECK(fixture_run(labels,
                    "struct Labels<required: usize, _ anchor: usize> {}\n"));
  CHECK(labels->result.status == W_SEED_FRONTEND_OK &&
        labels->generic_parameters[0].label_kind ==
            W_SEED_FRONTEND_LABEL_REQUIRED &&
        labels->generic_parameters[1].label_kind ==
            W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
        labels->generic_parameters[0].external_label.length == 8u &&
        memcmp(labels->generic_parameters[0].external_label.data, "required",
               8u) == 0 &&
        labels->generic_parameters[1].external_label.length == 0u);

  CHECK(fixture_run(labels,
                    "struct Box<external internal: usize> {}\n"));
  CHECK(labels->result.status == W_SEED_FRONTEND_OK &&
        labels->result.written.generic_parameters == 1u &&
        labels->generic_parameters[0].label_kind ==
            W_SEED_FRONTEND_LABEL_REQUIRED &&
        labels->generic_parameters[0].external_label.length == 8u &&
        memcmp(labels->generic_parameters[0].external_label.data, "external",
               8u) == 0 &&
        labels->generic_parameters[0].internal_name.length == 8u &&
        memcmp(labels->generic_parameters[0].internal_name.data, "internal",
               8u) == 0);

  fixture *range = &fixture_literal;
  CHECK(fixture_run(range,
                    "struct Tile<rows: usize<(1...4096)>> {}\n"));
  CHECK(range->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        range->generic_parameters[0].kind ==
            W_SEED_FRONTEND_GENERIC_KIND_VALUE &&
        range->generic_parameters[0].refinement_kind ==
            W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID &&
        range->generic_parameters[0].subject_kind ==
            W_SEED_FRONTEND_GENERIC_SUBJECT_NONE &&
        range->generic_parameters[0].predicate_function_index ==
            W_SEED_FRONTEND_NONE &&
        has_fact(range, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));

  fixture *non_bool = &fixture_condition;
  CHECK(fixture_run(
      non_bool,
      "enum Stage { accepted completed }\n"
      "const fn invalid(stages: StaticList<Stage>): usize { return 1 }\n"
      "struct Invalid<_ stages: StaticList<Stage><(invalid(.member))>> {}\n"));
  CHECK(non_bool->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(non_bool, "W-CONTRACT-0003") &&
        !has_diagnostic(non_bool, "W-CONST-0004") &&
        non_bool->generic_parameters[0].refinement_kind ==
            W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID &&
        non_bool->generic_parameters[0].subject_kind ==
            W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID);
  const w_seed_frontend_diagnostic *non_bool_diagnostic =
      diagnostic_for_code(non_bool, "W-CONTRACT-0003");
  CHECK(non_bool_diagnostic != NULL &&
        non_bool_diagnostic->fact_count == 3u &&
        non_bool_diagnostic->label_count == 2u &&
        diagnostic_record_ranges_are_valid(non_bool, non_bool_diagnostic));
  CHECK(diagnostic_fact_string_is(non_bool, non_bool_diagnostic, 0u,
                                  "expectedType", "Bool"));
  CHECK(diagnostic_fact_string_is(non_bool, non_bool_diagnostic, 1u, "head",
                                  "Invalid"));
  CHECK(diagnostic_fact_string_is(non_bool, non_bool_diagnostic, 2u,
                                  "predicateType", "usize"));
  CHECK(diagnostic_label_role_is(non_bool, non_bool_diagnostic, 0u,
                                 "contract-head") &&
        diagnostic_label_role_is(non_bool, non_bool_diagnostic, 1u,
                                 "slot-declaration"));
  CHECK(non_bool->diagnostic_labels[non_bool_diagnostic->first_label]
            .document_index == 0u &&
        fixture_span_text_is(
            non_bool, 0u,
            non_bool->diagnostic_labels[non_bool_diagnostic->first_label].span,
            "Invalid") &&
        non_bool->diagnostic_labels[non_bool_diagnostic->first_label + 1u]
                .span.start_byte == non_bool->generic_parameters[0].span.start_byte &&
        non_bool->diagnostic_labels[non_bool_diagnostic->first_label + 1u]
                .span.end_byte == non_bool->generic_parameters[0].span.end_byte);

  fixture *non_const = &fixture_external;
  CHECK(fixture_run(
      non_const,
      "enum Stage { accepted completed }\n"
      "fn isValid(stages: StaticList<Stage>): Bool { return true }\n"
      "struct Invalid<_ stages: StaticList<Stage><(isValid(.member))>> {}\n"));
  CHECK(non_const->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(non_const, "W-CONST-0001") &&
        !has_diagnostic(non_const, "W-CONST-0004") &&
        non_const->generic_parameters[0].predicate_function_index == 0u &&
        non_const->generic_parameters[0].refinement_kind ==
            W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID &&
        non_const->generic_parameters[0].subject_kind ==
            W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID);
  const w_seed_frontend_diagnostic *non_const_diagnostic =
      diagnostic_for_code(non_const, "W-CONST-0001");
  static const char *const predicate_call_chain[] = {"isValid"};
  CHECK(non_const_diagnostic != NULL &&
        non_const_diagnostic->fact_count == 4u &&
        non_const_diagnostic->label_count == 1u &&
        diagnostic_record_ranges_are_valid(non_const, non_const_diagnostic));
  CHECK(diagnostic_fact_array_is(non_const, non_const_diagnostic, 0u,
                                 "callChain", predicate_call_chain, 1u));
  CHECK(diagnostic_fact_string_is(non_const, non_const_diagnostic, 1u,
                                  "operation", "call"));
  CHECK(diagnostic_fact_string_is(non_const, non_const_diagnostic, 2u,
                                  "reason", "not const-safe"));
  CHECK(diagnostic_fact_string_is(non_const, non_const_diagnostic, 3u,
                                  "symbol", "isValid"));
  CHECK(diagnostic_label_role_is(non_const, non_const_diagnostic, 0u,
                                 "const-owner"));

  fixture *malformed = &fixture_narrowing;
  CHECK(fixture_run(
      malformed,
      "enum Stage { accepted completed }\n"
      "const fn isValid(stages: StaticList<Stage>): Bool { return true }\n"
      "struct Invalid<_ stages: StaticList<Stage>"
      "<(isValid(.member) && true)>> {}\n"));
  CHECK(malformed->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(malformed, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION) &&
        malformed->generic_parameters[0].refinement_kind ==
            W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID &&
        malformed->generic_parameters[0].subject_kind ==
            W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID);

  static const char *const strict_shapes[] = {
      "isValid(.member, .member)",
      "wrapper(isValid(.member))",
  };
  for (size_t shape = 0; shape < sizeof(strict_shapes) / sizeof(strict_shapes[0]);
       shape += 1u) {
    char source[512];
    const int written = snprintf(
        source, sizeof(source),
        "enum Stage { accepted completed }\n"
        "const fn isValid(stages: StaticList<Stage>): Bool { return true }\n"
        "struct Invalid<_ stages: StaticList<Stage><(%s)>> {}\n",
        strict_shapes[shape]);
    CHECK(written > 0 && (size_t)written < sizeof(source));
    CHECK(fixture_run(malformed, source));
    CHECK(malformed->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
          has_fact(malformed, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION) &&
          malformed->generic_parameters[0].predicate_function_index ==
              W_SEED_FRONTEND_NONE &&
          malformed->generic_parameters[0].refinement_kind ==
              W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID &&
          malformed->generic_parameters[0].subject_kind ==
              W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID);
  }

  fixture *unresolved = &fixture_unresolved;
  CHECK(fixture_run(unresolved,
                    "struct Unknown<rows: UnknownName> {}\n"));
  CHECK(unresolved->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        unresolved->generic_parameters[0].kind ==
            W_SEED_FRONTEND_GENERIC_KIND_INVALID &&
        has_diagnostic(unresolved, "W-GENERIC-0001"));
  const w_seed_frontend_diagnostic *unresolved_domain =
      diagnostic_for_code(unresolved, "W-GENERIC-0001");
  CHECK(unresolved_domain != NULL && unresolved_domain->fact_count == 3u &&
        unresolved_domain->label_count == 1u &&
        diagnostic_record_ranges_are_valid(unresolved, unresolved_domain));
  CHECK(diagnostic_fact_string_is(unresolved, unresolved_domain, 0u, "domain",
                                  "UnknownName"));
  CHECK(diagnostic_fact_string_is(unresolved, unresolved_domain, 1u,
                                  "parameter", "rows"));
  CHECK(diagnostic_fact_string_is(unresolved, unresolved_domain, 2u,
                                  "resolutionReason", "unresolved-domain"));
  CHECK(diagnostic_label_role_is(unresolved, unresolved_domain, 0u,
                                 "generic-parameter"));
  CHECK(unresolved->diagnostic_labels[unresolved_domain->first_label]
            .document_index == 0u &&
        unresolved->diagnostic_labels[unresolved_domain->first_label]
                .span.start_byte == unresolved->generic_parameters[0].span.start_byte &&
        unresolved->diagnostic_labels[unresolved_domain->first_label]
                .span.end_byte == unresolved->generic_parameters[0].span.end_byte);

  CHECK(fixture_run(
      unresolved,
      "enum Stage { accepted completed }\n"
      "struct Invalid<_ stages: StaticList<Stage><(missing(.member))>> {}\n"));
  CHECK(unresolved->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(unresolved, W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL) &&
        unresolved->generic_parameters[0].predicate_function_index ==
            W_SEED_FRONTEND_NONE &&
        unresolved->generic_parameters[0].refinement_kind ==
            W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID &&
        unresolved->generic_parameters[0].subject_kind ==
            W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID);

  fixture *wrong_signature = &fixture_collision;
  CHECK(fixture_run(
      wrong_signature,
      "enum Stage { accepted completed }\n"
      "const fn wrongArity(a: StaticList<Stage>, b: StaticList<Stage>): Bool { "
      "return true }\n"
      "const fn wrongDomain(value: usize): Bool { return true }\n"
      "struct Arity<_ stages: StaticList<Stage><(wrongArity(.member))>> {}\n"
      "struct Domain<_ stages: StaticList<Stage><(wrongDomain(.member))>> {}\n"));
  CHECK(wrong_signature->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(wrong_signature, "W-CONTRACT-0002") &&
        wrong_signature->result.written.generic_parameters == 2u &&
        wrong_signature->generic_parameters[0].refinement_kind ==
            W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID &&
        wrong_signature->generic_parameters[0].subject_kind ==
            W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID &&
        wrong_signature->generic_parameters[1].refinement_kind ==
            W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID &&
        wrong_signature->generic_parameters[1].subject_kind ==
            W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID);
  const w_seed_frontend_diagnostic *wrong_arity =
      diagnostic_for_code_occurrence(wrong_signature, "W-CONTRACT-0002", 0u);
  const w_seed_frontend_diagnostic *wrong_domain =
      diagnostic_for_code_occurrence(wrong_signature, "W-CONTRACT-0002", 1u);
  CHECK(wrong_arity != NULL && wrong_domain != NULL &&
        wrong_arity->fact_count == 4u && wrong_arity->label_count == 2u &&
        wrong_domain->fact_count == 4u && wrong_domain->label_count == 2u &&
        diagnostic_record_ranges_are_valid(wrong_signature, wrong_arity) &&
        diagnostic_record_ranges_are_valid(wrong_signature, wrong_domain));
  CHECK(diagnostic_fact_string_is(wrong_signature, wrong_arity, 0u,
                                  "actualKind", "arity:2") &&
        diagnostic_fact_string_is(wrong_signature, wrong_arity, 1u,
                                  "expectedKind", "arity:1") &&
        diagnostic_fact_string_is(wrong_signature, wrong_arity, 2u, "head",
                                  "Arity") &&
        diagnostic_fact_string_is(wrong_signature, wrong_arity, 3u, "slot",
                                  "stages"));
  CHECK(diagnostic_label_role_is(wrong_signature, wrong_arity, 0u,
                                 "contract-head") &&
        diagnostic_label_role_is(wrong_signature, wrong_arity, 1u,
                                 "slot-declaration"));
  CHECK(wrong_signature->diagnostic_labels[wrong_arity->first_label]
            .document_index == 0u &&
        fixture_span_text_is(
            wrong_signature, 0u,
            wrong_signature->diagnostic_labels[wrong_arity->first_label].span,
            "Arity") &&
        wrong_signature
                ->diagnostic_labels[wrong_arity->first_label + 1u]
                .span.start_byte == wrong_signature->generic_parameters[0].span.start_byte &&
        wrong_signature
                ->diagnostic_labels[wrong_arity->first_label + 1u]
                .span.end_byte == wrong_signature->generic_parameters[0].span.end_byte);
  CHECK(diagnostic_fact_string_is(wrong_signature, wrong_domain, 0u,
                                  "actualKind", "value:usize") &&
        diagnostic_fact_string_is(wrong_signature, wrong_domain, 1u,
                                  "expectedKind", "value:StaticList<Stage>") &&
        diagnostic_fact_string_is(wrong_signature, wrong_domain, 2u, "head",
                                  "Domain") &&
        diagnostic_fact_string_is(wrong_signature, wrong_domain, 3u, "slot",
                                  "stages"));
  CHECK(diagnostic_label_role_is(wrong_signature, wrong_domain, 0u,
                                 "contract-head") &&
        diagnostic_label_role_is(wrong_signature, wrong_domain, 1u,
                                 "slot-declaration"));
  CHECK(wrong_signature->diagnostic_labels[wrong_domain->first_label]
            .document_index == 0u &&
        fixture_span_text_is(
            wrong_signature, 0u,
            wrong_signature->diagnostic_labels[wrong_domain->first_label].span,
            "Domain") &&
        wrong_signature
                ->diagnostic_labels[wrong_domain->first_label + 1u]
                .span.start_byte == wrong_signature->generic_parameters[1].span.start_byte &&
        wrong_signature
                ->diagnostic_labels[wrong_domain->first_label + 1u]
                .span.end_byte == wrong_signature->generic_parameters[1].span.end_byte);

  fixture *forward = &fixture_generic;
  CHECK(fixture_run(
      forward,
      "enum Stage { accepted completed }\n"
      "struct Forward<_ stages: StaticList<Stage><(isValid(.member))>> {}\n"
      "const fn isValid(stages: StaticList<Stage>): Bool { return true }\n"));
  CHECK(forward->result.status == W_SEED_FRONTEND_OK &&
        forward->generic_parameters[0].predicate_function_index == 0u &&
        forward->generic_parameters[0].predicate_function_span.start_byte >
            forward->generic_parameters[0].span.end_byte);
  CHECK(fixture_run(
      &fixture_callback,
      "struct First<A, count: usize> {}\n"
      "struct Second<_ value: usize> {}\n"));
  CHECK(fixture_callback.result.status == W_SEED_FRONTEND_OK &&
        fixture_callback.structs[0].first_generic_parameter == 0u &&
        fixture_callback.structs[0].generic_parameter_count == 2u &&
        fixture_callback.structs[1].first_generic_parameter == 2u &&
        fixture_callback.structs[1].generic_parameter_count == 1u &&
        fixture_callback.generic_parameters[0].owner_index == 0u &&
        fixture_callback.generic_parameters[0].ordinal == 0u &&
        fixture_callback.generic_parameters[1].owner_index == 0u &&
        fixture_callback.generic_parameters[1].ordinal == 1u &&
        fixture_callback.generic_parameters[2].owner_index == 1u &&
        fixture_callback.generic_parameters[2].ordinal == 0u);
  fixture *repeat = &fixture_callback;
  CHECK(fixture_run(repeat,
                    "enum Stage { accepted completed }\n"
                    "struct Forward<_ stages: StaticList<Stage>"
                    "<(isValid(.member))>> {}\n"
                    "const fn isValid(stages: StaticList<Stage>): Bool { "
                    "return true }\n"));
  CHECK(repeat->result.status == forward->result.status &&
        repeat->result.receipt_bytes == forward->result.receipt_bytes);
  CHECK(memcmp(repeat->receipt, forward->receipt, forward->result.receipt_bytes) ==
        0);
  return true;
}

static bool test_generic_applications(void) {
  fixture *forward = &fixture_generic;
  CHECK(fixture_run(
      forward,
      "type MatrixUse = Matrix<f32, rows: 3, columns: 4,>\n"
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"));
  CHECK(forward->result.status == W_SEED_FRONTEND_OK &&
        forward->result.written.generic_applications == 1u &&
        forward->result.written.generic_arguments == 3u &&
        forward->result.written.const_values == 2u);
  const w_seed_frontend_generic_application *matrix_application =
      &forward->generic_applications[0];
  CHECK(matrix_application->head_struct == 0u &&
        matrix_application->owner_type == 0u &&
        matrix_application->binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE &&
        forward->types[matrix_application->owner_type]
                .generic_application_index == 0u);
  CHECK(forward->generic_arguments[0].kind ==
            W_SEED_FRONTEND_GENERIC_ARGUMENT_TYPE &&
        forward->generic_arguments[0].parameter_index == 0u &&
        forward->generic_arguments[1].kind ==
            W_SEED_FRONTEND_GENERIC_ARGUMENT_VALUE &&
        forward->generic_arguments[1].label.length == 4u &&
        forward->generic_arguments[1].parameter_index == 1u &&
        forward->generic_arguments[2].parameter_index == 2u);
  CHECK(forward->const_values[0].kind == W_SEED_FRONTEND_CONST_INTEGER &&
        forward->const_values[0].integer_byte_count == 8u &&
        forward->const_values[0].integer_bytes[0] == 3u &&
        forward->const_values[1].integer_bytes[0] == 4u);
  CHECK(fixture_run(
      forward,
      "struct Inner<T> {}\n"
      "struct Outer<X> {}\n"
      "struct Use { let value: Outer<Inner<u8>> }\n"));
  CHECK(forward->result.status == W_SEED_FRONTEND_OK &&
        forward->result.written.generic_applications == 2u &&
        forward->result.written.generic_arguments == 2u &&
        forward->generic_applications[0].head_struct == 1u &&
        forward->generic_applications[1].head_struct == 0u &&
        forward->generic_arguments[0].type_index == 1u &&
        forward->generic_arguments[1].type_index == 2u);

  fixture *static_value = &fixture_label;
  CHECK(fixture_run(
      static_value,
      "struct StaticValue<T, _ value: T> {}\n"
      "struct Use { let a: StaticValue<Bool, true> "
      "let b: StaticValue<String, \"The final seating\"> }\n"));
  CHECK(static_value->result.status == W_SEED_FRONTEND_OK &&
        static_value->result.written.generic_applications == 2u &&
        static_value->result.written.const_values == 2u);
  CHECK(static_value->generic_parameters[1].kind ==
            W_SEED_FRONTEND_GENERIC_KIND_VALUE &&
        static_value->generic_parameters[1].domain_kind ==
            W_SEED_FRONTEND_GENERIC_DOMAIN_DEPENDENT &&
        static_value->generic_parameters[1].dependent_type_parameter_ordinal ==
            0u);
  CHECK(static_value->const_values[0].kind == W_SEED_FRONTEND_CONST_BOOL &&
        static_value->const_values[0].bool_value &&
        static_value->const_values[1].kind == W_SEED_FRONTEND_CONST_STRING &&
        static_value->const_values[1].byte_count == 17u &&
        memcmp(static_value->const_bytes + static_value->const_values[1].first_byte,
               "The final seating", 17u) == 0);

  fixture *stage = &fixture_condition;
  CHECK(fixture_run(
      stage,
      "enum ServiceStage { accepted completed }\n"
      "struct StagePath<_ stages: StaticList<ServiceStage>> {}\n"
      "struct Use { let a: StagePath<[.accepted, .accepted]> "
      "let b: StagePath<[]> let c: StagePath<[.accepted]> }\n"));
  CHECK(stage->result.status == W_SEED_FRONTEND_OK &&
        stage->result.written.generic_applications == 3u &&
        stage->result.written.generic_arguments == 3u &&
        stage->result.written.const_values == 6u &&
        stage->result.written.const_elements == 3u);
  CHECK(stage->const_values[0].kind == W_SEED_FRONTEND_CONST_STATIC_LIST &&
        stage->const_values[0].element_count == 2u &&
        stage->const_values[3].kind == W_SEED_FRONTEND_CONST_STATIC_LIST &&
        stage->const_values[3].element_count == 0u &&
        stage->const_values[3].first_element == W_SEED_FRONTEND_NONE &&
        stage->const_values[4].element_count == 1u &&
        stage->const_elements[0].owner_value == 0u &&
        stage->const_elements[1].ordinal == 1u &&
        stage->const_elements[2].owner_value == 4u);
  /* A StaticList generic argument points at its parent ConstValue.  This
   * keeps empty and non-empty lists on the same normalized relation. */
  CHECK(stage->generic_arguments[0].const_value_index == 0u &&
        stage->generic_arguments[1].const_value_index == 3u &&
        stage->generic_arguments[2].const_value_index == 4u);
  CHECK(stage->generic_arguments[0].label.length == 0u &&
        stage->generic_arguments[2].label.length == 0u &&
        stage->generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE &&
        stage->generic_applications[1].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE &&
        stage->generic_applications[2].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE);
  CHECK(fixture_run(
      stage,
      "enum ServiceStage { accepted completed }\n"
      "struct StagePath<_ stages: StaticList</* c */ ServiceStage >"
      "<(isValid(.member))>> {}\n"
      "const fn isValid(stages: StaticList</* c */ ServiceStage >): Bool { "
      "return true }\n"
      "struct Use { let value: StagePath<[.accepted]> }\n"));
  CHECK(stage->result.status == W_SEED_FRONTEND_OK &&
        stage->result.written.generic_applications == 1u &&
        stage->generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE &&
        stage->generic_applications[0].requires_const_evaluation);

  static const char *const invalid_sources[] = {
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, 3, columns: 4> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, rows: 3, 3> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, bogus: 3, columns: 4> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, rows: 3, rows: 4> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, rows: 3> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, rows: 3, columns: 4, 5> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<rows: f32, columns: 3> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, rows: 3, columns: "
      "18446744073709551616> }\n",
  };
  static const char *const invalid_codes[] = {
      "W-GENERIC-0003", "W-GENERIC-0003",
      "W-CONTRACT-0001", "W-CONTRACT-0004", "W-GENERIC-0002",
      "W-GENERIC-0003", "W-GENERIC-0003", "W-TYPE-0122",
  };
  for (size_t index = 0u;
       index < sizeof(invalid_sources) / sizeof(invalid_sources[0]);
       index += 1u) {
    CHECK(fixture_run(&fixture_collision, invalid_sources[index]));
    CHECK(fixture_collision.result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
          has_diagnostic(&fixture_collision, invalid_codes[index]) &&
          fixture_collision.result.written.generic_applications == 1u &&
          fixture_collision.generic_applications[0].binding_status ==
              W_SEED_FRONTEND_GENERIC_BINDING_INVALID);
  }

  CHECK(fixture_run(
      &fixture_collision,
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, columns: 4, rows: 3> }\n"));
  CHECK(fixture_collision.result.status == W_SEED_FRONTEND_OK &&
        fixture_collision.result.written.generic_applications == 1u &&
        fixture_collision.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE &&
        fixture_collision.generic_arguments[0].parameter_ordinal == 0u &&
        fixture_collision.generic_arguments[1].parameter_ordinal == 2u &&
        fixture_collision.generic_arguments[2].parameter_ordinal == 1u);

  CHECK(fixture_run(
      &fixture_collision,
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, bogus: 3, columns: 4> }\n"));
  {
    const w_seed_frontend_diagnostic *unknown_label =
        diagnostic_for_code(&fixture_collision, "W-CONTRACT-0001");
    static const char *const available_slots[] = {"columns", "rows"};
    CHECK(unknown_label != NULL && unknown_label->fact_count == 3u &&
          unknown_label->label_count == 1u &&
          diagnostic_record_ranges_are_valid(&fixture_collision,
                                             unknown_label));
    CHECK(diagnostic_fact_set_is(&fixture_collision, unknown_label, 0u,
                                 "availableSlots", available_slots, 2u) &&
          diagnostic_fact_string_is(&fixture_collision, unknown_label, 1u,
                                    "head", "Matrix") &&
          diagnostic_fact_string_is(&fixture_collision, unknown_label, 2u,
                                    "slot", "bogus"));
    CHECK(fixture_span_text_is(
              &fixture_collision, 0u, unknown_label->primary, "bogus: 3") &&
          diagnostic_label_role_is(&fixture_collision, unknown_label, 0u,
                                   "contract-head") &&
          fixture_collision.diagnostic_labels[unknown_label->first_label]
                  .document_index == 0u &&
          fixture_span_text_is(
              &fixture_collision, 0u,
              fixture_collision.diagnostic_labels[unknown_label->first_label]
                  .span,
              "Matrix"));
  }

  CHECK(fixture_run(
      &fixture_collision,
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, 3, columns: 4> }\n"));
  {
    const w_seed_frontend_diagnostic *required_label =
        diagnostic_for_code_occurrence(&fixture_collision, "W-GENERIC-0003",
                                       0u);
    CHECK(required_label != NULL && required_label->fact_count == 5u &&
          required_label->label_count == 1u &&
          diagnostic_record_ranges_are_valid(&fixture_collision,
                                             required_label));
    CHECK(diagnostic_fact_string_is(&fixture_collision, required_label, 0u,
                                    "externalLabel", "_") &&
          diagnostic_fact_string_is(&fixture_collision, required_label, 1u,
                                    "kind", "value") &&
          diagnostic_fact_string_is(&fixture_collision, required_label, 2u,
                                    "parameter", "rows") &&
          diagnostic_fact_integer_is(&fixture_collision, required_label, 3u,
                                     "position", 1) &&
          diagnostic_fact_string_is(&fixture_collision, required_label, 4u,
                                    "reason", "required-label-omitted"));
    CHECK(fixture_span_text_is(&fixture_collision, 0u,
                               required_label->primary, "3") &&
          diagnostic_label_role_is(&fixture_collision, required_label, 0u,
                                   "generic-parameter") &&
          fixture_collision.diagnostic_labels[required_label->first_label]
                  .document_index == 0u &&
          fixture_collision.diagnostic_labels[required_label->first_label]
                  .span.start_byte == fixture_collision.generic_parameters[1]
                                             .span.start_byte &&
          fixture_collision.diagnostic_labels[required_label->first_label]
                  .span.end_byte == fixture_collision.generic_parameters[1]
                                            .span.end_byte);
  }

  CHECK(fixture_run(
      &fixture_collision,
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, columns: 4, rows: 3> }\n"));
  CHECK(fixture_collision.result.status == W_SEED_FRONTEND_OK &&
        fixture_collision.generic_arguments[0].parameter_ordinal == 0u &&
        fixture_collision.generic_arguments[1].parameter_ordinal == 2u &&
        fixture_collision.generic_arguments[2].parameter_ordinal == 1u);

  CHECK(fixture_run(
      &fixture_collision,
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, rows: 3, 3> }\n"));
  {
    const w_seed_frontend_diagnostic *positional_after_named =
        diagnostic_for_code_occurrence(&fixture_collision, "W-GENERIC-0003",
                                       0u);
    CHECK(positional_after_named != NULL &&
          positional_after_named->fact_count == 5u &&
          positional_after_named->label_count == 1u &&
          diagnostic_record_ranges_are_valid(&fixture_collision,
                                             positional_after_named));
    CHECK(diagnostic_fact_string_is(&fixture_collision, positional_after_named,
                                    0u, "externalLabel", "_") &&
          diagnostic_fact_string_is(&fixture_collision, positional_after_named,
                                    1u, "kind", "value") &&
          diagnostic_fact_string_is(&fixture_collision, positional_after_named,
                                    2u, "parameter", "columns") &&
          diagnostic_fact_integer_is(&fixture_collision, positional_after_named,
                                     3u, "position", 2) &&
          diagnostic_fact_string_is(&fixture_collision, positional_after_named,
                                    4u, "reason", "required-label-omitted"));
    CHECK(fixture_span_text_is(&fixture_collision, 0u,
                               positional_after_named->primary, "3") &&
          diagnostic_label_role_is(&fixture_collision, positional_after_named,
                                   0u, "generic-parameter") &&
          fixture_collision.diagnostic_labels[
              positional_after_named->first_label].document_index == 0u &&
          fixture_collision.diagnostic_labels[
              positional_after_named->first_label]
                  .span.start_byte == fixture_collision.generic_parameters[2]
                                             .span.start_byte &&
          fixture_collision.diagnostic_labels[
              positional_after_named->first_label]
                  .span.end_byte == fixture_collision.generic_parameters[2]
                                            .span.end_byte);
  }

  CHECK(fixture_run(
      &fixture_collision,
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, rows: 3, columns: 4, 5> }\n"));
  {
    const w_seed_frontend_diagnostic *extra_argument =
        diagnostic_for_code(&fixture_collision, "W-GENERIC-0003");
    CHECK(fixture_collision.result.written.diagnostics == 1u &&
          extra_argument != NULL && extra_argument->fact_count == 5u &&
          extra_argument->label_count == 1u &&
          diagnostic_record_ranges_are_valid(&fixture_collision,
                                             extra_argument));
    CHECK(diagnostic_fact_string_is(&fixture_collision, extra_argument, 0u,
                                    "externalLabel", "_") &&
          diagnostic_fact_string_is(&fixture_collision, extra_argument, 1u,
                                    "kind", "value") &&
          diagnostic_fact_string_is(&fixture_collision, extra_argument, 2u,
                                    "parameter", "extra") &&
          diagnostic_fact_integer_is(&fixture_collision, extra_argument, 3u,
                                     "position", 3) &&
          diagnostic_fact_string_is(&fixture_collision, extra_argument, 4u,
                                    "reason", "extra-argument"));
    CHECK(fixture_span_text_is(&fixture_collision, 0u,
                               extra_argument->primary, "5") &&
          diagnostic_label_role_is(&fixture_collision, extra_argument, 0u,
                                   "generic-parameter") &&
          fixture_collision.diagnostic_labels[extra_argument->first_label]
                  .document_index == 0u &&
          fixture_span_text_is(
              &fixture_collision, 0u,
              fixture_collision.diagnostic_labels[extra_argument->first_label]
                  .span,
              "5"));
  }

  CHECK(fixture_run(&fixture_collision,
                    "struct Box<T> {}\n"
                    "struct Use { let value: Box<T: u8> }\n"));
  {
    const w_seed_frontend_diagnostic *type_label =
        diagnostic_for_code(&fixture_collision, "W-GENERIC-0003");
    CHECK(type_label != NULL && type_label->fact_count == 5u &&
          type_label->label_count == 1u &&
          diagnostic_record_ranges_are_valid(&fixture_collision, type_label));
    CHECK(diagnostic_fact_string_is(&fixture_collision, type_label, 0u,
                                    "externalLabel", "T") &&
          diagnostic_fact_string_is(&fixture_collision, type_label, 1u,
                                    "kind", "type") &&
          diagnostic_fact_string_is(&fixture_collision, type_label, 2u,
                                    "parameter", "T") &&
          diagnostic_fact_integer_is(&fixture_collision, type_label, 3u,
                                     "position", 0) &&
          diagnostic_fact_string_is(&fixture_collision, type_label, 4u,
                                    "reason", "type-parameter-must-be-positional"));
    CHECK(fixture_span_text_is(&fixture_collision, 0u, type_label->primary,
                               "T: u8") &&
          diagnostic_label_role_is(&fixture_collision, type_label, 0u,
                                   "generic-parameter") &&
          fixture_collision.diagnostic_labels[type_label->first_label]
                  .document_index == 0u &&
          fixture_collision.diagnostic_labels[type_label->first_label]
                  .span.start_byte == fixture_collision.generic_parameters[0]
                                             .span.start_byte &&
          fixture_collision.diagnostic_labels[type_label->first_label]
                  .span.end_byte == fixture_collision.generic_parameters[0]
                                            .span.end_byte);
  }

  CHECK(fixture_run(
      &fixture_collision,
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { let x: Matrix<f32, rows: 3> }\n"));
  {
    const w_seed_frontend_diagnostic *missing_slot =
        diagnostic_for_code(&fixture_collision, "W-GENERIC-0002");
    CHECK(fixture_collision.result.written.diagnostics == 1u &&
          missing_slot != NULL && missing_slot->fact_count == 4u &&
          missing_slot->label_count == 2u &&
          diagnostic_record_ranges_are_valid(&fixture_collision, missing_slot));
    CHECK(diagnostic_fact_set_is(&fixture_collision, missing_slot, 0u,
                                 "candidates", NULL, 0u) &&
          diagnostic_fact_set_is(&fixture_collision, missing_slot, 1u,
                                 "equationSources", NULL, 0u) &&
          diagnostic_fact_string_is(&fixture_collision, missing_slot, 2u,
                                    "parameter", "columns") &&
          diagnostic_fact_string_is(&fixture_collision, missing_slot, 3u,
                                    "reason", "missing-required-argument"));
    CHECK(diagnostic_label_role_is(&fixture_collision, missing_slot, 0u,
                                   "call-owner") &&
          diagnostic_label_role_is(&fixture_collision, missing_slot, 1u,
                                   "generic-parameter") &&
          fixture_collision.diagnostic_labels[missing_slot->first_label]
                  .document_index == 0u &&
          fixture_collision.diagnostic_labels[missing_slot->first_label].span
                  .start_byte ==
              fixture_collision.generic_applications[0].envelope_span.start_byte &&
          fixture_collision.diagnostic_labels[missing_slot->first_label].span
                  .end_byte ==
              fixture_collision.generic_applications[0].envelope_span.end_byte &&
          fixture_collision
                  .diagnostic_labels[missing_slot->first_label + 1u]
                  .document_index == 0u &&
          fixture_collision
                  .diagnostic_labels[missing_slot->first_label + 1u]
                  .span.start_byte == fixture_collision.generic_parameters[2]
                                             .span.start_byte &&
          fixture_collision
                  .diagnostic_labels[missing_slot->first_label + 1u]
                  .span.end_byte == fixture_collision.generic_parameters[2]
                                            .span.end_byte);
  }

  CHECK(fixture_run(
      &fixture_unresolved,
      "struct StaticValue<T, _ value: T> {}\n"
      "struct Use { let bad: StaticValue<f32, 0> }\n"));
  CHECK(fixture_unresolved.result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(&fixture_unresolved, "W-CONTRACT-0002") &&
        fixture_unresolved.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_INVALID);
  {
    const w_seed_frontend_diagnostic *value_kind =
        diagnostic_for_code(&fixture_unresolved, "W-CONTRACT-0002");
    CHECK(fixture_unresolved.result.written.diagnostics == 1u &&
          value_kind != NULL && value_kind->fact_count == 4u &&
          value_kind->label_count == 2u &&
          diagnostic_record_ranges_are_valid(&fixture_unresolved, value_kind));
    CHECK(diagnostic_fact_string_is(&fixture_unresolved, value_kind, 0u,
                                    "actualKind", "value:integer") &&
          diagnostic_fact_string_is(&fixture_unresolved, value_kind, 1u,
                                    "expectedKind", "value:f32") &&
          diagnostic_fact_string_is(&fixture_unresolved, value_kind, 2u,
                                    "head", "StaticValue") &&
          diagnostic_fact_string_is(&fixture_unresolved, value_kind, 3u,
                                    "slot", "value"));
    CHECK(fixture_span_text_is(&fixture_unresolved, 0u, value_kind->primary,
                               "0") &&
          diagnostic_label_role_is(&fixture_unresolved, value_kind, 0u,
                                   "contract-head") &&
          diagnostic_label_role_is(&fixture_unresolved, value_kind, 1u,
                                   "slot-declaration") &&
          fixture_unresolved.diagnostic_labels[value_kind->first_label]
                  .document_index == 0u &&
          fixture_span_text_is(
              &fixture_unresolved, 0u,
              fixture_unresolved.diagnostic_labels[value_kind->first_label]
                  .span,
              "StaticValue") &&
          fixture_unresolved
                  .diagnostic_labels[value_kind->first_label + 1u]
                  .document_index == 0u &&
          fixture_unresolved
                  .diagnostic_labels[value_kind->first_label + 1u]
                  .span.start_byte == fixture_unresolved.generic_parameters[1]
                                             .span.start_byte &&
          fixture_unresolved
                  .diagnostic_labels[value_kind->first_label + 1u]
                  .span.end_byte == fixture_unresolved.generic_parameters[1]
                                            .span.end_byte);
  }

  CHECK(fixture_run(
      &fixture_external,
      "struct Box<T> {}\n"
      "type Bad = Box<true>\n"));
  CHECK(fixture_external.result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(&fixture_external, "W-CONTRACT-0002") &&
        fixture_external.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_INVALID);
  {
    const w_seed_frontend_diagnostic *type_kind =
        diagnostic_for_code(&fixture_external, "W-CONTRACT-0002");
    CHECK(fixture_external.result.written.diagnostics == 1u &&
          type_kind != NULL && type_kind->fact_count == 4u &&
          type_kind->label_count == 2u &&
          diagnostic_record_ranges_are_valid(&fixture_external, type_kind));
    CHECK(diagnostic_fact_string_is(&fixture_external, type_kind, 0u,
                                    "actualKind", "value:Bool") &&
          diagnostic_fact_string_is(&fixture_external, type_kind, 1u,
                                    "expectedKind", "type") &&
          diagnostic_fact_string_is(&fixture_external, type_kind, 2u, "head",
                                    "Box") &&
          diagnostic_fact_string_is(&fixture_external, type_kind, 3u, "slot",
                                    "T"));
    CHECK(fixture_span_text_is(&fixture_external, 0u, type_kind->primary,
                               "true") &&
          diagnostic_label_role_is(&fixture_external, type_kind, 0u,
                                   "contract-head") &&
          diagnostic_label_role_is(&fixture_external, type_kind, 1u,
                                   "slot-declaration") &&
          fixture_external.diagnostic_labels[type_kind->first_label]
                  .document_index == 0u &&
          fixture_span_text_is(
              &fixture_external, 0u,
              fixture_external.diagnostic_labels[type_kind->first_label].span,
              "Box") &&
          fixture_external
                  .diagnostic_labels[type_kind->first_label + 1u]
                  .document_index == 0u &&
          fixture_external
                  .diagnostic_labels[type_kind->first_label + 1u]
                  .span.start_byte == fixture_external.generic_parameters[0]
                                             .span.start_byte &&
          fixture_external
                  .diagnostic_labels[type_kind->first_label + 1u]
                  .span.end_byte == fixture_external.generic_parameters[0]
                                            .span.end_byte);
  }

  CHECK(fixture_run(
      &fixture_narrowing,
      "struct Plain {}\n"
      "struct Use { let value: Plain<true> }\n"));
  CHECK(fixture_narrowing.result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        fixture_narrowing.result.written.generic_applications == 1u &&
        fixture_narrowing.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_INVALID &&
        has_diagnostic(&fixture_narrowing, "W-GENERIC-0003"));

  CHECK(fixture_run(
      &fixture_generic,
      "enum ServiceStage { accepted completed }\n"
      "enum OtherStage { accepted }\n"
      "struct StagePath<_ stages: StaticList<ServiceStage>> {}\n"
      "struct Use { let value: StagePath<[OtherStage.accepted]> }\n"));
  CHECK(fixture_generic.result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        fixture_generic.result.written.generic_applications == 1u &&
        fixture_generic.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_INVALID &&
        fixture_generic.const_values[0].kind ==
            W_SEED_FRONTEND_CONST_INVALID &&
        fixture_generic.const_values[1].kind ==
            W_SEED_FRONTEND_CONST_INVALID);

  CHECK(fixture_run(
      &fixture_label,
      "struct Holder<value: u64> {}\n"
      "struct Use { let a: Holder<value: 1> let b: Holder<value: 1_u8> }\n"));
  CHECK(fixture_label.result.status == W_SEED_FRONTEND_OK &&
        fixture_label.result.written.const_values == 2u &&
        fixture_label.const_values[0].integer_bit_width == 64u &&
        fixture_label.const_values[1].integer_bit_width == 64u &&
        fixture_label.const_values[0].integer_byte_count == 8u &&
        fixture_label.const_values[1].integer_byte_count == 8u &&
        memcmp(fixture_label.const_values[0].integer_bytes,
               fixture_label.const_values[1].integer_bytes, 8u) == 0);

  CHECK(fixture_run(
      &fixture_collision,
      "struct Duplicate<T, T> {}\n"
      "struct Use { let value: Duplicate<u8, u8> }\n"));
  CHECK(fixture_collision.result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(&fixture_collision, "W-CONTRACT-0004") &&
        fixture_collision.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_INVALID);
  const w_seed_frontend_diagnostic *duplicate_slot =
      diagnostic_for_code(&fixture_collision, "W-CONTRACT-0004");
  static const char *const duplicate_slot_order[] = {"T", "T"};
  CHECK(duplicate_slot != NULL && duplicate_slot->fact_count == 4u &&
        duplicate_slot->label_count == 2u &&
        diagnostic_record_ranges_are_valid(&fixture_collision,
                                           duplicate_slot));
  CHECK(diagnostic_fact_string_is(&fixture_collision, duplicate_slot, 0u,
                                  "head", "Duplicate") &&
        diagnostic_fact_string_is(&fixture_collision, duplicate_slot, 1u,
                                  "slot", "T") &&
        diagnostic_fact_array_is(&fixture_collision, duplicate_slot, 2u,
                                 "slotOrder", duplicate_slot_order, 2u) &&
        diagnostic_fact_string_is(&fixture_collision, duplicate_slot, 3u,
                                  "violation", "duplicate"));
  CHECK(diagnostic_label_role_is(&fixture_collision, duplicate_slot, 0u,
                                 "contract-head") &&
        diagnostic_label_role_is(&fixture_collision, duplicate_slot, 1u,
                                 "slot-declaration"));
  CHECK(fixture_collision.diagnostic_labels[duplicate_slot->first_label]
            .document_index == 0u &&
        fixture_span_text_is(
            &fixture_collision, 0u,
            fixture_collision
                .diagnostic_labels[duplicate_slot->first_label]
                .span,
            "Duplicate") &&
        fixture_collision
                .diagnostic_labels[duplicate_slot->first_label + 1u]
                .span.start_byte == fixture_collision.generic_parameters[1].span.start_byte &&
        fixture_collision
                .diagnostic_labels[duplicate_slot->first_label + 1u]
                .span.end_byte == fixture_collision.generic_parameters[1].span.end_byte);

  CHECK(fixture_run(
      &fixture_collision,
      "struct S<T> {}\n"
      "type Use = S<u8><(true)>\n"));
  CHECK(fixture_collision.result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(&fixture_collision, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE) &&
        fixture_collision.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED);

  CHECK(fixture_run(
      &fixture_collision,
      "struct S<value: String> {}\n"
      "struct Use { let value: S<value: \"a\\\\n\"> }\n"));
  CHECK(fixture_collision.result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        fixture_collision.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED &&
        has_fact(&fixture_collision, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE));

  CHECK(fixture_run(
      &fixture_collision,
      "struct S<value: String> {}\n"
      "struct Use { let value: S<bad: \"a\\\\n\"> }\n"));
  CHECK(fixture_collision.result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(&fixture_collision, "W-CONTRACT-0001") &&
        fixture_collision.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_INVALID);

  CHECK(fixture_run(
      &fixture_collision,
      "struct S<T> {}\n"
      "struct Use { let value: S<u8> }\n"));
  CHECK(fixture_collision.result.status == W_SEED_FRONTEND_OK &&
        fixture_collision.result.written.generic_applications == 1u &&
        fixture_collision.types[fixture_collision.generic_applications[0].owner_type]
                .generic_application_index == 0u);
  return true;
}

static bool test_typed_const_expressions(void) {
  static const char source[] =
      "const fn isUltimateAnswer(value: i64): Bool { return value == 42 }\n"
      "struct UltimateAnswer<_ value: i64<(isUltimateAnswer(.member))>> {}\n"
      "struct Use { let immediate: UltimateAnswer<42> let computed: "
      "UltimateAnswer<(6 * 7)> }\n";
  fixture *value = &fixture_const;
  CHECK(fixture_run(value, source));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.generic_applications == 2u &&
        value->result.written.generic_arguments == 2u &&
        value->result.written.typed_const_expressions == 1u);
  uint32_t pending_application = W_SEED_FRONTEND_NONE;
  uint32_t immediate_application = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.generic_applications;
       index += 1u) {
    const w_seed_frontend_generic_application *application =
        &value->generic_applications[index];
    if (application->binding_status ==
        W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST)
      pending_application = (uint32_t)index;
    else if (application->binding_status ==
             W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE)
      immediate_application = (uint32_t)index;
  }
  CHECK(pending_application != W_SEED_FRONTEND_NONE &&
        immediate_application != W_SEED_FRONTEND_NONE);
  const w_seed_frontend_generic_argument *immediate =
      &value->generic_arguments[
          value->generic_applications[immediate_application].first_argument];
  const w_seed_frontend_generic_argument *pending =
      &value->generic_arguments[
          value->generic_applications[pending_application].first_argument];
  CHECK(immediate->binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE &&
        immediate->const_value_index != W_SEED_FRONTEND_NONE &&
        immediate->typed_const_expression_index == W_SEED_FRONTEND_NONE &&
        pending->binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST &&
        pending->const_value_index == W_SEED_FRONTEND_NONE &&
        pending->typed_const_expression_index == 0u &&
        value->generic_applications[pending_application]
            .requires_const_evaluation);
  const w_seed_frontend_typed_const_expression *typed =
      &value->typed_const_expressions[0];
  CHECK(typed->owner_application == pending_application &&
        typed->argument_ordinal == 0u &&
        typed->expression_index != W_SEED_FRONTEND_NONE &&
        typed->expected_type != W_SEED_FRONTEND_NONE &&
        typed->effective_type != W_SEED_FRONTEND_NONE &&
        typed->span.start_byte < typed->span.end_byte &&
        receipt_contains(value, "typed-const-expression=",
                         strlen("typed-const-expression=")));

  static const char unsupported_identifier[] =
      "struct Box<_ value: i64> {}\n"
      "struct Use { let value: Box<(unknownValue)> }\n";
  CHECK(fixture_run(value, unsupported_identifier));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        value->result.written.typed_const_expressions == 0u &&
        value->generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED &&
        value->generic_arguments[0].typed_const_expression_index ==
            W_SEED_FRONTEND_NONE &&
        has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE));

  static const char unsupported_string[] =
      "struct Box<_ value: i64> {}\n"
      "struct Use { let value: Box<(\"42\")> }\n";
  CHECK(fixture_run(value, unsupported_string));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        value->result.written.typed_const_expressions == 0u &&
        value->generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED &&
        value->generic_arguments[0].typed_const_expression_index ==
            W_SEED_FRONTEND_NONE);

  CHECK(fixture_parse(value, source));
  const uint8_t sentinel = 0xa5u;
  fixture_fill_output(value, sentinel);
  value->output.typed_const_expression_capacity = 0u;
  value->output.typed_const_expressions = NULL;
  (void)w_seed_frontend_run(&value->input, &value->output, &value->result);
  CHECK(value->result.status == W_SEED_FRONTEND_CAPACITY &&
        value->result.required.typed_const_expressions == 1u &&
        fixture_output_is(value, sentinel, true));
  return true;
}

static bool test_string_expression_projection(void) {
  static const char source[] =
      "const fn equals(value: String): Bool { return value == \"a\" }\n"
      "const fn empty(value: String): Bool { return value == \"\" }\n"
      "struct Text<_ value: String> {}\n"
      "struct Use { let one: Text<\"a\"> let zero: Text<\"\"> }\n";
  fixture *value = &fixture_const;
  CHECK(fixture_run(value, source));
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  bool saw_one = false;
  bool saw_empty = false;
  size_t string_expression_count = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_STRING) {
      string_expression_count += 1u;
      CHECK(expression->inferred_type != W_SEED_FRONTEND_NONE &&
            expression->const_byte_offset != W_SEED_FRONTEND_NONE &&
            expression->const_byte_offset <= value->result.written.const_bytes &&
            expression->const_byte_count <=
                value->result.written.const_bytes -
                    expression->const_byte_offset);
      if (expression->const_byte_count == 0u) {
        saw_empty = true;
      } else {
        CHECK(expression->const_byte_count == 1u &&
              value->const_bytes[expression->const_byte_offset] == 'a');
        saw_one = true;
      }
    } else {
      CHECK(expression->const_byte_offset == W_SEED_FRONTEND_NONE &&
            expression->const_byte_count == 0u);
    }
  }
  CHECK(string_expression_count == 2u && saw_one && saw_empty);

  /* The const-byte arena is part of the same measured transaction. */
  const uint8_t sentinel = 0xa5u;
  CHECK(fixture_parse(&fixture_capacity, source));
  fixture_fill_output(&fixture_capacity, sentinel);
  fixture_capacity.output.const_bytes_capacity = 0u;
  fixture_capacity.output.const_bytes = NULL;
  (void)w_seed_frontend_run(&fixture_capacity.input,
                            &fixture_capacity.output,
                            &fixture_capacity.result);
  CHECK(fixture_capacity.result.status == W_SEED_FRONTEND_CAPACITY &&
        fixture_output_is(&fixture_capacity, sentinel, true));

  CHECK(fixture_run(
      value,
      "const fn equals(value: String): Bool { return value == \"a\\\\n\" }\n"));
  CHECK(value->result.status != W_SEED_FRONTEND_OK &&
        has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    if (value->expressions[index].kind == W_SEED_FRONTEND_EXPR_STRING)
      CHECK(!value->expressions[index].supported &&
            value->expressions[index].const_byte_offset ==
                W_SEED_FRONTEND_NONE &&
            value->expressions[index].const_byte_count == 0u);
  }
  return true;
}

static bool test_interpolated_string_projection(void) {
  static const char source[] =
      "fn answer(): String { return \"The answer is ${6 * 7}\" }\n";
  fixture *value = &fixture_const;
  CHECK(fixture_run(value, source));
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  CHECK(value->result.written.interpolation_segments == 2u);
  CHECK(value->result.written.const_bytes == 14u);

  uint32_t interpolation_index = W_SEED_FRONTEND_NONE;
  uint32_t binary_index = W_SEED_FRONTEND_NONE;
  size_t integer_count = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING) {
      interpolation_index = (uint32_t)index;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_BINARY) {
      binary_index = (uint32_t)index;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_INTEGER) {
      integer_count += 1u;
    }
  }
  CHECK(interpolation_index != W_SEED_FRONTEND_NONE);
  CHECK(binary_index != W_SEED_FRONTEND_NONE);
  CHECK(integer_count == 2u);
  const w_seed_frontend_expression *interpolation =
      &value->expressions[interpolation_index];
  CHECK(interpolation->supported &&
        interpolation->first_interpolation_segment == 0u &&
        interpolation->interpolation_segment_count == 2u);
  const w_seed_frontend_interpolation_segment *text =
      &value->interpolation_segments[0];
  const w_seed_frontend_interpolation_segment *expression =
      &value->interpolation_segments[1];
  CHECK(text->kind == W_SEED_FRONTEND_INTERPOLATION_TEXT &&
        text->owner_expression == interpolation_index && text->ordinal == 0u &&
        text->expression_index == W_SEED_FRONTEND_NONE &&
        text->const_byte_offset == 0u && text->const_byte_count == 14u &&
        memcmp(value->const_bytes, "The answer is ", 14u) == 0);
  CHECK(expression->kind == W_SEED_FRONTEND_INTERPOLATION_EXPRESSION &&
        expression->owner_expression == interpolation_index &&
        expression->ordinal == 1u &&
        expression->expression_index == binary_index &&
        expression->const_byte_offset == W_SEED_FRONTEND_NONE &&
        expression->const_byte_count == 0u);
  const uint32_t integer_type = value->expressions[binary_index].inferred_type;
  CHECK(integer_type != W_SEED_FRONTEND_NONE &&
        integer_type < value->result.written.types &&
        value->types[integer_type].kind == W_SEED_FRONTEND_TYPE_INTEGER &&
        value->types[integer_type].is_signed &&
        value->types[integer_type].bit_width == 64u);
  CHECK(value->expressions[value->expressions[binary_index].left].inferred_type ==
            integer_type &&
        value->expressions[value->expressions[binary_index].right].inferred_type ==
            integer_type);

  const uint8_t sentinel = 0xa5u;
  CHECK(fixture_parse(&fixture_capacity, source));
  fixture_fill_output(&fixture_capacity, sentinel);
  fixture_capacity.output.interpolation_segments = NULL;
  fixture_capacity.output.interpolation_segment_capacity = 0u;
  (void)w_seed_frontend_run(&fixture_capacity.input,
                            &fixture_capacity.output,
                            &fixture_capacity.result);
  CHECK(fixture_capacity.result.status == W_SEED_FRONTEND_CAPACITY &&
        fixture_capacity.result.required.interpolation_segments == 2u &&
        fixture_output_is(&fixture_capacity, sentinel, true));

  static const char direct_negative_source[] =
      "fn balance(): String { return \"Balance ${-7}\" }\n";
  value = &fixture_literal;
  CHECK(fixture_run(value, direct_negative_source));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.interpolation_segments == 2u);
  uint32_t negative_literal = W_SEED_FRONTEND_NONE;
  uint32_t negative_unary = W_SEED_FRONTEND_NONE;
  uint32_t negative_interpolation = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *item = &value->expressions[index];
    if (item->kind == W_SEED_FRONTEND_EXPR_INTEGER)
      negative_literal = (uint32_t)index;
    else if (item->kind == W_SEED_FRONTEND_EXPR_UNARY)
      negative_unary = (uint32_t)index;
    else if (item->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING)
      negative_interpolation = (uint32_t)index;
  }
  CHECK(negative_literal != W_SEED_FRONTEND_NONE &&
        negative_unary != W_SEED_FRONTEND_NONE &&
        negative_interpolation != W_SEED_FRONTEND_NONE);
  const uint32_t negative_type =
      value->expressions[negative_unary].inferred_type;
  CHECK(negative_type != W_SEED_FRONTEND_NONE &&
        negative_type < value->result.written.types &&
        value->types[negative_type].kind == W_SEED_FRONTEND_TYPE_INTEGER &&
        value->types[negative_type].is_signed &&
        value->types[negative_type].bit_width == 64u &&
        value->expressions[negative_literal].inferred_type == negative_type &&
        value->expressions[negative_unary].left == negative_literal &&
        frontend_text_is(value->expressions[negative_unary].operator_text,
                         "-") &&
        value->interpolation_segments[1].expression_index == negative_unary &&
        value->interpolation_segments[1].owner_expression ==
            negative_interpolation);

  static const char builtin_source[] =
      "fn main() { let state = \"open\" "
      "print(\"${true}/${false}/${state}\") }\nentry(main)\n";
  value = &fixture_literal;
  CHECK(fixture_parse(value, builtin_source));
  fixture_configure_print_host(value);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  size_t bool_count = 0u;
  size_t binding_read_count = 0u;
  uint32_t bool_type = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *item = &value->expressions[index];
    if (item->kind == W_SEED_FRONTEND_EXPR_BOOL) {
      CHECK(item->supported && item->has_bool_value &&
            !item->has_integer_value &&
            item->inferred_type != W_SEED_FRONTEND_NONE &&
            (size_t)item->inferred_type < value->result.written.types &&
            value->types[item->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_BOOL);
      if (bool_type == W_SEED_FRONTEND_NONE)
        bool_type = item->inferred_type;
      else
        CHECK(item->inferred_type == bool_type);
      bool_count += 1u;
    }
    if (item->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        frontend_text_is(item->spelling, "state")) {
      CHECK(item->supported && item->resolved_binding_statement == 0u &&
            item->inferred_type == value->statements[0].effective_type);
      binding_read_count += 1u;
    }
  }
  CHECK(bool_count == 2u && binding_read_count == 1u &&
        bool_type != W_SEED_FRONTEND_NONE);

  static const char typed_binding_source[] =
      "fn serve() { let table = 6 * 7 let isOpen = true "
      "let state = \"open\" "
      "print(\"Table ${table}; open: ${isOpen}; state: ${state}\") }\n"
      "entry(serve)\n";
  value = &fixture_literal;
  CHECK(fixture_parse(value, typed_binding_source));
  fixture_configure_print_host(value);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.written.statements == 4u &&
        value->statements[0].effective_type != W_SEED_FRONTEND_NONE &&
        value->types[value->statements[0].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        value->types[value->statements[0].effective_type].is_signed &&
        value->types[value->statements[0].effective_type].bit_width == 64u &&
        value->types[value->statements[1].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_BOOL &&
        value->types[value->statements[2].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_STRING);
  size_t typed_reads = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *item = &value->expressions[index];
    if (item->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
        item->resolved_binding_statement == W_SEED_FRONTEND_NONE)
      continue;
    CHECK(item->resolved_binding_statement < 3u &&
          item->inferred_type ==
              value->statements[item->resolved_binding_statement]
                  .effective_type);
    typed_reads += 1u;
  }
  CHECK(typed_reads == 3u);
  return true;
}

typedef enum {
  TEST_GENERIC_CAPACITY_APPLICATION = 0,
  TEST_GENERIC_CAPACITY_ARGUMENT,
  TEST_GENERIC_CAPACITY_CONST_VALUE,
  TEST_GENERIC_CAPACITY_CONST_ELEMENT,
  TEST_GENERIC_CAPACITY_CONST_BYTES,
} test_generic_capacity_target;

static bool test_generic_capacity_target_run(
    fixture *value, test_generic_capacity_target target) {
  static const char source[] =
      "enum Stage { accepted }\n"
      "struct Text<value: String> {}\n"
      "struct Path<stages: StaticList<Stage>> {}\n"
      "struct Use { let text: Text<value: \"x\"> let path: Path<stages: [.accepted]> }\n";
  const uint8_t sentinel = 0xa5u;
  CHECK(fixture_parse(value, source));
  fixture_fill_output(value, sentinel);
  switch (target) {
    case TEST_GENERIC_CAPACITY_APPLICATION:
      value->output.generic_application_capacity = 0u;
      value->output.generic_applications = NULL;
      break;
    case TEST_GENERIC_CAPACITY_ARGUMENT:
      value->output.generic_argument_capacity = 0u;
      value->output.generic_arguments = NULL;
      break;
    case TEST_GENERIC_CAPACITY_CONST_VALUE:
      value->output.const_value_capacity = 0u;
      value->output.const_values = NULL;
      break;
    case TEST_GENERIC_CAPACITY_CONST_ELEMENT:
      value->output.const_element_capacity = 0u;
      value->output.const_elements = NULL;
      break;
    case TEST_GENERIC_CAPACITY_CONST_BYTES:
      value->output.const_bytes_capacity = 0u;
      value->output.const_bytes = NULL;
      break;
  }
  (void)w_seed_frontend_run(&value->input, &value->output, &value->result);
  CHECK(value->result.status == W_SEED_FRONTEND_CAPACITY);
  CHECK(fixture_output_is(value, sentinel, true));
  return true;
}

static bool test_barrier_and_capacity(void) {
  fixture *recovered = &fixture_recovered;
  CHECK(fixture_parse(recovered, "fn f(): () { if 1 { return }\n"));
  CHECK(recovered->parse.status != W_SEED_PARSE_COMPLETE);
  const uint8_t sentinel = 0xa5u;
  fixture_fill_output(recovered, sentinel);
  (void)w_seed_frontend_run(&recovered->input, &recovered->output,
                            &recovered->result);
  CHECK(recovered->result.status == W_SEED_FRONTEND_BARRIER);
  CHECK(fixture_output_is(recovered, sentinel, true));

  fixture *capacity = &fixture_capacity;
  CHECK(fixture_parse(capacity, "fn f(): u32 { return 1 }\nentry(f)\n"));
  capacity->output.module_capacity = 0;
  capacity->output.modules = NULL;
  fixture_fill_output(capacity, sentinel);
  capacity->output.module_capacity = 0;
  capacity->output.modules = NULL;
  (void)w_seed_frontend_run(&capacity->input, &capacity->output,
                            &capacity->result);
  CHECK(capacity->result.status == W_SEED_FRONTEND_CAPACITY);
  CHECK(fixture_output_is(capacity, sentinel, false));

  CHECK(fixture_parse(capacity,
                      "enum E { ready(Value) failed(reason: Error) }\n"));
  fixture_fill_output(capacity, sentinel);
  /* Restore the module slot from the previous zero-capacity probe.  The
   * enum/case/payload capacities below must be the barrier that prevents the
   * emit pass, not a stale module pointer. */
  capacity->output.module_capacity = TEST_MODULES;
  capacity->output.modules = capacity->modules;
  capacity->output.enum_capacity = 0;
  capacity->output.enums = NULL;
  capacity->output.enum_case_capacity = 0;
  capacity->output.enum_cases = NULL;
  capacity->output.enum_case_parameter_capacity = 0;
  capacity->output.enum_case_parameters = NULL;
  (void)w_seed_frontend_run(&capacity->input, &capacity->output,
                            &capacity->result);
  CHECK(capacity->result.status == W_SEED_FRONTEND_CAPACITY);
  CHECK(fixture_output_is(capacity, sentinel, true));

  CHECK(fixture_parse(
      capacity,
      "enum Stage { accepted completed }\n"
      "struct Path<_ stages: StaticList<Stage>> {}\n"));
  fixture_fill_output(capacity, sentinel);
  capacity->output.generic_parameter_capacity = 0;
  capacity->output.generic_parameters = NULL;
  (void)w_seed_frontend_run(&capacity->input, &capacity->output,
                            &capacity->result);
  CHECK(capacity->result.status == W_SEED_FRONTEND_CAPACITY);
  CHECK(fixture_output_is(capacity, sentinel, true));

  CHECK(fixture_parse(
      capacity,
      "enum Stage { accepted reserving preparing }\n"
      "fn label(stage: Stage): String { return switch stage { "
      "case .accepted: \"A\" case .reserving: \"R\" case .preparing: \"P\" } }\n"));
  fixture_fill_output(capacity, sentinel);
  capacity->output.switch_arm_capacity = 0;
  capacity->output.switch_arms = NULL;
  (void)w_seed_frontend_run(&capacity->input, &capacity->output,
                            &capacity->result);
  CHECK(capacity->result.status == W_SEED_FRONTEND_CAPACITY);
  CHECK(fixture_output_is(capacity, sentinel, true));

  fixture *cycle = &fixture_capacity;
  CHECK(fixture_parse(cycle, "fn f(): () { if true { return } }\nentry(f)\n"));
  uint32_t block_index = W_SEED_CST_NONE;
  for (size_t index = 0; index < cycle->parse.node_count; index += 1) {
    if (cycle->nodes[index].kind == W_SEED_CST_BLOCK) {
      block_index = (uint32_t)index;
      break;
    }
  }
  CHECK(block_index != W_SEED_CST_NONE);
  cycle->nodes[block_index].first_child = block_index;
  fixture_fill_output(cycle, sentinel);
  (void)w_seed_frontend_run(&cycle->input, &cycle->output, &cycle->result);
  CHECK(cycle->result.status == W_SEED_FRONTEND_INVALID);
  CHECK(fixture_output_is(cycle, sentinel, true));
  for (int target = TEST_GENERIC_CAPACITY_APPLICATION;
       target <= TEST_GENERIC_CAPACITY_CONST_BYTES; target += 1) {
    CHECK(test_generic_capacity_target_run(
        &fixture_capacity, (test_generic_capacity_target)target));
  }
  return true;
}

static bool test_scalar_type_measure_emit_parity(void) {
  static const char *const sources[] = {
      "fn main() { print(\"${3 == 3}\") }\nentry(main)\n",
      "fn main() { let guests = 3 print(\"${guests}\") }\nentry(main)\n",
      "fn main() { let fits = 3 <= 4 print(\"${fits}\") }\nentry(main)\n",
      "fn main() { let guests = 3 let seats = 4 let fits = guests <= seats "
      "print(\"${fits}\") if guests <= seats { print(\"Seat party\") } "
      "else { print(\"Waitlist\") } }\nentry(main)\n",
  };
  fixture *value = &fixture_literal;
  for (size_t source = 0u; source < sizeof(sources) / sizeof(sources[0]); source += 1u) {
    CHECK(fixture_parse(value, sources[source]));
    fixture_configure_print_host(value);
    CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
          W_SEED_FRONTEND_OK);
    CHECK(counts_equal(&value->result.required, &value->result.written));
    CHECK(value->result.receipt_bytes == value->result.required.receipt_bytes);
    for (size_t index = 0u; index < value->result.written.expressions; index += 1u) {
      const w_seed_frontend_expression *expression = &value->expressions[index];
      if (source == 3u && expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
          (frontend_text_is(expression->spelling, "guests") ||
           frontend_text_is(expression->spelling, "seats"))) {
        CHECK(expression->supported && expression->resolved_binding_statement < 2u);
        CHECK(frontend_text_is(expression->spelling, "guests")
                  ? expression->resolved_binding_statement == 0u
                  : expression->resolved_binding_statement == 1u);
      }
      if (expression->kind != W_SEED_FRONTEND_EXPR_INTEGER) continue;
      CHECK(expression->inferred_type != W_SEED_FRONTEND_NONE &&
            expression->inferred_type < value->result.written.types);
      const w_seed_frontend_type *type = &value->types[expression->inferred_type];
      CHECK(type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            type->bit_width == 64u && type->is_signed);
    }
    const size_t required_types = value->result.required.types;
    CHECK(required_types > 0u);
    CHECK(fixture_parse(value, sources[source]));
    fixture_configure_print_host(value);
    fixture_fill_output(value, 0xa5u);
    value->output.type_capacity = required_types - 1u;
    CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
          W_SEED_FRONTEND_CAPACITY);
    CHECK(fixture_output_is(value, 0xa5u, true));
  }
  return true;
}

static bool test_checked_shift_frontend_matrix(void) {
  typedef struct {
    const char *name;
    bool is_signed;
    uint16_t bit_width;
  } integer_case;
  static const integer_case INTEGERS[] = {
      {"i8", true, 8u},     {"u8", false, 8u},
      {"i16", true, 16u},   {"u16", false, 16u},
      {"i32", true, 32u},   {"u32", false, 32u},
      {"i64", true, 64u},   {"u64", false, 64u},
      {"Int", true, 64u},   {"UInt", false, 64u},
  };
  fixture *value = &fixture_literal;
  char source[768];
  for (size_t integer_index = 0u;
       integer_index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       integer_index += 1u) {
    const integer_case *integer = &INTEGERS[integer_index];
    const int written = snprintf(
        source, sizeof(source),
        "fn left(value: %s, count: UInt): %s { return value << count }\n"
        "fn right(value: %s, count: u64): %s { return value >> count }\n"
        "entry { }\n",
        integer->name, integer->name, integer->name, integer->name);
    CHECK(written > 0 && (size_t)written < sizeof(source) &&
          fixture_run(value, source));
    CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
          value->result.status == W_SEED_FRONTEND_OK &&
          counts_equal(&value->result.required, &value->result.written));

    size_t shift_count = 0u;
    bool saw_left = false;
    bool saw_right = false;
    for (size_t expression_index = 0u;
         expression_index < value->result.written.expressions;
         expression_index += 1u) {
      const w_seed_frontend_expression *expression =
          &value->expressions[expression_index];
      if (expression->kind != W_SEED_FRONTEND_EXPR_BINARY ||
          (!frontend_text_is(expression->operator_text, "<<") &&
           !frontend_text_is(expression->operator_text, ">>")))
        continue;
      shift_count += 1u;
      CHECK(expression->supported && expression->left <
                                            value->result.written.expressions &&
            expression->right < value->result.written.expressions &&
            expression->inferred_type < value->result.written.types);
      const w_seed_frontend_expression *left =
          &value->expressions[expression->left];
      const w_seed_frontend_expression *right =
          &value->expressions[expression->right];
      CHECK(left->inferred_type < value->result.written.types &&
            right->inferred_type < value->result.written.types &&
            expression->inferred_type == left->inferred_type);
      const w_seed_frontend_type *left_type =
          &value->types[left->inferred_type];
      const w_seed_frontend_type *right_type =
          &value->types[right->inferred_type];
      const w_seed_frontend_type *result_type =
          &value->types[expression->inferred_type];
      CHECK(left_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            left_type->is_signed == integer->is_signed &&
            left_type->bit_width == integer->bit_width &&
            result_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            result_type->is_signed == integer->is_signed &&
            result_type->bit_width == integer->bit_width &&
            right_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            !right_type->is_signed && right_type->bit_width == 64u &&
            !frontend_text_is(right_type->spelling, "usize"));
      if (frontend_text_is(expression->operator_text, "<<")) {
        CHECK(!saw_left);
        saw_left = true;
      } else {
        CHECK(!saw_right);
        saw_right = true;
      }
    }
    CHECK(shift_count == 2u && saw_left && saw_right);
  }

  /* Out-of-width counts remain represented for checked evaluation, which is
   * responsible for rejecting count >= the result's logical width. */
  CHECK(fixture_run(value,
                    "fn shift(value: i8): i8 { return value << 8_u64 }\n"
                    "entry { }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  bool saw_out_of_width_count = false;
  for (size_t expression_index = 0u;
       expression_index < value->result.written.expressions;
       expression_index += 1u) {
    const w_seed_frontend_expression *expression =
        &value->expressions[expression_index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_BINARY ||
        !frontend_text_is(expression->operator_text, "<<"))
      continue;
    CHECK(expression->right < value->result.written.expressions);
    const w_seed_frontend_expression *count =
        &value->expressions[expression->right];
    CHECK(count->kind == W_SEED_FRONTEND_EXPR_INTEGER &&
          count->inferred_type < value->result.written.types);
    const w_seed_frontend_type *count_type =
        &value->types[count->inferred_type];
    CHECK(count_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
          !count_type->is_signed && count_type->bit_width == 64u);
    saw_out_of_width_count = true;
  }
  CHECK(saw_out_of_width_count);

  static const char *const REJECTED[] = {
      "fn bad(value: i8, count: u32): i8 { return value << count }\nentry {}\n",
      "fn bad(value: u8, count: Int): u8 { return value >> count }\nentry {}\n",
      "fn bad(value: i16, count: usize): i16 { return value << count }\nentry {}\n",
      "fn bad(value: usize, count: UInt): usize { return value >> count }\nentry {}\n",
  };
  for (size_t index = 0u; index < sizeof(REJECTED) / sizeof(REJECTED[0]);
       index += 1u) {
    CHECK(fixture_run(value, REJECTED[index]));
    CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
          has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  }
  return true;
}

static bool test_checked_shift_binding_interpolation_frontend(void) {
  typedef struct {
    const char *name;
    bool is_signed;
    uint16_t bit_width;
  } integer_case;
  static const integer_case INTEGERS[] = {
      {"i8", true, 8u},     {"u8", false, 8u},
      {"i16", true, 16u},   {"u16", false, 16u},
      {"i32", true, 32u},   {"u32", false, 32u},
      {"i64", true, 64u},   {"u64", false, 64u},
      {"Int", true, 64u},   {"UInt", false, 64u},
  };
  char source[4096];
  size_t source_length = 0u;
  for (size_t integer_index = 0u;
       integer_index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       integer_index += 1u) {
    const integer_case *integer = &INTEGERS[integer_index];
    const int written = snprintf(
        source + source_length, sizeof(source) - source_length,
        "fn shift%u(value: %s) {\n"
        "  let right = value >> 2_u64\n"
        "  let left = value << 1_u64\n"
        "  print(\"%s ${right}/${left}\")\n"
        "}\n",
        (unsigned int)integer_index, integer->name, integer->name);
    CHECK(written > 0 && (size_t)written < sizeof(source) - source_length);
    source_length += (size_t)written;
  }
  const int entry_written = snprintf(
      source + source_length, sizeof(source) - source_length, "entry { }\n");
  CHECK(entry_written > 0 &&
        (size_t)entry_written < sizeof(source) - source_length);
  fixture *value = &fixture_literal;
  CHECK(fixture_parse(value, source));
  fixture_configure_print_host(value);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));

  size_t interpolated_count = 0u;
  size_t binding_read_count = 0u;
  for (size_t expression_index = 0u;
       expression_index < value->result.written.expressions;
       expression_index += 1u) {
    const w_seed_frontend_expression *expression =
        &value->expressions[expression_index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING) {
      CHECK(expression->supported &&
            expression->first_interpolation_segment != W_SEED_FRONTEND_NONE &&
            expression->interpolation_segment_count == 4u);
      interpolated_count += 1u;
      continue;
    }
    if (expression->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
        (!frontend_text_is(expression->spelling, "right") &&
         !frontend_text_is(expression->spelling, "left")))
      continue;
    CHECK(expression->owner_function <
              sizeof(INTEGERS) / sizeof(INTEGERS[0]) &&
          expression->supported &&
          expression->resolved_binding_statement != W_SEED_FRONTEND_NONE &&
          expression->resolved_binding_statement <
              value->result.written.statements &&
          expression->inferred_type < value->result.written.types);
    const integer_case *integer = &INTEGERS[expression->owner_function];
    const w_seed_frontend_type *type =
        &value->types[expression->inferred_type];
    const w_seed_frontend_statement *binding =
        &value->statements[expression->resolved_binding_statement];
    CHECK(type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
          type->is_signed == integer->is_signed &&
          type->bit_width == integer->bit_width &&
          binding->effective_type == expression->inferred_type);
    binding_read_count += 1u;
  }
  CHECK(interpolated_count == sizeof(INTEGERS) / sizeof(INTEGERS[0]) &&
        binding_read_count ==
            2u * sizeof(INTEGERS) / sizeof(INTEGERS[0]));

  static const char rejected_count[] =
      "fn bad(value: i8, count: i32): i8 { return value << count }\n"
      "entry { }\n";
  CHECK(fixture_parse(value, rejected_count));
  fixture_configure_print_host(value);
  const w_seed_frontend_status rejected_status =
      w_seed_frontend_run(&value->input, &value->output, &value->result);
  CHECK(rejected_status == W_SEED_FRONTEND_UNSUPPORTED &&
        value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  return true;
}

static bool test_u64_binary_frontend(void) {
  static const char SOURCE[] =
      "fn unsigned(left: UInt, right: UInt): UInt { "
      "let add = left + right "
      "let subtract = left - right "
      "let multiply = left * right "
      "let divide = left / 0_u64 "
      "let remainder = left % 0_u64 "
      "let bitAnd = left & right "
      "let bitOr = left | right "
      "let bitXor = left ^ right "
      "let equal = left == right "
      "let notEqual = left != right "
      "let less = left < right "
      "let lessEqual = left <= right "
      "let greater = left > right "
      "let greaterEqual = left >= right "
      "let overflowAdd = 18446744073709551615_u64 + 1_u64 "
      "let underflow = 0_u64 - 1_u64 "
      "let overflowMultiply = 18446744073709551615_u64 * 2_u64 "
      "let contextual = left + 1 return add }\n"
      "entry { }\n";
  fixture *value = &fixture_literal;
  CHECK(fixture_run(value, SOURCE));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));

  size_t arithmetic_count[5] = {0u, 0u, 0u, 0u, 0u};
  size_t comparison_count[6] = {0u, 0u, 0u, 0u, 0u, 0u};
  size_t bitwise_count[3] = {0u, 0u, 0u};
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_BINARY) continue;
    size_t *count = NULL;
    const char *operators[] = {"+", "-", "*", "/", "%", "==", "!=",
                               "<", "<=", ">", ">=", "&", "|", "^"};
    size_t operator_index = 0u;
    for (; operator_index < sizeof(operators) / sizeof(operators[0]);
         operator_index += 1u) {
      if (frontend_text_is(expression->operator_text, operators[operator_index]))
        break;
    }
    CHECK(operator_index < sizeof(operators) / sizeof(operators[0]) &&
          expression->supported && expression->left < value->result.written.expressions &&
          expression->right < value->result.written.expressions &&
          expression->inferred_type < value->result.written.types);
    const w_seed_frontend_expression *left =
        &value->expressions[expression->left];
    const w_seed_frontend_expression *right =
        &value->expressions[expression->right];
    CHECK(left->inferred_type < value->result.written.types &&
          right->inferred_type < value->result.written.types);
    const w_seed_frontend_type *left_type = &value->types[left->inferred_type];
    const w_seed_frontend_type *right_type =
        &value->types[right->inferred_type];
    const w_seed_frontend_type *result_type =
        &value->types[expression->inferred_type];
    if (operator_index < 5u) {
      count = &arithmetic_count[operator_index];
      CHECK(left_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            right_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            !left_type->is_signed && !right_type->is_signed &&
            left_type->bit_width == 64u && right_type->bit_width == 64u &&
            result_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            !result_type->is_signed && result_type->bit_width == 64u);
    } else if (operator_index < 11u) {
      count = &comparison_count[operator_index - 5u];
      CHECK(left_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            right_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            !left_type->is_signed && !right_type->is_signed &&
            left_type->bit_width == 64u && right_type->bit_width == 64u &&
            result_type->kind == W_SEED_FRONTEND_TYPE_BOOL);
    } else {
      count = &bitwise_count[operator_index - 11u];
      CHECK(left_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            right_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            !left_type->is_signed && !right_type->is_signed &&
            left_type->bit_width == 64u && right_type->bit_width == 64u &&
            result_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            !result_type->is_signed && result_type->bit_width == 64u);
    }
    *count += 1u;
  }
  CHECK(arithmetic_count[0] == 3u && arithmetic_count[1] == 2u &&
        arithmetic_count[2] == 2u && arithmetic_count[3] == 1u &&
        arithmetic_count[4] == 1u && comparison_count[0] == 1u &&
        comparison_count[1] == 1u && comparison_count[2] == 1u &&
        comparison_count[3] == 1u && comparison_count[4] == 1u &&
        comparison_count[5] == 1u && bitwise_count[0] == 1u &&
        bitwise_count[1] == 1u && bitwise_count[2] == 1u);

  static const char *UNSIGNED_PREFIX_TYPES[] = {"u8", "u16", "u32", "u64",
                                                 "UInt"};
  char unsigned_prefix_source[128];
  for (size_t index = 0u;
       index < sizeof(UNSIGNED_PREFIX_TYPES) /
                   sizeof(UNSIGNED_PREFIX_TYPES[0]);
       index += 1u) {
    const int written = snprintf(
        unsigned_prefix_source, sizeof(unsigned_prefix_source),
        "fn bad(value: %s): %s { return -value }\nentry { }\n",
        UNSIGNED_PREFIX_TYPES[index], UNSIGNED_PREFIX_TYPES[index]);
    CHECK(written > 0 && (size_t)written < sizeof(unsigned_prefix_source));
    CHECK(fixture_run(value, unsigned_prefix_source));
    CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
          has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  }
  CHECK(fixture_run(value,
                    "fn invert(value: UInt): UInt { return ~value }\n"
                    "entry { }\n"));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  uint32_t unary_index = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_UNARY) continue;
    CHECK(unary_index == W_SEED_FRONTEND_NONE);
    unary_index = (uint32_t)index;
  }
  CHECK(unary_index != W_SEED_FRONTEND_NONE);
  const w_seed_frontend_expression *unary = &value->expressions[unary_index];
  CHECK(unary->supported && frontend_text_is(unary->operator_text, "~") &&
        unary->left < value->result.written.expressions &&
        unary->right == W_SEED_FRONTEND_NONE &&
        unary->inferred_type < value->result.written.types);
  const w_seed_frontend_expression *operand =
      &value->expressions[unary->left];
  const w_seed_frontend_type *unary_type =
      &value->types[unary->inferred_type];
  CHECK(operand->supported && operand->inferred_type == unary->inferred_type &&
        unary_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
        !unary_type->is_signed && unary_type->bit_width == 64u);
  CHECK(fixture_run(value,
                    "fn mixed(left: Int, right: UInt): Int { "
                    "return left + right }\nentry { }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(value, "W-TYPE-0122") &&
        has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  return true;
}

static bool test_i128_literal_frontend(void) {
  static const char SOURCE[] =
      "fn signedMax(): i128 { return "
      "170141183460469231731687303715884105727_i128 }\n"
      "fn signedMin(): i128 { return "
      "-170141183460469231731687303715884105728_i128 }\n"
      "fn unsignedMax(): u128 { return "
      "340282366920938463463374607431768211455_u128 }\n"
      "fn unsignedHexMax(): u128 { return "
      "0xffffffffffffffffffffffffffffffff_u128 }\n"
      "fn echoSigned(value: i128): i128 { return value }\n"
      "fn echoUnsigned(value: u128): u128 { return value }\n"
      "fn boundUnsigned(): u128 { let wide: u128 = "
      "0x80000000000000000000000000000000_u128 return wide }\n"
      "fn maxU8(): u8 { return 255_u8 }\n"
      "fn maxU16(): u16 { return 65535_u16 }\n"
      "fn maxU32(): u32 { return 4294967295_u32 }\n"
      "fn maxU64(): u64 { return 18446744073709551615_u64 }\n"
      "entry { }\n";
  fixture *value = &fixture_literal;
  CHECK(fixture_run(value, SOURCE));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written) &&
        value->result.required.receipt_bytes == value->result.receipt_bytes &&
        value->result.written.facts == 0u &&
        value->result.written.diagnostics == 0u);

  w_seed_frontend_counts measured;
  w_seed_frontend_result measure_result;
  CHECK(w_seed_frontend_measure(&value->input, &measured, &measure_result) ==
        W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&measured, &value->result.required) &&
        measure_result.required.receipt_bytes == value->result.receipt_bytes &&
        frontend_text_is(measure_result.schema_version,
                         W_SEED_FRONTEND_SCHEMA_VERSION));

  static const struct {
    const char *name;
    bool is_signed;
    uint16_t bit_width;
  } FUNCTIONS[] = { {"signedMax", true, 128u},
                    {"signedMin", true, 128u},
                    {"unsignedMax", false, 128u},
                    {"unsignedHexMax", false, 128u},
                    {"echoSigned", true, 128u},
                    {"echoUnsigned", false, 128u},
                    {"boundUnsigned", false, 128u},
                    {"maxU8", false, 8u},
                    {"maxU16", false, 16u},
                    {"maxU32", false, 32u},
                    {"maxU64", false, 64u} };
  for (size_t expected = 0u;
       expected < sizeof(FUNCTIONS) / sizeof(FUNCTIONS[0]); expected += 1u) {
    bool found = false;
    for (size_t index = 0u; index < value->result.written.functions;
         index += 1u) {
      const w_seed_frontend_function *function = &value->functions[index];
      if (!frontend_text_is(function->name, FUNCTIONS[expected].name))
        continue;
      CHECK(function->return_type < value->result.written.types);
      const w_seed_frontend_type *type =
          &value->types[function->return_type];
      CHECK(type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            type->is_signed == FUNCTIONS[expected].is_signed &&
            type->bit_width == FUNCTIONS[expected].bit_width);
      if (expected == 4u || expected == 5u) {
        CHECK(function->parameter_count == 1u &&
              function->first_parameter < value->result.written.parameters);
        const w_seed_frontend_parameter *parameter =
            &value->parameters[function->first_parameter];
        CHECK(parameter->type_index < value->result.written.types);
        const w_seed_frontend_type *parameter_type =
            &value->types[parameter->type_index];
        CHECK(parameter_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
              parameter_type->is_signed == FUNCTIONS[expected].is_signed &&
              parameter_type->bit_width == FUNCTIONS[expected].bit_width);
      }
      found = true;
      break;
    }
    CHECK(found);
  }

  uint8_t signed_max[16];
  uint8_t signed_min[16] = {0u};
  uint8_t unsigned_max[16];
  uint8_t unsigned_high_bit[16] = {0u};
  uint8_t u8_max[16] = {0xffu};
  uint8_t u16_max[16] = {0xffu, 0xffu};
  uint8_t u32_max[16] = {0xffu, 0xffu, 0xffu, 0xffu};
  uint8_t u64_max[16] = {0xffu, 0xffu, 0xffu, 0xffu,
                          0xffu, 0xffu, 0xffu, 0xffu};
  (void)memset(signed_max, 0xff, sizeof(signed_max));
  (void)memset(unsigned_max, 0xff, sizeof(unsigned_max));
  signed_max[15] = 0x7fu;
  signed_min[15] = 0x80u;
  unsigned_high_bit[15] = 0x80u;
  const struct {
    const char *spelling;
    bool is_signed;
    uint16_t bit_width;
    const uint8_t *magnitude;
  } LITERALS[] = {
      {"170141183460469231731687303715884105727_i128", true, 128u,
       signed_max},
      {"170141183460469231731687303715884105728_i128", true, 128u,
       signed_min},
      {"340282366920938463463374607431768211455_u128", false, 128u,
       unsigned_max},
      {"0xffffffffffffffffffffffffffffffff_u128", false, 128u,
       unsigned_max},
      {"0x80000000000000000000000000000000_u128", false, 128u,
       unsigned_high_bit},
      {"255_u8", false, 8u, u8_max},
      {"65535_u16", false, 16u, u16_max},
      {"4294967295_u32", false, 32u, u32_max},
      {"18446744073709551615_u64", false, 64u, u64_max},
  };
  uint32_t signed_min_literal = W_SEED_FRONTEND_NONE;
  for (size_t expected = 0u;
       expected < sizeof(LITERALS) / sizeof(LITERALS[0]); expected += 1u) {
    bool found = false;
    for (size_t index = 0u; index < value->result.written.expressions;
         index += 1u) {
      const w_seed_frontend_expression *expression =
          &value->expressions[index];
      if (expression->kind != W_SEED_FRONTEND_EXPR_INTEGER ||
          !frontend_text_is(expression->spelling, LITERALS[expected].spelling))
        continue;
      CHECK(expression->supported && expression->has_integer_value &&
            expression->inferred_type < value->result.written.types &&
            memcmp(expression->integer_value, LITERALS[expected].magnitude,
                   sizeof(expression->integer_value)) == 0);
      const w_seed_frontend_type *type =
          &value->types[expression->inferred_type];
      CHECK(type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            type->is_signed == LITERALS[expected].is_signed &&
            type->bit_width == LITERALS[expected].bit_width);
      if (expected == 1u) signed_min_literal = (uint32_t)index;
      found = true;
      break;
    }
    CHECK(found);
  }
  uint32_t bound_literal = W_SEED_FRONTEND_NONE;
  bool found_bound_read = false;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_INTEGER &&
        frontend_text_is(expression->spelling,
                         "0x80000000000000000000000000000000_u128")) {
      bound_literal = (uint32_t)index;
      CHECK(expression->supported && expression->has_integer_value &&
            memcmp(expression->integer_value, unsigned_high_bit,
                   sizeof(unsigned_high_bit)) == 0);
      continue;
    }
    if (expression->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
        !frontend_text_is(expression->spelling, "wide"))
      continue;
    CHECK(expression->supported &&
          expression->resolved_binding_statement != W_SEED_FRONTEND_NONE &&
          expression->resolved_binding_statement <
              value->result.written.statements &&
          expression->inferred_type < value->result.written.types);
    const w_seed_frontend_statement *binding =
        &value->statements[expression->resolved_binding_statement];
    const w_seed_frontend_type *type =
        &value->types[expression->inferred_type];
    CHECK(binding->kind == W_SEED_FRONTEND_STMT_LET &&
          binding->expression_index == bound_literal &&
          binding->effective_type == expression->inferred_type &&
          type->kind == W_SEED_FRONTEND_TYPE_INTEGER && !type->is_signed &&
          type->bit_width == 128u);
    found_bound_read = true;
  }
  CHECK(bound_literal != W_SEED_FRONTEND_NONE && found_bound_read);
  bool found_signed_min_root = false;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_UNARY ||
        expression->left != signed_min_literal)
      continue;
    CHECK(expression->supported && expression->right == W_SEED_FRONTEND_NONE &&
          frontend_text_is(expression->operator_text, "-") &&
          expression->inferred_type ==
              value->expressions[signed_min_literal].inferred_type);
    found_signed_min_root = true;
  }
  CHECK(found_signed_min_root);

  static const char *const UNSUPPORTED[] = {
      "fn bad(): i128 { return "
      "170141183460469231731687303715884105728_i128 } entry { }\n",
      "fn bad(): i128 { return "
      "-170141183460469231731687303715884105729_i128 } entry { }\n",
      "fn bad(): u128 { return "
      "340282366920938463463374607431768211456_u128 } entry { }\n",
      "fn bad(): u128 { return -1_u128 } entry { }\n",
      "fn bad(): i128 { return 1_i128tail } entry { }\n",
      "fn bad(): i128 { return 1_i129 } entry { }\n",
      "fn add(left: i128, right: i128): i128 { return left + right } "
      "entry { }\n",
      "fn bits(left: u128, right: u128): u128 { return left ^ right } "
      "entry { }\n",
      "fn shift(value: i128, count: u64): i128 { return value << count } "
      "entry { }\n",
      "fn negate(value: i128): i128 { return -value } entry { }\n",
      "fn invert(value: u128): u128 { return ~value } entry { }\n",
      "fn assign(value: i128): i128 { var current: i128 = value "
      "current = value return current } entry { }\n",
      "fn widen(value: i64): i128 { return value } entry { }\n",
      "fn contextual(): i128 { return 1 } entry { }\n",
      "fn convert(value: i16): i128 { "
      "return i128(truncatingBits: value) } entry { }\n",
      "fn convert(value: i16): i128 throws NumericConversionError { "
      "return try i128(exactly: value) } entry { }\n",
  };
  for (size_t index = 0u;
       index < sizeof(UNSUPPORTED) / sizeof(UNSUPPORTED[0]); index += 1u) {
    CHECK(fixture_run(value, UNSUPPORTED[index]));
    CHECK(value->result.status != W_SEED_FRONTEND_OK &&
          (has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION) ||
           has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE) ||
           value->result.status == W_SEED_FRONTEND_DIAGNOSTICS));
    if (index >= 6u && index <= 11u)
      CHECK(has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  }
  CHECK(fixture_run_with_print_host(
      value, "fn show(value: i128) { print(\"${value}\") } entry(show)\n"));
  CHECK(value->result.status != W_SEED_FRONTEND_OK &&
        has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  return true;
}

static bool test_flat_aggregate_pair_frontend(void) {
  static const char SOURCE[] =
      "type Pair = (i64, i64)\n"
      "\n"
      "fn makePair(left: i64, right: i64): Pair {\n"
      "  let pair: Pair = (left, right)\n"
      "  return pair\n"
      "}\n"
      "\n"
      "fn combine(pair: Pair, scale: i64): i64 {\n"
      "  let product = pair.0 * scale\n"
      "  return product + pair.1\n"
      "}\n"
      "\n"
      "entry {\n"
      "  let original = makePair(left: 7, right: 5)\n"
      "  let result = combine(pair: original, scale: 3)\n"
      "  print(\"${original.0},${original.1},${result}\")\n"
      "}\n";
  fixture *value = &fixture_literal;
  CHECK(fixture_run_with_print_host(value, SOURCE));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written) &&
        value->result.required.receipt_bytes == value->result.receipt_bytes &&
        value->result.written.tuple_components >= 2u &&
        value->result.written.tuple_elements == 2u);

  w_seed_frontend_counts measured;
  w_seed_frontend_result measure_result;
  CHECK(w_seed_frontend_measure(&value->input, &measured, &measure_result) ==
        W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&measured, &value->result.required) &&
        measure_result.required.receipt_bytes == value->result.receipt_bytes &&
        frontend_text_is(measure_result.schema_version,
                         W_SEED_FRONTEND_SCHEMA_VERSION));

  size_t pair_declarations = 0u;
  for (size_t index = 0u; index < value->result.written.type_declarations;
       index += 1u) {
    const w_seed_frontend_type_declaration *declaration =
        &value->type_declarations[index];
    if (!frontend_text_is(declaration->name, "Pair")) continue;
    CHECK(declaration->type_index < value->result.written.types);
    const w_seed_frontend_type *type = &value->types[declaration->type_index];
    CHECK(type->kind == W_SEED_FRONTEND_TYPE_TUPLE &&
          type->tuple_component_count == 2u &&
          type->first_tuple_component != W_SEED_FRONTEND_NONE &&
          (size_t)type->first_tuple_component <=
              value->result.written.tuple_components &&
          (size_t)type->tuple_component_count <=
              value->result.written.tuple_components -
                  (size_t)type->first_tuple_component);
    for (size_t ordinal = 0u; ordinal < type->tuple_component_count;
         ordinal += 1u) {
      const w_seed_frontend_tuple_component *component =
          &value->tuple_components[(size_t)type->first_tuple_component +
                                   ordinal];
      CHECK(component->owner_type == declaration->type_index &&
            component->ordinal == ordinal && component->label.length == 0u &&
            component->type_index < value->result.written.types);
      const w_seed_frontend_type *component_type =
          &value->types[component->type_index];
      CHECK(component_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            component_type->is_signed && component_type->bit_width == 64u);
      CHECK(component->span.start_byte < component->span.end_byte &&
            component_type->span.start_byte == component->span.start_byte &&
            component_type->span.end_byte == component->span.end_byte &&
            fixture_span_text_is(value, declaration->module_index,
                                 component->span, "i64"));
    }
    pair_declarations += 1u;
  }
  CHECK(pair_declarations == 1u);

  size_t tuple_constructors = 0u;
  size_t projection_zero = 0u;
  size_t projection_one = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_TUPLE) {
      CHECK(expression->supported &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_TUPLE &&
            expression->tuple_element_count == 2u &&
            expression->first_tuple_element != W_SEED_FRONTEND_NONE &&
            (size_t)expression->first_tuple_element <=
                value->result.written.tuple_elements &&
            (size_t)expression->tuple_element_count <=
                value->result.written.tuple_elements -
                    (size_t)expression->first_tuple_element);
      static const char *const element_names[] = {"left", "right"};
      for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
        const w_seed_frontend_tuple_element *element =
            &value->tuple_elements[(size_t)expression->first_tuple_element +
                                   ordinal];
        CHECK(element->owner_expression == index &&
              element->ordinal == ordinal && element->label.length == 0u &&
              element->expression_index < value->result.written.expressions &&
              element->span.start_byte < element->span.end_byte);
        const w_seed_frontend_expression *child =
            &value->expressions[element->expression_index];
        CHECK(child->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
              child->supported &&
              frontend_text_is(child->spelling, element_names[ordinal]) &&
              child->span.start_byte == element->span.start_byte &&
              child->span.end_byte == element->span.end_byte &&
              fixture_span_text_is(value, expression->module_index,
                                   element->span, element_names[ordinal]));
        const w_seed_frontend_type *tuple_type =
            &value->types[expression->inferred_type];
        const w_seed_frontend_tuple_component *component =
            &value->tuple_components[(size_t)tuple_type->first_tuple_component +
                                     ordinal];
        CHECK(component->type_index == child->inferred_type);
      }
      tuple_constructors += 1u;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_MEMBER &&
               frontend_text_is(expression->member_name, "0")) {
      CHECK(expression->supported &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_INTEGER &&
            value->types[expression->inferred_type].is_signed &&
            value->types[expression->inferred_type].bit_width == 64u);
      CHECK(expression->left < value->result.written.expressions);
      const w_seed_frontend_expression *receiver =
          &value->expressions[expression->left];
      CHECK(receiver->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
            receiver->inferred_type < value->result.written.types &&
            value->types[receiver->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_TUPLE &&
            receiver->span.start_byte < receiver->span.end_byte &&
            expression->span.start_byte == receiver->span.start_byte);
      const w_seed_frontend_type *receiver_type =
          &value->types[receiver->inferred_type];
      const w_seed_frontend_tuple_component *component =
          &value->tuple_components[receiver_type->first_tuple_component];
      CHECK(component->ordinal == 0u &&
            component->type_index == expression->inferred_type);
      projection_zero += 1u;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_MEMBER &&
               frontend_text_is(expression->member_name, "1")) {
      CHECK(expression->supported &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_INTEGER &&
            value->types[expression->inferred_type].is_signed &&
            value->types[expression->inferred_type].bit_width == 64u);
      CHECK(expression->left < value->result.written.expressions);
      const w_seed_frontend_expression *receiver =
          &value->expressions[expression->left];
      CHECK(receiver->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
            receiver->inferred_type < value->result.written.types &&
            value->types[receiver->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_TUPLE &&
            receiver->span.start_byte < receiver->span.end_byte &&
            expression->span.start_byte == receiver->span.start_byte);
      const w_seed_frontend_type *receiver_type =
          &value->types[receiver->inferred_type];
      const w_seed_frontend_tuple_component *component =
          &value->tuple_components[(size_t)receiver_type->first_tuple_component +
                                   1u];
      CHECK(component->ordinal == 1u &&
            component->type_index == expression->inferred_type);
      projection_one += 1u;
    }
  }
  CHECK(tuple_constructors == 1u && projection_zero == 2u &&
        projection_one == 2u &&
        receipt_contains(value, "tuple-component=", 16u) &&
        receipt_contains(value, "tuple-element=", 14u) &&
        receipt_contains(value, "|label=0:|type=", 15u));

  fixture *repeat = &fixture_a;
  CHECK(fixture_run_with_print_host(repeat, SOURCE));
  CHECK(repeat->result.status == W_SEED_FRONTEND_OK &&
        repeat->result.receipt_bytes == value->result.receipt_bytes &&
        memcmp(repeat->receipt, value->receipt,
               value->result.receipt_bytes) == 0);

  const uint8_t sentinel = 0xa5u;
  const size_t tuple_component_capacity =
      value->result.required.tuple_components;
  const size_t tuple_element_capacity = value->result.required.tuple_elements;
  CHECK(tuple_component_capacity > 0u && tuple_element_capacity > 0u);
  const size_t short_capacities[] = {tuple_component_capacity - 1u,
                                     tuple_element_capacity - 1u};
  for (size_t target = 0u; target < 2u; target += 1u) {
    CHECK(fixture_parse(value, SOURCE));
    fixture_configure_print_host(value);
    fixture_fill_output(value, sentinel);
    if (target == 0u)
      value->output.tuple_component_capacity = short_capacities[target];
    else
      value->output.tuple_element_capacity = short_capacities[target];
    (void)w_seed_frontend_run(&value->input, &value->output, &value->result);
    CHECK(value->result.status == W_SEED_FRONTEND_CAPACITY &&
          value->result.required.tuple_components == tuple_component_capacity &&
          value->result.required.tuple_elements == tuple_element_capacity &&
          fixture_output_is(value, sentinel, true));
    value->output.tuple_component_capacity = TEST_TUPLE_COMPONENTS;
    value->output.tuple_element_capacity = TEST_TUPLE_ELEMENTS;
  }

  CHECK(fixture_parse(value, SOURCE));
  fixture_configure_print_host(value);
  fixture_fill_output(value, sentinel);
  value->output.tuple_components =
      (w_seed_frontend_tuple_component *)(void *)value->types;
  value->output.tuple_component_capacity = tuple_component_capacity;
  (void)w_seed_frontend_run(&value->input, &value->output, &value->result);
  CHECK(value->result.status == W_SEED_FRONTEND_INVALID &&
        fixture_output_is(value, sentinel, true));
  value->output.tuple_components = value->tuple_components;
  value->output.tuple_component_capacity = TEST_TUPLE_COMPONENTS;

  CHECK(fixture_parse(value, SOURCE));
  fixture_configure_print_host(value);
  fixture_fill_output(value, sentinel);
  value->output.tuple_elements =
      (w_seed_frontend_tuple_element *)(void *)value->expressions;
  value->output.tuple_element_capacity = tuple_element_capacity;
  (void)w_seed_frontend_run(&value->input, &value->output, &value->result);
  CHECK(value->result.status == W_SEED_FRONTEND_INVALID &&
        fixture_output_is(value, sentinel, true));
  value->output.tuple_elements = value->tuple_elements;
  value->output.tuple_element_capacity = TEST_TUPLE_ELEMENTS;

  static const char *const PARSER_REJECTIONS[] = {
      "type Single = (i64,)\n",
      "fn single(value: i64): (i64, i64) { return (value,) }\n",
      "type Named = (left: i64, right: i64)\n",
      "fn named(left: i64, right: i64): (i64, i64) { "
      "return (first: left, second: right) }\n",
  };
  for (size_t index = 0u;
       index < sizeof(PARSER_REJECTIONS) / sizeof(PARSER_REJECTIONS[0]);
       index += 1u) {
    CHECK(fixture_parse(value, PARSER_REJECTIONS[index]));
    CHECK(value->parse.status != W_SEED_PARSE_COMPLETE);
  }

  static const char MISMATCH[] =
      "fn reversed(): (i64, Bool) { return (true, 1_i64) }\n"
      "entry(reversed)\n";
  CHECK(fixture_run(value, MISMATCH));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status != W_SEED_FRONTEND_OK &&
        has_diagnostic(value, "W-TYPE-0120"));
  const w_seed_frontend_diagnostic *tuple_mismatch =
      diagnostic_for_code(value, "W-TYPE-0120");
  CHECK(tuple_mismatch != NULL && tuple_mismatch->label_count == 2u &&
        diagnostic_record_ranges_are_valid(value, tuple_mismatch) &&
        diagnostic_label_role_is(value, tuple_mismatch, 0u,
                                 "actual-component") &&
        diagnostic_label_role_is(value, tuple_mismatch, 1u,
                                 "expected-component"));
  bool saw_reversed_tuple = false;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_TUPLE) continue;
    CHECK(expression->tuple_element_count == 2u &&
          expression->first_tuple_element != W_SEED_FRONTEND_NONE);
    const w_seed_frontend_tuple_element *first =
        &value->tuple_elements[expression->first_tuple_element];
    const w_seed_frontend_tuple_element *second =
        &value->tuple_elements[(size_t)expression->first_tuple_element + 1u];
    CHECK(first->ordinal == 0u && second->ordinal == 1u &&
          value->expressions[first->expression_index].kind ==
              W_SEED_FRONTEND_EXPR_BOOL &&
          value->expressions[second->expression_index].kind ==
              W_SEED_FRONTEND_EXPR_INTEGER &&
          first->span.start_byte ==
              value->expressions[first->expression_index].span.start_byte &&
          first->span.end_byte ==
              value->expressions[first->expression_index].span.end_byte &&
          fixture_span_text_is(value, 0u, first->span, "true"));
    const w_seed_frontend_diagnostic_label *actual_label =
        &value->diagnostic_labels[tuple_mismatch->first_label];
    const w_seed_frontend_diagnostic_label *expected_label =
        &value->diagnostic_labels[tuple_mismatch->first_label + 1u];
    CHECK(actual_label->span.start_byte == first->span.start_byte &&
          actual_label->span.end_byte == first->span.end_byte &&
          fixture_span_text_is(value, actual_label->document_index,
                               actual_label->span, "true"));
    bool expected_component_span_found = false;
    for (size_t type_index = 0u;
         type_index < value->result.written.types; type_index += 1u) {
      const w_seed_frontend_type *type = &value->types[type_index];
      if (type->kind != W_SEED_FRONTEND_TYPE_TUPLE ||
          !frontend_text_is(type->spelling, "(i64, Bool)"))
        continue;
      const w_seed_frontend_tuple_component *component =
          &value->tuple_components[type->first_tuple_component];
      if (expected_label->span.start_byte == component->span.start_byte &&
          expected_label->span.end_byte == component->span.end_byte &&
          fixture_span_text_is(value, expected_label->document_index,
                               expected_label->span, "i64"))
        expected_component_span_found = true;
    }
    CHECK(expected_component_span_found);
    saw_reversed_tuple = true;
  }
  CHECK(saw_reversed_tuple);

  static const char WHITESPACE_TYPES[] =
      "fn flat(value: (i64,i64)): ( i64 , i64 ) { return value }\n"
      "fn nested(value: ( i64 , (Bool,i64) )): (i64,( Bool , i64 )) { "
      "return value }\n"
      "entry(flat)\n";
  fixture *whitespace = &fixture_external;
  CHECK(fixture_run(whitespace, WHITESPACE_TYPES));
  CHECK(whitespace->parse.status == W_SEED_PARSE_COMPLETE &&
        whitespace->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&whitespace->result.required,
                     &whitespace->result.written));

  static const char UNANNOTATED_TUPLE[] =
      "fn unannotated(left: i64, right: i64): i64 { "
      "let pair = (left, right)\nreturn left }\n"
      "entry(unannotated)\n";
  fixture *unannotated = &fixture_capacity;
  CHECK(fixture_run(unannotated, UNANNOTATED_TUPLE));
  CHECK(unannotated->parse.status == W_SEED_PARSE_COMPLETE &&
        unannotated->result.status != W_SEED_FRONTEND_OK);
  bool saw_unannotated_pair = false;
  for (size_t index = 0u; index < unannotated->result.written.statements;
       index += 1u) {
    const w_seed_frontend_statement *statement =
        &unannotated->statements[index];
    if (statement->kind != W_SEED_FRONTEND_STMT_LET ||
        !frontend_text_is(statement->binding_name, "pair"))
      continue;
    CHECK(statement->effective_type == W_SEED_FRONTEND_NONE &&
          statement->expression_index <
              unannotated->result.written.expressions &&
          unannotated->expressions[statement->expression_index].kind ==
              W_SEED_FRONTEND_EXPR_TUPLE &&
          unannotated->expressions[statement->expression_index].inferred_type ==
              W_SEED_FRONTEND_NONE);
    saw_unannotated_pair = true;
  }
  CHECK(saw_unannotated_pair);

  CHECK(fixture_run(value, "fn unit(): () { return () }\nentry(unit)\n"));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.tuple_components == 0u &&
        value->result.written.tuple_elements == 0u);

  static const char NESTED[] =
      "fn nested(value: i64, flag: Bool): (i64, (Bool, i64)) { "
      "return (value, (flag, value)) }\n"
      "entry(nested)\n";
  fixture *nested = &fixture_b;
  CHECK(fixture_run(nested, NESTED));
  CHECK(nested->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(nested->result.status == W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&nested->result.required, &nested->result.written));
  CHECK(nested->result.written.tuple_components >= 4u);
  CHECK(nested->result.written.tuple_elements == 4u);
  size_t tuple_type_count = 0u;
  size_t tuple_expression_count = 0u;
  for (size_t index = 0u; index < nested->result.written.types; index += 1u) {
    const w_seed_frontend_type *type = &nested->types[index];
    if (type->kind != W_SEED_FRONTEND_TYPE_TUPLE) {
      CHECK(type->first_tuple_component == W_SEED_FRONTEND_NONE &&
            type->tuple_component_count == 0u);
      continue;
    }
    CHECK(type->tuple_component_count == 2u &&
          type->first_tuple_component != W_SEED_FRONTEND_NONE &&
          (size_t)type->first_tuple_component <=
              nested->result.written.tuple_components &&
          (size_t)type->tuple_component_count <=
              nested->result.written.tuple_components -
                  (size_t)type->first_tuple_component);
    for (size_t ordinal = 0u; ordinal < type->tuple_component_count;
         ordinal += 1u) {
      const w_seed_frontend_tuple_component *component =
          &nested->tuple_components[(size_t)type->first_tuple_component +
                                    ordinal];
      CHECK(component->owner_type == index && component->ordinal == ordinal &&
            component->label.length == 0u &&
            component->type_index < nested->result.written.types);
      const w_seed_frontend_type *component_type =
          &nested->types[component->type_index];
      CHECK(component->span.start_byte < component->span.end_byte);
      if (component_type->kind == W_SEED_FRONTEND_TYPE_TUPLE) {
        CHECK(fixture_span_text_is(nested, 0u, component->span,
                                   "(Bool, i64)"));
      } else if (component_type->kind == W_SEED_FRONTEND_TYPE_BOOL) {
        CHECK(fixture_span_text_is(nested, 0u, component->span, "Bool"));
      } else {
        CHECK(component_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
              component_type->is_signed && component_type->bit_width == 64u &&
              fixture_span_text_is(nested, 0u, component->span, "i64"));
      }
      if (component_type->kind == W_SEED_FRONTEND_TYPE_TUPLE) {
        CHECK(component_type->tuple_component_count == 2u &&
              component_type->first_tuple_component !=
                  W_SEED_FRONTEND_NONE);
        const w_seed_frontend_tuple_component *inner_first =
            &nested->tuple_components[component_type->first_tuple_component];
        const w_seed_frontend_tuple_component *inner_second =
            &nested->tuple_components[
                (size_t)component_type->first_tuple_component + 1u];
        CHECK(inner_first->ordinal == 0u && inner_second->ordinal == 1u &&
              nested->types[inner_first->type_index].kind ==
                  W_SEED_FRONTEND_TYPE_BOOL &&
              nested->types[inner_second->type_index].kind ==
                  W_SEED_FRONTEND_TYPE_INTEGER &&
              nested->types[inner_second->type_index].is_signed &&
              nested->types[inner_second->type_index].bit_width == 64u);
      }
    }
    tuple_type_count += 1u;
  }
  for (size_t index = 0u; index < nested->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression =
        &nested->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_TUPLE) {
      CHECK(expression->first_tuple_element == W_SEED_FRONTEND_NONE &&
            expression->tuple_element_count == 0u);
      continue;
    }
    CHECK(expression->supported &&
          expression->tuple_element_count == 2u &&
          expression->first_tuple_element != W_SEED_FRONTEND_NONE &&
          (size_t)expression->first_tuple_element <=
              nested->result.written.tuple_elements &&
          (size_t)expression->tuple_element_count <=
              nested->result.written.tuple_elements -
                  (size_t)expression->first_tuple_element);
    for (size_t ordinal = 0u; ordinal < expression->tuple_element_count;
         ordinal += 1u) {
      const w_seed_frontend_tuple_element *element =
          &nested->tuple_elements[(size_t)expression->first_tuple_element +
                                  ordinal];
      CHECK(element->owner_expression == index &&
            element->ordinal == ordinal && element->label.length == 0u &&
            element->expression_index < nested->result.written.expressions &&
            element->span.start_byte < element->span.end_byte);
      const w_seed_frontend_expression *child =
          &nested->expressions[element->expression_index];
      const w_seed_frontend_type *expression_type =
          &nested->types[expression->inferred_type];
      const w_seed_frontend_tuple_component *component =
          &nested->tuple_components[(size_t)expression_type->first_tuple_component +
                                    ordinal];
      CHECK(element->span.start_byte == child->span.start_byte &&
            element->span.end_byte == child->span.end_byte &&
            fixture_type_records_equal(nested, component->type_index,
                                       child->inferred_type, 0u));
    }
    tuple_expression_count += 1u;
  }
  CHECK(tuple_type_count >= 2u && tuple_expression_count == 2u);
  return true;
}

static bool test_flat_value_struct_pair_frontend(void) {
  static const char SOURCE[] =
      "struct Pair { let left: i64 let right: i64 }\n"
      "\n"
      "fn makePair(left: i64, right: i64): Pair {\n"
      "  let pair: Pair = Pair(right: right, left: left)\n"
      "  return pair\n"
      "}\n"
      "\n"
      "fn combine(pair: Pair, scale: i64): i64 {\n"
      "  let product = pair.left * scale\n"
      "  return product + pair.right\n"
      "}\n"
      "\n"
      "entry {\n"
      "  let original = makePair(left: 7, right: 5)\n"
      "  let result = combine(pair: original, scale: 3)\n"
      "  print(\"${original.left},${original.right},${result}\")\n"
      "}\n";
  fixture *value = &fixture_literal;
  CHECK(fixture_run_with_print_host(value, SOURCE));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE);
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&value->result.required, &value->result.written));
  CHECK(value->result.required.receipt_bytes == value->result.receipt_bytes);
  CHECK(value->result.written.structs == 1u);
  CHECK(value->result.written.fields == 2u);
  CHECK(value->result.written.functions == 3u);
  CHECK(value->result.written.entries == 1u);
  const size_t required_arguments = value->result.required.arguments;

  uint32_t pair_index = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.structs; index += 1u) {
    if (frontend_text_is(value->structs[index].name, "Pair")) {
      CHECK(pair_index == W_SEED_FRONTEND_NONE);
      pair_index = (uint32_t)index;
    }
  }
  CHECK(pair_index == 0u);
  const w_seed_frontend_struct *pair = &value->structs[pair_index];
  CHECK(pair->field_count == 2u && pair->first_field == 0u &&
        pair->generic_parameter_count == 0u);
  static const char *const FIELD_NAMES[] = {"left", "right"};
  for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
    const w_seed_frontend_field *field =
        &value->fields[(size_t)pair->first_field + ordinal];
    CHECK(field->owner_struct == pair_index &&
          frontend_text_is(field->name, FIELD_NAMES[ordinal]) &&
          field->type_index < value->result.written.types &&
          value->types[field->type_index].kind ==
              W_SEED_FRONTEND_TYPE_INTEGER &&
          value->types[field->type_index].is_signed &&
          value->types[field->type_index].bit_width == 64u &&
          field->span.start_byte < field->span.end_byte);
  }

  size_t nominal_pair_types = 0u;
  for (size_t index = 0u; index < value->result.written.types; index += 1u) {
    const w_seed_frontend_type *type = &value->types[index];
    if (type->kind == W_SEED_FRONTEND_TYPE_NOMINAL &&
        frontend_text_is(type->spelling, "Pair")) {
      CHECK(type->enum_base_index == pair_index &&
            type->external_module_index == W_SEED_FRONTEND_NONE &&
            type->external_symbol_index == W_SEED_FRONTEND_NONE);
      nominal_pair_types += 1u;
    }
  }
  CHECK(nominal_pair_types >= 3u);

  uint32_t make_pair_function = W_SEED_FRONTEND_NONE;
  uint32_t combine_function = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.functions; index += 1u) {
    const w_seed_frontend_function *function = &value->functions[index];
    if (frontend_text_is(function->name, "makePair"))
      make_pair_function = (uint32_t)index;
    if (frontend_text_is(function->name, "combine"))
      combine_function = (uint32_t)index;
  }
  CHECK(make_pair_function != W_SEED_FRONTEND_NONE &&
        combine_function != W_SEED_FRONTEND_NONE &&
        value->functions[make_pair_function].return_type <
            value->result.written.types &&
        value->types[value->functions[make_pair_function].return_type].kind ==
            W_SEED_FRONTEND_TYPE_NOMINAL &&
        value->types[value->functions[make_pair_function].return_type]
                .enum_base_index == pair_index &&
        value->functions[combine_function].parameter_count == 2u);
  const w_seed_frontend_parameter *pair_parameter =
      &value->parameters[value->functions[combine_function].first_parameter];
  CHECK(frontend_text_is(pair_parameter->name, "pair") &&
        pair_parameter->type_index < value->result.written.types &&
        value->types[pair_parameter->type_index].kind ==
            W_SEED_FRONTEND_TYPE_NOMINAL &&
        value->types[pair_parameter->type_index].enum_base_index == pair_index);

  size_t struct_constructors = 0u;
  size_t left_projections = 0u;
  size_t right_projections = 0u;
  size_t labelled_pair_argument = 0u;
  size_t pair_local_bindings = 0u;
  for (size_t index = 0u; index < value->result.written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_CALL &&
        expression->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_LOCAL_STRUCT_CONSTRUCTOR) {
      CHECK(expression->supported &&
            expression->resolved_function_index == pair_index &&
            expression->argument_count == 2u &&
            expression->first_argument < value->result.written.arguments);
      const w_seed_frontend_expression *callee =
          &value->expressions[expression->left];
      CHECK(callee->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
            frontend_text_is(callee->spelling, "Pair") &&
            callee->resolved_callee_kind ==
                W_SEED_FRONTEND_CALLEE_LOCAL_STRUCT_CONSTRUCTOR &&
            callee->resolved_function_index == pair_index);
      const w_seed_frontend_argument *first =
          &value->arguments[expression->first_argument];
      const w_seed_frontend_argument *second =
          &value->arguments[(size_t)expression->first_argument + 1u];
      CHECK(frontend_text_is(first->label, "right") &&
            first->resolved_parameter_ordinal == 1u &&
            frontend_text_is(second->label, "left") &&
            second->resolved_parameter_ordinal == 0u);
      struct_constructors += 1u;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_MEMBER &&
               (frontend_text_is(expression->member_name, "left") ||
                frontend_text_is(expression->member_name, "right")) &&
               expression->supported) {
      CHECK(expression->left < value->result.written.expressions &&
            expression->resolved_parameter_ordinal < 2u &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_INTEGER &&
            value->types[expression->inferred_type].is_signed &&
            value->types[expression->inferred_type].bit_width == 64u);
      const w_seed_frontend_expression *receiver =
          &value->expressions[expression->left];
      CHECK(receiver->inferred_type < value->result.written.types &&
            value->types[receiver->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_NOMINAL &&
            value->types[receiver->inferred_type].enum_base_index ==
                pair_index);
      if (frontend_text_is(expression->member_name, "left")) {
        CHECK(expression->resolved_parameter_ordinal == 0u);
        left_projections += 1u;
      } else {
        CHECK(expression->resolved_parameter_ordinal == 1u);
        right_projections += 1u;
      }
    }
  }
  for (size_t index = 0u; index < value->result.written.arguments; index += 1u) {
    const w_seed_frontend_argument *argument = &value->arguments[index];
    if (frontend_text_is(argument->label, "pair") &&
        argument->resolved_parameter_ordinal == 0u)
      labelled_pair_argument += 1u;
  }
  for (size_t index = 0u; index < value->result.written.statements; index += 1u) {
    const w_seed_frontend_statement *statement = &value->statements[index];
    if ((statement->kind == W_SEED_FRONTEND_STMT_LET ||
         statement->kind == W_SEED_FRONTEND_STMT_VAR) &&
        (frontend_text_is(statement->binding_name, "pair") ||
         frontend_text_is(statement->binding_name, "original")) &&
        statement->effective_type < value->result.written.types &&
        value->types[statement->effective_type].kind ==
            W_SEED_FRONTEND_TYPE_NOMINAL &&
        value->types[statement->effective_type].enum_base_index == pair_index)
      pair_local_bindings += 1u;
  }
  CHECK(struct_constructors == 1u && left_projections == 2u &&
        right_projections == 2u && labelled_pair_argument == 1u &&
        pair_local_bindings == 2u &&
        receipt_contains(value, "struct-constructor=", 19u) &&
        receipt_contains(value, "struct-field-projection=", 24u));

  static const char *const INVALID_INITIALIZERS[] = {
      "struct Pair { let left: i64 let right: i64 }\n"
      "fn build(): Pair { return Pair(left: 1, left: 2) }\nentry(build)\n",
      "struct Pair { let left: i64 let right: i64 }\n"
      "fn build(): Pair { return Pair(left: 1) }\nentry(build)\n",
      "struct Pair { let left: i64 let right: i64 }\n"
      "fn build(): Pair { return Pair(left: 1, other: 2) }\nentry(build)\n",
      "struct Pair { let left: i64 let right: i64 }\n"
      "fn build(): Pair { return Pair(1, 2) }\nentry(build)\n",
      "struct Pair { let left: i64 let right: i64 }\n"
      "fn build(): Pair { return Pair(left: true, right: 2) }\n"
      "entry(build)\n",
      "struct Pair { let left: i64 let left: i64 }\n"
      "fn build(): Pair { return Pair(left: 1, right: 2) }\nentry(build)\n",
  };
  for (size_t index = 0u;
       index < sizeof(INVALID_INITIALIZERS) / sizeof(INVALID_INITIALIZERS[0]);
       index += 1u) {
    CHECK(fixture_run(value, INVALID_INITIALIZERS[index]));
    CHECK(value->parse.status == W_SEED_PARSE_COMPLETE);
    CHECK(value->result.status != W_SEED_FRONTEND_OK);
  }

  const uint8_t sentinel = 0xacu;
  CHECK(required_arguments > 0u);
  CHECK(fixture_parse(value, SOURCE));
  fixture_configure_print_host(value);
  fixture_fill_output(value, sentinel);
  value->output.argument_capacity = required_arguments - 1u;
  (void)w_seed_frontend_run(&value->input, &value->output, &value->result);
  CHECK(value->result.status == W_SEED_FRONTEND_CAPACITY &&
        value->result.required.arguments == required_arguments &&
        fixture_output_is(value, sentinel, true));
  value->output.argument_capacity = TEST_ARGUMENTS;

  return true;
}

static bool test_u64_overflowing_products_frontend(void) {
  static const char SOURCE[] =
      "entry { "
      "let added = u64.overflowingAdd(18446744073709551615_u64, 1_u64) "
      "let subtracted = u64.overflowingSubtract(0_u64, 1_u64) "
      "let multiplied = u64.overflowingMultiply(18446744073709551615_u64, 2_u64) "
      "let negated = u64.overflowingNegate(1_u64) "
      "let powered = u64.overflowingPower(2_u64, 3_u64) "
      "let overflowPowered = u64.overflowingPower(2_u64, 64_u64) "
      "let zeroPowered = u64.overflowingPower(0_u64, 0_u64) "
      "let addedValue = added.0 let addedOverflow = added.1 "
      "let subtractedValue = subtracted.0 let subtractedOverflow = subtracted.1 "
      "let multipliedValue = multiplied.0 let multipliedOverflow = multiplied.1 "
      "let negatedValue = negated.0 let negatedOverflow = negated.1 "
      "let poweredValue = powered.0 let poweredOverflow = powered.1 "
      "let overflowPoweredValue = overflowPowered.0 "
      "let overflowPoweredOverflow = overflowPowered.1 "
      "let zeroPoweredValue = zeroPowered.0 let zeroPoweredOverflow = zeroPowered.1 "
      "}\n";
  fixture *value = &fixture_literal;
  CHECK(fixture_run(value, SOURCE));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));

  size_t add_count = 0u;
  size_t subtract_count = 0u;
  size_t multiply_count = 0u;
  size_t negate_count = 0u;
  size_t power_count = 0u;
  size_t wrapped_count = 0u;
  size_t overflowed_count = 0u;
  for (uint32_t index = 0u;
       (size_t)index < value->result.written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_CALL &&
        (expression->builtin_operation ==
             W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_ADD ||
         expression->builtin_operation ==
             W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_SUBTRACT ||
         expression->builtin_operation ==
             W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_MULTIPLY ||
         expression->builtin_operation ==
             W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_POWER ||
         expression->builtin_operation ==
             W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_NEGATE)) {
      const bool negate = expression->builtin_operation ==
                          W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_NEGATE;
      CHECK(expression->supported &&
            expression->argument_count == (negate ? 1u : 2u) &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_TUPLE &&
            frontend_text_is(value->types[expression->inferred_type].spelling,
                             "(u64, Bool)"));
      if (expression->builtin_operation ==
          W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_ADD)
        add_count += 1u;
      else if (expression->builtin_operation ==
               W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_SUBTRACT)
        subtract_count += 1u;
      else if (expression->builtin_operation ==
               W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_MULTIPLY)
        multiply_count += 1u;
      else if (expression->builtin_operation ==
               W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_POWER)
        power_count += 1u;
      else
        negate_count += 1u;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_MEMBER &&
               frontend_text_is(expression->member_name, "0")) {
      CHECK(expression->supported &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_INTEGER &&
            !value->types[expression->inferred_type].is_signed &&
            value->types[expression->inferred_type].bit_width == 64u);
      wrapped_count += 1u;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_MEMBER &&
               frontend_text_is(expression->member_name, "1")) {
      CHECK(expression->supported &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_BOOL);
      overflowed_count += 1u;
    }
  }
  CHECK(add_count == 1u && subtract_count == 1u && multiply_count == 1u &&
        negate_count == 1u && power_count == 3u && wrapped_count == 7u &&
        overflowed_count == 7u);

  static const char *const REJECTED[] = {
      "entry { let pair = u64.overflowingAdd(1_u64, 2_u64) "
      "let invalid = pair.2 }\n",
      "entry { let pair = u64.overflowingAdd(left: 1_u64, 2_u64) }\n",
      "entry { let pair = u64.overflowingAdd(1_u64) }\n",
      "entry { let pair = u64.overflowingAdd(1_u64, true) }\n",
      "entry { let pair = UInt.overflowingAdd(1_u64, 2_u64) }\n",
      "entry { let pair = u64.overflowingSubtract(1_u64, true) }\n",
      "entry { let pair = u64.overflowingMultiply(1_u64) }\n",
      "entry { let pair = u64.overflowingNegate(1_u64, 2_u64) }\n",
      "entry { let pair = u64.overflowingPower(1_u64) }\n",
      "entry { let pair = u64.overflowingPower(1_u64, true) }\n",
      "entry { let pair = UInt.overflowingPower(1_u64, 2_u64) }\n",
  };
  for (size_t index = 0u;
       index < sizeof(REJECTED) / sizeof(REJECTED[0]); index += 1u) {
    CHECK(fixture_run(value, REJECTED[index]));
    CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
          has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  }
  return true;
}

static bool test_u64_saturating_policy_frontend(void) {
  static const char SOURCE[] =
      "entry { let negZero = u64.saturatingNegate(0_u64) "
      "let negMaximum = u64.saturatingNegate(18446744073709551615_u64) "
      "let ordinary = u64.saturatingPower(2_u64, 3_u64) "
      "let clamped = u64.saturatingPower(2_u64, 64_u64) "
      "let zeroPowerZero = u64.saturatingPower(0_u64, 0_u64) }\n";
  fixture *value = &fixture_literal;
  CHECK(fixture_run(value, SOURCE));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  size_t negate_count = 0u;
  size_t power_count = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_CALL ||
        (expression->builtin_operation !=
             W_SEED_FRONTEND_BUILTIN_U64_SATURATING_NEGATE &&
         expression->builtin_operation !=
             W_SEED_FRONTEND_BUILTIN_U64_SATURATING_POWER))
      continue;
    const bool negate = expression->builtin_operation ==
                        W_SEED_FRONTEND_BUILTIN_U64_SATURATING_NEGATE;
    CHECK(expression->supported &&
          expression->argument_count == (negate ? 1u : 2u) &&
          expression->inferred_type < value->result.written.types &&
          value->types[expression->inferred_type].kind ==
              W_SEED_FRONTEND_TYPE_INTEGER &&
          !value->types[expression->inferred_type].is_signed &&
          value->types[expression->inferred_type].bit_width == 64u);
    if (negate)
      negate_count += 1u;
    else
      power_count += 1u;
  }
  CHECK(negate_count == 2u && power_count == 3u);

  static const char *const REJECTED[] = {
      "entry { let value = u64.saturatingNegate(1_u64, 2_u64) }\n",
      "entry { let value = u64.saturatingNegate(true) }\n",
      "entry { let value = u64.saturatingPower(1_u64) }\n",
      "entry { let value = u64.saturatingPower(1_u64, true) }\n",
      "entry { let value = UInt.saturatingPower(1_u64, 2_u64) }\n",
      "entry { let value = u64.saturatingPower(left: 1_u64, 2_u64) }\n",
  };
  for (size_t index = 0u;
       index < sizeof(REJECTED) / sizeof(REJECTED[0]); index += 1u) {
    CHECK(fixture_run(value, REJECTED[index]));
    CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
          has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  }
  return true;
}

static bool test_u64_bool_tuple_product_boundary_frontend(void) {
  static const char SOURCE[] =
      "export const product: (u64, Bool) = "
      "u64.overflowingAdd(18446744073709551615_u64, 1_u64)\n"
      "const fn direct(): (u64, Bool) { return "
      "u64.overflowingAdd(18446744073709551615_u64, 1_u64) }\n"
      "const fn spaced(): (u64 , Bool) { return "
      "u64.overflowingAdd(1_u64, 2_u64) }\n"
      "const fn forwarded(): (u64, Bool) { return direct() }\n"
      "const fn directValue(): u64 { return direct().0 }\n"
      "const fn directFlag(): Bool { return direct().1 }\n"
      "const fn moduleValue(): u64 { return product.0 }\n"
      "const fn moduleFlag(): Bool { return product.1 }\n"
      "const fn moduleRepeat(): Bool { return product.0 == product.0 }\n";
  fixture *value = &fixture_literal;
  CHECK(fixture_run(value, SOURCE));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written) &&
        value->result.written.const_declarations == 1u &&
        value->result.written.functions == 8u);

  const w_seed_frontend_const_declaration *declaration =
      &value->const_declarations[0];
  CHECK(declaration->exported && declaration->has_explicit_type &&
        declaration->declared_type < value->result.written.types &&
        declaration->effective_type == declaration->declared_type &&
        value->types[declaration->declared_type].kind ==
            W_SEED_FRONTEND_TYPE_TUPLE &&
        frontend_text_is(value->types[declaration->declared_type].spelling,
                         "(u64, Bool)"));

  CHECK(value->functions[0].return_type < value->result.written.types &&
        value->functions[1].return_type < value->result.written.types &&
        value->functions[2].return_type < value->result.written.types &&
        value->types[value->functions[0].return_type].kind ==
            W_SEED_FRONTEND_TYPE_TUPLE &&
        value->types[value->functions[1].return_type].kind ==
            W_SEED_FRONTEND_TYPE_TUPLE &&
        value->types[value->functions[2].return_type].kind ==
            W_SEED_FRONTEND_TYPE_TUPLE &&
        frontend_text_is(value->types[value->functions[0].return_type].spelling,
                         "(u64, Bool)") &&
        frontend_text_is(value->types[value->functions[1].return_type].spelling,
                         "(u64, Bool)") &&
        frontend_text_is(value->types[value->functions[2].return_type].spelling,
                         "(u64, Bool)"));
  for (size_t index = 3u; index < 8u; index += 1u)
    CHECK(value->functions[index].return_type < value->result.written.types &&
          value->types[value->functions[index].return_type].kind !=
              W_SEED_FRONTEND_TYPE_TUPLE);

  size_t builtin_count = 0u;
  size_t local_tuple_call_count = 0u;
  size_t module_const_read_count = 0u;
  size_t projection_zero_count = 0u;
  size_t projection_one_count = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_CALL &&
        expression->builtin_operation ==
            W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_ADD) {
      CHECK(expression->supported && expression->argument_count == 2u &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_TUPLE &&
            frontend_text_is(value->types[expression->inferred_type].spelling,
                             "(u64, Bool)"));
      builtin_count += 1u;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_CALL &&
               expression->resolved_function_index == 0u) {
      CHECK(expression->supported && expression->argument_count == 0u &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_TUPLE);
      local_tuple_call_count += 1u;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
               expression->resolved_const_declaration == 0u) {
      CHECK(expression->supported &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_TUPLE);
      module_const_read_count += 1u;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_MEMBER &&
               frontend_text_is(expression->member_name, "0")) {
      CHECK(expression->supported && expression->left != W_SEED_FRONTEND_NONE &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_INTEGER &&
            !value->types[expression->inferred_type].is_signed &&
            value->types[expression->inferred_type].bit_width == 64u);
      projection_zero_count += 1u;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_MEMBER &&
               frontend_text_is(expression->member_name, "1")) {
      CHECK(expression->supported && expression->left != W_SEED_FRONTEND_NONE &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_BOOL);
      projection_one_count += 1u;
    }
  }
  CHECK(builtin_count == 3u && local_tuple_call_count == 3u &&
        module_const_read_count == 4u && projection_zero_count == 4u &&
        projection_one_count == 2u);

  static const char *const REJECTED[] = {
      "const fn reversed(): (Bool, u64) { return "
      "u64.overflowingAdd(1_u64, 2_u64) }\n",
      "const fn widened(): (u64, u64) { return "
      "u64.overflowingAdd(1_u64, 2_u64) }\n",
      "const fn malformed(): (u64, Bool, Bool) { return "
      "u64.overflowingAdd(1_u64, 2_u64) }\n",
      "const inferredProduct = u64.overflowingAdd(1_u64, 2_u64)\n",
      "const missingOperand: (u64, Bool) = "
      "u64.overflowingAdd(1_u64)\n",
      "const wrongOperand: (u64, Bool) = "
      "u64.overflowingAdd(1_u64, true)\n",
  };
  for (size_t index = 0u;
       index < sizeof(REJECTED) / sizeof(REJECTED[0]); index += 1u) {
    CHECK(fixture_run(value, REJECTED[index]));
    CHECK(value->result.status != W_SEED_FRONTEND_OK &&
          (has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE) ||
           has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION) ||
           value->result.status == W_SEED_FRONTEND_DIAGNOSTICS));
  }
  return true;
}

static bool test_f64_scalar_projection(void) {
  fixture *value = &fixture_literal;
  CHECK(fixture_parse(
      value,
      "entry { let sum = 1.5 + 2.25_f64 let difference = 9.5 - 5.5 "
      "let product = 1.5 * 2.0 let quotient = 7.5e0 / 2.5 "
      "let negative = -0.0 let valid = sum == 3.75 && "
      "difference != 5.0 && product < 4.0 && quotient <= 3.0 && "
      "sum > 3.0 && sum >= 3.75 }\n"));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
            W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  size_t literal_count = 0u;
  size_t float_operator_count = 0u;
  bool saw_one_point_five = false;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_FLOAT) {
      CHECK(expression->supported && expression->has_float_value &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_FLOAT &&
            value->types[expression->inferred_type].bit_width == 64u);
      literal_count += 1u;
      if (expression->float_bits == UINT64_C(0x3ff8000000000000))
        saw_one_point_five = true;
    }
    if ((expression->kind == W_SEED_FRONTEND_EXPR_BINARY ||
         expression->kind == W_SEED_FRONTEND_EXPR_UNARY) &&
        expression->inferred_type < value->result.written.types &&
        value->types[expression->inferred_type].kind ==
            W_SEED_FRONTEND_TYPE_FLOAT)
      float_operator_count += 1u;
  }
  CHECK(literal_count >= 14u && float_operator_count == 5u &&
        saw_one_point_five);

  CHECK(fixture_parse(
      value,
      "entry { let integral = 1_f64 let subnormal = 5e-324_f64 "
      "let underflow = 1e-400_f64 let zero = 0.0_f64 }\n"));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
            W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  size_t strict_literal_count = 0u;
  bool saw_integral_bits = false;
  bool saw_subnormal_bits = false;
  bool saw_underflow_zero_bits = false;
  bool saw_zero_bits = false;
  size_t zero_bits_count = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_FLOAT) continue;
    CHECK(expression->supported && expression->has_float_value &&
          expression->inferred_type < value->result.written.types &&
          value->types[expression->inferred_type].kind ==
              W_SEED_FRONTEND_TYPE_FLOAT &&
          value->types[expression->inferred_type].bit_width == 64u);
    strict_literal_count += 1u;
    if (expression->float_bits == UINT64_C(0x3ff0000000000000))
      saw_integral_bits = true;
    if (expression->float_bits == UINT64_C(0x0000000000000001))
      saw_subnormal_bits = true;
    if (expression->float_bits == UINT64_C(0x0000000000000000)) {
      zero_bits_count += 1u;
      if (fixture_span_text_is(value, 0u, expression->span,
                               "1e-400_f64"))
        saw_underflow_zero_bits = true;
      if (fixture_span_text_is(value, 0u, expression->span, "0.0_f64"))
        saw_zero_bits = true;
    }
  }
  CHECK(strict_literal_count == 4u && saw_integral_bits &&
        saw_subnormal_bits && saw_underflow_zero_bits && saw_zero_bits &&
        zero_bits_count == 2u);

  CHECK(fixture_run(value, "entry { let overflow = 1e999_f64 }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  CHECK(fixture_parse(value, "entry { let hex = 0x1.0p0_f64 }\n"));
  (void)w_seed_frontend_run(&value->input, &value->output, &value->result);
  CHECK((value->parse.status != W_SEED_PARSE_COMPLETE &&
         value->result.status == W_SEED_FRONTEND_BARRIER) ||
        (value->parse.status == W_SEED_PARSE_COMPLETE &&
         value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
         has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION)));

  static const char *const mixed_sources[] = {
      "entry { let sum = 1_i32 + 2.0_f64 }\n",
      "entry { let equal = 1_f64 == 2_i32 }\n",
  };
  for (size_t index = 0u;
       index < sizeof(mixed_sources) / sizeof(mixed_sources[0]); index += 1u) {
    CHECK(fixture_run(value, mixed_sources[index]));
    CHECK(value->result.status == W_SEED_FRONTEND_OK &&
          counts_equal(&value->result.required, &value->result.written));
    size_t numeric_widen_count = 0u;
    for (size_t expression_index = 0u;
         expression_index < value->result.written.expressions;
         expression_index += 1u) {
      const w_seed_frontend_expression *expression =
          &value->expressions[expression_index];
      if (expression->kind != W_SEED_FRONTEND_EXPR_NUMERIC_WIDEN) continue;
      CHECK(!expression->numeric_widen_is_explicit &&
            expression->conversion_source_type <
                value->result.written.types &&
            expression->conversion_destination_type <
                value->result.written.types &&
            value->types[expression->conversion_source_type].kind ==
                W_SEED_FRONTEND_TYPE_INTEGER &&
            value->types[expression->conversion_source_type].is_signed &&
            value->types[expression->conversion_source_type].bit_width ==
                32u &&
            value->types[expression->conversion_destination_type].kind ==
                W_SEED_FRONTEND_TYPE_FLOAT &&
            value->types[expression->conversion_destination_type].bit_width ==
                64u);
      numeric_widen_count += 1u;
    }
    CHECK(numeric_widen_count == 1u);
  }

  CHECK(fixture_parse(value, "entry { let invalid = 1e999 }\n"));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
            W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  return true;
}

static bool test_f32_scalar_projection(void) {
  fixture *value = &fixture_literal;
  static const char SOURCE[] =
      "entry { let sum = 1.5_f32 + 2.25_f32 "
      "let difference = 9.5_f32 - 5.5_f32 "
      "let product = 1.5_f32 * 2.0_f32 "
      "let quotient = 7.5e0_f32 / 2.5_f32 "
      "let negativeZero = -0.0_f32 "
      "let underflowNegative = -1e-50_f32 "
      "let nan = 0.0_f32 / 0.0_f32 "
      "let valid = sum == 3.75_f32 && difference != 5.0_f32 && "
      "product < 4.0_f32 && quotient <= 3.0_f32 && "
      "sum > 3.0_f32 && sum >= 3.75_f32 && nan != nan }\n";
  CHECK(fixture_parse(value, SOURCE));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
            W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  size_t literal_count = 0u;
  size_t arithmetic_and_unary_count = 0u;
  size_t comparison_count = 0u;
  bool saw_one_point_five = false;
  bool saw_negative_zero_operation = false;
  bool saw_negative_underflow_operation = false;
  bool saw_nan_runtime_divide = false;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_FLOAT) {
      CHECK(expression->supported && expression->has_float_value &&
            expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_FLOAT &&
            value->types[expression->inferred_type].bit_width == 32u &&
            (expression->float_bits >> 32u) == 0u);
      literal_count += 1u;
      if (expression->float_bits == UINT64_C(0x3fc00000))
        saw_one_point_five = true;
    } else if ((expression->kind == W_SEED_FRONTEND_EXPR_BINARY ||
                expression->kind == W_SEED_FRONTEND_EXPR_UNARY) &&
               expression->inferred_type < value->result.written.types &&
               value->types[expression->inferred_type].kind ==
                   W_SEED_FRONTEND_TYPE_FLOAT) {
      CHECK(value->types[expression->inferred_type].bit_width == 32u);
      arithmetic_and_unary_count += 1u;
      if (expression->kind == W_SEED_FRONTEND_EXPR_UNARY &&
          frontend_text_is(expression->operator_text, "-") &&
          expression->left < value->result.written.expressions &&
          value->expressions[expression->left].kind ==
              W_SEED_FRONTEND_EXPR_FLOAT &&
          value->expressions[expression->left].float_bits == 0u) {
        if (fixture_span_text_is(value, 0u, expression->span,
                                 "-0.0_f32"))
          saw_negative_zero_operation = true;
        if (fixture_span_text_is(value, 0u, expression->span,
                                 "-1e-50_f32"))
          saw_negative_underflow_operation = true;
      }
      if (expression->kind == W_SEED_FRONTEND_EXPR_BINARY &&
          frontend_text_is(expression->operator_text, "/"))
        saw_nan_runtime_divide = true;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_BINARY &&
               (frontend_text_is(expression->operator_text, "==") ||
                frontend_text_is(expression->operator_text, "!=") ||
                frontend_text_is(expression->operator_text, "<") ||
                frontend_text_is(expression->operator_text, "<=") ||
                frontend_text_is(expression->operator_text, ">") ||
                frontend_text_is(expression->operator_text, ">="))) {
      CHECK(expression->inferred_type < value->result.written.types &&
            value->types[expression->inferred_type].kind ==
                W_SEED_FRONTEND_TYPE_BOOL);
      comparison_count += 1u;
    }
  }
  CHECK(literal_count >= 18u && arithmetic_and_unary_count >= 7u &&
        comparison_count == 7u && saw_one_point_five &&
        saw_negative_zero_operation && saw_negative_underflow_operation &&
        saw_nan_runtime_divide);

  CHECK(fixture_parse(
      value,
      "entry { let integral = 1_f32 let smallest = "
      "1.401298464324817070923729583289916131280e-45_f32 "
      "let underflow = 1e-50_f32 let midpointDown = "
      "1.000000059604644775390625_f32 let midpointUp = "
      "1.000000178813934326171875_f32 }\n"));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
            W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  size_t exact_literal_count = 0u;
  bool saw_integral_bits = false;
  bool saw_smallest_subnormal_bits = false;
  bool saw_underflow_positive_zero = false;
  bool saw_first_midpoint_even_lower = false;
  bool saw_second_midpoint_even_upper = false;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_FLOAT) continue;
    CHECK(expression->supported && expression->has_float_value &&
          expression->inferred_type < value->result.written.types &&
          value->types[expression->inferred_type].kind ==
              W_SEED_FRONTEND_TYPE_FLOAT &&
          value->types[expression->inferred_type].bit_width == 32u &&
          (expression->float_bits >> 32u) == 0u);
    exact_literal_count += 1u;
    if (expression->float_bits == UINT64_C(0x3f800000) &&
        fixture_span_text_is(value, 0u, expression->span, "1_f32"))
      saw_integral_bits = true;
    if (expression->float_bits == UINT64_C(0x00000001))
      saw_smallest_subnormal_bits = true;
    if (expression->float_bits == 0u &&
        fixture_span_text_is(value, 0u, expression->span, "1e-50_f32"))
      saw_underflow_positive_zero = true;
    if (expression->float_bits == UINT64_C(0x3f800000))
      saw_first_midpoint_even_lower = true;
    if (expression->float_bits == UINT64_C(0x3f800002))
      saw_second_midpoint_even_upper = true;
  }
  CHECK(exact_literal_count == 5u && saw_integral_bits &&
        saw_smallest_subnormal_bits && saw_underflow_positive_zero &&
        saw_first_midpoint_even_lower && saw_second_midpoint_even_upper);

  CHECK(fixture_run(value,
                    "entry { let sum = 1.0_f32 + 2.0_f64 "
                    "let equal = 1.0_f32 == 1.0_f64 }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  size_t mixed_float_widens = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_NUMERIC_WIDEN) continue;
    CHECK(!expression->numeric_widen_is_explicit &&
          expression->conversion_source_type < value->result.written.types &&
          expression->conversion_destination_type <
              value->result.written.types &&
          value->types[expression->conversion_source_type].kind ==
              W_SEED_FRONTEND_TYPE_FLOAT &&
          value->types[expression->conversion_source_type].bit_width == 32u &&
          value->types[expression->conversion_destination_type].kind ==
              W_SEED_FRONTEND_TYPE_FLOAT &&
          value->types[expression->conversion_destination_type].bit_width ==
              64u);
    mixed_float_widens += 1u;
  }
  CHECK(mixed_float_widens == 2u);

  /* C numeric parsing and IEEE conversion ignore the caller's locale and
   * rounding mode. Verify the ties-to-even cases while FE_DOWNWARD is active
   * when that mode is available, then verify the frontend restores it. */
  const int saved_rounding = fegetround();
  if (saved_rounding != -1 && fesetround(FE_DOWNWARD) == 0) {
    const bool parsed = fixture_parse(
        value,
        "entry { let lower = 1.000000059604644775390625_f32 "
        "let upper = 1.000000178813934326171875_f32 }\n");
    const w_seed_frontend_status status =
        parsed ? w_seed_frontend_run(&value->input, &value->output,
                                     &value->result)
               : W_SEED_FRONTEND_INVALID;
    bool saw_lower = false;
    bool saw_upper = false;
    for (size_t index = 0u; index < value->result.written.expressions;
         index += 1u) {
      const w_seed_frontend_expression *expression = &value->expressions[index];
      if (expression->kind != W_SEED_FRONTEND_EXPR_FLOAT) continue;
      if (expression->float_bits == UINT64_C(0x3f800000)) saw_lower = true;
      if (expression->float_bits == UINT64_C(0x3f800002)) saw_upper = true;
    }
    const int frontend_rounding = fegetround();
    const bool restored = fesetround(saved_rounding) == 0;
    CHECK(restored && frontend_rounding == FE_DOWNWARD && parsed &&
          status == W_SEED_FRONTEND_OK && saw_lower && saw_upper);
  }

  static const char *const REJECTED[] = {
      "entry { let overflow = 1e999_f32 }\n",
      "entry { let hex = 0x1.0p0_f32 }\n",
      "entry { let mixed = 1_i32 + 2.0_f32 }\n",
      "entry { let mixed = 1.0_f32 == 2_i32 }\n",
      "entry { let remainder = 1.0_f32 % 2.0_f32 }\n",
      "entry { let powered = 1.0_f32 ** 2.0_f32 }\n",
      "entry { let bitwise = 1.0_f32 & 2.0_f32 }\n",
      "entry { let cast = f32(1.0_f64) }\n",
      "entry { let cast = i32(1.0_f32) }\n",
      "entry { let rendered = \"${1.0_f32}\" }\n",
  };
  for (size_t index = 0u; index < sizeof(REJECTED) / sizeof(REJECTED[0]);
       index += 1u) {
    CHECK(fixture_run(value, REJECTED[index]));
    CHECK(value->result.status != W_SEED_FRONTEND_OK &&
          (has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION) ||
           has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE) ||
           value->result.status == W_SEED_FRONTEND_DIAGNOSTICS));
  }
  return true;
}

static bool test_numeric_widening_frontend(void) {
  CHECK(strcmp(W_SEED_FRONTEND_SCHEMA_VERSION, "w-seed-frontend-76") == 0);
  typedef struct {
    const char *source_name;
    bool source_is_float;
    bool source_is_signed;
    uint16_t source_width;
    const char *destination_name;
    uint16_t destination_width;
  } numeric_route_case;
  static const numeric_route_case ROUTES[] = {
      {"i8", false, true, 8u, "f32", 32u},
      {"u8", false, false, 8u, "f32", 32u},
      {"i16", false, true, 16u, "f32", 32u},
      {"u16", false, false, 16u, "f32", 32u},
      {"f32", true, false, 32u, "f64", 64u},
      {"i8", false, true, 8u, "f64", 64u},
      {"u8", false, false, 8u, "f64", 64u},
      {"i16", false, true, 16u, "f64", 64u},
      {"u16", false, false, 16u, "f64", 64u},
      {"i32", false, true, 32u, "f64", 64u},
      {"u32", false, false, 32u, "f64", 64u},
  };
  fixture *value = &fixture_literal;
  char source[256];
  char receipt_line[128];
  for (size_t route_index = 0u;
       route_index < sizeof(ROUTES) / sizeof(ROUTES[0]); route_index += 1u) {
    for (size_t explicit_index = 0u; explicit_index < 2u;
         explicit_index += 1u) {
      const bool explicit_surface = explicit_index != 0u;
      const int written = explicit_surface
          ? snprintf(source, sizeof(source),
                     "fn f(value: %s): %s { return %s(value) } entry(f)\n",
                     ROUTES[route_index].source_name,
                     ROUTES[route_index].destination_name,
                     ROUTES[route_index].destination_name)
          : snprintf(source, sizeof(source),
                     "fn f(value: %s): %s { return value } entry(f)\n",
                     ROUTES[route_index].source_name,
                     ROUTES[route_index].destination_name);
      CHECK(written > 0 && (size_t)written < sizeof(source));
      CHECK(fixture_run(value, source));
      CHECK(value->result.status == W_SEED_FRONTEND_OK &&
            counts_equal(&value->result.required, &value->result.written));
      const w_seed_frontend_expression *wrapper = NULL;
      size_t wrapper_index = 0u;
      size_t wrapper_count = 0u;
      for (size_t expression_index = 0u;
           expression_index < value->result.written.expressions;
           expression_index += 1u) {
        const w_seed_frontend_expression *candidate =
            &value->expressions[expression_index];
        if (candidate->kind != W_SEED_FRONTEND_EXPR_NUMERIC_WIDEN) continue;
        wrapper = candidate;
        wrapper_index = expression_index;
        wrapper_count += 1u;
      }
      CHECK(wrapper_count == 1u && wrapper != NULL &&
            wrapper->supported &&
            wrapper->left < value->result.written.expressions &&
            wrapper->right == W_SEED_FRONTEND_NONE &&
            wrapper->conversion_source_type < value->result.written.types &&
            wrapper->conversion_destination_type <
                value->result.written.types &&
            wrapper->inferred_type ==
                wrapper->conversion_destination_type &&
            wrapper->numeric_widen_is_explicit == explicit_surface);
      const w_seed_frontend_expression *child =
          &value->expressions[wrapper->left];
      const w_seed_frontend_type *source_type =
          &value->types[wrapper->conversion_source_type];
      const w_seed_frontend_type *destination_type =
          &value->types[wrapper->conversion_destination_type];
      CHECK(child->inferred_type == wrapper->conversion_source_type &&
            destination_type->kind == W_SEED_FRONTEND_TYPE_FLOAT &&
            destination_type->bit_width ==
                ROUTES[route_index].destination_width);
      if (ROUTES[route_index].source_is_float) {
        CHECK(source_type->kind == W_SEED_FRONTEND_TYPE_FLOAT &&
              source_type->bit_width == ROUTES[route_index].source_width);
      } else {
        CHECK(source_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
              source_type->is_signed ==
                  ROUTES[route_index].source_is_signed &&
              source_type->bit_width == ROUTES[route_index].source_width);
      }
      const int receipt_length = snprintf(
          receipt_line, sizeof(receipt_line),
          "numeric-widen=%llu|source=%u|destination=%u|surface=%s\n",
          (unsigned long long)wrapper_index,
          wrapper->conversion_source_type,
          wrapper->conversion_destination_type,
          explicit_surface ? "explicit" : "implicit");
      CHECK(receipt_length > 0 &&
            (size_t)receipt_length < sizeof(receipt_line) &&
            receipt_contains(value, receipt_line, (size_t)receipt_length));
    }
  }

  CHECK(fixture_run(
      value,
      "fn returnValue(value: i8): f32 { return value }\n"
      "fn bindingValue(value: u16): f32 { let result: f32 = value "
      "return result }\n"
      "fn sink(value: f64): f64 { return value }\n"
      "fn argumentValue(value: i32): f64 { return sink(value: value) }\n"
      "fn mixedSum(value: u32, offset: f64): f64 { return value + offset }\n"
      "fn mixedComparison(value: u16, limit: f32): Bool { "
      "return value < limit }\n"
      "entry {}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  size_t contextual_widens = 0u;
  bool saw_return = false;
  bool saw_binding = false;
  bool saw_argument = false;
  bool saw_mixed_arithmetic = false;
  bool saw_mixed_comparison = false;
  for (size_t expression_index = 0u;
       expression_index < value->result.written.expressions;
       expression_index += 1u) {
    const w_seed_frontend_expression *wrapper =
        &value->expressions[expression_index];
    if (wrapper->kind != W_SEED_FRONTEND_EXPR_NUMERIC_WIDEN) continue;
    CHECK(!wrapper->numeric_widen_is_explicit &&
          wrapper->conversion_source_type < value->result.written.types &&
          wrapper->conversion_destination_type < value->result.written.types);
    contextual_widens += 1u;
    for (size_t statement_index = 0u;
         statement_index < value->result.written.statements;
         statement_index += 1u) {
      const w_seed_frontend_statement *statement =
          &value->statements[statement_index];
      if (statement->expression_index != expression_index) continue;
      if (statement->kind == W_SEED_FRONTEND_STMT_RETURN) saw_return = true;
      if (statement->kind == W_SEED_FRONTEND_STMT_LET) saw_binding = true;
    }
    for (size_t argument_index = 0u;
         argument_index < value->result.written.arguments;
         argument_index += 1u) {
      if (value->arguments[argument_index].expression_index == expression_index)
        saw_argument = true;
    }
    for (size_t parent_index = 0u;
         parent_index < value->result.written.expressions; parent_index += 1u) {
      const w_seed_frontend_expression *parent =
          &value->expressions[parent_index];
      if (parent->kind != W_SEED_FRONTEND_EXPR_BINARY ||
          (parent->left != expression_index &&
           parent->right != expression_index))
        continue;
      if (frontend_text_is(parent->operator_text, "+"))
        saw_mixed_arithmetic = true;
      if (frontend_text_is(parent->operator_text, "<"))
        saw_mixed_comparison = true;
    }
  }
  CHECK(contextual_widens == 5u && saw_return && saw_binding && saw_argument &&
        saw_mixed_arithmetic && saw_mixed_comparison);

  static const struct {
    const char *source;
    bool explicit_widen;
  } BINDING_READS[] = {
      {"entry { let widened: f64 = 1.5_f32 "
       "let valid = widened == 1.5_f64 }\n", false},
      {"entry { let widened = f64(1.5_f32) "
       "let valid = widened == 1.5_f64 }\n", true},
      {"entry { let widened = (2_i32 + 0.5_f64) "
       "let valid = widened == 2.5_f64 }\n", false},
  };
  for (size_t case_index = 0u;
       case_index < sizeof(BINDING_READS) / sizeof(BINDING_READS[0]);
       case_index += 1u) {
    CHECK(fixture_run(value, BINDING_READS[case_index].source));
    CHECK(value->result.status == W_SEED_FRONTEND_OK &&
          counts_equal(&value->result.required, &value->result.written));
    size_t widen_count = 0u;
    bool saw_read_comparison = false;
    for (size_t expression_index = 0u;
         expression_index < value->result.written.expressions;
         expression_index += 1u) {
      const w_seed_frontend_expression *expression =
          &value->expressions[expression_index];
      if (expression->kind == W_SEED_FRONTEND_EXPR_NUMERIC_WIDEN) {
        CHECK(expression->numeric_widen_is_explicit ==
              BINDING_READS[case_index].explicit_widen);
        widen_count += 1u;
      }
      if (expression->kind == W_SEED_FRONTEND_EXPR_BINARY &&
          frontend_text_is(expression->operator_text, "==") &&
          expression->left < value->result.written.expressions &&
          value->expressions[expression->left].kind ==
              W_SEED_FRONTEND_EXPR_IDENTIFIER &&
          frontend_text_is(value->expressions[expression->left].spelling,
                           "widened"))
        saw_read_comparison = true;
    }
    CHECK(widen_count == 1u && saw_read_comparison);
  }
  CHECK(fixture_parse(
      value,
      "entry { let widened: f64 = 1.5_f32 "
      "let valid = widened == 1.5_f64 "
      "if valid { print(\"ok\") } else { print(\"bad\") } }\n"));
  fixture_configure_print_host(value);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  CHECK(fixture_parse(
      value,
      "entry { let widenedFloat: f64 = 1.5_f32 "
      "let explicitFloat = f64(1.25_f32) "
      "let valid = widenedFloat == 1.5_f64 && "
      "explicitFloat == 1.25_f64 "
      "if valid { print(\"ok\") } else { print(\"bad\") } }\n"));
  fixture_configure_print_host(value);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));

  CHECK(fixture_run(
      value,
      "entry { let mixedInteger = 2_i32 + 0.5_f64 "
      "let mixedFloat = 1.5_f32 + 2.25_f64 "
      "let mixedComparison = 65535_u16 == 65535.0_f32 "
      "let valid = mixedInteger == 2.5_f64 && "
      "mixedFloat == 3.75_f64 && mixedComparison }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  size_t inferred_f64_bindings = 0u;
  bool saw_inferred_bool_binding = false;
  for (size_t statement_index = 0u;
       statement_index < value->result.written.statements;
       statement_index += 1u) {
    const w_seed_frontend_statement *statement =
        &value->statements[statement_index];
    if (statement->kind != W_SEED_FRONTEND_STMT_LET ||
        statement->effective_type >= value->result.written.types)
      continue;
    const w_seed_frontend_type *type = &value->types[statement->effective_type];
    if ((frontend_text_is(statement->binding_name, "mixedInteger") ||
         frontend_text_is(statement->binding_name, "mixedFloat")) &&
        type->kind == W_SEED_FRONTEND_TYPE_FLOAT && type->bit_width == 64u)
      inferred_f64_bindings += 1u;
    if (frontend_text_is(statement->binding_name, "mixedComparison") &&
        type->kind == W_SEED_FRONTEND_TYPE_BOOL)
      saw_inferred_bool_binding = true;
  }
  CHECK(inferred_f64_bindings == 2u && saw_inferred_bool_binding);

  CHECK(fixture_run(
      value,
      "entry { let exact32: f32 = 16777216 "
      "let exact64: f64 = 9007199254740992 }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  bool saw_exact_f32_integer = false;
  bool saw_exact_f64_integer = false;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_FLOAT) continue;
    if (expression->float_bits == UINT64_C(0x4b800000))
      saw_exact_f32_integer = true;
    if (expression->float_bits == UINT64_C(0x4340000000000000))
      saw_exact_f64_integer = true;
  }
  CHECK(saw_exact_f32_integer && saw_exact_f64_integer);

  static const char *const REJECTED[] = {
      "fn f(value: i32): f32 { return value } entry(f)\n",
      "fn f(value: u32): f32 { return value } entry(f)\n",
      "fn f(value: i64): f64 { return value } entry(f)\n",
      "fn f(value: u64): f64 { return value } entry(f)\n",
      "fn f(value: Int): f64 { return value } entry(f)\n",
      "fn f(value: UInt): f64 { return value } entry(f)\n",
      "fn f(value: f64): f32 { return value } entry(f)\n",
      "fn f(value: f32): i32 { return value } entry(f)\n",
      "entry { let rejected = f32(1.0_f64) "
      "let read = rejected == 1.0_f32 }\n",
      "fn f(value: i32): f32 { return f32(value) } entry(f)\n",
      "fn f(value: i64): f64 { return f64(value) } entry(f)\n",
      "fn f(value: f64): f32 { return f32(value) } entry(f)\n",
      "entry { let notExact32: f32 = 16777217 }\n",
      "entry { let notExact64: f64 = 9007199254740993 }\n",
      "entry { let notExact32 = f32(16777217) }\n",
      "entry { let notExact64 = f64(9007199254740993) }\n",
  };
  for (size_t index = 0u; index < sizeof(REJECTED) / sizeof(REJECTED[0]);
       index += 1u) {
    CHECK(fixture_run(value, REJECTED[index]));
    CHECK(value->result.status != W_SEED_FRONTEND_OK);
    for (size_t expression_index = 0u;
         expression_index < value->result.written.expressions;
         expression_index += 1u)
      CHECK(value->expressions[expression_index].kind !=
            W_SEED_FRONTEND_EXPR_NUMERIC_WIDEN);
  }
  return true;
}

static bool test_f64_locale_isolation(void) {
  enum { TEST_LOCALE_NAME_BYTES = 128 };
  const char *current_locale = setlocale(LC_NUMERIC, NULL);
  CHECK(current_locale != NULL);
  const size_t current_length = strlen(current_locale);
  CHECK(current_length < TEST_LOCALE_NAME_BYTES);
  char saved_locale[TEST_LOCALE_NAME_BYTES];
  (void)memcpy(saved_locale, current_locale, current_length + 1u);

  /* Use names accepted by the maintained Windows/MSVC and POSIX bootstrap
   * hosts. A host without an installed non-C locale skips this targeted branch
   * after restoring its original locale; the available-locale branch proves
   * both decimal interpretation and process-locale preservation. */
  static const char *const candidates[] = {
      "de-DE", "German_Germany.1252", "de_DE.UTF-8", "de_DE.utf8",
      "de_DE", "pt_BR.UTF-8", "pt_BR", "fr-FR", "French_France.1252",
      "fr_FR.UTF-8", "fr_FR"};
  char active_locale[TEST_LOCALE_NAME_BYTES];
  bool has_non_c_locale = false;
  for (size_t index = 0u; index < sizeof(candidates) / sizeof(candidates[0]);
       index += 1u) {
    const char *candidate = setlocale(LC_NUMERIC, candidates[index]);
    if (candidate == NULL || strcmp(candidate, "C") == 0 ||
        strcmp(candidate, "POSIX") == 0)
      continue;
    const size_t candidate_length = strlen(candidate);
    if (candidate_length >= sizeof(active_locale)) continue;
    (void)memcpy(active_locale, candidate, candidate_length + 1u);
    has_non_c_locale = true;
    break;
  }
  if (!has_non_c_locale) {
    CHECK(setlocale(LC_NUMERIC, saved_locale) != NULL);
    return true;
  }

  fixture *value = &fixture_literal;
  const bool parsed = fixture_parse(
      value, "entry { let locale_value = 1.5_f64 "
             "let locale_value_f32 = 1.5_f32 }\n");
  w_seed_frontend_status status = W_SEED_FRONTEND_INVALID;
  bool saw_expected_value = false;
  bool saw_expected_f32_value = false;
  if (parsed) {
    status = w_seed_frontend_run(&value->input, &value->output,
                                 &value->result);
    for (size_t index = 0u; index < value->result.written.expressions;
         index += 1u) {
      const w_seed_frontend_expression *expression =
          &value->expressions[index];
      if (expression->kind == W_SEED_FRONTEND_EXPR_FLOAT &&
          expression->supported && expression->has_float_value &&
          expression->float_bits == UINT64_C(0x3ff8000000000000)) {
        saw_expected_value = true;
      }
      if (expression->kind == W_SEED_FRONTEND_EXPR_FLOAT &&
          expression->supported && expression->has_float_value &&
          expression->float_bits == UINT64_C(0x3fc00000)) {
        saw_expected_f32_value = true;
      }
    }
  }
  const char *after_locale = setlocale(LC_NUMERIC, NULL);
  const bool locale_unchanged =
      after_locale != NULL && strcmp(after_locale, active_locale) == 0;
  const bool restored = setlocale(LC_NUMERIC, saved_locale) != NULL;
  CHECK(restored);
  CHECK(parsed && status == W_SEED_FRONTEND_OK && saw_expected_value &&
        saw_expected_f32_value && locale_unchanged);
  return true;
}

typedef enum {
  PROCESS_NEGATIVE_NONCONST = 0,
  PROCESS_NEGATIVE_MODULE,
  PROCESS_NEGATIVE_TYPE,
  PROCESS_NEGATIVE_RECEIVER,
  PROCESS_NEGATIVE_RETURN,
  PROCESS_NEGATIVE_PARAMETERS,
  PROCESS_NEGATIVE_EXPORT,
  PROCESS_NEGATIVE_METADATA,
} process_negative_case;

static bool process_negative_fixture(
    fixture *value, const char *source, process_negative_case negative_case) {
  CHECK(fixture_parse(value, source));
  fixture_configure_process_abi_external(value);
  CHECK(fixture_resolve_external_imports(value));
  switch (negative_case) {
    case PROCESS_NEGATIVE_NONCONST:
      value->external_symbols[3].is_const = false;
      break;
    case PROCESS_NEGATIVE_MODULE:
      value->external_modules[0].module_id =
          (w_seed_frontend_text){"alt.process", 11u};
      break;
    case PROCESS_NEGATIVE_TYPE:
      /* The source alias resolves to Context, not ExitCode. */
      break;
    case PROCESS_NEGATIVE_RECEIVER:
      value->external_symbols[3].receiver_type =
          (w_seed_frontend_text){"Context", 7u};
      break;
    case PROCESS_NEGATIVE_RETURN:
      value->external_symbols[3].return_type =
          (w_seed_frontend_text){"Context", 7u};
      break;
    case PROCESS_NEGATIVE_PARAMETERS:
      value->external_parameters[0] =
          (w_seed_frontend_external_parameter){
              .name = (w_seed_frontend_text){"reason", 6u},
              .type = (w_seed_frontend_text){"Context", 7u},
              .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
      value->external_symbols[3].parameters = value->external_parameters;
      value->external_symbols[3].parameter_count = 1u;
      break;
    case PROCESS_NEGATIVE_EXPORT:
      value->external_symbols[3].exported = false;
      break;
    case PROCESS_NEGATIVE_METADATA:
      value->external_symbols[3].receiver_type =
          (w_seed_frontend_text){NULL, 1u};
      fixture_fill_output(value, 0xa5u);
      break;
  }
  const w_seed_frontend_status status =
      w_seed_frontend_run(&value->input, &value->output, &value->result);
  if (negative_case == PROCESS_NEGATIVE_METADATA) {
    CHECK(status == W_SEED_FRONTEND_INVALID);
    CHECK(fixture_output_is(value, 0xa5u, true));
  } else {
    CHECK(status == W_SEED_FRONTEND_UNSUPPORTED);
  }
  return true;
}

static bool test_process_abi_duplicate_import_aliases(const char *source) {
  fixture *value = &fixture_callback;
  CHECK(fixture_parse(value, source));
  fixture_configure_process_abi_external(value);
  CHECK(fixture_resolve_external_imports(value));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(value, W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL));
  return true;
}

static bool test_process_abi_alias_and_exit_case(void) {
  static const char source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  fixture *value = &fixture_external;
  CHECK(fixture_parse(value, source));
  fixture_configure_process_abi_external(value);
  CHECK(fixture_resolve_external_imports(value));
  const w_seed_frontend_status process_status =
      w_seed_frontend_run(&value->input, &value->output, &value->result);
  CHECK(process_status == W_SEED_FRONTEND_OK);
  CHECK(value->result.written.imports == 1u &&
        value->result.written.import_items == 3u &&
        value->result.written.functions == 1u &&
        value->result.written.parameters == 2u);
  const w_seed_frontend_function *function = &value->functions[0];
  CHECK(function->is_async && function->parameter_count == 2u &&
        function->return_type != W_SEED_FRONTEND_NONE &&
        function->return_type < value->result.written.types);
  const w_seed_frontend_type *return_type =
      &value->types[function->return_type];
  CHECK(return_type->kind == W_SEED_FRONTEND_TYPE_NOMINAL &&
        frontend_text_is(return_type->spelling, "ProcessExitCode") &&
        return_type->external_module_index == 0u &&
        return_type->external_symbol_index == 2u);
  CHECK(value->parameters[0].type_index < value->result.written.types &&
        value->parameters[1].type_index < value->result.written.types);
  CHECK(value->types[value->parameters[0].type_index].external_module_index ==
            0u &&
        value->types[value->parameters[0].type_index].external_symbol_index ==
            0u &&
        frontend_text_is(value->types[value->parameters[0].type_index].spelling,
                          "ProcessArguments"));
  CHECK(value->types[value->parameters[1].type_index].external_module_index ==
            0u &&
        value->types[value->parameters[1].type_index].external_symbol_index ==
            1u &&
        frontend_text_is(value->types[value->parameters[1].type_index].spelling,
                          "ProcessContext"));
  uint32_t case_expression = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE) {
      CHECK(case_expression == W_SEED_FRONTEND_NONE);
      case_expression = (uint32_t)index;
    }
  }
  CHECK(case_expression != W_SEED_FRONTEND_NONE);
  const w_seed_frontend_expression *case_value =
      &value->expressions[case_expression];
  CHECK(case_value->supported && case_value->enum_index ==
            W_SEED_FRONTEND_NONE &&
        case_value->enum_case_index == W_SEED_FRONTEND_NONE &&
        case_value->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL &&
        case_value->resolved_external_module_index == 0u &&
        case_value->resolved_external_symbol_index == 3u &&
        frontend_text_is(case_value->member_name, "success"));
  CHECK(receipt_contains(value, "|external=0:2|enum-base=",
                         strlen("|external=0:2|enum-base=")) &&
        receipt_contains(value, "enum-case=", strlen("enum-case=")));
  for (size_t type_index = 0u; type_index < value->result.written.types;
       type_index += 1u) {
    const w_seed_frontend_type *type = &value->types[type_index];
    const bool module_none =
        type->external_module_index == W_SEED_FRONTEND_NONE;
    const bool symbol_none =
        type->external_symbol_index == W_SEED_FRONTEND_NONE;
    CHECK(module_none == symbol_none);
    if (!module_none) {
      CHECK(type->kind == W_SEED_FRONTEND_TYPE_NOMINAL &&
            type->external_module_index < value->input.external_module_count);
      const w_seed_frontend_external_module *module =
          &value->external_modules[type->external_module_index];
      CHECK(type->external_symbol_index < module->symbol_count &&
            module->symbols[type->external_symbol_index].kind ==
                W_SEED_FRONTEND_EXTERNAL_TYPE &&
            module->symbols[type->external_symbol_index].exported);
    }
  }
  const size_t canonical_receipt_bytes = value->result.receipt_bytes;
  const w_seed_frontend_counts canonical_required = value->result.required;
  uint8_t canonical_receipt[TEST_RECEIPT];
  CHECK(canonical_receipt_bytes <= sizeof(canonical_receipt));
  (void)memcpy(canonical_receipt, value->receipt, canonical_receipt_bytes);

  /* Re-running the same resolver input must reproduce the exact receipt. */
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.receipt_bytes == canonical_receipt_bytes &&
        memcmp(value->receipt, canonical_receipt, canonical_receipt_bytes) ==
            0);

  w_seed_frontend_counts measured;
  w_seed_frontend_result measured_result;
  CHECK(w_seed_frontend_measure(&value->input, &measured, &measured_result) ==
        W_SEED_FRONTEND_OK);
  CHECK(counts_equal(&measured, &value->result.required) &&
        measured_result.required.receipt_bytes == value->result.receipt_bytes);

  /* A short receipt buffer is a pre-emit capacity barrier.  It must preserve
   * every caller-owned output slot and still report the same requirement. */
  const size_t receipt_capacity = value->output.receipt_capacity;
  CHECK(canonical_receipt_bytes > 0u && canonical_receipt_bytes < receipt_capacity);
  fixture_fill_output(value, 0xa5u);
  value->output.receipt_capacity = canonical_receipt_bytes - 1u;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_CAPACITY);
  CHECK(counts_equal(&value->result.required, &canonical_required) &&
        value->result.receipt_bytes == canonical_receipt_bytes &&
        fixture_output_is(value, 0xa5u, true));
  value->output.receipt_capacity = receipt_capacity;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.receipt_bytes == canonical_receipt_bytes &&
        memcmp(value->receipt, canonical_receipt, canonical_receipt_bytes) ==
            0);

  /* Alias spelling is source provenance, while external identity remains the
   * resolver-owned 0:2 pair.  A valid alias mutation changes the receipt
   * deterministically without changing the accepted ABI shape. */
  static const char alias_variant_source[] =
      "import { Arguments as ProcessArgs, Context as ProcessCtx, "
      "ExitCode as ProcessStatus } from std.process\n"
      "async fn run(args: ProcessArgs, ctx: ProcessCtx): ProcessStatus { "
      "return .success }\n"
      "entry(run)\n";
  fixture *variant = &fixture_b;
  CHECK(fixture_parse(variant, alias_variant_source));
  fixture_configure_process_abi_external(variant);
  CHECK(fixture_resolve_external_imports(variant));
  CHECK(w_seed_frontend_run(&variant->input, &variant->output,
                            &variant->result) == W_SEED_FRONTEND_OK);
  CHECK(variant->result.receipt_bytes != canonical_receipt_bytes ||
        memcmp(variant->receipt, canonical_receipt, canonical_receipt_bytes) !=
            0);
  const size_t variant_receipt_bytes = variant->result.receipt_bytes;
  uint8_t variant_receipt[TEST_RECEIPT];
  CHECK(variant_receipt_bytes <= sizeof(variant_receipt));
  (void)memcpy(variant_receipt, variant->receipt, variant_receipt_bytes);
  CHECK(w_seed_frontend_run(&variant->input, &variant->output,
                            &variant->result) == W_SEED_FRONTEND_OK);
  CHECK(variant->result.receipt_bytes == variant_receipt_bytes &&
        memcmp(variant->receipt, variant_receipt, variant_receipt_bytes) == 0);

  static const char canonical_negative_source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  static const char wrong_type_source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "Context as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  CHECK(process_negative_fixture(value, canonical_negative_source,
                                 PROCESS_NEGATIVE_NONCONST));
  CHECK(process_negative_fixture(value, canonical_negative_source,
                                 PROCESS_NEGATIVE_MODULE));
  CHECK(process_negative_fixture(value, wrong_type_source,
                                 PROCESS_NEGATIVE_TYPE));
  CHECK(process_negative_fixture(value, canonical_negative_source,
                                 PROCESS_NEGATIVE_RECEIVER));
  CHECK(process_negative_fixture(value, canonical_negative_source,
                                 PROCESS_NEGATIVE_RETURN));
  CHECK(process_negative_fixture(value, canonical_negative_source,
                                 PROCESS_NEGATIVE_PARAMETERS));
  CHECK(process_negative_fixture(value, canonical_negative_source,
                                 PROCESS_NEGATIVE_EXPORT));
  CHECK(process_negative_fixture(value, canonical_negative_source,
                                 PROCESS_NEGATIVE_METADATA));

  static const char duplicate_alias_source[] =
      "import { Arguments as ProcessName, Context as ProcessName, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessName, ctx: ProcessName): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  static const char duplicate_value_type_source[] =
      "import { Arguments as ProcessName, success as ProcessName, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessName, ctx: ProcessName): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  CHECK(test_process_abi_duplicate_import_aliases(duplicate_alias_source));
  CHECK(test_process_abi_duplicate_import_aliases(
      duplicate_value_type_source));

  /* Keep the exact process-handler source healthy through the same import,
   * external receiver/member, conditional, and labelled-result flows used by
   * the selected handler witness. */
  static const char process_handler_source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode, } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode {\n"
      "  if args.isEmpty { print(\"missing\"); return .failure(2) }\n"
      "  else { print(\"received\"); return .success }\n"
      "}\n"
      "entry(run)\n";
  CHECK(fixture_parse(value, process_handler_source));
  fixture_configure_process_abi_external(value);
  fixture_configure_print_host(value);
  CHECK(fixture_resolve_external_imports(value));
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_OK);
  CHECK(value->result.written.functions == 1u &&
        value->result.written.entries == 1u &&
        value->result.written.expressions > 0u &&
        value->result.written.arguments > 0u);
  return true;
}

static bool test_local_assignment_projection(void) {
  fixture *value = &fixture_mutation;
  CHECK(fixture_run(value,
                    "entry {\n"
                    "  var seats = 5\n"
                    "  seats = seats + 1\n"
                    "}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        frontend_text_is(value->result.schema_version,
                         W_SEED_FRONTEND_SCHEMA_VERSION) &&
        value->result.written.statements == 2u);
  CHECK(value->statements[0].kind == W_SEED_FRONTEND_STMT_VAR &&
        value->statements[0].effective_type != W_SEED_FRONTEND_NONE &&
        value->statements[1].kind == W_SEED_FRONTEND_STMT_EXPRESSION);
  const uint32_t assignment_index = value->statements[1].expression_index;
  CHECK(assignment_index < value->result.written.expressions);
  const w_seed_frontend_expression *assignment =
      &value->expressions[assignment_index];
  CHECK(assignment->kind == W_SEED_FRONTEND_EXPR_ASSIGNMENT &&
        assignment->supported &&
        frontend_text_is(assignment->operator_text, "=") &&
        assignment->left < value->result.written.expressions &&
        assignment->right < value->result.written.expressions &&
        assignment->inferred_type < value->result.written.types &&
        value->types[assignment->inferred_type].kind ==
            W_SEED_FRONTEND_TYPE_UNIT);
  const w_seed_frontend_expression *target =
      &value->expressions[assignment->left];
  const w_seed_frontend_expression *replacement =
      &value->expressions[assignment->right];
  CHECK(target->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        target->resolved_binding_statement == 0u &&
        target->inferred_type == value->statements[0].effective_type &&
        replacement->kind == W_SEED_FRONTEND_EXPR_BINARY &&
        replacement->inferred_type == value->statements[0].effective_type &&
        value->expressions[replacement->left].resolved_binding_statement ==
            0u);

  CHECK(fixture_run(value,
                    "entry {\n"
                    "  var seats = 5\n"
                    "  if true { seats = seats + 1 }\n"
                    "  else { seats = seats - 1 }\n"
                    "}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.statements == 4u &&
        value->statements[0].kind == W_SEED_FRONTEND_STMT_VAR &&
        value->statements[1].kind == W_SEED_FRONTEND_STMT_IF &&
        value->statements[1].first_child == 2u &&
        value->statements[1].else_child == 3u);
  size_t resolved_seats = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        frontend_text_is(expression->spelling, "seats")) {
      CHECK(expression->resolved_binding_statement == 0u);
      resolved_seats += 1u;
    }
  }
  CHECK(resolved_seats == 4u);

  CHECK(fixture_run(value,
                    "entry {\n"
                    "  if true { let branchValue = 1 }\n"
                    "  let escaped = branchValue\n"
                    "}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL));
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        frontend_text_is(expression->spelling, "branchValue"))
      CHECK(expression->resolved_binding_statement == W_SEED_FRONTEND_NONE);
  }

  CHECK(fixture_run(value,
                    "entry {\n"
                    "  let seats = 5\n"
                    "  seats = seats + 1\n"
                    "}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u)
    CHECK(value->expressions[index].kind !=
          W_SEED_FRONTEND_EXPR_ASSIGNMENT);

  CHECK(fixture_run(value,
                    "entry {\n"
                    "  var seats = 5\n"
                    "  var tables = 2\n"
                    "  if true { tables = tables + 10 seats = seats + 1 }\n"
                    "  else { seats = seats - 1 tables = tables - 10 }\n"
                    "}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.statements == 7u &&
        value->statements[2].kind == W_SEED_FRONTEND_STMT_IF &&
        value->statements[2].first_child == 3u &&
        value->statements[2].else_child == 5u);
  const uint32_t expected_roots[] = {1u, 0u, 0u, 1u};
  size_t multi_assignment_index = 0u;
  for (uint32_t statement_index = 3u; statement_index <= 6u;
       statement_index += 1u) {
    const w_seed_frontend_statement *statement =
        &value->statements[statement_index];
    CHECK(statement->kind == W_SEED_FRONTEND_STMT_EXPRESSION &&
          statement->expression_index < value->result.written.expressions);
    const w_seed_frontend_expression *multi_assignment =
        &value->expressions[statement->expression_index];
    CHECK(multi_assignment->kind == W_SEED_FRONTEND_EXPR_ASSIGNMENT &&
          multi_assignment->left < value->result.written.expressions);
    const w_seed_frontend_expression *multi_target =
        &value->expressions[multi_assignment->left];
    CHECK(multi_target->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
          multi_target->resolved_binding_statement ==
              expected_roots[multi_assignment_index]);
    multi_assignment_index += 1u;
  }
  CHECK(multi_assignment_index ==
        sizeof(expected_roots) / sizeof(expected_roots[0]));
  return true;
}

static bool test_structured_async_projection(void) {
  fixture *value = &fixture_async;
  static const char valid_source[] =
      "fn prepare(value: i64): i64 { return value }\n"
      "entry {\n"
      "  let left = async prepare(value: 20)\n"
      "  let right = async prepare(value: 22)\n"
      "  let first = await left\n"
      "  let second = await right\n"
      "  let total = first + second\n"
      "}\n";
  CHECK(fixture_run(value, valid_source));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.facts == 0u);
  size_t task_types = 0u;
  size_t launches = 0u;
  size_t awaits = 0u;
  for (size_t index = 0u; index < value->result.written.types; index += 1u) {
    const w_seed_frontend_type *type = &value->types[index];
    if (type->kind != W_SEED_FRONTEND_TYPE_TASK) continue;
    CHECK(type->task_result_type < value->result.written.types &&
          type->element_type == type->task_result_type &&
          value->types[type->task_result_type].kind ==
              W_SEED_FRONTEND_TYPE_INTEGER);
    task_types += 1u;
  }
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH) {
      CHECK(expression->supported &&
            expression->task_call_expression <
                value->result.written.expressions &&
            value->expressions[expression->task_call_expression].kind ==
                W_SEED_FRONTEND_EXPR_CALL &&
            expression->task_result_type < value->result.written.types &&
            expression->enum_index == W_SEED_FRONTEND_NONE &&
            expression->enum_case_index == W_SEED_FRONTEND_NONE);
      launches += 1u;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_AWAIT) {
      CHECK(expression->supported &&
            expression->task_binding_statement <
                value->result.written.statements &&
            expression->task_result_type < value->result.written.types &&
            expression->enum_index == W_SEED_FRONTEND_NONE &&
            expression->enum_case_index == W_SEED_FRONTEND_NONE);
      awaits += 1u;
    }
  }
  CHECK(task_types == 1u && launches == 2u && awaits == 2u);
  const size_t receipt_bytes = value->result.receipt_bytes;
  uint8_t receipt[TEST_RECEIPT];
  CHECK(receipt_bytes <= sizeof(receipt));
  (void)memcpy(receipt, value->receipt, receipt_bytes);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
            W_SEED_FRONTEND_OK &&
        value->result.receipt_bytes == receipt_bytes &&
        memcmp(value->receipt, receipt, receipt_bytes) == 0);

  CHECK(fixture_run(
      value,
      "async fn prepare(value: i64): i64 { await execution#yield() "
      "return value }\n"
      "entry { let left = spawn<.main> prepare(value: 20) "
      "let right = spawn<.main> prepare(value: 22) "
      "let first = await left let second = await right "
      "let total = first + second }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.facts == 0u);
  size_t main_spawns = 0u;
  awaits = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_SPAWN_MAIN_LAUNCH) {
      CHECK(expression->supported &&
            expression->task_call_expression <
                value->result.written.expressions &&
            value->expressions[expression->task_call_expression].kind ==
                W_SEED_FRONTEND_EXPR_CALL &&
            expression->task_result_type < value->result.written.types &&
            frontend_text_is(expression->operator_text, "spawn"));
      main_spawns += 1u;
    } else if (expression->kind == W_SEED_FRONTEND_EXPR_AWAIT) {
      CHECK(expression->supported);
      awaits += 1u;
    }
  }
  CHECK(main_spawns == 2u && awaits == 2u);

  CHECK(fixture_run(
      value,
      "async fn prepare(value: i64): i64 { await execution#yield() "
      "return value }\n"
      "entry { let pending = spawn<.domain> prepare(value: 1) "
      "let result = await pending }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value,
                 W_SEED_FRONTEND_FACT_SPAWN_PARALLEL_DOMAIN_LAUNCH));

  static const char parallel_source[] =
      "fn prepare(value: i64): i64 { return value }\n"
      "entry { let pending = spawn<.domain> prepare(value: 1) "
      "let result = await pending }\n";
  CHECK(fixture_run_with_domain(
      value, parallel_source, W_SEED_FRONTEND_DOMAIN_MODE_SERIAL,
      W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value,
                 W_SEED_FRONTEND_FACT_SPAWN_PARALLEL_DOMAIN_LAUNCH));
  CHECK(fixture_run_with_domain(
      value, parallel_source, W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
      W_SEED_FRONTEND_DOMAIN_CAPABILITY_NONE));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value,
                 W_SEED_FRONTEND_FACT_SPAWN_PARALLEL_DOMAIN_LAUNCH));
  CHECK(fixture_run_with_domain(
      value, parallel_source, W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
      W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.facts == 0u);
  size_t parallel_spawns = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind !=
        W_SEED_FRONTEND_EXPR_SPAWN_PARALLEL_DOMAIN_LAUNCH)
      continue;
    CHECK(expression->supported && expression->domain_index == 0u &&
          expression->domain_mode ==
              W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT &&
          expression->domain_capabilities ==
              W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL &&
          expression->task_call_expression <
              value->result.written.expressions);
    parallel_spawns += 1u;
  }
  CHECK(parallel_spawns == 1u &&
        receipt_contains(value,
                         "domain=0|7:2e646f6d61696e|kind=0|mode=1|capabilities=1|maximum=0\n",
                         strlen("domain=0|7:2e646f6d61696e|kind=0|mode=1|capabilities=1|maximum=0\n")));

  CHECK(fixture_parse(value, parallel_source));
  fixture_configure_domain(value, W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
                           W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL);
  value->domains[1] = value->domains[0];
  value->input.domain_count = 2u;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_INVALID);

  CHECK(fixture_run(
      value,
      "async fn pause(value: i64): i64 { return value }\n"
      "entry { let pending = async pause(value: 42) "
      "let result = await pending }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.functions == 2u);
  size_t async_launch = W_SEED_FRONTEND_NONE;
  size_t async_call = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_ASYNC_LAUNCH) {
      async_launch = index;
      async_call = expression->task_call_expression;
      break;
    }
  }
  CHECK(async_launch != W_SEED_FRONTEND_NONE &&
        async_call < value->result.written.expressions &&
        value->expressions[async_call].kind == W_SEED_FRONTEND_EXPR_CALL &&
        value->expressions[async_call].resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION &&
        value->expressions[async_call].resolved_function_index == 0u &&
        value->functions[0].is_async);

  CHECK(fixture_run(
      value,
      "async fn pause(value: i64): i64 { "
      "let staged = value + 1 await execution#yield() "
      "await execution#yield() "
      "return staged * 2 }\n"
      "entry { let pending = async pause(value: 20) "
      "let result = await pending }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.facts == 0u);
  size_t execution_yields = 0u;
  uint32_t execution_yield_type = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_EXECUTION_YIELD) continue;
    CHECK(expression->supported);
    CHECK(expression->left == W_SEED_FRONTEND_NONE);
    CHECK(expression->right == W_SEED_FRONTEND_NONE);
    CHECK(expression->first_argument == W_SEED_FRONTEND_NONE);
    CHECK(expression->argument_count == 0u);
    CHECK(expression->inferred_type < value->result.written.types);
    CHECK(value->types[expression->inferred_type].kind ==
          W_SEED_FRONTEND_TYPE_UNIT);
    if (execution_yield_type == W_SEED_FRONTEND_NONE)
      execution_yield_type = expression->inferred_type;
    else
      CHECK(expression->inferred_type == execution_yield_type);
    CHECK(frontend_text_is(expression->operator_text, "yield"));
    execution_yields += 1u;
  }
  CHECK(execution_yields == 2u &&
        execution_yield_type != W_SEED_FRONTEND_NONE &&
        value->types[execution_yield_type].span.start_byte == 0u &&
        value->types[execution_yield_type].span.end_byte == 0u);

  CHECK(fixture_run(value,
                    "fn pause(): i64 { await execution#yield() return 1 }\n"
                    "entry { let value = pause() }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_EXECUTION_YIELD));

  CHECK(fixture_run(
      value,
      "fn prepare(value: i64): i64 { return value }\n"
      "entry { let task = async prepare(value: 1) "
      "let first = await task "
      "if true { let first = true let nested = !first } "
      "let total = first + 1 }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.facts == 0u);

  CHECK(fixture_run(value,
                    "fn prepare(value: i64): i64 { return value }\n"
                    "entry { let task = async prepare(value: 1) }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_TASK_ESCAPE));
  CHECK(fixture_run(
      value,
      "fn prepare(value: i64): i64 { return value }\n"
      "entry { let task = async prepare(value: 1) let escaped = task "
      "let value = await task }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_TASK_ESCAPE));
  CHECK(fixture_run(
      value,
      "fn prepare(value: i64): i64 { return value }\n"
      "entry { let task = async prepare(value: 1) let first = await task "
      "let second = await task }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_AWAIT));
  CHECK(fixture_run(
      value,
      "fn prepare(value: i64): i64 { return value }\n"
      "entry { let task = async prepare(value: 1) if true { let task = 2 "
      "let local = task } let value = await task }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.facts == 0u);
  CHECK(fixture_run(
      value,
      "fn prepare(value: i32): i32 { return value }\n"
      "entry { let task = async prepare(value: 1) let value = await task }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_ASYNC_LAUNCH));
  CHECK(fixture_run(
      value,
      "fn prepare(value: i64): i64 { return value }\n"
      "entry { let task = async prepare(value: 1) let task = 2 "
      "let value = await task }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_AWAIT));
  CHECK(fixture_run(
      value,
      "fn prepare(value: i64): i64 { return value }\n"
      "entry { var task = async prepare(value: 1) let first = await task }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_TASK_ESCAPE));
  CHECK(fixture_run(
      value,
      "fn prepare(value: i64): i64 { return value }\n"
      "entry { let value = await prepare(value: 1) }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_AWAIT));
  return true;
}

static bool test_while_projection(void) {
  fixture *value = &fixture_mutation;
  CHECK(fixture_run(value,
                    "fn countTo(limit: i64): i64 {\n"
                    "  var count = 0\n"
                    "  while count < limit { count = count + 1 }\n"
                    "  return count\n"
                    "}\n"
                    "entry { let observed = countTo(limit: 3) }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.functions == 2u &&
        value->result.written.statements == 5u);
  const w_seed_frontend_statement *loop = &value->statements[1];
  CHECK(loop->kind == W_SEED_FRONTEND_STMT_WHILE &&
        loop->condition_expression < value->result.written.expressions &&
        loop->first_child == 2u && loop->child_count == 1u &&
        loop->else_child == W_SEED_FRONTEND_NONE);
  CHECK(value->expressions[loop->condition_expression].kind ==
        W_SEED_FRONTEND_EXPR_BINARY);
  const w_seed_frontend_statement *body = &value->statements[2];
  CHECK(body->kind == W_SEED_FRONTEND_STMT_EXPRESSION &&
        body->expression_index < value->result.written.expressions);
  const w_seed_frontend_expression *loop_assignment =
      &value->expressions[body->expression_index];
  CHECK(loop_assignment->kind == W_SEED_FRONTEND_EXPR_ASSIGNMENT &&
        loop_assignment->left < value->result.written.expressions &&
        value->expressions[loop_assignment->left].resolved_binding_statement ==
            0u);
  size_t loop_count_reads = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        frontend_text_is(expression->spelling, "count")) {
      CHECK(expression->resolved_binding_statement == 0u);
      loop_count_reads += 1u;
    }
  }
  CHECK(loop_count_reads == 4u);

  CHECK(fixture_run(value,
                    "fn invalid(): i64 {\n"
                    "  var count = 0\n"
                    "  while count { count = count + 1 }\n"
                    "  return count\n"
                    "}\n"
                    "entry { let observed = invalid() }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(value, "W-SEM-0001"));
  return true;
}

static bool test_break_continue_projection(void) {
  static const char source[] =
      "fn scan(limit: i64): i64 {\n"
      "  var index = 0\n"
      "  var total = 0\n"
      "  while index < limit {\n"
      "    index = index + 1\n"
      "    if index == 2 { continue }\n"
      "    if index == 5 { break }\n"
      "    total = total + index\n"
      "  }\n"
      "  return total\n"
      "}\n"
      "entry { let result = scan(limit: 9) }\n";
  fixture *value = &fixture_mutation;
  CHECK(fixture_run(value, source));
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  const w_seed_frontend_function *scan = &value->functions[0];
  CHECK(scan->statement_count == 10u);
  const w_seed_frontend_statement *loop =
      &value->statements[scan->first_statement + 2u];
  CHECK(loop->kind == W_SEED_FRONTEND_STMT_WHILE &&
        loop->child_count == 4u &&
        loop->condition_expression < value->result.written.expressions);
  const w_seed_frontend_statement *index_update =
      &value->statements[loop->first_child];
  const w_seed_frontend_statement *continue_if =
      &value->statements[index_update->next_sibling];
  CHECK(continue_if->kind == W_SEED_FRONTEND_STMT_IF &&
        continue_if->first_child != W_SEED_FRONTEND_NONE &&
        value->statements[continue_if->first_child].kind ==
            W_SEED_FRONTEND_STMT_CONTINUE);
  const w_seed_frontend_statement *break_if =
      &value->statements[continue_if->next_sibling];
  CHECK(break_if->kind == W_SEED_FRONTEND_STMT_IF &&
        break_if->first_child != W_SEED_FRONTEND_NONE &&
        value->statements[break_if->first_child].kind ==
            W_SEED_FRONTEND_STMT_BREAK);
  CHECK(value->statements[break_if->next_sibling].kind ==
        W_SEED_FRONTEND_STMT_EXPRESSION);

  CHECK(fixture_run(value, "fn invalid(): i64 { break return 0 }\nentry {}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(fixture_run(value,
                    "fn invalid(): i64 { while true { break outer } return 0 }\n"
                    "entry {}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  return true;
}

static bool test_nested_labeled_while_projection(void) {
  static const char source[] =
      "fn walk(limit: i64): i64 {\n"
      "  var outer = 0\n"
      "  var inner = 0\n"
      "  var total = 0\n"
      "  outerLoop: while outer < limit {\n"
      "    outer = outer + 1\n"
      "    inner = 0\n"
      "    while inner < limit {\n"
      "      inner = inner + 1\n"
      "      if inner == 2 { continue }\n"
      "      if inner == 3 { break }\n"
      "      if outer == 4 { continue outerLoop }\n"
      "      if outer == 5 { break outerLoop }\n"
      "      total = total + 1\n"
      "    }\n"
      "  }\n"
      "  return total\n"
      "}\n"
      "entry {}\n";
  fixture *value = &fixture_mutation;
  CHECK(fixture_run(value, source));
  CHECK(value->result.status == W_SEED_FRONTEND_OK);
  uint32_t outer_loop_index = W_SEED_FRONTEND_NONE;
  uint32_t inner_loop_index = W_SEED_FRONTEND_NONE;
  size_t loops = 0u;
  for (size_t index = 0u; index < value->result.written.statements; index += 1u) {
    const w_seed_frontend_statement *statement = &value->statements[index];
    if (statement->kind != W_SEED_FRONTEND_STMT_WHILE) continue;
    loops += 1u;
    if (frontend_text_is(statement->loop_label, "outerLoop"))
      outer_loop_index = (uint32_t)index;
    else
      inner_loop_index = (uint32_t)index;
  }
  CHECK(loops == 2u && outer_loop_index != W_SEED_FRONTEND_NONE &&
        inner_loop_index != W_SEED_FRONTEND_NONE &&
        outer_loop_index != inner_loop_index);
  const w_seed_frontend_statement *outer_loop =
      &value->statements[outer_loop_index];
  const w_seed_frontend_statement *inner_loop =
      &value->statements[inner_loop_index];
  CHECK(outer_loop->kind == W_SEED_FRONTEND_STMT_WHILE &&
        frontend_text_is(outer_loop->loop_label, "outerLoop") &&
        outer_loop->transfer_target_statement == W_SEED_FRONTEND_NONE &&
        inner_loop->kind == W_SEED_FRONTEND_STMT_WHILE &&
        inner_loop->loop_label.length == 0u);
  size_t transfers = 0u;
  size_t labeled_breaks = 0u;
  size_t labeled_continues = 0u;
  size_t inner_breaks = 0u;
  size_t inner_continues = 0u;
  for (size_t index = 0u; index < value->result.written.statements; index += 1u) {
    const w_seed_frontend_statement *statement = &value->statements[index];
    if (statement->kind != W_SEED_FRONTEND_STMT_BREAK &&
        statement->kind != W_SEED_FRONTEND_STMT_CONTINUE)
      continue;
    transfers += 1u;
    if (frontend_text_is(statement->transfer_label, "outerLoop")) {
      CHECK(statement->transfer_target_statement == outer_loop_index);
      if (statement->kind == W_SEED_FRONTEND_STMT_BREAK)
        labeled_breaks += 1u;
      else
        labeled_continues += 1u;
    } else {
      CHECK(statement->transfer_label.length == 0u &&
            statement->transfer_target_statement == inner_loop_index);
      if (statement->kind == W_SEED_FRONTEND_STMT_BREAK)
        inner_breaks += 1u;
      else
        inner_continues += 1u;
    }
  }
  CHECK(transfers == 4u && labeled_breaks == 1u &&
        labeled_continues == 1u && inner_breaks == 1u &&
        inner_continues == 1u);

  CHECK(fixture_run(
      value,
      "fn first() { same: while false {} }\n"
      "fn second() { same: while false {} }\n"
      "entry {}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK);

  CHECK(fixture_run(
      value,
      "fn invalid() {\n"
      "  outerLoop: while true {\n"
      "    outerLoop: while true { break outerLoop }\n"
      "  }\n"
      "}\nentry {}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(fixture_run(value,
                    "fn invalid() { while true { break missing } }\n"
                    "entry {}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(fixture_run(
      value,
      "fn invalid() {\n"
      "  while true { break later }\n"
      "  later: while false {}\n"
      "}\nentry {}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(fixture_run(
      value,
      "fn invalid() {\n"
      "  outerLoop: while true {\n"
      "    repeat { break outerLoop } while false\n"
      "  }\n"
      "}\nentry {}\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  return true;
}

static bool test_repeat_projection(void) {
  fixture *value = &fixture_mutation;
  CHECK(fixture_run(value,
                    "fn countTo(limit: i64): i64 {\n"
                    "  var count = 0\n"
                    "  repeat { count = count + 1 } while count < limit\n"
                    "  return count\n"
                    "}\n"
                    "entry { let observed = countTo(limit: 3) }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.functions == 2u &&
        value->result.written.statements == 5u);
  const w_seed_frontend_statement *loop = &value->statements[1];
  CHECK(loop->kind == W_SEED_FRONTEND_STMT_REPEAT &&
        loop->condition_expression < value->result.written.expressions &&
        loop->first_child == 2u && loop->child_count == 1u &&
        loop->else_child == W_SEED_FRONTEND_NONE);
  CHECK(value->expressions[loop->condition_expression].kind ==
        W_SEED_FRONTEND_EXPR_BINARY);
  const w_seed_frontend_statement *body = &value->statements[2];
  CHECK(body->kind == W_SEED_FRONTEND_STMT_EXPRESSION &&
        body->expression_index < value->result.written.expressions);
  const w_seed_frontend_expression *assignment =
      &value->expressions[body->expression_index];
  CHECK(assignment->kind == W_SEED_FRONTEND_EXPR_ASSIGNMENT &&
        assignment->left < value->result.written.expressions &&
        value->expressions[assignment->left].resolved_binding_statement ==
            0u);
  size_t count_reads = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        frontend_text_is(expression->spelling, "count")) {
      CHECK(expression->resolved_binding_statement == 0u);
      count_reads += 1u;
    }
  }
  CHECK(count_reads == 4u);

  CHECK(fixture_run(value,
                    "fn invalid(): i64 {\n"
                    "  var count = 0\n"
                    "  repeat { count = count + 1 } while count\n"
                    "  return count\n"
                    "}\n"
                    "entry { let observed = invalid() }\n"));
  CHECK(value->result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(value, "W-SEM-0001"));
  return true;
}

/* Build the smallest explicit two-document resolver graph used by the
 * Frontend32 kernel-import slice. The documents remain caller-owned; the
 * frontend receives only their source/CST views and resolver edge records. */
static bool setup_multidocument_kernel_case(
    fixture *caller, fixture *provider, w_seed_frontend_document documents[2],
    const char *caller_source, const char *provider_source,
    w_seed_frontend_resolved_import_kind target_kind, uint32_t target_index) {
  CHECK(caller != NULL && provider != NULL && documents != NULL &&
        caller_source != NULL && provider_source != NULL);
  CHECK(fixture_parse(caller, caller_source));
  CHECK(fixture_parse(provider, provider_source));
  caller->document.logical_source_id =
      (w_seed_frontend_text){"caller-source", sizeof("caller-source") - 1u};
  caller->document.module_id =
      (w_seed_frontend_text){"caller", sizeof("caller") - 1u};
  caller->document.local_module_name = caller->document.module_id;
  provider->document.logical_source_id = (w_seed_frontend_text){
      "provider-source", sizeof("provider-source") - 1u};
  provider->document.module_id =
      (w_seed_frontend_text){"provider", sizeof("provider") - 1u};
  provider->document.local_module_name = provider->document.module_id;
  w_seed_module_origin origins[TEST_IMPORTS];
  w_seed_module_scan_result scan_result;
  CHECK(w_seed_module_scan(
            &caller->source, caller->nodes, caller->parse.node_count,
            &caller->parse, origins, TEST_IMPORTS, &scan_result) ==
        W_SEED_MODULE_SCAN_OK);
  CHECK(scan_result.written <= TEST_IMPORTS);
  for (size_t index = 0u; index < scan_result.written; index += 1u) {
    caller->resolved_imports[index] = (w_seed_frontend_resolved_import){
        .source_document_index = 0u,
        .direct_import_ordinal = origins[index].direct_import_ordinal,
        .import_declaration_span = origins[index].declaration_span,
        .target_kind = target_kind,
        .target_index = target_index};
  }
  documents[0] = caller->document;
  documents[1] = provider->document;
  caller->input.documents = documents;
  caller->input.document_count = 2u;
  caller->input.import_resolution_complete = true;
  caller->input.resolved_imports = caller->resolved_imports;
  caller->input.resolved_import_count = scan_result.written;
  caller->input.external_modules = NULL;
  caller->input.external_module_count = 0u;
  if (target_kind == W_SEED_FRONTEND_RESOLVED_IMPORT_EXTERNAL_MODULE) {
    caller->external_modules[0] = (w_seed_frontend_external_module){
        .module_id =
            (w_seed_frontend_text){"provider-external",
                                   sizeof("provider-external") - 1u},
        .symbols = NULL,
        .symbol_count = 0u};
    caller->input.external_modules = caller->external_modules;
    caller->input.external_module_count = 1u;
  }
  fixture_configure_accelerated_domain(caller, 4u);
  fixture_fill_output(caller, 0u);
  return true;
}

static bool test_multidocument_kernel_import_frontend(void) {
  static fixture caller;
  static fixture provider;
  static w_seed_frontend_document documents[2];
  static const char provider_reordered[] =
      "module provider<kernels: { forecast: forecastKernel, old: oldKernel }>\n"
      "fn forecastKernel(): i64 { return 42 }\n"
      "fn oldKernel(): i64 { return 7 }\n"
      "export fn host(): i64 { return 9 }\n";
  static const char provider_source[] =
      "module provider<kernels: { old: oldKernel, forecast: forecastKernel }>\n"
      "fn forecastKernel(): i64 { return 42 }\n"
      "fn oldKernel(): i64 { return 7 }\n"
      "export fn host(): i64 { return 9 }\n";
  static const char caller_source[] =
      "module caller\n"
      "import { host } from provider\n"
      "import kernel { old as renamed, forecast } from provider\n"
      "entry { let hostResult = host() let pending = "
      "spawn<.inference> renamed() let result = await pending }\n";
  CHECK(setup_multidocument_kernel_case(
      &caller, &provider, documents, caller_source, provider_source,
      W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT, 1u));
  CHECK(w_seed_frontend_run(&caller.input, &caller.output, &caller.result) ==
        W_SEED_FRONTEND_OK);
  CHECK(caller.result.written.modules == 2u &&
        caller.result.written.imports == 2u &&
        caller.result.written.import_items == 3u &&
        caller.result.written.kernel_modules == 1u &&
        caller.result.written.kernel_bindings == 2u &&
        caller.kernel_modules[0].module_index == 1u &&
        caller.kernel_bindings[0].owner_kernel_module == 0u &&
        caller.kernel_bindings[1].owner_kernel_module == 0u &&
        frontend_text_is(caller.kernel_bindings[0].label, "forecast") &&
        frontend_text_is(caller.kernel_bindings[1].label, "old") &&
        caller.kernel_bindings[0].function_index == 1u &&
        caller.kernel_bindings[1].function_index == 2u);
  CHECK(caller.import_items[0].resolved_kernel_module_index ==
            W_SEED_FRONTEND_NONE &&
        caller.import_items[0].resolved_kernel_binding_index ==
            W_SEED_FRONTEND_NONE &&
        caller.import_items[0].resolved_kernel_function_index ==
            W_SEED_FRONTEND_NONE &&
        frontend_text_is(caller.import_items[1].name, "old") &&
        frontend_text_is(caller.import_items[1].local_name, "renamed") &&
        caller.import_items[1].resolved_kernel_module_index == 0u &&
        caller.import_items[1].resolved_kernel_binding_index == 1u &&
        caller.import_items[1].resolved_kernel_function_index == 2u &&
        frontend_text_is(caller.import_items[2].name, "forecast") &&
        caller.import_items[2].resolved_kernel_module_index == 0u &&
        caller.import_items[2].resolved_kernel_binding_index == 0u &&
        caller.import_items[2].resolved_kernel_function_index == 1u);
  CHECK(receipt_contains(&caller, "|kernel=0:1:2\n",
                         sizeof("|kernel=0:1:2\n") - 1u) &&
        receipt_contains(&caller, "|kernel=0:0:1\n",
                         sizeof("|kernel=0:0:1\n") - 1u));
  bool saw_kernel_call = false;
  bool saw_ordinary_call = false;
  for (size_t index = 0u; index < caller.result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &caller.expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_CALL) continue;
    if (expression->resolved_callee_kind ==
        W_SEED_FRONTEND_CALLEE_KERNEL_BINDING) {
      CHECK(expression->supported && expression->module_index == 0u &&
            expression->resolved_kernel_module_index == 0u &&
            expression->resolved_kernel_binding_index == 1u &&
            expression->resolved_function_index == 2u);
      saw_kernel_call = true;
    } else if (expression->resolved_callee_kind ==
                   W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION &&
               expression->resolved_function_index == 3u) {
      saw_ordinary_call = true;
    }
  }
  CHECK(saw_kernel_call && saw_ordinary_call);

  /* Contract field order and local alias/order are provenance. The same
   * canonical `(provider, label)` identities survive both reorderings. */
  static const char reordered_caller_source[] =
      "module caller\n"
      "import kernel { forecast as first, old as second } from provider\n"
      "entry { let pending = spawn<.inference> second() "
      "let result = await pending }\n";
  CHECK(setup_multidocument_kernel_case(
      &caller, &provider, documents, reordered_caller_source,
      provider_reordered, W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT, 1u));
  CHECK(w_seed_frontend_run(&caller.input, &caller.output, &caller.result) ==
        W_SEED_FRONTEND_OK);
  CHECK(caller.result.written.import_items == 2u &&
        frontend_text_is(caller.import_items[0].local_name, "first") &&
        caller.import_items[0].resolved_kernel_module_index == 0u &&
        caller.import_items[0].resolved_kernel_binding_index == 0u &&
        caller.import_items[0].resolved_kernel_function_index == 1u &&
        frontend_text_is(caller.import_items[1].local_name, "second") &&
        caller.import_items[1].resolved_kernel_module_index == 0u &&
        caller.import_items[1].resolved_kernel_binding_index == 1u &&
        caller.import_items[1].resolved_kernel_function_index == 2u);
  bool saw_reordered_kernel_call = false;
  for (size_t index = 0u; index < caller.result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &caller.expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_CALL &&
        expression->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_KERNEL_BINDING) {
      CHECK(expression->supported &&
            expression->resolved_kernel_module_index == 0u &&
            expression->resolved_kernel_binding_index == 1u &&
            expression->resolved_function_index == 2u);
      saw_reordered_kernel_call = true;
    }
  }
  CHECK(saw_reordered_kernel_call);

  /* The canonical relation is entirely index-based. Copy it before the
   * producer-side fixture storage is torn down; no resolver pointer is part
   * of the imported binding record. */
  const uint32_t saved_module =
      caller.import_items[1].resolved_kernel_module_index;
  const uint32_t saved_binding =
      caller.import_items[1].resolved_kernel_binding_index;
  const uint32_t saved_function =
      caller.import_items[1].resolved_kernel_function_index;
  (void)memset(&provider, 0xa5, sizeof(provider));
  CHECK(caller.import_items[1].resolved_kernel_module_index == saved_module &&
        caller.import_items[1].resolved_kernel_binding_index == saved_binding &&
        caller.import_items[1].resolved_kernel_function_index == saved_function);
  return true;
}

static bool test_multidocument_kernel_import_rejections(void) {
  static fixture caller;
  static fixture provider;
  static w_seed_frontend_document documents[2];
  static const char caller_source[] =
      "module caller\n"
      "import kernel { forecast } from provider\n"
      "entry { let pending = spawn<.inference> forecast() "
      "let result = await pending }\n";
  static const char provider_source[] =
      "module provider<kernels: { forecast: forecastKernel }>\n"
      "fn forecastKernel(): i64 { return 42 }\n";
  CHECK(setup_multidocument_kernel_case(
      &caller, &provider, documents, caller_source, provider_source,
      W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT, 1u));
  documents[1].module_id =
      (w_seed_frontend_text){"forged", sizeof("forged") - 1u};
  fixture_fill_output(&caller, 0xa5u);
  CHECK(w_seed_frontend_run(&caller.input, &caller.output, &caller.result) ==
        W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(&caller, W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL));

  /* A self-edge is a forged resolver identity and must fail before output is
   * touched. */
  CHECK(setup_multidocument_kernel_case(
      &caller, &provider, documents, caller_source, provider_source,
      W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT, 1u));
  caller.resolved_imports[0].target_index = 0u;
  fixture_fill_output(&caller, 0xa5u);
  CHECK(w_seed_frontend_run(&caller.input, &caller.output, &caller.result) ==
        W_SEED_FRONTEND_INVALID && fixture_output_is(&caller, 0xa5u, true));

  static const char missing_label_caller[] =
      "module caller\n"
      "import kernel { missing } from provider\n"
      "entry { let pending = spawn<.inference> missing() "
      "let result = await pending }\n";
  CHECK(setup_multidocument_kernel_case(
      &caller, &provider, documents, missing_label_caller, provider_source,
      W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT, 1u));
  CHECK(w_seed_frontend_run(&caller.input, &caller.output, &caller.result) ==
        W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(&caller, W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL));

  static const char collision_caller[] =
      "module caller\n"
      "import kernel { forecast as value } from provider\n"
      "import { host as value } from provider\n"
      "entry { let pending = spawn<.inference> value() "
      "let result = await pending }\n";
  static const char provider_with_host[] =
      "module provider<kernels: { forecast: forecastKernel }>\n"
      "fn forecastKernel(): i64 { return 42 }\n"
      "export fn host(): i64 { return 9 }\n";
  CHECK(setup_multidocument_kernel_case(
      &caller, &provider, documents, collision_caller, provider_with_host,
      W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT, 1u));
  CHECK(w_seed_frontend_run(&caller.input, &caller.output, &caller.result) ==
        W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(&caller, W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL));

  static const char duplicate_caller[] =
      "module caller\n"
      "import kernel { forecast as renamed } from provider\n"
      "import kernel { forecast as renamed } from provider\n"
      "entry { let pending = spawn<.inference> renamed() "
      "let result = await pending }\n";
  CHECK(setup_multidocument_kernel_case(
      &caller, &provider, documents, duplicate_caller, provider_source,
      W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT, 1u));
  CHECK(w_seed_frontend_run(&caller.input, &caller.output, &caller.result) ==
        W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(&caller, W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL));

  static const char generic_provider[] =
      "module provider<kernels: { forecast: forecastKernel }>\n"
      "fn forecastKernel<T>(value: T): T { return value }\n";
  CHECK(setup_multidocument_kernel_case(
      &caller, &provider, documents, caller_source, generic_provider,
      W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT, 1u));
  CHECK(w_seed_frontend_run(&caller.input, &caller.output, &caller.result) ==
        W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(&caller, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE));

  CHECK(setup_multidocument_kernel_case(
      &caller, &provider, documents, caller_source, provider_source,
      W_SEED_FRONTEND_RESOLVED_IMPORT_EXTERNAL_MODULE, 0u));
  CHECK(w_seed_frontend_run(&caller.input, &caller.output, &caller.result) ==
        W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(&caller, W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL));

  static const char qualified_caller[] =
      "module caller\n"
      "import kernel provider as kernels\n"
      "entry { }\n";
  CHECK(setup_multidocument_kernel_case(
      &caller, &provider, documents, qualified_caller, provider_source,
      W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT, 1u));
  CHECK(w_seed_frontend_run(&caller.input, &caller.output, &caller.result) ==
        W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(&caller, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE));
  return true;
}

static bool test_kernel_module_frontend(void) {
  static const char source[] =
      "module accelerated_invocation<kernels: { hello: kernel }>\n"
      "fn kernel(): i64 { return 42 }\n"
      "entry { }\n";
  fixture *value = &fixture_const;
  CHECK(fixture_run(value, source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        value->result.required.kernel_modules == 1u &&
        value->result.required.kernel_bindings == 1u &&
        value->result.written.kernel_modules == 1u &&
        value->result.written.kernel_bindings == 1u);
  const w_seed_frontend_kernel_module *module =
      &value->kernel_modules[0];
  const w_seed_frontend_kernel_binding *kernel =
      &value->kernel_bindings[0];
  CHECK(module->module_index == 0u &&
        module->first_kernel == 0u && module->kernel_count == 1u &&
        module->span.start_byte < module->span.end_byte);
  CHECK(kernel->module_index == 0u &&
        kernel->owner_kernel_module == 0u && kernel->ordinal == 0u &&
        frontend_text_is(kernel->label, "hello") &&
        kernel->function_index == 0u &&
        kernel->span.start_byte >= module->span.start_byte &&
        kernel->span.end_byte <= module->span.end_byte);
  CHECK(receipt_contains(
      value, "kernel-module=0|span=",
      sizeof("kernel-module=0|span=") - 1u));
  CHECK(receipt_contains(
      value, "kernel-binding=0|owner=0|ordinal=0|label=5:68656c6c6f",
      sizeof("kernel-binding=0|owner=0|ordinal=0|label=5:68656c6c6f") -
          1u));

  /* Projection imports are source provenance only in the seed.  Their kind,
   * qualified alias, ordinary-name distinction, and item ranges all remain
   * explicit frontend receipt identity. */
  static const char projection_imports[] =
      "module import_surface\n"
      "import kernel { forecast, old as renamed } from models.forecast\n"
      "import kernel models.forecast as models\n"
      "import kernel;\n"
      "import kernel.foo;\n";
  CHECK(fixture_run(value, projection_imports));
  CHECK(value->result.written.imports == 4u &&
        value->result.written.import_items == 5u &&
        value->imports[0].kind == W_SEED_FRONTEND_IMPORT_KERNEL &&
        frontend_text_is(value->imports[0].path, "models.forecast") &&
        value->imports[0].alias.length == 0u &&
        value->imports[0].first_item == 0u &&
        value->imports[0].item_count == 2u &&
        value->imports[1].kind == W_SEED_FRONTEND_IMPORT_KERNEL &&
        frontend_text_is(value->imports[1].alias, "models") &&
        value->imports[1].first_item == 2u &&
        value->imports[1].item_count == 1u &&
        value->imports[2].kind == W_SEED_FRONTEND_IMPORT_ORDINARY &&
        frontend_text_is(value->imports[2].path, "kernel") &&
        value->imports[3].kind == W_SEED_FRONTEND_IMPORT_ORDINARY &&
        frontend_text_is(value->imports[3].path, "kernel.foo") &&
        frontend_text_is(value->import_items[1].local_name, "renamed") &&
        frontend_text_is(value->import_items[1].name, "old") &&
        receipt_contains(value, "|kind=1|alias=0:",
                         sizeof("|kind=1|alias=0:") - 1u) &&
        receipt_contains(value,
                         "import-item=1|module=0|name=3:6f6c64|local=7:72656e616d6564",
                         sizeof("import-item=1|module=0|name=3:6f6c64|local=7:72656e616d6564") -
                             1u));

  static const char kernel_alias_collision[] =
      "module collision\n"
      "import kernel models.forecast as value\n"
      "import { Other as value } from other\n"
      "fn local(): i64 { return 1 }\n";
  CHECK(fixture_run(value, kernel_alias_collision));
  CHECK(value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        has_fact(value, W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL));

  static const char accelerated_spawn[] =
      "module accelerated_invocation<kernels: { hello: kernel }>\n"
      "fn kernel(value: i64): i64 { return value }\n"
      "entry { let pending = spawn<.inference> hello(value: 42) "
      "let result = await pending }\n";
  CHECK(fixture_parse(value, accelerated_spawn));
  fixture_configure_accelerated_domain(value, 4u);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
            W_SEED_FRONTEND_OK &&
        value->result.written.facts == 0u);
  size_t accelerated_launch_count = 0u;
  size_t accelerated_call_count = 0u;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind ==
        W_SEED_FRONTEND_EXPR_SPAWN_ACCELERATED_DOMAIN_LAUNCH) {
      CHECK(expression->supported && expression->domain_index == 0u &&
            expression->domain_kind == W_SEED_FRONTEND_DOMAIN_ACCELERATED &&
            expression->domain_mode ==
                W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT &&
            expression->domain_capabilities ==
                W_SEED_FRONTEND_DOMAIN_CAPABILITY_DEVICE &&
            expression->domain_maximum == 4u &&
            expression->task_call_expression <
                value->result.written.expressions);
      accelerated_launch_count += 1u;
    }
    if (expression->kind == W_SEED_FRONTEND_EXPR_CALL &&
        expression->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_KERNEL_BINDING) {
      CHECK(expression->supported &&
            expression->resolved_kernel_module_index == 0u &&
            expression->resolved_kernel_binding_index == 0u &&
            expression->resolved_function_index == 0u &&
            expression->argument_count == 1u);
      accelerated_call_count += 1u;
    }
  }
  CHECK(accelerated_launch_count == 1u && accelerated_call_count == 1u);
  CHECK(receipt_contains(
      value,
      "domain=0|10:2e696e666572656e6365|kind=1|mode=1|capabilities=2|maximum=4\n",
      sizeof("domain=0|10:2e696e666572656e6365|kind=1|mode=1|capabilities=2|maximum=4\n") -
          1u));
  CHECK(receipt_contains(
      value, "|kind=4|host=4294967295|external=4294967295:4294967295|kernel=0:0\n",
      sizeof("|kind=4|host=4294967295|external=4294967295:4294967295|kernel=0:0\n") -
          1u));

  static const char *const rejected_launches[] = {
      "module accelerated_invocation<kernels: { hello: kernel }>\n"
      "fn kernel(value: i64): i64 { return value }\n"
      "entry { let value = hello(value: 42) }\n",
      "module accelerated_invocation<kernels: { hello: kernel }>\n"
      "fn kernel(value: i64): i64 { return value }\n"
      "entry { let pending = async hello(value: 42) }\n",
      "module accelerated_invocation<kernels: { hello: kernel }>\n"
      "fn kernel(value: i64): i64 { return value }\n"
      "entry { let pending = spawn<.inference> kernel(value: 42) }\n",
      "module accelerated_invocation<kernels: { hello: kernel }>\n"
      "fn kernel(value: i64): i64 { return value }\n"
      "entry { let pending = spawn<.inference> missing(value: 42) }\n",
  };
  for (size_t index = 0u;
       index < sizeof(rejected_launches) / sizeof(rejected_launches[0]);
       index += 1u) {
    CHECK(fixture_parse(value, rejected_launches[index]));
    fixture_configure_accelerated_domain(value, 4u);
    CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
          W_SEED_FRONTEND_UNSUPPORTED);
  }

  CHECK(fixture_parse(value, accelerated_spawn));
  fixture_configure_accelerated_domain(value, 0u);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_INVALID);
  CHECK(fixture_parse(value, accelerated_spawn));
  fixture_configure_accelerated_domain(value, 4u);
  value->domains[0].capabilities = W_SEED_FRONTEND_DOMAIN_CAPABILITY_NONE;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_INVALID);
  CHECK(fixture_parse(value, accelerated_spawn));
  fixture_configure_accelerated_domain(value, 4u);
  value->domains[0].capabilities |=
      W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_INVALID);
  CHECK(fixture_parse(value, accelerated_spawn));
  fixture_configure_accelerated_domain(value, 4u);
  value->domains[0].mode = W_SEED_FRONTEND_DOMAIN_MODE_SERIAL;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
            W_SEED_FRONTEND_OK &&
        value->result.written.facts == 0u);
  CHECK(fixture_parse(value, accelerated_spawn));
  fixture_configure_domain(value, W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
                           W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL);
  value->domains[0].name = (w_seed_frontend_text){".inference", 10u};
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_UNSUPPORTED);

  static const char multiple_modules[] =
      "module primary<kernels: { first: first }>\n"
      "fn first(): i64 { return 1 }\n"
      "entry { let pending = spawn<.inference> first() "
      "let result = await pending }\n";
  CHECK(fixture_parse(value, multiple_modules));
  fixture_configure_accelerated_domain(value, 2u);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
            W_SEED_FRONTEND_OK &&
        value->result.written.facts == 0u);
  bool selected_second_module = false;
  for (size_t index = 0u; index < value->result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression = &value->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_CALL &&
        expression->resolved_callee_kind ==
            W_SEED_FRONTEND_CALLEE_KERNEL_BINDING) {
      CHECK(expression->resolved_kernel_module_index == 0u &&
            expression->resolved_kernel_binding_index == 0u &&
            expression->resolved_function_index == 0u);
      selected_second_module = true;
    }
  }
  CHECK(selected_second_module);

  static const char nested_accelerator_call[] =
      "module accelerated_invocation<kernels: { hello: kernel }>\n"
      "fn kernel(value: i64): i64 { return value }\n"
      "fn wrapper(value: i64): i64 { return value }\n"
      "entry { let pending = spawn<.inference> wrapper(value: "
      "hello(value: 42)) }\n";
  CHECK(fixture_parse(value, nested_accelerator_call));
  fixture_configure_accelerated_domain(value, 4u);
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_UNSUPPORTED);

  static const char composed[] =
      "module composed<kernels: { first: first, second: second }>\n"
      "fn first(): i64 { return 1 }\n"
      "fn second(): i64 { return 2 }\n"
      "entry { }\n";
  CHECK(fixture_run(value, composed));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.kernel_modules == 1u &&
        value->result.written.kernel_bindings == 2u &&
        value->kernel_modules[0].first_kernel == 0u &&
        value->kernel_modules[0].kernel_count == 2u &&
        value->kernel_bindings[0].ordinal == 0u &&
        frontend_text_is(value->kernel_bindings[0].label, "first") &&
        value->kernel_bindings[0].function_index == 0u &&
        value->kernel_bindings[1].ordinal == 1u &&
        frontend_text_is(value->kernel_bindings[1].label, "second") &&
        value->kernel_bindings[1].function_index == 1u);

  /* Public labels, rather than source field order, define canonical ordinals.
   * Reordering the contract therefore preserves the module/binding interface
   * identity while the source spans remain provenance-specific. */
  static const char composed_reordered[] =
      "module composed<kernels: { second: second, first: first }>\n"
      "fn first(): i64 { return 1 }\n"
      "fn second(): i64 { return 2 }\n"
      "entry { }\n";
  fixture *reordered = &fixture_a;
  CHECK(fixture_run(reordered, composed_reordered));
  CHECK(reordered->result.status == W_SEED_FRONTEND_OK &&
        reordered->result.written.kernel_modules == 1u &&
        reordered->result.written.kernel_bindings == 2u &&
        reordered->kernel_modules[0].module_index ==
            value->kernel_modules[0].module_index &&
        reordered->kernel_modules[0].first_kernel ==
            value->kernel_modules[0].first_kernel &&
        reordered->kernel_modules[0].kernel_count ==
            value->kernel_modules[0].kernel_count &&
        reordered->kernel_bindings[0].ordinal ==
            value->kernel_bindings[0].ordinal &&
        frontend_text_is(reordered->kernel_bindings[0].label, "first") &&
        reordered->kernel_bindings[0].function_index ==
            value->kernel_bindings[0].function_index &&
        reordered->kernel_bindings[1].ordinal ==
            value->kernel_bindings[1].ordinal &&
        frontend_text_is(reordered->kernel_bindings[1].label, "second") &&
        reordered->kernel_bindings[1].function_index ==
            value->kernel_bindings[1].function_index);

  /* The implementation has no small fixed kernel-field ceiling. */
  size_t many_kernel_length = 0u;
  CHECK(append_many_source(long_source, sizeof(long_source),
                           &many_kernel_length,
                           "module many<kernels: { "));
  for (size_t index = 0u; index < 24u; index += 1u)
    CHECK(append_kernel_contract_field(
        long_source, sizeof(long_source), &many_kernel_length, index,
        index == 23u));
  CHECK(append_many_source(long_source, sizeof(long_source),
                           &many_kernel_length, " }>\n"));
  for (size_t index = 0u; index < 24u; index += 1u)
    CHECK(append_many_piece(long_source, sizeof(long_source),
                            &many_kernel_length, index, "fn f",
                            "(): i64 { return 0 }\n"));
  CHECK(fixture_run(reordered, long_source));
  CHECK(reordered->result.status == W_SEED_FRONTEND_OK &&
        reordered->result.written.kernel_modules == 1u &&
        reordered->result.written.kernel_bindings == 24u &&
        reordered->result.written.functions == 24u);

  static const char *const rejected[] = {
      "module missing<kernels: { hello: missing }>\n",
      "module duplicate<kernels: { hello: kernel, hello: kernel }>\n"
      "fn kernel(): i64 { return 42 }\n",
      "module generic<kernels: { hello: kernel }>\n"
      "fn kernel<T>(value: T): T { return value }\n",
  };
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    CHECK(fixture_run(value, rejected[index]));
    CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
          value->result.status == W_SEED_FRONTEND_UNSUPPORTED &&
          value->result.written.kernel_modules == 0u &&
          value->result.written.kernel_bindings == 0u &&
          has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE));
  }

  /* Invalid contract targets are transactional: no partially published
   * module/binding record can survive either a missing target, duplicate
   * public label, or a generic target without a concrete specialization. */
  const uint8_t contract_sentinel = 0xa5u;
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    CHECK(fixture_parse(value, rejected[index]));
    fixture_fill_output(value, contract_sentinel);
    CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
          W_SEED_FRONTEND_UNSUPPORTED);
    CHECK(all_bytes_equal(value->kernel_modules,
                          sizeof(value->kernel_modules), contract_sentinel) &&
          all_bytes_equal(value->kernel_bindings,
                          sizeof(value->kernel_bindings), contract_sentinel));
  }

  const uint8_t sentinel = 0xa5u;
  CHECK(fixture_parse(value, source));
  fixture_fill_output(value, sentinel);
  value->output.kernel_modules = NULL;
  value->output.kernel_module_capacity = 0u;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_CAPACITY);
  CHECK(fixture_output_is(value, sentinel, true));

  CHECK(fixture_parse(value, source));
  fixture_fill_output(value, sentinel);
  value->output.kernel_bindings = NULL;
  value->output.kernel_binding_capacity = 0u;
  CHECK(w_seed_frontend_run(&value->input, &value->output, &value->result) ==
        W_SEED_FRONTEND_CAPACITY);
  CHECK(fixture_output_is(value, sentinel, true));
  return true;
}

static bool read_text_file(const char *path, char *buffer, size_t capacity) {
  if (path == NULL || buffer == NULL || capacity < 2u) return false;
  FILE *file = fopen(path, "rb");
  if (file == NULL) return false;
  const size_t bytes = fread(buffer, 1u, capacity - 1u, file);
  const int extra = fgetc(file);
  const bool ok = !ferror(file) && extra == EOF && bytes != 0u &&
                  memchr(buffer, '\0', bytes) == NULL;
  (void)fclose(file);
  if (!ok) return false;
  buffer[bytes] = '\0';
  return true;
}

static bool gpu_text_is(const w_seed_gpu_module_program *program,
                        size_t offset, size_t bytes, const char *expected) {
  const size_t expected_bytes = strlen(expected);
  return offset <= program->text_bytes &&
         bytes <= program->text_bytes - offset && bytes == expected_bytes &&
         memcmp(program->text + offset, expected, bytes) == 0;
}

static bool gpu_bytes_contain(const uint8_t *bytes, size_t length,
                              const char *needle) {
  if (bytes == NULL || needle == NULL) return false;
  const size_t needle_length = strlen(needle);
  if (needle_length == 0u || needle_length > length) return false;
  for (size_t index = 0u; index <= length - needle_length; index += 1u)
    if (memcmp(bytes + index, needle, needle_length) == 0) return true;
  return false;
}

static bool test_gpu_module_bridge(const char *path) {
  char source[4096];
  CHECK(read_text_file(path, source, sizeof(source)));
  fixture *value = &fixture_const;
  CHECK(fixture_run(value, source));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        value->result.written.kernel_modules == 1u &&
        value->result.written.kernel_bindings == 1u);

  const w_seed_gpu_module_input input = {
      .frontend_input = &value->input,
      .frontend_output = &value->output,
      .frontend_result = &value->result};
  w_seed_gpu_module_counts counts;
  w_seed_gpu_module_result measured;
  (void)memset(&counts, 0xa5, sizeof(counts));
  (void)memset(&measured, 0xa5, sizeof(measured));
  CHECK(w_seed_gpu_module_measure(&input, &counts, &measured) ==
        W_SEED_GPU_MODULE_OK);
  CHECK(counts.modules == 1u && counts.kernels == 1u &&
        counts.text_bytes != 0u && counts.frontend_receipt_bytes != 0u &&
        measured.status == W_SEED_GPU_MODULE_OK &&
        measured.required.modules == 1u && measured.written.modules == 0u);

  (void)memset(&gpu_storage, 0xa5, sizeof(gpu_storage));
  w_seed_gpu_module_output output = {
      .modules = gpu_storage.modules,
      .module_capacity = TEST_KERNEL_MODULES,
      .kernels = gpu_storage.kernels,
      .kernel_capacity = TEST_KERNEL_BINDINGS,
      .text = gpu_storage.text,
      .text_capacity = sizeof(gpu_storage.text),
      .frontend_receipt = gpu_storage.receipt,
      .frontend_receipt_capacity = sizeof(gpu_storage.receipt)};
  w_seed_gpu_module_result result;
  (void)memset(&result, 0xa5, sizeof(result));
  CHECK(w_seed_gpu_module_run(&input, &output, &result) ==
        W_SEED_GPU_MODULE_OK);
  CHECK(result.status == W_SEED_GPU_MODULE_OK &&
        result.written.modules == counts.modules &&
        result.written.kernels == counts.kernels &&
        result.written.text_bytes == counts.text_bytes &&
        result.written.frontend_receipt_bytes == counts.frontend_receipt_bytes);

  w_seed_gpu_module_program program;
  (void)memset(&program, 0xa5, sizeof(program));
  CHECK(w_seed_gpu_module_program_from_output(&output, &result, &program));
  CHECK(w_seed_gpu_module_verify(&program, &result));
  CHECK(program.module_count == 1u && program.kernel_count == 1u &&
        program.modules[0].first_kernel == 0u &&
        program.modules[0].kernel_count == 1u &&
        gpu_text_is(&program, program.modules[0].kernel_contract_name_offset,
                    program.modules[0].kernel_contract_name_bytes, "kernels") &&
        program.kernels[0].owner_module == 0u &&
        program.kernels[0].ordinal == 0u &&
        program.kernels[0].return_bit_width == 32u &&
        program.kernels[0].return_is_signed &&
        program.kernels[0].payload == 42 &&
        gpu_text_is(&program, program.kernels[0].label_offset,
                    program.kernels[0].label_bytes, "hello") &&
        gpu_text_is(&program, program.kernels[0].function_name_offset,
                    program.kernels[0].function_name_bytes, "helloKernel"));

  gpu0_projection_storage projection_storage;
  (void)memset(&projection_storage, 0xa5, sizeof(projection_storage));
  const w_seed_gpu0_projection_output projection_output = {
      .functions = projection_storage.functions,
      .function_capacity = sizeof(projection_storage.functions) /
                           sizeof(projection_storage.functions[0]),
      .operations = projection_storage.operations,
      .operation_capacity = sizeof(projection_storage.operations) /
                           sizeof(projection_storage.operations[0]),
      .text = projection_storage.text,
      .text_capacity = sizeof(projection_storage.text)};
  w_seed_gpu0_program projected;
  (void)memset(&projected, 0xa5, sizeof(projected));
  CHECK(w_seed_gpu0_program_from_gpu_module(
      &program, &result, 0u, 0u, &projection_output, &projected));
  CHECK(projected.function_count == 2u && projected.operation_count == 7u &&
        projected.functions[0].role == W_SEED_GPU0_FUNCTION_HOST_ROOT &&
        projected.functions[0].interface_name == NULL &&
        projected.functions[0].name_length ==
            program.modules[0].kernel_contract_name_bytes &&
        projected.functions[0].name == projection_storage.text &&
        memcmp(projected.functions[0].name, "kernels",
               projected.functions[0].name_length) == 0 &&
        projected.functions[1].role == W_SEED_GPU0_FUNCTION_DEVICE_KERNEL &&
        projected.functions[1].interface_name_length ==
            program.kernels[0].label_bytes &&
        projected.functions[1].interface_name ==
            projection_storage.text +
                program.modules[0].kernel_contract_name_bytes &&
        memcmp(projected.functions[1].interface_name, "hello",
               projected.functions[1].interface_name_length) == 0 &&
        projected.functions[1].name_length ==
            program.kernels[0].function_name_bytes &&
        projected.functions[1].name ==
            projection_storage.text +
                program.modules[0].kernel_contract_name_bytes +
                program.kernels[0].label_bytes &&
        memcmp(projected.functions[1].name, "helloKernel",
               projected.functions[1].name_length) == 0 &&
        projected.operations[5].i32_value == 42 &&
        projected.operations[6].i32_value == 42);

  gpu0_run_storage gpu0_run;
  (void)memset(&gpu0_run, 0xa5, sizeof(gpu0_run));
  const w_seed_gpu0_output gpu0_output = {
      .host_artifact = gpu0_run.host_artifact,
      .host_capacity = sizeof(gpu0_run.host_artifact),
      .device_artifact = gpu0_run.device_artifact,
      .device_capacity = sizeof(gpu0_run.device_artifact),
      .device_result = &gpu0_run.device_result,
      .host_result = &gpu0_run.host_result};
  CHECK(w_seed_gpu0_run(&projected, &gpu0_output, &gpu0_run.result) ==
        W_SEED_GPU0_OK);
  CHECK(gpu0_run.device_result == 42 && gpu0_run.host_result == 42 &&
        gpu0_run.result.device_result_value == 42 &&
        gpu0_run.result.host_result_value == 42 &&
        gpu_bytes_contain(gpu0_run.device_artifact,
                          gpu0_run.result.measurement.device_artifact_bytes,
                          "// kernel=hello") &&
        gpu_bytes_contain(gpu0_run.device_artifact,
                          gpu0_run.result.measurement.device_artifact_bytes,
                          "// implementation=helloKernel"));
  CHECK(w_seed_gpu0_verify(&projected, &gpu0_output, &gpu0_run.result));

  const gpu0_projection_storage projection_before = projection_storage;
  w_seed_gpu0_projection_output alias_projection = projection_output;
  alias_projection.operations =
      (w_seed_gpu0_operation *)(void *)projection_storage.functions;
  CHECK(!w_seed_gpu0_program_from_gpu_module(
      &program, &result, 0u, 0u, &alias_projection, &projected));
  CHECK(memcmp(&projection_storage, &projection_before,
               sizeof(projection_storage)) == 0 &&
        w_seed_gpu0_verify(&projected, &gpu0_output, &gpu0_run.result));
  CHECK(!w_seed_gpu0_program_from_gpu_module(
      &program, &result, 1u, 0u, &projection_output, &projected));

  const uint8_t saved_module_digest = result.semantic_digest[0];
  result.semantic_digest[0] ^= UINT8_C(1);
  gpu0_projection_storage failed_projection_storage;
  (void)memset(&failed_projection_storage, 0xa5,
               sizeof(failed_projection_storage));
  const w_seed_gpu0_projection_output failed_projection = {
      .functions = failed_projection_storage.functions,
      .function_capacity = sizeof(failed_projection_storage.functions) /
                           sizeof(failed_projection_storage.functions[0]),
      .operations = failed_projection_storage.operations,
      .operation_capacity = sizeof(failed_projection_storage.operations) /
                           sizeof(failed_projection_storage.operations[0]),
      .text = failed_projection_storage.text,
      .text_capacity = sizeof(failed_projection_storage.text)};
  w_seed_gpu0_program failed_projected;
  (void)memset(&failed_projected, 0xa5, sizeof(failed_projected));
  CHECK(!w_seed_gpu0_program_from_gpu_module(
      &program, &result, 0u, 0u, &failed_projection, &failed_projected));
  CHECK(all_bytes_equal(&failed_projection_storage,
                        sizeof(failed_projection_storage), 0xa5u) &&
        all_bytes_equal(&failed_projected, sizeof(failed_projected), 0xa5u));
  result.semantic_digest[0] = saved_module_digest;

  gpu0_projection_storage short_projection_storage;
  (void)memset(&short_projection_storage, 0xa5,
               sizeof(short_projection_storage));
  const w_seed_gpu0_projection_output short_projection = {
      .functions = short_projection_storage.functions,
      .function_capacity = sizeof(short_projection_storage.functions) /
                           sizeof(short_projection_storage.functions[0]),
      .operations = short_projection_storage.operations,
      .operation_capacity = sizeof(short_projection_storage.operations) /
                           sizeof(short_projection_storage.operations[0]),
      .text = short_projection_storage.text,
      .text_capacity = 1u};
  w_seed_gpu0_program short_projected;
  (void)memset(&short_projected, 0xa5, sizeof(short_projected));
  CHECK(!w_seed_gpu0_program_from_gpu_module(
      &program, &result, 0u, 0u, &short_projection, &short_projected));
  CHECK(all_bytes_equal(&short_projection_storage,
                        sizeof(short_projection_storage), 0xa5u) &&
        all_bytes_equal(&short_projected, sizeof(short_projected), 0xa5u));

  w_seed_gpu0_projection_output text_alias_projection = projection_output;
  text_alias_projection.text = (char *)(void *)program.text;
  text_alias_projection.text_capacity = program.text_capacity;
  CHECK(!w_seed_gpu0_program_from_gpu_module(
      &program, &result, 0u, 0u, &text_alias_projection, &short_projected));
  CHECK(all_bytes_equal(&short_projected, sizeof(short_projected), 0xa5u));

  static const char non42_source[] =
      "module payloads<kernels: { value: payloadKernel }>\n"
      "fn payloadKernel(): i32 { return 43 }\n"
      "entry { }\n";
  fixture *non42 = &fixture_a;
  CHECK(fixture_run(non42, non42_source));
  const w_seed_gpu_module_input non42_input = {
      .frontend_input = &non42->input,
      .frontend_output = &non42->output,
      .frontend_result = &non42->result};
  (void)memset(&gpu_non42_storage, 0xa5, sizeof(gpu_non42_storage));
  w_seed_gpu_module_output non42_output = {
      .modules = gpu_non42_storage.modules,
      .module_capacity = TEST_KERNEL_MODULES,
      .kernels = gpu_non42_storage.kernels,
      .kernel_capacity = TEST_KERNEL_BINDINGS,
      .text = gpu_non42_storage.text,
      .text_capacity = sizeof(gpu_non42_storage.text),
      .frontend_receipt = gpu_non42_storage.receipt,
      .frontend_receipt_capacity = sizeof(gpu_non42_storage.receipt)};
  w_seed_gpu_module_result non42_result;
  CHECK(w_seed_gpu_module_run(&non42_input, &non42_output, &non42_result) ==
        W_SEED_GPU_MODULE_OK);
  w_seed_gpu_module_program non42_program;
  CHECK(w_seed_gpu_module_program_from_output(&non42_output, &non42_result,
                                              &non42_program));
  gpu0_projection_storage non42_projection_storage;
  (void)memset(&non42_projection_storage, 0xa5,
               sizeof(non42_projection_storage));
  const w_seed_gpu0_projection_output non42_projection = {
      .functions = non42_projection_storage.functions,
      .function_capacity = sizeof(non42_projection_storage.functions) /
                           sizeof(non42_projection_storage.functions[0]),
      .operations = non42_projection_storage.operations,
      .operation_capacity = sizeof(non42_projection_storage.operations) /
                           sizeof(non42_projection_storage.operations[0]),
      .text = non42_projection_storage.text,
      .text_capacity = sizeof(non42_projection_storage.text)};
  w_seed_gpu0_program non42_gpu0;
  CHECK(w_seed_gpu0_program_from_gpu_module(
      &non42_program, &non42_result, 0u, 0u, &non42_projection,
      &non42_gpu0));
  CHECK(non42_gpu0.operations[5].i32_value == 43 &&
        non42_gpu0.operations[6].i32_value == 43);
  gpu0_run_storage non42_run;
  (void)memset(&non42_run, 0xa5, sizeof(non42_run));
  const w_seed_gpu0_output non42_gpu0_output = {
      .host_artifact = non42_run.host_artifact,
      .host_capacity = sizeof(non42_run.host_artifact),
      .device_artifact = non42_run.device_artifact,
      .device_capacity = sizeof(non42_run.device_artifact),
      .device_result = &non42_run.device_result,
      .host_result = &non42_run.host_result};
  CHECK(w_seed_gpu0_run(&non42_gpu0, &non42_gpu0_output, &non42_run.result) ==
        W_SEED_GPU0_OK);
  CHECK(non42_run.device_result == 43 && non42_run.host_result == 43 &&
        non42_run.result.device_result_value == 43 &&
        non42_run.result.host_result_value == 43 &&
        gpu_bytes_contain(non42_run.device_artifact,
                          non42_run.result.measurement.device_artifact_bytes,
                          "memref.store %value") &&
        w_seed_gpu0_verify(&non42_gpu0, &non42_gpu0_output,
                           &non42_run.result));

  static const char trivia_source[] =
      "module gpu0<kernels: { hello: helloKernel }>\n"
      "fn helloKernel(): i32 {\n"
      "  // semantic identity ignores source trivia\n"
      "  return 42\n"
      "}\n\n"
      "entry { }\n";
  fixture *trivia = &fixture_b;
  CHECK(fixture_run(trivia, trivia_source));
  const w_seed_gpu_module_input trivia_input = {
      .frontend_input = &trivia->input,
      .frontend_output = &trivia->output,
      .frontend_result = &trivia->result};
  (void)memset(&gpu_capacity_storage, 0,
               sizeof(gpu_capacity_storage));
  w_seed_gpu_module_output trivia_output = {
      .modules = gpu_capacity_storage.modules,
      .module_capacity = TEST_KERNEL_MODULES,
      .kernels = gpu_capacity_storage.kernels,
      .kernel_capacity = TEST_KERNEL_BINDINGS,
      .text = gpu_capacity_storage.text,
      .text_capacity = sizeof(gpu_capacity_storage.text),
      .frontend_receipt = gpu_capacity_storage.receipt,
      .frontend_receipt_capacity = sizeof(gpu_capacity_storage.receipt)};
  w_seed_gpu_module_result trivia_result;
  CHECK(w_seed_gpu_module_run(&trivia_input, &trivia_output, &trivia_result) ==
        W_SEED_GPU_MODULE_OK);
  CHECK(memcmp(result.semantic_digest, trivia_result.semantic_digest,
               sizeof(result.semantic_digest)) == 0 &&
        memcmp(result.provenance_digest, trivia_result.provenance_digest,
               sizeof(result.provenance_digest)) != 0);

  const uint8_t sentinel = 0xa5u;
  (void)memset(&gpu_capacity_storage, sentinel,
               sizeof(gpu_capacity_storage));
  w_seed_gpu_module_output short_output = {
      .modules = gpu_capacity_storage.modules,
      .module_capacity = counts.modules,
      .kernels = gpu_capacity_storage.kernels,
      .kernel_capacity = counts.kernels,
      .text = gpu_capacity_storage.text,
      .text_capacity = counts.text_bytes - 1u,
      .frontend_receipt = gpu_capacity_storage.receipt,
      .frontend_receipt_capacity = counts.frontend_receipt_bytes};
  w_seed_gpu_module_result short_result;
  (void)memset(&short_result, sentinel, sizeof(short_result));
  CHECK(w_seed_gpu_module_run(&input, &short_output, &short_result) ==
        W_SEED_GPU_MODULE_CAPACITY);
  CHECK(all_bytes_equal(&gpu_capacity_storage,
                        sizeof(gpu_capacity_storage), sentinel) &&
        all_bytes_equal(&short_result, sizeof(short_result), sentinel));

  w_seed_gpu_module_output alias_output = output;
  alias_output.text = (uint8_t *)(void *)gpu_storage.modules;
  alias_output.text_capacity = sizeof(gpu_storage.modules);
  w_seed_gpu_module_result alias_result;
  (void)memset(&alias_result, sentinel, sizeof(alias_result));
  CHECK(w_seed_gpu_module_run(&input, &alias_output, &alias_result) ==
        W_SEED_GPU_MODULE_ALIAS);
  CHECK(all_bytes_equal(&alias_result, sizeof(alias_result), sentinel) &&
        w_seed_gpu_module_verify(&program, &result));
  CHECK(!w_seed_gpu_module_program_from_output(
      &output, &result,
      (w_seed_gpu_module_program *)(void *)gpu_storage.modules));
  CHECK(w_seed_gpu_module_verify(&program, &result));

  w_seed_gpu_module_counts unchanged_counts;
  w_seed_gpu_module_result unchanged_result;
  const w_seed_frontend_text saved_schema = value->result.schema_version;
  value->result.schema_version = (w_seed_frontend_text){"forged", 6u};
  (void)memset(&unchanged_counts, sentinel, sizeof(unchanged_counts));
  (void)memset(&unchanged_result, sentinel, sizeof(unchanged_result));
  CHECK(w_seed_gpu_module_measure(&input, &unchanged_counts,
                                  &unchanged_result) ==
        W_SEED_GPU_MODULE_INVALID_SCHEMA);
  CHECK(all_bytes_equal(&unchanged_counts, sizeof(unchanged_counts), sentinel) &&
        all_bytes_equal(&unchanged_result, sizeof(unchanged_result), sentinel));
  value->result.schema_version = saved_schema;

  value->result.required.functions += 1u;
  CHECK(w_seed_gpu_module_measure(&input, &unchanged_counts,
                                  &unchanged_result) ==
        W_SEED_GPU_MODULE_INCONSISTENT);
  value->result.required.functions -= 1u;

  const uint32_t saved_function = value->kernel_bindings[0].function_index;
  value->kernel_bindings[0].function_index = UINT32_MAX;
  (void)memset(&unchanged_counts, sentinel, sizeof(unchanged_counts));
  (void)memset(&unchanged_result, sentinel, sizeof(unchanged_result));
  CHECK(w_seed_gpu_module_measure(&input, &unchanged_counts,
                                  &unchanged_result) ==
        W_SEED_GPU_MODULE_UNSUPPORTED);
  CHECK(all_bytes_equal(&unchanged_counts, sizeof(unchanged_counts), sentinel) &&
        all_bytes_equal(&unchanged_result, sizeof(unchanged_result), sentinel));
  value->kernel_bindings[0].function_index = saved_function;

  const int64_t saved_payload = program.kernels[0].payload;
  gpu_storage.kernels[0].payload = 43;
  CHECK(!w_seed_gpu_module_verify(&program, &result));
  gpu_storage.kernels[0].payload = saved_payload;
  CHECK(w_seed_gpu_module_verify(&program, &result));

  const w_seed_span saved_field_span = gpu_storage.kernels[0].field_span;
  gpu_storage.kernels[0].field_span.start_byte =
      gpu_storage.modules[0].kernel_contract_span.end_byte + 1u;
  CHECK(!w_seed_gpu_module_verify(&program, &result));
  gpu_storage.kernels[0].field_span = saved_field_span;
  CHECK(w_seed_gpu_module_verify(&program, &result));

  const size_t label_offset = gpu_storage.kernels[0].label_offset;
  const uint8_t saved_label = gpu_storage.text[label_offset];
  gpu_storage.text[label_offset] = (uint8_t)'?';
  CHECK(!w_seed_gpu_module_verify(&program, &result));
  gpu_storage.text[label_offset] = saved_label;
  CHECK(w_seed_gpu_module_verify(&program, &result));

  result.semantic_digest[0] ^= UINT8_C(1);
  CHECK(!w_seed_gpu_module_verify(&program, &result));
  result.semantic_digest[0] ^= UINT8_C(1);
  CHECK(w_seed_gpu_module_verify(&program, &result));

  (void)memset(value, 0, sizeof(*value));
  (void)memset(source, 0, sizeof(source));
  CHECK(w_seed_gpu_module_verify(&program, &result));
  (void)memset(&gpu_storage, 0, sizeof(gpu_storage));
  CHECK(w_seed_gpu0_verify(&projected, &gpu0_output, &gpu0_run.result));
  return true;
}

static bool test_integer_wrapping_frontend_matrix(void) {
  typedef struct {
    const char *receiver;
    const char *literal_suffix;
    bool is_signed;
    uint16_t bit_width;
  } receiver_case;
  static const receiver_case RECEIVERS[] = {
      {"i8", "i8", true, 8u},   {"i16", "i16", true, 16u},
      {"i32", "i32", true, 32u}, {"i64", "i64", true, 64u},
      {"u8", "u8", false, 8u},   {"u16", "u16", false, 16u},
      {"u32", "u32", false, 32u}, {"u64", "u64", false, 64u},
      {"Int", "i64", true, 64u},  {"UInt", "u64", false, 64u},
  };
  typedef struct {
    const char *member;
    w_seed_frontend_builtin_operation operation;
    size_t argument_count;
  } operation_case;
  static const operation_case OPERATIONS[] = {
      {"wrappingAdd", W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_ADD, 2u},
      {"wrappingSubtract",
       W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_SUBTRACT, 2u},
      {"wrappingMultiply",
       W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_MULTIPLY, 2u},
      {"wrappingNegate", W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_NEGATE, 1u},
      {"wrappingPower", W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_POWER, 2u},
      {"wrappingShiftLeft",
       W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_SHIFT_LEFT, 2u},
  };
  fixture *value = &fixture_literal;
  char source[512];
  for (size_t receiver_index = 0u;
       receiver_index < sizeof(RECEIVERS) / sizeof(RECEIVERS[0]);
       receiver_index += 1u) {
    const receiver_case *receiver = &RECEIVERS[receiver_index];
    for (size_t operation_index = 0u;
         operation_index < sizeof(OPERATIONS) / sizeof(OPERATIONS[0]);
         operation_index += 1u) {
      const operation_case *operation = &OPERATIONS[operation_index];
      int written = 0;
      if (operation->argument_count == 1u) {
        written = snprintf(
            source, sizeof(source),
            "entry { let result = %s.%s(2_%s) }\n", receiver->receiver,
            operation->member, receiver->literal_suffix);
      } else if (operation->operation ==
                     W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_POWER ||
                 operation->operation ==
                     W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_SHIFT_LEFT) {
        written = snprintf(
            source, sizeof(source),
            "entry { let result = %s.%s(2_%s, 3_u64) }\n",
            receiver->receiver, operation->member, receiver->literal_suffix);
      } else {
        written = snprintf(
            source, sizeof(source),
            "entry { let result = %s.%s(2_%s, 3_%s) }\n",
            receiver->receiver, operation->member, receiver->literal_suffix,
            receiver->literal_suffix);
      }
      CHECK(written > 0 && (size_t)written < sizeof(source));
      CHECK(fixture_run(value, source));
      CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
            value->result.status == W_SEED_FRONTEND_OK &&
            counts_equal(&value->result.required, &value->result.written));
      size_t matching_calls = 0u;
      for (size_t expression_index = 0u;
           expression_index < value->result.written.expressions;
           expression_index += 1u) {
        const w_seed_frontend_expression *expression =
            &value->expressions[expression_index];
        if (expression->kind != W_SEED_FRONTEND_EXPR_CALL ||
            expression->builtin_operation != operation->operation)
          continue;
        matching_calls += 1u;
        CHECK(expression->supported &&
              expression->argument_count == operation->argument_count &&
              expression->inferred_type < value->result.written.types);
        const w_seed_frontend_type *type =
            &value->types[expression->inferred_type];
        CHECK(type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
              type->is_signed == receiver->is_signed &&
              type->bit_width == receiver->bit_width);
      }
      CHECK(matching_calls == 1u);
    }
  }

  static const char NESTED[] =
      "entry { let result = "
      "i8.wrappingAdd(i8.wrappingNegate(1_i8), 2_i8) }\n";
  CHECK(fixture_run(value, NESTED));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  uint32_t inner_call = W_SEED_FRONTEND_NONE;
  uint32_t outer_call = W_SEED_FRONTEND_NONE;
  for (size_t expression_index = 0u;
       expression_index < value->result.written.expressions;
       expression_index += 1u) {
    const w_seed_frontend_expression *expression =
        &value->expressions[expression_index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_CALL ||
        !expression->supported)
      continue;
    if (expression->builtin_operation ==
        W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_NEGATE)
      inner_call = (uint32_t)expression_index;
    if (expression->builtin_operation ==
        W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_ADD)
      outer_call = (uint32_t)expression_index;
  }
  CHECK(inner_call != W_SEED_FRONTEND_NONE &&
        outer_call != W_SEED_FRONTEND_NONE);
  const w_seed_frontend_expression *inner = &value->expressions[inner_call];
  const w_seed_frontend_expression *outer = &value->expressions[outer_call];
  CHECK(inner->argument_count == 1u && outer->argument_count == 2u &&
        inner->first_argument < outer->first_argument &&
        (size_t)outer->first_argument + outer->argument_count <=
            value->result.written.arguments &&
        value->arguments[outer->first_argument].expression_index ==
            inner_call);

  static const char NESTED_REORDERED_LOCAL[] =
      "fn add(left: i8, right: i8): i8 { "
      "return i8.wrappingAdd(left, right) }\n"
      "entry { let result = "
      "add(right: i8.wrappingNegate(1_i8), left: 2_i8) }\n";
  CHECK(fixture_run(value, NESTED_REORDERED_LOCAL));
  CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
        value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));
  inner_call = W_SEED_FRONTEND_NONE;
  outer_call = W_SEED_FRONTEND_NONE;
  for (size_t expression_index = 0u;
       expression_index < value->result.written.expressions;
       expression_index += 1u) {
    const w_seed_frontend_expression *expression =
        &value->expressions[expression_index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_CALL ||
        !expression->supported)
      continue;
    if (expression->builtin_operation ==
        W_SEED_FRONTEND_BUILTIN_INTEGER_WRAPPING_NEGATE)
      inner_call = (uint32_t)expression_index;
    else if (expression->argument_count == 2u)
      outer_call = (uint32_t)expression_index;
  }
  CHECK(inner_call != W_SEED_FRONTEND_NONE &&
        outer_call != W_SEED_FRONTEND_NONE);
  inner = &value->expressions[inner_call];
  outer = &value->expressions[outer_call];
  CHECK(inner->argument_count == 1u && outer->argument_count == 2u &&
        inner->first_argument < outer->first_argument &&
        (size_t)outer->first_argument + outer->argument_count <=
            value->result.written.arguments &&
        value->arguments[outer->first_argument].expression_index ==
            inner_call &&
        value->arguments[outer->first_argument].label.length == 5u &&
        memcmp(value->arguments[outer->first_argument].label.data, "right",
               5u) == 0 &&
        value->arguments[outer->first_argument + 1u].label.length == 4u &&
        memcmp(value->arguments[outer->first_argument + 1u].label.data,
               "left", 4u) == 0);

  static const char *const REJECTED[] = {
      "entry { let result = usize.wrappingAdd(1_u64, 2_u64) }\n",
      "entry { let result = f64.wrappingAdd(1_u64, 2_u64) }\n",
      "entry { let result = i8.wrappingAdd(1_i8, 2_i16) }\n",
      "entry { let result = i8.wrappingSubtract(1_i8, true) }\n",
      "entry { let result = i8.wrappingMultiply(1_i8) }\n",
      "entry { let result = i8.wrappingNegate(1_i8, 2_i8) }\n",
      "entry { let result = i8.wrappingPower(1_i8, 2_i64) }\n",
      "entry { let result = i8.wrappingShiftLeft(1_i8, true) }\n",
      "entry { let result = i8.wrappingAdd(left: 1_i8, right: 2_i8) }\n",
      "entry { let result = i8?.wrappingAdd(1_i8, 2_i8) }\n",
      "entry { let i8 = 1_i8 let result = i8.wrappingAdd(1_i8, 2_i8) }\n",
  };
  for (size_t index = 0u; index < sizeof(REJECTED) / sizeof(REJECTED[0]);
       index += 1u) {
    CHECK(fixture_run(value, REJECTED[index]));
    CHECK(value->result.status != W_SEED_FRONTEND_OK &&
          has_fact(value, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
    if (index == 2u) CHECK(has_diagnostic(value, "W-TYPE-0122"));
  }
  return true;
}

static bool test_fixed_integer_bit_primitives_frontend_matrix(void) {
  typedef struct {
    const char *spelling;
    bool is_signed;
    uint16_t bit_width;
  } integer_case;
  static const integer_case INTEGERS[] = {
      {"i8", true, 8u},   {"i16", true, 16u}, {"i32", true, 32u},
      {"i64", true, 64u}, {"u8", false, 8u},  {"u16", false, 16u},
      {"u32", false, 32u}, {"u64", false, 64u},
  };
  typedef struct {
    const char *member;
    w_seed_frontend_builtin_operation operation;
    size_t argument_count;
    bool returns_count;
  } bit_operation_case;
  static const bit_operation_case OPERATIONS[] = {
      {"rotatedLeft", W_SEED_FRONTEND_BUILTIN_U64_ROTATED_LEFT, 2u, false},
      {"rotatedRight", W_SEED_FRONTEND_BUILTIN_U64_ROTATED_RIGHT, 2u,
       false},
      {"countOnes", W_SEED_FRONTEND_BUILTIN_U64_COUNT_ONES, 1u, true},
      {"countZeros", W_SEED_FRONTEND_BUILTIN_U64_COUNT_ZEROS, 1u, true},
      {"countLeadingZeros",
       W_SEED_FRONTEND_BUILTIN_U64_COUNT_LEADING_ZEROS, 1u, true},
      {"countTrailingZeros",
       W_SEED_FRONTEND_BUILTIN_U64_COUNT_TRAILING_ZEROS, 1u, true},
      {"reversedBits", W_SEED_FRONTEND_BUILTIN_U64_REVERSED_BITS, 1u, false},
      {"reversedBytes", W_SEED_FRONTEND_BUILTIN_U64_REVERSED_BYTES, 1u,
       false},
  };
  fixture *value = &fixture_literal;
  char source[512];
  for (size_t integer_index = 0u;
       integer_index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       integer_index += 1u) {
    const integer_case *integer = &INTEGERS[integer_index];
    for (size_t operation_index = 0u;
         operation_index < sizeof(OPERATIONS) / sizeof(OPERATIONS[0]);
         operation_index += 1u) {
      const bit_operation_case *operation = &OPERATIONS[operation_index];
      const char *result_type = operation->returns_count ? "UInt"
                                                         : integer->spelling;
      int written = 0;
      if (operation->argument_count == 2u) {
        written = snprintf(
            source, sizeof(source),
            "fn apply(value: %s, count: UInt): %s { return %s.%s(value, count) }\n"
            "entry { }\n",
            integer->spelling, result_type, integer->spelling,
            operation->member);
      } else {
        written = snprintf(
            source, sizeof(source),
            "fn apply(value: %s): %s { return %s.%s(value) }\n"
            "entry { }\n",
            integer->spelling, result_type, integer->spelling,
            operation->member);
      }
      CHECK(written > 0 && (size_t)written < sizeof(source));
      CHECK(fixture_run(value, source));
      CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
            value->result.status == W_SEED_FRONTEND_OK &&
            counts_equal(&value->result.required, &value->result.written));
      size_t matching_calls = 0u;
      for (size_t expression_index = 0u;
           expression_index < value->result.written.expressions;
           expression_index += 1u) {
        const w_seed_frontend_expression *call =
            &value->expressions[expression_index];
        if (call->kind != W_SEED_FRONTEND_EXPR_CALL ||
            call->builtin_operation != operation->operation)
          continue;
        matching_calls += 1u;
        CHECK(call->supported && call->argument_count ==
                                      operation->argument_count &&
              call->left < value->result.written.expressions &&
              call->inferred_type < value->result.written.types);
        const w_seed_frontend_expression *callee =
            &value->expressions[call->left];
        CHECK(callee->kind == W_SEED_FRONTEND_EXPR_MEMBER &&
              callee->supported &&
              callee->builtin_operation == operation->operation &&
              callee->inferred_type < value->result.written.types &&
              callee->left < value->result.written.expressions);
        const w_seed_frontend_expression *receiver =
            &value->expressions[callee->left];
        CHECK(receiver->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
              receiver->builtin_operation ==
                  W_SEED_FRONTEND_BUILTIN_INTEGER_RECEIVER &&
              receiver->supported && receiver->spelling.length ==
                                         strlen(integer->spelling) &&
              memcmp(receiver->spelling.data, integer->spelling,
                     receiver->spelling.length) == 0);
        const w_seed_frontend_type *type =
            &value->types[call->inferred_type];
        const w_seed_frontend_type *member_type =
            &value->types[callee->inferred_type];
        CHECK(type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
              type->is_signed ==
                  (operation->returns_count ? false : integer->is_signed) &&
              type->bit_width ==
                  (operation->returns_count ? 64u : integer->bit_width));
        CHECK(member_type->kind == type->kind &&
              member_type->is_signed == type->is_signed &&
              member_type->bit_width == type->bit_width);
        if (operation->returns_count) {
          CHECK(type->spelling.length == 4u &&
                memcmp(type->spelling.data, "UInt", 4u) == 0);
        }
        for (size_t ordinal = 0u; ordinal < operation->argument_count;
             ordinal += 1u) {
          const size_t argument_index =
              (size_t)call->first_argument + ordinal;
          CHECK(argument_index < value->result.written.arguments &&
                value->arguments[argument_index].label.length == 0u &&
                value->arguments[argument_index].resolved_parameter_ordinal ==
                    ordinal);
        }
      }
      CHECK(matching_calls == 1u);
    }
  }

  static const char U64_COMPATIBILITY[] =
      "fn count(value: i8): u64 { return i8.countOnes(value) }\n"
      "entry { }\n";
  CHECK(fixture_run(value, U64_COMPATIBILITY));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written));

  static const char *const REJECTED[] = {
      "entry { let result = Int.countOnes(1_i64) }\n",
      "entry { let result = UInt.reversedBits(1_u64) }\n",
      "entry { let result = usize.rotatedLeft(1_u64, 1_u64) }\n",
      "entry { let result = i8.countOnes(1_u16) }\n",
      "entry { let result = u16.reversedBits(1_i16) }\n",
      "entry { let result = i32.reversedBits(1_i8) }\n",
      "entry { let result = i32.rotatedLeft(1_i32, 1_u8) }\n",
      "entry { let result = u64.rotatedRight(1_u64, -1_i64) }\n",
      "entry { let result = i16.reversedBits(1_i16, 2_u64) }\n",
      "entry { let result = i16.rotatedLeft(value: 1_i16, count: 1_u64) }\n",
      "entry { let result = i16?.countZeros(1_i16) }\n",
      "entry { let i8 = 1_i8 let result = i8.reversedBytes(1_i8) }\n",
  };
  for (size_t index = 0u; index < sizeof(REJECTED) / sizeof(REJECTED[0]);
       index += 1u) {
    CHECK(fixture_run(value, REJECTED[index]));
    CHECK(value->result.status != W_SEED_FRONTEND_OK);
  }
  return true;
}

static bool test_fixed_integer_shift_policies_frontend_matrix(void) {
  typedef struct {
    const char *spelling;
    bool is_signed;
    uint16_t bit_width;
  } integer_case;
  static const integer_case INTEGERS[] = {
      {"i8", true, 8u},   {"i16", true, 16u}, {"i32", true, 32u},
      {"i64", true, 64u}, {"u8", false, 8u},  {"u16", false, 16u},
      {"u32", false, 32u}, {"u64", false, 64u},
  };
  typedef struct {
    const char *member;
    w_seed_frontend_builtin_operation operation;
  } shift_operation_case;
  static const shift_operation_case OPERATIONS[] = {
      {"maskedShiftLeft", W_SEED_FRONTEND_BUILTIN_U64_MASKED_SHIFT_LEFT},
      {"maskedShiftRight", W_SEED_FRONTEND_BUILTIN_U64_MASKED_SHIFT_RIGHT},
      {"logicalShiftRight", W_SEED_FRONTEND_BUILTIN_U64_LOGICAL_SHIFT_RIGHT},
  };
  fixture *value = &fixture_literal;
  char source[512];
  for (size_t integer_index = 0u;
       integer_index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       integer_index += 1u) {
    const integer_case *integer = &INTEGERS[integer_index];
    for (size_t operation_index = 0u;
         operation_index < sizeof(OPERATIONS) / sizeof(OPERATIONS[0]);
         operation_index += 1u) {
      const shift_operation_case *operation = &OPERATIONS[operation_index];
      const int written = snprintf(
          source, sizeof(source),
          "fn apply(value: %s, count: UInt): %s { "
          "return %s.%s(value, count) }\nentry { }\n",
          integer->spelling, integer->spelling, integer->spelling,
          operation->member);
      CHECK(written > 0 && (size_t)written < sizeof(source));
      CHECK(fixture_run(value, source));
      CHECK(value->parse.status == W_SEED_PARSE_COMPLETE &&
            value->result.status == W_SEED_FRONTEND_OK &&
            counts_equal(&value->result.required, &value->result.written));
      size_t matching_calls = 0u;
      for (size_t expression_index = 0u;
           expression_index < value->result.written.expressions;
           expression_index += 1u) {
        const w_seed_frontend_expression *call =
            &value->expressions[expression_index];
        if (call->kind != W_SEED_FRONTEND_EXPR_CALL ||
            call->builtin_operation != operation->operation)
          continue;
        matching_calls += 1u;
        CHECK(call->supported && call->argument_count == 2u &&
              call->left < value->result.written.expressions &&
              call->inferred_type < value->result.written.types &&
              (size_t)call->first_argument + call->argument_count <=
                  value->result.written.arguments);
        const w_seed_frontend_expression *callee =
            &value->expressions[call->left];
        CHECK(callee->kind == W_SEED_FRONTEND_EXPR_MEMBER &&
              callee->supported &&
              callee->builtin_operation == operation->operation &&
              callee->left < value->result.written.expressions &&
              callee->inferred_type < value->result.written.types);
        const w_seed_frontend_expression *receiver =
            &value->expressions[callee->left];
        CHECK(receiver->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
              receiver->builtin_operation ==
                  W_SEED_FRONTEND_BUILTIN_INTEGER_RECEIVER &&
              receiver->supported && receiver->spelling.length ==
                                         strlen(integer->spelling) &&
              memcmp(receiver->spelling.data, integer->spelling,
                     receiver->spelling.length) == 0);
        const w_seed_frontend_type *result_type =
            &value->types[call->inferred_type];
        const w_seed_frontend_type *member_type =
            &value->types[callee->inferred_type];
        CHECK(result_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
              result_type->is_signed == integer->is_signed &&
              result_type->bit_width == integer->bit_width &&
              member_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
              member_type->is_signed == integer->is_signed &&
              member_type->bit_width == integer->bit_width);
        for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
          const w_seed_frontend_argument *argument =
              &value->arguments[(size_t)call->first_argument + ordinal];
          CHECK(argument->label.length == 0u &&
                argument->resolved_parameter_ordinal == ordinal &&
                argument->expression_index <
                    value->result.written.expressions);
          const w_seed_frontend_expression *operand =
              &value->expressions[argument->expression_index];
          CHECK(operand->inferred_type < value->result.written.types);
          const w_seed_frontend_type *operand_type =
              &value->types[operand->inferred_type];
          CHECK(operand_type->kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                operand_type->is_signed ==
                    (ordinal == 0u ? integer->is_signed : false) &&
                operand_type->bit_width ==
                    (ordinal == 0u ? integer->bit_width : 64u));
        }
      }
      CHECK(matching_calls == 1u);
    }
  }

  static const char *const REJECTED[] = {
      "entry { let result = i8.maskedShiftLeft(1_i8) }\n",
      "entry { let result = i8.maskedShiftRight(1_i8, 1_u64, 2_u64) }\n",
      "entry { let result = i8.logicalShiftRight(value: 1_i8, count: 1_u64) }\n",
      "entry { let result = i8.maskedShiftLeft(1_i8, 1_i32) }\n",
      "entry { let result = i8.maskedShiftRight(1_i8, 1_u32) }\n",
      "entry { let result = i8.logicalShiftRight(1_i16, 1_u64) }\n",
      "entry { let result = u8.maskedShiftRight(1_i8, 1_u64) }\n",
      "entry { let result = Int.maskedShiftLeft(1_i64, 1_u64) }\n",
      "entry { let result = UInt.maskedShiftRight(1_u64, 1_u64) }\n",
      "entry { let result = isize.logicalShiftRight(1_i64, 1_u64) }\n",
      "entry { let result = usize.maskedShiftLeft(1_u64, 1_u64) }\n",
      "entry { let result = i128.maskedShiftRight(1_i64, 1_u64) }\n",
  };
  for (size_t index = 0u; index < sizeof(REJECTED) / sizeof(REJECTED[0]);
       index += 1u) {
    CHECK(fixture_run(value, REJECTED[index]));
    CHECK(value->result.status != W_SEED_FRONTEND_OK);
  }
  return true;
}

static bool fixture_run_with_print_host(fixture *value,
                                        const char *source) {
  CHECK(fixture_parse(value, source));
  value->host_requirements[0] = (w_seed_frontend_host_requirement){
      .name = (w_seed_frontend_text){"Console", 7u}};
  value->host_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  value->host_symbols[0] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"print", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = value->host_parameters,
      .parameter_count = 1u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .requirements = value->host_requirements,
      .requirement_count = 1u};
  value->host_scope = (w_seed_frontend_host_prelude){
      .profile = (w_seed_frontend_text){"native-process@1", 16u},
      .symbols = value->host_symbols,
      .symbol_count = 1u};
  value->input.host_scope = &value->host_scope;
  (void)w_seed_frontend_run(&value->input, &value->output, &value->result);
  return true;
}

static bool test_dry_local_integer_result_interpolation(void) {
  static const char SOURCE[] =
      "fn bitMatrix() { let bit0 = i8.countOnes(0x52_i8) "
      "print(\"${bit0}\") }\nentry(bitMatrix)\n";
  static const char UNRESOLVED[] =
      "fn bitMatrix() { print(\"${missing}\") }\nentry(bitMatrix)\n";
  fixture *value = &fixture_host;
  CHECK(fixture_run_with_print_host(value, SOURCE));
  CHECK(value->result.status == W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written) &&
        value->result.required.facts == 0u &&
        value->result.written.facts == 0u &&
        value->result.written.interpolation_segments != 0u);

  CHECK(fixture_run_with_print_host(value, UNRESOLVED));
  CHECK(value->result.status != W_SEED_FRONTEND_OK &&
        counts_equal(&value->result.required, &value->result.written) &&
        has_fact(value, W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL));
  return true;
}

int main(int argc, char **argv) {
  if (argc == 2) return test_gpu_module_bridge(argv[1]) ? 0 : 1;
  if (argc != 1) return 2;
  if (!test_i128_literal_frontend()) return 1;
  if (!test_short_entry_frontend()) return 1;
  if (!test_scalar_if_frontend_subset()) return 1;
  if (!test_break_continue_projection()) return 1;
  if (!test_nested_labeled_while_projection()) return 1;
  if (!test_scalar_type_measure_emit_parity()) return 1;
  if (!test_checked_shift_frontend_matrix()) return 1;
  if (!test_checked_shift_binding_interpolation_frontend()) return 1;
  if (!test_u64_binary_frontend()) return 1;
  if (!test_flat_aggregate_pair_frontend()) return 1;
  if (!test_flat_value_struct_pair_frontend()) return 1;
  if (!test_u64_overflowing_products_frontend()) return 1;
  if (!test_u64_saturating_policy_frontend()) return 1;
  if (!test_u64_bool_tuple_product_boundary_frontend()) return 1;
  if (!test_f64_scalar_projection()) return 1;
  if (!test_f32_scalar_projection()) return 1;
  if (!test_numeric_widening_frontend()) return 1;
  if (!test_f64_locale_isolation()) return 1;
  if (!test_declarations_and_determinism()) return 1;
  if (!test_enums_and_payloads()) return 1;
  if (!test_typed_throw_projection()) return 1;
  if (!test_synchronous_defer_projection()) return 1;
  if (!test_enum_subsets()) return 1;
  if (!test_enum_values_constructors_and_switches()) return 1;
  if (!test_const_and_membership()) return 1;
  if (!test_module_named_consts()) return 1;
  if (!test_host_scope_and_callee_identity()) return 1;
  if (!test_external_nominal_member_resolution()) return 1;
  if (!test_local_binding_resolution()) return 1;
  if (!test_multidocument_const_ordinals()) return 1;
  if (!test_multidocument_predicate_owner()) return 1;
  if (!test_resolved_import_edges_and_identity()) return 1;
  if (!test_resolved_import_edge_validation()) return 1;
  if (!test_semantic_diagnostics()) return 1;
  if (!test_implicit_integer_widening_frontend()) return 1;
  if (!test_explicit_integer_truncating_bits_frontend()) return 1;
  if (!test_explicit_integer_exactly_frontend()) return 1;
  if (!test_float_bits_frontend()) return 1;
  if (!test_float_integer_rounding_frontend()) return 1;
  if (!test_explicit_integer_saturating_frontend()) return 1;
  if (!test_graph_facts_and_external_stub()) return 1;
  if (!test_receipt_encoding_and_long_fields()) return 1;
  if (!test_generic_schema()) return 1;
  if (!test_generic_applications()) return 1;
  if (!test_typed_const_expressions()) return 1;
  if (!test_string_expression_projection()) return 1;
  if (!test_interpolated_string_projection()) return 1;
  if (!test_barrier_and_capacity()) return 1;
  if (!test_process_abi_alias_and_exit_case()) return 1;
  if (!test_local_assignment_projection()) return 1;
  if (!test_structured_async_projection()) return 1;
  if (!test_while_projection()) return 1;
  if (!test_repeat_projection()) return 1;
  if (!test_multidocument_kernel_import_frontend()) return 1;
  if (!test_multidocument_kernel_import_rejections()) return 1;
  if (!test_kernel_module_frontend()) return 1;
  if (!test_integer_wrapping_frontend_matrix()) return 1;
  if (!test_fixed_integer_bit_primitives_frontend_matrix()) return 1;
  if (!test_fixed_integer_shift_policies_frontend_matrix()) return 1;
  if (!test_dry_local_integer_result_interpolation()) return 1;
  return 0;
}
