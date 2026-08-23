#include "w_seed_frontend.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum {
  PROBE_SOURCE_CAPACITY = 16 * 1024 * 1024,
  PROBE_LEXER_FRAMES = 2048,
  PROBE_TOKENS = 4096,
  PROBE_NODES = 262144,
  PROBE_PARSE_FRAMES = 16384,
  PROBE_ISSUES = 4096,
  PROBE_MODULES = 64,
  PROBE_IMPORTS = 4096,
  PROBE_IMPORT_ITEMS = 4096,
  PROBE_STRUCTS = 4096,
  PROBE_FIELDS = 16384,
  PROBE_DECLARATIONS = 4096,
  PROBE_TYPES = 32768,
  PROBE_FUNCTIONS = 4096,
  PROBE_PARAMETERS = 32768,
  PROBE_ENTRIES = 4096,
  PROBE_STATEMENTS = 65536,
  PROBE_EXPRESSIONS = 262144,
  PROBE_ARGUMENTS = 65536,
  PROBE_SYMBOLS = 131072,
  PROBE_FACTS = 131072,
  PROBE_DIAGNOSTICS = 65536,
  PROBE_RECEIPT = 8 * 1024 * 1024,
};

static uint8_t input_bytes[PROBE_SOURCE_CAPACITY];
static w_seed_lexer_frame lexer_frames[PROBE_LEXER_FRAMES];
static w_seed_parse_token tokens[PROBE_TOKENS];
static w_seed_cst_node nodes[PROBE_NODES];
static w_seed_parse_frame parse_frames[PROBE_PARSE_FRAMES];
static w_seed_parse_issue issues[PROBE_ISSUES];
static w_seed_frontend_module modules[PROBE_MODULES];
static w_seed_frontend_import imports[PROBE_IMPORTS];
static w_seed_frontend_import_item import_items[PROBE_IMPORT_ITEMS];
static w_seed_frontend_struct structs[PROBE_STRUCTS];
static w_seed_frontend_field fields[PROBE_FIELDS];
static w_seed_frontend_type_declaration type_declarations[PROBE_DECLARATIONS];
static w_seed_frontend_alias aliases[PROBE_DECLARATIONS];
static w_seed_frontend_type types[PROBE_TYPES];
static w_seed_frontend_function functions[PROBE_FUNCTIONS];
static w_seed_frontend_parameter parameters[PROBE_PARAMETERS];
static w_seed_frontend_entry entries[PROBE_ENTRIES];
static w_seed_frontend_statement statements[PROBE_STATEMENTS];
static w_seed_frontend_expression expressions[PROBE_EXPRESSIONS];
static w_seed_frontend_argument arguments[PROBE_ARGUMENTS];
static w_seed_frontend_symbol symbols[PROBE_SYMBOLS];
static w_seed_frontend_fact facts[PROBE_FACTS];
static w_seed_frontend_diagnostic diagnostics[PROBE_DIAGNOSTICS];
static uint8_t receipt[PROBE_RECEIPT];

static const char *status_name(w_seed_frontend_status status) {
  switch (status) {
    case W_SEED_FRONTEND_OK:
      return "ok";
    case W_SEED_FRONTEND_DIAGNOSTICS:
      return "diagnostics";
    case W_SEED_FRONTEND_UNSUPPORTED:
      return "unsupported";
    case W_SEED_FRONTEND_CAPACITY:
      return "capacity";
    case W_SEED_FRONTEND_BARRIER:
      return "barrier";
    case W_SEED_FRONTEND_INVALID:
      return "invalid";
  }
  return "unknown";
}

int main(void) {
#ifdef _WIN32
  (void)_setmode(_fileno(stdin), _O_BINARY);
#endif
  size_t length = 0;
  while (length < PROBE_SOURCE_CAPACITY) {
    const size_t room = PROBE_SOURCE_CAPACITY - length;
    const size_t count = fread(input_bytes + length, 1, room, stdin);
    length += count;
    if (count < room) {
      if (ferror(stdin) != 0) return 2;
      break;
    }
  }
  const w_seed_byte_view bytes = {input_bytes, length};
  w_seed_source source;
  w_seed_source_error source_error;
  if (!w_seed_source_init(bytes, &source, &source_error)) return 2;
  w_seed_parser parser;
  w_seed_lex_error lex_error;
  const w_seed_span bounds = {0, length};
  if (!w_seed_parser_init(&source, bounds, (w_seed_foreign_limits){65536u, 256u},
                          lexer_frames, PROBE_LEXER_FRAMES, tokens,
                          PROBE_TOKENS, nodes, PROBE_NODES, parse_frames,
                          PROBE_PARSE_FRAMES, issues, PROBE_ISSUES, &parser,
                          &lex_error)) {
    return 2;
  }
  w_seed_parse_result parse;
  if (!w_seed_parser_parse(&parser, &parse)) return 2;
  const w_seed_frontend_document document = {
      {"probe", 5}, {"probe", 5}, &source, nodes, parse.node_count, parse};
  const w_seed_frontend_input input = {&document, 1, NULL, 0};
  w_seed_frontend_output output = {
      .modules = modules,
      .module_capacity = PROBE_MODULES,
      .imports = imports,
      .import_capacity = PROBE_IMPORTS,
      .import_items = import_items,
      .import_item_capacity = PROBE_IMPORT_ITEMS,
      .structs = structs,
      .struct_capacity = PROBE_STRUCTS,
      .fields = fields,
      .field_capacity = PROBE_FIELDS,
      .type_declarations = type_declarations,
      .type_declaration_capacity = PROBE_DECLARATIONS,
      .aliases = aliases,
      .alias_capacity = PROBE_DECLARATIONS,
      .types = types,
      .type_capacity = PROBE_TYPES,
      .functions = functions,
      .function_capacity = PROBE_FUNCTIONS,
      .parameters = parameters,
      .parameter_capacity = PROBE_PARAMETERS,
      .arguments = arguments,
      .argument_capacity = PROBE_ARGUMENTS,
      .entries = entries,
      .entry_capacity = PROBE_ENTRIES,
      .statements = statements,
      .statement_capacity = PROBE_STATEMENTS,
      .expressions = expressions,
      .expression_capacity = PROBE_EXPRESSIONS,
      .symbols = symbols,
      .symbol_capacity = PROBE_SYMBOLS,
      .facts = facts,
      .fact_capacity = PROBE_FACTS,
      .diagnostics = diagnostics,
      .diagnostic_capacity = PROBE_DIAGNOSTICS,
      .receipt = receipt,
      .receipt_capacity = PROBE_RECEIPT,
  };
  w_seed_frontend_result result;
  const w_seed_frontend_status status =
      w_seed_frontend_run(&input, &output, &result);
  (void)printf("RESULT parse=%d frontend=%s modules=%" PRIuMAX
               " imports=%" PRIuMAX " structs=%" PRIuMAX
               " types=%" PRIuMAX " functions=%" PRIuMAX
               " params=%" PRIuMAX " entries=%" PRIuMAX
               " statements=%" PRIuMAX " expressions=%" PRIuMAX
               " arguments=%" PRIuMAX " symbols=%" PRIuMAX
               " facts=%" PRIuMAX " diagnostics=%" PRIuMAX
               " receipt=%" PRIuMAX "\n",
               (int)parse.status, status_name(status),
               (uintmax_t)result.written.modules,
               (uintmax_t)result.written.imports,
               (uintmax_t)result.written.structs,
               (uintmax_t)result.written.types,
               (uintmax_t)result.written.functions,
               (uintmax_t)result.written.parameters,
               (uintmax_t)result.written.entries,
               (uintmax_t)result.written.statements,
               (uintmax_t)result.written.expressions,
               (uintmax_t)result.written.arguments,
               (uintmax_t)result.written.symbols,
               (uintmax_t)result.written.facts,
               (uintmax_t)result.written.diagnostics,
               (uintmax_t)result.receipt_bytes);
  for (size_t index = 0; index < result.written.diagnostics; index += 1) {
    const w_seed_frontend_diagnostic *diagnostic = &diagnostics[index];
    (void)printf("DIAGNOSTIC code=%.*s start=%" PRIuMAX " end=%" PRIuMAX
                 "\n",
                 (int)diagnostic->code.length, diagnostic->code.data,
                 (uintmax_t)diagnostic->primary.start_byte,
                 (uintmax_t)diagnostic->primary.end_byte);
  }
  if (result.receipt_bytes != 0) {
    (void)fwrite(receipt, 1, result.receipt_bytes, stdout);
  }
  return status == W_SEED_FRONTEND_INVALID ? 2 : 0;
}
