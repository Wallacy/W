#include "w_seed_accelerated_invocation0.h"
#include "w_seed_accelerated_binding0.h"
#include "w_seed_accelerated_request0.h"
#include "w_seed_gpu0_projection.h"

#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "accelerated invocation check failed: %s (%s:%d)\n", \
                    #condition, __FILE__, __LINE__);                           \
      return false;                                                             \
    }                                                                           \
  } while (0)

enum {
  TEST_NODES = 4096,
  TEST_TOKENS = 4096,
  TEST_FRAMES = 4096,
  TEST_LEX_FRAMES = 1024,
  TEST_ISSUES = 64,
  TEST_MODULES = 4,
  TEST_IMPORTS = 8,
  TEST_IMPORT_ITEMS = 8,
  TEST_STRUCTS = 8,
  TEST_GENERIC_PARAMETERS = 32,
  TEST_GENERIC_APPLICATIONS = 32,
  TEST_GENERIC_ARGUMENTS = 64,
  TEST_TYPED_CONST_EXPRESSIONS = 64,
  TEST_CONST_VALUES = 128,
  TEST_CONST_ELEMENTS = 128,
  TEST_CONST_BYTES = 2048,
  TEST_ENUMS = 8,
  TEST_ENUM_CASES = 32,
  TEST_ENUM_CASE_PARAMETERS = 64,
  TEST_ENUM_SUBSET_MEMBERS = 64,
  TEST_FIELDS = 32,
  TEST_DECLARATIONS = 16,
  TEST_KERNEL_MODULES = 8,
  TEST_KERNEL_BINDINGS = 16,
  TEST_TYPES = 64,
  TEST_FUNCTIONS = 16,
  TEST_PARAMETERS = 32,
  TEST_ENTRIES = 8,
  TEST_STATEMENTS = 64,
  TEST_EXPRESSIONS = 256,
  TEST_INTERPOLATION_SEGMENTS = 64,
  TEST_ARGUMENTS = 64,
  TEST_SWITCH_ARMS = 64,
  TEST_PATTERN_CAPTURES = 64,
  TEST_ENUM_MEMBERSHIP_CASES = 64,
  TEST_SYMBOLS = 128,
  TEST_FACTS = 128,
  TEST_DIAGNOSTICS = 64,
  TEST_DIAGNOSTIC_FACTS = 320,
  TEST_DIAGNOSTIC_ITEMS = 256,
  TEST_DIAGNOSTIC_LABELS = 128,
  TEST_RECEIPT = 32768,
  TEST_GPU_TEXT = 1024,
  TEST_GPU_RECEIPT = TEST_RECEIPT,
  TEST_ACC_TEXT = 256,
  TEST_BINDING_TEXT = 512,
  TEST_GPU0_TEXT = 64,
  TEST_REQUEST_TEXT = 512,
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
  w_seed_frontend_diagnostic_fact diagnostic_facts[TEST_DIAGNOSTIC_FACTS];
  w_seed_frontend_diagnostic_item diagnostic_items[TEST_DIAGNOSTIC_ITEMS];
  w_seed_frontend_diagnostic_label diagnostic_labels[TEST_DIAGNOSTIC_LABELS];
  w_seed_frontend_domain domains[1];
  uint8_t receipt[TEST_RECEIPT];
  w_seed_frontend_output output;
  w_seed_frontend_result result;
} frontend_fixture;

typedef struct {
  w_seed_gpu_module_record modules[TEST_KERNEL_MODULES];
  w_seed_gpu_module_kernel kernels[TEST_KERNEL_BINDINGS];
  uint8_t text[TEST_GPU_TEXT];
  uint8_t receipt[TEST_GPU_RECEIPT];
} gpu_storage;

typedef struct {
  w_seed_accelerated_invocation0_record invocations[1];
  uint8_t text[TEST_ACC_TEXT];
} acc_storage;

typedef struct {
  w_seed_accelerated_binding0_record bindings[1];
  uint8_t binding_text[TEST_BINDING_TEXT];
  w_seed_gpu0_function functions[W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY];
  w_seed_gpu0_operation operations[W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY];
  char gpu0_text[TEST_GPU0_TEXT];
  uint8_t host_artifact[W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY];
  uint8_t device_artifact[W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY];
  int32_t host_result;
  int32_t device_result;
} accelerated_provider_storage;

typedef struct {
  w_seed_accelerated_request0_record requests[1];
  uint8_t text[TEST_REQUEST_TEXT];
  uint8_t device_artifact[W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY];
} accelerated_request_storage;

static frontend_fixture frontend;
static gpu_storage gpu;
static acc_storage accelerated;
static acc_storage negative;
static accelerated_provider_storage provider_storage;
static accelerated_request_storage request_storage;

static bool all_bytes_equal(const void *data, size_t bytes, uint8_t value) {
  if (bytes != 0u && data == NULL) return false;
  const uint8_t *cursor = (const uint8_t *)data;
  for (size_t index = 0u; index < bytes; index += 1u)
    if (cursor[index] != value) return false;
  return true;
}

static bool read_file(const char *path, char *buffer, size_t capacity) {
  if (path == NULL || buffer == NULL || capacity < 2u) return false;
  FILE *file = fopen(path, "rb");
  if (file == NULL) return false;
  const size_t bytes = fread(buffer, 1u, capacity - 1u, file);
  const int extra = fgetc(file);
  const bool valid = !ferror(file) && extra == EOF && bytes != 0u &&
                     memchr(buffer, '\0', bytes) == NULL;
  (void)fclose(file);
  if (!valid) return false;
  buffer[bytes] = '\0';
  return true;
}

static bool parse_frontend(const char *source_text) {
  (void)memset(&frontend, 0, sizeof(frontend));
  const w_seed_byte_view bytes = {(const uint8_t *)source_text,
                                  strlen(source_text)};
  w_seed_source_error source_error;
  CHECK(w_seed_source_init(bytes, &frontend.source, &source_error));
  w_seed_lex_error lex_error;
  CHECK(w_seed_parser_init(
      &frontend.source, (w_seed_span){0u, bytes.length},
      (w_seed_foreign_limits){65536u, 256u}, frontend.lexer_frames,
      TEST_LEX_FRAMES, frontend.tokens, TEST_TOKENS, frontend.nodes,
      TEST_NODES, frontend.frames, TEST_FRAMES, frontend.issues, TEST_ISSUES,
      &frontend.parser, &lex_error));
  CHECK(w_seed_parser_parse(&frontend.parser, &frontend.parse));
  frontend.document.logical_source_id = (w_seed_frontend_text){"test", 4u};
  frontend.document.module_id = (w_seed_frontend_text){"test", 4u};
  frontend.document.local_module_name = frontend.document.module_id;
  w_seed_module_scan_result scan_result;
  const w_seed_module_scan_status scan_status = w_seed_module_scan(
      &frontend.source, frontend.nodes, frontend.parse.node_count,
      &frontend.parse, NULL, 0u, &scan_result);
  if ((scan_status == W_SEED_MODULE_SCAN_OK ||
       scan_status == W_SEED_MODULE_SCAN_CAPACITY) &&
      scan_result.has_module_header_name) {
    frontend.document.local_module_name = (w_seed_frontend_text){
        (const char *)frontend.source.bytes.data +
            scan_result.module_header_name_span.start_byte,
        scan_result.module_header_name_span.end_byte -
            scan_result.module_header_name_span.start_byte};
  }
  frontend.document.source = &frontend.source;
  frontend.document.nodes = frontend.nodes;
  frontend.document.node_count = frontend.parse.node_count;
  frontend.document.parse = frontend.parse;
  frontend.input.documents = &frontend.document;
  frontend.input.document_count = 1u;
  frontend.input.external_modules = NULL;
  frontend.input.external_module_count = 0u;
  frontend.input.host_scope = NULL;
  frontend.input.import_resolution_complete = false;
  frontend.input.resolved_imports = NULL;
  frontend.input.resolved_import_count = 0u;
  frontend.input.domains = frontend.domains;
  frontend.input.domain_count = 1u;
  frontend.domains[0] = (w_seed_frontend_domain){
      .name = (w_seed_frontend_text){".inference", 10u},
      .kind = W_SEED_FRONTEND_DOMAIN_ACCELERATED,
      .mode = W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
      .capabilities = W_SEED_FRONTEND_DOMAIN_CAPABILITY_DEVICE,
      .maximum = 4u};
  frontend.output = (w_seed_frontend_output){
      .modules = frontend.modules,
      .module_capacity = TEST_MODULES,
      .imports = frontend.imports,
      .import_capacity = TEST_IMPORTS,
      .import_items = frontend.import_items,
      .import_item_capacity = TEST_IMPORT_ITEMS,
      .structs = frontend.structs,
      .struct_capacity = TEST_STRUCTS,
      .generic_parameters = frontend.generic_parameters,
      .generic_parameter_capacity = TEST_GENERIC_PARAMETERS,
      .generic_applications = frontend.generic_applications,
      .generic_application_capacity = TEST_GENERIC_APPLICATIONS,
      .generic_arguments = frontend.generic_arguments,
      .generic_argument_capacity = TEST_GENERIC_ARGUMENTS,
      .typed_const_expressions = frontend.typed_const_expressions,
      .typed_const_expression_capacity = TEST_TYPED_CONST_EXPRESSIONS,
      .const_values = frontend.const_values,
      .const_value_capacity = TEST_CONST_VALUES,
      .const_elements = frontend.const_elements,
      .const_element_capacity = TEST_CONST_ELEMENTS,
      .const_bytes = frontend.const_bytes,
      .const_bytes_capacity = TEST_CONST_BYTES,
      .enums = frontend.enums,
      .enum_capacity = TEST_ENUMS,
      .enum_cases = frontend.enum_cases,
      .enum_case_capacity = TEST_ENUM_CASES,
      .enum_case_parameters = frontend.enum_case_parameters,
      .enum_case_parameter_capacity = TEST_ENUM_CASE_PARAMETERS,
      .enum_subset_members = frontend.enum_subset_members,
      .enum_subset_member_capacity = TEST_ENUM_SUBSET_MEMBERS,
      .fields = frontend.fields,
      .field_capacity = TEST_FIELDS,
      .type_declarations = frontend.type_declarations,
      .type_declaration_capacity = TEST_DECLARATIONS,
      .aliases = frontend.aliases,
      .alias_capacity = TEST_DECLARATIONS,
      .const_declarations = frontend.const_declarations,
      .const_declaration_capacity = TEST_DECLARATIONS,
      .kernel_modules = frontend.kernel_modules,
      .kernel_module_capacity = TEST_KERNEL_MODULES,
      .kernel_bindings = frontend.kernel_bindings,
      .kernel_binding_capacity = TEST_KERNEL_BINDINGS,
      .types = frontend.types,
      .type_capacity = TEST_TYPES,
      .functions = frontend.functions,
      .function_capacity = TEST_FUNCTIONS,
      .parameters = frontend.parameters,
      .parameter_capacity = TEST_PARAMETERS,
      .entries = frontend.entries,
      .entry_capacity = TEST_ENTRIES,
      .statements = frontend.statements,
      .statement_capacity = TEST_STATEMENTS,
      .expressions = frontend.expressions,
      .expression_capacity = TEST_EXPRESSIONS,
      .interpolation_segments = frontend.interpolation_segments,
      .interpolation_segment_capacity = TEST_INTERPOLATION_SEGMENTS,
      .arguments = frontend.arguments,
      .argument_capacity = TEST_ARGUMENTS,
      .switch_arms = frontend.switch_arms,
      .switch_arm_capacity = TEST_SWITCH_ARMS,
      .pattern_captures = frontend.pattern_captures,
      .pattern_capture_capacity = TEST_PATTERN_CAPTURES,
      .enum_membership_cases = frontend.enum_membership_cases,
      .enum_membership_case_capacity = TEST_ENUM_MEMBERSHIP_CASES,
      .symbols = frontend.symbols,
      .symbol_capacity = TEST_SYMBOLS,
      .facts = frontend.facts,
      .fact_capacity = TEST_FACTS,
      .diagnostics = frontend.diagnostics,
      .diagnostic_capacity = TEST_DIAGNOSTICS,
      .diagnostic_facts = frontend.diagnostic_facts,
      .diagnostic_fact_capacity = TEST_DIAGNOSTIC_FACTS,
      .diagnostic_items = frontend.diagnostic_items,
      .diagnostic_item_capacity = TEST_DIAGNOSTIC_ITEMS,
      .diagnostic_labels = frontend.diagnostic_labels,
      .diagnostic_label_capacity = TEST_DIAGNOSTIC_LABELS,
      .receipt = frontend.receipt,
      .receipt_capacity = TEST_RECEIPT};
  const w_seed_frontend_status frontend_status =
      w_seed_frontend_run(&frontend.input, &frontend.output, &frontend.result);
  CHECK(frontend_status == W_SEED_FRONTEND_OK);
  CHECK(frontend.parse.status == W_SEED_PARSE_COMPLETE &&
        frontend.result.status == W_SEED_FRONTEND_OK &&
        frontend.result.written.kernel_modules == 1u &&
        frontend.result.written.kernel_bindings == 1u &&
        frontend.result.written.facts == 0u &&
        frontend.result.written.diagnostics == 0u);
  return true;
}

static bool text_is(const uint8_t *text, size_t text_bytes, size_t offset,
                    size_t bytes, const char *expected) {
  const size_t expected_bytes = strlen(expected);
  return text != NULL && expected != NULL && offset <= text_bytes &&
         bytes <= text_bytes - offset && bytes == expected_bytes &&
         memcmp(text + offset, expected, bytes) == 0;
}

static void fill_digest(uint8_t digest[32], uint8_t seed) {
  for (size_t index = 0u; index < 32u; index += 1u)
    digest[index] = (uint8_t)(seed + (uint8_t)index);
}

static w_seed_accelerated_binding0_closed_profile closed_profile(void) {
  w_seed_accelerated_binding0_closed_profile profile = {
      .schema = W_SEED_ACCELERATED_BINDING0_PROFILE_SCHEMA_VERSION,
      .schema_bytes =
          sizeof(W_SEED_ACCELERATED_BINDING0_PROFILE_SCHEMA_VERSION) - 1u,
      .root_identity = {"root:main", 9u},
      .domain_identity = {"inference", 9u},
      .kernel_contract_name = {"kernels", 7u},
      .kernel_label = {"hello", 5u},
      .module_identity = {"example.kernels@1", 17u},
      .artifact_identity = {"artifact:gpu0", 13u},
      .artifact_module_identity = {"example.kernels@1", 17u},
      .kernel_instance_identity = {"instance:hello:0", 16u},
      .artifact_target = {"nvptx64-nvidia-cuda", 19u},
      .device_target = {"nvptx64-nvidia-cuda", 19u},
      .provider_class = {"cuda", 4u},
      .queue_identity = {"queue:0", 7u},
      .device_identity = {"device:0", 8u},
      .provider_generation = {"generation:1", 12u},
      .bound_gpu_module_index = 0u,
      .bound_gpu_kernel_index = 0u,
      .profile_maximum = 8u,
      .root_maximum = 6u,
      .deployment_maximum = 3u,
      .limits = {5u, 4096u, 1024u, 1024u, 8u, 65536u, 8u, 8u},
      .submission = W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
      .numeric_mode = W_SEED_ACCELERATED_BINDING0_NUMERIC_REPRODUCIBLE,
      .fallback = W_SEED_ACCELERATED_BINDING0_FALLBACK_REJECT,
      .artifact_closed = true,
      .root_owned = true,
      .provider_resolved = true,
      .queue_device_match = true,
      .selected_instance_member = true,
  };
  fill_digest(profile.artifact_provider_abi_digest, UINT8_C(0x20));
  memcpy(profile.provider_abi_digest, profile.artifact_provider_abi_digest,
         32u);
  fill_digest(profile.artifact_instances_digest, UINT8_C(0x50));
  fill_digest(profile.profile_semantic_digest, UINT8_C(0x80));
  fill_digest(profile.profile_provenance_digest, UINT8_C(0xa0));
  fill_digest(profile.root_binding_digest, UINT8_C(0xc0));
  return profile;
}

static bool make_gpu_program(w_seed_gpu_module_program *program,
                             w_seed_gpu_module_result *result) {
  const w_seed_gpu_module_input input = {
      .frontend_input = &frontend.input,
      .frontend_output = &frontend.output,
      .frontend_result = &frontend.result};
  w_seed_gpu_module_counts counts;
  CHECK(w_seed_gpu_module_measure(&input, &counts, result) ==
        W_SEED_GPU_MODULE_OK);
  CHECK(counts.modules == 1u && counts.kernels == 1u &&
        counts.text_bytes != 0u && counts.frontend_receipt_bytes != 0u);
  (void)memset(&gpu, 0xa5, sizeof(gpu));
  w_seed_gpu_module_output output = {
      .modules = gpu.modules,
      .module_capacity = TEST_KERNEL_MODULES,
      .kernels = gpu.kernels,
      .kernel_capacity = TEST_KERNEL_BINDINGS,
      .text = gpu.text,
      .text_capacity = sizeof(gpu.text),
      .frontend_receipt = gpu.receipt,
      .frontend_receipt_capacity = sizeof(gpu.receipt)};
  CHECK(w_seed_gpu_module_run(&input, &output, result) == W_SEED_GPU_MODULE_OK);
  CHECK(w_seed_gpu_module_program_from_output(&output, result, program));
  CHECK(w_seed_gpu_module_verify(program, result));
  CHECK(program->module_count == 1u && program->kernel_count == 1u &&
        program->kernels[0].return_bit_width == 32u &&
        program->kernels[0].return_is_signed && program->kernels[0].payload == 42 &&
        text_is(program->text, program->text_bytes,
                program->modules[0].kernel_contract_name_offset,
                program->modules[0].kernel_contract_name_bytes, "kernels") &&
        text_is(program->text, program->text_bytes,
                program->kernels[0].label_offset,
                program->kernels[0].label_bytes, "hello") &&
        text_is(program->text, program->text_bytes,
                program->kernels[0].function_name_offset,
                program->kernels[0].function_name_bytes, "kernel"));
  return true;
}

static size_t find_expression(w_seed_frontend_expr_kind kind) {
  for (size_t index = 0u; index < frontend.result.written.expressions;
       index += 1u)
    if (frontend.output.expressions[index].kind == kind) return index;
  return SIZE_MAX;
}

static bool run_accelerated(const w_seed_gpu_module_program *gpu_program,
                            const w_seed_gpu_module_result *gpu_result,
                            w_seed_accelerated_invocation0_program *program,
                            w_seed_accelerated_invocation0_result *result) {
  const w_seed_accelerated_invocation0_input input = {
      .frontend_input = &frontend.input,
      .frontend_output = &frontend.output,
      .frontend_result = &frontend.result,
      .gpu_module_program = gpu_program,
      .gpu_module_result = gpu_result};
  w_seed_accelerated_invocation0_counts counts;
  (void)memset(&counts, 0xa5, sizeof(counts));
  (void)memset(result, 0xa5, sizeof(*result));
  CHECK(w_seed_accelerated_invocation0_measure(&input, &counts, result) ==
        W_SEED_ACCELERATED_INVOCATION0_OK);
  CHECK(counts.invocations == 1u && counts.text_bytes != 0u &&
        result->required.invocations == 1u && result->written.invocations == 0u &&
        result->required.text_bytes == counts.text_bytes);
  (void)memset(&accelerated, 0xa5, sizeof(accelerated));
  const w_seed_accelerated_invocation0_output output = {
      .invocations = accelerated.invocations,
      .invocation_capacity = 1u,
      .text = accelerated.text,
      .text_capacity = sizeof(accelerated.text)};
  CHECK(w_seed_accelerated_invocation0_run(&input, &output, result) ==
        W_SEED_ACCELERATED_INVOCATION0_OK);
  CHECK(result->written.invocations == counts.invocations &&
        result->written.text_bytes == counts.text_bytes);
  CHECK(w_seed_accelerated_invocation0_program_from_output(&output, result,
                                                            program));
  CHECK(w_seed_accelerated_invocation0_verify(program, result));
  return true;
}

static bool make_complete_request(
    const w_seed_gpu_module_program *gpu_module_program,
    const w_seed_gpu_module_result *gpu_module_result,
    const w_seed_accelerated_invocation0_program *invocation_program,
    const w_seed_accelerated_invocation0_result *invocation_result,
    w_seed_accelerated_request0_program *request_program,
    w_seed_accelerated_request0_result *request_result) {
  (void)memset(&provider_storage, 0xa5, sizeof(provider_storage));
  (void)memset(&request_storage, 0xa5, sizeof(request_storage));
  w_seed_accelerated_binding0_closed_profile profile = closed_profile();
  const w_seed_accelerated_binding0_input binding_input = {
      invocation_program, invocation_result, &profile};
  w_seed_accelerated_binding0_counts binding_counts;
  w_seed_accelerated_binding0_result binding_result;
  CHECK(w_seed_accelerated_binding0_measure(&binding_input, &binding_counts,
                                             &binding_result) ==
        W_SEED_ACCELERATED_BINDING0_OK);
  const w_seed_accelerated_binding0_output binding_output = {
      provider_storage.bindings, 1u, provider_storage.binding_text,
      sizeof(provider_storage.binding_text)};
  CHECK(w_seed_accelerated_binding0_run(&binding_input, &binding_output,
                                         &binding_result) ==
        W_SEED_ACCELERATED_BINDING0_OK);
  w_seed_accelerated_binding0_program binding_program;
  CHECK(w_seed_accelerated_binding0_program_from_output(
      &binding_output, &binding_result, &binding_program));
  CHECK(w_seed_accelerated_binding0_verify(&binding_program, &binding_result));

  const w_seed_gpu0_projection_output projection_output = {
      provider_storage.functions,
      W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY,
      provider_storage.operations,
      W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY,
      provider_storage.gpu0_text,
      sizeof(provider_storage.gpu0_text)};
  w_seed_gpu0_program gpu0_program;
  CHECK(w_seed_gpu0_program_from_gpu_module(
      gpu_module_program, gpu_module_result, 0u, 0u, &projection_output,
      &gpu0_program));
  provider_storage.host_result = -1;
  provider_storage.device_result = -1;
  const w_seed_gpu0_output gpu0_output = {
      provider_storage.host_artifact,
      sizeof(provider_storage.host_artifact),
      provider_storage.device_artifact,
      sizeof(provider_storage.device_artifact),
      &provider_storage.device_result,
      &provider_storage.host_result};
  w_seed_gpu0_result gpu0_result;
  CHECK(w_seed_gpu0_run(&gpu0_program, &gpu0_output, &gpu0_result) ==
        W_SEED_GPU0_OK);
  CHECK(w_seed_gpu0_verify(&gpu0_program, &gpu0_output, &gpu0_result));

  const w_seed_accelerated_request0_input request_input = {
      &binding_program, &binding_result, &gpu0_program, &gpu0_output,
      &gpu0_result};
  w_seed_accelerated_request0_counts request_counts;
  CHECK(w_seed_accelerated_request0_measure(
            &request_input, &request_counts, request_result) ==
        W_SEED_ACCELERATED_REQUEST0_OK);
  const w_seed_accelerated_request0_output request_output = {
      request_storage.requests,
      1u,
      request_storage.text,
      sizeof(request_storage.text),
      request_storage.device_artifact,
      sizeof(request_storage.device_artifact)};
  CHECK(w_seed_accelerated_request0_run(&request_input, &request_output,
                                         request_result) ==
        W_SEED_ACCELERATED_REQUEST0_OK);
  CHECK(w_seed_accelerated_request0_program_from_output(
      &request_output, request_result, request_program));
  CHECK(w_seed_accelerated_request0_verify(request_program, request_result));
  CHECK(request_program->request_count == 1u &&
        request_program->requests[0].sentinel_expected_i32 == 42 &&
        text_is(request_program->text, request_program->text_bytes,
                request_program->requests[0].kernel_symbol_offset,
                request_program->requests[0].kernel_symbol_bytes,
                "w_gpu0_kernel"));

  /* ACCREQ0 owns the complete request and artifact after physical and binding
   * producer storage disappears. */
  (void)memset(&profile, 0, sizeof(profile));
  (void)memset(&binding_program, 0, sizeof(binding_program));
  (void)memset(&binding_result, 0, sizeof(binding_result));
  (void)memset(&gpu0_program, 0, sizeof(gpu0_program));
  (void)memset(&gpu0_result, 0, sizeof(gpu0_result));
  (void)memset(&provider_storage, 0, sizeof(provider_storage));
  CHECK(w_seed_accelerated_request0_verify(request_program, request_result));
  return true;
}

static bool check_identity_and_relations(
    const w_seed_accelerated_invocation0_program *program) {
  const w_seed_accelerated_invocation0_record *record = &program->invocations[0];
  CHECK(text_is(program->text, program->text_bytes, record->domain_name_offset,
                record->domain_name_bytes, ".inference"));
  CHECK(text_is(program->text, program->text_bytes, record->module_name_offset,
                record->module_name_bytes, "kernels"));
  CHECK(text_is(program->text, program->text_bytes, record->kernel_label_offset,
                record->kernel_label_bytes, "hello"));
  CHECK(text_is(program->text, program->text_bytes,
                record->function_name_offset, record->function_name_bytes,
                "kernel"));
  CHECK(record->frontend_module_index == 0u &&
        record->frontend_kernel_module_index == 0u &&
        record->frontend_kernel_binding_index == 0u &&
        record->gpu_module_index == 0u && record->gpu_kernel_index == 0u &&
        record->domain_index == 0u &&
        record->domain_kind == W_SEED_FRONTEND_DOMAIN_ACCELERATED &&
        record->submission == W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT &&
        record->domain_capabilities == W_SEED_FRONTEND_DOMAIN_CAPABILITY_DEVICE &&
        record->domain_maximum == 4u && record->result_bit_width == 32u &&
        record->result_is_signed);
  CHECK(program->frontend_domain_count == 1u);
  CHECK(record->source_launch_expression != W_SEED_ACCELERATED_INVOCATION0_NONE &&
        record->source_call_expression != W_SEED_ACCELERATED_INVOCATION0_NONE &&
        record->source_await_expression != W_SEED_ACCELERATED_INVOCATION0_NONE &&
        record->source_launch_binding_statement !=
            W_SEED_ACCELERATED_INVOCATION0_NONE &&
        record->source_result_binding_statement !=
            W_SEED_ACCELERATED_INVOCATION0_NONE);
  CHECK(record->launch_span.start_byte <= record->call_span.start_byte &&
        record->call_span.end_byte <= record->launch_span.end_byte &&
        record->launch_binding_span.start_byte <= record->call_span.start_byte &&
        record->call_span.end_byte <= record->launch_binding_span.end_byte &&
        record->result_binding_span.start_byte <= record->await_span.start_byte &&
        record->await_span.end_byte <= record->result_binding_span.end_byte &&
        record->call_span.end_byte <= record->await_span.start_byte &&
        record->launch_binding_span.end_byte <=
            record->result_binding_span.start_byte);
  return true;
}

static bool test_negative_boundaries(
    const w_seed_gpu_module_program *gpu_program,
    const w_seed_gpu_module_result *gpu_result,
    const w_seed_accelerated_invocation0_program *good_program,
    const w_seed_accelerated_invocation0_result *good_result) {
  const w_seed_accelerated_invocation0_input input = {
      .frontend_input = &frontend.input,
      .frontend_output = &frontend.output,
      .frontend_result = &frontend.result,
      .gpu_module_program = gpu_program,
      .gpu_module_result = gpu_result};
  w_seed_accelerated_invocation0_counts saved_counts;
  w_seed_accelerated_invocation0_result result;
  (void)memset(&result, 0x5a, sizeof(result));
  saved_counts = (w_seed_accelerated_invocation0_counts){0x5a, 0x5a};
  const w_seed_accelerated_invocation0_counts before_counts = saved_counts;
  const w_seed_accelerated_invocation0_result before_result = result;

  const w_seed_frontend_text saved_schema = frontend.result.schema_version;
  frontend.result.schema_version = (w_seed_frontend_text){"forged", 6u};
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_INVALID_SCHEMA);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  frontend.result.schema_version = saved_schema;

  const size_t saved_frontend_types = frontend.result.written.types;
  frontend.result.written.types += 1u;
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_INCONSISTENT);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  frontend.result.written.types = saved_frontend_types;

  const size_t saved_gpu_kernels = gpu_result->written.kernels;
  ((w_seed_gpu_module_result *)(void *)gpu_result)->written.kernels += 1u;
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_INCONSISTENT);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  ((w_seed_gpu_module_result *)(void *)gpu_result)->written.kernels =
      saved_gpu_kernels;

  const size_t call_index = find_expression(W_SEED_FRONTEND_EXPR_CALL);
  const size_t launch_index =
      find_expression(W_SEED_FRONTEND_EXPR_SPAWN_ACCELERATED_DOMAIN_LAUNCH);
  const size_t await_index = find_expression(W_SEED_FRONTEND_EXPR_AWAIT);
  CHECK(call_index != SIZE_MAX && launch_index != SIZE_MAX &&
        await_index != SIZE_MAX);
  w_seed_frontend_expression *call = &frontend.output.expressions[call_index];
  w_seed_frontend_expression *launch =
      &frontend.output.expressions[launch_index];
  w_seed_frontend_expression *await =
      &frontend.output.expressions[await_index];
  CHECK(call->left < frontend.result.written.expressions);
  w_seed_frontend_expression *callee =
      &frontend.output.expressions[call->left];
  CHECK(good_program->invocations[0].source_launch_binding_statement <
            frontend.result.written.statements &&
        good_program->invocations[0].source_result_binding_statement <
            frontend.result.written.statements);
  w_seed_frontend_statement *launch_binding = &frontend.output.statements[
      good_program->invocations[0].source_launch_binding_statement];
  w_seed_frontend_statement *result_binding = &frontend.output.statements[
      good_program->invocations[0].source_result_binding_statement];

  const uint32_t saved_launch_owner = launch->owner_function;
  launch->owner_function = 0u;
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  launch->owner_function = saved_launch_owner;

  const uint32_t saved_callee_module = callee->module_index;
  callee->module_index = 1u;
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  callee->module_index = saved_callee_module;

  const uint32_t saved_callee_owner = callee->owner_function;
  callee->owner_function = 0u;
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  callee->owner_function = saved_callee_owner;

  const uint32_t saved_await_module = await->module_index;
  await->module_index = 1u;
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  await->module_index = saved_await_module;

  const uint32_t saved_launch_binding_module = launch_binding->module_index;
  launch_binding->module_index = 1u;
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  launch_binding->module_index = saved_launch_binding_module;

  const uint32_t saved_result_binding_module = result_binding->module_index;
  result_binding->module_index = 1u;
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  result_binding->module_index = saved_result_binding_module;

  const uint32_t saved_argument_count = call->argument_count;
  call->argument_count = 1u;
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  call->argument_count = saved_argument_count;

  const uint32_t saved_module = call->resolved_kernel_module_index;
  call->resolved_kernel_module_index =
      W_SEED_ACCELERATED_INVOCATION0_NONE;
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  call->resolved_kernel_module_index = saved_module;

  const w_seed_frontend_domain_kind saved_domain_kind = frontend.domains[0].kind;
  frontend.domains[0].kind = W_SEED_FRONTEND_DOMAIN_HOST;
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  frontend.domains[0].kind = saved_domain_kind;

  const w_seed_frontend_expr_kind saved_await_kind =
      frontend.output.expressions[await_index].kind;
  frontend.output.expressions[await_index].kind = W_SEED_FRONTEND_EXPR_INTEGER;
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  frontend.output.expressions[await_index].kind = saved_await_kind;

  const w_seed_frontend_expression saved_extra = frontend.output.expressions[0];
  frontend.output.expressions[0] = frontend.output.expressions[launch_index];
  CHECK(w_seed_accelerated_invocation0_measure(&input, &saved_counts, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED);
  CHECK(memcmp(&before_counts, &saved_counts, sizeof(saved_counts)) == 0 &&
        memcmp(&before_result, &result, sizeof(result)) == 0);
  frontend.output.expressions[0] = saved_extra;

  w_seed_accelerated_invocation0_output output = {
      .invocations = negative.invocations,
      .invocation_capacity = 0u,
      .text = negative.text,
      .text_capacity = sizeof(negative.text)};
  (void)memset(&negative, 0x5a, sizeof(negative));
  CHECK(w_seed_accelerated_invocation0_run(&input, &output, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_CAPACITY);
  CHECK(all_bytes_equal(&negative, sizeof(negative), 0x5a));
  output.invocations = NULL;
  CHECK(w_seed_accelerated_invocation0_run(&input, &output, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_CAPACITY);

  output = (w_seed_accelerated_invocation0_output){
      .invocations = accelerated.invocations,
      .invocation_capacity = 1u,
      .text = (uint8_t *)(void *)accelerated.invocations,
      .text_capacity = sizeof(accelerated.invocations)};
  CHECK(w_seed_accelerated_invocation0_run(&input, &output, &result) ==
        W_SEED_ACCELERATED_INVOCATION0_ALIAS);
  CHECK(w_seed_accelerated_invocation0_verify(good_program, good_result));

  w_seed_accelerated_invocation0_program forged = *good_program;
  const uint32_t saved_index = accelerated.invocations[0].gpu_kernel_index;
  accelerated.invocations[0].gpu_kernel_index = 1u;
  CHECK(!w_seed_accelerated_invocation0_verify(&forged, good_result));
  accelerated.invocations[0].gpu_kernel_index = saved_index;
  const w_seed_span saved_span = accelerated.invocations[0].call_span;
  accelerated.invocations[0].call_span.end_byte = saved_span.end_byte + 1u;
  CHECK(!w_seed_accelerated_invocation0_verify(&forged, good_result));
  accelerated.invocations[0].call_span = saved_span;
  const uint32_t saved_capability = accelerated.invocations[0].domain_capabilities;
  accelerated.invocations[0].domain_capabilities =
      W_SEED_FRONTEND_DOMAIN_CAPABILITY_NONE;
  CHECK(!w_seed_accelerated_invocation0_verify(&forged, good_result));
  accelerated.invocations[0].domain_capabilities = saved_capability;
  const uint8_t saved_text = accelerated.text[0];
  accelerated.text[0] = (uint8_t)'?';
  CHECK(!w_seed_accelerated_invocation0_verify(&forged, good_result));
  accelerated.text[0] = saved_text;
  w_seed_accelerated_invocation0_result forged_result = *good_result;
  forged_result.semantic_digest[0] ^= UINT8_C(1);
  CHECK(!w_seed_accelerated_invocation0_verify(good_program, &forged_result));
  CHECK(w_seed_accelerated_invocation0_verify(good_program, good_result));
  return true;
}

static bool emit_request_artifact(
    const char *path,
    const w_seed_accelerated_request0_program *request_program,
    const w_seed_accelerated_request0_result *request_result) {
  CHECK(path != NULL && path[0] != '\0' &&
        w_seed_accelerated_request0_verify(request_program, request_result));
  const w_seed_accelerated_request0_record *request =
      &request_program->requests[0];
  CHECK(request->kernel_symbol_bytes <= (size_t)INT_MAX &&
        request->kernel_symbol_offset <= request_program->text_bytes &&
        request->kernel_symbol_bytes <=
            request_program->text_bytes - request->kernel_symbol_offset);
  FILE *existing = fopen(path, "rb");
  if (existing != NULL) {
    (void)fclose(existing);
    return false;
  }
  FILE *file = fopen(path, "wb");
  if (file == NULL) return false;
  bool okay = fwrite(request_program->device_artifact, 1u,
                     request_program->device_artifact_bytes, file) ==
                  request_program->device_artifact_bytes &&
              fflush(file) == 0;
  if (fclose(file) != 0) okay = false;
  if (!okay) {
    (void)remove(path);
    return false;
  }
  const int printed = fprintf(
      stdout,
      "{\"schema\":\"%s\",\"kernel\":\"%.*s\",\"expected\":%ld,"
      "\"deviceArtifactBytes\":%" PRIuMAX "}\n",
      W_SEED_ACCELERATED_REQUEST0_SCHEMA_VERSION,
      (int)request->kernel_symbol_bytes,
      (const char *)request_program->text + request->kernel_symbol_offset,
      (long)request->sentinel_expected_i32,
      (uintmax_t)request_program->device_artifact_bytes);
  if (printed < 0 || fflush(stdout) != 0) {
    (void)remove(path);
    return false;
  }
  return true;
}

static bool test_accelerated_invocation(const char *fixture_path,
                                        const char *emit_path) {
  char source[4096];
  CHECK(read_file(fixture_path, source, sizeof(source)));
  CHECK(parse_frontend(source));
  w_seed_gpu_module_program gpu_program;
  w_seed_gpu_module_result gpu_result;
  (void)memset(&gpu_program, 0, sizeof(gpu_program));
  (void)memset(&gpu_result, 0, sizeof(gpu_result));
  CHECK(make_gpu_program(&gpu_program, &gpu_result));
  w_seed_accelerated_invocation0_program program;
  w_seed_accelerated_invocation0_result result;
  (void)memset(&program, 0, sizeof(program));
  CHECK(run_accelerated(&gpu_program, &gpu_result, &program, &result));
  CHECK(check_identity_and_relations(&program));
  CHECK(test_negative_boundaries(&gpu_program, &gpu_result, &program, &result));
  w_seed_accelerated_request0_program request_program;
  w_seed_accelerated_request0_result request_result;
  CHECK(make_complete_request(&gpu_program, &gpu_result, &program, &result,
                              &request_program, &request_result));

  (void)memset(&frontend, 0, sizeof(frontend));
  (void)memset(&gpu, 0, sizeof(gpu));
  CHECK(w_seed_accelerated_invocation0_verify(&program, &result));
  (void)memset(&accelerated, 0, sizeof(accelerated));
  (void)memset(&negative, 0, sizeof(negative));
  CHECK(w_seed_accelerated_request0_verify(&request_program, &request_result));
  if (emit_path != NULL)
    return emit_request_artifact(emit_path, &request_program, &request_result);
  (void)printf("ACCINV0 source->frontend31->gpu-module-2->program: PASS\n");
  (void)printf("ACCINV0 identities/spans/digests/teardown/negative barriers: PASS\n");
  (void)printf("ACCREQ0 source-derived request/artifact/teardown: PASS\n");
  return true;
}

int main(int argc, char **argv) {
  if (argc == 2)
    return test_accelerated_invocation(argv[1], NULL) ? 0 : 1;
  if (argc == 4 && strcmp(argv[1], "--emit-request") == 0)
    return test_accelerated_invocation(argv[2], argv[3]) ? 0 : 1;
  (void)fprintf(stderr,
                "usage: %s <gpu-module-fixture>\n"
                "   or: %s --emit-request <gpu-module-fixture> <device.mlir>\n",
                argv[0], argv[0]);
  return 2;
}
