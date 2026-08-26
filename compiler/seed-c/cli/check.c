#include "check.h"

#include "w_seed_diagnostic.h"
#include "w_seed_frontend.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* This bounded one-shot core is shared by the internal evidence driver and
 * the public `w check` command. It does not resolve packages or workspaces. */
enum {
  CHECK_SOURCE_CAPACITY = 16 * 1024 * 1024,
  CHECK_LEXER_FRAMES = 2048,
  CHECK_TOKENS = 32768,
  CHECK_NODES = 262144,
  CHECK_PARSE_FRAMES = 16384,
  CHECK_ISSUES = 4096,
  CHECK_MODULES = 64,
  CHECK_IMPORTS = 4096,
  CHECK_IMPORT_ITEMS = 4096,
  CHECK_STRUCTS = 4096,
  CHECK_GENERIC_PARAMETERS = 65536,
  CHECK_GENERIC_APPLICATIONS = 65536,
  CHECK_GENERIC_ARGUMENTS = 262144,
  CHECK_TYPED_CONST_EXPRESSIONS = 262144,
  CHECK_CONST_VALUES = 262144,
  CHECK_CONST_ELEMENTS = 262144,
  CHECK_CONST_BYTES = 8 * 1024 * 1024,
  CHECK_ENUMS = 4096,
  CHECK_ENUM_CASES = 16384,
  CHECK_ENUM_CASE_PARAMETERS = 32768,
  CHECK_ENUM_SUBSET_MEMBERS = 65536,
  CHECK_FIELDS = 16384,
  CHECK_DECLARATIONS = 4096,
  CHECK_CONST_DECLARATIONS = 4096,
  CHECK_TYPES = 32768,
  CHECK_FUNCTIONS = 4096,
  CHECK_PARAMETERS = 32768,
  CHECK_ENTRIES = 4096,
  CHECK_STATEMENTS = 65536,
  CHECK_EXPRESSIONS = 262144,
  CHECK_ARGUMENTS = 65536,
  CHECK_SWITCH_ARMS = 65536,
  CHECK_ENUM_MEMBERSHIP_CASES = 262144,
  CHECK_SYMBOLS = 131072,
  CHECK_FACTS = 131072,
  CHECK_DIAGNOSTICS = 65536,
  CHECK_RECEIPT = 8 * 1024 * 1024,
  CHECK_D0_OUTPUT_CAPACITY = 64 * 1024 * 1024,
};

static uint8_t source_bytes[CHECK_SOURCE_CAPACITY];
static w_seed_lexer_frame lexer_frames[CHECK_LEXER_FRAMES];
static w_seed_parse_token tokens[CHECK_TOKENS];
static w_seed_cst_node nodes[CHECK_NODES];
static w_seed_parse_frame parse_frames[CHECK_PARSE_FRAMES];
static w_seed_parse_issue issues[CHECK_ISSUES];
static w_seed_frontend_module modules[CHECK_MODULES];
static w_seed_frontend_import imports[CHECK_IMPORTS];
static w_seed_frontend_import_item import_items[CHECK_IMPORT_ITEMS];
static w_seed_frontend_struct structs[CHECK_STRUCTS];
static w_seed_frontend_generic_parameter
    generic_parameters[CHECK_GENERIC_PARAMETERS];
static w_seed_frontend_generic_application
    generic_applications[CHECK_GENERIC_APPLICATIONS];
static w_seed_frontend_generic_argument
    generic_arguments[CHECK_GENERIC_ARGUMENTS];
static w_seed_frontend_typed_const_expression
    typed_const_expressions[CHECK_TYPED_CONST_EXPRESSIONS];
static w_seed_frontend_const_value const_values[CHECK_CONST_VALUES];
static w_seed_frontend_const_element const_elements[CHECK_CONST_ELEMENTS];
static uint8_t const_bytes[CHECK_CONST_BYTES];
static w_seed_frontend_enum enums[CHECK_ENUMS];
static w_seed_frontend_enum_case enum_cases[CHECK_ENUM_CASES];
static w_seed_frontend_enum_case_parameter
    enum_case_parameters[CHECK_ENUM_CASE_PARAMETERS];
static w_seed_frontend_enum_subset_member
    enum_subset_members[CHECK_ENUM_SUBSET_MEMBERS];
static w_seed_frontend_field fields[CHECK_FIELDS];
static w_seed_frontend_type_declaration type_declarations[CHECK_DECLARATIONS];
static w_seed_frontend_alias aliases[CHECK_DECLARATIONS];
static w_seed_frontend_const_declaration
    const_declarations[CHECK_CONST_DECLARATIONS];
static w_seed_frontend_type types[CHECK_TYPES];
static w_seed_frontend_function functions[CHECK_FUNCTIONS];
static w_seed_frontend_parameter parameters[CHECK_PARAMETERS];
static w_seed_frontend_entry entries[CHECK_ENTRIES];
static w_seed_frontend_statement statements[CHECK_STATEMENTS];
static w_seed_frontend_expression expressions[CHECK_EXPRESSIONS];
static w_seed_frontend_argument arguments[CHECK_ARGUMENTS];
static w_seed_frontend_switch_arm switch_arms[CHECK_SWITCH_ARMS];
static w_seed_frontend_enum_membership_case
    enum_membership_cases[CHECK_ENUM_MEMBERSHIP_CASES];
static w_seed_frontend_symbol symbols[CHECK_SYMBOLS];
static w_seed_frontend_fact facts[CHECK_FACTS];
static w_seed_frontend_diagnostic diagnostics[CHECK_DIAGNOSTICS];
static w_seed_module_origin module_origins[CHECK_IMPORTS];
static uint8_t receipt[CHECK_RECEIPT];
static uint8_t d0_output[CHECK_D0_OUTPUT_CAPACITY];

static bool frontend_counts_equal(const w_seed_frontend_counts *left,
                                 const w_seed_frontend_counts *right) {
  if (left == NULL || right == NULL) return false;
#define SAME_COUNT(field) (left->field == right->field)
  return SAME_COUNT(modules) && SAME_COUNT(imports) &&
         SAME_COUNT(import_items) && SAME_COUNT(structs) &&
         SAME_COUNT(fields) && SAME_COUNT(type_declarations) &&
         SAME_COUNT(aliases) && SAME_COUNT(types) && SAME_COUNT(functions) &&
         SAME_COUNT(parameters) && SAME_COUNT(entries) &&
         SAME_COUNT(statements) && SAME_COUNT(expressions) &&
         SAME_COUNT(arguments) && SAME_COUNT(symbols) && SAME_COUNT(facts) &&
         SAME_COUNT(diagnostics) && SAME_COUNT(receipt_bytes) &&
         SAME_COUNT(enums) && SAME_COUNT(enum_cases) &&
         SAME_COUNT(enum_case_parameters) && SAME_COUNT(switch_arms) &&
         SAME_COUNT(enum_subset_members) &&
         SAME_COUNT(enum_membership_cases) &&
         SAME_COUNT(generic_parameters) &&
         SAME_COUNT(generic_applications) && SAME_COUNT(generic_arguments) &&
         SAME_COUNT(typed_const_expressions) && SAME_COUNT(const_values) &&
         SAME_COUNT(const_elements) && SAME_COUNT(const_bytes) &&
         SAME_COUNT(const_declarations);
#undef SAME_COUNT
}

static bool frontend_result_complete(const w_seed_frontend_result *result) {
  if (result == NULL || !frontend_counts_equal(&result->required,
                                               &result->written) ||
      result->receipt_bytes != result->written.receipt_bytes) {
    return false;
  }
  if (result->status == W_SEED_FRONTEND_OK) {
    return result->written.facts == 0u && result->written.diagnostics == 0u;
  }
  if (result->status == W_SEED_FRONTEND_DIAGNOSTICS) {
    return result->written.facts == 0u && result->written.diagnostics != 0u &&
           result->primary_diagnostic != W_SEED_FRONTEND_NONE_SIZE;
  }
  return false;
}

static int read_source_file(const char *path, size_t *length) {
  if (path == NULL || length == NULL || path[0] == '\0') return 2;
  FILE *file = fopen(path, "rb");
  if (file == NULL) return 2;
  *length = 0u;
  while (*length < CHECK_SOURCE_CAPACITY) {
    const size_t room = CHECK_SOURCE_CAPACITY - *length;
    const size_t count = fread(source_bytes + *length, 1u, room, file);
    *length += count;
    if (count < room) {
      if (ferror(file) != 0) {
        (void)fclose(file);
        return 2;
      }
      break;
    }
  }
  if (*length == CHECK_SOURCE_CAPACITY) {
    const int extra = fgetc(file);
    if (extra != EOF || ferror(file) != 0) {
      (void)fclose(file);
      return 2;
    }
  }
  if (fclose(file) != 0) return 2;
  return 0;
}

static void report_text(FILE *stream, const char *text, size_t length,
                        size_t limit) {
  if (stream == NULL || text == NULL) return;
  const size_t count = length < limit ? length : limit;
  for (size_t index = 0; index < count; index += 1u) {
    const unsigned char byte = (unsigned char)text[index];
    if (byte == '\n' || byte == '\r' || byte == '\t') {
      (void)fputc(' ', stream);
    } else if (byte < 0x20u) {
      (void)fputc('?', stream);
    } else {
      (void)fputc((int)byte, stream);
    }
  }
  if (count != length) (void)fputs("...", stream);
}

static int report_human_diagnostic(const char *path,
                                   const w_seed_source *source,
                                   const w_seed_frontend_diagnostic *diagnostic) {
  if (path == NULL || source == NULL || diagnostic == NULL) return 3;
  w_seed_source_point point;
  w_seed_source_error source_error;
  if (!w_seed_source_offset_to_point(source, diagnostic->primary.start_byte,
                                     &point, &source_error)) {
    return 3;
  }
  (void)fprintf(stderr, "%s:%" PRIuMAX ":%" PRIuMAX ":%.*s: actual=",
                path, (uintmax_t)(point.line + 1u),
                (uintmax_t)(point.byte_column + 1u),
                (int)diagnostic->code.length, diagnostic->code.data);
  report_text(stderr, diagnostic->actual.data, diagnostic->actual.length, 160u);
  (void)fputs(" expected=", stderr);
  report_text(stderr, diagnostic->expected.data, diagnostic->expected.length,
              160u);
  (void)fputc('\n', stderr);
  return 0;
}

static void report_failure(const char *path, const char *reason) {
  if (path != NULL && reason != NULL) {
    (void)fprintf(stderr, "%s: %s\n", path, reason);
  } else if (reason != NULL) {
    (void)fprintf(stderr, "w_seed_check: %s\n", reason);
  }
}

static bool local_module_name_from_path(const char *path, size_t path_length,
                                        w_seed_frontend_text *name) {
  if (path == NULL || name == NULL || path_length == 0u) return false;
  size_t end = path_length;
  if (end >= 2u && path[end - 2u] == '.' && path[end - 1u] == 'w') {
    end -= 2u;
  }
  size_t start = end;
  while (start > 0u && path[start - 1u] != '/' &&
         path[start - 1u] != '\\') {
    start -= 1u;
  }
  if (start == end) return false;
  *name = (w_seed_frontend_text){path + start, end - start};
  return true;
}

static int emit_json_diagnostics(const char *path, const w_seed_source *source,
                                 size_t count) {
  size_t total = 0u;
  for (size_t index = 0u; index < count; index += 1u) {
    char instance[8];
    const int written = snprintf(instance, sizeof(instance), "D%06" PRIuMAX,
                                 (uintmax_t)(index + 1u));
    if (written != 7) return 2;
    w_seed_diagnostic_result measured;
    const w_seed_diagnostic_status status =
        w_seed_diagnostic_frontend_record(
            instance, sizeof(instance) - 1u, path, strlen(path), source,
            0u, &diagnostics[index], NULL, 0u, &measured);
    if (status != W_SEED_DIAGNOSTIC_CAPACITY ||
        measured.required_bytes > CHECK_D0_OUTPUT_CAPACITY - total ||
        CHECK_D0_OUTPUT_CAPACITY - total - measured.required_bytes < 1u) {
      return 2;
    }
    total += measured.required_bytes + 1u;
  }
  size_t offset = 0u;
  for (size_t index = 0u; index < count; index += 1u) {
    char instance[8];
    const int written = snprintf(instance, sizeof(instance), "D%06" PRIuMAX,
                                 (uintmax_t)(index + 1u));
    if (written != 7) return 3;
    w_seed_diagnostic_result emitted;
    const w_seed_diagnostic_status status =
        w_seed_diagnostic_frontend_record(
            instance, sizeof(instance) - 1u, path, strlen(path), source,
            0u, &diagnostics[index], d0_output + offset,
            CHECK_D0_OUTPUT_CAPACITY - offset, &emitted);
    if (status != W_SEED_DIAGNOSTIC_OK || emitted.written_bytes == 0u ||
        emitted.written_bytes + 1u > CHECK_D0_OUTPUT_CAPACITY - offset) {
      return 3;
    }
    offset += emitted.written_bytes;
    d0_output[offset] = (uint8_t)'\n';
    offset += 1u;
  }
  if (fwrite(d0_output, 1u, offset, stdout) != offset) return 3;
  return 1;
}

int w_seed_check_run(const char *path, bool json) {
  if (path == NULL || path[0] == '\0') {
    report_failure(path, "path is empty");
    return 2;
  }
  size_t length = 0u;
  if (read_source_file(path, &length) != 0) {
    report_failure(path, "cannot read source or source exceeds 16 MiB");
    return 2;
  }

  w_seed_source source;
  w_seed_source_error source_error;
  if (!w_seed_source_init((w_seed_byte_view){source_bytes, length}, &source,
                          &source_error)) {
    report_failure(path, "source is not valid UTF-8");
    return 2;
  }
  w_seed_parser parser;
  w_seed_lex_error lex_error;
  if (!w_seed_parser_init(
          &source, (w_seed_span){0u, length},
          (w_seed_foreign_limits){65536u, 256u}, lexer_frames,
          CHECK_LEXER_FRAMES, tokens, CHECK_TOKENS, nodes, CHECK_NODES,
          parse_frames, CHECK_PARSE_FRAMES, issues, CHECK_ISSUES, &parser,
          &lex_error)) {
    report_failure(path, "parser initialization failed");
    return 2;
  }
  w_seed_parse_result parse;
  if (!w_seed_parser_parse(&parser, &parse) ||
      parse.status != W_SEED_PARSE_COMPLETE || parse.issue_count != 0u) {
    report_failure(path, "source parse is incomplete");
    return 2;
  }

  for (size_t index = 0u; index < parse.node_count; index += 1u) {
    if (nodes[index].kind == W_SEED_CST_IMPORT) {
      report_failure(path, "imports require module graph resolution");
      return 2;
    }
  }

  w_seed_module_scan_result module_scan;
  if (w_seed_module_scan(&source, nodes, parse.node_count, &parse,
                         module_origins, CHECK_IMPORTS, &module_scan) !=
      W_SEED_MODULE_SCAN_OK) {
    report_failure(path, "module origin scan failed");
    return 2;
  }

  const size_t path_length = strlen(path);
  w_seed_frontend_text local_module_name;
  if (module_scan.has_module_header_name) {
    const w_seed_span header = module_scan.module_header_name_span;
    local_module_name = (w_seed_frontend_text){
        (const char *)source.bytes.data + header.start_byte,
        header.end_byte - header.start_byte,
    };
  } else if (!local_module_name_from_path(path, path_length,
                                          &local_module_name)) {
    report_failure(path, "logical module name is empty");
    return 2;
  }
  const w_seed_frontend_document document = {
      .logical_source_id = {path, path_length},
      .module_id = {path, path_length},
      .local_module_name = local_module_name,
      .source = &source,
      .nodes = nodes,
      .node_count = parse.node_count,
      .parse = parse,
  };
  const w_seed_frontend_input input = {
      .documents = &document,
      .document_count = 1u,
      .external_modules = NULL,
      .external_module_count = 0u,
  };
  w_seed_frontend_output output = {
      .modules = modules,
      .module_capacity = CHECK_MODULES,
      .imports = imports,
      .import_capacity = CHECK_IMPORTS,
      .import_items = import_items,
      .import_item_capacity = CHECK_IMPORT_ITEMS,
      .structs = structs,
      .struct_capacity = CHECK_STRUCTS,
      .generic_parameters = generic_parameters,
      .generic_parameter_capacity = CHECK_GENERIC_PARAMETERS,
      .generic_applications = generic_applications,
      .generic_application_capacity = CHECK_GENERIC_APPLICATIONS,
      .generic_arguments = generic_arguments,
      .generic_argument_capacity = CHECK_GENERIC_ARGUMENTS,
      .typed_const_expressions = typed_const_expressions,
      .typed_const_expression_capacity = CHECK_TYPED_CONST_EXPRESSIONS,
      .const_values = const_values,
      .const_value_capacity = CHECK_CONST_VALUES,
      .const_elements = const_elements,
      .const_element_capacity = CHECK_CONST_ELEMENTS,
      .const_bytes = const_bytes,
      .const_bytes_capacity = CHECK_CONST_BYTES,
      .enums = enums,
      .enum_capacity = CHECK_ENUMS,
      .enum_cases = enum_cases,
      .enum_case_capacity = CHECK_ENUM_CASES,
      .enum_case_parameters = enum_case_parameters,
      .enum_case_parameter_capacity = CHECK_ENUM_CASE_PARAMETERS,
      .enum_subset_members = enum_subset_members,
      .enum_subset_member_capacity = CHECK_ENUM_SUBSET_MEMBERS,
      .fields = fields,
      .field_capacity = CHECK_FIELDS,
      .type_declarations = type_declarations,
      .type_declaration_capacity = CHECK_DECLARATIONS,
      .aliases = aliases,
      .alias_capacity = CHECK_DECLARATIONS,
      .const_declarations = const_declarations,
      .const_declaration_capacity = CHECK_CONST_DECLARATIONS,
      .types = types,
      .type_capacity = CHECK_TYPES,
      .functions = functions,
      .function_capacity = CHECK_FUNCTIONS,
      .parameters = parameters,
      .parameter_capacity = CHECK_PARAMETERS,
      .arguments = arguments,
      .argument_capacity = CHECK_ARGUMENTS,
      .switch_arms = switch_arms,
      .switch_arm_capacity = CHECK_SWITCH_ARMS,
      .enum_membership_cases = enum_membership_cases,
      .enum_membership_case_capacity = CHECK_ENUM_MEMBERSHIP_CASES,
      .entries = entries,
      .entry_capacity = CHECK_ENTRIES,
      .statements = statements,
      .statement_capacity = CHECK_STATEMENTS,
      .expressions = expressions,
      .expression_capacity = CHECK_EXPRESSIONS,
      .symbols = symbols,
      .symbol_capacity = CHECK_SYMBOLS,
      .facts = facts,
      .fact_capacity = CHECK_FACTS,
      .diagnostics = diagnostics,
      .diagnostic_capacity = CHECK_DIAGNOSTICS,
      .receipt = receipt,
      .receipt_capacity = CHECK_RECEIPT,
  };
  w_seed_frontend_result frontend_result;
  const w_seed_frontend_status frontend_status =
      w_seed_frontend_run(&input, &output, &frontend_result);
  if (frontend_status != frontend_result.status ||
      !frontend_result_complete(&frontend_result)) {
    report_failure(path, "frontend result is incomplete or unsupported");
    return 2;
  }
  if (frontend_status == W_SEED_FRONTEND_OK) return 0;

  for (size_t index = 0u; index < frontend_result.written.diagnostics;
       index += 1u) {
    w_seed_diagnostic_result measured;
    char instance[8];
    const int written = snprintf(instance, sizeof(instance), "D%06" PRIuMAX,
                                 (uintmax_t)(index + 1u));
    if (written != 7 ||
        w_seed_diagnostic_frontend_record(
            instance, sizeof(instance) - 1u, path, path_length, &source,
            0u, &diagnostics[index], NULL, 0u, &measured) !=
            W_SEED_DIAGNOSTIC_CAPACITY) {
      report_failure(path, "frontend diagnostic is outside the bounded D0 mapping");
      return 2;
    }
  }
  if (json) return emit_json_diagnostics(path, &source,
                                         frontend_result.written.diagnostics);
  for (size_t index = 0u; index < frontend_result.written.diagnostics;
       index += 1u) {
    if (report_human_diagnostic(path, &source, &diagnostics[index]) != 0) {
      report_failure(path, "cannot render diagnostic source location");
      return 3;
    }
  }
  return 1;
}
