#include "w_seed_product_closure0.h"
#include "w_seed_mlir0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Reuse the parsed two-document witness and its caller-owned HIR setup.  The
 * included translation unit has a renamed main, so this test exercises the
 * same source-backed app -> lib graph without duplicating fixture plumbing. */
#define main w_seed_hir0_multidoc_embedded_main
#include "test_hir0_multidoc.c"
#undef main
#undef CHECK

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "product closure check failed: %s (%s:%d)\n",   \
                    #condition, __FILE__, __LINE__);                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

enum {
  PRODUCT_MODULES = W_SEED_PRODUCT_CLOSURE0_MAX_MODULES,
  PRODUCT_FUNCTIONS = W_SEED_PRODUCT_CLOSURE0_MAX_FUNCTIONS,
  PRODUCT_IDENTITIES = W_SEED_PRODUCT_CLOSURE0_MAX_IDENTITIES,
  PRODUCT_TYPES = W_SEED_PRODUCT_CLOSURE0_MAX_TYPES,
  PRODUCT_VALUES = W_SEED_PRODUCT_CLOSURE0_MAX_VALUES,
  PRODUCT_REQUIREMENTS = W_SEED_PRODUCT_CLOSURE0_MAX_REQUIREMENTS,
  PRODUCT_EXTERNAL_MODULES = W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_MODULES,
  PRODUCT_EXTERNAL_SYMBOLS = W_SEED_PRODUCT_CLOSURE0_MAX_EXTERNAL_SYMBOLS,
};

typedef struct {
  uint32_t reachable_modules[PRODUCT_MODULES];
  uint32_t omitted_modules[PRODUCT_MODULES];
  uint32_t reachable_functions[PRODUCT_FUNCTIONS];
  uint32_t omitted_functions[PRODUCT_FUNCTIONS];
  uint32_t reachable_identities[PRODUCT_IDENTITIES];
  uint32_t reachable_types[PRODUCT_TYPES];
  uint32_t reachable_values[PRODUCT_VALUES];
  uint32_t reachable_requirements[PRODUCT_REQUIREMENTS];
  uint32_t reachable_external_modules[PRODUCT_EXTERNAL_MODULES];
  uint32_t reachable_external_symbols[PRODUCT_EXTERNAL_SYMBOLS];
  uint32_t module_remap[PRODUCT_MODULES];
  uint32_t function_remap[PRODUCT_FUNCTIONS];
  uint32_t identity_remap[PRODUCT_IDENTITIES];
  uint32_t type_remap[PRODUCT_TYPES];
  uint32_t value_remap[PRODUCT_VALUES];
  uint32_t requirement_remap[PRODUCT_REQUIREMENTS];
  uint32_t external_module_remap[PRODUCT_EXTERNAL_MODULES];
  uint32_t external_symbol_remap[PRODUCT_EXTERNAL_SYMBOLS];
  w_seed_product_closure0_value_fact value_facts[PRODUCT_VALUES];
  w_seed_product_closure0_type_fact type_facts[PRODUCT_TYPES];
  w_seed_product_closure0_requirement_fact requirement_facts[
      PRODUCT_REQUIREMENTS];
  w_seed_product_closure0_checked_fault_operation checked_fault_operations[
      PRODUCT_VALUES];
} product_storage;

static w_seed_product_closure0_output product_output(product_storage *storage) {
  return (w_seed_product_closure0_output){
      .reachable_modules = storage->reachable_modules,
      .reachable_module_capacity = PRODUCT_MODULES,
      .omitted_modules = storage->omitted_modules,
      .omitted_module_capacity = PRODUCT_MODULES,
      .reachable_functions = storage->reachable_functions,
      .reachable_function_capacity = PRODUCT_FUNCTIONS,
      .omitted_functions = storage->omitted_functions,
      .omitted_function_capacity = PRODUCT_FUNCTIONS,
      .reachable_identities = storage->reachable_identities,
      .reachable_identity_capacity = PRODUCT_IDENTITIES,
      .reachable_types = storage->reachable_types,
      .reachable_type_capacity = PRODUCT_TYPES,
      .reachable_values = storage->reachable_values,
      .reachable_value_capacity = PRODUCT_VALUES,
      .reachable_requirements = storage->reachable_requirements,
      .reachable_requirement_capacity = PRODUCT_REQUIREMENTS,
      .reachable_external_modules = storage->reachable_external_modules,
      .reachable_external_module_capacity = PRODUCT_EXTERNAL_MODULES,
      .reachable_external_symbols = storage->reachable_external_symbols,
      .reachable_external_symbol_capacity = PRODUCT_EXTERNAL_SYMBOLS,
      .module_remap = storage->module_remap,
      .module_remap_capacity = PRODUCT_MODULES,
      .function_remap = storage->function_remap,
      .function_remap_capacity = PRODUCT_FUNCTIONS,
      .identity_remap = storage->identity_remap,
      .identity_remap_capacity = PRODUCT_IDENTITIES,
      .type_remap = storage->type_remap,
      .type_remap_capacity = PRODUCT_TYPES,
      .value_remap = storage->value_remap,
      .value_remap_capacity = PRODUCT_VALUES,
      .requirement_remap = storage->requirement_remap,
      .requirement_remap_capacity = PRODUCT_REQUIREMENTS,
      .external_module_remap = storage->external_module_remap,
      .external_module_remap_capacity = PRODUCT_EXTERNAL_MODULES,
      .external_symbol_remap = storage->external_symbol_remap,
      .external_symbol_remap_capacity = PRODUCT_EXTERNAL_SYMBOLS,
      .value_facts = storage->value_facts,
      .value_fact_capacity = PRODUCT_VALUES,
      .type_facts = storage->type_facts,
      .type_fact_capacity = PRODUCT_TYPES,
      .requirement_facts = storage->requirement_facts,
      .requirement_fact_capacity = PRODUCT_REQUIREMENTS,
      .checked_fault_operations = storage->checked_fault_operations,
      .checked_fault_operation_capacity = PRODUCT_VALUES};
}

static bool configure_print_host(multidoc_fixture *fixture) {
  if (fixture == NULL) return false;
  fixture->host_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  fixture->host_requirements[0] = (w_seed_frontend_host_requirement){
      .name = (w_seed_frontend_text){"Console", 7u}};
  fixture->host_symbols[0] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"print", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = fixture->host_parameters,
      .parameter_count = 1u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = fixture->host_requirements,
      .requirement_count = 1u};
  fixture->host_scope.profile = (w_seed_frontend_text){"native-process@1", 16u};
  fixture->host_scope.symbols = fixture->host_symbols;
  fixture->host_scope.symbol_count = 1u;
  return true;
}

static bool prepare_print_fixture(multidoc_fixture *fixture,
                                  const char *root_source,
                                  const char *library_source) {
  if (!initialize_fixture(fixture, root_source, library_source) ||
      !configure_print_host(fixture))
    return false;
  setup_frontend_output(fixture);
  return w_seed_frontend_run(&fixture->frontend_input, &fixture->frontend_output,
                             &fixture->frontend_result) == W_SEED_FRONTEND_OK;
}

static bool prepare_process_fixture(multidoc_fixture *fixture,
                                    const char *root_source) {
  if (fixture == NULL || root_source == NULL) return false;
  (void)memset(fixture, 0, sizeof(*fixture));
  if (!parse_document(&fixture->parsed[0], root_source)) return false;
  fixture->documents[0] = (w_seed_frontend_document){
      .logical_source_id = (w_seed_frontend_text){"app-source", 10u},
      .module_id = (w_seed_frontend_text){"app", 3u},
      .local_module_name = (w_seed_frontend_text){"app", 3u},
      .source = &fixture->parsed[0].source,
      .nodes = fixture->parsed[0].nodes,
      .node_count = fixture->parsed[0].parse.node_count,
      .parse = fixture->parsed[0].parse};
  w_seed_module_origin origins[TEST_IMPORTS];
  w_seed_module_scan_result scan_result;
  if (w_seed_module_scan(
          fixture->documents[0].source, fixture->documents[0].nodes,
          fixture->documents[0].parse.node_count, &fixture->documents[0].parse,
          origins, TEST_IMPORTS, &scan_result) != W_SEED_MODULE_SCAN_OK ||
      scan_result.written != 1u)
    return false;
  static const w_seed_frontend_text empty = {NULL, 0u};
  fixture->frontend_external_symbols[0] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"Arguments", 9u},
          .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
          .exported = true,
          .return_type = (w_seed_frontend_text){"Arguments", 9u},
          .receiver_type = empty};
  fixture->frontend_external_symbols[1] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"Context", 7u},
          .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
          .exported = true,
          .return_type = (w_seed_frontend_text){"Context", 7u},
          .receiver_type = empty};
  fixture->frontend_external_symbols[2] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"ExitCode", 8u},
          .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
          .exported = true,
          .return_type = (w_seed_frontend_text){"ExitCode", 8u},
          .receiver_type = empty};
  fixture->frontend_external_symbols[3] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"success", 7u},
          .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
          .exported = true,
          .return_type = (w_seed_frontend_text){"ExitCode", 8u},
          .is_const = true,
          .receiver_type = (w_seed_frontend_text){"ExitCode", 8u}};
  fixture->frontend_external_symbols[4] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"isEmpty", 7u},
          .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
          .exported = true,
          .return_type = (w_seed_frontend_text){"Bool", 4u},
          .is_const = true,
          .receiver_type = (w_seed_frontend_text){"Arguments", 9u}};
  fixture->frontend_external_parameters[0] =
      (w_seed_frontend_external_parameter){
          .name = (w_seed_frontend_text){"code", 4u},
          .type = (w_seed_frontend_text){"i64", 3u},
          .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  fixture->frontend_external_symbols[5] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"failure", 7u},
          .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
          .exported = true,
          .parameters = fixture->frontend_external_parameters,
          .parameter_count = 1u,
          .return_type = (w_seed_frontend_text){"ExitCode", 8u},
          .is_const = true,
          .receiver_type = (w_seed_frontend_text){"ExitCode", 8u}};
  fixture->frontend_external_symbols[6] =
      (w_seed_frontend_external_symbol){
          .name = (w_seed_frontend_text){"count", 5u},
          .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
          .exported = true,
          .return_type = (w_seed_frontend_text){"usize", 5u},
          .is_const = true,
          .receiver_type = (w_seed_frontend_text){"Arguments", 9u}};
  fixture->frontend_external_modules[0] =
      (w_seed_frontend_external_module){
          .module_id = (w_seed_frontend_text){"std.process", 11u},
          .symbols = fixture->frontend_external_symbols,
          .symbol_count = 7u};
  fixture->resolved_imports[0] = (w_seed_frontend_resolved_import){
      .source_document_index = 0u,
      .direct_import_ordinal = origins[0].direct_import_ordinal,
      .import_declaration_span = origins[0].declaration_span,
      .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_EXTERNAL_MODULE,
      .target_index = 0u};
  (void)configure_print_host(fixture);
  fixture->frontend_input = (w_seed_frontend_input){
      .documents = fixture->documents,
      .document_count = 1u,
      .external_modules = fixture->frontend_external_modules,
      .external_module_count = 1u,
      .host_scope = &fixture->host_scope,
      .import_resolution_complete = true,
      .resolved_imports = fixture->resolved_imports,
      .resolved_import_count = 1u};
  setup_frontend_output(fixture);
  const w_seed_frontend_status frontend_status =
      w_seed_frontend_run(&fixture->frontend_input,
                          &fixture->frontend_output,
                          &fixture->frontend_result);
  if (frontend_status != W_SEED_FRONTEND_OK) {
    (void)fprintf(stderr, "process frontend status=%d\n", (int)frontend_status);
    return false;
  }
  const w_seed_hir0_input hir_input = {
      .frontend_input = &fixture->frontend_input,
      .frontend_output = &fixture->frontend_output,
      .frontend_result = &fixture->frontend_result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  const w_seed_hir0_status hir_measure_status =
      w_seed_hir0_measure(&hir_input, &fixture->hir_counts,
                          &fixture->hir_result);
  if (hir_measure_status != W_SEED_HIR0_OK) {
    (void)fprintf(stderr, "process HIR measure status=%d\n",
                  (int)hir_measure_status);
    return false;
  }
  setup_hir_output(fixture);
  const w_seed_hir0_status hir_run_status =
      w_seed_hir0_run(&hir_input, &fixture->hir_output,
                      &fixture->hir_result);
  if (hir_run_status != W_SEED_HIR0_OK) {
    (void)fprintf(stderr, "process HIR run status=%d\n", (int)hir_run_status);
    return false;
  }
  if (!w_seed_hir0_program_from_output(&fixture->hir_output,
                                       &fixture->hir_result,
                                       &fixture->hir_program)) {
    (void)fprintf(stderr, "process HIR program conversion failed\n");
    return false;
  }
  if (!w_seed_hir0_verify(&fixture->hir_program, &fixture->hir_result)) {
    (void)fprintf(stderr, "process HIR verify failed\n");
    return false;
  }
  return true;
}

static bool prepare_direct_fixture(multidoc_fixture *fixture,
                                   const char *root_source) {
  if (fixture == NULL || root_source == NULL) return false;
  (void)memset(fixture, 0, sizeof(*fixture));
  if (!parse_document(&fixture->parsed[0], root_source)) return false;
  fixture->documents[0] = (w_seed_frontend_document){
      .logical_source_id = (w_seed_frontend_text){"app-source", 10u},
      .module_id = (w_seed_frontend_text){"app", 3u},
      .local_module_name = (w_seed_frontend_text){"app", 3u},
      .source = &fixture->parsed[0].source,
      .nodes = fixture->parsed[0].nodes,
      .node_count = fixture->parsed[0].parse.node_count,
      .parse = fixture->parsed[0].parse};
  w_seed_module_origin origins[TEST_IMPORTS];
  w_seed_module_scan_result scan_result;
  if (w_seed_module_scan(
          fixture->documents[0].source, fixture->documents[0].nodes,
          fixture->documents[0].parse.node_count, &fixture->documents[0].parse,
          origins, TEST_IMPORTS, &scan_result) != W_SEED_MODULE_SCAN_OK ||
      scan_result.written != 0u)
    return false;
  (void)configure_print_host(fixture);
  fixture->frontend_input = (w_seed_frontend_input){
      .documents = fixture->documents,
      .document_count = 1u,
      .external_modules = NULL,
      .external_module_count = 0u,
      .host_scope = &fixture->host_scope,
      .import_resolution_complete = true,
      .resolved_imports = NULL,
      .resolved_import_count = 0u};
  setup_frontend_output(fixture);
  if (w_seed_frontend_run(&fixture->frontend_input, &fixture->frontend_output,
                          &fixture->frontend_result) != W_SEED_FRONTEND_OK)
    return false;
  const w_seed_hir0_input hir_input = {
      .frontend_input = &fixture->frontend_input,
      .frontend_output = &fixture->frontend_output,
      .frontend_result = &fixture->frontend_result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  if (w_seed_hir0_measure(&hir_input, &fixture->hir_counts,
                          &fixture->hir_result) != W_SEED_HIR0_OK)
    return false;
  setup_hir_output(fixture);
  if (w_seed_hir0_run(&hir_input, &fixture->hir_output,
                      &fixture->hir_result) != W_SEED_HIR0_OK)
    return false;
  return w_seed_hir0_program_from_output(&fixture->hir_output,
                                         &fixture->hir_result,
                                         &fixture->hir_program) &&
         w_seed_hir0_verify(&fixture->hir_program, &fixture->hir_result);
}

static void reseal_process_hir(multidoc_fixture *fixture) {
  if (fixture == NULL) return;
  uint8_t semantic_digest[W_SEED_PRODUCT_CLOSURE0_DIGEST_BYTES];
  uint8_t provenance_digest[W_SEED_PRODUCT_CLOSURE0_DIGEST_BYTES];
  const w_seed_hir0_counts counts = fixture->hir_result.written;
  digest_program(&fixture->hir_program, &counts, semantic_digest);
  digest_provenance(&fixture->hir_program, &counts, provenance_digest);
  (void)memcpy(fixture->hir_result.semantic_digest, semantic_digest,
               sizeof(semantic_digest));
  (void)memcpy(fixture->hir_result.provenance_digest, provenance_digest,
               sizeof(provenance_digest));
  write_receipt_unchecked(fixture->hir_receipt, &counts, semantic_digest,
                          provenance_digest);
}

static bool add_dead_module(multidoc_fixture *fixture) {
  if (fixture == NULL || fixture->hir_program.module_count != 2u ||
      fixture->hir_program.identity_count < 2u ||
      fixture->hir_program.text_byte_count > sizeof(fixture->hir_text) - 24u)
    return false;
  w_seed_hir0_program *program = &fixture->hir_program;
  w_seed_hir0_module *modules = (w_seed_hir0_module *)(void *)program->modules;
  w_seed_hir0_identity *identities =
      (w_seed_hir0_identity *)(void *)program->identities;
  w_seed_hir0_function *functions =
      (w_seed_hir0_function *)(void *)program->functions;
  w_seed_hir0_entry *entries = (w_seed_hir0_entry *)(void *)program->entries;
  w_seed_hir0_call *calls = (w_seed_hir0_call *)(void *)program->calls;
  w_seed_hir0_requirement *requirements =
      (w_seed_hir0_requirement *)(void *)program->requirements;
  w_seed_hir0_host_parameter *host_parameters =
      (w_seed_hir0_host_parameter *)(void *)program->host_parameters;
  uint8_t *text_bytes = (uint8_t *)(void *)program->text_bytes;
  const size_t old_identity_count = program->identity_count;
  for (size_t index = old_identity_count; index > 2u; index -= 1u)
    identities[index] = identities[index - 1u];
  for (size_t function = 0u; function < program->function_count; function += 1u)
    functions[function].identity_index += 1u;
  for (size_t entry = 0u; entry < program->entry_count; entry += 1u) {
    entries[entry].identity_index += 1u;
    entries[entry].target_identity += 1u;
  }
  for (size_t call = 0u; call < program->call_count; call += 1u)
    if (calls[call].callee_identity >= 2u) calls[call].callee_identity += 1u;
  for (size_t requirement = 0u; requirement < program->requirement_count;
       requirement += 1u)
    if (requirements[requirement].owner_index >= 2u)
      requirements[requirement].owner_index += 1u;
  for (size_t parameter = 0u; parameter < program->host_parameter_count;
       parameter += 1u)
    if (host_parameters[parameter].owner_identity >= 2u)
      host_parameters[parameter].owner_identity += 1u;

  const uint32_t text_offset = (uint32_t)program->text_byte_count;
  static const uint8_t dead_source[] = "deaddead-source";
  const size_t dead_source_bytes = sizeof(dead_source) - 1u;
  (void)memcpy(text_bytes + text_offset, dead_source,
               dead_source_bytes);
  program->text_byte_count += dead_source_bytes;
  const w_seed_hir0_text dead_id = {text_offset, 4u};
  const w_seed_hir0_text dead_source_id = {
      (uint32_t)(text_offset + 4u), 11u};
  const w_seed_hir0_text dead_local = {text_offset, 4u};
  modules[2] = (w_seed_hir0_module){
      .module_index = 2u,
      .identity_index = 2u,
      .source_id = dead_source_id,
      .module_id = dead_id,
      .local_module_name = dead_local,
      .source_span = {0u, (uint32_t)dead_source_bytes},
      .source_length = dead_source_bytes,
      .first_function = (uint32_t)program->function_count,
      .function_count = 0u,
      .first_entry = (uint32_t)program->entry_count,
      .entry_count = 0u};
  identities[2] = (w_seed_hir0_identity){
      .kind = W_SEED_HIR0_IDENTITY_MODULE,
      .owner_module = W_SEED_HIR0_NONE,
      .target_index = 2u,
      .name = dead_id,
      .first_parameter = W_SEED_HIR0_NONE,
      .first_requirement = W_SEED_HIR0_NONE,
      .return_type = W_SEED_HIR0_NONE,
      .profile = {0u, 0u}};
  program->module_count = 3u;
  program->identity_count = old_identity_count + 1u;
  fixture->hir_result.required.modules = program->module_count;
  fixture->hir_result.written.modules = program->module_count;
  fixture->hir_result.required.identities = program->identity_count;
  fixture->hir_result.written.identities = program->identity_count;
  fixture->hir_result.required.text_bytes = program->text_byte_count;
  fixture->hir_result.written.text_bytes = program->text_byte_count;
  fixture->hir_counts.modules = program->module_count;
  fixture->hir_counts.identities = program->identity_count;
  fixture->hir_counts.text_bytes = program->text_byte_count;
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  const w_seed_hir0_counts counts = fixture->hir_result.written;
  digest_program(program, &counts, semantic_digest);
  digest_provenance(program, &counts, provenance_digest);
  (void)memcpy(fixture->hir_result.semantic_digest, semantic_digest,
               sizeof(semantic_digest));
  (void)memcpy(fixture->hir_result.provenance_digest, provenance_digest,
               sizeof(provenance_digest));
  write_receipt_unchecked(fixture->hir_receipt, &counts, semantic_digest,
                          provenance_digest);
  return w_seed_hir0_verify(program, &fixture->hir_result);
}

static bool test_reachable_and_omitted(void) {
  static multidoc_fixture fixture;
  CHECK(prepare_fixture(&fixture, ROOT_SOURCE,
                        "module lib\n"
                        "export fn helper(): i64 { return 42 }\n"
                        "export fn unused_export(): i64 { return 7 }\n"
                        "fn unused_private(): i64 { return 9 }\n"));
  CHECK(lower_fixture(&fixture));
  product_storage storage;
  (void)memset(&storage, 0xa5, sizeof(storage));
  const w_seed_product_closure0_input input =
      {&fixture.hir_program, &fixture.hir_result};
  const w_seed_product_closure0_output output = product_output(&storage);
  w_seed_product_closure0_result result = {0};
  CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(result.root.entry_index == 0u && result.root.module_index == 0u &&
        result.root.function_index == 0u);
  CHECK(result.written.reachable_modules == 2u &&
        result.written.omitted_modules == 0u);
  CHECK(result.written.reachable_functions == 2u &&
        result.written.omitted_functions == 2u);
  CHECK(result.written.reachable_identities == 5u);
  CHECK(storage.reachable_modules[0] == 0u && storage.reachable_modules[1] == 1u);
  CHECK(storage.reachable_functions[0] == 0u && storage.reachable_functions[1] == 1u);
  CHECK(storage.omitted_functions[0] == 2u && storage.omitted_functions[1] == 3u);
  CHECK(storage.module_remap[0] == 0u &&
        storage.function_remap[0] == 0u && storage.function_remap[1] == 1u &&
        storage.function_remap[2] == W_SEED_PRODUCT_CLOSURE0_NONE &&
        storage.function_remap[3] == W_SEED_PRODUCT_CLOSURE0_NONE);
  CHECK(storage.value_facts[0].source_index == storage.reachable_values[0]);
  CHECK(storage.type_facts[0].source_index == storage.reachable_types[0] &&
        result.written.reachable_requirements == 0u);
  CHECK(w_seed_product_closure0_verify(&input, &output, &result));
  CHECK(w_seed_product_closure0_cross_check_functions(
      &fixture.hir_program, &fixture.hir_result,
      (const bool[]){true, true, false, false}, PRODUCT_FUNCTIONS));
  return true;
}

static bool test_native_process_typed_throw(void) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "enum ProcessFailure: Error { denied unavailable }\n"
      "enum OtherFailure: Error { unrelated }\n"
      "enum PlainFailure { plain }\n"
      "fn helper(value: i64): i64 { return value }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws ProcessFailure { throw .denied }\n"
      "entry(run)\n";
  static multidoc_fixture fixture;
  static product_storage storage;
  static product_storage saved_storage;
  CHECK(prepare_process_fixture(&fixture, SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  const uint32_t target = program->entries[0].target_function;
  const w_seed_hir0_function *handler = &program->functions[target];
  const w_seed_hir0_block *block = &program->blocks[handler->first_block];
  const w_seed_hir0_terminator *term =
      &program->terminators[block->terminator_index];
  const w_seed_hir0_value *thrown = &program->values[term->value_index];
  const w_seed_product_closure0_input input =
      {program, &fixture.hir_result};
  (void)memset(&storage, 0xa5, sizeof(storage));
  const w_seed_product_closure0_output output = product_output(&storage);
  w_seed_product_closure0_result result = {0};
  CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(result.root.entry_index == 0u && result.root.module_index == 0u &&
        result.root.function_index == target &&
        result.root.identity_index == program->entries[0].identity_index &&
        result.root.target_identity_index == handler->identity_index &&
        result.root.identity_index != result.root.target_identity_index &&
        result.root.adapter_kind == W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS);
  CHECK(result.outcome.kind == W_SEED_PRODUCT_CLOSURE0_OUTCOME_TYPED_THROW &&
        result.outcome.terminator_index == block->terminator_index &&
        result.outcome.value_index == term->value_index &&
        result.outcome.error_type_index == handler->error_type &&
        result.outcome.error_enum_index == thrown->enum_index &&
        result.outcome.error_case_index == thrown->enum_case_index);
  CHECK(result.root.cleanup_obligation ==
            W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS_REVERSE_ON_TYPED_ERROR &&
        result.root.first_cleanup_owner_parameter == handler->first_parameter &&
        result.root.cleanup_owner_parameter_count == 2u &&
        result.root.cleanup_release_parameter_count == 2u &&
        result.root.cleanup_release_parameters[0] ==
            handler->first_parameter + 1u &&
        result.root.cleanup_release_parameters[1] == handler->first_parameter);
  CHECK(program->types[handler->return_type].kind ==
            W_SEED_HIR0_TYPE_NOMINAL &&
        program->types[handler->return_type].external_symbol_index == 2u);
  CHECK(result.written.reachable_modules == 1u &&
        result.written.reachable_functions == 1u &&
        result.written.omitted_functions == 1u &&
        result.written.reachable_values == 1u &&
        result.written.reachable_types == 4u &&
        result.written.reachable_external_modules == 1u &&
        result.written.reachable_external_symbols == 3u &&
        storage.reachable_functions[0] == target &&
        storage.omitted_functions[0] == 0u &&
        storage.reachable_external_symbols[0] == 0u &&
        storage.reachable_external_symbols[1] == 1u &&
        storage.reachable_external_symbols[2] == 2u);
  CHECK(w_seed_product_closure0_verify(&input, &output, &result));

  w_seed_product_closure0_counts counts = {0};
  w_seed_product_closure0_result measured = {0};
  CHECK(w_seed_product_closure0_measure(&input, &counts, &measured) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(memcmp(&counts, &result.required, sizeof(counts)) == 0 &&
        memcmp(&measured, &result, sizeof(measured)) == 0);

  /* Capacity and alias failures must leave both caller-owned publication
   * buffers and the result unchanged for this new typed root too. */
  saved_storage = storage;
  const w_seed_product_closure0_result saved_result = result;
  w_seed_product_closure0_output limited_output = product_output(&storage);
  limited_output.reachable_external_symbol_capacity = 2u;
  CHECK(w_seed_product_closure0_run(&input, &limited_output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_CAPACITY);
  CHECK(memcmp(&storage, &saved_storage, sizeof(storage)) == 0 &&
        memcmp(&result, &saved_result, sizeof(result)) == 0);

  w_seed_product_closure0_output aliased_output = product_output(&storage);
  aliased_output.reachable_types = (uint32_t *)(void *)fixture.hir_cleanups;
  aliased_output.reachable_type_capacity = PRODUCT_TYPES;
  CHECK(w_seed_product_closure0_run(&input, &aliased_output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_INVALID);
  CHECK(memcmp(&storage, &saved_storage, sizeof(storage)) == 0 &&
        memcmp(&result, &saved_result, sizeof(result)) == 0);
  const w_seed_product_closure0_counts saved_counts = counts;
  w_seed_product_closure0_result measure_result = measured;
  CHECK(w_seed_product_closure0_measure(
            &input,
            (w_seed_product_closure0_counts *)(void *)fixture.hir_cleanups,
            &measure_result) == W_SEED_PRODUCT_CLOSURE0_INVALID);
  CHECK(memcmp(&counts, &saved_counts, sizeof(counts)) == 0 &&
        memcmp(&measure_result, &measured, sizeof(measure_result)) == 0);

  /* The result is a caller-owned fact product: neither its root identity,
   * outcome channel, cleanup order, nor digest can be forged in isolation. */
  result.root.target_identity_index = result.root.identity_index;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;
  result.root.first_cleanup_owner_parameter = handler->first_parameter + 1u;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;
  result.outcome.error_case_index = W_SEED_PRODUCT_CLOSURE0_NONE;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;
  result.root.cleanup_release_parameters[0] = handler->first_parameter;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;
  result.reachable_semantic_digest[0] ^= 1u;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;

  const uint32_t case_name_offset =
      program->enum_cases[thrown->enum_case_index].name.offset;
  const uint8_t saved_case_name = fixture.hir_text[case_name_offset];
  fixture.hir_text[case_name_offset] ^= 1u;
  reseal_process_hir(&fixture);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  fixture.hir_text[case_name_offset] = saved_case_name;
  reseal_process_hir(&fixture);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_product_closure0_verify(&input, &output, &result));

  /* The ProductClosure boundary still insists that HIR itself authenticate a
   * root THROW's shape; a resealed malformed HIR term cannot bypass it. */
  w_seed_hir0_terminator *mutable_term =
      &fixture.hir_terminators[block->terminator_index];
  const w_seed_hir0_terminator saved_term = *mutable_term;
  mutable_term->error_type = handler->error_type;
  reseal_process_hir(&fixture);
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  w_seed_product_closure0_counts forged_counts;
  w_seed_product_closure0_result forged_result;
  (void)memset(&forged_counts, 0x46, sizeof(forged_counts));
  (void)memset(&forged_result, 0x57, sizeof(forged_result));
  const w_seed_product_closure0_counts forged_counts_before = forged_counts;
  const w_seed_product_closure0_result forged_result_before = forged_result;
  CHECK(w_seed_product_closure0_measure(&input, &forged_counts,
                                        &forged_result) ==
        W_SEED_PRODUCT_CLOSURE0_INVALID);
  CHECK(memcmp(&forged_counts, &forged_counts_before, sizeof(forged_counts)) ==
            0 &&
        memcmp(&forged_result, &forged_result_before, sizeof(forged_result)) ==
            0);
  *mutable_term = saved_term;
  reseal_process_hir(&fixture);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

/* Keep a second numeric process witness with one unreachable function and its
 * block/terminator records physically before the live handler.  The HIR
 * indices therefore change while owner/function/block ordinals and all live
 * values remain identical. */
static bool prepend_dead_numeric_function(multidoc_fixture *fixture) {
  if (fixture == NULL) return false;
  w_seed_hir0_program *program = &fixture->hir_program;
  if (program->module_count != 1u || program->function_count == 0u ||
      program->block_count == 0u || program->terminator_count != program->block_count ||
      program->identity_count <= program->module_count ||
      program->text_byte_count > program->text_byte_capacity - 4u)
    return false;
  const size_t old_function_count = program->function_count;
  const size_t old_block_count = program->block_count;
  const size_t old_identity_count = program->identity_count;
  const size_t function_base = program->module_count;
  const size_t old_host_base = program->module_count + old_function_count +
                               program->entry_count;
  if (old_function_count + 1u > program->function_capacity ||
      old_block_count + 1u > program->block_capacity ||
      old_identity_count + 1u > program->identity_capacity)
    return false;

  w_seed_hir0_function *functions =
      (w_seed_hir0_function *)(void *)program->functions;
  w_seed_hir0_block *blocks = (w_seed_hir0_block *)(void *)program->blocks;
  w_seed_hir0_terminator *terminators =
      (w_seed_hir0_terminator *)(void *)program->terminators;
  w_seed_hir0_identity *identities =
      (w_seed_hir0_identity *)(void *)program->identities;
  w_seed_hir0_parameter *parameters =
      (w_seed_hir0_parameter *)(void *)program->parameters;
  w_seed_hir0_block_argument *block_arguments =
      (w_seed_hir0_block_argument *)(void *)program->block_arguments;
  w_seed_hir0_instruction *instructions =
      (w_seed_hir0_instruction *)(void *)program->instructions;
  w_seed_hir0_binding *bindings =
      (w_seed_hir0_binding *)(void *)program->bindings;
  w_seed_hir0_value *values = (w_seed_hir0_value *)(void *)program->values;
  w_seed_hir0_entry *entries = (w_seed_hir0_entry *)(void *)program->entries;
  w_seed_hir0_cleanup *cleanups =
      (w_seed_hir0_cleanup *)(void *)program->cleanups;
  w_seed_hir0_call *calls = (w_seed_hir0_call *)(void *)program->calls;
  w_seed_hir0_edge_argument *edge_arguments =
      (w_seed_hir0_edge_argument *)(void *)program->edge_arguments;
  w_seed_hir0_switch_edge *switch_edges =
      (w_seed_hir0_switch_edge *)(void *)program->switch_edges;
  w_seed_hir0_module *modules =
      (w_seed_hir0_module *)(void *)program->modules;

  const uint32_t dead_name_offset = (uint32_t)program->text_byte_count;
  (void)memcpy((uint8_t *)(void *)program->text_bytes + dead_name_offset,
               "dead", 4u);
  program->text_byte_count += 4u;

  (void)memmove(&identities[function_base + 1u], &identities[function_base],
                (old_identity_count - function_base) * sizeof(*identities));
  for (size_t index = function_base + 1u; index < old_identity_count + 1u;
       index += 1u)
    if (identities[index].kind == W_SEED_HIR0_IDENTITY_FUNCTION)
      identities[index].target_index += 1u;
  identities[function_base] = (w_seed_hir0_identity){
      .kind = W_SEED_HIR0_IDENTITY_FUNCTION,
      .owner_module = 0u,
      .target_index = 0u,
      .name = {dead_name_offset, 4u},
      .first_parameter = 0u,
      .parameter_count = 0u,
      .first_requirement = W_SEED_HIR0_NONE,
      .requirement_count = 0u,
      .return_type = W_SEED_HIR0_TYPE_UNIT,
      .is_const = false,
      .profile = {0u, 0u}};
  for (size_t index = 0u; index < old_function_count; index += 1u)
    functions[index].identity_index += 1u;
  for (size_t index = 0u; index < program->entry_count; index += 1u) {
    entries[index].identity_index += 1u;
    entries[index].target_identity += 1u;
    entries[index].target_function += 1u;
  }
  for (size_t index = 0u; index < program->host_parameter_count; index += 1u)
    if (program->host_parameters[index].owner_identity >= function_base)
      ((w_seed_hir0_host_parameter *)(void *)program->host_parameters)[index]
          .owner_identity += 1u;
  for (size_t index = 0u; index < program->requirement_count; index += 1u)
    if (program->requirements[index].owner_kind ==
            W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY &&
        program->requirements[index].owner_index >= old_host_base)
      ((w_seed_hir0_requirement *)(void *)program->requirements)[index]
          .owner_index += 1u;
  for (size_t index = 0u; index < program->call_count; index += 1u)
    if (calls[index].callee_identity >= function_base)
      calls[index].callee_identity += 1u;

  (void)memmove(&functions[1u], &functions[0u],
                old_function_count * sizeof(*functions));
  functions[0] = (w_seed_hir0_function){
      .module_index = 0u,
      .identity_index = (uint32_t)function_base,
      .name = {dead_name_offset, 4u},
      .exported = false,
      .source_span = {0u, 0u},
      .body_span = {0u, 0u},
      .return_type = W_SEED_HIR0_TYPE_UNIT,
      .error_type = W_SEED_HIR0_NONE,
      .first_parameter = 0u,
      .parameter_count = 0u,
      .first_block = 0u,
      .block_count = 1u,
      .is_const = false,
      .is_async = false,
      .is_throws = false,
      .is_unsafe = false,
      .has_borrow_clause = false,
      .is_anonymous_entry = false,
      .suspension = W_SEED_HIR0_SUSPENSION_NEVER,
      .direct_entry = W_SEED_HIR0_DIRECT_ENTRY_ABSENT};
  functions[1].first_block += 1u;
  for (size_t index = 0u; index < program->parameter_count; index += 1u)
    parameters[index].owner_function += 1u;

  (void)memmove(&blocks[1u], &blocks[0u], old_block_count * sizeof(*blocks));
  (void)memmove(&terminators[1u], &terminators[0u],
                old_block_count * sizeof(*terminators));
  blocks[0] = (w_seed_hir0_block){
      .owner_function = 0u,
      .ordinal = 0u,
      .first_instruction = 0u,
      .instruction_count = 0u,
      .terminator_index = 0u,
      .source_span = {0u, 0u},
      .next_block = W_SEED_HIR0_NONE,
      .first_block_argument = W_SEED_HIR0_NONE,
      .block_argument_count = 0u};
  terminators[0] = (w_seed_hir0_terminator){
      .owner_block = 0u,
      .kind = W_SEED_HIR0_TERMINATOR_RETURN_UNIT,
      .ordinal = 0u,
      .call_index = W_SEED_HIR0_NONE,
      .value_index = W_SEED_HIR0_NONE,
      .result_type = W_SEED_HIR0_TYPE_UNIT,
      .error_type = W_SEED_HIR0_NONE,
      .target_block = W_SEED_HIR0_NONE,
      .else_block = W_SEED_HIR0_NONE,
      .third_block = W_SEED_HIR0_NONE,
      .first_edge_argument = W_SEED_HIR0_NONE,
      .edge_argument_count = 0u,
      .logical_operator = W_SEED_HIR0_LOGICAL_NONE,
      .switch_enum_index = W_SEED_HIR0_NONE,
      .first_switch_edge = W_SEED_HIR0_NONE,
      .switch_edge_count = 0u,
      .switch_carrier_width = 0u,
      .source_span = {0u, 0u},
      .panic_code = W_SEED_HIR0_PANIC_CODE_INVALID,
      .numeric_conversion_error_case =
          W_SEED_HIR0_NUMERIC_CONVERSION_ERROR_NONE,
      .rounding_mode = W_SEED_HIR0_ROUNDING_MODE_NONE};
  for (size_t index = 1u; index < old_block_count + 1u; index += 1u) {
    blocks[index].owner_function += 1u;
    blocks[index].terminator_index += 1u;
    terminators[index].owner_block += 1u;
    if (terminators[index].target_block != W_SEED_HIR0_NONE)
      terminators[index].target_block += 1u;
    if (terminators[index].else_block != W_SEED_HIR0_NONE)
      terminators[index].else_block += 1u;
    if (terminators[index].third_block != W_SEED_HIR0_NONE)
      terminators[index].third_block += 1u;
  }
  for (size_t index = 0u; index < program->block_argument_count; index += 1u)
    block_arguments[index].owner_block += 1u;
  for (size_t index = 0u; index < program->instruction_count; index += 1u)
    instructions[index].owner_block += 1u;
  for (size_t index = 0u; index < program->binding_count; index += 1u)
    bindings[index].owner_block += 1u;
  for (size_t index = 0u; index < program->value_count; index += 1u)
    if (values[index].owner_kind == W_SEED_HIR0_VALUE_OWNER_TERMINATOR)
      values[index].owner_index += 1u;
  for (size_t index = 0u; index < program->cleanup_count; index += 1u) {
    cleanups[index].owner_function += 1u;
    cleanups[index].invoke_terminator += 1u;
    cleanups[index].normal_block += 1u;
    cleanups[index].error_block += 1u;
  }
  for (size_t index = 0u; index < program->call_count; index += 1u) {
    calls[index].owner_block += 1u;
    if (calls[index].owner_terminator != W_SEED_HIR0_NONE)
      calls[index].owner_terminator += 1u;
  }
  for (size_t index = 0u; index < program->edge_argument_count; index += 1u) {
    edge_arguments[index].owner_terminator += 1u;
    edge_arguments[index].owner_block += 1u;
  }
  for (size_t index = 0u; index < program->switch_edge_count; index += 1u) {
    switch_edges[index].owner_terminator += 1u;
    switch_edges[index].target_block += 1u;
  }

  program->function_count += 1u;
  program->identity_count += 1u;
  program->block_count += 1u;
  program->terminator_count += 1u;
  modules[0].function_count += 1u;
  fixture->hir_result.required.functions = program->function_count;
  fixture->hir_result.written.functions = program->function_count;
  fixture->hir_result.required.identities = program->identity_count;
  fixture->hir_result.written.identities = program->identity_count;
  fixture->hir_result.required.blocks = program->block_count;
  fixture->hir_result.written.blocks = program->block_count;
  fixture->hir_result.required.terminators = program->terminator_count;
  fixture->hir_result.written.terminators = program->terminator_count;
  fixture->hir_result.required.text_bytes = program->text_byte_count;
  fixture->hir_result.written.text_bytes = program->text_byte_count;
  fixture->hir_counts.functions = program->function_count;
  fixture->hir_counts.identities = program->identity_count;
  fixture->hir_counts.blocks = program->block_count;
  fixture->hir_counts.terminators = program->terminator_count;
  fixture->hir_counts.text_bytes = program->text_byte_count;
  reseal_process_hir(fixture);
  const bool verified = w_seed_hir0_verify(program, &fixture->hir_result);
  return verified;
}

static bool test_native_process_numeric_split(void) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let narrowed = try i8(exactly: 1) "
      "return .success }\n"
      "entry(run)\n";
  static multidoc_fixture fixture;
  static multidoc_fixture shifted_fixture;
  static product_storage storage;
  static product_storage shifted_storage;
  CHECK(prepare_process_fixture(&fixture, SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  const uint32_t target = program->entries[0].target_function;
  const w_seed_hir0_function *handler = &program->functions[target];
  const uint32_t split_block = handler->first_block;
  const w_seed_hir0_terminator *split =
      &program->terminators[program->blocks[split_block].terminator_index];
  const uint32_t normal_block = split->target_block;
  const uint32_t error_block = split->else_block;
  const w_seed_hir0_terminator *normal =
      &program->terminators[program->blocks[normal_block].terminator_index];
  const w_seed_hir0_terminator *error =
      &program->terminators[program->blocks[error_block].terminator_index];
  const w_seed_product_closure0_input input =
      {program, &fixture.hir_result};
  (void)memset(&storage, 0, sizeof(storage));
  const w_seed_product_closure0_output output = product_output(&storage);
  w_seed_product_closure0_result result = {0};
  CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(result.normal_outcome.kind == W_SEED_PRODUCT_CLOSURE0_OUTCOME_NORMAL &&
        result.normal_outcome.source_terminator_index ==
            program->blocks[split_block].terminator_index &&
        result.normal_outcome.terminator_index ==
            program->blocks[normal_block].terminator_index &&
        result.normal_outcome.successor_block_index == normal_block &&
        result.normal_outcome.successor_argument_index ==
            program->blocks[normal_block].first_block_argument &&
        result.normal_outcome.successor_argument_count == 1u &&
        result.normal_outcome.value_index == normal->value_index &&
        result.normal_outcome.result_type_index == normal->result_type &&
        result.normal_outcome.error_type_index == W_SEED_PRODUCT_CLOSURE0_NONE);
  CHECK(result.outcome.kind == W_SEED_PRODUCT_CLOSURE0_OUTCOME_TYPED_THROW &&
        result.outcome.source_terminator_index ==
            program->blocks[split_block].terminator_index &&
        result.outcome.terminator_index ==
            program->blocks[error_block].terminator_index &&
        result.outcome.successor_block_index == error_block &&
        result.outcome.successor_argument_index ==
            program->blocks[error_block].first_block_argument &&
        result.outcome.successor_argument_count == 1u &&
        result.outcome.value_index == error->value_index &&
        result.outcome.result_type_index == error->result_type &&
        result.outcome.error_type_index == handler->error_type &&
        result.outcome.error_enum_index == W_SEED_PRODUCT_CLOSURE0_NONE &&
        result.outcome.error_case_index == W_SEED_PRODUCT_CLOSURE0_NONE);
  CHECK(result.out_of_range_outcome.kind ==
            W_SEED_PRODUCT_CLOSURE0_OUTCOME_TYPED_THROW &&
        memcmp(&result.outcome, &result.out_of_range_outcome,
               sizeof(result.outcome)) == 0 &&
        result.non_finite_outcome.kind == W_SEED_PRODUCT_CLOSURE0_OUTCOME_NONE);
  CHECK(result.root.cleanup_obligation ==
            W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS_REVERSE_ON_ALL_OUTCOMES &&
        result.root.cleanup_release_parameter_count == 2u &&
        result.root.cleanup_release_parameters[0] == handler->first_parameter + 1u &&
        result.root.cleanup_release_parameters[1] == handler->first_parameter &&
        w_seed_product_closure0_verify(&input, &output, &result));

  CHECK(prepare_process_fixture(&shifted_fixture, SOURCE));
  CHECK(prepend_dead_numeric_function(&shifted_fixture));
  const w_seed_product_closure0_input shifted_input =
      {&shifted_fixture.hir_program, &shifted_fixture.hir_result};
  const w_seed_product_closure0_output shifted_output =
      product_output(&shifted_storage);
  w_seed_product_closure0_result shifted_result = {0};
  CHECK(w_seed_product_closure0_run(&shifted_input, &shifted_output,
                                    &shifted_result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(shifted_result.normal_outcome.source_terminator_index !=
            result.normal_outcome.source_terminator_index &&
        shifted_result.normal_outcome.successor_block_index !=
            result.normal_outcome.successor_block_index &&
        shifted_result.outcome.terminator_index != result.outcome.terminator_index &&
        memcmp(shifted_result.reachable_semantic_digest,
               result.reachable_semantic_digest,
               W_SEED_PRODUCT_CLOSURE0_DIGEST_BYTES) == 0 &&
        w_seed_product_closure0_verify(&shifted_input, &shifted_output,
                                       &shifted_result));

  const w_seed_product_closure0_result saved_result = result;
  result.normal_outcome.successor_block_index = error_block;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;
  result.outcome.successor_argument_index =
      program->blocks[normal_block].first_block_argument;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;
  result.normal_outcome.result_type_index = handler->error_type;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;
  result.outcome.value_index = result.normal_outcome.value_index;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;

  w_seed_hir0_terminator *mutable_split =
      &fixture.hir_terminators[program->blocks[split_block].terminator_index];
  const w_seed_hir0_terminator saved_split = *mutable_split;
  mutable_split->target_block = error_block;
  mutable_split->else_block = normal_block;
  reseal_process_hir(&fixture);
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  CHECK(w_seed_product_closure0_measure(&input,
                                        &(w_seed_product_closure0_counts){0},
                                        &(w_seed_product_closure0_result){0}) ==
        W_SEED_PRODUCT_CLOSURE0_INVALID);
  *mutable_split = saved_split;
  reseal_process_hir(&fixture);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_native_process_checked_local_helper(void) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn checkedOffset(value: u8): u8 { return value + 255_u8 }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let count = try u8(exactly: args.count)\n"
      "print(\"Begin ${checkedOffset(value: count)}\")\n"
      "return .success }\n"
      "entry(run)\n";
  static multidoc_fixture fixture;
  static product_storage storage;
  static product_storage storage_before;
  CHECK(prepare_process_fixture(&fixture, SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  const w_seed_product_closure0_input input =
      {program, &fixture.hir_result};
  (void)memset(&storage, 0, sizeof(storage));
  const w_seed_product_closure0_output output = product_output(&storage);
  w_seed_product_closure0_result result = {0};
  CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  const uint32_t split_index =
      program->blocks[program->functions[program->entries[0].target_function]
                          .first_block]
          .terminator_index;
  const w_seed_hir0_terminator *split = &program->terminators[split_index];
  uint32_t helper_function = W_SEED_HIR0_NONE;
  for (size_t call_index = 0u; call_index < program->call_count; call_index++) {
    const w_seed_hir0_call *call = &program->calls[call_index];
    if (call->callee_identity < program->identity_count &&
        program->identities[call->callee_identity].kind ==
            W_SEED_HIR0_IDENTITY_FUNCTION &&
        program->identities[call->callee_identity].target_index !=
            program->entries[0].target_function)
      helper_function =
          program->identities[call->callee_identity].target_index;
  }
  CHECK(helper_function < program->function_count &&
        result.checked_fault_relation.present &&
        result.checked_fault_relation.source_conversion_terminator_index ==
            split_index &&
        result.checked_fault_relation.normal_successor_block_index ==
            split->target_block &&
        result.checked_fault_relation.conversion_failure_status == 1u &&
        result.checked_fault_relation.arithmetic_failure_status == 2u &&
        result.checked_fault_relation.operation_count == 1u &&
        result.required.reachable_checked_fault_operations == 1u);
  const w_seed_product_closure0_checked_fault_operation saved_operation =
      storage.checked_fault_operations[0];
  CHECK(saved_operation.source_value_index < program->value_count &&
        saved_operation.owner_function_index == helper_function &&
        saved_operation.type_index < program->type_count &&
        program->types[saved_operation.type_index].kind ==
            W_SEED_HIR0_TYPE_INTEGER &&
        !program->types[saved_operation.type_index].integer_is_signed &&
        program->types[saved_operation.type_index].integer_bit_width == 8u &&
        saved_operation.binary_operator == W_SEED_HIR0_BINARY_ADD &&
        program->values[saved_operation.source_value_index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        w_seed_product_closure0_verify(&input, &output, &result));

  const w_seed_product_closure0_result saved_result = result;
  w_seed_product_closure0_checked_fault_operation mutations[4] = {
      saved_operation, saved_operation, saved_operation, saved_operation};
  mutations[0].source_value_index =
      program->values[saved_operation.source_value_index].left_value;
  mutations[1].owner_function_index =
      program->entries[0].target_function;
  mutations[2].type_index = W_SEED_HIR0_NONE;
  for (size_t type_index = 0u; type_index < program->type_count; type_index++)
    if (type_index != saved_operation.type_index) {
      mutations[2].type_index = (uint32_t)type_index;
      break;
    }
  mutations[3].binary_operator = W_SEED_HIR0_BINARY_SUBTRACT;
  CHECK(mutations[0].source_value_index < program->value_count &&
        mutations[0].source_value_index != saved_operation.source_value_index &&
        mutations[1].owner_function_index < program->function_count &&
        mutations[1].owner_function_index != saved_operation.owner_function_index &&
        mutations[2].type_index < program->type_count &&
        mutations[2].type_index != saved_operation.type_index &&
        mutations[3].binary_operator != saved_operation.binary_operator);
  for (size_t mutation_index = 0u; mutation_index < 4u; mutation_index++) {
    storage.checked_fault_operations[0] = mutations[mutation_index];
    (void)memcpy(&storage_before, &storage, sizeof(storage));
    result = saved_result;
    CHECK(!w_seed_product_closure0_verify(&input, &output, &result) &&
          memcmp(&storage_before, &storage, sizeof(storage)) == 0 &&
          memcmp(&result, &saved_result, sizeof(saved_result)) == 0);
  }
  storage.checked_fault_operations[0] = saved_operation;
  result.checked_fault_relation.arithmetic_failure_status = 1u;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;
  result.checked_fault_relation.present = false;
  result.checked_fault_relation.operation_count = 0u;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;
  result.checked_fault_relation.normal_successor_block_index =
      split->else_block;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;
  storage.checked_fault_operations[0].binary_operator =
      W_SEED_HIR0_BINARY_SUBTRACT;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  storage.checked_fault_operations[0] = saved_operation;
  CHECK(w_seed_product_closure0_verify(&input, &output, &result));

  w_seed_product_closure0_output limited_output = output;
  limited_output.checked_fault_operation_capacity = 0u;
  w_seed_product_closure0_result atomic_result = saved_result;
  (void)memcpy(&storage_before, &storage, sizeof(storage));
  CHECK(w_seed_product_closure0_run(&input, &limited_output, &atomic_result) ==
            W_SEED_PRODUCT_CLOSURE0_CAPACITY &&
        memcmp(&atomic_result, &saved_result, sizeof(saved_result)) == 0 &&
        memcmp(&storage_before, &storage, sizeof(storage)) == 0);
  w_seed_product_closure0_output aliased_output = output;
  aliased_output.checked_fault_operations =
      (w_seed_product_closure0_checked_fault_operation *)(void *)program->values;
  aliased_output.checked_fault_operation_capacity = 1u;
  (void)memcpy(&storage_before, &storage, sizeof(storage));
  CHECK(w_seed_product_closure0_run(&input, &aliased_output, &atomic_result) ==
            W_SEED_PRODUCT_CLOSURE0_INVALID &&
        memcmp(&atomic_result, &saved_result, sizeof(saved_result)) == 0 &&
        memcmp(&storage_before, &storage, sizeof(storage)) == 0);

  /* Exact same-start and partial byte-range overlap between two published
   * output projections must reject before changing any output or the result. */
  aliased_output = output;
  aliased_output.checked_fault_operations =
      (w_seed_product_closure0_checked_fault_operation *)(void *)
          storage.value_facts;
  aliased_output.checked_fault_operation_capacity = 1u;
  (void)memcpy(&storage_before, &storage, sizeof(storage));
  atomic_result = saved_result;
  CHECK(w_seed_product_closure0_run(&input, &aliased_output, &atomic_result) ==
            W_SEED_PRODUCT_CLOSURE0_INVALID &&
        memcmp(&atomic_result, &saved_result, sizeof(saved_result)) == 0 &&
        memcmp(&storage_before, &storage, sizeof(storage)) == 0);

  aliased_output = output;
  aliased_output.checked_fault_operations =
      (w_seed_product_closure0_checked_fault_operation *)(void *)(
          (unsigned char *)(void *)storage.value_facts +
          sizeof(storage.value_facts[0]));
  aliased_output.checked_fault_operation_capacity = 1u;
  (void)memcpy(&storage_before, &storage, sizeof(storage));
  atomic_result = saved_result;
  CHECK(w_seed_product_closure0_run(&input, &aliased_output, &atomic_result) ==
            W_SEED_PRODUCT_CLOSURE0_INVALID &&
        memcmp(&atomic_result, &saved_result, sizeof(saved_result)) == 0 &&
        memcmp(&storage_before, &storage, sizeof(storage)) == 0);
  return true;
}

static bool test_native_process_checked_scalar_if_join(void) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn choose(value: i8): i8 { return if value == 2_i8 { "
      "value * 127_i8 } else { value - 1_i8 } }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let count = try i8(exactly: args.count)\n"
      "print(\"Joined ${choose(value: count)}\")\n"
      "return .success }\n"
      "entry(run)\n";
  static multidoc_fixture fixture;
  static product_storage storage;
  CHECK(prepare_process_fixture(&fixture, SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  const w_seed_product_closure0_input input = {program, &fixture.hir_result};
  (void)memset(&storage, 0, sizeof(storage));
  const w_seed_product_closure0_output output = product_output(&storage);
  w_seed_product_closure0_result result = {0};
  CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(result.checked_fault_relation.present &&
        result.checked_fault_relation.conversion_failure_status == 1u &&
        result.checked_fault_relation.arithmetic_failure_status == 2u &&
        result.checked_fault_relation.operation_count == 2u &&
        result.required.reachable_checked_fault_operations == 2u);

  uint32_t helper_function = W_SEED_HIR0_NONE;
  const uint32_t process_function = program->entries[0].target_function;
  for (size_t call_index = 0u; call_index < program->call_count; call_index++) {
    const w_seed_hir0_call *call = &program->calls[call_index];
    if (call->owner_block >= program->block_count ||
        program->blocks[call->owner_block].owner_function != process_function ||
        call->callee_identity >= program->identity_count)
      continue;
    const w_seed_hir0_identity *callee =
        &program->identities[call->callee_identity];
    if (callee->kind == W_SEED_HIR0_IDENTITY_FUNCTION)
      helper_function = callee->target_index;
  }
  CHECK(helper_function < program->function_count &&
        program->functions[helper_function].block_count == 4u);
  bool saw_multiply = false;
  bool saw_subtract = false;
  for (size_t operation_index = 0u; operation_index < 2u; operation_index++) {
    const w_seed_product_closure0_checked_fault_operation *operation =
        &storage.checked_fault_operations[operation_index];
    CHECK(operation->source_value_index < program->value_count &&
          operation->owner_function_index == helper_function &&
          operation->type_index < program->type_count &&
          program->types[operation->type_index].kind ==
              W_SEED_HIR0_TYPE_INTEGER &&
          program->types[operation->type_index].integer_is_signed &&
          program->types[operation->type_index].integer_bit_width == 8u &&
          program->values[operation->source_value_index].kind ==
              W_SEED_HIR0_VALUE_BINARY_I64);
    if (operation->binary_operator == W_SEED_HIR0_BINARY_MULTIPLY)
      saw_multiply = true;
    else if (operation->binary_operator == W_SEED_HIR0_BINARY_SUBTRACT)
      saw_subtract = true;
    else
      CHECK(false);
  }
  CHECK(saw_multiply && saw_subtract &&
        w_seed_product_closure0_verify(&input, &output, &result));
  return true;
}

static bool test_native_process_float_raw_bits_from_runtime_count(void) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let bits: u64 = try u64(exactly: args.count) "
      "let value: f64 = f64.fromBits(bits) "
      "let roundTrip: u64 = value.toBits() "
      "print(\"Bits ${roundTrip}\") "
      "return .success }\n"
      "entry(run)\n";
  static multidoc_fixture fixture;
  CHECK(prepare_process_fixture(&fixture, SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  bool saw_from_bits = false;
  bool saw_to_bits = false;
  uint32_t from_bits_value = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    if (program->values[index].kind == W_SEED_HIR0_VALUE_FLOAT_FROM_BITS) {
      saw_from_bits = true;
      from_bits_value = (uint32_t)index;
    }
    saw_to_bits = saw_to_bits ||
                  program->values[index].kind ==
                      W_SEED_HIR0_VALUE_FLOAT_TO_BITS;
  }
  CHECK(saw_from_bits && saw_to_bits &&
        from_bits_value < program->value_count &&
        program->values[from_bits_value].left_value < program->value_count);

  w_seed_hir0_value *source =
      &fixture.hir_values[program->values[from_bits_value].left_value];
  const w_seed_hir0_value saved_source = *source;
  source->binding_index = (uint32_t)program->binding_count;
  reseal_process_hir(&fixture);
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  *source = saved_source;
  reseal_process_hir(&fixture);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  static const char UNOBSERVED_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let bits: u64 = try u64(exactly: args.count) "
      "let value: f64 = f64.fromBits(bits) "
      "let roundTrip: u64 = value.toBits() "
      "return .success }\n"
      "entry(run)\n";
  CHECK(!prepare_process_fixture(&fixture, UNOBSERVED_SOURCE));
  return true;
}

static bool test_native_process_float_rounding_split(void) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let rounded = try i8(rounding: 2.5_f64, mode: .nearestEven) "
      "print(\"Rounded ${rounded}\") "
      "return .success }\n"
      "entry(run)\n";
  static multidoc_fixture fixture;
  static product_storage storage;
  CHECK(prepare_process_fixture(&fixture, SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  const uint32_t target = program->entries[0].target_function;
  const w_seed_hir0_function *handler = &program->functions[target];
  const uint32_t split_block = handler->first_block;
  const w_seed_hir0_terminator *split =
      &program->terminators[program->blocks[split_block].terminator_index];
  const uint32_t normal_block = split->target_block;
  const uint32_t non_finite_block = split->else_block;
  const uint32_t out_of_range_block = split->third_block;
  const w_seed_hir0_block *normal_block_record =
      &program->blocks[normal_block];
  const uint32_t rounded_binding =
      program->instructions[normal_block_record->first_instruction]
          .binding_index;
  CHECK(program->call_count == 1u &&
        normal_block_record->instruction_count == 2u &&
        rounded_binding < program->binding_count);
  const w_seed_hir0_call *print = &program->calls[0];
  const w_seed_hir0_value *message =
      &program->values[program->arguments[print->first_argument].value_index];
  CHECK(message->kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING &&
        message->interpolation_segment_count == 2u);
  const w_seed_hir0_interpolation_segment *rounded_segment =
      &program->interpolation_segments[
          (size_t)message->first_interpolation_segment + 1u];
  CHECK(rounded_segment->kind == W_SEED_HIR0_INTERPOLATION_VALUE &&
        program->values[rounded_segment->value_index].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[rounded_segment->value_index].binding_index ==
            rounded_binding);
  const w_seed_hir0_terminator *normal =
      &program->terminators[program->blocks[normal_block].terminator_index];
  const w_seed_hir0_terminator *non_finite = &program->terminators[
      program->blocks[non_finite_block].terminator_index];
  const w_seed_hir0_terminator *out_of_range = &program->terminators[
      program->blocks[out_of_range_block].terminator_index];
  const w_seed_product_closure0_input input =
      {program, &fixture.hir_result};
  const w_seed_product_closure0_output output = product_output(&storage);
  w_seed_product_closure0_result result = {0};
  CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(result.normal_outcome.kind == W_SEED_PRODUCT_CLOSURE0_OUTCOME_NORMAL &&
        result.normal_outcome.source_terminator_index ==
            program->blocks[split_block].terminator_index &&
        result.normal_outcome.terminator_index ==
            program->blocks[normal_block].terminator_index &&
        result.normal_outcome.successor_block_index == normal_block &&
        result.normal_outcome.successor_argument_index ==
            program->blocks[normal_block].first_block_argument &&
        result.normal_outcome.value_index == normal->value_index &&
        result.normal_outcome.result_type_index == normal->result_type &&
        result.non_finite_outcome.kind ==
            W_SEED_PRODUCT_CLOSURE0_OUTCOME_TYPED_THROW &&
        result.non_finite_outcome.terminator_index ==
            program->blocks[non_finite_block].terminator_index &&
        result.non_finite_outcome.successor_block_index == non_finite_block &&
        result.non_finite_outcome.value_index == non_finite->value_index &&
        result.non_finite_outcome.error_type_index == handler->error_type &&
        result.out_of_range_outcome.kind ==
            W_SEED_PRODUCT_CLOSURE0_OUTCOME_TYPED_THROW &&
        result.out_of_range_outcome.terminator_index ==
            program->blocks[out_of_range_block].terminator_index &&
        result.out_of_range_outcome.successor_block_index ==
            out_of_range_block &&
        result.out_of_range_outcome.value_index == out_of_range->value_index &&
        result.out_of_range_outcome.error_type_index == handler->error_type &&
        memcmp(&result.outcome, &result.out_of_range_outcome,
               sizeof(result.outcome)) == 0 &&
        result.root.cleanup_obligation ==
            W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS_REVERSE_ON_ALL_OUTCOMES &&
        result.root.cleanup_release_parameter_count == 2u &&
        w_seed_product_closure0_verify(&input, &output, &result));

  const w_seed_product_closure0_result saved_result = result;
  result.non_finite_outcome.successor_block_index = out_of_range_block;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;
  result.out_of_range_outcome.value_index = result.normal_outcome.value_index;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;

  w_seed_hir0_interpolation_segment *mutable_rounded_segment =
      &fixture.hir_interpolation_segments[
          (size_t)message->first_interpolation_segment + 1u];
  const w_seed_hir0_interpolation_segment saved_rounded_segment =
      *mutable_rounded_segment;
  mutable_rounded_segment->value_index = split->value_index;
  reseal_process_hir(&fixture);
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  CHECK(w_seed_product_closure0_measure(
            &input, &(w_seed_product_closure0_counts){0},
            &(w_seed_product_closure0_result){0}) ==
        W_SEED_PRODUCT_CLOSURE0_INVALID);
  *mutable_rounded_segment = saved_rounded_segment;
  reseal_process_hir(&fixture);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  w_seed_hir0_binding *binding = &fixture.hir_bindings[0];
  const w_seed_hir0_binding saved_binding = *binding;
  binding->is_mutable = true;
  reseal_process_hir(&fixture);
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  CHECK(w_seed_product_closure0_measure(&input,
                                        &(w_seed_product_closure0_counts){0},
                                        &(w_seed_product_closure0_result){0}) ==
        W_SEED_PRODUCT_CLOSURE0_INVALID);
  *binding = saved_binding;
  reseal_process_hir(&fixture);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  static const char UNOBSERVED_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let rounded = try i8(rounding: 2.5_f64, mode: .nearestEven) "
      "print(\"Rounded 2\") return .success }\n"
      "entry(run)\n";
  CHECK(!prepare_process_fixture(&fixture, UNOBSERVED_SOURCE));
  return true;
}

static bool test_native_process_float_rounding_diamond(void) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let rounded = try i8(rounding: if args.count == 0 { 2.5_f64 } "
      "else { 3.5_f64 }, mode: .nearestEven) "
      "print(\"Rounded ${rounded}\") return .success }\n"
      "entry(run)\n";
  static multidoc_fixture fixture;
  static product_storage storage;
  CHECK(prepare_process_fixture(&fixture, SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  const uint32_t function_index = program->entries[0].target_function;
  const w_seed_hir0_function *function = &program->functions[function_index];
  const uint32_t branch_block = function->first_block;
  const uint32_t split_block = branch_block + 3u;
  const uint32_t normal_block = branch_block + 4u;
  const uint32_t non_finite_block = branch_block + 5u;
  const uint32_t out_of_range_block = branch_block + 6u;
  CHECK(function->block_count == 7u && program->block_count == 7u &&
        program->blocks[branch_block].terminator_index <
            program->terminator_count &&
        program->terminators[program->blocks[branch_block].terminator_index]
                .kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->blocks[split_block].terminator_index <
            program->terminator_count &&
        program->terminators[program->blocks[split_block].terminator_index]
                .kind ==
            W_SEED_HIR0_TERMINATOR_FLOAT_TO_INTEGER_ROUNDING &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  const uint32_t split_terminator =
      program->blocks[split_block].terminator_index;
  const w_seed_product_closure0_input input =
      {program, &fixture.hir_result};
  const w_seed_product_closure0_output output = product_output(&storage);
  w_seed_product_closure0_result result = {0};
  CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(result.normal_outcome.kind == W_SEED_PRODUCT_CLOSURE0_OUTCOME_NORMAL &&
        result.normal_outcome.source_terminator_index == split_terminator &&
        result.normal_outcome.source_terminator_index !=
            program->blocks[branch_block].terminator_index &&
        result.normal_outcome.successor_block_index == normal_block &&
        result.non_finite_outcome.kind ==
            W_SEED_PRODUCT_CLOSURE0_OUTCOME_TYPED_THROW &&
        result.non_finite_outcome.source_terminator_index == split_terminator &&
        result.non_finite_outcome.successor_block_index == non_finite_block &&
        result.out_of_range_outcome.kind ==
            W_SEED_PRODUCT_CLOSURE0_OUTCOME_TYPED_THROW &&
        result.out_of_range_outcome.source_terminator_index == split_terminator &&
        result.out_of_range_outcome.successor_block_index == out_of_range_block &&
        result.root.cleanup_obligation ==
            W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS_REVERSE_ON_ALL_OUTCOMES &&
        result.root.cleanup_release_parameter_count == 2u &&
        w_seed_product_closure0_verify(&input, &output, &result));

  const w_seed_product_closure0_result saved_result = result;
  result.normal_outcome.source_terminator_index =
      program->blocks[branch_block].terminator_index;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result = saved_result;

  w_seed_hir0_terminator *mutable_branch =
      &fixture.hir_terminators[program->blocks[branch_block].terminator_index];
  const w_seed_hir0_terminator saved_branch = *mutable_branch;
  mutable_branch->else_block = branch_block + 1u;
  reseal_process_hir(&fixture);
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  CHECK(w_seed_product_closure0_measure(
            &input, &(w_seed_product_closure0_counts){0},
            &(w_seed_product_closure0_result){0}) ==
        W_SEED_PRODUCT_CLOSURE0_INVALID);
  *mutable_branch = saved_branch;
  reseal_process_hir(&fixture);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_direct_float_rounding_product(void) {
  static const char *const source_types[] = {"f32", "f64"};
  static const char *const destination_types[] = {"i8", "u64"};
  static const char *const mode_names[] = {
      "nearestEven", "nearestAwayFromZero", "towardZero", "towardPositive",
      "towardNegative"};
  static const w_seed_hir0_rounding_mode modes[] = {
      W_SEED_HIR0_ROUNDING_MODE_NEAREST_EVEN,
      W_SEED_HIR0_ROUNDING_MODE_NEAREST_AWAY_FROM_ZERO,
      W_SEED_HIR0_ROUNDING_MODE_TOWARD_ZERO,
      W_SEED_HIR0_ROUNDING_MODE_TOWARD_POSITIVE,
      W_SEED_HIR0_ROUNDING_MODE_TOWARD_NEGATIVE};
  static multidoc_fixture fixture;
  static multidoc_fixture shifted_fixture;
  static product_storage storage;
  static product_storage shifted_storage;

  for (size_t source = 0u; source < 2u; source += 1u) {
    for (size_t destination = 0u; destination < 2u; destination += 1u) {
      for (size_t mode = 0u; mode < 5u; mode += 1u) {
        char source_text[1024];
        const int source_length = snprintf(
            source_text, sizeof(source_text),
            "fn convert(value: %s): %s throws NumericConversionError { "
            "return try %s(rounding: value, mode: .%s) }\n"
            "entry(convert)\n",
            source_types[source], destination_types[destination],
            destination_types[destination], mode_names[mode]);
        CHECK(source_length > 0 && (size_t)source_length < sizeof(source_text));
        CHECK(prepare_direct_fixture(&fixture, source_text));
        const w_seed_hir0_program *program = &fixture.hir_program;
        const uint32_t target = program->entries[0].target_function;
        const w_seed_hir0_function *handler = &program->functions[target];
        const uint32_t split_block = handler->first_block;
        const w_seed_hir0_terminator *split =
            &program->terminators[program->blocks[split_block].terminator_index];
        const uint32_t normal_block = split->target_block;
        const uint32_t non_finite_block = split->else_block;
        const uint32_t out_of_range_block = split->third_block;
        const w_seed_hir0_terminator *normal =
            &program->terminators[program->blocks[normal_block].terminator_index];
        const w_seed_hir0_terminator *non_finite = &program->terminators[
            program->blocks[non_finite_block].terminator_index];
        const w_seed_hir0_terminator *out_of_range = &program->terminators[
            program->blocks[out_of_range_block].terminator_index];
        CHECK(handler->block_count == 4u &&
              split->kind == W_SEED_HIR0_TERMINATOR_FLOAT_TO_INTEGER_ROUNDING &&
              split->rounding_mode == modes[mode] &&
              split->target_block == normal_block &&
              split->else_block == non_finite_block &&
              split->third_block == out_of_range_block &&
              non_finite->kind == W_SEED_HIR0_TERMINATOR_THROW &&
              out_of_range->kind == W_SEED_HIR0_TERMINATOR_THROW);

        const w_seed_product_closure0_input input =
            {program, &fixture.hir_result};
        (void)memset(&storage, 0xa5, sizeof(storage));
        const w_seed_product_closure0_output output = product_output(&storage);
        w_seed_product_closure0_result result = {0};
        CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
              W_SEED_PRODUCT_CLOSURE0_OK);
        CHECK(result.normal_outcome.kind ==
                  W_SEED_PRODUCT_CLOSURE0_OUTCOME_NORMAL &&
              result.normal_outcome.source_terminator_index ==
                  program->blocks[split_block].terminator_index &&
              result.normal_outcome.terminator_index ==
                  program->blocks[normal_block].terminator_index &&
              result.normal_outcome.successor_block_index == normal_block &&
              result.normal_outcome.successor_argument_index ==
                  program->blocks[normal_block].first_block_argument &&
              result.normal_outcome.successor_argument_count == 1u &&
              result.normal_outcome.value_index == normal->value_index &&
              result.normal_outcome.result_type_index == normal->result_type &&
              result.normal_outcome.error_type_index ==
                  W_SEED_PRODUCT_CLOSURE0_NONE);
        CHECK(result.non_finite_outcome.kind ==
                  W_SEED_PRODUCT_CLOSURE0_OUTCOME_TYPED_THROW &&
              result.non_finite_outcome.source_terminator_index ==
                  program->blocks[split_block].terminator_index &&
              result.non_finite_outcome.terminator_index ==
                  program->blocks[non_finite_block].terminator_index &&
              result.non_finite_outcome.successor_block_index ==
                  non_finite_block &&
              result.non_finite_outcome.successor_argument_index ==
                  program->blocks[non_finite_block].first_block_argument &&
              result.non_finite_outcome.successor_argument_count == 1u &&
              result.non_finite_outcome.value_index == non_finite->value_index &&
              result.non_finite_outcome.result_type_index ==
                  non_finite->result_type &&
              result.non_finite_outcome.error_type_index == handler->error_type &&
              result.non_finite_outcome.error_enum_index ==
                  W_SEED_PRODUCT_CLOSURE0_NONE &&
              result.non_finite_outcome.error_case_index ==
                  W_SEED_PRODUCT_CLOSURE0_NONE);
        CHECK(result.out_of_range_outcome.kind ==
                  W_SEED_PRODUCT_CLOSURE0_OUTCOME_TYPED_THROW &&
              result.out_of_range_outcome.source_terminator_index ==
                  program->blocks[split_block].terminator_index &&
              result.out_of_range_outcome.terminator_index ==
                  program->blocks[out_of_range_block].terminator_index &&
              result.out_of_range_outcome.successor_block_index ==
                  out_of_range_block &&
              result.out_of_range_outcome.successor_argument_index ==
                  program->blocks[out_of_range_block].first_block_argument &&
              result.out_of_range_outcome.successor_argument_count == 1u &&
              result.out_of_range_outcome.value_index == out_of_range->value_index &&
              result.out_of_range_outcome.result_type_index ==
                  out_of_range->result_type &&
              result.out_of_range_outcome.error_type_index == handler->error_type &&
              result.outcome.kind ==
                  W_SEED_PRODUCT_CLOSURE0_OUTCOME_TYPED_THROW &&
              memcmp(&result.outcome, &result.out_of_range_outcome,
                     sizeof(result.outcome)) == 0);
        CHECK(w_seed_product_closure0_verify(&input, &output, &result));

        if (source == 0u && destination == 0u && mode == 0u) {
          const product_storage saved_storage = storage;
          const w_seed_product_closure0_result saved_result = result;
          w_seed_product_closure0_output limited_output = product_output(&storage);
          limited_output.reachable_value_capacity = 0u;
          CHECK(w_seed_product_closure0_run(&input, &limited_output, &result) ==
                W_SEED_PRODUCT_CLOSURE0_CAPACITY);
          CHECK(memcmp(&storage, &saved_storage, sizeof(storage)) == 0 &&
                memcmp(&result, &saved_result, sizeof(result)) == 0);

          result.non_finite_outcome.successor_block_index = out_of_range_block;
          CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
          result = saved_result;
          result.out_of_range_outcome.value_index =
              result.normal_outcome.value_index;
          CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
          result = saved_result;
          result.outcome.successor_block_index = non_finite_block;
          CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
          result = saved_result;
          result.reachable_semantic_digest[0] ^= 1u;
          CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
          result = saved_result;

          w_seed_hir0_terminator *mutable_split =
              &fixture.hir_terminators[program->blocks[split_block]
                                           .terminator_index];
          const w_seed_hir0_terminator saved_split = *mutable_split;
          mutable_split->rounding_mode =
              W_SEED_HIR0_ROUNDING_MODE_TOWARD_NEGATIVE;
          reseal_process_hir(&fixture);
          CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
          CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
          *mutable_split = saved_split;
          reseal_process_hir(&fixture);
          CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
          CHECK(w_seed_product_closure0_verify(&input, &output, &result));

          CHECK(prepare_direct_fixture(&shifted_fixture, source_text));
          CHECK(prepend_dead_numeric_function(&shifted_fixture));
          const w_seed_product_closure0_input shifted_input =
              {&shifted_fixture.hir_program, &shifted_fixture.hir_result};
          const w_seed_product_closure0_output shifted_output =
              product_output(&shifted_storage);
          w_seed_product_closure0_result shifted_result = {0};
          CHECK(w_seed_product_closure0_run(&shifted_input, &shifted_output,
                                            &shifted_result) ==
                W_SEED_PRODUCT_CLOSURE0_OK);
          CHECK(shifted_result.normal_outcome.source_terminator_index !=
                    result.normal_outcome.source_terminator_index &&
                shifted_result.non_finite_outcome.terminator_index !=
                    result.non_finite_outcome.terminator_index &&
                shifted_result.out_of_range_outcome.terminator_index !=
                    result.out_of_range_outcome.terminator_index &&
                memcmp(shifted_result.reachable_semantic_digest,
                       result.reachable_semantic_digest,
                       W_SEED_PRODUCT_CLOSURE0_DIGEST_BYTES) == 0 &&
                w_seed_product_closure0_verify(&shifted_input, &shifted_output,
                                               &shifted_result));
        }
      }
    }
  }
  return true;
}

static bool expect_process_product_status(
    const char *source, w_seed_product_closure0_status expected_status) {
  static multidoc_fixture fixture;
  CHECK(prepare_process_fixture(&fixture, source));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  const w_seed_product_closure0_input input =
      {&fixture.hir_program, &fixture.hir_result};
  w_seed_product_closure0_counts counts;
  w_seed_product_closure0_result result;
  (void)memset(&counts, 0xa1, sizeof(counts));
  (void)memset(&result, 0xb2, sizeof(result));
  const w_seed_product_closure0_counts counts_before = counts;
  const w_seed_product_closure0_result result_before = result;
  const w_seed_product_closure0_status status =
      w_seed_product_closure0_measure(&input, &counts, &result);
  CHECK(status == expected_status);
  if (status != W_SEED_PRODUCT_CLOSURE0_OK)
    CHECK(memcmp(&counts, &counts_before, sizeof(counts)) == 0 &&
          memcmp(&result, &result_before, sizeof(result)) == 0);
  return true;
}

static bool expect_unit_product_status(
    const char *root_source, const char *library_source,
    w_seed_product_closure0_status expected_status) {
  static multidoc_fixture fixture;
  CHECK(prepare_fixture(&fixture, root_source, library_source));
  CHECK(lower_fixture(&fixture));
  const w_seed_product_closure0_input input =
      {&fixture.hir_program, &fixture.hir_result};
  w_seed_product_closure0_counts counts;
  w_seed_product_closure0_result result;
  (void)memset(&counts, 0xa1, sizeof(counts));
  (void)memset(&result, 0xb2, sizeof(result));
  const w_seed_product_closure0_counts counts_before = counts;
  const w_seed_product_closure0_result result_before = result;
  const w_seed_product_closure0_status status =
      w_seed_product_closure0_measure(&input, &counts, &result);
  CHECK(status == expected_status);
  if (status != W_SEED_PRODUCT_CLOSURE0_OK)
    CHECK(memcmp(&counts, &counts_before, sizeof(counts)) == 0 &&
          memcmp(&result, &result_before, sizeof(result)) == 0);
  return true;
}

static bool test_typed_process_fail_closed_shapes(void) {
  static const char BRANCHED_ROOT[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.isEmpty { return .success } "
      "else { return .success } }\n"
      "entry(run)\n";
  static const char PAYLOAD_ENUM[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "enum ProcessFailure: Error { denied unavailable }\n"
      "enum PayloadFailure: Error { unavailable(code: i64) }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws ProcessFailure { throw .denied }\n"
      "entry(run)\n";
  static const char INTEGER_EXACTLY_UNIT_HELPER[] =
      "import { helper as h } from lib\n"
      "fn convert(value: i16): i8 throws NumericConversionError { "
      "return try i8(exactly: value) }\n"
      "fn run() { let ignored = h() }\n"
      "entry(run)\n";
  static const char FLOAT_ROUNDING_PROCESS_HELPER[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn convert(value: f32): i8 throws NumericConversionError { "
      "return try i8(rounding: value, mode: .nearestEven) }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  CHECK(expect_process_product_status(
      BRANCHED_ROOT, W_SEED_PRODUCT_CLOSURE0_UNSUPPORTED));
  CHECK(expect_process_product_status(
      PAYLOAD_ENUM, W_SEED_PRODUCT_CLOSURE0_UNSUPPORTED));
  CHECK(expect_unit_product_status(INTEGER_EXACTLY_UNIT_HELPER, LIB_SOURCE,
                                   W_SEED_PRODUCT_CLOSURE0_UNSUPPORTED));
  CHECK(expect_process_product_status(FLOAT_ROUNDING_PROCESS_HELPER,
                                      W_SEED_PRODUCT_CLOSURE0_UNSUPPORTED));
  return true;
}

static bool test_digest_and_measure(void) {
  static multidoc_fixture canonical;
  static multidoc_fixture repeated;
  CHECK(prepare_fixture(&canonical, ROOT_SOURCE, LIB_SOURCE));
  CHECK(lower_fixture(&canonical));
  CHECK(prepare_fixture(&repeated, ROOT_SOURCE, LIB_SOURCE));
  CHECK(lower_fixture(&repeated));
  product_storage first_storage;
  product_storage second_storage;
  w_seed_product_closure0_output first_output = product_output(&first_storage);
  w_seed_product_closure0_output second_output = product_output(&second_storage);
  const w_seed_product_closure0_input first_input =
      {&canonical.hir_program, &canonical.hir_result};
  const w_seed_product_closure0_input second_input =
      {&repeated.hir_program, &repeated.hir_result};
  w_seed_product_closure0_result first_result = {0};
  w_seed_product_closure0_result second_result = {0};
  CHECK(w_seed_product_closure0_run(&first_input, &first_output, &first_result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(w_seed_product_closure0_run(&second_input, &second_output,
                                    &second_result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(memcmp(first_result.reachable_semantic_digest,
               second_result.reachable_semantic_digest,
               W_SEED_PRODUCT_CLOSURE0_DIGEST_BYTES) == 0);
  w_seed_product_closure0_counts counts = {0};
  w_seed_product_closure0_result measured = {0};
  CHECK(w_seed_product_closure0_measure(&first_input, &counts, &measured) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(memcmp(&counts, &first_result.required, sizeof(counts)) == 0 &&
        memcmp(&measured, &first_result, sizeof(measured)) == 0);

  /* Measurement is also a borrowed, transactional boundary: its output
   * structs may not alias any HIR storage or one another. */
  w_seed_product_closure0_counts saved_counts = counts;
  CHECK(w_seed_product_closure0_measure(
            &first_input,
            (w_seed_product_closure0_counts *)(void *)&canonical.hir_program,
            &measured) == W_SEED_PRODUCT_CLOSURE0_INVALID);
  CHECK(memcmp(&counts, &saved_counts, sizeof(counts)) == 0);

  /* A forged HIR count must be rejected by the HIR verifier before the
   * measure alias table consults any of the program's storage capacities. */
  const size_t saved_function_count = canonical.hir_program.function_count;
  const w_seed_product_closure0_result saved_measured = measured;
  canonical.hir_program.function_count = SIZE_MAX;
  CHECK(w_seed_product_closure0_measure(&first_input, &counts, &measured) ==
        W_SEED_PRODUCT_CLOSURE0_INVALID);
  CHECK(memcmp(&counts, &saved_counts, sizeof(counts)) == 0 &&
        memcmp(&measured, &saved_measured, sizeof(measured)) == 0);
  canonical.hir_program.function_count = saved_function_count;
  return true;
}

static bool test_transaction_barriers(void) {
  static multidoc_fixture fixture;
  CHECK(prepare_fixture(&fixture, ROOT_SOURCE, LIB_SOURCE));
  CHECK(lower_fixture(&fixture));
  const w_seed_product_closure0_input input =
      {&fixture.hir_program, &fixture.hir_result};
  product_storage storage;
  w_seed_product_closure0_output output = product_output(&storage);
  w_seed_product_closure0_result result = {0};
  CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  const w_seed_product_closure0_value_fact saved_value_fact =
      storage.value_facts[0];
  storage.value_facts[0].closure_index += 1u;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  storage.value_facts[0] = saved_value_fact;
  const w_seed_product_closure0_failure saved_failure = result.failure;
  result.failure = W_SEED_PRODUCT_CLOSURE0_FAILURE_HIR;
  CHECK(!w_seed_product_closure0_verify(&input, &output, &result));
  result.failure = saved_failure;
  const w_seed_product_closure0_result saved_result = result;
  uint8_t saved_byte = 0u;
  (void)memcpy(&saved_byte, &storage, sizeof(saved_byte));

  output.reachable_function_capacity = 1u;
  CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_CAPACITY);
  uint8_t current_byte = 0u;
  (void)memcpy(&current_byte, &storage, sizeof(current_byte));
  CHECK(memcmp(&result, &saved_result, sizeof(result)) == 0 &&
        current_byte == saved_byte);
  output = product_output(&storage);
  output.reachable_modules = (uint32_t *)(void *)fixture.hir_modules;
  result = saved_result;
  CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_INVALID);
  CHECK(memcmp(&result, &saved_result, sizeof(result)) == 0);

  /* A malformed dead function is rejected by full-HIR verification before
   * ProductClosure0 can omit it. */
  const w_seed_hir0_function saved_function = fixture.hir_functions[1];
  fixture.hir_functions[1].module_index = 0u;
  result = saved_result;
  output = product_output(&storage);
  CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_INVALID);
  CHECK(memcmp(&result, &saved_result, sizeof(result)) == 0);
  fixture.hir_functions[1] = saved_function;

  const w_seed_hir0_entry saved_entry = fixture.hir_entries[0];
  fixture.hir_entries[0].target_function = 1u;
  result = saved_result;
  CHECK(w_seed_product_closure0_run(&input, &output, &result) ==
        W_SEED_PRODUCT_CLOSURE0_INVALID);
  fixture.hir_entries[0] = saved_entry;
  return true;
}

static bool test_dead_module_and_mlir(void) {
  static const char print_root[] =
      "import { helper as h } from lib\n"
      "fn run() { let value = h() print(\"answer ${value}\") }\n"
      "entry(run)\n";
  static const char print_library[] =
      "module lib\n"
      "export fn helper(): i64 { return 42 }\n";
  static multidoc_fixture base;
  static multidoc_fixture dead;
  CHECK(prepare_print_fixture(&base, print_root, print_library));
  CHECK(lower_fixture(&base));
  CHECK(prepare_print_fixture(&dead, print_root, print_library));
  CHECK(lower_fixture(&dead));
  CHECK(add_dead_module(&dead));

  product_storage base_storage;
  product_storage dead_storage;
  (void)memset(&base_storage, 0, sizeof(base_storage));
  (void)memset(&dead_storage, 0, sizeof(dead_storage));
  const w_seed_product_closure0_input base_input =
      {&base.hir_program, &base.hir_result};
  const w_seed_product_closure0_input dead_input =
      {&dead.hir_program, &dead.hir_result};
  const w_seed_product_closure0_output base_output =
      product_output(&base_storage);
  const w_seed_product_closure0_output dead_output =
      product_output(&dead_storage);
  w_seed_product_closure0_result base_result = {0};
  w_seed_product_closure0_result dead_result = {0};
  CHECK(w_seed_product_closure0_run(&base_input, &base_output, &base_result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(w_seed_product_closure0_run(&dead_input, &dead_output, &dead_result) ==
        W_SEED_PRODUCT_CLOSURE0_OK);
  CHECK(base_result.root.adapter_kind ==
            W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT &&
        base_result.outcome.kind == W_SEED_PRODUCT_CLOSURE0_OUTCOME_NONE &&
        base_result.outcome.terminator_index == W_SEED_PRODUCT_CLOSURE0_NONE &&
        base_result.outcome.value_index == W_SEED_PRODUCT_CLOSURE0_NONE &&
        base_result.outcome.error_type_index == W_SEED_PRODUCT_CLOSURE0_NONE &&
        base_result.root.cleanup_obligation ==
            W_SEED_HIR0_ENTRY_CLEANUP_NONE);
  CHECK(dead_result.written.reachable_modules == 2u &&
        dead_result.written.omitted_modules == 1u &&
        dead_storage.reachable_modules[0] == 0u &&
        dead_storage.reachable_modules[1] == 1u &&
        dead_storage.omitted_modules[0] == 2u &&
        dead_storage.module_remap[2] == W_SEED_PRODUCT_CLOSURE0_NONE);
  CHECK(memcmp(base_result.reachable_semantic_digest,
               dead_result.reachable_semantic_digest,
               W_SEED_PRODUCT_CLOSURE0_DIGEST_BYTES) == 0);
  CHECK(w_seed_product_closure0_verify(&dead_input, &dead_output,
                                       &dead_result));

  static const w_seed_mlir0_target target = {
      W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU};
  const w_seed_mlir0_input base_mlir_input = {
      &base.hir_program, &base.hir_result, W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  const w_seed_mlir0_input dead_mlir_input = {
      &dead.hir_program, &dead.hir_result, W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  w_seed_mlir0_counts base_counts = {0};
  w_seed_mlir0_counts dead_counts = {0};
  w_seed_mlir0_result base_mlir_result = {0};
  w_seed_mlir0_result dead_mlir_result = {0};
  static uint8_t base_bytes[W_SEED_MLIR0_MAX_BYTES];
  static uint8_t dead_bytes[W_SEED_MLIR0_MAX_BYTES];
  CHECK(w_seed_mlir0_measure(&base_mlir_input, &target, &base_counts,
                             &base_mlir_result) == W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_measure(&dead_mlir_input, &target, &dead_counts,
                             &dead_mlir_result) == W_SEED_MLIR0_OK);
  CHECK(base_counts.mlir_bytes == dead_counts.mlir_bytes);
  CHECK(w_seed_mlir0_emit(
            &base_mlir_input, &target,
            &(w_seed_mlir0_output){base_bytes, sizeof(base_bytes)},
            &base_mlir_result) == W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &dead_mlir_input, &target,
            &(w_seed_mlir0_output){dead_bytes, sizeof(dead_bytes)},
            &dead_mlir_result) == W_SEED_MLIR0_OK);
  CHECK(memcmp(base_bytes, dead_bytes, base_counts.mlir_bytes) == 0);
  return true;
}

int main(void) {
  if (!test_reachable_and_omitted()) return 1;
  if (!test_digest_and_measure()) return 1;
  if (!test_transaction_barriers()) return 1;
  if (!test_native_process_typed_throw()) return 1;
  if (!test_native_process_numeric_split()) return 1;
  if (!test_native_process_checked_local_helper()) return 1;
  if (!test_native_process_checked_scalar_if_join()) return 1;
  if (!test_native_process_float_raw_bits_from_runtime_count()) return 1;
  if (!test_native_process_float_rounding_split()) return 1;
  if (!test_native_process_float_rounding_diamond()) return 1;
  if (!test_direct_float_rounding_product()) return 1;
  if (!test_typed_process_fail_closed_shapes()) return 1;
  return test_dead_module_and_mlir() ? 0 : 1;
}
