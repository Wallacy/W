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
  TEST_TOKENS = 256,
  TEST_FRAMES = 256,
  TEST_LEX_FRAMES = 128,
  TEST_ISSUES = 64,
  TEST_MODULES = 8,
  TEST_IMPORTS = 32,
  TEST_IMPORT_ITEMS = 32,
  TEST_STRUCTS = 16,
  TEST_FIELDS = 64,
  TEST_DECLARATIONS = 32,
  TEST_TYPES = 128,
  TEST_FUNCTIONS = 32,
  TEST_PARAMETERS = 128,
  TEST_ENTRIES = 16,
  TEST_STATEMENTS = 256,
  TEST_EXPRESSIONS = 1024,
  TEST_ARGUMENTS = 256,
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
         left->type_declarations == right->type_declarations &&
         left->aliases == right->aliases && left->types == right->types &&
         left->functions == right->functions &&
         left->parameters == right->parameters &&
         left->entries == right->entries &&
         left->statements == right->statements &&
         left->expressions == right->expressions &&
         left->arguments == right->arguments && left->symbols == right->symbols &&
         left->facts == right->facts &&
         left->diagnostics == right->diagnostics &&
         left->receipt_bytes == right->receipt_bytes;
}

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
  return true;
}

int main(void) {
  if (!test_declarations_and_determinism()) return 1;
  if (!test_semantic_diagnostics()) return 1;
  if (!test_graph_facts_and_external_stub()) return 1;
  if (!test_receipt_encoding_and_long_fields()) return 1;
  if (!test_barrier_and_capacity()) return 1;
  return 0;
}
