#include "w_seed_frontend.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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
  TEST_ENUMS = 16,
  TEST_ENUM_CASES = 128,
  TEST_ENUM_CASE_PARAMETERS = 256,
  TEST_ENUM_SUBSET_MEMBERS = 256,
  TEST_FIELDS = 64,
  TEST_DECLARATIONS = 32,
  TEST_TYPES = 128,
  TEST_FUNCTIONS = 32,
  TEST_PARAMETERS = 128,
  TEST_ENTRIES = 16,
  TEST_STATEMENTS = 256,
  TEST_EXPRESSIONS = 1024,
  TEST_INTERPOLATION_SEGMENTS = 1024,
  TEST_ARGUMENTS = 256,
  TEST_SWITCH_ARMS = 256,
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
  w_seed_frontend_type types[TEST_TYPES];
  w_seed_frontend_function functions[TEST_FUNCTIONS];
  w_seed_frontend_parameter parameters[TEST_PARAMETERS];
  w_seed_frontend_entry entries[TEST_ENTRIES];
  w_seed_frontend_statement statements[TEST_STATEMENTS];
  w_seed_frontend_expression expressions[TEST_EXPRESSIONS];
  w_seed_frontend_interpolation_segment
      interpolation_segments[TEST_INTERPOLATION_SEGMENTS];
  w_seed_frontend_argument arguments[TEST_ARGUMENTS];
  w_seed_frontend_switch_arm switch_arms[TEST_SWITCH_ARMS];
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
  w_seed_frontend_external_parameter external_parameters[2];
  w_seed_frontend_external_symbol external_symbols[2];
  w_seed_frontend_external_module external_modules[2];
  w_seed_frontend_host_requirement host_requirements[2];
  w_seed_frontend_external_parameter host_parameters[2];
  w_seed_frontend_host_prelude_symbol host_symbols[2];
  w_seed_frontend_host_prelude host_scope;
  uint8_t receipt[TEST_RECEIPT];
  w_seed_frontend_output output;
  w_seed_frontend_result result;
} fixture;

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
  (void)memset(fixture_value->types, value, sizeof(fixture_value->types));
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
         all_bytes_equal(fixture_value->types, sizeof(fixture_value->types),
                         value) &&
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
      .types = fixture_value->types,
      .type_capacity = TEST_TYPES,
      .functions = fixture_value->functions,
      .function_capacity = TEST_FUNCTIONS,
      .parameters = fixture_value->parameters,
      .parameter_capacity = TEST_PARAMETERS,
      .arguments = fixture_value->arguments,
      .argument_capacity = TEST_ARGUMENTS,
      .switch_arms = fixture_value->switch_arms,
      .switch_arm_capacity = TEST_SWITCH_ARMS,
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
  CHECK(fixture_run(values,
                    "enum Stage { ready }\n"
                    "fn value(): Stage { return .ready }\n"));
  CHECK(values->result.status == W_SEED_FRONTEND_OK);

  CHECK(fixture_run(values,
                    "enum Stage { accepted reserving preparing }\n"
                    "enum DomainError { invalidTransition(from: Stage, to: Stage) }\n"
                    "fn f(from: Stage, to: Stage): DomainError { "
                    "return .invalidTransition(to: to, from: from) }\n"));
  CHECK(values->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(values, "W-LABEL-0005"));
  CHECK(!has_diagnostic(values, "W-PATTERN-0002"));
  const w_seed_frontend_diagnostic *reversed_label =
      diagnostic_for_code(values, "W-LABEL-0005");
  static const char *const from_form[] = {"from"};
  CHECK(reversed_label != NULL && reversed_label->fact_count == 3u &&
        reversed_label->label_count == 0u &&
        diagnostic_record_ranges_are_valid(values, reversed_label));
  CHECK(diagnostic_fact_array_is(values, reversed_label, 0u, "acceptedForms",
                                 from_form, 1u));
  CHECK(diagnostic_fact_string_is(values, reversed_label, 1u, "declaration",
                                  "invalidTransition"));
  CHECK(diagnostic_fact_string_is(values, reversed_label, 2u, "label", "to"));
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
  CHECK(receipt_contains(value, "|async=0|throws=0|unsafe=0|borrows=0\n",
                         strlen("|async=0|throws=0|unsafe=0|borrows=0\n")));
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
                         "w-seed-frontend-12") &&
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
        receipt_contains(value, "schema=w-seed-frontend-12\n",
                         strlen("schema=w-seed-frontend-12\n")));

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
  for (size_t index = 0u; index < nested->result.written.expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &nested->expressions[index];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
        frontend_text_is(expression->spelling, "message"))
      CHECK(expression->resolved_binding_statement == W_SEED_FRONTEND_NONE);
  }
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

int main(void) {
  if (!test_declarations_and_determinism()) return 1;
  if (!test_enums_and_payloads()) return 1;
  if (!test_enum_subsets()) return 1;
  if (!test_enum_values_constructors_and_switches()) return 1;
  if (!test_const_and_membership()) return 1;
  if (!test_module_named_consts()) return 1;
  if (!test_host_scope_and_callee_identity()) return 1;
  if (!test_local_binding_resolution()) return 1;
  if (!test_multidocument_const_ordinals()) return 1;
  if (!test_multidocument_predicate_owner()) return 1;
  if (!test_resolved_import_edges_and_identity()) return 1;
  if (!test_resolved_import_edge_validation()) return 1;
  if (!test_semantic_diagnostics()) return 1;
  if (!test_graph_facts_and_external_stub()) return 1;
  if (!test_receipt_encoding_and_long_fields()) return 1;
  if (!test_generic_schema()) return 1;
  if (!test_generic_applications()) return 1;
  if (!test_typed_const_expressions()) return 1;
  if (!test_string_expression_projection()) return 1;
  if (!test_interpolated_string_projection()) return 1;
  if (!test_barrier_and_capacity()) return 1;
  return 0;
}
