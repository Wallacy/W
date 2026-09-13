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
      .requirement_fact_capacity = PRODUCT_REQUIREMENTS};
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
  return test_reachable_and_omitted() && test_digest_and_measure() &&
                 test_transaction_barriers() && test_dead_module_and_mlir()
             ? 0
             : 1;
}
