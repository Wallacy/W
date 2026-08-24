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
  TEST_ARGUMENTS = 256,
  TEST_SWITCH_ARMS = 256,
  TEST_ENUM_MEMBERSHIP_CASES = 1024,
  TEST_SYMBOLS = 512,
  TEST_FACTS = 512,
  TEST_DIAGNOSTICS = 128,
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
  w_seed_frontend_module modules[TEST_MODULES];
  w_seed_frontend_import imports[TEST_IMPORTS];
  w_seed_frontend_import_item import_items[TEST_IMPORT_ITEMS];
  w_seed_frontend_struct structs[TEST_STRUCTS];
  w_seed_frontend_generic_parameter
      generic_parameters[TEST_GENERIC_PARAMETERS];
  w_seed_frontend_generic_application
      generic_applications[TEST_GENERIC_APPLICATIONS];
  w_seed_frontend_generic_argument generic_arguments[TEST_GENERIC_ARGUMENTS];
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
  w_seed_frontend_type types[TEST_TYPES];
  w_seed_frontend_function functions[TEST_FUNCTIONS];
  w_seed_frontend_parameter parameters[TEST_PARAMETERS];
  w_seed_frontend_entry entries[TEST_ENTRIES];
  w_seed_frontend_statement statements[TEST_STATEMENTS];
  w_seed_frontend_expression expressions[TEST_EXPRESSIONS];
  w_seed_frontend_argument arguments[TEST_ARGUMENTS];
  w_seed_frontend_switch_arm switch_arms[TEST_SWITCH_ARMS];
  w_seed_frontend_enum_membership_case
      enum_membership_cases[TEST_ENUM_MEMBERSHIP_CASES];
  w_seed_frontend_symbol symbols[TEST_SYMBOLS];
  w_seed_frontend_fact facts[TEST_FACTS];
  w_seed_frontend_diagnostic diagnostics[TEST_DIAGNOSTICS];
  w_seed_frontend_external_parameter external_parameters[2];
  w_seed_frontend_external_symbol external_symbols[2];
  w_seed_frontend_external_module external_modules[2];
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
  fixture_value->document.source = &fixture_value->source;
  fixture_value->document.nodes = fixture_value->nodes;
  fixture_value->document.node_count = fixture_value->parse.node_count;
  fixture_value->document.parse = fixture_value->parse;
  fixture_value->input.documents = &fixture_value->document;
  fixture_value->input.document_count = 1;
  fixture_value->input.external_modules = NULL;
  fixture_value->input.external_module_count = 0;
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
      .symbols = fixture_value->symbols,
      .symbol_capacity = TEST_SYMBOLS,
      .facts = fixture_value->facts,
      .fact_capacity = TEST_FACTS,
      .diagnostics = fixture_value->diagnostics,
      .diagnostic_capacity = TEST_DIAGNOSTICS,
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

static bool counts_equal(const w_seed_frontend_counts *left,
                         const w_seed_frontend_counts *right) {
  return left->modules == right->modules && left->imports == right->imports &&
         left->import_items == right->import_items &&
         left->structs == right->structs && left->fields == right->fields &&
         left->generic_parameters == right->generic_parameters &&
         left->generic_applications == right->generic_applications &&
         left->generic_arguments == right->generic_arguments &&
         left->const_values == right->const_values &&
         left->const_elements == right->const_elements &&
         left->const_bytes == right->const_bytes &&
         left->enums == right->enums &&
         left->enum_cases == right->enum_cases &&
         left->enum_case_parameters == right->enum_case_parameters &&
         left->enum_subset_members == right->enum_subset_members &&
         left->type_declarations == right->type_declarations &&
         left->aliases == right->aliases && left->types == right->types &&
         left->functions == right->functions &&
         left->parameters == right->parameters &&
         left->entries == right->entries &&
         left->statements == right->statements &&
         left->expressions == right->expressions &&
         left->arguments == right->arguments && left->symbols == right->symbols &&
         left->switch_arms == right->switch_arms &&
         left->enum_membership_cases == right->enum_membership_cases &&
         left->facts == right->facts &&
         left->diagnostics == right->diagnostics &&
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

static bool test_declarations_and_determinism(void) {
  static const char source[] =
      "module demo\n"
      "import { ext } from dep\n"
      "export struct Pair { left: u8 right: u16 }\n"
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
      "enum Callbacks { positional(fn(named value: u32): Bool) "
      "labeled(handler: fn(named value: u32): Bool) }\n"));
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
  fixture *call_type = &fixture_label;
  CHECK(fixture_run(call_type,
                    "fn callee(value: u32): u32 { return value }\n"
                    "fn f(): u32 { return callee(true) }\nentry(f)\n"));
  CHECK(call_type->result.status == W_SEED_FRONTEND_UNSUPPORTED);
  CHECK(has_fact(call_type, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION));
  CHECK(fixture_run(call_type,
                    "fn inspect(named: Bool): Bool { return named }\n"
                    "fn f(): Bool { return inspect(true) }\nentry(f)\n"));
  CHECK(call_type->result.status == W_SEED_FRONTEND_OK);
  CHECK(fixture_run(call_type,
                    "fn inspect(named value: Bool): Bool { return value }\n"
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
      "fn asBase(stage: WorkStage): Stage { return stage }\n"
      "fn asSuperset(stage: WorkStage): FullStage { return stage }\n"
      "fn call(stage: WorkStage): FullStage { return asSuperset(stage) }\n"
      "fn caseValue(): WorkStage { return .preparing }\n"
      "fn label(stage: WorkStage): String { return switch stage { "
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
      "fn acceptStage(value: Stage): Stage { return value }\n"
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
  CHECK(fixture_run(values,
                    "enum Stage { accepted reserving preparing }\n"
                    "enum DomainError { invalidTransition(from: Stage, to: Stage) }\n"
                    "fn f(from: Stage, to: Stage): DomainError { "
                    "return .invalidTransition(from: from, from: to) }\n"));
  CHECK(values->result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(has_diagnostic(values, "W-LABEL-0006"));
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
  return true;
}

static bool test_const_and_membership(void) {
  static const char source[] =
      "enum Stage { accepted reserving preparing serving }\n"
      "alias WorkStage = Stage<[.preparing, .serving]>\n"
      "const fn isWork(stage: Stage): Bool { return stage in "
      "(Stage.serving, .preparing) }\n"
      "const fn subset(stage: WorkStage): Bool { return stage in "
      "(.accepted, .preparing) }\n"
      "const fn calls(stage: Stage): Bool { return isWork(stage) }\n"
      "fn ordinary(stage: Stage): Bool { return stage in (.accepted) }\n";
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
      "<(isValidStagePath(.member))>> { orderId: u64 }\n";
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
        stage_parameter->label_kind == W_SEED_FRONTEND_LABEL_OPTIONAL &&
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
            W_SEED_FRONTEND_LABEL_NAMED_REQUIRED);
  CHECK(matrix->generic_parameters[2].kind ==
            W_SEED_FRONTEND_GENERIC_KIND_VALUE &&
        matrix->generic_parameters[2].label_kind ==
            W_SEED_FRONTEND_LABEL_NAMED_REQUIRED &&
        matrix->generic_parameters[1].external_label.length == 4u &&
        memcmp(matrix->generic_parameters[1].external_label.data, "rows",
               4u) == 0);

  fixture *labels = &fixture_label;
  CHECK(fixture_run(labels,
                    "struct Labels<required: usize, _ optional: usize> {}\n"));
  CHECK(labels->result.status == W_SEED_FRONTEND_OK &&
        labels->generic_parameters[0].label_kind ==
            W_SEED_FRONTEND_LABEL_NAMED_REQUIRED &&
        labels->generic_parameters[1].label_kind ==
            W_SEED_FRONTEND_LABEL_OPTIONAL &&
        labels->generic_parameters[0].external_label.length == 8u &&
        memcmp(labels->generic_parameters[0].external_label.data, "required",
               8u) == 0 &&
        labels->generic_parameters[1].external_label.length == 0u);

  CHECK(fixture_run(labels,
                    "struct Box<external internal: usize> {}\n"));
  CHECK(labels->result.status == W_SEED_FRONTEND_OK &&
        labels->result.written.generic_parameters == 1u &&
        labels->generic_parameters[0].label_kind ==
            W_SEED_FRONTEND_LABEL_EXTERNAL_REQUIRED &&
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
      "struct Use { value: Outer<Inner<u8>> }\n"));
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
      "struct Use { a: StaticValue<Bool, true> "
      "b: StaticValue<String, \"The final seating\"> }\n"));
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
      "struct Use { a: StagePath<[.accepted, .accepted]> "
      "b: StagePath<[]> c: StagePath<stages: [.accepted]> }\n"));
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
        stage->generic_arguments[2].label.length == 6u &&
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
      "struct Use { value: StagePath<[.accepted]> }\n"));
  CHECK(stage->result.status == W_SEED_FRONTEND_OK &&
        stage->result.written.generic_applications == 1u &&
        stage->generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE &&
        stage->generic_applications[0].requires_const_evaluation);

  static const char *const invalid_sources[] = {
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { x: Matrix<f32, 3, columns: 4> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { x: Matrix<f32, columns: 4, rows: 3> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { x: Matrix<f32, rows: 3, 3> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { x: Matrix<f32, bogus: 3, columns: 4> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { x: Matrix<f32, rows: 3, rows: 4> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { x: Matrix<f32, rows: 3> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { x: Matrix<f32, rows: 3, columns: 4, 5> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { x: Matrix<rows: f32, columns: 3> }\n",
      "struct Matrix<Element, rows: usize, columns: usize> {}\n"
      "struct Use { x: Matrix<f32, rows: 3, columns: "
      "18446744073709551616> }\n",
  };
  static const char *const invalid_codes[] = {
      "W-GENERIC-0003", "W-GENERIC-0003", "W-GENERIC-0003",
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
      &fixture_unresolved,
      "struct StaticValue<T, _ value: T> {}\n"
      "struct Use { bad: StaticValue<f32, value: 0> }\n"));
  CHECK(fixture_unresolved.result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(&fixture_unresolved, "W-CONTRACT-0002") &&
        fixture_unresolved.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_INVALID);

  CHECK(fixture_run(
      &fixture_external,
      "struct Box<T> {}\n"
      "type Bad = Box<true>\n"));
  CHECK(fixture_external.result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(&fixture_external, "W-CONTRACT-0002") &&
        fixture_external.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_INVALID);

  CHECK(fixture_run(
      &fixture_narrowing,
      "struct Plain {}\n"
      "struct Use { value: Plain<true> }\n"));
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
      "struct Use { value: StagePath<[OtherStage.accepted]> }\n"));
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
      "struct Holder<_ value: u64> {}\n"
      "struct Use { a: Holder<value: 1> b: Holder<value: 1_u8> }\n"));
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
      "struct Use { value: Duplicate<u8, u8> }\n"));
  CHECK(fixture_collision.result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(&fixture_collision, "W-CONTRACT-0004") &&
        fixture_collision.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_INVALID);

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
      "struct S<_ value: String> {}\n"
      "struct Use { value: S<value: \"a\\\\n\"> }\n"));
  CHECK(fixture_collision.result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        fixture_collision.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED &&
        has_fact(&fixture_collision, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE));

  CHECK(fixture_run(
      &fixture_collision,
      "struct S<_ value: String> {}\n"
      "struct Use { value: S<bad: \"a\\\\n\"> }\n"));
  CHECK(fixture_collision.result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        has_diagnostic(&fixture_collision, "W-CONTRACT-0001") &&
        fixture_collision.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_INVALID);

  CHECK(fixture_run(
      &fixture_collision,
      "struct S<T> {}\n"
      "struct Use { value: S<u8> }\n"));
  CHECK(fixture_collision.result.status == W_SEED_FRONTEND_OK &&
        fixture_collision.result.written.generic_applications == 1u &&
        fixture_collision.types[fixture_collision.generic_applications[0].owner_type]
                .generic_application_index == 0u);
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
      "struct Text<_ value: String> {}\n"
      "struct Path<_ stages: StaticList<Stage>> {}\n"
      "struct Use { text: Text<value: \"x\"> path: Path<stages: [.accepted]> }\n";
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
  if (!test_semantic_diagnostics()) return 1;
  if (!test_graph_facts_and_external_stub()) return 1;
  if (!test_receipt_encoding_and_long_fields()) return 1;
  if (!test_generic_schema()) return 1;
  if (!test_generic_applications()) return 1;
  if (!test_barrier_and_capacity()) return 1;
  return 0;
}
