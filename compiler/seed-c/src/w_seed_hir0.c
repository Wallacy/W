#include "w_seed_hir0.h"

#include <limits.h>
#include <string.h>

#include "w_seed_sha256.h"

_Static_assert(CHAR_BIT == 8, "w-seed HIR0 requires 8-bit bytes");

enum {
  HIR0_DIGEST_BYTES = 32,
  HIR0_RECEIPT_SCHEMA_BYTES = 16,
  HIR0_RECEIPT_COUNT_FIELDS = 18,
  HIR0_RECEIPT_BYTES = HIR0_RECEIPT_SCHEMA_BYTES +
                       HIR0_RECEIPT_COUNT_FIELDS * 8 + HIR0_DIGEST_BYTES * 2,
};

static const char HIR0_UNIT_NAME[] = "()";
static const char HIR0_STRING_NAME[] = "String";
static const char HIR0_I64_NAME[] = "i64";
static const char HIR0_BOOL_NAME[] = "Bool";
static const char HIR0_SLOT_NAME[] = ".default";

static w_seed_hir0_label_kind hir_label_kind(
    w_seed_frontend_label_kind kind) {
  switch (kind) {
    case W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY:
      return W_SEED_HIR0_LABEL_POSITIONAL_ONLY;
    case W_SEED_FRONTEND_LABEL_REQUIRED:
      return W_SEED_HIR0_LABEL_REQUIRED;
  }
  return W_SEED_HIR0_LABEL_POSITIONAL_ONLY;
}

static bool hir_label_valid(w_seed_hir0_label_kind kind,
                            w_seed_hir0_text label, bool text_is_valid) {
  if (!text_is_valid || kind > W_SEED_HIR0_LABEL_REQUIRED) return false;
  if (kind == W_SEED_HIR0_LABEL_POSITIONAL_ONLY) return label.count == 0u;
  if (kind == W_SEED_HIR0_LABEL_REQUIRED) return label.count != 0u;
  return true;
}

typedef enum {
  HIR0_PREPARE_READY = 0,
  HIR0_PREPARE_FRONTEND,
  HIR0_PREPARE_UNSUPPORTED,
  HIR0_PREPARE_INVALID,
} hir0_prepare_status;

static bool add_size(size_t left, size_t right, size_t *out) {
  if (out == NULL || right > SIZE_MAX - left) return false;
  *out = left + right;
  return true;
}

static bool count_u32(size_t value) { return value <= (size_t)UINT32_MAX; }

static bool range_valid(size_t first, size_t count, size_t total) {
  return first <= total && count <= total - first;
}

static bool text_valid(w_seed_frontend_text text) {
  return text.length == 0u || text.data != NULL;
}

static bool frontend_label_valid(w_seed_frontend_label_kind kind,
                                 w_seed_frontend_text label) {
  if (!text_valid(label) || kind > W_SEED_FRONTEND_LABEL_REQUIRED)
    return false;
  if (kind == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY) return label.length == 0u;
  if (kind == W_SEED_FRONTEND_LABEL_REQUIRED) return label.length != 0u;
  return true;
}

static bool text_equal(w_seed_frontend_text left,
                       w_seed_frontend_text right);

/* HIR0 canonicalizes every required host parameter label to the public
 * external_parameter.name. This closed subset accepts only required and
 * positional-only callable policies. */
static bool frontend_host_label_matches(
    w_seed_frontend_label_kind kind, w_seed_frontend_text name,
    w_seed_frontend_text label) {
  if (!frontend_label_valid(kind, label)) return false;
  if (kind == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY)
    return label.length == 0u;
  return name.length != 0u && text_equal(name, label);
}

static bool frontend_parameter_label_matches(
    const w_seed_frontend_parameter *parameter,
    w_seed_frontend_text argument_label) {
  if (parameter == NULL || !frontend_label_valid(parameter->label_kind,
                                                   parameter->label) ||
      !text_valid(argument_label))
    return false;
  if (parameter->label_kind == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY)
    return argument_label.length == 0u;
  return text_equal(parameter->label, argument_label);
}

static bool text_is(w_seed_frontend_text text, const char *literal) {
  if (literal == NULL) return false;
  const size_t length = strlen(literal);
  return text.length == length &&
         (length == 0u ||
          (text.data != NULL && memcmp(text.data, literal, length) == 0));
}

static bool text_equal(w_seed_frontend_text left,
                       w_seed_frontend_text right) {
  return left.length == right.length &&
         (left.length == 0u ||
          (left.data != NULL && right.data != NULL &&
           memcmp(left.data, right.data, left.length) == 0));
}

static bool hir_text_valid(const w_seed_hir0_program *program,
                           w_seed_hir0_text text);

static bool hir_text_equal(const w_seed_hir0_program *program,
                           w_seed_hir0_text left, w_seed_hir0_text right) {
  if (!hir_text_valid(program, left) || !hir_text_valid(program, right) ||
      left.count != right.count)
    return false;
  if (left.count == 0u) return true;
  return memcmp(program->text_bytes + left.offset,
                program->text_bytes + right.offset, left.count) == 0;
}

static bool hir_text_is(const w_seed_hir0_program *program,
                        w_seed_hir0_text text, const char *literal) {
  if (!hir_text_valid(program, text) || literal == NULL) return false;
  const size_t length = strlen(literal);
  return text.count == length &&
         (length == 0u ||
          memcmp(program->text_bytes + text.offset, literal, length) == 0);
}

static bool slice_valid(uint32_t offset, uint32_t count, size_t total) {
  return (size_t)offset <= total && (size_t)count <= total - (size_t)offset;
}

static bool hir_text_valid(const w_seed_hir0_program *program,
                           w_seed_hir0_text text) {
  return program != NULL && slice_valid(text.offset, text.count,
                                        program->text_byte_count) &&
         (text.count == 0u || program->text_bytes != NULL);
}

static bool hir_host_label_matches(const w_seed_hir0_program *program,
                                   w_seed_hir0_label_kind kind,
                                   w_seed_hir0_text name,
                                   w_seed_hir0_text label) {
  if (!hir_text_valid(program, name) || !hir_text_valid(program, label) ||
      !hir_label_valid(kind, label, true))
    return false;
  if (kind == W_SEED_HIR0_LABEL_POSITIONAL_ONLY)
    return label.count == 0u;
  return name.count != 0u && hir_text_equal(program, name, label);
}

static bool byte_slice_valid(const w_seed_hir0_program *program,
                             uint32_t offset, uint32_t count) {
  return program != NULL && slice_valid(offset, count, program->value_byte_count) &&
         (count == 0u || program->value_bytes != NULL);
}

static bool span_valid(w_seed_span span, size_t source_length) {
  return span.start_byte <= span.end_byte &&
         span.end_byte <= source_length;
}

static bool frontend_type_supported(const w_seed_frontend_type *type) {
  if (type == NULL || !text_valid(type->spelling)) return false;
  if (type->kind == W_SEED_FRONTEND_TYPE_UNIT)
    return text_is(type->spelling, HIR0_UNIT_NAME);
  if (type->kind == W_SEED_FRONTEND_TYPE_STRING)
    return text_is(type->spelling, HIR0_STRING_NAME);
  if (type->kind == W_SEED_FRONTEND_TYPE_BOOL)
    return text_is(type->spelling, HIR0_BOOL_NAME);
  if (type->kind == W_SEED_FRONTEND_TYPE_INTEGER)
    return type->is_signed && type->bit_width == 64u &&
           text_is(type->spelling, HIR0_I64_NAME);
  return false;
}

static bool frontend_binding_type_supported(
    const w_seed_frontend_type *type) {
  return frontend_type_supported(type) &&
         type->kind != W_SEED_FRONTEND_TYPE_UNIT;
}

static bool frontend_supported_types_equal(
    const w_seed_frontend_type *left, const w_seed_frontend_type *right) {
  if (!frontend_type_supported(left) || !frontend_type_supported(right) ||
      left->kind != right->kind)
    return false;
  if (left->kind == W_SEED_FRONTEND_TYPE_INTEGER)
    return left->is_signed == right->is_signed &&
           left->bit_width == right->bit_width;
  return true;
}

static bool frontend_counts_equal(const w_seed_frontend_counts *left,
                                  const w_seed_frontend_counts *right) {
  if (left == NULL || right == NULL) return false;
#define HIR0_COUNT(field) if (left->field != right->field) return false
  HIR0_COUNT(modules);
  HIR0_COUNT(imports);
  HIR0_COUNT(import_items);
  HIR0_COUNT(structs);
  HIR0_COUNT(fields);
  HIR0_COUNT(type_declarations);
  HIR0_COUNT(aliases);
  HIR0_COUNT(types);
  HIR0_COUNT(functions);
  HIR0_COUNT(parameters);
  HIR0_COUNT(entries);
  HIR0_COUNT(statements);
  HIR0_COUNT(expressions);
  HIR0_COUNT(interpolation_segments);
  HIR0_COUNT(arguments);
  HIR0_COUNT(symbols);
  HIR0_COUNT(facts);
  HIR0_COUNT(diagnostics);
  HIR0_COUNT(diagnostic_facts);
  HIR0_COUNT(diagnostic_items);
  HIR0_COUNT(diagnostic_labels);
  HIR0_COUNT(receipt_bytes);
  HIR0_COUNT(enums);
  HIR0_COUNT(enum_cases);
  HIR0_COUNT(enum_case_parameters);
  HIR0_COUNT(switch_arms);
  HIR0_COUNT(enum_subset_members);
  HIR0_COUNT(enum_membership_cases);
  HIR0_COUNT(generic_parameters);
  HIR0_COUNT(generic_applications);
  HIR0_COUNT(generic_arguments);
  HIR0_COUNT(typed_const_expressions);
  HIR0_COUNT(const_values);
  HIR0_COUNT(const_elements);
  HIR0_COUNT(const_bytes);
  HIR0_COUNT(const_declarations);
#undef HIR0_COUNT
  return true;
}

static bool hir_counts_equal(const w_seed_hir0_counts *left,
                             const w_seed_hir0_counts *right) {
  if (left == NULL || right == NULL) return false;
#define HIR0_COUNT(field) if (left->field != right->field) return false
  HIR0_COUNT(modules);
  HIR0_COUNT(identities);
  HIR0_COUNT(types);
  HIR0_COUNT(functions);
  HIR0_COUNT(parameters);
  HIR0_COUNT(blocks);
  HIR0_COUNT(instructions);
  HIR0_COUNT(bindings);
  HIR0_COUNT(calls);
  HIR0_COUNT(host_parameters);
  HIR0_COUNT(arguments);
  HIR0_COUNT(requirements);
  HIR0_COUNT(values);
  HIR0_COUNT(interpolation_segments);
  HIR0_COUNT(terminators);
  HIR0_COUNT(entries);
  HIR0_COUNT(text_bytes);
  HIR0_COUNT(value_bytes);
  HIR0_COUNT(receipt_bytes);
#undef HIR0_COUNT
  return true;
}

static bool frontend_array_ok(const void *pointer, size_t count,
                              size_t capacity, size_t element_size) {
  if (count > capacity || (count != 0u && pointer == NULL)) return false;
  return count == 0u || count <= SIZE_MAX / element_size;
}

static bool frontend_shape_ok(const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL)
    return false;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (result->status != W_SEED_FRONTEND_OK ||
      !text_is(result->schema_version, W_SEED_FRONTEND_SCHEMA_VERSION) ||
      !frontend_counts_equal(&result->required, &result->written) ||
      result->receipt_bytes != result->written.receipt_bytes ||
      result->written.receipt_bytes > output->receipt_capacity ||
      (result->written.receipt_bytes != 0u && output->receipt == NULL) ||
       frontend_input->documents == NULL || frontend_input->document_count != 1u ||
       frontend_input->document_count > W_SEED_FRONTEND_MAX_DOCUMENTS ||
      frontend_input->external_module_count != 0u ||
      frontend_input->resolved_import_count != 0u ||
      frontend_input->import_resolution_complete)
    return false;
#define HIR0_FRONTEND_ARRAY(field, capacity_field, type)                       \
  if (!frontend_array_ok(output->field, result->written.field,               \
                         output->capacity_field, sizeof(type))) return false
  HIR0_FRONTEND_ARRAY(modules, module_capacity, w_seed_frontend_module);
  HIR0_FRONTEND_ARRAY(types, type_capacity, w_seed_frontend_type);
  HIR0_FRONTEND_ARRAY(functions, function_capacity, w_seed_frontend_function);
  HIR0_FRONTEND_ARRAY(parameters, parameter_capacity, w_seed_frontend_parameter);
  HIR0_FRONTEND_ARRAY(entries, entry_capacity, w_seed_frontend_entry);
  HIR0_FRONTEND_ARRAY(statements, statement_capacity, w_seed_frontend_statement);
  HIR0_FRONTEND_ARRAY(expressions, expression_capacity, w_seed_frontend_expression);
  HIR0_FRONTEND_ARRAY(interpolation_segments, interpolation_segment_capacity,
                      w_seed_frontend_interpolation_segment);
  HIR0_FRONTEND_ARRAY(arguments, argument_capacity, w_seed_frontend_argument);
  HIR0_FRONTEND_ARRAY(const_bytes, const_bytes_capacity, uint8_t);
#undef HIR0_FRONTEND_ARRAY
   if (result->written.modules != 1u || result->written.functions == 0u ||
       result->written.entries != 1u || result->written.types == 0u ||
      frontend_input->host_scope == NULL ||
      frontend_input->host_scope->symbols == NULL ||
      frontend_input->host_scope->symbol_count == 0u ||
      frontend_input->host_scope->symbol_count > W_SEED_FRONTEND_MAX_HOST_SYMBOLS)
    return false;
  return true;
}

static bool host_shape_ok(const w_seed_frontend_host_prelude *scope) {
  if (scope == NULL || !text_valid(scope->profile) || scope->profile.length == 0u ||
      scope->symbols == NULL || scope->symbol_count == 0u ||
      scope->symbol_count > W_SEED_FRONTEND_MAX_HOST_SYMBOLS)
    return false;
  size_t parameters = 0u;
  for (size_t index = 0u; index < scope->symbol_count; index += 1u) {
    const w_seed_frontend_host_prelude_symbol *symbol = &scope->symbols[index];
    if (!text_valid(symbol->name) || symbol->name.length == 0u ||
        symbol->kind != W_SEED_FRONTEND_EXTERNAL_VALUE ||
        !text_valid(symbol->return_type) || !text_is(symbol->return_type, "()") ||
        symbol->parameter_count > W_SEED_FRONTEND_MAX_HOST_PARAMETERS ||
        (symbol->parameter_count != 0u && symbol->parameters == NULL) ||
        symbol->requirement_count > W_SEED_FRONTEND_MAX_HOST_REQUIREMENTS ||
        (symbol->requirement_count != 0u && symbol->requirements == NULL) ||
        !add_size(parameters, symbol->parameter_count, &parameters) ||
        parameters > W_SEED_FRONTEND_MAX_HOST_PARAMETERS)
      return false;
    for (size_t prior = 0u; prior < index; prior += 1u)
      if (text_equal(symbol->name, scope->symbols[prior].name)) return false;
    for (size_t parameter = 0u; parameter < symbol->parameter_count;
         parameter += 1u) {
      const w_seed_frontend_external_parameter *value =
          &symbol->parameters[parameter];
      if (!text_valid(value->name) || !text_valid(value->type) ||
          !text_is(value->type, HIR0_STRING_NAME) ||
          value->label_kind > W_SEED_FRONTEND_LABEL_REQUIRED ||
          (value->label_kind != W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
           value->name.length == 0u))
        return false;
    }
    for (size_t requirement = 0u; requirement < symbol->requirement_count;
         requirement += 1u) {
      const w_seed_frontend_host_requirement *value =
          &symbol->requirements[requirement];
      if (!text_valid(value->name) || value->name.length == 0u) return false;
      for (size_t prior = 0u; prior < requirement; prior += 1u)
        if (text_equal(value->name, symbol->requirements[prior].name))
          return false;
    }
  }
  return true;
}

static bool frontend_span_ok(const w_seed_frontend_document *document,
                             w_seed_span span) {
  return document != NULL && document->source != NULL &&
         w_seed_source_validate_span(document->source, span, NULL);
}

static bool frontend_sources_ok(const w_seed_hir0_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_input->documents == NULL)
    return false;
  for (size_t index = 0u; index < input->frontend_input->document_count;
       index += 1u) {
    const w_seed_frontend_document *document =
        &input->frontend_input->documents[index];
    if (document->source == NULL) return false;
    const w_seed_byte_view bytes = w_seed_source_bytes(document->source);
    if (bytes.length != 0u && bytes.data == NULL) return false;
  }
  return true;
}

static bool frontend_type_records_ok(const w_seed_hir0_input *input) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  for (size_t index = 0u; index < result->written.types; index += 1u) {
    const w_seed_frontend_type *type = &output->types[index];
    if (!frontend_type_supported(type) || !frontend_span_ok(
            &input->frontend_input->documents[0], type->span))
      return false;
  }
  return true;
}

static bool frontend_module_ranges_ok(const w_seed_hir0_input *input) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  size_t function_cursor = 0u;
  size_t entry_cursor = 0u;
  for (size_t index = 0u; index < result->written.modules; index += 1u) {
    const w_seed_frontend_module *module = &output->modules[index];
    if (module->document_index >= frontend_input->document_count ||
        !text_valid(module->source_id) || !text_valid(module->module_id) ||
        !text_valid(module->local_module_name) ||
         !frontend_span_ok(&frontend_input->documents[module->document_index],
                           module->span) ||
         module->first_function != function_cursor ||
         module->first_entry != entry_cursor ||
         !range_valid(module->first_function, module->function_count,
                     result->written.functions) ||
        !range_valid(module->first_entry, module->entry_count,
                     result->written.entries) ||
        module->first_function > UINT32_MAX || module->first_entry > UINT32_MAX)
      return false;
    for (size_t prior = 0u; prior < index; prior += 1u)
      if (text_equal(module->module_id, output->modules[prior].module_id))
        return false;
    function_cursor += module->function_count;
    entry_cursor += module->entry_count;
  }
  return function_cursor == result->written.functions &&
         entry_cursor == result->written.entries;
}

static bool frontend_function_ranges_ok(const w_seed_hir0_input *input) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  size_t parameter_cursor = 0u;
  size_t statement_cursor = 0u;
  for (size_t index = 0u; index < result->written.functions; index += 1u) {
    const w_seed_frontend_function *function = &output->functions[index];
    if (function->module_index >= result->written.modules ||
        !text_valid(function->name) || function->name.length == 0u ||
        function->return_type == W_SEED_FRONTEND_NONE ||
        (size_t)function->return_type >= result->written.types ||
        (output->types[function->return_type].kind !=
             W_SEED_FRONTEND_TYPE_UNIT &&
         !frontend_binding_type_supported(
             &output->types[function->return_type])) ||
        !frontend_span_ok(&input->frontend_input->documents[
                              output->modules[function->module_index]
                                  .document_index],
                          function->span) ||
         !frontend_span_ok(&input->frontend_input->documents[
                               output->modules[function->module_index]
                                   .document_index],
                           function->body_span) ||
         function->first_parameter != parameter_cursor ||
         function->first_statement != statement_cursor ||
         function->first_parameter > UINT32_MAX ||
        !range_valid(function->first_parameter, function->parameter_count,
                     result->written.parameters) ||
        !range_valid(function->first_statement, function->statement_count,
                     result->written.statements))
      return false;
    const w_seed_frontend_module *module =
        &output->modules[function->module_index];
    if (!range_valid(module->first_function, module->function_count,
                     result->written.functions) ||
        index < module->first_function ||
        index >= (size_t)module->first_function + module->function_count)
      return false;
    for (size_t prior = 0u; prior < index; prior += 1u)
      if (output->functions[prior].module_index == function->module_index &&
          text_equal(output->functions[prior].name, function->name))
        return false;
    for (size_t parameter = 0u; parameter < function->parameter_count;
         parameter += 1u) {
      const size_t parameter_index = (size_t)function->first_parameter + parameter;
      const w_seed_frontend_parameter *value = &output->parameters[parameter_index];
      if (value->owner_function != index || value->module_index != function->module_index ||
          value->type_index == W_SEED_FRONTEND_NONE ||
          (size_t)value->type_index >= result->written.types ||
          !frontend_binding_type_supported(&output->types[value->type_index]) ||
          !frontend_span_ok(&input->frontend_input->documents[
                                module->document_index],
                            value->span) ||
          !text_valid(value->name) ||
           !frontend_label_valid(value->label_kind, value->label))
        return false;
    }
    parameter_cursor += function->parameter_count;
    statement_cursor += function->statement_count;
  }
  return parameter_cursor == result->written.parameters &&
         statement_cursor == result->written.statements;
}

static bool frontend_entry_records_ok(const w_seed_hir0_input *input) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  for (size_t index = 0u; index < result->written.entries; index += 1u) {
    const w_seed_frontend_entry *entry = &output->entries[index];
    if (entry->module_index >= result->written.modules || !entry->valid ||
        !text_valid(entry->target) || entry->target.length == 0u ||
        !frontend_span_ok(&input->frontend_input->documents[
                              output->modules[entry->module_index]
                                  .document_index],
                          entry->span))
      return false;
    const w_seed_frontend_module *module = &output->modules[entry->module_index];
    if (index < module->first_entry ||
        index >= (size_t)module->first_entry + module->entry_count)
      return false;
    bool found = false;
    for (size_t function = 0u; function < result->written.functions; function += 1u) {
      if (output->functions[function].module_index == entry->module_index &&
          text_equal(output->functions[function].name, entry->target)) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  return true;
}

/* The frontend symbol table is an auxiliary index, not an HIR0 record family.
 * The one-module HIR0 subset nevertheless requires its exact canonical
 * projection so a forged symbol cannot contradict the graph that HIR0 copies
 * from modules/functions/entries. HIR lowering never reads symbols as an
 * authority. */
static bool frontend_symbol_records_ok(const w_seed_hir0_input *input) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  size_t expected = 1u;
  for (size_t statement = 0u; statement < result->written.statements;
       statement += 1u) {
    const w_seed_frontend_statement *source = &output->statements[statement];
    if ((source->kind == W_SEED_FRONTEND_STMT_LET ||
         source->kind == W_SEED_FRONTEND_STMT_VAR) &&
        source->binding_name.length != 0u &&
        !add_size(expected, 1u, &expected))
      return false;
  }
  if (!add_size(expected, result->written.functions, &expected) ||
      !add_size(expected, result->written.parameters, &expected) ||
      !add_size(expected, result->written.entries, &expected) ||
      result->written.symbols != expected)
    return false;
  const w_seed_frontend_module *module = &output->modules[0];
  const w_seed_frontend_symbol *module_symbol = &output->symbols[0];
  if (module_symbol->kind != W_SEED_FRONTEND_SYMBOL_MODULE ||
      module_symbol->module_index != 0u || module_symbol->owner_index != 0u ||
      !text_equal(module_symbol->name, module->module_id) ||
      !module_symbol->exported ||
      !frontend_span_ok(&input->frontend_input->documents[0], module_symbol->span))
    return false;
  size_t symbol_cursor = 1u;
  for (size_t function = 0u; function < result->written.functions; function += 1u) {
    const w_seed_frontend_function *source = &output->functions[function];
    for (size_t parameter = 0u; parameter < source->parameter_count;
         parameter += 1u) {
      const size_t parameter_index = (size_t)source->first_parameter + parameter;
      const w_seed_frontend_parameter *parameter_source =
          &output->parameters[parameter_index];
      const w_seed_frontend_symbol *symbol = &output->symbols[symbol_cursor++];
      if (symbol->kind != W_SEED_FRONTEND_SYMBOL_PARAMETER ||
          symbol->module_index != source->module_index ||
          symbol->owner_index != parameter_index ||
          !text_equal(symbol->name, parameter_source->name) || symbol->exported ||
          symbol->type_index != parameter_source->type_index ||
          !frontend_span_ok(&input->frontend_input->documents[0], symbol->span))
         return false;
    }
    for (size_t statement = 0u; statement < source->statement_count;
         statement += 1u) {
      const size_t statement_index = (size_t)source->first_statement + statement;
      const w_seed_frontend_statement *statement_source =
          &output->statements[statement_index];
      if ((statement_source->kind != W_SEED_FRONTEND_STMT_LET &&
           statement_source->kind != W_SEED_FRONTEND_STMT_VAR) ||
          statement_source->binding_name.length == 0u)
        continue;
      const w_seed_frontend_symbol *binding = &output->symbols[symbol_cursor++];
      if (binding->kind != W_SEED_FRONTEND_SYMBOL_BINDING ||
          binding->module_index != statement_source->module_index ||
          binding->owner_index != statement_index ||
          !text_equal(binding->name, statement_source->binding_name) ||
          binding->exported || binding->type_index != statement_source->effective_type ||
          !frontend_span_ok(&input->frontend_input->documents[0], binding->span))
        return false;
    }
    const w_seed_frontend_symbol *symbol = &output->symbols[symbol_cursor++];
    if (symbol->kind != W_SEED_FRONTEND_SYMBOL_FUNCTION ||
        symbol->module_index != source->module_index ||
        symbol->owner_index != function || !text_equal(symbol->name, source->name) ||
        symbol->exported || symbol->type_index != source->return_type ||
        !frontend_span_ok(&input->frontend_input->documents[0], symbol->span))
      return false;
  }
  for (size_t entry = 0u; entry < result->written.entries; entry += 1u) {
    const w_seed_frontend_symbol *symbol = &output->symbols[symbol_cursor++];
    const w_seed_frontend_entry *source = &output->entries[entry];
    if (symbol->kind != W_SEED_FRONTEND_SYMBOL_ENTRY ||
        symbol->module_index != source->module_index ||
        symbol->owner_index != entry || !text_equal(symbol->name, source->target) ||
        symbol->exported || symbol->type_index != W_SEED_FRONTEND_NONE ||
        !frontend_span_ok(&input->frontend_input->documents[0], symbol->span))
      return false;
  }
  return symbol_cursor == result->written.symbols;
}

static bool frontend_integer_i64(const w_seed_frontend_expression *value,
                                 int64_t *out) {
  if (value == NULL || out == NULL || !value->has_integer_value) return false;
  uint64_t magnitude = 0u;
  for (size_t index = 8u; index < sizeof(value->integer_value); index += 1u)
    if (value->integer_value[index] != 0u) return false;
  for (size_t index = 0u; index < 8u; index += 1u)
    magnitude |= (uint64_t)value->integer_value[index] << (index * 8u);
  if (magnitude > (uint64_t)INT64_MAX) return false;
  *out = (int64_t)magnitude;
  return true;
}

static bool frontend_value_common_ok(
    const w_seed_hir0_input *input, const w_seed_frontend_expression *value,
    size_t module_index, size_t function_index, size_t document_index) {
  if (value == NULL || !value->supported || value->module_index != module_index ||
      value->owner_function != function_index ||
      value->first_argument != W_SEED_FRONTEND_NONE ||
      value->argument_count != 0u ||
      (value->inferred_type != W_SEED_FRONTEND_NONE &&
       ((size_t)value->inferred_type >= input->frontend_result->written.types ||
        !frontend_type_supported(
            &input->frontend_output->types[value->inferred_type]))) ||
      !frontend_span_ok(&input->frontend_input->documents[document_index],
                        value->span))
    return false;
  return true;
}

static bool frontend_value_has_no_resolution(
    const w_seed_frontend_expression *value) {
  return value != NULL &&
         value->enum_index == W_SEED_FRONTEND_NONE &&
         value->enum_case_index == W_SEED_FRONTEND_NONE &&
         value->first_switch_arm == W_SEED_FRONTEND_NONE &&
         value->switch_arm_count == 0u &&
         value->first_membership_case == W_SEED_FRONTEND_NONE &&
         value->membership_case_count == 0u &&
         value->resolved_parameter_ordinal == W_SEED_FRONTEND_NONE &&
         value->resolved_function_index == W_SEED_FRONTEND_NONE &&
         value->resolved_callee_kind == W_SEED_FRONTEND_CALLEE_NONE &&
         value->resolved_host_symbol_index == W_SEED_FRONTEND_NONE &&
         value->resolved_external_module_index == W_SEED_FRONTEND_NONE &&
         value->resolved_external_symbol_index == W_SEED_FRONTEND_NONE &&
         value->resolved_local_ordinal == W_SEED_FRONTEND_NONE &&
         value->resolved_const_declaration == W_SEED_FRONTEND_NONE &&
         value->member_name.length == 0u && text_valid(value->member_name);
}

/* Frontend expressions are append-only postorder records. This walk consumes
 * exactly one dense subtree and measures the normalized HIR value tree. A
 * finite depth bound keeps validation stack use independent of hostile input. */
static bool frontend_value_tree_ok(
    const w_seed_hir0_input *input, size_t module_index, size_t function_index,
    size_t document_index, size_t use_statement, uint32_t root_index,
    size_t depth,
    size_t *expression_cursor, size_t *segment_cursor,
    size_t *const_byte_cursor, size_t *value_total, size_t *segment_total,
    size_t *value_bytes) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (depth > 256u || expression_cursor == NULL || segment_cursor == NULL ||
      const_byte_cursor == NULL || value_total == NULL ||
      segment_total == NULL || value_bytes == NULL ||
      root_index == W_SEED_FRONTEND_NONE ||
      (size_t)root_index >= result->written.expressions)
    return false;
  const w_seed_frontend_expression *value = &output->expressions[root_index];
  if (!frontend_value_common_ok(input, value, module_index, function_index,
                                document_index))
    return false;

  if (value->kind == W_SEED_FRONTEND_EXPR_BINARY) {
    if (value->left == W_SEED_FRONTEND_NONE ||
        value->right == W_SEED_FRONTEND_NONE ||
        !frontend_value_tree_ok(input, module_index, function_index,
                                document_index, use_statement, value->left,
                                depth + 1u,
                                expression_cursor, segment_cursor,
                                const_byte_cursor, value_total, segment_total,
                                value_bytes) ||
        !frontend_value_tree_ok(input, module_index, function_index,
                                document_index, use_statement, value->right,
                                depth + 1u,
                                expression_cursor, segment_cursor,
                                const_byte_cursor, value_total, segment_total,
                                value_bytes) ||
        (size_t)root_index != *expression_cursor ||
        value->inferred_type == W_SEED_FRONTEND_NONE ||
        output->types[value->inferred_type].kind !=
            W_SEED_FRONTEND_TYPE_INTEGER ||
        output->types[value->inferred_type].bit_width != 64u ||
        !output->types[value->inferred_type].is_signed ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u || value->has_bool_value ||
        value->has_integer_value ||
        !(text_is(value->operator_text, "+") ||
          text_is(value->operator_text, "-") ||
          text_is(value->operator_text, "*") ||
          text_is(value->operator_text, "/") ||
          text_is(value->operator_text, "%")) ||
        !add_size(*value_total, 1u, value_total) ||
        !add_size(*expression_cursor, 1u, expression_cursor))
      return false;
    return true;
  }

  if (value->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS) {
    if (value->left == W_SEED_FRONTEND_NONE ||
        value->right != W_SEED_FRONTEND_NONE ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u || value->has_bool_value ||
        value->has_integer_value ||
        !frontend_value_tree_ok(input, module_index, function_index,
                                document_index, use_statement, value->left,
                                depth + 1u,
                                expression_cursor, segment_cursor,
                                const_byte_cursor, value_total, segment_total,
                                value_bytes) ||
        (size_t)root_index != *expression_cursor ||
        !add_size(*expression_cursor, 1u, expression_cursor))
      return false;
    return true;
  }

  if (value->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING) {
    if (value->left != W_SEED_FRONTEND_NONE ||
        value->right != W_SEED_FRONTEND_NONE ||
        (value->inferred_type != W_SEED_FRONTEND_NONE &&
         output->types[value->inferred_type].kind !=
             W_SEED_FRONTEND_TYPE_STRING) ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u || value->has_bool_value ||
        value->has_integer_value ||
        value->first_interpolation_segment != *segment_cursor ||
        !range_valid(value->first_interpolation_segment,
                     value->interpolation_segment_count,
                     result->written.interpolation_segments))
      return false;
    for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_frontend_interpolation_segment *segment =
          &output->interpolation_segments[*segment_cursor];
      if (segment->owner_expression != root_index ||
          segment->ordinal != ordinal ||
          !frontend_span_ok(&input->frontend_input->documents[document_index],
                            segment->span))
        return false;
      if (segment->kind == W_SEED_FRONTEND_INTERPOLATION_TEXT) {
        if (segment->expression_index != W_SEED_FRONTEND_NONE ||
            segment->const_byte_offset == W_SEED_FRONTEND_NONE ||
            (size_t)segment->const_byte_offset != *const_byte_cursor ||
            !range_valid(segment->const_byte_offset, segment->const_byte_count,
                         result->written.const_bytes) ||
            !add_size(*const_byte_cursor, segment->const_byte_count,
                      const_byte_cursor) ||
            !add_size(*value_bytes, segment->const_byte_count, value_bytes))
          return false;
      } else if (segment->kind ==
                 W_SEED_FRONTEND_INTERPOLATION_EXPRESSION) {
        if (segment->const_byte_offset != W_SEED_FRONTEND_NONE ||
            segment->const_byte_count != 0u ||
            !frontend_value_tree_ok(
                input, module_index, function_index, document_index,
                use_statement,
                segment->expression_index, depth + 1u, expression_cursor,
                segment_cursor, const_byte_cursor, value_total, segment_total,
                value_bytes))
          return false;
      } else {
        return false;
      }
      if (!add_size(*segment_cursor, 1u, segment_cursor) ||
          !add_size(*segment_total, 1u, segment_total))
        return false;
    }
    if ((size_t)root_index != *expression_cursor ||
        !add_size(*value_total, 1u, value_total) ||
        !add_size(*expression_cursor, 1u, expression_cursor))
      return false;
    return true;
  }

  if ((size_t)root_index != *expression_cursor ||
      value->left != W_SEED_FRONTEND_NONE ||
      value->right != W_SEED_FRONTEND_NONE ||
      value->first_interpolation_segment != W_SEED_FRONTEND_NONE ||
      value->interpolation_segment_count != 0u)
    return false;
  if (value->kind == W_SEED_FRONTEND_EXPR_STRING) {
    if ((value->inferred_type != W_SEED_FRONTEND_NONE &&
         output->types[value->inferred_type].kind !=
             W_SEED_FRONTEND_TYPE_STRING) ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->has_bool_value || value->has_integer_value ||
        value->const_byte_offset == W_SEED_FRONTEND_NONE ||
        (size_t)value->const_byte_offset != *const_byte_cursor ||
        !range_valid(value->const_byte_offset, value->const_byte_count,
                     result->written.const_bytes) ||
        !add_size(*const_byte_cursor, value->const_byte_count,
                  const_byte_cursor) ||
        !add_size(*value_bytes, value->const_byte_count, value_bytes))
      return false;
  } else if (value->kind == W_SEED_FRONTEND_EXPR_INTEGER) {
    int64_t ignored = 0;
    if (value->inferred_type == W_SEED_FRONTEND_NONE ||
        output->types[value->inferred_type].kind !=
            W_SEED_FRONTEND_TYPE_INTEGER ||
        !frontend_integer_i64(value, &ignored) ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u || value->has_bool_value)
      return false;
  } else if (value->kind == W_SEED_FRONTEND_EXPR_BOOL) {
    if (value->inferred_type == W_SEED_FRONTEND_NONE ||
        output->types[value->inferred_type].kind != W_SEED_FRONTEND_TYPE_BOOL ||
        !value->has_bool_value || value->has_integer_value ||
        !frontend_value_has_no_resolution(value) ||
        value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u)
      return false;
  } else if (value->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER) {
    if (value->inferred_type == W_SEED_FRONTEND_NONE ||
        !frontend_binding_type_supported(
            &output->types[value->inferred_type]) ||
        value->const_byte_offset != W_SEED_FRONTEND_NONE ||
        value->const_byte_count != 0u || value->has_bool_value ||
        value->has_integer_value ||
        value->resolved_function_index != W_SEED_FRONTEND_NONE ||
        value->resolved_callee_kind != W_SEED_FRONTEND_CALLEE_NONE ||
        value->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
        value->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
        value->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
        value->resolved_local_ordinal != W_SEED_FRONTEND_NONE ||
        value->resolved_const_declaration != W_SEED_FRONTEND_NONE ||
        value->member_name.length != 0u || !text_valid(value->member_name))
      return false;
    if (value->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE) {
      const w_seed_frontend_function *function =
          &output->functions[function_index];
      if (value->resolved_binding_statement != W_SEED_FRONTEND_NONE ||
          value->resolved_parameter_ordinal >= function->parameter_count)
        return false;
      const size_t parameter_index =
          (size_t)function->first_parameter +
          value->resolved_parameter_ordinal;
      if (parameter_index >= result->written.parameters)
        return false;
      const w_seed_frontend_parameter *parameter =
          &output->parameters[parameter_index];
      if (parameter->owner_function != function_index ||
          parameter->module_index != module_index ||
          !frontend_supported_types_equal(
              &output->types[parameter->type_index],
              &output->types[value->inferred_type]) ||
          !text_equal(parameter->name, value->spelling))
        return false;
    } else {
      if (value->resolved_binding_statement == W_SEED_FRONTEND_NONE ||
          (size_t)value->resolved_binding_statement >= use_statement ||
          (size_t)value->resolved_binding_statement >=
              result->written.statements)
        return false;
      const w_seed_frontend_statement *binding =
          &output->statements[value->resolved_binding_statement];
      if (binding->kind != W_SEED_FRONTEND_STMT_LET ||
          binding->owner_function != function_index ||
          binding->module_index != module_index ||
          binding->effective_type != value->inferred_type ||
          !text_equal(binding->binding_name, value->spelling))
        return false;
    }
  } else {
    return false;
  }
  return add_size(*value_total, 1u, value_total) &&
         add_size(*expression_cursor, 1u, expression_cursor);
}

static bool frontend_string_value_root(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, uint32_t expression_index,
    size_t depth) {
  if (output == NULL || result == NULL || output->expressions == NULL ||
      depth > 256u || (size_t)expression_index >= result->written.expressions)
    return false;
  const w_seed_frontend_expression *value =
      &output->expressions[expression_index];
  if (value->kind == W_SEED_FRONTEND_EXPR_STRING ||
      value->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING)
    return true;
  if (value->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS &&
      value->left != W_SEED_FRONTEND_NONE)
    return frontend_string_value_root(output, result, value->left, depth + 1u);
  return value->inferred_type != W_SEED_FRONTEND_NONE &&
         (size_t)value->inferred_type < result->written.types &&
         output->types[value->inferred_type].kind ==
             W_SEED_FRONTEND_TYPE_STRING;
}

/* Validate one direct call expression while consuming the frontend's dense
 * postorder expression and argument ranges. A call used as a statement must
 * return Unit. A call used as a binding initializer must be local and return
 * one scalar value; that result becomes one HIR SSA value. */
static bool frontend_call_expression_ok(
    const w_seed_hir0_input *input, size_t module_index,
    size_t function_index, size_t document_index, size_t statement_index,
    uint32_t root_index, bool result_value, size_t *expression_cursor,
    size_t *interpolation_segment_cursor, size_t *const_byte_cursor,
    size_t *call_total, size_t *argument_total, size_t *value_total,
    size_t *segment_total, size_t *value_bytes) {
  if (input == NULL || expression_cursor == NULL ||
      interpolation_segment_cursor == NULL || const_byte_cursor == NULL ||
      call_total == NULL || argument_total == NULL || value_total == NULL ||
      segment_total == NULL || value_bytes == NULL)
    return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (root_index == W_SEED_FRONTEND_NONE ||
      (size_t)root_index >= result->written.expressions)
    return false;
  const w_seed_frontend_expression *call = &output->expressions[root_index];
  const bool host_call =
      call->resolved_callee_kind ==
      W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL;
  const bool local_call =
      call->resolved_callee_kind == W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION;
  if (call->kind != W_SEED_FRONTEND_EXPR_CALL || !call->supported ||
      call->module_index != module_index ||
      call->owner_function != function_index ||
      call->left == W_SEED_FRONTEND_NONE ||
      (size_t)call->left >= result->written.expressions ||
      (size_t)call->left != *expression_cursor ||
      call->right != W_SEED_FRONTEND_NONE ||
      call->first_argument != *argument_total ||
      !range_valid(call->first_argument, call->argument_count,
                   result->written.arguments) ||
      (!host_call && !local_call) || (result_value && !local_call) ||
      !frontend_span_ok(&input->frontend_input->documents[document_index],
                        call->span))
    return false;
  const w_seed_frontend_expression *callee = &output->expressions[call->left];
  if (callee->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
      !callee->supported || callee->module_index != module_index ||
      callee->owner_function != function_index ||
      callee->resolved_callee_kind != call->resolved_callee_kind ||
      callee->resolved_function_index != call->resolved_function_index ||
      callee->resolved_host_symbol_index != call->resolved_host_symbol_index ||
      callee->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
      callee->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
      !frontend_span_ok(&input->frontend_input->documents[document_index],
                        callee->span) ||
      !add_size(*expression_cursor, 1u, expression_cursor))
    return false;

  const w_seed_frontend_host_prelude_symbol *host = NULL;
  const w_seed_frontend_function *target_function = NULL;
  size_t parameter_count = 0u;
  uint32_t return_type = W_SEED_FRONTEND_NONE;
  if (host_call) {
    if (call->resolved_function_index != W_SEED_FRONTEND_NONE ||
        call->resolved_host_symbol_index == W_SEED_FRONTEND_NONE ||
        (size_t)call->resolved_host_symbol_index >=
            input->frontend_input->host_scope->symbol_count)
      return false;
    host = &input->frontend_input->host_scope
                ->symbols[call->resolved_host_symbol_index];
    if (!text_equal(callee->spelling, host->name) ||
        !text_is(host->return_type, "()"))
      return false;
    parameter_count = host->parameter_count;
  } else {
    if (call->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
        call->resolved_function_index == W_SEED_FRONTEND_NONE ||
        (size_t)call->resolved_function_index >= result->written.functions)
      return false;
    target_function = &output->functions[call->resolved_function_index];
    if (target_function->module_index != module_index ||
        !text_equal(callee->spelling, target_function->name) ||
        target_function->return_type == W_SEED_FRONTEND_NONE ||
        (size_t)target_function->return_type >= result->written.types)
      return false;
    return_type = target_function->return_type;
    const w_seed_frontend_type_kind return_kind =
        output->types[return_type].kind;
    if ((!result_value && return_kind != W_SEED_FRONTEND_TYPE_UNIT) ||
        (result_value && return_kind != W_SEED_FRONTEND_TYPE_INTEGER &&
         return_kind != W_SEED_FRONTEND_TYPE_BOOL) ||
        (return_kind == W_SEED_FRONTEND_TYPE_INTEGER &&
         (!output->types[return_type].is_signed ||
          output->types[return_type].bit_width != 64u)))
      return false;
    parameter_count = target_function->parameter_count;
  }
  if (parameter_count != call->argument_count ||
      (!host_call &&
       (call->inferred_type == W_SEED_FRONTEND_NONE ||
        !frontend_supported_types_equal(&output->types[call->inferred_type],
                                        &output->types[return_type]))))
    return false;

  for (size_t argument_ordinal = 0u;
       argument_ordinal < call->argument_count; argument_ordinal += 1u) {
    const size_t argument_index =
        (size_t)call->first_argument + argument_ordinal;
    const w_seed_frontend_argument *argument =
        &output->arguments[argument_index];
    if (argument->module_index != module_index ||
        argument->owner_expression != call->left ||
        argument->expression_index == W_SEED_FRONTEND_NONE ||
        (size_t)argument->expression_index >= result->written.expressions ||
        argument->resolved_parameter_ordinal == W_SEED_FRONTEND_NONE ||
        argument->resolved_parameter_ordinal >= parameter_count ||
        !frontend_span_ok(&input->frontend_input->documents[document_index],
                          argument->span))
      return false;
    for (size_t prior = 0u; prior < argument_ordinal; prior += 1u) {
      const w_seed_frontend_argument *prior_argument =
          &output->arguments[(size_t)call->first_argument + prior];
      if (prior_argument->resolved_parameter_ordinal ==
          argument->resolved_parameter_ordinal)
        return false;
    }
    const w_seed_frontend_expression *value =
        &output->expressions[argument->expression_index];
    uint32_t expected_type = W_SEED_FRONTEND_NONE;
    if (host_call) {
      const w_seed_frontend_external_parameter *host_parameter =
          &host->parameters[argument->resolved_parameter_ordinal];
      if (!text_is(host_parameter->type, HIR0_STRING_NAME) ||
          !frontend_host_label_matches(host_parameter->label_kind,
                                       host_parameter->name, argument->label))
        return false;
    } else {
      const size_t parameter_index =
          (size_t)target_function->first_parameter +
          argument->resolved_parameter_ordinal;
      if (parameter_index >= result->written.parameters)
        return false;
      const w_seed_frontend_parameter *parameter =
          &output->parameters[parameter_index];
      if (!frontend_parameter_label_matches(parameter, argument->label))
        return false;
      expected_type = parameter->type_index;
    }
    if ((local_call &&
         (expected_type == W_SEED_FRONTEND_NONE ||
          value->inferred_type == W_SEED_FRONTEND_NONE ||
          !frontend_supported_types_equal(&output->types[value->inferred_type],
                                          &output->types[expected_type]))) ||
        (host_call && !frontend_string_value_root(
                          output, result, argument->expression_index, 0u)) ||
        !frontend_value_tree_ok(
            input, module_index, function_index, document_index,
            statement_index, argument->expression_index, 0u,
            expression_cursor, interpolation_segment_cursor,
            const_byte_cursor, value_total, segment_total, value_bytes) ||
        !add_size(*argument_total, 1u, argument_total))
      return false;
  }
  if ((size_t)root_index != *expression_cursor ||
      !add_size(*expression_cursor, 1u, expression_cursor) ||
      !add_size(*call_total, 1u, call_total) ||
      (result_value && !add_size(*value_total, 1u, value_total)))
    return false;
  return true;
}

static bool frontend_statement_and_expression_ok(
    const w_seed_hir0_input *input, size_t *binding_total, size_t *call_total,
    size_t *argument_total, size_t *value_total, size_t *segment_total,
    size_t *value_bytes, size_t *text_bytes) {
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  if (binding_total == NULL || call_total == NULL || argument_total == NULL ||
      value_total == NULL || segment_total == NULL || value_bytes == NULL ||
      text_bytes == NULL)
    return false;
  size_t bindings = 0u;
  size_t calls = 0u;
  size_t args = 0u;
  size_t bytes = 0u;
  size_t expression_cursor = 0u;
  size_t interpolation_segment_cursor = 0u;
  size_t const_byte_cursor = 0u;
  size_t values = 0u;
  size_t segments = 0u;
  for (size_t function = 0u; function < result->written.functions; function += 1u) {
    const w_seed_frontend_function *owner = &output->functions[function];
    const size_t module_index = owner->module_index;
    const size_t document_index = output->modules[module_index].document_index;
    for (size_t ordinal = 0u; ordinal < owner->statement_count; ordinal += 1u) {
      const size_t statement_index = (size_t)owner->first_statement + ordinal;
      const w_seed_frontend_statement *statement =
          &output->statements[statement_index];
      const uint32_t expected_next_sibling =
          ordinal + 1u < owner->statement_count
              ? (uint32_t)(statement_index + 1u)
              : W_SEED_FRONTEND_NONE;
      if (statement->module_index != module_index ||
          statement->owner_function != function ||
          statement->expression_index == W_SEED_FRONTEND_NONE ||
          (size_t)statement->expression_index >= result->written.expressions ||
          statement->condition_expression != W_SEED_FRONTEND_NONE ||
          statement->first_child != W_SEED_FRONTEND_NONE ||
          statement->child_count != 0u ||
          statement->next_sibling != expected_next_sibling ||
          statement->else_child != W_SEED_FRONTEND_NONE ||
          statement->range_lower_expression != W_SEED_FRONTEND_NONE ||
          statement->range_upper_expression != W_SEED_FRONTEND_NONE ||
          statement->loop_local_ordinal != W_SEED_FRONTEND_NONE ||
          !frontend_span_ok(&input->frontend_input->documents[document_index],
                            statement->span))
        return false;
      if (statement->kind == W_SEED_FRONTEND_STMT_LET) {
        if (!text_valid(statement->binding_name) ||
            statement->binding_name.length == 0u ||
            statement->effective_type == W_SEED_FRONTEND_NONE ||
            (size_t)statement->effective_type >= result->written.types ||
            !frontend_binding_type_supported(
                &output->types[statement->effective_type]) ||
            (statement->declared_type != W_SEED_FRONTEND_NONE &&
             (size_t)statement->declared_type >= result->written.types))
          return false;
        if (statement->declared_type != W_SEED_FRONTEND_NONE &&
            !frontend_supported_types_equal(
                &output->types[statement->declared_type],
                &output->types[statement->effective_type]))
          return false;
        const w_seed_frontend_expression *initializer =
            &output->expressions[statement->expression_index];
        if (initializer->inferred_type != statement->effective_type)
          return false;
        if (initializer->kind == W_SEED_FRONTEND_EXPR_CALL) {
          if (!frontend_call_expression_ok(
                  input, module_index, function, document_index,
                  statement_index, statement->expression_index, true,
                  &expression_cursor, &interpolation_segment_cursor,
                  &const_byte_cursor, &calls, &args, &values, &segments,
                  &bytes))
            return false;
        } else if (!frontend_value_tree_ok(
                       input, module_index, function, document_index,
                       statement_index, statement->expression_index, 0u,
                       &expression_cursor, &interpolation_segment_cursor,
                       &const_byte_cursor, &values, &segments, &bytes)) {
          return false;
        }
        if (!add_size(bindings, 1u, &bindings))
          return false;
        continue;
      }
      if (statement->kind == W_SEED_FRONTEND_STMT_RETURN) {
        if (ordinal + 1u != owner->statement_count ||
            statement->binding_name.length != 0u ||
            statement->declared_type != W_SEED_FRONTEND_NONE ||
            statement->effective_type != W_SEED_FRONTEND_NONE ||
            owner->return_type == W_SEED_FRONTEND_NONE ||
            (size_t)owner->return_type >= result->written.types ||
            (output->types[owner->return_type].kind !=
                 W_SEED_FRONTEND_TYPE_INTEGER &&
             output->types[owner->return_type].kind !=
                 W_SEED_FRONTEND_TYPE_BOOL) ||
            (output->types[owner->return_type].kind ==
                 W_SEED_FRONTEND_TYPE_INTEGER &&
             (!output->types[owner->return_type].is_signed ||
              output->types[owner->return_type].bit_width != 64u)) ||
            !frontend_value_tree_ok(
                input, module_index, function, document_index,
                statement_index, statement->expression_index, 0u,
                &expression_cursor, &interpolation_segment_cursor,
                &const_byte_cursor, &values, &segments, &bytes))
          return false;
        continue;
      }
      if (statement->kind != W_SEED_FRONTEND_STMT_EXPRESSION) return false;
      const size_t expression_index = (size_t)statement->expression_index;
      const w_seed_frontend_expression *call = &output->expressions[expression_index];
      const bool host_call =
          call->resolved_callee_kind ==
          W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL;
      const bool local_call =
          call->resolved_callee_kind == W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION;
      if (call->kind != W_SEED_FRONTEND_EXPR_CALL || !call->supported ||
          call->module_index != module_index || call->owner_function != function ||
           call->left == W_SEED_FRONTEND_NONE ||
           (size_t)call->left >= result->written.expressions ||
           (size_t)call->left != expression_cursor ||
           call->right != W_SEED_FRONTEND_NONE ||
           call->first_argument != args ||
           !range_valid(call->first_argument, call->argument_count,
                       result->written.arguments) ||
          (!host_call && !local_call) ||
          !frontend_span_ok(&input->frontend_input->documents[document_index],
                            call->span))
        return false;
      expression_cursor += 1u;
      const w_seed_frontend_expression *callee =
          &output->expressions[call->left];
      if (callee->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
          !callee->supported || callee->module_index != module_index ||
          callee->owner_function != function ||
          callee->resolved_callee_kind != call->resolved_callee_kind ||
          callee->resolved_function_index != call->resolved_function_index ||
          callee->resolved_host_symbol_index !=
              call->resolved_host_symbol_index ||
          callee->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
          callee->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
          !frontend_span_ok(&input->frontend_input->documents[document_index],
                            callee->span))
        return false;
      const w_seed_frontend_host_prelude_symbol *host = NULL;
      const w_seed_frontend_function *target_function = NULL;
      size_t parameter_count = 0u;
      if (host_call) {
        if (call->resolved_function_index != W_SEED_FRONTEND_NONE ||
            call->resolved_host_symbol_index == W_SEED_FRONTEND_NONE ||
            (size_t)call->resolved_host_symbol_index >=
                input->frontend_input->host_scope->symbol_count)
          return false;
        host = &input->frontend_input->host_scope
                    ->symbols[call->resolved_host_symbol_index];
        if (!text_equal(callee->spelling, host->name)) return false;
        parameter_count = host->parameter_count;
      } else {
        if (call->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
            call->resolved_function_index == W_SEED_FRONTEND_NONE ||
            (size_t)call->resolved_function_index >=
                result->written.functions)
          return false;
        target_function =
            &output->functions[call->resolved_function_index];
        if (target_function->module_index != module_index ||
            !text_equal(callee->spelling, target_function->name) ||
            target_function->return_type == W_SEED_FRONTEND_NONE ||
            (size_t)target_function->return_type >= result->written.types ||
            output->types[target_function->return_type].kind !=
                W_SEED_FRONTEND_TYPE_UNIT)
          return false;
        parameter_count = target_function->parameter_count;
      }
      if (parameter_count != call->argument_count) return false;
      for (size_t argument_ordinal = 0u;
           argument_ordinal < call->argument_count; argument_ordinal += 1u) {
        const size_t argument_index =
            (size_t)call->first_argument + argument_ordinal;
        const w_seed_frontend_argument *argument = &output->arguments[argument_index];
        if (argument->module_index != module_index ||
            argument->owner_expression != call->left ||
            argument->expression_index == W_SEED_FRONTEND_NONE ||
            (size_t)argument->expression_index >= result->written.expressions ||
            argument->resolved_parameter_ordinal == W_SEED_FRONTEND_NONE ||
            argument->resolved_parameter_ordinal >= parameter_count ||
            !frontend_span_ok(&input->frontend_input->documents[document_index],
                              argument->span))
          return false;
        for (size_t prior = 0u; prior < argument_ordinal; prior += 1u) {
          const w_seed_frontend_argument *prior_argument =
              &output->arguments[(size_t)call->first_argument + prior];
          if (prior_argument->resolved_parameter_ordinal ==
              argument->resolved_parameter_ordinal)
            return false;
        }
        const w_seed_frontend_expression *value =
            &output->expressions[argument->expression_index];
        uint32_t expected_type = W_SEED_FRONTEND_NONE;
        if (host_call) {
          const w_seed_frontend_external_parameter *host_parameter =
              &host->parameters[argument->resolved_parameter_ordinal];
          if (!text_is(host_parameter->type, HIR0_STRING_NAME) ||
              !frontend_host_label_matches(host_parameter->label_kind,
                                           host_parameter->name,
                                           argument->label))
            return false;
        } else {
          const size_t parameter_index =
              (size_t)target_function->first_parameter +
              argument->resolved_parameter_ordinal;
          if (parameter_index >= result->written.parameters)
            return false;
          const w_seed_frontend_parameter *parameter =
              &output->parameters[parameter_index];
          if (!frontend_parameter_label_matches(parameter, argument->label))
            return false;
          expected_type = parameter->type_index;
        }
        const bool host_string_value = frontend_string_value_root(
            output, result, argument->expression_index, 0u);
        if ((local_call &&
             (expected_type == W_SEED_FRONTEND_NONE ||
              value->inferred_type == W_SEED_FRONTEND_NONE ||
              !frontend_supported_types_equal(
                  &output->types[value->inferred_type],
                  &output->types[expected_type]))) ||
            (host_call && !host_string_value) ||
            !frontend_value_tree_ok(
                input, module_index, function, document_index,
                statement_index,
                argument->expression_index, 0u, &expression_cursor,
                &interpolation_segment_cursor, &const_byte_cursor, &values,
                &segments, &bytes))
          return false;
        if (!add_size(args, 1u, &args)) return false;
      }
      if ((size_t)statement->expression_index != expression_cursor)
        return false;
      if (!add_size(calls, 1u, &calls) || !add_size(expression_cursor, 1u,
                                                   &expression_cursor))
        return false;
    }
    const w_seed_frontend_type_kind return_kind =
        output->types[owner->return_type].kind;
    const bool has_value_return =
        owner->statement_count != 0u &&
        output->statements[(size_t)owner->first_statement +
                           owner->statement_count - 1u]
                .kind == W_SEED_FRONTEND_STMT_RETURN;
    if ((return_kind == W_SEED_FRONTEND_TYPE_UNIT && has_value_return) ||
        (return_kind != W_SEED_FRONTEND_TYPE_UNIT && !has_value_return))
      return false;
  }
  if (args != result->written.arguments ||
      expression_cursor != result->written.expressions ||
      interpolation_segment_cursor !=
          result->written.interpolation_segments ||
      const_byte_cursor != result->written.const_bytes)
    return false;
  if (!count_u32(bindings) || !count_u32(calls) || !count_u32(values) ||
      !count_u32(segments))
    return false;
  *binding_total = bindings;
  *call_total = calls;
  *argument_total = args;
  *value_total = values;
  *segment_total = segments;
  *value_bytes = bytes;
  *text_bytes = 0u;
  return true;
}

static bool add_text_size(w_seed_frontend_text text, size_t *total) {
  return text_valid(text) && add_size(*total, text.length, total);
}

static bool text_size_for_input(const w_seed_hir0_input *input, size_t *total) {
  if (input == NULL || total == NULL) return false;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_result *result = input->frontend_result;
  size_t value = 15u; /* canonical Unit, String, i64, and Bool names */
  for (size_t index = 0u; index < result->written.modules; index += 1u) {
    const w_seed_frontend_module *module = &output->modules[index];
    if (!add_text_size(module->source_id, &value) ||
        !add_text_size(module->module_id, &value) ||
        !add_text_size(module->local_module_name, &value))
      return false;
  }
  for (size_t index = 0u; index < result->written.functions; index += 1u)
    if (!add_text_size(output->functions[index].name, &value)) return false;
  for (size_t index = 0u; index < result->written.parameters; index += 1u)
    if (!add_text_size(output->parameters[index].name, &value) ||
        !add_text_size(output->parameters[index].label, &value))
      return false;
  for (size_t index = 0u; index < result->written.statements; index += 1u)
    if (output->statements[index].kind == W_SEED_FRONTEND_STMT_LET &&
        !add_text_size(output->statements[index].binding_name, &value))
      return false;
  for (size_t index = 0u; index < result->written.entries; index += 1u)
    if (!add_text_size(output->entries[index].target, &value) ||
        !add_text_size((w_seed_frontend_text){HIR0_SLOT_NAME,
                                              sizeof(HIR0_SLOT_NAME) - 1u},
                       &value))
      return false;
  const w_seed_frontend_host_prelude *scope = input->frontend_input->host_scope;
  for (size_t index = 0u; index < scope->symbol_count; index += 1u) {
    const w_seed_frontend_host_prelude_symbol *symbol = &scope->symbols[index];
    if (!add_text_size(symbol->name, &value) ||
        !add_text_size(scope->profile, &value)) return false;
    for (size_t parameter = 0u; parameter < symbol->parameter_count; parameter += 1u) {
      const w_seed_frontend_external_parameter *host_parameter =
          &symbol->parameters[parameter];
      if (!add_text_size(host_parameter->name, &value) ||
          (host_parameter->label_kind !=
               W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
           !add_text_size(host_parameter->name, &value)))
        return false;
    }
    for (size_t requirement = 0u; requirement < symbol->requirement_count;
         requirement += 1u)
      if (!add_text_size(symbol->requirements[requirement].name, &value)) return false;
  }
  /* Labels are copied once per frontend argument because they are call facts. */
  for (size_t index = 0u; index < result->written.arguments; index += 1u)
    if (!add_text_size(output->arguments[index].label, &value)) return false;
  return count_u32(value) && value <= W_SEED_HIR0_MAX_TEXT_BYTES
             ? (*total = value, true)
             : false;
}

static hir0_prepare_status collect(const w_seed_hir0_input *input,
                                   w_seed_hir0_counts *counts) {
  if (counts == NULL) return HIR0_PREPARE_INVALID;
  (void)memset(counts, 0, sizeof(*counts));
  if (!frontend_shape_ok(input)) return HIR0_PREPARE_FRONTEND;
  const w_seed_frontend_result *frontend_result = input->frontend_result;
  if (!frontend_sources_ok(input)) return HIR0_PREPARE_INVALID;
  if (!host_shape_ok(input->frontend_input->host_scope))
    return HIR0_PREPARE_INVALID;
  if (!frontend_type_records_ok(input) || !frontend_module_ranges_ok(input) ||
      !frontend_function_ranges_ok(input) || !frontend_entry_records_ok(input) ||
      !frontend_symbol_records_ok(input))
    return HIR0_PREPARE_INVALID;
  /* HIR0 is a deliberately small, but real, linear-call subset. */
  /* HIR0 is intentionally closed. Every frontend family not represented by
   * an HIR0 record is an explicit barrier, including append-only families. */
  if (frontend_result->written.imports != 0u ||
      frontend_result->written.import_items != 0u ||
      frontend_result->written.structs != 0u ||
      frontend_result->written.fields != 0u ||
      frontend_result->written.type_declarations != 0u ||
      frontend_result->written.aliases != 0u || frontend_result->written.enums != 0u ||
      frontend_result->written.facts != 0u ||
      frontend_result->written.diagnostics != 0u ||
      frontend_result->written.diagnostic_facts != 0u ||
      frontend_result->written.diagnostic_items != 0u ||
      frontend_result->written.diagnostic_labels != 0u ||
      frontend_result->written.enum_cases != 0u ||
      frontend_result->written.enum_case_parameters != 0u ||
      frontend_result->written.switch_arms != 0u ||
      frontend_result->written.enum_subset_members != 0u ||
      frontend_result->written.enum_membership_cases != 0u ||
      frontend_result->written.generic_parameters != 0u ||
      frontend_result->written.generic_applications != 0u ||
      frontend_result->written.generic_arguments != 0u ||
      frontend_result->written.typed_const_expressions != 0u ||
      frontend_result->written.const_values != 0u ||
      frontend_result->written.const_elements != 0u ||
      frontend_result->written.const_declarations != 0u ||
      frontend_result->written.parameters > W_SEED_HIR0_MAX_TEXT_BYTES)
    return HIR0_PREPARE_UNSUPPORTED;
  size_t binding_count = 0u;
  size_t call_count = 0u;
  size_t argument_count = 0u;
  size_t value_count = 0u;
  size_t interpolation_segment_count = 0u;
  size_t value_bytes = 0u;
  size_t ignored_text = 0u;
  if (!frontend_statement_and_expression_ok(
          input, &binding_count, &call_count, &argument_count, &value_count,
          &interpolation_segment_count, &value_bytes, &ignored_text))
    return HIR0_PREPARE_UNSUPPORTED;
  size_t text_bytes = 0u;
  if (!text_size_for_input(input, &text_bytes)) return HIR0_PREPARE_UNSUPPORTED;
  const size_t modules = frontend_result->written.modules;
  const size_t functions = frontend_result->written.functions;
  const size_t entries = frontend_result->written.entries;
  const size_t host_symbols = input->frontend_input->host_scope->symbol_count;
  size_t identities = 0u;
  if (!add_size(modules, functions, &identities) ||
      !add_size(identities, entries, &identities) ||
      !add_size(identities, host_symbols, &identities) ||
      !count_u32(identities) || !count_u32(functions) || !count_u32(entries) ||
      !count_u32(binding_count) || !count_u32(call_count) ||
      !count_u32(argument_count) || !count_u32(value_count) ||
      !count_u32(interpolation_segment_count) || !count_u32(value_bytes))
    return HIR0_PREPARE_UNSUPPORTED;
  counts->modules = modules;
  counts->identities = identities;
  counts->types = 4u;
  counts->functions = functions;
  counts->parameters = frontend_result->written.parameters;
  counts->blocks = functions;
  counts->bindings = binding_count;
  if (!add_size(binding_count, call_count, &counts->instructions))
    return HIR0_PREPARE_UNSUPPORTED;
  if (!count_u32(counts->instructions)) return HIR0_PREPARE_UNSUPPORTED;
  counts->calls = call_count;
  size_t host_parameters = 0u;
  size_t requirements = 0u;
  for (size_t index = 0u; index < host_symbols; index += 1u) {
    if (!add_size(host_parameters,
                  input->frontend_input->host_scope->symbols[index]
                      .parameter_count,
                  &host_parameters) ||
        !add_size(requirements,
                  input->frontend_input->host_scope->symbols[index]
                      .requirement_count,
                  &requirements))
      return HIR0_PREPARE_UNSUPPORTED;
  }
  counts->host_parameters = host_parameters;
  counts->arguments = argument_count;
  counts->requirements = requirements;
  counts->values = value_count;
  counts->interpolation_segments = interpolation_segment_count;
  counts->terminators = functions;
  counts->entries = entries;
  counts->text_bytes = text_bytes;
  counts->value_bytes = value_bytes;
  counts->receipt_bytes = HIR0_RECEIPT_BYTES;
  if (text_bytes > W_SEED_HIR0_MAX_TEXT_BYTES ||
      value_bytes > W_SEED_HIR0_MAX_VALUE_BYTES ||
      host_parameters > UINT32_MAX || requirements > UINT32_MAX)
    return HIR0_PREPARE_UNSUPPORTED;
  return HIR0_PREPARE_READY;
}

static bool output_capacity_ok(const w_seed_hir0_output *output,
                               const w_seed_hir0_counts *counts) {
  if (output == NULL || counts == NULL) return false;
#define HIR0_OUTPUT(field, capacity_field)                                     \
  if (counts->field > output->capacity_field ||                               \
      (counts->field != 0u && output->field == NULL)) return false
  HIR0_OUTPUT(modules, module_capacity);
  HIR0_OUTPUT(identities, identity_capacity);
  HIR0_OUTPUT(types, type_capacity);
  HIR0_OUTPUT(functions, function_capacity);
  HIR0_OUTPUT(parameters, parameter_capacity);
  HIR0_OUTPUT(blocks, block_capacity);
  HIR0_OUTPUT(instructions, instruction_capacity);
  HIR0_OUTPUT(bindings, binding_capacity);
  HIR0_OUTPUT(calls, call_capacity);
  HIR0_OUTPUT(host_parameters, host_parameter_capacity);
  HIR0_OUTPUT(arguments, argument_capacity);
  HIR0_OUTPUT(requirements, requirement_capacity);
  HIR0_OUTPUT(values, value_capacity);
  HIR0_OUTPUT(interpolation_segments, interpolation_segment_capacity);
  HIR0_OUTPUT(terminators, terminator_capacity);
  HIR0_OUTPUT(entries, entry_capacity);
#undef HIR0_OUTPUT
  return counts->text_bytes <= output->text_byte_capacity &&
         (counts->text_bytes == 0u || output->text_bytes != NULL) &&
         counts->value_bytes <= output->value_byte_capacity &&
         (counts->value_bytes == 0u || output->value_bytes != NULL) &&
         counts->receipt_bytes <= output->receipt_capacity &&
         (counts->receipt_bytes == 0u || output->receipt != NULL);
}

static bool pointer_range(const void *pointer, size_t bytes, uintptr_t *start,
                          uintptr_t *end) {
  if (start == NULL || end == NULL) return false;
  if (pointer == NULL || bytes == 0u) {
    *start = 0u;
    *end = 0u;
    return true;
  }
  *start = (uintptr_t)pointer;
  if (bytes > UINTPTR_MAX - *start) return false;
  *end = *start + (uintptr_t)bytes;
  return true;
}

static bool ranges_overlap(const void *left, size_t left_bytes,
                           const void *right, size_t right_bytes) {
  uintptr_t left_start = 0u;
  uintptr_t left_end = 0u;
  uintptr_t right_start = 0u;
  uintptr_t right_end = 0u;
  if (left == NULL || right == NULL || left_bytes == 0u || right_bytes == 0u)
    return false;
  if (!pointer_range(left, left_bytes, &left_start, &left_end) ||
      !pointer_range(right, right_bytes, &right_start, &right_end))
    return true;
  return left_start < right_end && right_start < left_end;
}

typedef struct {
  const void *pointer;
  size_t bytes;
} hir0_memory_range;

static bool range_table_add(hir0_memory_range *ranges, size_t *count,
                            size_t capacity, const void *pointer,
                            size_t elements, size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity ||
      (elements != 0u && pointer == NULL) ||
      (elements > SIZE_MAX / element_size))
    return false;
  const size_t bytes = elements * element_size;
  uintptr_t start = 0u;
  uintptr_t end = 0u;
  if (!pointer_range(pointer, bytes, &start, &end)) return false;
  (void)start;
  (void)end;
  ranges[*count] = (hir0_memory_range){pointer, bytes};
  *count += 1u;
  return true;
}

static bool output_range_table(const w_seed_hir0_output *output,
                               hir0_memory_range *ranges, size_t *count) {
  if (output == NULL || ranges == NULL || count == NULL) return false;
  *count = 0u;
#define HIR0_ADD_OUTPUT(field, capacity_field, type)                          \
  if (!range_table_add(ranges, count, 32u, output->field,                    \
                       output->capacity_field, sizeof(type))) return false
  HIR0_ADD_OUTPUT(modules, module_capacity, w_seed_hir0_module);
  HIR0_ADD_OUTPUT(identities, identity_capacity, w_seed_hir0_identity);
  HIR0_ADD_OUTPUT(types, type_capacity, w_seed_hir0_type);
  HIR0_ADD_OUTPUT(functions, function_capacity, w_seed_hir0_function);
  HIR0_ADD_OUTPUT(parameters, parameter_capacity, w_seed_hir0_parameter);
  HIR0_ADD_OUTPUT(blocks, block_capacity, w_seed_hir0_block);
  HIR0_ADD_OUTPUT(instructions, instruction_capacity, w_seed_hir0_instruction);
  HIR0_ADD_OUTPUT(bindings, binding_capacity, w_seed_hir0_binding);
  HIR0_ADD_OUTPUT(calls, call_capacity, w_seed_hir0_call);
  HIR0_ADD_OUTPUT(host_parameters, host_parameter_capacity,
                  w_seed_hir0_host_parameter);
  HIR0_ADD_OUTPUT(arguments, argument_capacity, w_seed_hir0_argument);
  HIR0_ADD_OUTPUT(requirements, requirement_capacity, w_seed_hir0_requirement);
  HIR0_ADD_OUTPUT(values, value_capacity, w_seed_hir0_value);
  HIR0_ADD_OUTPUT(interpolation_segments, interpolation_segment_capacity,
                  w_seed_hir0_interpolation_segment);
  HIR0_ADD_OUTPUT(terminators, terminator_capacity, w_seed_hir0_terminator);
  HIR0_ADD_OUTPUT(entries, entry_capacity, w_seed_hir0_entry);
#undef HIR0_ADD_OUTPUT
  if (!range_table_add(ranges, count, 32u, output->text_bytes,
                       output->text_byte_capacity, sizeof(uint8_t)) ||
      !range_table_add(ranges, count, 32u, output->value_bytes,
                       output->value_byte_capacity, sizeof(uint8_t)) ||
      !range_table_add(ranges, count, 32u, output->receipt,
                       output->receipt_capacity, sizeof(uint8_t)))
    return false;
  return true;
}

static bool output_aliases(const w_seed_hir0_output *output,
                           const w_seed_hir0_counts *counts) {
  (void)counts;
  hir0_memory_range ranges[32];
  size_t count = 0u;
  if (!output_range_table(output, ranges, &count)) return true;
  for (size_t first = 0u; first < count; first += 1u)
    for (size_t second = first + 1u; second < count; second += 1u)
      if (ranges_overlap(ranges[first].pointer, ranges[first].bytes,
                         ranges[second].pointer, ranges[second].bytes))
        return true;
  return false;
}

static bool output_overlaps_memory(const hir0_memory_range *outputs,
                                   size_t output_count, const void *pointer,
                                   size_t bytes) {
  if (bytes != 0u && pointer == NULL) return true;
  for (size_t index = 0u; index < output_count; index += 1u)
    if (ranges_overlap(outputs[index].pointer, outputs[index].bytes, pointer,
                       bytes))
      return true;
  return false;
}

static bool output_overlaps_elements(const hir0_memory_range *outputs,
                                     size_t output_count, const void *pointer,
                                     size_t elements, size_t element_size) {
  if (elements > SIZE_MAX / element_size) return true;
  return output_overlaps_memory(outputs, output_count, pointer,
                                elements * element_size);
}

static bool output_overlaps_frontend_text(const hir0_memory_range *outputs,
                                          size_t output_count,
                                          w_seed_frontend_text text) {
  if (!text_valid(text)) return true;
  return output_overlaps_memory(outputs, output_count, text.data, text.length);
}

/* Lowering reads every range below before the first output write. Rejecting
 * all output/input overlap makes that read set stable through the commit. */
static bool output_overlaps_input(const w_seed_hir0_input *input,
                                  const w_seed_hir0_output *output,
                                  const w_seed_hir0_result *result) {
  if (input == NULL || output == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL ||
      result == NULL)
    return true;
  hir0_memory_range outputs[33];
  size_t output_count = 0u;
  if (!output_range_table(output, outputs, &output_count)) return true;
  if (output_count >= 33u) return true;
  if (output_overlaps_memory(outputs, output_count, result, sizeof(*result)))
    return true;
  outputs[output_count++] = (hir0_memory_range){result, sizeof(*result)};
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  const w_seed_frontend_output *frontend = input->frontend_output;
  const w_seed_frontend_result *frontend_result = input->frontend_result;
  if (output_overlaps_memory(outputs, output_count, output, sizeof(*output)) ||
      output_overlaps_memory(outputs, output_count, input, sizeof(*input)) ||
      output_overlaps_memory(outputs, output_count, frontend_input,
                             sizeof(*frontend_input)) ||
      output_overlaps_memory(outputs, output_count, frontend, sizeof(*frontend)) ||
      output_overlaps_memory(outputs, output_count, frontend_result,
                             sizeof(*frontend_result))) {
    return true;
  }
#define HIR0_INPUT_RANGE(field, type)                                         \
  if (output_overlaps_elements(outputs, output_count, frontend->field,        \
                                frontend_result->written.field, sizeof(type))) { \
    return true; \
  }
  HIR0_INPUT_RANGE(modules, w_seed_frontend_module);
  HIR0_INPUT_RANGE(imports, w_seed_frontend_import);
  HIR0_INPUT_RANGE(import_items, w_seed_frontend_import_item);
  HIR0_INPUT_RANGE(structs, w_seed_frontend_struct);
  HIR0_INPUT_RANGE(fields, w_seed_frontend_field);
  HIR0_INPUT_RANGE(type_declarations, w_seed_frontend_type_declaration);
  HIR0_INPUT_RANGE(aliases, w_seed_frontend_alias);
  HIR0_INPUT_RANGE(types, w_seed_frontend_type);
  HIR0_INPUT_RANGE(functions, w_seed_frontend_function);
  HIR0_INPUT_RANGE(parameters, w_seed_frontend_parameter);
  HIR0_INPUT_RANGE(arguments, w_seed_frontend_argument);
  HIR0_INPUT_RANGE(entries, w_seed_frontend_entry);
  HIR0_INPUT_RANGE(statements, w_seed_frontend_statement);
  HIR0_INPUT_RANGE(expressions, w_seed_frontend_expression);
  HIR0_INPUT_RANGE(interpolation_segments,
                   w_seed_frontend_interpolation_segment);
  HIR0_INPUT_RANGE(symbols, w_seed_frontend_symbol);
  HIR0_INPUT_RANGE(facts, w_seed_frontend_fact);
  HIR0_INPUT_RANGE(diagnostics, w_seed_frontend_diagnostic);
  HIR0_INPUT_RANGE(diagnostic_facts, w_seed_frontend_diagnostic_fact);
  HIR0_INPUT_RANGE(diagnostic_items, w_seed_frontend_diagnostic_item);
  HIR0_INPUT_RANGE(diagnostic_labels, w_seed_frontend_diagnostic_label);
  HIR0_INPUT_RANGE(enums, w_seed_frontend_enum);
  HIR0_INPUT_RANGE(enum_cases, w_seed_frontend_enum_case);
  HIR0_INPUT_RANGE(enum_case_parameters, w_seed_frontend_enum_case_parameter);
  HIR0_INPUT_RANGE(const_declarations, w_seed_frontend_const_declaration);
  HIR0_INPUT_RANGE(switch_arms, w_seed_frontend_switch_arm);
  HIR0_INPUT_RANGE(enum_subset_members, w_seed_frontend_enum_subset_member);
  HIR0_INPUT_RANGE(enum_membership_cases, w_seed_frontend_enum_membership_case);
  HIR0_INPUT_RANGE(generic_parameters, w_seed_frontend_generic_parameter);
  HIR0_INPUT_RANGE(generic_applications, w_seed_frontend_generic_application);
  HIR0_INPUT_RANGE(generic_arguments, w_seed_frontend_generic_argument);
  HIR0_INPUT_RANGE(typed_const_expressions,
                   w_seed_frontend_typed_const_expression);
  HIR0_INPUT_RANGE(const_values, w_seed_frontend_const_value);
  HIR0_INPUT_RANGE(const_elements, w_seed_frontend_const_element);
#undef HIR0_INPUT_RANGE
  if (output_overlaps_memory(outputs, output_count, frontend->receipt,
                              frontend_result->written.receipt_bytes) ||
      output_overlaps_memory(outputs, output_count, frontend->const_bytes,
                              frontend_result->written.const_bytes))
    return true;
  if (output_overlaps_elements(outputs, output_count, frontend_input->documents,
                               frontend_input->document_count,
                               sizeof(w_seed_frontend_document)) ||
      output_overlaps_elements(outputs, output_count,
                               frontend_input->external_modules,
                               frontend_input->external_module_count,
                               sizeof(w_seed_frontend_external_module)) ||
      output_overlaps_elements(outputs, output_count,
                               frontend_input->resolved_imports,
                               frontend_input->resolved_import_count,
                               sizeof(w_seed_frontend_resolved_import)) ||
      output_overlaps_memory(outputs, output_count, frontend_input->host_scope,
                             sizeof(*frontend_input->host_scope)))
    return true;
  for (size_t document = 0u; document < frontend_input->document_count;
       document += 1u) {
    const w_seed_frontend_document *value = &frontend_input->documents[document];
    if (output_overlaps_memory(outputs, output_count, value->source,
                               sizeof(*value->source)) ||
        output_overlaps_elements(outputs, output_count, value->nodes,
                                 value->node_count, sizeof(*value->nodes)) ||
        (value->source != NULL &&
         output_overlaps_memory(outputs, output_count,
                                value->source->bytes.data,
                                value->source->bytes.length)) ||
        output_overlaps_frontend_text(outputs, output_count,
                                      value->logical_source_id) ||
        output_overlaps_frontend_text(outputs, output_count, value->module_id) ||
        output_overlaps_frontend_text(outputs, output_count,
                                      value->local_module_name))
      return true;
  }
  if (frontend_input->host_scope != NULL) {
    const w_seed_frontend_host_prelude *scope = frontend_input->host_scope;
    if (output_overlaps_frontend_text(outputs, output_count, scope->profile) ||
        output_overlaps_elements(outputs, output_count, scope->symbols,
                                 scope->symbol_count,
                                 sizeof(*scope->symbols)))
      return true;
    for (size_t symbol = 0u; symbol < scope->symbol_count; symbol += 1u) {
      const w_seed_frontend_host_prelude_symbol *value = &scope->symbols[symbol];
      if (output_overlaps_frontend_text(outputs, output_count, value->name) ||
          output_overlaps_frontend_text(outputs, output_count,
                                        value->return_type) ||
          output_overlaps_elements(outputs, output_count, value->parameters,
                                   value->parameter_count,
                                   sizeof(*value->parameters)) ||
          output_overlaps_elements(outputs, output_count, value->requirements,
                                   value->requirement_count,
                                   sizeof(*value->requirements)))
        return true;
      for (size_t parameter = 0u; parameter < value->parameter_count;
           parameter += 1u) {
        if (output_overlaps_frontend_text(
                outputs, output_count, value->parameters[parameter].name) ||
            output_overlaps_frontend_text(
                outputs, output_count, value->parameters[parameter].type))
          return true;
      }
      for (size_t requirement = 0u; requirement < value->requirement_count;
           requirement += 1u)
        if (output_overlaps_frontend_text(
                outputs, output_count, value->requirements[requirement].name))
          return true;
    }
  }
  for (size_t module = 0u; module < frontend_result->written.modules; module += 1u) {
    const w_seed_frontend_module *value = &frontend->modules[module];
    if (output_overlaps_frontend_text(outputs, output_count, value->source_id) ||
        output_overlaps_frontend_text(outputs, output_count, value->module_id) ||
        output_overlaps_frontend_text(outputs, output_count,
                                      value->local_module_name))
      return true;
  }
  for (size_t function = 0u; function < frontend_result->written.functions; function += 1u)
    if (output_overlaps_frontend_text(outputs, output_count,
                                      frontend->functions[function].name))
      return true;
  for (size_t parameter = 0u; parameter < frontend_result->written.parameters;
       parameter += 1u)
    if (output_overlaps_frontend_text(outputs, output_count,
                                      frontend->parameters[parameter].name) ||
        output_overlaps_frontend_text(outputs, output_count,
                                      frontend->parameters[parameter].label))
      return true;
  for (size_t entry = 0u; entry < frontend_result->written.entries; entry += 1u)
    if (output_overlaps_frontend_text(outputs, output_count,
                                      frontend->entries[entry].target))
      return true;
  for (size_t argument = 0u; argument < frontend_result->written.arguments; argument += 1u)
    if (output_overlaps_frontend_text(outputs, output_count,
                                      frontend->arguments[argument].label))
      return true;
  for (size_t statement = 0u;
       statement < frontend_result->written.statements; statement += 1u)
    if (output_overlaps_frontend_text(
            outputs, output_count, frontend->statements[statement].binding_name))
      return true;
  for (size_t expression = 0u; expression < frontend_result->written.expressions;
       expression += 1u) {
    const w_seed_frontend_expression *value = &frontend->expressions[expression];
    if (output_overlaps_frontend_text(outputs, output_count, value->spelling) ||
        output_overlaps_frontend_text(outputs, output_count,
                                      value->operator_text) ||
        output_overlaps_frontend_text(outputs, output_count,
                                      value->member_name))
      return true;
  }
  return false;
}

static bool program_range_table(const w_seed_hir0_program *program,
                                hir0_memory_range *ranges, size_t *count) {
  if (program == NULL || ranges == NULL || count == NULL) return false;
  *count = 0u;
#define HIR0_ADD_PROGRAM(field, capacity_field, type)                         \
  if (!range_table_add(ranges, count, 32u, program->field,                   \
                       program->capacity_field, sizeof(type))) return false
  HIR0_ADD_PROGRAM(modules, module_capacity, w_seed_hir0_module);
  HIR0_ADD_PROGRAM(identities, identity_capacity, w_seed_hir0_identity);
  HIR0_ADD_PROGRAM(types, type_capacity, w_seed_hir0_type);
  HIR0_ADD_PROGRAM(functions, function_capacity, w_seed_hir0_function);
  HIR0_ADD_PROGRAM(parameters, parameter_capacity, w_seed_hir0_parameter);
  HIR0_ADD_PROGRAM(blocks, block_capacity, w_seed_hir0_block);
  HIR0_ADD_PROGRAM(instructions, instruction_capacity, w_seed_hir0_instruction);
  HIR0_ADD_PROGRAM(bindings, binding_capacity, w_seed_hir0_binding);
  HIR0_ADD_PROGRAM(calls, call_capacity, w_seed_hir0_call);
  HIR0_ADD_PROGRAM(host_parameters, host_parameter_capacity,
                   w_seed_hir0_host_parameter);
  HIR0_ADD_PROGRAM(arguments, argument_capacity, w_seed_hir0_argument);
  HIR0_ADD_PROGRAM(requirements, requirement_capacity, w_seed_hir0_requirement);
  HIR0_ADD_PROGRAM(values, value_capacity, w_seed_hir0_value);
  HIR0_ADD_PROGRAM(interpolation_segments, interpolation_segment_capacity,
                   w_seed_hir0_interpolation_segment);
  HIR0_ADD_PROGRAM(terminators, terminator_capacity, w_seed_hir0_terminator);
  HIR0_ADD_PROGRAM(entries, entry_capacity, w_seed_hir0_entry);
#undef HIR0_ADD_PROGRAM
  if (!range_table_add(ranges, count, 32u, program->text_bytes,
                       program->text_byte_capacity, sizeof(uint8_t)) ||
      !range_table_add(ranges, count, 32u, program->value_bytes,
                       program->value_byte_capacity, sizeof(uint8_t)) ||
      !range_table_add(ranges, count, 32u, program->receipt,
                       program->receipt_capacity, sizeof(uint8_t)))
    return false;
  return true;
}

/* collect() proves text_size_for_input() and all individual source slices.
 * These helpers therefore have no failure path after the output commit point. */
static void append_text_unchecked(const w_seed_frontend_text source,
                                  uint8_t *buffer, size_t *offset,
                                  w_seed_hir0_text *out) {
  out->offset = (uint32_t)*offset;
  out->count = (uint32_t)source.length;
  if (source.length != 0u)
    (void)memcpy(buffer + *offset, source.data, source.length);
  *offset += source.length;
}

static void append_bytes_unchecked(const uint8_t *source, size_t count,
                                   uint8_t *buffer, size_t *offset,
                                   uint32_t *out_offset, uint32_t *out_count) {
  *out_offset = (uint32_t)*offset;
  *out_count = (uint32_t)count;
  if (count != 0u)
    (void)memcpy(buffer + *offset, source, count);
  *offset += count;
}

static uint32_t hir_type_from_frontend(const w_seed_frontend_output *output,
                                       const w_seed_frontend_result *result,
                                       uint32_t frontend_type) {
  if (frontend_type == W_SEED_FRONTEND_NONE || output == NULL || result == NULL ||
      (size_t)frontend_type >= result->written.types || output->types == NULL)
    return W_SEED_HIR0_NONE;
  const w_seed_frontend_type *type = &output->types[frontend_type];
  if (type->kind == W_SEED_FRONTEND_TYPE_UNIT) return 0u;
  if (type->kind == W_SEED_FRONTEND_TYPE_STRING) return 1u;
  if (type->kind == W_SEED_FRONTEND_TYPE_INTEGER && type->is_signed &&
      type->bit_width == 64u)
    return 2u;
  if (type->kind == W_SEED_FRONTEND_TYPE_BOOL) return 3u;
  return W_SEED_HIR0_NONE;
}

static uint32_t hir_host_identity_index(const w_seed_hir0_counts *counts,
                                        size_t host_index) {
  return (uint32_t)(counts->modules + counts->functions + counts->entries +
                    host_index);
}

static bool function_for_entry(const w_seed_frontend_output *output,
                               size_t function_count, size_t module_index,
                               w_seed_frontend_text target, size_t *out) {
  if (output == NULL || out == NULL) return false;
  for (size_t function = 0u; function < function_count; function += 1u)
    if (output->functions[function].module_index == module_index &&
        text_equal(output->functions[function].name, target)) {
      *out = function;
      return true;
    }
  return false;
}

static bool binding_index_for_statement(
    const w_seed_frontend_output *output,
    const w_seed_frontend_result *result, size_t function,
    size_t use_statement, uint32_t target_statement, uint32_t *out) {
  if (output == NULL || result == NULL || out == NULL ||
      (size_t)target_statement >= result->written.statements ||
      (size_t)target_statement >= use_statement)
    return false;
  size_t binding = 0u;
  for (size_t index = 0u; index < result->written.statements; index += 1u) {
    const w_seed_frontend_statement *statement = &output->statements[index];
    if (statement->kind != W_SEED_FRONTEND_STMT_LET) continue;
    if (index == (size_t)target_statement) {
      if (statement->owner_function != function || !count_u32(binding))
        return false;
      *out = (uint32_t)binding;
      return true;
    }
    if (!add_size(binding, 1u, &binding)) return false;
  }
  return false;
}

static w_seed_hir0_binary_operator hir_binary_operator(
    w_seed_frontend_text text) {
  if (text_is(text, "+")) return W_SEED_HIR0_BINARY_ADD;
  if (text_is(text, "-")) return W_SEED_HIR0_BINARY_SUBTRACT;
  if (text_is(text, "*")) return W_SEED_HIR0_BINARY_MULTIPLY;
  if (text_is(text, "/")) return W_SEED_HIR0_BINARY_DIVIDE;
  return W_SEED_HIR0_BINARY_REMAINDER;
}

/* collect() proves every branch and capacity. Emission therefore has no
 * failure path after the transaction commit point. Values are appended in
 * postorder, so every edge points backward and verification stays linear. */
static uint32_t emit_value_tree_unchecked(
    const w_seed_frontend_output *frontend,
    const w_seed_frontend_result *frontend_result, size_t function,
    size_t statement_index, uint32_t expression_index,
    w_seed_hir0_value_owner_kind owner_kind, uint32_t owner_index,
    uint32_t owner_ordinal, w_seed_hir0_output *output, size_t *value_cursor,
    size_t *segment_cursor, size_t *value_byte_cursor) {
  const w_seed_frontend_expression *source =
      &frontend->expressions[expression_index];
  if (source->kind == W_SEED_FRONTEND_EXPR_PARENTHESIS)
    return emit_value_tree_unchecked(
        frontend, frontend_result, function, statement_index, source->left,
        owner_kind, owner_index, owner_ordinal, output, value_cursor,
        segment_cursor, value_byte_cursor);

  uint32_t left = W_SEED_HIR0_NONE;
  uint32_t right = W_SEED_HIR0_NONE;
  uint32_t first_segment = W_SEED_HIR0_NONE;
  if (source->kind == W_SEED_FRONTEND_EXPR_BINARY) {
    left = emit_value_tree_unchecked(
        frontend, frontend_result, function, statement_index, source->left,
        W_SEED_HIR0_VALUE_OWNER_BINARY, W_SEED_HIR0_NONE, 0u, output,
        value_cursor, segment_cursor, value_byte_cursor);
    right = emit_value_tree_unchecked(
        frontend, frontend_result, function, statement_index, source->right,
        W_SEED_HIR0_VALUE_OWNER_BINARY, W_SEED_HIR0_NONE, 1u, output,
        value_cursor, segment_cursor, value_byte_cursor);
  } else if (source->kind == W_SEED_FRONTEND_EXPR_INTERPOLATED_STRING) {
    first_segment = (uint32_t)*segment_cursor;
    for (size_t ordinal = 0u; ordinal < source->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_frontend_interpolation_segment *source_segment =
          &frontend->interpolation_segments[
              (size_t)source->first_interpolation_segment + ordinal];
      const uint32_t target_segment_index = (uint32_t)*segment_cursor;
      w_seed_hir0_interpolation_segment *target_segment =
          &output->interpolation_segments[*segment_cursor];
      *target_segment = (w_seed_hir0_interpolation_segment){
          .kind = source_segment->kind == W_SEED_FRONTEND_INTERPOLATION_TEXT
                      ? W_SEED_HIR0_INTERPOLATION_TEXT
                      : W_SEED_HIR0_INTERPOLATION_VALUE,
          .owner_value = W_SEED_HIR0_NONE,
          .ordinal = (uint32_t)ordinal,
          .value_index = W_SEED_HIR0_NONE,
          .byte_offset = 0u,
          .byte_count = 0u,
          .source_span = source_segment->span};
      *segment_cursor += 1u;
      if (source_segment->kind == W_SEED_FRONTEND_INTERPOLATION_TEXT) {
        const uint8_t *bytes =
            source_segment->const_byte_count == 0u
                ? NULL
                : frontend->const_bytes + source_segment->const_byte_offset;
        append_bytes_unchecked(bytes, source_segment->const_byte_count,
                               output->value_bytes, value_byte_cursor,
                               &target_segment->byte_offset,
                               &target_segment->byte_count);
      } else {
        target_segment->value_index = emit_value_tree_unchecked(
            frontend, frontend_result, function, statement_index,
            source_segment->expression_index,
            W_SEED_HIR0_VALUE_OWNER_INTERPOLATION_SEGMENT,
            target_segment_index, 0u, output, value_cursor, segment_cursor,
            value_byte_cursor);
      }
    }
  }

  const uint32_t result = (uint32_t)*value_cursor;
  w_seed_hir0_value *target = &output->values[*value_cursor];
  *target = (w_seed_hir0_value){
      .kind = W_SEED_HIR0_VALUE_CONST_STRING,
      .owner_kind = owner_kind,
      .owner_index = owner_index,
      .owner_ordinal = owner_ordinal,
      .type_index = 1u,
      .binding_index = W_SEED_HIR0_NONE,
      .parameter_index = W_SEED_HIR0_NONE,
      .call_index = W_SEED_HIR0_NONE,
      .left_value = W_SEED_HIR0_NONE,
      .right_value = W_SEED_HIR0_NONE,
      .first_interpolation_segment = W_SEED_HIR0_NONE,
      .interpolation_segment_count = 0u,
      .binary_operator = W_SEED_HIR0_BINARY_ADD,
      .integer_value = 0,
      .bool_value = false,
      .byte_offset = 0u,
      .byte_count = 0u,
      .source_span = source->span};
  *value_cursor += 1u;
  if (source->kind == W_SEED_FRONTEND_EXPR_STRING) {
    const uint8_t *bytes = source->const_byte_count == 0u
                               ? NULL
                               : frontend->const_bytes +
                                     source->const_byte_offset;
    append_bytes_unchecked(bytes, source->const_byte_count,
                           output->value_bytes, value_byte_cursor,
                           &target->byte_offset, &target->byte_count);
  } else if (source->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER) {
    target->type_index = hir_type_from_frontend(
        frontend, frontend_result, source->inferred_type);
    if (source->resolved_parameter_ordinal != W_SEED_FRONTEND_NONE) {
      target->kind = W_SEED_HIR0_VALUE_PARAMETER_READ;
      target->parameter_index =
          frontend->functions[function].first_parameter +
          source->resolved_parameter_ordinal;
    } else {
      target->kind = W_SEED_HIR0_VALUE_BINDING_READ;
      (void)binding_index_for_statement(
          frontend, frontend_result, function, statement_index,
          source->resolved_binding_statement, &target->binding_index);
    }
  } else if (source->kind == W_SEED_FRONTEND_EXPR_INTEGER) {
    target->kind = W_SEED_HIR0_VALUE_CONST_I64;
    target->type_index = 2u;
    (void)frontend_integer_i64(source, &target->integer_value);
  } else if (source->kind == W_SEED_FRONTEND_EXPR_BOOL) {
    target->kind = W_SEED_HIR0_VALUE_CONST_BOOL;
    target->type_index = 3u;
    target->bool_value = source->bool_value;
  } else if (source->kind == W_SEED_FRONTEND_EXPR_BINARY) {
    target->kind = W_SEED_HIR0_VALUE_BINARY_I64;
    target->type_index = 2u;
    target->left_value = left;
    target->right_value = right;
    target->binary_operator = hir_binary_operator(source->operator_text);
    output->values[left].owner_index = result;
    output->values[right].owner_index = result;
  } else {
    target->kind = W_SEED_HIR0_VALUE_INTERPOLATED_STRING;
    target->first_interpolation_segment = first_segment;
    target->interpolation_segment_count = source->interpolation_segment_count;
    for (size_t ordinal = 0u; ordinal < source->interpolation_segment_count;
         ordinal += 1u)
      output->interpolation_segments[(size_t)first_segment + ordinal]
          .owner_value = result;
  }
  return result;
}

/* Preflight has validated every index, label, type, and capacity used here.
 * This helper therefore emits one call instruction without a failure path. */
static uint32_t emit_call_instruction_unchecked(
    const w_seed_frontend_input *frontend_input,
    const w_seed_frontend_output *frontend,
    const w_seed_frontend_result *frontend_result,
    const w_seed_hir0_counts *counts, size_t function,
    size_t statement_index, uint32_t call_expression,
    w_seed_hir0_output *output, size_t *text_offset,
    size_t *instruction_offset, size_t *call_offset,
    size_t *argument_offset, size_t *value_index,
    size_t *interpolation_segment_index, size_t *value_offset) {
  const w_seed_frontend_expression *call_source =
      &frontend->expressions[call_expression];
  const bool host_call = call_source->resolved_callee_kind ==
                         W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL;
  const uint32_t target_function_index = call_source->resolved_function_index;
  const uint32_t host_index = call_source->resolved_host_symbol_index;
  const w_seed_frontend_host_prelude_symbol *host =
      host_call ? &frontend_input->host_scope->symbols[host_index] : NULL;
  const w_seed_frontend_function *local =
      host_call ? NULL : &frontend->functions[target_function_index];
  const uint32_t emitted_call = (uint32_t)*call_offset;
  w_seed_hir0_instruction *instruction =
      &output->instructions[*instruction_offset];
  instruction->kind = W_SEED_HIR0_INSTRUCTION_CALL;
  instruction->owner_block = (uint32_t)function;
  instruction->ordinal =
      (uint32_t)(*instruction_offset - output->blocks[function].first_instruction);
  instruction->call_index = emitted_call;
  instruction->binding_index = W_SEED_HIR0_NONE;
  instruction->source_span = call_source->span;
  w_seed_hir0_call *call = &output->calls[*call_offset];
  call->owner_instruction = (uint32_t)*instruction_offset;
  call->owner_block = (uint32_t)function;
  call->ordinal = instruction->ordinal;
  call->callee_identity =
      host_call
          ? hir_host_identity_index(counts, host_index)
          : (uint32_t)(counts->modules + target_function_index);
  call->first_argument = (uint32_t)*argument_offset;
  call->argument_count = call_source->argument_count;
  const w_seed_hir0_identity *callee_identity =
      &output->identities[call->callee_identity];
  call->first_requirement =
      host_call ? callee_identity->first_requirement : W_SEED_HIR0_NONE;
  call->requirement_count =
      host_call ? callee_identity->requirement_count : 0u;
  call->result_type = callee_identity->return_type;
  instruction->result_type = call->result_type;
  call->source_span = call_source->span;
  for (size_t argument = 0u; argument < call_source->argument_count;
       argument += 1u) {
    const size_t frontend_argument_index =
        (size_t)call_source->first_argument + argument;
    const w_seed_frontend_argument *argument_source =
        &frontend->arguments[frontend_argument_index];
    w_seed_hir0_argument *target_argument =
        &output->arguments[*argument_offset];
    target_argument->owner_call = emitted_call;
    target_argument->ordinal = (uint32_t)argument;
    target_argument->parameter_ordinal =
        argument_source->resolved_parameter_ordinal;
    target_argument->value_index = W_SEED_HIR0_NONE;
    if (host_call) {
      const w_seed_frontend_external_parameter *parameter =
          &host->parameters[target_argument->parameter_ordinal];
      target_argument->type_index = 1u;
      target_argument->label_kind = hir_label_kind(parameter->label_kind);
    } else {
      const w_seed_frontend_parameter *parameter =
          &frontend->parameters[(size_t)local->first_parameter +
                                target_argument->parameter_ordinal];
      target_argument->type_index = hir_type_from_frontend(
          frontend, frontend_result, parameter->type_index);
      target_argument->label_kind = hir_label_kind(parameter->label_kind);
    }
    append_text_unchecked(argument_source->label, output->text_bytes,
                          text_offset, &target_argument->label);
    target_argument->source_span = argument_source->span;
    target_argument->value_index = emit_value_tree_unchecked(
        frontend, frontend_result, function, statement_index,
        argument_source->expression_index, W_SEED_HIR0_VALUE_OWNER_ARGUMENT,
        (uint32_t)*argument_offset, 0u, output, value_index,
        interpolation_segment_index, value_offset);
    *argument_offset += 1u;
  }
  *instruction_offset += 1u;
  *call_offset += 1u;
  return emitted_call;
}

static bool source_digest(const w_seed_frontend_document *document,
                          uint8_t digest[32]) {
  if (document == NULL || document->source == NULL || digest == NULL) return false;
  const w_seed_byte_view bytes = w_seed_source_bytes(document->source);
  if (bytes.length != 0u && bytes.data == NULL) return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, bytes.data, bytes.length);
  w_seed_sha256_final(&state, digest);
  return true;
}

static void zero_bytes(void *pointer, size_t bytes) {
  if (bytes != 0u) (void)memset(pointer, 0, bytes);
}

static void emit_records(const w_seed_hir0_input *input,
                         const w_seed_hir0_counts *counts,
                         w_seed_hir0_output *output) {
  const w_seed_frontend_output *frontend = input->frontend_output;
  const w_seed_frontend_result *frontend_result = input->frontend_result;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  size_t text_offset = 0u;
  size_t value_offset = 0u;
  zero_bytes(output->modules, counts->modules * sizeof(*output->modules));
  zero_bytes(output->identities,
             counts->identities * sizeof(*output->identities));
  zero_bytes(output->types, counts->types * sizeof(*output->types));
  zero_bytes(output->functions,
             counts->functions * sizeof(*output->functions));
  zero_bytes(output->parameters,
             counts->parameters * sizeof(*output->parameters));
  zero_bytes(output->blocks, counts->blocks * sizeof(*output->blocks));
  zero_bytes(output->instructions,
             counts->instructions * sizeof(*output->instructions));
  zero_bytes(output->bindings,
             counts->bindings * sizeof(*output->bindings));
  zero_bytes(output->calls, counts->calls * sizeof(*output->calls));
  zero_bytes(output->host_parameters,
             counts->host_parameters * sizeof(*output->host_parameters));
  zero_bytes(output->arguments,
             counts->arguments * sizeof(*output->arguments));
  zero_bytes(output->requirements,
             counts->requirements * sizeof(*output->requirements));
  zero_bytes(output->values, counts->values * sizeof(*output->values));
  zero_bytes(output->interpolation_segments,
             counts->interpolation_segments *
                 sizeof(*output->interpolation_segments));
  zero_bytes(output->terminators,
             counts->terminators * sizeof(*output->terminators));
  zero_bytes(output->entries, counts->entries * sizeof(*output->entries));
  zero_bytes(output->text_bytes, counts->text_bytes);
  zero_bytes(output->value_bytes, counts->value_bytes);
  zero_bytes(output->receipt, counts->receipt_bytes);
  output->types[0] = (w_seed_hir0_type){
      W_SEED_HIR0_TYPE_UNIT, W_SEED_HIR0_NONE, {0u, 2u}};
  output->types[1] = (w_seed_hir0_type){
      W_SEED_HIR0_TYPE_STRING, W_SEED_HIR0_NONE, {2u, 6u}};
  output->types[2] = (w_seed_hir0_type){
      W_SEED_HIR0_TYPE_I64, W_SEED_HIR0_NONE, {8u, 3u}};
  output->types[3] = (w_seed_hir0_type){
      W_SEED_HIR0_TYPE_BOOL, W_SEED_HIR0_NONE, {11u, 4u}};
  /* output_capacity_ok proves these storage preconditions. */
  (void)memcpy(output->text_bytes, HIR0_UNIT_NAME, 2u);
  (void)memcpy(output->text_bytes + 2u, HIR0_STRING_NAME, 6u);
  (void)memcpy(output->text_bytes + 8u, HIR0_I64_NAME, 3u);
  (void)memcpy(output->text_bytes + 11u, HIR0_BOOL_NAME, 4u);
  text_offset = 15u;
  /* Module, function, and entry identities have deterministic dense ranges. */
  for (size_t module = 0u; module < counts->modules; module += 1u) {
    const w_seed_frontend_module *source = &frontend->modules[module];
    const size_t document_index = source->document_index;
    const w_seed_frontend_document *document =
        &frontend_input->documents[document_index];
    w_seed_hir0_module *target = &output->modules[module];
    target->module_index = (uint32_t)module;
    target->identity_index = (uint32_t)module;
    append_text_unchecked(source->source_id, output->text_bytes, &text_offset,
                          &target->source_id);
    append_text_unchecked(source->module_id, output->text_bytes, &text_offset,
                          &target->module_id);
    append_text_unchecked(source->local_module_name, output->text_bytes,
                          &text_offset, &target->local_module_name);
    (void)source_digest(document, target->source_sha256);
    target->source_span = source->span;
    target->source_length = document->source->bytes.length;
    target->first_function = source->first_function;
    target->function_count = source->function_count;
    target->first_entry = source->first_entry;
    target->entry_count = source->entry_count;
    w_seed_hir0_identity *identity = &output->identities[module];
    *identity = (w_seed_hir0_identity){
        .kind = W_SEED_HIR0_IDENTITY_MODULE,
        .owner_module = W_SEED_HIR0_NONE,
        .target_index = (uint32_t)module,
        .name = target->module_id,
        .first_parameter = W_SEED_HIR0_NONE,
        .parameter_count = 0u,
        .first_requirement = W_SEED_HIR0_NONE,
        .requirement_count = 0u,
        .return_type = W_SEED_HIR0_NONE,
        .is_const = false,
        .profile = {0u, 0u}};
  }
  const size_t function_identity_base = counts->modules;
  for (size_t function = 0u; function < counts->functions; function += 1u) {
    const w_seed_frontend_function *source = &frontend->functions[function];
    w_seed_hir0_function *target = &output->functions[function];
    const size_t module = source->module_index;
    const w_seed_frontend_module *module_source = &frontend->modules[module];
    const size_t document_index = module_source->document_index;
    target->module_index = (uint32_t)module;
    target->identity_index = (uint32_t)(function_identity_base + function);
    append_text_unchecked(source->name, output->text_bytes, &text_offset,
                          &target->name);
    target->source_span = source->span;
    target->body_span = source->body_span;
    target->return_type = hir_type_from_frontend(
        frontend, frontend_result, source->return_type);
    target->first_parameter = source->first_parameter;
    target->parameter_count = source->parameter_count;
    target->first_block = (uint32_t)function;
    target->block_count = 1u;
    target->is_const = source->is_const;
    target->is_async = source->is_async;
    target->is_throws = source->is_throws;
    target->is_unsafe = source->is_unsafe;
    target->has_borrow_clause = source->has_borrow_clause;
    output->identities[function_identity_base + function] =
        (w_seed_hir0_identity){
            .kind = W_SEED_HIR0_IDENTITY_FUNCTION,
            .owner_module = (uint32_t)module,
            .target_index = (uint32_t)function,
            .name = target->name,
            .first_parameter = target->first_parameter,
            .parameter_count = target->parameter_count,
            .first_requirement = W_SEED_HIR0_NONE,
            .requirement_count = 0u,
            .return_type = target->return_type,
            .is_const = source->is_const,
            .profile = {0u, 0u}};
    (void)document_index;
  }
  size_t host_parameter_offset = 0u;
  size_t requirement_offset = 0u;
  const size_t host_identity_base = counts->modules + counts->functions +
                                    counts->entries;
  for (size_t host = 0u; host < frontend_input->host_scope->symbol_count;
       host += 1u) {
    const w_seed_frontend_host_prelude_symbol *source =
        &frontend_input->host_scope->symbols[host];
    const uint32_t identity_index =
        (uint32_t)(host_identity_base + host);
    w_seed_hir0_identity *identity = &output->identities[identity_index];
    append_text_unchecked(source->name, output->text_bytes, &text_offset,
                          &identity->name);
    append_text_unchecked(frontend_input->host_scope->profile,
                          output->text_bytes, &text_offset, &identity->profile);
    identity->kind = W_SEED_HIR0_IDENTITY_HOST_PRELUDE;
    identity->owner_module = W_SEED_HIR0_NONE;
    identity->target_index = (uint32_t)host;
    identity->first_parameter = (uint32_t)host_parameter_offset;
    identity->parameter_count = (uint32_t)source->parameter_count;
    identity->first_requirement = (uint32_t)requirement_offset;
    identity->requirement_count = (uint32_t)source->requirement_count;
    identity->return_type = 0u;
    identity->is_const = source->is_const;
    const size_t host_parameter_base = host_parameter_offset;
    for (size_t parameter = 0u; parameter < source->parameter_count;
         parameter += 1u) {
      const w_seed_frontend_external_parameter *value =
          &source->parameters[parameter];
      w_seed_hir0_host_parameter *target =
          &output->host_parameters[host_parameter_base + parameter];
      target->owner_identity = identity_index;
      target->ordinal = (uint32_t)parameter;
      target->type_index = 1u;
      target->label_kind = hir_label_kind(value->label_kind);
      append_text_unchecked(value->name, output->text_bytes, &text_offset,
                            &target->name);
      if (value->label_kind == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY)
        target->label = (w_seed_hir0_text){0u, 0u};
      else
        append_text_unchecked(value->name, output->text_bytes, &text_offset,
                              &target->label);
    }
    host_parameter_offset += source->parameter_count;
    for (size_t requirement = 0u; requirement < source->requirement_count;
         requirement += 1u) {
      w_seed_hir0_requirement *target =
          &output->requirements[requirement_offset + requirement];
      target->owner_kind = W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY;
      target->owner_index = identity_index;
      target->ordinal = (uint32_t)requirement;
      append_text_unchecked(source->requirements[requirement].name,
                            output->text_bytes, &text_offset, &target->name);
    }
    requirement_offset += source->requirement_count;
  }
  /* Parameters preserve function signature facts. */
  for (size_t parameter = 0u; parameter < counts->parameters; parameter += 1u) {
    const w_seed_frontend_parameter *source = &frontend->parameters[parameter];
    w_seed_hir0_parameter *target = &output->parameters[parameter];
    target->owner_function = source->owner_function;
    target->ordinal = (uint32_t)(parameter -
                                 frontend->functions[source->owner_function]
                                     .first_parameter);
    target->type_index = hir_type_from_frontend(frontend, frontend_result,
                                                source->type_index);
    target->label_kind = hir_label_kind(source->label_kind);
    append_text_unchecked(source->name, output->text_bytes, &text_offset,
                          &target->name);
    append_text_unchecked(source->label, output->text_bytes, &text_offset,
                          &target->label);
    target->source_span = source->span;
  }
  size_t instruction_offset = 0u;
  size_t binding_offset = 0u;
  size_t call_offset = 0u;
  size_t argument_offset = 0u;
  size_t value_index = 0u;
  size_t interpolation_segment_index = 0u;
  for (size_t function = 0u; function < counts->functions; function += 1u) {
    const w_seed_frontend_function *source = &frontend->functions[function];
    w_seed_hir0_block *block = &output->blocks[function];
    block->owner_function = (uint32_t)function;
    block->ordinal = 0u;
    block->first_instruction = (uint32_t)instruction_offset;
    block->instruction_count = 0u;
    block->terminator_index = (uint32_t)function;
    block->source_span = source->body_span;
    block->next_block = W_SEED_HIR0_NONE;
    for (size_t ordinal = 0u; ordinal < source->statement_count; ordinal += 1u) {
      const size_t statement_index = (size_t)source->first_statement + ordinal;
      const w_seed_frontend_statement *statement = &frontend->statements[statement_index];
      if (statement->kind == W_SEED_FRONTEND_STMT_LET) {
        const w_seed_frontend_expression *initializer =
            &frontend->expressions[statement->expression_index];
        uint32_t call_result = W_SEED_HIR0_NONE;
        if (initializer->kind == W_SEED_FRONTEND_EXPR_CALL) {
          const uint32_t nested_call = emit_call_instruction_unchecked(
              frontend_input, frontend, frontend_result, counts, function,
              statement_index, statement->expression_index, output,
              &text_offset, &instruction_offset, &call_offset,
              &argument_offset, &value_index, &interpolation_segment_index,
              &value_offset);
          call_result = (uint32_t)value_index;
          output->values[value_index] = (w_seed_hir0_value){
              .kind = W_SEED_HIR0_VALUE_CALL_RESULT,
              .owner_kind = W_SEED_HIR0_VALUE_OWNER_BINDING,
              .owner_index = (uint32_t)binding_offset,
              .owner_ordinal = 0u,
              .type_index = output->calls[nested_call].result_type,
              .binding_index = W_SEED_HIR0_NONE,
              .parameter_index = W_SEED_HIR0_NONE,
              .call_index = nested_call,
              .left_value = W_SEED_HIR0_NONE,
              .right_value = W_SEED_HIR0_NONE,
              .first_interpolation_segment = W_SEED_HIR0_NONE,
              .interpolation_segment_count = 0u,
              .binary_operator = W_SEED_HIR0_BINARY_ADD,
              .integer_value = 0,
              .bool_value = false,
              .byte_offset = 0u,
              .byte_count = 0u,
              .source_span = initializer->span};
          value_index += 1u;
        }
        w_seed_hir0_instruction *instruction =
            &output->instructions[instruction_offset];
        instruction->owner_block = (uint32_t)function;
        instruction->ordinal = (uint32_t)(instruction_offset -
                                           block->first_instruction);
        instruction->result_type = 0u;
        instruction->kind = W_SEED_HIR0_INSTRUCTION_BINDING;
        instruction->call_index = W_SEED_HIR0_NONE;
        instruction->binding_index = (uint32_t)binding_offset;
        instruction->source_span = statement->span;
        w_seed_hir0_binding *binding = &output->bindings[binding_offset];
        binding->owner_instruction = (uint32_t)instruction_offset;
        binding->owner_block = (uint32_t)function;
        binding->ordinal = instruction->ordinal;
        binding->type_index = hir_type_from_frontend(
            frontend, frontend_result, statement->effective_type);
        append_text_unchecked(statement->binding_name, output->text_bytes,
                              &text_offset, &binding->name);
        binding->is_mutable = false;
        binding->initializer_value =
            call_result != W_SEED_HIR0_NONE
                ? call_result
                : emit_value_tree_unchecked(
                      frontend, frontend_result, function, statement_index,
                      statement->expression_index,
                      W_SEED_HIR0_VALUE_OWNER_BINDING,
                      (uint32_t)binding_offset, 0u, output, &value_index,
                      &interpolation_segment_index, &value_offset);
        binding->source_span = statement->span;
        binding_offset += 1u;
        instruction_offset += 1u;
        continue;
      }
      if (statement->kind == W_SEED_FRONTEND_STMT_RETURN) {
        output->terminators[function] = (w_seed_hir0_terminator){
            .owner_block = (uint32_t)function,
            .kind = W_SEED_HIR0_TERMINATOR_RETURN_VALUE,
            .ordinal = (uint32_t)(instruction_offset -
                                  block->first_instruction),
            .value_index = W_SEED_HIR0_NONE,
            .result_type = output->functions[function].return_type,
            .source_span = statement->span};
        continue;
      }
      (void)emit_call_instruction_unchecked(
          frontend_input, frontend, frontend_result, counts, function,
          statement_index, statement->expression_index, output, &text_offset,
          &instruction_offset, &call_offset, &argument_offset, &value_index,
          &interpolation_segment_index, &value_offset);
    }
    block->instruction_count =
        (uint32_t)(instruction_offset - block->first_instruction);
    if (output->functions[function].return_type == 0u) {
      output->terminators[function] = (w_seed_hir0_terminator){
          (uint32_t)function, W_SEED_HIR0_TERMINATOR_RETURN_UNIT,
          block->instruction_count, W_SEED_HIR0_NONE, 0u,
          source->body_span};
    }
  }
  /* Return value trees follow every instruction-owned value tree. This keeps
   * both partitions dense and lets verification consume each owner family in
   * one monotonic pass. */
  for (size_t function = 0u; function < counts->functions; function += 1u) {
    if (output->functions[function].return_type == 0u) continue;
    const w_seed_frontend_function *source = &frontend->functions[function];
    const size_t statement_index =
        (size_t)source->first_statement + source->statement_count - 1u;
    const w_seed_frontend_statement *statement =
        &frontend->statements[statement_index];
    output->terminators[function].value_index = emit_value_tree_unchecked(
        frontend, frontend_result, function, statement_index,
        statement->expression_index, W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
        (uint32_t)function, 0u, output, &value_index,
        &interpolation_segment_index, &value_offset);
  }
  /* Entry identities and records are dense after functions. */
  const size_t entry_identity_base = counts->modules + counts->functions;
  for (size_t entry = 0u; entry < counts->entries; entry += 1u) {
    const w_seed_frontend_entry *source = &frontend->entries[entry];
    size_t function = 0u;
    (void)function_for_entry(frontend, counts->functions, source->module_index,
                             source->target, &function);
    w_seed_hir0_entry *target = &output->entries[entry];
    target->module_index = source->module_index;
    target->identity_index = (uint32_t)(entry_identity_base + entry);
    target->target_function = (uint32_t)function;
    target->target_identity = output->functions[function].identity_index;
    append_text_unchecked(source->target, output->text_bytes, &text_offset,
                          &target->target_name);
    append_text_unchecked((w_seed_frontend_text){HIR0_SLOT_NAME,
                                                 sizeof(HIR0_SLOT_NAME) - 1u},
                          output->text_bytes, &text_offset, &target->slot);
    target->source_span = source->span;
    output->identities[entry_identity_base + entry] =
        (w_seed_hir0_identity){
            .kind = W_SEED_HIR0_IDENTITY_ENTRY,
            .owner_module = source->module_index,
            .target_index = (uint32_t)entry,
            .name = target->target_name,
            .first_parameter = W_SEED_HIR0_NONE,
            .parameter_count = 0u,
            .first_requirement = W_SEED_HIR0_NONE,
            .requirement_count = 0u,
            .return_type = W_SEED_HIR0_NONE,
            .is_const = false,
            .profile = {0u, 0u}};
  }
  /* collect() proves these cursors equal the measured bounds. */
}

/* The digest encoding uses explicit big-endian scalar fields. It never hashes
 * C object representation or padding. */
static void digest_u32(w_seed_sha256_state *state, uint32_t value) {
  uint8_t bytes[4] = {(uint8_t)(value >> 24u), (uint8_t)(value >> 16u),
                      (uint8_t)(value >> 8u), (uint8_t)value};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void digest_u64(w_seed_sha256_state *state, uint64_t value) {
  uint8_t bytes[8] = {(uint8_t)(value >> 56u), (uint8_t)(value >> 48u),
                      (uint8_t)(value >> 40u), (uint8_t)(value >> 32u),
                      (uint8_t)(value >> 24u), (uint8_t)(value >> 16u),
                      (uint8_t)(value >> 8u), (uint8_t)value};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void digest_bool(w_seed_sha256_state *state, bool value) {
  const uint8_t byte = value ? 1u : 0u;
  w_seed_sha256_update(state, &byte, 1u);
}

static void digest_span(w_seed_sha256_state *state, w_seed_span span) {
  digest_u64(state, (uint64_t)span.start_byte);
  digest_u64(state, (uint64_t)span.end_byte);
}

static void digest_text(w_seed_sha256_state *state,
                        const w_seed_hir0_program *program,
                        w_seed_hir0_text text) {
  digest_u32(state, text.offset);
  digest_u32(state, text.count);
  if (text.count != 0u)
    w_seed_sha256_update(state, program->text_bytes + text.offset, text.count);
}

static void digest_bytes(w_seed_sha256_state *state,
                         const w_seed_hir0_program *program, uint32_t offset,
                         uint32_t count) {
  digest_u32(state, offset);
  digest_u32(state, count);
  if (count != 0u)
    w_seed_sha256_update(state, program->value_bytes + offset, count);
}

static void digest_counts(w_seed_sha256_state *state,
                          const w_seed_hir0_counts *counts) {
  digest_u64(state, counts->modules);
  digest_u64(state, counts->identities);
  digest_u64(state, counts->types);
  digest_u64(state, counts->functions);
  digest_u64(state, counts->parameters);
  digest_u64(state, counts->blocks);
  digest_u64(state, counts->instructions);
  digest_u64(state, counts->bindings);
  digest_u64(state, counts->calls);
  digest_u64(state, counts->host_parameters);
  digest_u64(state, counts->arguments);
  digest_u64(state, counts->requirements);
  digest_u64(state, counts->values);
  digest_u64(state, counts->interpolation_segments);
  digest_u64(state, counts->terminators);
  digest_u64(state, counts->entries);
  digest_u64(state, counts->text_bytes);
  digest_u64(state, counts->value_bytes);
}

static void digest_program(const w_seed_hir0_program *program,
                           const w_seed_hir0_counts *counts,
                           uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)W_SEED_HIR0_SCHEMA_VERSION,
                       sizeof(W_SEED_HIR0_SCHEMA_VERSION) - 1u);
  digest_counts(&state, counts);
#define HIR0_RECORD_TAG(value) digest_u32(&state, (uint32_t)(value))
  for (size_t index = 0u; index < counts->modules; index += 1u) {
    const w_seed_hir0_module *value = &program->modules[index];
    HIR0_RECORD_TAG(1u);
    digest_u32(&state, value->module_index);
    digest_u32(&state, value->identity_index);
    digest_text(&state, program, value->module_id);
    digest_text(&state, program, value->local_module_name);
    digest_u32(&state, value->first_function);
    digest_u32(&state, value->function_count);
    digest_u32(&state, value->first_entry);
    digest_u32(&state, value->entry_count);
  }
  for (size_t index = 0u; index < counts->identities; index += 1u) {
    const w_seed_hir0_identity *value = &program->identities[index];
    HIR0_RECORD_TAG(2u);
    digest_u32(&state, (uint32_t)value->kind);
    digest_u32(&state, value->owner_module);
    digest_u32(&state, value->target_index);
    digest_text(&state, program, value->name);
    digest_u32(&state, value->first_parameter);
    digest_u32(&state, value->parameter_count);
    digest_u32(&state, value->first_requirement);
    digest_u32(&state, value->requirement_count);
    digest_u32(&state, value->return_type);
    digest_bool(&state, value->is_const);
    digest_text(&state, program, value->profile);
  }
  for (size_t index = 0u; index < counts->types; index += 1u) {
    const w_seed_hir0_type *value = &program->types[index];
    HIR0_RECORD_TAG(3u);
    digest_u32(&state, (uint32_t)value->kind);
    digest_u32(&state, value->owner_module);
    digest_text(&state, program, value->name);
  }
  for (size_t index = 0u; index < counts->functions; index += 1u) {
    const w_seed_hir0_function *value = &program->functions[index];
    HIR0_RECORD_TAG(4u);
    digest_u32(&state, value->module_index);
    digest_u32(&state, value->identity_index);
    digest_text(&state, program, value->name);
    digest_u32(&state, value->return_type);
    digest_u32(&state, value->first_parameter);
    digest_u32(&state, value->parameter_count);
    digest_u32(&state, value->first_block);
    digest_u32(&state, value->block_count);
    digest_bool(&state, value->is_const);
    digest_bool(&state, value->is_async);
    digest_bool(&state, value->is_throws);
    digest_bool(&state, value->is_unsafe);
    digest_bool(&state, value->has_borrow_clause);
  }
  for (size_t index = 0u; index < counts->parameters; index += 1u) {
    const w_seed_hir0_parameter *value = &program->parameters[index];
    HIR0_RECORD_TAG(5u);
    digest_u32(&state, value->owner_function);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->type_index);
    digest_text(&state, program, value->name);
    digest_text(&state, program, value->label);
    digest_u32(&state, (uint32_t)value->label_kind);
  }
  for (size_t index = 0u; index < counts->blocks; index += 1u) {
    const w_seed_hir0_block *value = &program->blocks[index];
    HIR0_RECORD_TAG(6u);
    digest_u32(&state, value->owner_function);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->first_instruction);
    digest_u32(&state, value->instruction_count);
    digest_u32(&state, value->terminator_index);
    digest_u32(&state, value->next_block);
  }
  for (size_t index = 0u; index < counts->instructions; index += 1u) {
    const w_seed_hir0_instruction *value = &program->instructions[index];
    HIR0_RECORD_TAG(7u);
    digest_u32(&state, (uint32_t)value->kind);
    digest_u32(&state, value->owner_block);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->call_index);
    digest_u32(&state, value->binding_index);
    digest_u32(&state, value->result_type);
  }
  for (size_t index = 0u; index < counts->calls; index += 1u) {
    const w_seed_hir0_call *value = &program->calls[index];
    HIR0_RECORD_TAG(8u);
    digest_u32(&state, value->owner_instruction);
    digest_u32(&state, value->owner_block);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->callee_identity);
    digest_u32(&state, value->first_argument);
    digest_u32(&state, value->argument_count);
    digest_u32(&state, value->first_requirement);
    digest_u32(&state, value->requirement_count);
    digest_u32(&state, value->result_type);
  }
  for (size_t index = 0u; index < counts->host_parameters; index += 1u) {
    const w_seed_hir0_host_parameter *value = &program->host_parameters[index];
    HIR0_RECORD_TAG(9u);
    digest_u32(&state, value->owner_identity);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->type_index);
    digest_text(&state, program, value->name);
    digest_text(&state, program, value->label);
    digest_u32(&state, (uint32_t)value->label_kind);
  }
  for (size_t index = 0u; index < counts->arguments; index += 1u) {
    const w_seed_hir0_argument *value = &program->arguments[index];
    HIR0_RECORD_TAG(10u);
    digest_u32(&state, value->owner_call);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->parameter_ordinal);
    digest_u32(&state, value->value_index);
    digest_u32(&state, value->type_index);
    digest_text(&state, program, value->label);
    digest_u32(&state, (uint32_t)value->label_kind);
  }
  for (size_t index = 0u; index < counts->requirements; index += 1u) {
    const w_seed_hir0_requirement *value = &program->requirements[index];
    HIR0_RECORD_TAG(11u);
    digest_u32(&state, (uint32_t)value->owner_kind);
    digest_u32(&state, value->owner_index);
    digest_u32(&state, value->ordinal);
    digest_text(&state, program, value->name);
  }
  for (size_t index = 0u; index < counts->values; index += 1u) {
    const w_seed_hir0_value *value = &program->values[index];
    HIR0_RECORD_TAG(12u);
    digest_u32(&state, (uint32_t)value->kind);
    digest_u32(&state, (uint32_t)value->owner_kind);
    digest_u32(&state, value->owner_index);
    digest_u32(&state, value->owner_ordinal);
    digest_u32(&state, value->type_index);
    digest_u32(&state, value->binding_index);
    digest_u32(&state, value->parameter_index);
    digest_u32(&state, value->call_index);
    digest_u32(&state, value->left_value);
    digest_u32(&state, value->right_value);
    digest_u32(&state, value->first_interpolation_segment);
    digest_u32(&state, value->interpolation_segment_count);
    digest_u32(&state, (uint32_t)value->binary_operator);
    digest_u64(&state, (uint64_t)value->integer_value);
    digest_bool(&state, value->bool_value);
    digest_bytes(&state, program, value->byte_offset, value->byte_count);
  }
  for (size_t index = 0u; index < counts->interpolation_segments;
       index += 1u) {
    const w_seed_hir0_interpolation_segment *value =
        &program->interpolation_segments[index];
    HIR0_RECORD_TAG(16u);
    digest_u32(&state, (uint32_t)value->kind);
    digest_u32(&state, value->owner_value);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->value_index);
    digest_bytes(&state, program, value->byte_offset, value->byte_count);
  }
  for (size_t index = 0u; index < counts->terminators; index += 1u) {
    const w_seed_hir0_terminator *value = &program->terminators[index];
    HIR0_RECORD_TAG(13u);
    digest_u32(&state, value->owner_block);
    digest_u32(&state, (uint32_t)value->kind);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->value_index);
    digest_u32(&state, value->result_type);
  }
  for (size_t index = 0u; index < counts->entries; index += 1u) {
    const w_seed_hir0_entry *value = &program->entries[index];
    HIR0_RECORD_TAG(14u);
    digest_u32(&state, value->module_index);
    digest_u32(&state, value->identity_index);
    digest_u32(&state, value->target_function);
    digest_u32(&state, value->target_identity);
    digest_text(&state, program, value->target_name);
    digest_text(&state, program, value->slot);
  }
  for (size_t index = 0u; index < counts->bindings; index += 1u) {
    const w_seed_hir0_binding *value = &program->bindings[index];
    HIR0_RECORD_TAG(15u);
    digest_u32(&state, value->owner_instruction);
    digest_u32(&state, value->owner_block);
    digest_u32(&state, value->ordinal);
    digest_u32(&state, value->type_index);
    digest_text(&state, program, value->name);
    digest_bool(&state, value->is_mutable);
    digest_u32(&state, value->initializer_value);
  }
#undef HIR0_RECORD_TAG
  w_seed_sha256_final(&state, digest);
}

static void digest_provenance(const w_seed_hir0_program *program,
                              const w_seed_hir0_counts *counts,
                              uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)"w-seed-hir0-provenance-1",
                       sizeof("w-seed-hir0-provenance-1") - 1u);
  digest_counts(&state, counts);
  for (size_t index = 0u; index < counts->modules; index += 1u) {
    const w_seed_hir0_module *value = &program->modules[index];
    digest_text(&state, program, value->source_id);
    digest_u64(&state, (uint64_t)value->source_length);
    digest_span(&state, value->source_span);
    w_seed_sha256_update(&state, value->source_sha256, 32u);
  }
  for (size_t index = 0u; index < counts->functions; index += 1u) {
    digest_span(&state, program->functions[index].source_span);
    digest_span(&state, program->functions[index].body_span);
  }
  for (size_t index = 0u; index < counts->parameters; index += 1u)
    digest_span(&state, program->parameters[index].source_span);
  for (size_t index = 0u; index < counts->blocks; index += 1u)
    digest_span(&state, program->blocks[index].source_span);
  for (size_t index = 0u; index < counts->instructions; index += 1u)
    digest_span(&state, program->instructions[index].source_span);
  for (size_t index = 0u; index < counts->calls; index += 1u)
    digest_span(&state, program->calls[index].source_span);
  for (size_t index = 0u; index < counts->arguments; index += 1u)
    digest_span(&state, program->arguments[index].source_span);
  for (size_t index = 0u; index < counts->values; index += 1u)
    digest_span(&state, program->values[index].source_span);
  for (size_t index = 0u; index < counts->interpolation_segments;
       index += 1u)
    digest_span(&state, program->interpolation_segments[index].source_span);
  for (size_t index = 0u; index < counts->terminators; index += 1u)
    digest_span(&state, program->terminators[index].source_span);
  for (size_t index = 0u; index < counts->entries; index += 1u)
    digest_span(&state, program->entries[index].source_span);
  for (size_t index = 0u; index < counts->bindings; index += 1u)
    digest_span(&state, program->bindings[index].source_span);
  w_seed_sha256_final(&state, digest);
}

static void write_u64_be(uint8_t *buffer, size_t offset, uint64_t value) {
  buffer[offset] = (uint8_t)(value >> 56u);
  buffer[offset + 1u] = (uint8_t)(value >> 48u);
  buffer[offset + 2u] = (uint8_t)(value >> 40u);
  buffer[offset + 3u] = (uint8_t)(value >> 32u);
  buffer[offset + 4u] = (uint8_t)(value >> 24u);
  buffer[offset + 5u] = (uint8_t)(value >> 16u);
  buffer[offset + 6u] = (uint8_t)(value >> 8u);
  buffer[offset + 7u] = (uint8_t)value;
}

static void write_receipt_unchecked(uint8_t *buffer,
                                    const w_seed_hir0_counts *counts,
                                    const uint8_t semantic_digest[32],
                                    const uint8_t provenance_digest[32]) {
  (void)memset(buffer, 0, HIR0_RECEIPT_BYTES);
  (void)memcpy(buffer, W_SEED_HIR0_SCHEMA_VERSION,
               sizeof(W_SEED_HIR0_SCHEMA_VERSION) - 1u);
  const size_t fields[HIR0_RECEIPT_COUNT_FIELDS] = {
      counts->modules,       counts->identities, counts->types,
      counts->functions,      counts->parameters, counts->blocks,
      counts->instructions,   counts->bindings,   counts->calls,
      counts->host_parameters,
      counts->arguments,      counts->requirements, counts->values,
      counts->interpolation_segments,
      counts->terminators,    counts->entries,     counts->text_bytes,
      counts->value_bytes};
  size_t offset = HIR0_RECEIPT_SCHEMA_BYTES;
  for (size_t index = 0u; index < HIR0_RECEIPT_COUNT_FIELDS; index += 1u) {
    write_u64_be(buffer, offset, (uint64_t)fields[index]);
    offset += 8u;
  }
  (void)memcpy(buffer + offset, semantic_digest, HIR0_DIGEST_BYTES);
  offset += HIR0_DIGEST_BYTES;
  (void)memcpy(buffer + offset, provenance_digest, HIR0_DIGEST_BYTES);
}

static bool basic_program_shape(const w_seed_hir0_program *program,
                                const w_seed_hir0_result *result) {
  if (program == NULL || result == NULL ||
      result->status != W_SEED_HIR0_OK ||
      result->schema[sizeof(result->schema) - 1u] != '\0' ||
      memcmp(result->schema, W_SEED_HIR0_SCHEMA_VERSION,
             sizeof(W_SEED_HIR0_SCHEMA_VERSION)) != 0)
    return false;
#define HIR0_PROGRAM(field, count_field, capacity_field, type)                 \
  if (program->count_field > program->capacity_field ||                       \
      (program->count_field != 0u && program->field == NULL) ||               \
      (program->count_field != 0u &&                                          \
       program->count_field > SIZE_MAX / sizeof(type))) return false
  HIR0_PROGRAM(modules, module_count, module_capacity, w_seed_hir0_module);
  HIR0_PROGRAM(identities, identity_count, identity_capacity,
               w_seed_hir0_identity);
  HIR0_PROGRAM(types, type_count, type_capacity, w_seed_hir0_type);
  HIR0_PROGRAM(functions, function_count, function_capacity,
               w_seed_hir0_function);
  HIR0_PROGRAM(parameters, parameter_count, parameter_capacity,
               w_seed_hir0_parameter);
  HIR0_PROGRAM(blocks, block_count, block_capacity, w_seed_hir0_block);
  HIR0_PROGRAM(instructions, instruction_count, instruction_capacity,
               w_seed_hir0_instruction);
  HIR0_PROGRAM(bindings, binding_count, binding_capacity, w_seed_hir0_binding);
  HIR0_PROGRAM(calls, call_count, call_capacity, w_seed_hir0_call);
  HIR0_PROGRAM(host_parameters, host_parameter_count, host_parameter_capacity,
               w_seed_hir0_host_parameter);
  HIR0_PROGRAM(arguments, argument_count, argument_capacity,
               w_seed_hir0_argument);
  HIR0_PROGRAM(requirements, requirement_count, requirement_capacity,
               w_seed_hir0_requirement);
  HIR0_PROGRAM(values, value_count, value_capacity, w_seed_hir0_value);
  HIR0_PROGRAM(interpolation_segments, interpolation_segment_count,
               interpolation_segment_capacity,
               w_seed_hir0_interpolation_segment);
  HIR0_PROGRAM(terminators, terminator_count, terminator_capacity,
               w_seed_hir0_terminator);
  HIR0_PROGRAM(entries, entry_count, entry_capacity, w_seed_hir0_entry);
#undef HIR0_PROGRAM
  if (program->text_byte_count > program->text_byte_capacity ||
      (program->text_byte_count != 0u && program->text_bytes == NULL) ||
      program->value_byte_count > program->value_byte_capacity ||
      (program->value_byte_count != 0u && program->value_bytes == NULL) ||
      program->receipt_count > program->receipt_capacity ||
      program->receipt_count != HIR0_RECEIPT_BYTES || program->receipt == NULL)
    return false;
  return true;
}

static bool program_aliases(const w_seed_hir0_program *program) {
  if (program == NULL) return true;
  hir0_memory_range ranges[32];
  size_t count = 0u;
  if (!program_range_table(program, ranges, &count)) return true;
  for (size_t first = 0u; first < count; first += 1u)
    for (size_t second = first + 1u; second < count; second += 1u)
      if (ranges_overlap(ranges[first].pointer, ranges[first].bytes,
                         ranges[second].pointer, ranges[second].bytes))
        return true;
  return false;
}

static bool verify_identity_records(const w_seed_hir0_program *program) {
  const size_t module_count = program->module_count;
  const size_t function_base = module_count;
  const size_t entry_base = module_count + program->function_count;
  const size_t host_base = entry_base + program->entry_count;
  if (program->identity_count < host_base)
    return false;
  size_t host_count = program->identity_count - host_base;
  for (size_t index = 0u; index < module_count; index += 1u) {
    const w_seed_hir0_identity *value = &program->identities[index];
    if (value->kind != W_SEED_HIR0_IDENTITY_MODULE ||
        value->owner_module != W_SEED_HIR0_NONE || value->target_index != index ||
        value->first_parameter != W_SEED_HIR0_NONE || value->parameter_count != 0u ||
        value->first_requirement != W_SEED_HIR0_NONE || value->requirement_count != 0u ||
        value->return_type != W_SEED_HIR0_NONE || value->is_const ||
        value->profile.count != 0u || !hir_text_valid(program, value->profile) ||
        !hir_text_equal(program, value->name, program->modules[index].module_id))
      return false;
  }
  for (size_t index = 0u; index < program->function_count; index += 1u) {
    const w_seed_hir0_identity *value = &program->identities[function_base + index];
    if (value->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
        value->target_index != index || value->owner_module >= module_count ||
        value->first_parameter != program->functions[index].first_parameter ||
        value->parameter_count != program->functions[index].parameter_count ||
        value->first_requirement != W_SEED_HIR0_NONE || value->requirement_count != 0u ||
        value->return_type != program->functions[index].return_type ||
        value->is_const != program->functions[index].is_const ||
        value->profile.count != 0u || !hir_text_valid(program, value->profile) ||
        !hir_text_equal(program, value->name, program->functions[index].name))
      return false;
  }
  for (size_t index = 0u; index < program->entry_count; index += 1u) {
    const w_seed_hir0_identity *value = &program->identities[entry_base + index];
    if (value->kind != W_SEED_HIR0_IDENTITY_ENTRY ||
        value->target_index != index || value->owner_module >= module_count ||
        value->first_parameter != W_SEED_HIR0_NONE || value->parameter_count != 0u ||
        value->first_requirement != W_SEED_HIR0_NONE || value->requirement_count != 0u ||
        value->return_type != W_SEED_HIR0_NONE || value->is_const ||
        value->profile.count != 0u || !hir_text_valid(program, value->profile) ||
        !hir_text_equal(program, value->name, program->entries[index].target_name))
      return false;
  }
  if (host_count == 0u) return false;
  size_t host_parameter_cursor = 0u;
  size_t requirement_cursor = 0u;
  for (size_t index = 0u; index < host_count; index += 1u) {
    const w_seed_hir0_identity *value = &program->identities[host_base + index];
    if (value->kind != W_SEED_HIR0_IDENTITY_HOST_PRELUDE ||
        value->owner_module != W_SEED_HIR0_NONE || value->target_index != index ||
        value->return_type != 0u || !hir_text_valid(program, value->profile) ||
        value->profile.count == 0u ||
        !range_valid(value->first_parameter, value->parameter_count,
                     program->host_parameter_count) ||
        !range_valid(value->first_requirement, value->requirement_count,
                     program->requirement_count) ||
        value->first_parameter != host_parameter_cursor ||
        value->first_requirement != requirement_cursor ||
        !hir_text_valid(program, value->name) || value->name.count == 0u ||
        (index != 0u &&
         !hir_text_equal(program, value->profile,
                         program->identities[host_base].profile)))
      return false;
    for (size_t parameter = 0u; parameter < value->parameter_count; parameter += 1u) {
      const w_seed_hir0_host_parameter *item =
          &program->host_parameters[(size_t)value->first_parameter + parameter];
      if (item->owner_identity != host_base + index || item->ordinal != parameter ||
          item->type_index != 1u || !hir_text_valid(program, item->name) ||
          !hir_text_valid(program, item->label) ||
          item->label_kind > W_SEED_HIR0_LABEL_REQUIRED ||
           !hir_host_label_matches(program, item->label_kind, item->name,
                                   item->label))
        return false;
    }
    for (size_t requirement = 0u; requirement < value->requirement_count;
         requirement += 1u) {
      const w_seed_hir0_requirement *item =
          &program->requirements[(size_t)value->first_requirement + requirement];
      if (item->owner_kind != W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY ||
          item->owner_index != host_base + index || item->ordinal != requirement ||
          !hir_text_valid(program, item->name) || item->name.count == 0u)
         return false;
    }
    host_parameter_cursor += value->parameter_count;
    requirement_cursor += value->requirement_count;
  }
  if (host_parameter_cursor != program->host_parameter_count ||
      requirement_cursor != program->requirement_count)
    return false;
  return true;
}

static bool verify_value_tree(
    const w_seed_hir0_program *program, uint32_t root_index,
    w_seed_hir0_value_owner_kind owner_kind, uint32_t owner_index,
    uint32_t owner_ordinal, uint32_t current_block,
    uint32_t current_instruction, size_t source_length, size_t depth,
    size_t *value_cursor, size_t *segment_cursor, size_t *byte_cursor) {
  if (program == NULL || value_cursor == NULL || segment_cursor == NULL ||
      byte_cursor == NULL || depth > 256u ||
      (size_t)root_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[root_index];
  if (value->owner_kind != owner_kind || value->owner_index != owner_index ||
      value->owner_ordinal != owner_ordinal ||
      !span_valid(value->source_span, source_length) ||
      (value->kind != W_SEED_HIR0_VALUE_CALL_RESULT &&
       value->call_index != W_SEED_HIR0_NONE))
    return false;

  if (value->kind == W_SEED_HIR0_VALUE_BINARY_I64) {
    if (value->left_value == W_SEED_HIR0_NONE ||
        value->right_value == W_SEED_HIR0_NONE ||
        !verify_value_tree(program, value->left_value,
                           W_SEED_HIR0_VALUE_OWNER_BINARY, root_index, 0u,
                           current_block, current_instruction, source_length,
                           depth + 1u, value_cursor, segment_cursor,
                           byte_cursor) ||
        !verify_value_tree(program, value->right_value,
                           W_SEED_HIR0_VALUE_OWNER_BINARY, root_index, 1u,
                           current_block, current_instruction, source_length,
                           depth + 1u, value_cursor, segment_cursor,
                           byte_cursor) ||
        (size_t)root_index != *value_cursor || value->type_index != 2u ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != W_SEED_HIR0_NONE ||
        value->interpolation_segment_count != 0u ||
        value->binary_operator > W_SEED_HIR0_BINARY_REMAINDER ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
    *value_cursor += 1u;
    return true;
  }

  if (value->kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
    if (value->type_index != 1u ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->left_value != W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->first_interpolation_segment != *segment_cursor ||
        value->interpolation_segment_count == 0u ||
        !range_valid(value->first_interpolation_segment,
                     value->interpolation_segment_count,
                     program->interpolation_segment_count) ||
        value->binary_operator != W_SEED_HIR0_BINARY_ADD ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
    for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
         ordinal += 1u) {
      const size_t index = *segment_cursor;
      const w_seed_hir0_interpolation_segment *segment =
          &program->interpolation_segments[index];
      if (segment->owner_value != root_index || segment->ordinal != ordinal ||
          !span_valid(segment->source_span, source_length))
        return false;
      *segment_cursor += 1u;
      if (segment->kind == W_SEED_HIR0_INTERPOLATION_TEXT) {
        if (segment->value_index != W_SEED_HIR0_NONE ||
            (size_t)segment->byte_offset != *byte_cursor ||
            !byte_slice_valid(program, segment->byte_offset,
                              segment->byte_count) ||
            !add_size(*byte_cursor, segment->byte_count, byte_cursor))
          return false;
      } else if (segment->kind == W_SEED_HIR0_INTERPOLATION_VALUE) {
        if (segment->byte_offset != 0u || segment->byte_count != 0u ||
            !verify_value_tree(
                program, segment->value_index,
                W_SEED_HIR0_VALUE_OWNER_INTERPOLATION_SEGMENT,
                (uint32_t)index, 0u, current_block, current_instruction,
                source_length, depth + 1u, value_cursor, segment_cursor,
                byte_cursor))
          return false;
      } else {
        return false;
      }
    }
    if ((size_t)root_index != *value_cursor) return false;
    *value_cursor += 1u;
    return true;
  }

  if ((size_t)root_index != *value_cursor ||
      value->left_value != W_SEED_HIR0_NONE ||
      value->right_value != W_SEED_HIR0_NONE ||
      value->first_interpolation_segment != W_SEED_HIR0_NONE ||
      value->interpolation_segment_count != 0u ||
      value->binary_operator != W_SEED_HIR0_BINARY_ADD)
    return false;
  if (value->kind == W_SEED_HIR0_VALUE_CALL_RESULT) {
    if ((value->type_index != 2u && value->type_index != 3u) ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index == W_SEED_HIR0_NONE ||
        (size_t)value->call_index >= program->call_count ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
    const w_seed_hir0_call *call = &program->calls[value->call_index];
    if (call->owner_block != current_block ||
        call->owner_instruction >= current_instruction ||
        call->result_type != value->type_index ||
        program->identities[call->callee_identity].kind !=
            W_SEED_HIR0_IDENTITY_FUNCTION)
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_CONST_STRING) {
    if (value->type_index != 1u ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->bool_value ||
        (size_t)value->byte_offset != *byte_cursor ||
        !byte_slice_valid(program, value->byte_offset, value->byte_count) ||
        !add_size(*byte_cursor, value->byte_count, byte_cursor))
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
    if (value->type_index == 0u || value->type_index > 3u ||
        value->binding_index == W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        (size_t)value->binding_index >= program->binding_count ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
    const w_seed_hir0_binding *binding =
        &program->bindings[value->binding_index];
    if (binding->owner_block != current_block ||
        binding->owner_instruction >= current_instruction ||
        binding->type_index != value->type_index)
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_PARAMETER_READ) {
    if (value->type_index == 0u || value->type_index > 3u ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index == W_SEED_HIR0_NONE ||
        (size_t)value->parameter_index >= program->parameter_count ||
        value->integer_value != 0 || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
    const w_seed_hir0_parameter *parameter =
        &program->parameters[value->parameter_index];
    if (current_block >= program->block_count ||
        parameter->owner_function !=
            program->blocks[current_block].owner_function ||
        parameter->type_index != value->type_index)
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_CONST_I64) {
    if (value->type_index != 2u ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE || value->bool_value ||
        value->byte_offset != 0u || value->byte_count != 0u)
      return false;
  } else if (value->kind == W_SEED_HIR0_VALUE_CONST_BOOL) {
    if (value->type_index != 3u ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->integer_value != 0 || value->byte_offset != 0u ||
        value->byte_count != 0u)
      return false;
  } else {
    return false;
  }
  *value_cursor += 1u;
  return true;
}

static bool verify_records(const w_seed_hir0_program *program) {
  size_t expected_instructions = 0u;
  if (program->module_count != 1u || program->function_count == 0u ||
      program->entry_count != 1u || program->type_count != 4u ||
      program->block_count != program->function_count ||
      program->terminator_count != program->function_count ||
      !add_size(program->binding_count, program->call_count,
                &expected_instructions) ||
      program->instruction_count != expected_instructions ||
      program->argument_count > program->value_count ||
      !hir_text_is(program, program->types[0].name, HIR0_UNIT_NAME) ||
      !hir_text_is(program, program->types[1].name, HIR0_STRING_NAME) ||
      !hir_text_is(program, program->types[2].name, HIR0_I64_NAME) ||
      !hir_text_is(program, program->types[3].name, HIR0_BOOL_NAME) ||
      program->types[0].kind != W_SEED_HIR0_TYPE_UNIT ||
      program->types[1].kind != W_SEED_HIR0_TYPE_STRING ||
      program->types[2].kind != W_SEED_HIR0_TYPE_I64 ||
      program->types[3].kind != W_SEED_HIR0_TYPE_BOOL ||
      program->types[0].owner_module != W_SEED_HIR0_NONE ||
      program->types[1].owner_module != W_SEED_HIR0_NONE ||
      program->types[2].owner_module != W_SEED_HIR0_NONE ||
      program->types[3].owner_module != W_SEED_HIR0_NONE ||
      !verify_identity_records(program))
    return false;
  size_t module_function_cursor = 0u;
  size_t module_entry_cursor = 0u;
  for (size_t module = 0u; module < program->module_count; module += 1u) {
    const w_seed_hir0_module *value = &program->modules[module];
    if (value->module_index != module || value->identity_index != module ||
        !hir_text_valid(program, value->source_id) ||
        !hir_text_valid(program, value->module_id) ||
        !hir_text_valid(program, value->local_module_name) ||
         value->source_length == 0u ||
         !span_valid(value->source_span, value->source_length) ||
         value->first_function != module_function_cursor ||
         value->first_entry != module_entry_cursor ||
         !range_valid(value->first_function, value->function_count,
                     program->function_count) ||
        !range_valid(value->first_entry, value->entry_count,
                     program->entry_count))
      return false;
    for (size_t prior = 0u; prior < module; prior += 1u)
      if (hir_text_equal(program, value->module_id,
                          program->modules[prior].module_id))
        return false;
    module_function_cursor += value->function_count;
    module_entry_cursor += value->entry_count;
  }
  if (module_function_cursor != program->function_count ||
      module_entry_cursor != program->entry_count)
    return false;
  size_t function_parameter_cursor = 0u;
  for (size_t function = 0u; function < program->function_count; function += 1u) {
    const w_seed_hir0_function *value = &program->functions[function];
    if (value->module_index >= program->module_count ||
        value->identity_index != program->module_count + function ||
        !hir_text_valid(program, value->name) || value->name.count == 0u ||
        (value->return_type != 0u && value->return_type != 2u &&
         value->return_type != 3u) ||
        !span_valid(value->source_span,
                    program->modules[value->module_index].source_length) ||
         !span_valid(value->body_span,
                     program->modules[value->module_index].source_length) ||
         value->first_parameter != function_parameter_cursor ||
         !range_valid(value->first_parameter, value->parameter_count,
                     program->parameter_count) || value->first_block != function ||
        value->block_count != 1u)
      return false;
    for (size_t parameter = 0u; parameter < value->parameter_count; parameter += 1u) {
      const w_seed_hir0_parameter *item =
          &program->parameters[(size_t)value->first_parameter + parameter];
      if (item->owner_function != function || item->ordinal != parameter ||
          item->type_index == 0u || item->type_index > 3u ||
          !hir_text_valid(program, item->name) || item->name.count == 0u ||
          !hir_text_valid(program, item->label) ||
          item->label_kind > W_SEED_HIR0_LABEL_REQUIRED ||
          !hir_label_valid(item->label_kind, item->label, true) ||
         !span_valid(item->source_span,
                      program->modules[value->module_index].source_length))
         return false;
    }
    for (size_t prior = 0u; prior < function; prior += 1u)
      if (program->functions[prior].module_index == value->module_index &&
          hir_text_equal(program, program->functions[prior].name, value->name))
        return false;
    function_parameter_cursor += value->parameter_count;
  }
  if (function_parameter_cursor != program->parameter_count)
    return false;
  size_t block_instruction_cursor = 0u;
  for (size_t block = 0u; block < program->block_count; block += 1u) {
    const w_seed_hir0_block *value = &program->blocks[block];
    if (value->owner_function >= program->function_count ||
        value->ordinal != 0u || value->owner_function != block ||
         value->next_block != W_SEED_HIR0_NONE ||
         value->first_instruction != block_instruction_cursor ||
        !range_valid(value->first_instruction, value->instruction_count,
                     program->instruction_count) ||
        value->terminator_index != block ||
        !span_valid(value->source_span,
                    program->modules[program->functions[value->owner_function]
                                         .module_index]
                        .source_length))
      return false;
    block_instruction_cursor += value->instruction_count;
  }
  if (block_instruction_cursor != program->instruction_count)
    return false;
  size_t call_instruction_cursor = 0u;
  size_t binding_instruction_cursor = 0u;
  for (size_t instruction = 0u; instruction < program->instruction_count;
       instruction += 1u) {
    const w_seed_hir0_instruction *value = &program->instructions[instruction];
    if (value->owner_block >= program->block_count ||
        value->result_type >= program->type_count)
      return false;
    const w_seed_hir0_block *block = &program->blocks[value->owner_block];
    if (instruction < block->first_instruction ||
        instruction >= (size_t)block->first_instruction + block->instruction_count ||
        value->ordinal != instruction - block->first_instruction ||
        !span_valid(value->source_span,
                    program->modules[program->functions[block->owner_function]
                                         .module_index]
                        .source_length))
      return false;
    if (value->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (value->call_index == W_SEED_HIR0_NONE ||
          (size_t)value->call_index >= program->call_count ||
          value->call_index != call_instruction_cursor ||
          value->binding_index != W_SEED_HIR0_NONE ||
          program->calls[value->call_index].owner_instruction != instruction)
        return false;
      call_instruction_cursor += 1u;
    } else if (value->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (value->binding_index == W_SEED_HIR0_NONE ||
          (size_t)value->binding_index >= program->binding_count ||
          value->binding_index != binding_instruction_cursor ||
          value->result_type != 0u ||
          value->call_index != W_SEED_HIR0_NONE)
        return false;
      const w_seed_hir0_binding *binding =
          &program->bindings[value->binding_index];
      if (binding->owner_instruction != instruction ||
          binding->owner_block != value->owner_block ||
          binding->ordinal != value->ordinal || binding->type_index == 0u ||
          binding->type_index > 3u ||
          !hir_text_valid(program, binding->name) || binding->name.count == 0u ||
          binding->is_mutable ||
          binding->initializer_value >= program->value_count ||
          !span_valid(binding->source_span,
                      program->modules[program->functions[block->owner_function]
                                           .module_index]
                          .source_length))
        return false;
      binding_instruction_cursor += 1u;
    } else {
      return false;
    }
  }
  if (call_instruction_cursor != program->call_count ||
      binding_instruction_cursor != program->binding_count)
    return false;
  const size_t host_base = program->module_count + program->function_count +
                           program->entry_count;
  size_t call_argument_cursor = 0u;
  for (size_t call = 0u; call < program->call_count; call += 1u) {
    const w_seed_hir0_call *value = &program->calls[call];
    if (value->owner_instruction >= program->instruction_count ||
        value->owner_block >= program->block_count ||
        value->callee_identity >= program->identity_count ||
        (value->result_type != 0u && value->result_type != 2u &&
         value->result_type != 3u) ||
         value->first_argument != call_argument_cursor ||
         !range_valid(value->first_argument, value->argument_count,
                     program->argument_count) ||
        !span_valid(value->source_span,
                    program->modules[program->functions[
                                         program->blocks[value->owner_block]
                                             .owner_function]
                                         .module_index]
                        .source_length))
      return false;
    const w_seed_hir0_instruction *instruction =
        &program->instructions[value->owner_instruction];
    const w_seed_hir0_identity *identity = &program->identities[value->callee_identity];
    const bool host_call =
        identity->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE;
    const bool local_call = identity->kind == W_SEED_HIR0_IDENTITY_FUNCTION;
    if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
        instruction->call_index != call ||
        instruction->binding_index != W_SEED_HIR0_NONE ||
        instruction->owner_block != value->owner_block ||
        instruction->ordinal != value->ordinal ||
        instruction->result_type != value->result_type ||
        (!host_call && !local_call) ||
        identity->parameter_count != value->argument_count ||
        identity->return_type != value->result_type)
      return false;
    if (host_call) {
      if (value->callee_identity < host_base ||
          identity->first_requirement != value->first_requirement ||
          identity->requirement_count != value->requirement_count ||
          !range_valid(value->first_requirement, value->requirement_count,
                       program->requirement_count))
        return false;
    } else if (value->callee_identity !=
                   program->module_count + identity->target_index ||
               identity->target_index >= program->function_count ||
               value->first_requirement != W_SEED_HIR0_NONE ||
               value->requirement_count != 0u) {
      return false;
    }
    for (size_t argument = 0u; argument < value->argument_count; argument += 1u) {
      const w_seed_hir0_argument *item =
          &program->arguments[(size_t)value->first_argument + argument];
      if (item->owner_call != call || item->ordinal != argument ||
          item->parameter_ordinal >= identity->parameter_count ||
           item->value_index >= program->value_count ||
          !hir_text_valid(program, item->label) ||
          item->label_kind > W_SEED_HIR0_LABEL_REQUIRED ||
          !hir_label_valid(item->label_kind, item->label, true) ||
          !span_valid(item->source_span,
                      program->modules[program->functions[
                                           program->blocks[value->owner_block]
                                               .owner_function]
                                           .module_index]
                          .source_length))
        return false;
      for (size_t prior = 0u; prior < argument; prior += 1u) {
        const w_seed_hir0_argument *prior_item =
            &program->arguments[(size_t)value->first_argument + prior];
        if (prior_item->parameter_ordinal == item->parameter_ordinal)
          return false;
      }
      if (host_call) {
        const w_seed_hir0_host_parameter *parameter =
            &program->host_parameters[(size_t)identity->first_parameter +
                                      item->parameter_ordinal];
        if (item->type_index != parameter->type_index ||
            item->type_index != 1u ||
            item->label_kind != parameter->label_kind ||
            !hir_text_equal(program, item->label, parameter->label))
          return false;
      } else {
        const w_seed_hir0_parameter *parameter =
            &program->parameters[(size_t)identity->first_parameter +
                                 item->parameter_ordinal];
        if (parameter->owner_function != identity->target_index ||
            item->type_index != parameter->type_index ||
            item->label_kind != parameter->label_kind ||
            !hir_text_equal(program, item->label, parameter->label))
          return false;
      }
      call_argument_cursor += 1u;
    }
    for (size_t requirement = 0u; requirement < value->requirement_count;
         requirement += 1u) {
      const w_seed_hir0_requirement *item =
          &program->requirements[(size_t)value->first_requirement + requirement];
      if (item->owner_kind != W_SEED_HIR0_REQUIREMENT_HOST_IDENTITY ||
          item->owner_index != value->callee_identity || item->ordinal != requirement ||
          !hir_text_valid(program, item->name) || item->name.count == 0u)
        return false;
    }
  }
  if (call_argument_cursor != program->argument_count)
    return false;
  size_t value_byte_cursor = 0u;
  size_t value_cursor = 0u;
  size_t interpolation_segment_cursor = 0u;
  for (size_t instruction = 0u; instruction < program->instruction_count;
       instruction += 1u) {
    const w_seed_hir0_instruction *item = &program->instructions[instruction];
    if (item->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      const w_seed_hir0_binding *binding =
          &program->bindings[item->binding_index];
      const size_t function = program->blocks[item->owner_block].owner_function;
      const size_t module = program->functions[function].module_index;
      if (!verify_value_tree(
              program, binding->initializer_value,
              W_SEED_HIR0_VALUE_OWNER_BINDING, item->binding_index, 0u,
              item->owner_block, (uint32_t)instruction,
              program->modules[module].source_length, 0u, &value_cursor,
              &interpolation_segment_cursor, &value_byte_cursor))
        return false;
      const w_seed_hir0_value *initializer =
          &program->values[binding->initializer_value];
      if (initializer->type_index != binding->type_index) return false;
      continue;
    }
    const w_seed_hir0_call *call = &program->calls[item->call_index];
    for (size_t argument = 0u; argument < call->argument_count;
         argument += 1u) {
      const w_seed_hir0_argument *call_argument =
          &program->arguments[(size_t)call->first_argument + argument];
      const size_t function = program->blocks[item->owner_block].owner_function;
      const size_t module = program->functions[function].module_index;
      if (!verify_value_tree(
              program, call_argument->value_index,
              W_SEED_HIR0_VALUE_OWNER_ARGUMENT,
              (uint32_t)((size_t)call->first_argument + argument), 0u,
              item->owner_block, (uint32_t)instruction,
              program->modules[module].source_length, 0u, &value_cursor,
              &interpolation_segment_cursor, &value_byte_cursor))
        return false;
      const w_seed_hir0_value *root =
          &program->values[call_argument->value_index];
      if (root->type_index != call_argument->type_index) return false;
      if (root->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
        const w_seed_hir0_binding *binding =
            &program->bindings[root->binding_index];
        if (binding->owner_block != item->owner_block ||
            binding->owner_instruction >= call->owner_instruction ||
            binding->type_index != root->type_index)
          return false;
      }
    }
  }
  for (size_t terminator = 0u; terminator < program->terminator_count;
       terminator += 1u) {
    const w_seed_hir0_terminator *value = &program->terminators[terminator];
    if (value->owner_block != terminator ||
        value->ordinal != program->blocks[terminator].instruction_count ||
        !span_valid(value->source_span,
                    program->modules[program->functions[
                                         program->blocks[terminator].owner_function]
                                         .module_index]
                        .source_length))
      return false;
    const uint32_t function = program->blocks[terminator].owner_function;
    const uint32_t return_type = program->functions[function].return_type;
    if (return_type == 0u) {
      if (value->kind != W_SEED_HIR0_TERMINATOR_RETURN_UNIT ||
          value->value_index != W_SEED_HIR0_NONE || value->result_type != 0u)
        return false;
      continue;
    }
    if ((return_type != 2u && return_type != 3u) ||
        value->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE ||
        value->value_index >= program->value_count ||
        value->result_type != return_type ||
        !verify_value_tree(
            program, value->value_index, W_SEED_HIR0_VALUE_OWNER_TERMINATOR,
            (uint32_t)terminator, 0u, (uint32_t)terminator,
            (uint32_t)((size_t)program->blocks[terminator].first_instruction +
                       program->blocks[terminator].instruction_count),
            program->modules[program->functions[function].module_index]
                .source_length,
            0u, &value_cursor, &interpolation_segment_cursor,
            &value_byte_cursor))
      return false;
    if (program->values[value->value_index].type_index != return_type)
      return false;
  }
  if (value_cursor != program->value_count ||
      interpolation_segment_cursor != program->interpolation_segment_count ||
      value_byte_cursor != program->value_byte_count)
    return false;
  for (size_t entry = 0u; entry < program->entry_count; entry += 1u) {
    const w_seed_hir0_entry *value = &program->entries[entry];
    if (value->module_index >= program->module_count ||
        value->identity_index != program->module_count + program->function_count + entry ||
        value->target_function >= program->function_count ||
        value->target_identity != program->functions[value->target_function].identity_index ||
        program->functions[value->target_function].module_index != value->module_index ||
        !hir_text_valid(program, value->target_name) || value->target_name.count == 0u ||
        !hir_text_equal(program, value->target_name,
                        program->functions[value->target_function].name) ||
        !hir_text_is(program, value->slot, HIR0_SLOT_NAME) ||
        !span_valid(value->source_span,
                    program->modules[value->module_index].source_length))
      return false;
    const w_seed_hir0_module *module = &program->modules[value->module_index];
    if (entry < module->first_entry ||
        entry >= (size_t)module->first_entry + module->entry_count)
        return false;
  }
  return true;
}

bool w_seed_hir0_program_from_output(const w_seed_hir0_output *output,
                                     const w_seed_hir0_result *result,
                                     w_seed_hir0_program *program) {
  if (output == NULL || result == NULL || program == NULL ||
      result->status != W_SEED_HIR0_OK ||
      memcmp(result->schema, W_SEED_HIR0_SCHEMA_VERSION,
             sizeof(result->schema)) != 0)
    return false;
  const w_seed_hir0_counts counts = result->written;
  const w_seed_hir0_counts required = result->required;
  if (!hir_counts_equal(&counts, &required)) return false;
  hir0_memory_range output_ranges[32];
  size_t output_range_count = 0u;
  if (!output_range_table(output, output_ranges, &output_range_count) ||
      ranges_overlap(program, sizeof(*program), output, sizeof(*output)) ||
      ranges_overlap(program, sizeof(*program), result, sizeof(*result)) ||
      output_overlaps_memory(output_ranges, output_range_count, program,
                             sizeof(*program)))
    return false;
  w_seed_hir0_program candidate = {
      .modules = output->modules,
      .module_count = counts.modules,
      .module_capacity = output->module_capacity,
      .identities = output->identities,
      .identity_count = counts.identities,
      .identity_capacity = output->identity_capacity,
      .types = output->types,
      .type_count = counts.types,
      .type_capacity = output->type_capacity,
      .functions = output->functions,
      .function_count = counts.functions,
      .function_capacity = output->function_capacity,
      .parameters = output->parameters,
      .parameter_count = counts.parameters,
      .parameter_capacity = output->parameter_capacity,
      .blocks = output->blocks,
      .block_count = counts.blocks,
      .block_capacity = output->block_capacity,
      .instructions = output->instructions,
      .instruction_count = counts.instructions,
      .instruction_capacity = output->instruction_capacity,
      .bindings = output->bindings,
      .binding_count = counts.bindings,
      .binding_capacity = output->binding_capacity,
      .calls = output->calls,
      .call_count = counts.calls,
      .call_capacity = output->call_capacity,
      .host_parameters = output->host_parameters,
      .host_parameter_count = counts.host_parameters,
      .host_parameter_capacity = output->host_parameter_capacity,
      .arguments = output->arguments,
      .argument_count = counts.arguments,
      .argument_capacity = output->argument_capacity,
      .requirements = output->requirements,
      .requirement_count = counts.requirements,
      .requirement_capacity = output->requirement_capacity,
      .values = output->values,
      .value_count = counts.values,
      .value_capacity = output->value_capacity,
      .interpolation_segments = output->interpolation_segments,
      .interpolation_segment_count = counts.interpolation_segments,
      .interpolation_segment_capacity = output->interpolation_segment_capacity,
      .terminators = output->terminators,
      .terminator_count = counts.terminators,
      .terminator_capacity = output->terminator_capacity,
      .entries = output->entries,
      .entry_count = counts.entries,
      .entry_capacity = output->entry_capacity,
      .text_bytes = output->text_bytes,
      .text_byte_count = counts.text_bytes,
      .text_byte_capacity = output->text_byte_capacity,
      .value_bytes = output->value_bytes,
      .value_byte_count = counts.value_bytes,
      .value_byte_capacity = output->value_byte_capacity,
      .receipt = output->receipt,
      .receipt_count = counts.receipt_bytes,
      .receipt_capacity = output->receipt_capacity};
  if (!basic_program_shape(&candidate, result) || program_aliases(&candidate))
    return false;
  *program = candidate;
  return true;
}

bool w_seed_hir0_verify(const w_seed_hir0_program *program,
                        const w_seed_hir0_result *result) {
  if (!basic_program_shape(program, result) || program_aliases(program)) return false;
  w_seed_hir0_counts counts = {
      .modules = program->module_count,
      .identities = program->identity_count,
      .types = program->type_count,
      .functions = program->function_count,
      .parameters = program->parameter_count,
      .blocks = program->block_count,
      .instructions = program->instruction_count,
      .bindings = program->binding_count,
      .calls = program->call_count,
      .host_parameters = program->host_parameter_count,
      .arguments = program->argument_count,
      .requirements = program->requirement_count,
      .values = program->value_count,
      .interpolation_segments = program->interpolation_segment_count,
      .terminators = program->terminator_count,
      .entries = program->entry_count,
      .text_bytes = program->text_byte_count,
      .value_bytes = program->value_byte_count,
      .receipt_bytes = program->receipt_count};
  if (result->required.modules != counts.modules ||
      result->required.identities != counts.identities ||
      result->required.types != counts.types ||
      result->required.functions != counts.functions ||
      result->required.parameters != counts.parameters ||
      result->required.blocks != counts.blocks ||
      result->required.instructions != counts.instructions ||
      result->required.bindings != counts.bindings ||
      result->required.calls != counts.calls ||
      result->required.host_parameters != counts.host_parameters ||
      result->required.arguments != counts.arguments ||
      result->required.requirements != counts.requirements ||
      result->required.values != counts.values ||
      result->required.interpolation_segments != counts.interpolation_segments ||
      result->required.terminators != counts.terminators ||
      result->required.entries != counts.entries ||
      result->required.text_bytes != counts.text_bytes ||
      result->required.value_bytes != counts.value_bytes ||
      result->required.receipt_bytes != counts.receipt_bytes ||
      result->written.modules != counts.modules ||
      result->written.identities != counts.identities ||
      result->written.types != counts.types ||
      result->written.functions != counts.functions ||
      result->written.parameters != counts.parameters ||
      result->written.blocks != counts.blocks ||
      result->written.instructions != counts.instructions ||
      result->written.bindings != counts.bindings ||
      result->written.calls != counts.calls ||
      result->written.host_parameters != counts.host_parameters ||
      result->written.arguments != counts.arguments ||
      result->written.requirements != counts.requirements ||
      result->written.values != counts.values ||
      result->written.interpolation_segments != counts.interpolation_segments ||
      result->written.terminators != counts.terminators ||
      result->written.entries != counts.entries ||
      result->written.text_bytes != counts.text_bytes ||
      result->written.value_bytes != counts.value_bytes ||
      result->written.receipt_bytes != counts.receipt_bytes ||
      !verify_records(program))
    return false;
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  digest_program(program, &counts, semantic_digest);
  digest_provenance(program, &counts, provenance_digest);
  if (memcmp(semantic_digest, result->semantic_digest,
             sizeof(semantic_digest)) != 0 ||
      memcmp(provenance_digest, result->provenance_digest,
             sizeof(provenance_digest)) != 0)
    return false;
  uint8_t expected_receipt[HIR0_RECEIPT_BYTES];
  write_receipt_unchecked(expected_receipt, &counts, semantic_digest,
                          provenance_digest);
  if (memcmp(expected_receipt, program->receipt, HIR0_RECEIPT_BYTES) != 0)
    return false;
  return true;
}

w_seed_hir0_status w_seed_hir0_measure(const w_seed_hir0_input *input,
                                       w_seed_hir0_counts *counts,
                                       w_seed_hir0_result *result) {
  if (counts == NULL || result == NULL ||
      ranges_overlap(counts, sizeof(*counts), result, sizeof(*result)))
    return W_SEED_HIR0_INVALID;
  w_seed_hir0_counts candidate_counts;
  w_seed_hir0_result candidate_result;
  (void)memset(&candidate_counts, 0, sizeof(candidate_counts));
  (void)memset(&candidate_result, 0, sizeof(candidate_result));
  const hir0_prepare_status prepared = collect(input, &candidate_counts);
  if (prepared != HIR0_PREPARE_READY) {
    return prepared == HIR0_PREPARE_FRONTEND
               ? W_SEED_HIR0_FRONTEND
               : prepared == HIR0_PREPARE_UNSUPPORTED ? W_SEED_HIR0_UNSUPPORTED
                                                       : W_SEED_HIR0_INVALID;
  }
  (void)memcpy(candidate_result.schema, W_SEED_HIR0_SCHEMA_VERSION,
               sizeof(candidate_result.schema));
  candidate_result.required = candidate_counts;
  candidate_result.status = W_SEED_HIR0_OK;
  *counts = candidate_counts;
  *result = candidate_result;
  return W_SEED_HIR0_OK;
}

w_seed_hir0_status w_seed_hir0_run(const w_seed_hir0_input *input,
                                   w_seed_hir0_output *output,
                                   w_seed_hir0_result *result) {
  if (result == NULL) return W_SEED_HIR0_INVALID;
  w_seed_hir0_counts counts;
  w_seed_hir0_result measured;
  const w_seed_hir0_status measured_status =
      w_seed_hir0_measure(input, &counts, &measured);
  if (measured_status != W_SEED_HIR0_OK) {
    return measured_status;
  }
  if (output == NULL || !output_capacity_ok(output, &counts)) {
    return W_SEED_HIR0_CAPACITY;
  }
  if (output_aliases(output, &counts) ||
      output_overlaps_input(input, output, result)) {
    return W_SEED_HIR0_INVALID;
  }
  w_seed_hir0_program program;
  w_seed_hir0_result candidate_result = measured;
  candidate_result.written = counts;
  (void)memcpy(candidate_result.schema, W_SEED_HIR0_SCHEMA_VERSION,
               sizeof(candidate_result.schema));
  /* The bridge is structural here: records are still unwritten, but all
   * pointers, capacities, counts, and alias relations are already fixed. */
  if (!w_seed_hir0_program_from_output(output, &candidate_result, &program)) {
    return W_SEED_HIR0_INVALID;
  }
  /* All branches of emission are proven by collect() and the alias/capacity
   * preflight above. From this first write onward the commit is infallible. */
  emit_records(input, &counts, output);
  digest_program(&program, &counts, candidate_result.semantic_digest);
  digest_provenance(&program, &counts, candidate_result.provenance_digest);
  write_receipt_unchecked(output->receipt, &counts,
                          candidate_result.semantic_digest,
                          candidate_result.provenance_digest);
  candidate_result.status = W_SEED_HIR0_OK;
  *result = candidate_result;
  return W_SEED_HIR0_OK;
}
