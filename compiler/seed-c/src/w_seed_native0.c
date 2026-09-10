#include "w_seed_native0.h"
#include "w_seed_native_subset0.h"

#include <stdio.h>
#include <string.h>

_Static_assert(W_SEED_NATIVE_SUBSET0_MAX_BLOCKS >= W_SEED_NATIVE0_HIR_BLOCKS,
               "Native subset block cache covers Native0 HIR blocks");

static bool range_end(uintptr_t start, size_t length, uintptr_t *end) {
  if (end == NULL || length > UINTPTR_MAX - start) return false;
  *end = start + (uintptr_t)length;
  return true;
}

static bool ranges_overlap(const void *left, size_t left_length,
                           const void *right, size_t right_length) {
  if (left == NULL || right == NULL || left_length == 0u ||
      right_length == 0u)
    return false;
  const uintptr_t left_start = (uintptr_t)left;
  const uintptr_t right_start = (uintptr_t)right;
  uintptr_t left_end = 0u;
  uintptr_t right_end = 0u;
  if (!range_end(left_start, left_length, &left_end) ||
      !range_end(right_start, right_length, &right_end))
    return true;
  return left_start < right_end && right_start < left_end;
}

static bool input_shape_valid(const w_seed_native0_input *input) {
  return input != NULL && input->path != NULL && input->path_length != 0u &&
         input->path_length <= W_SEED_NATIVE0_MAX_PATH_BYTES &&
         input->path[input->path_length] == '\0' &&
         input->logical_source_id.data != NULL &&
         input->logical_source_id.length != 0u &&
         input->logical_source_id.length <=
             W_SEED_NATIVE0_MAX_SOURCE_ID_BYTES;
}

static bool source_span_is(const w_seed_source *source, w_seed_span span,
                           const uint8_t *literal, size_t literal_bytes) {
  if (source == NULL || literal == NULL) return false;
  w_seed_byte_view slice;
  w_seed_source_error error;
  if (!w_seed_source_slice(source, span, &slice, &error) ||
      slice.length != literal_bytes)
    return false;
  return literal_bytes == 0u || memcmp(slice.data, literal, literal_bytes) == 0;
}

static void configure_process_catalog(w_seed_native0_storage *storage,
                                      bool public_process) {
  if (storage == NULL) return;
  static const w_seed_frontend_text empty = {NULL, 0u};
  storage->process_external_symbols[0] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"Arguments", 9u},
      .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"Arguments", 9u},
      .is_const = false,
      .receiver_type = empty};
  storage->process_external_symbols[1] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"Context", 7u},
      .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"Context", 7u},
      .is_const = false,
      .receiver_type = empty};
  storage->process_external_symbols[2] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"ExitCode", 8u},
      .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"ExitCode", 8u},
      .is_const = false,
      .receiver_type = empty};
  storage->process_external_symbols[3] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"success", 7u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"ExitCode", 8u},
      .is_const = true,
      .receiver_type = (w_seed_frontend_text){"ExitCode", 8u}};
  if (public_process) {
    storage->process_external_symbols[4] =
        (w_seed_frontend_external_symbol){
            .name = (w_seed_frontend_text){"isEmpty", 7u},
            .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
            .exported = true,
            .parameters = NULL,
            .parameter_count = 0u,
            .return_type = (w_seed_frontend_text){"Bool", 4u},
            .is_const = true,
            .receiver_type = (w_seed_frontend_text){"Arguments", 9u}};
    storage->process_external_parameters[0] =
        (w_seed_frontend_external_parameter){
            .name = (w_seed_frontend_text){"code", 4u},
            .type = (w_seed_frontend_text){"i64", 3u},
            .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
    storage->process_external_symbols[5] =
        (w_seed_frontend_external_symbol){
            .name = (w_seed_frontend_text){"failure", 7u},
            .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
            .exported = true,
            .parameters = storage->process_external_parameters,
            .parameter_count = 1u,
            .return_type = (w_seed_frontend_text){"ExitCode", 8u},
            .is_const = true,
            .receiver_type = (w_seed_frontend_text){"ExitCode", 8u}};
  }
  storage->process_external_modules[0] = (w_seed_frontend_external_module){
      .module_id = (w_seed_frontend_text){"std.process", 11u},
      .symbols = storage->process_external_symbols,
      .symbol_count = public_process ? 6u : 4u};
}

/* Resolve the process catalog from parser-owned import records rather than
 * source substring or function-name recognition. A non-empty import graph
 * without this one exact supported edge fails closed in Native0. */
static w_seed_native0_status configure_external_catalog(
    w_seed_native0_storage *storage, bool public_process,
    bool *has_process_import) {
  if (storage == NULL || has_process_import == NULL)
    return W_SEED_NATIVE0_INVALID;
  *has_process_import = false;
  w_seed_module_origin origins[W_SEED_NATIVE0_IMPORTS];
  w_seed_module_scan_result scan_result;
  const w_seed_module_scan_status scan_status = w_seed_module_scan(
      &storage->source, storage->nodes, storage->parse.node_count,
      &storage->parse, origins, W_SEED_NATIVE0_IMPORTS, &scan_result);
  if (scan_status == W_SEED_MODULE_SCAN_CAPACITY)
    return W_SEED_NATIVE0_CAPACITY;
  if (scan_status != W_SEED_MODULE_SCAN_OK) return W_SEED_NATIVE0_UNSUPPORTED;
  if (scan_result.written == 0u) return W_SEED_NATIVE0_OK;
  if (scan_result.written != 1u) return W_SEED_NATIVE0_UNSUPPORTED;

  w_seed_span path_span;
  if (!w_seed_module_scan_import_path_span(
          &storage->source, storage->nodes, storage->parse.node_count,
          origins[0].declaration_span, &path_span) ||
      !source_span_is(&storage->source, path_span,
                      (const uint8_t *)"std.process", 11u))
    return W_SEED_NATIVE0_UNSUPPORTED;

  configure_process_catalog(storage, public_process);
  storage->process_resolved_imports[0] =
      (w_seed_frontend_resolved_import){
          .source_document_index = 0u,
          .direct_import_ordinal = origins[0].direct_import_ordinal,
          .import_declaration_span = origins[0].declaration_span,
          .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_EXTERNAL_MODULE,
          .target_index = 0u};
  *has_process_import = true;
  return W_SEED_NATIVE0_OK;
}

/* Reject every output/result alias that could be invalidated by the storage
 * reset or by the final publication. Input views into storage are rejected as
 * well because the storage is cleared before acquisition. */
static bool input_aliases_storage_or_outputs(
    const w_seed_native0_input *input, const w_seed_native0_storage *storage,
    const w_seed_native0_output *output,
    const w_seed_native0_result *result) {
  if (input == NULL || storage == NULL || output == NULL || result == NULL)
    return true;
  const void *storage_address = storage;
  const size_t storage_size = sizeof(*storage);
  if (ranges_overlap(input, sizeof(*input), storage_address, storage_size) ||
      ranges_overlap(input, sizeof(*input), output, sizeof(*output)) ||
      ranges_overlap(input, sizeof(*input), result, sizeof(*result)) ||
      ranges_overlap(result, sizeof(*result), storage_address, storage_size) ||
      ranges_overlap(output, sizeof(*output), storage_address, storage_size) ||
      ranges_overlap(result, sizeof(*result), output, sizeof(*output)) ||
      ranges_overlap(result, sizeof(*result), output->bytes,
                     output->capacity) ||
      ranges_overlap(output, sizeof(*output), output->bytes,
                     output->capacity) ||
      ranges_overlap(input->path, input->path_length + 1u, storage_address,
                     storage_size) ||
      ranges_overlap(input->path, input->path_length + 1u, result,
                     sizeof(*result)) ||
      ranges_overlap(input->path, input->path_length + 1u, output,
                     sizeof(*output)) ||
      ranges_overlap(input->logical_source_id.data,
                     input->logical_source_id.length, storage_address,
                     storage_size) ||
      ranges_overlap(input->logical_source_id.data,
                     input->logical_source_id.length, result, sizeof(*result)) ||
      ranges_overlap(input->logical_source_id.data,
                     input->logical_source_id.length, output,
                     sizeof(*output)) ||
      ranges_overlap(output->bytes, output->capacity, storage_address,
                     storage_size) ||
      ranges_overlap(output->bytes, output->capacity, input, sizeof(*input)) ||
      ranges_overlap(output->bytes, output->capacity, input->path,
                     input->path_length + 1u) ||
      ranges_overlap(output->bytes, output->capacity,
                     input->logical_source_id.data,
                     input->logical_source_id.length))
    return true;
  return false;
}

static bool read_source(const w_seed_native0_input *input,
                        w_seed_native0_storage *storage) {
  if (input == NULL || storage == NULL) return false;
  FILE *file = fopen(input->path, "rb");
  if (file == NULL) return false;
  const size_t read = fread(storage->source_bytes, sizeof(uint8_t),
                            sizeof(storage->source_bytes), file);
  const int read_error = ferror(file);
  const int close_result = fclose(file);
  if (read_error != 0 || close_result != 0 || read == 0u ||
      read > W_SEED_NATIVE0_MAX_SOURCE_BYTES)
    return false;
  storage->source_length = read;
  return true;
}

static w_seed_native0_status prepare_frontend(
    const w_seed_native0_input *input, w_seed_native0_storage *storage) {
  if (input == NULL || storage == NULL) return W_SEED_NATIVE0_INVALID;
  w_seed_source_error source_error;
  if (!w_seed_source_init(
          (w_seed_byte_view){storage->source_bytes, storage->source_length},
          &storage->source, &source_error))
    return W_SEED_NATIVE0_SOURCE;

  w_seed_lex_error lex_error;
  if (!w_seed_parser_init(
          &storage->source, (w_seed_span){0u, storage->source_length},
          (w_seed_foreign_limits){65536u, 256u}, storage->lexer_frames,
          W_SEED_NATIVE0_LEXER_FRAMES, storage->tokens,
          W_SEED_NATIVE0_TOKENS, storage->nodes, W_SEED_NATIVE0_NODES,
          storage->parse_frames, W_SEED_NATIVE0_PARSE_FRAMES, storage->issues,
          W_SEED_NATIVE0_ISSUES, &storage->parser, &lex_error) ||
      !w_seed_parser_parse(&storage->parser, &storage->parse) ||
      storage->parse.status != W_SEED_PARSE_COMPLETE)
    return W_SEED_NATIVE0_PARSE;

  storage->document = (w_seed_frontend_document){
      .logical_source_id = input->logical_source_id,
      .module_id = input->logical_source_id,
      .local_module_name = input->logical_source_id,
      .source = &storage->source,
      .nodes = storage->nodes,
      .node_count = storage->parse.node_count,
      .parse = storage->parse};
  bool has_process_import = false;
  const w_seed_native0_status catalog_status =
      configure_external_catalog(
          storage, input->artifact_kind != W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER,
          &has_process_import);
  if (catalog_status != W_SEED_NATIVE0_OK) return catalog_status;
  storage->input = (w_seed_frontend_input){
      .documents = &storage->document,
      .document_count = 1u,
      .external_modules =
          has_process_import ? storage->process_external_modules : NULL,
      .external_module_count = has_process_import ? 1u : 0u,
      .host_scope = &storage->host_scope,
      .import_resolution_complete = has_process_import,
      .resolved_imports =
          has_process_import ? storage->process_resolved_imports : NULL,
      .resolved_import_count = has_process_import ? 1u : 0u};
  storage->output = (w_seed_frontend_output){
      .modules = storage->modules,
      .module_capacity = W_SEED_NATIVE0_MODULES,
      .imports = storage->imports,
      .import_capacity = W_SEED_NATIVE0_IMPORTS,
      .import_items = storage->import_items,
      .import_item_capacity = W_SEED_NATIVE0_IMPORT_ITEMS,
      .structs = storage->structs,
      .struct_capacity = W_SEED_NATIVE0_STRUCTS,
      .fields = storage->fields,
      .field_capacity = W_SEED_NATIVE0_FIELDS,
      .type_declarations = storage->type_declarations,
      .type_declaration_capacity = W_SEED_NATIVE0_STRUCTS,
      .aliases = storage->aliases,
      .alias_capacity = W_SEED_NATIVE0_STRUCTS,
      .types = storage->types,
      .type_capacity = W_SEED_NATIVE0_TYPES,
      .functions = storage->functions,
      .function_capacity = W_SEED_NATIVE0_FUNCTIONS,
      .parameters = storage->parameters,
      .parameter_capacity = W_SEED_NATIVE0_PARAMETERS,
      .entries = storage->entries,
      .entry_capacity = W_SEED_NATIVE0_ENTRIES,
      .statements = storage->statements,
      .statement_capacity = W_SEED_NATIVE0_STATEMENTS,
      .expressions = storage->expressions,
      .expression_capacity = W_SEED_NATIVE0_EXPRESSIONS,
      .arguments = storage->arguments,
      .argument_capacity = W_SEED_NATIVE0_ARGUMENTS,
      .interpolation_segments = storage->interpolation_segments,
      .interpolation_segment_capacity =
          W_SEED_NATIVE0_INTERPOLATION_SEGMENTS,
      .symbols = storage->symbols,
      .symbol_capacity = W_SEED_NATIVE0_SYMBOLS,
      .facts = storage->facts,
      .fact_capacity = W_SEED_NATIVE0_FACTS,
      .diagnostics = storage->diagnostics,
      .diagnostic_capacity = W_SEED_NATIVE0_DIAGNOSTICS,
      .diagnostic_facts = storage->diagnostic_facts,
      .diagnostic_fact_capacity = W_SEED_NATIVE0_DIAGNOSTICS * 5u,
      .diagnostic_items = storage->diagnostic_items,
      .diagnostic_item_capacity = W_SEED_NATIVE0_DIAGNOSTICS * 4u,
      .diagnostic_labels = storage->diagnostic_labels,
      .diagnostic_label_capacity = W_SEED_NATIVE0_DIAGNOSTICS * 2u,
      .const_bytes = storage->const_bytes,
      .const_bytes_capacity = sizeof(storage->const_bytes),
      .receipt = storage->frontend_receipt,
      .receipt_capacity = sizeof(storage->frontend_receipt)};

  storage->host_requirements[0] = (w_seed_frontend_host_requirement){
      .name = (w_seed_frontend_text){"Console", 7u}};
  storage->host_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  storage->host_symbols[0] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"noop", 4u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = NULL,
      .requirement_count = 0u};
  storage->host_symbols[1] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"print", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = storage->host_parameters,
      .parameter_count = 1u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = storage->host_requirements,
      .requirement_count = 1u};
  storage->host_scope = (w_seed_frontend_host_prelude){
      .profile = (w_seed_frontend_text){"native-process@1", 16u},
      .symbols = storage->host_symbols,
      .symbol_count = 2u};

  const w_seed_frontend_status frontend_status = w_seed_frontend_run(
      &storage->input, &storage->output, &storage->frontend_result);
  if (frontend_status == W_SEED_FRONTEND_OK)
    return W_SEED_NATIVE0_OK;
  if (frontend_status == W_SEED_FRONTEND_UNSUPPORTED)
    return W_SEED_NATIVE0_UNSUPPORTED;
  if (frontend_status == W_SEED_FRONTEND_CAPACITY)
    return W_SEED_NATIVE0_CAPACITY;
  return W_SEED_NATIVE0_FRONTEND;
}

static w_seed_native0_status lower_hir(w_seed_native0_storage *storage) {
  if (storage == NULL) return W_SEED_NATIVE0_INVALID;
  storage->hir_output = (w_seed_hir0_output){
      .modules = storage->hir_modules,
      .module_capacity = W_SEED_NATIVE0_HIR_MODULES,
      .identities = storage->hir_identities,
      .identity_capacity = W_SEED_NATIVE0_HIR_IDENTITIES,
      .types = storage->hir_types,
      .type_capacity = W_SEED_NATIVE0_HIR_TYPES,
      .functions = storage->hir_functions,
      .function_capacity = W_SEED_NATIVE0_HIR_FUNCTIONS,
      .parameters = storage->hir_parameters,
      .parameter_capacity = W_SEED_NATIVE0_HIR_PARAMETERS,
      .blocks = storage->hir_blocks,
      .block_capacity = W_SEED_NATIVE0_HIR_BLOCKS,
      .block_arguments = storage->hir_block_arguments,
      .block_argument_capacity = W_SEED_NATIVE0_HIR_BLOCK_ARGUMENTS,
      .instructions = storage->hir_instructions,
      .instruction_capacity = W_SEED_NATIVE0_HIR_INSTRUCTIONS,
      .bindings = storage->hir_bindings,
      .binding_capacity = W_SEED_NATIVE0_HIR_BINDINGS,
      .calls = storage->hir_calls,
      .call_capacity = W_SEED_NATIVE0_HIR_CALLS,
      .host_parameters = storage->hir_host_parameters,
      .host_parameter_capacity = W_SEED_NATIVE0_HIR_HOST_PARAMETERS,
      .arguments = storage->hir_arguments,
      .argument_capacity = W_SEED_NATIVE0_HIR_ARGUMENTS,
      .requirements = storage->hir_requirements,
      .requirement_capacity = W_SEED_NATIVE0_HIR_REQUIREMENTS,
      .values = storage->hir_values,
      .value_capacity = W_SEED_NATIVE0_HIR_VALUE_RECORDS,
      .interpolation_segments = storage->hir_interpolation_segments,
      .interpolation_segment_capacity = W_SEED_NATIVE0_HIR_INTERPOLATION_SEGMENTS,
      .terminators = storage->hir_terminators,
      .terminator_capacity = W_SEED_NATIVE0_HIR_TERMINATORS,
      .entries = storage->hir_entries,
      .entry_capacity = W_SEED_NATIVE0_HIR_ENTRIES,
      .external_modules = storage->hir_external_modules,
      .external_module_capacity = W_SEED_NATIVE0_HIR_EXTERNAL_MODULES,
      .external_symbols = storage->hir_external_symbols,
      .external_symbol_capacity = W_SEED_NATIVE0_HIR_EXTERNAL_SYMBOLS,
      .text_bytes = storage->hir_text,
      .text_byte_capacity = sizeof(storage->hir_text),
      .value_bytes = storage->hir_value_bytes,
      .value_byte_capacity = sizeof(storage->hir_value_bytes),
      .receipt = storage->hir_receipt,
      .receipt_capacity = sizeof(storage->hir_receipt)};
  const w_seed_hir0_input input = {
      &storage->input, &storage->output, &storage->frontend_result};
  w_seed_hir0_counts counts;
  w_seed_hir0_result measured;
  const w_seed_hir0_status measure_status =
      w_seed_hir0_measure(&input, &counts, &measured);
  if (measure_status == W_SEED_HIR0_UNSUPPORTED)
    return W_SEED_NATIVE0_UNSUPPORTED;
  if (measure_status == W_SEED_HIR0_CAPACITY)
    return W_SEED_NATIVE0_CAPACITY;
  if (measure_status != W_SEED_HIR0_OK) return W_SEED_NATIVE0_HIR;
  const w_seed_hir0_status run_status =
      w_seed_hir0_run(&input, &storage->hir_output, &storage->hir_result);
  if (run_status == W_SEED_HIR0_UNSUPPORTED)
    return W_SEED_NATIVE0_UNSUPPORTED;
  if (run_status == W_SEED_HIR0_CAPACITY)
    return W_SEED_NATIVE0_CAPACITY;
  if (run_status != W_SEED_HIR0_OK ||
      !w_seed_hir0_program_from_output(&storage->hir_output,
                                        &storage->hir_result,
                                        &storage->hir_program) ||
      !w_seed_hir0_verify(&storage->hir_program, &storage->hir_result))
    return W_SEED_NATIVE0_HIR;
  return W_SEED_NATIVE0_OK;
}

static w_seed_native0_status map_mlir_status(w_seed_mlir0_status status) {
  switch (status) {
    case W_SEED_MLIR0_OK:
      return W_SEED_NATIVE0_OK;
    case W_SEED_MLIR0_UNSUPPORTED:
      return W_SEED_NATIVE0_UNSUPPORTED;
    case W_SEED_MLIR0_CAPACITY:
      return W_SEED_NATIVE0_CAPACITY;
    case W_SEED_MLIR0_INVALID_HIR:
    case W_SEED_MLIR0_ALIAS:
      return W_SEED_NATIVE0_MLIR;
  }
  return W_SEED_NATIVE0_MLIR;
}

w_seed_native0_status w_seed_native0_run(
    const w_seed_native0_input *input, w_seed_native0_storage *storage,
    const w_seed_native0_output *output, w_seed_native0_result *result) {
  if (input == NULL || storage == NULL || output == NULL || result == NULL)
    return W_SEED_NATIVE0_INVALID;
  if (!input_shape_valid(input)) return W_SEED_NATIVE0_SOURCE;
  if (input->artifact_kind != W_SEED_MLIR0_ARTIFACT_EXECUTABLE &&
      input->artifact_kind != W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER &&
      input->artifact_kind != W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE)
    return W_SEED_NATIVE0_UNSUPPORTED;
  if (output->capacity > W_SEED_MLIR0_MAX_BYTES ||
      (output->capacity != 0u && output->bytes == NULL))
    return W_SEED_NATIVE0_CAPACITY;
  if (!w_seed_mlir0_target_is_supported(&input->target))
    return W_SEED_NATIVE0_UNSUPPORTED;
  if (input_aliases_storage_or_outputs(input, storage, output, result))
    return W_SEED_NATIVE0_MLIR;

  (void)memset(storage, 0, sizeof(*storage));
  if (!read_source(input, storage)) return W_SEED_NATIVE0_SOURCE;
  w_seed_native0_status status = prepare_frontend(input, storage);
  if (status != W_SEED_NATIVE0_OK) return status;
  status = lower_hir(storage);
  if (status != W_SEED_NATIVE0_OK) return status;

  w_seed_mlir0_artifact_kind artifact_kind = input->artifact_kind;
  if (artifact_kind == W_SEED_MLIR0_ARTIFACT_EXECUTABLE &&
      storage->hir_program.external_module_count == 1u &&
      storage->hir_program.external_symbol_count == 6u)
    artifact_kind = W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE;
  const w_seed_mlir0_input mlir_input = {
      .program = &storage->hir_program,
      .hir_result = &storage->hir_result,
      .artifact_kind = artifact_kind};
  w_seed_mlir0_result mlir_result;
  const w_seed_native0_status mlir_status = map_mlir_status(
      w_seed_mlir0_emit(&mlir_input, &input->target,
                        output, &mlir_result));
  if (mlir_status != W_SEED_NATIVE0_OK) return mlir_status;

  const w_seed_native0_result candidate = {
      W_SEED_NATIVE0_OK, storage->source_length, mlir_result};
  *result = candidate;
  return W_SEED_NATIVE0_OK;
}
