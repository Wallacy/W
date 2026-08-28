#include "w_seed_hlo0.h"

#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "w-seed HLO0 requires 8-bit bytes");

enum {
  HLO0_RECEIPT_CAPACITY = 2048,
  HLO0_DIGEST_BYTES = 32,
  HLO0_NEWLINE_BYTES = 1,
};

static const char HLO0_PROFILE[] = "native-process@1";
static const char HLO0_SLOT[] = ".default";
static const char HLO0_ENTRY[] = "main";
static const char HLO0_CALLEE[] = "print";
static const char HLO0_REQUIREMENT[] = "Console";
static const char HLO0_PAYLOAD[] = "Hello, world!";

typedef struct {
  w_seed_hlo0_plan plan;
  uint8_t receipt[HLO0_RECEIPT_CAPACITY];
  size_t receipt_bytes;
} hlo0_candidate;

/* The frontend arrays are caller-owned and their ordinals are not semantic
 * identities.  Resolve the one accepted graph through its explicit edges and
 * retain the resulting indices only for this validation pass. */
typedef struct {
  size_t module_index;
  size_t function_index;
  size_t entry_index;
  size_t statement_index;
  size_t call_index;
  size_t identifier_index;
  size_t literal_index;
  size_t argument_index;
  size_t unit_type_index;
  size_t host_symbol_index;
} hlo0_graph;

static bool add_size(size_t left, size_t right, size_t *result) {
  if (result == NULL || right > SIZE_MAX - left) return false;
  *result = left + right;
  return true;
}

static bool mul_size(size_t left, size_t right, size_t *result) {
  if (result == NULL || (left != 0u && right > SIZE_MAX / left)) return false;
  *result = left * right;
  return true;
}

static bool range_valid(size_t start, size_t count, size_t total) {
  return start <= total && count <= total - start;
}

static bool optional_range_valid(uint32_t first, uint32_t count,
                                 size_t total) {
  if (count == 0u) return first == W_SEED_FRONTEND_NONE;
  return first != W_SEED_FRONTEND_NONE &&
         range_valid((size_t)first, (size_t)count, total);
}

static bool text_valid(w_seed_frontend_text text) {
  return text.length == 0u || text.data != NULL;
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

static bool span_valid(const w_seed_frontend_document *document,
                       w_seed_span span) {
  if (document == NULL || document->source == NULL) return false;
  w_seed_source_error error;
  return w_seed_source_validate_span(document->source, span, &error);
}

static bool span_contains(w_seed_span outer, w_seed_span inner) {
  return outer.start_byte <= inner.start_byte &&
         inner.end_byte <= outer.end_byte;
}

static bool count_u32(size_t value) { return value <= (size_t)UINT32_MAX; }

static bool counts_equal(const w_seed_frontend_counts *left,
                         const w_seed_frontend_counts *right) {
  if (left == NULL || right == NULL) return false;
  return left->modules == right->modules &&
         left->imports == right->imports &&
         left->import_items == right->import_items &&
         left->structs == right->structs && left->fields == right->fields &&
         left->type_declarations == right->type_declarations &&
         left->aliases == right->aliases && left->types == right->types &&
         left->functions == right->functions &&
         left->parameters == right->parameters &&
         left->entries == right->entries &&
         left->statements == right->statements &&
         left->expressions == right->expressions &&
         left->arguments == right->arguments &&
         left->symbols == right->symbols && left->facts == right->facts &&
         left->diagnostics == right->diagnostics &&
         left->diagnostic_facts == right->diagnostic_facts &&
         left->diagnostic_items == right->diagnostic_items &&
         left->diagnostic_labels == right->diagnostic_labels &&
         left->receipt_bytes == right->receipt_bytes &&
         left->enums == right->enums && left->enum_cases == right->enum_cases &&
         left->enum_case_parameters == right->enum_case_parameters &&
         left->switch_arms == right->switch_arms &&
         left->enum_subset_members == right->enum_subset_members &&
         left->enum_membership_cases == right->enum_membership_cases &&
         left->generic_parameters == right->generic_parameters &&
         left->generic_applications == right->generic_applications &&
         left->generic_arguments == right->generic_arguments &&
         left->typed_const_expressions == right->typed_const_expressions &&
         left->const_values == right->const_values &&
         left->const_elements == right->const_elements &&
         left->const_bytes == right->const_bytes &&
         left->const_declarations == right->const_declarations;
}

static bool output_array_capacity_valid(const w_seed_frontend_output *output,
                                        const w_seed_frontend_counts *counts) {
  if (output == NULL || counts == NULL) return false;
#define HLO0_ARRAY(field, capacity_field)                                      \
  if (counts->field > output->capacity_field ||                               \
      (counts->field != 0u && output->field == NULL))                         \
    return false
  HLO0_ARRAY(modules, module_capacity);
  HLO0_ARRAY(imports, import_capacity);
  HLO0_ARRAY(import_items, import_item_capacity);
  HLO0_ARRAY(structs, struct_capacity);
  HLO0_ARRAY(fields, field_capacity);
  HLO0_ARRAY(type_declarations, type_declaration_capacity);
  HLO0_ARRAY(aliases, alias_capacity);
  HLO0_ARRAY(types, type_capacity);
  HLO0_ARRAY(functions, function_capacity);
  HLO0_ARRAY(parameters, parameter_capacity);
  HLO0_ARRAY(entries, entry_capacity);
  HLO0_ARRAY(statements, statement_capacity);
  HLO0_ARRAY(expressions, expression_capacity);
  HLO0_ARRAY(arguments, argument_capacity);
  HLO0_ARRAY(symbols, symbol_capacity);
  HLO0_ARRAY(facts, fact_capacity);
  HLO0_ARRAY(diagnostics, diagnostic_capacity);
  HLO0_ARRAY(diagnostic_facts, diagnostic_fact_capacity);
  HLO0_ARRAY(diagnostic_items, diagnostic_item_capacity);
  HLO0_ARRAY(diagnostic_labels, diagnostic_label_capacity);
  HLO0_ARRAY(enums, enum_capacity);
  HLO0_ARRAY(enum_cases, enum_case_capacity);
  HLO0_ARRAY(enum_case_parameters, enum_case_parameter_capacity);
  HLO0_ARRAY(const_declarations, const_declaration_capacity);
  HLO0_ARRAY(switch_arms, switch_arm_capacity);
  HLO0_ARRAY(enum_subset_members, enum_subset_member_capacity);
  HLO0_ARRAY(enum_membership_cases, enum_membership_case_capacity);
  HLO0_ARRAY(generic_parameters, generic_parameter_capacity);
  HLO0_ARRAY(generic_applications, generic_application_capacity);
  HLO0_ARRAY(generic_arguments, generic_argument_capacity);
  HLO0_ARRAY(typed_const_expressions, typed_const_expression_capacity);
  HLO0_ARRAY(const_values, const_value_capacity);
  HLO0_ARRAY(const_elements, const_element_capacity);
#undef HLO0_ARRAY
  if (counts->const_bytes > output->const_bytes_capacity ||
      (counts->const_bytes != 0u && output->const_bytes == NULL) ||
      counts->receipt_bytes > output->receipt_capacity ||
      (counts->receipt_bytes != 0u && output->receipt == NULL))
    return false;
  return true;
}

static bool frontend_counts_u32(const w_seed_frontend_counts *counts) {
  if (counts == NULL) return false;
#define HLO0_COUNT(field)                                                       \
  if (!count_u32(counts->field)) return false
  HLO0_COUNT(modules);
  HLO0_COUNT(imports);
  HLO0_COUNT(import_items);
  HLO0_COUNT(structs);
  HLO0_COUNT(fields);
  HLO0_COUNT(type_declarations);
  HLO0_COUNT(aliases);
  HLO0_COUNT(types);
  HLO0_COUNT(functions);
  HLO0_COUNT(parameters);
  HLO0_COUNT(entries);
  HLO0_COUNT(statements);
  HLO0_COUNT(expressions);
  HLO0_COUNT(arguments);
  HLO0_COUNT(symbols);
  HLO0_COUNT(facts);
  HLO0_COUNT(diagnostics);
  HLO0_COUNT(diagnostic_facts);
  HLO0_COUNT(diagnostic_items);
  HLO0_COUNT(diagnostic_labels);
  HLO0_COUNT(enums);
  HLO0_COUNT(enum_cases);
  HLO0_COUNT(enum_case_parameters);
  HLO0_COUNT(switch_arms);
  HLO0_COUNT(enum_subset_members);
  HLO0_COUNT(enum_membership_cases);
  HLO0_COUNT(generic_parameters);
  HLO0_COUNT(generic_applications);
  HLO0_COUNT(generic_arguments);
  HLO0_COUNT(typed_const_expressions);
  HLO0_COUNT(const_values);
  HLO0_COUNT(const_elements);
  HLO0_COUNT(const_bytes);
  HLO0_COUNT(const_declarations);
  HLO0_COUNT(receipt_bytes);
#undef HLO0_COUNT
  return true;
}

static bool validate_document(const w_seed_frontend_document *document) {
  if (document == NULL || document->source == NULL ||
      document->nodes == NULL || document->node_count == 0u ||
      !text_valid(document->logical_source_id) ||
      !text_valid(document->module_id) ||
      !text_valid(document->local_module_name) ||
      document->parse.status != W_SEED_PARSE_COMPLETE ||
      document->parse.issue_count != 0u ||
      document->parse.root == W_SEED_CST_NONE ||
      (size_t)document->parse.root >= document->node_count ||
      document->parse.node_count != document->node_count ||
      document->parse.node_count > (size_t)UINT32_MAX ||
      document->parse.consumed_byte != document->source->bytes.length)
    return false;
  for (size_t index = 0u; index < document->node_count; index += 1u) {
    const w_seed_cst_node *node = &document->nodes[index];
    if (!span_valid(document, node->raw_span)) return false;
    if (node->first_child != W_SEED_CST_NONE &&
        (size_t)node->first_child >= document->node_count)
      return false;
    if (node->next_sibling != W_SEED_CST_NONE &&
        (size_t)node->next_sibling >= document->node_count)
      return false;
  }
  return true;
}

static bool validate_host_scope_shape(const w_seed_hlo0_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_input->host_scope != input->host_scope ||
      !text_valid(input->profile_identity))
    return false;
  if (input->host_scope == NULL) return true;
  const w_seed_frontend_host_prelude *scope = input->host_scope;
  if (!text_valid(scope->profile) || scope->profile.length == 0u ||
      scope->symbol_count > W_SEED_FRONTEND_MAX_HOST_SYMBOLS ||
      (scope->symbol_count != 0u && scope->symbols == NULL) ||
      !text_equal(input->profile_identity, scope->profile))
    return false;
  size_t total_parameters = 0u;
  for (size_t symbol_index = 0u; symbol_index < scope->symbol_count;
       symbol_index += 1u) {
    const w_seed_frontend_host_prelude_symbol *symbol =
        &scope->symbols[symbol_index];
    if (!text_valid(symbol->name) || symbol->name.length == 0u ||
        symbol->kind > W_SEED_FRONTEND_EXTERNAL_TYPE ||
        !text_valid(symbol->return_type) || symbol->return_type.length == 0u ||
        symbol->parameter_count > W_SEED_FRONTEND_MAX_HOST_PARAMETERS ||
        (symbol->parameter_count != 0u && symbol->parameters == NULL) ||
        symbol->requirement_count > W_SEED_FRONTEND_MAX_HOST_REQUIREMENTS ||
        (symbol->requirement_count != 0u && symbol->requirements == NULL))
      return false;
    if (!add_size(total_parameters, symbol->parameter_count,
                  &total_parameters) ||
        total_parameters > W_SEED_FRONTEND_MAX_HOST_PARAMETERS)
      return false;
    for (size_t prior = 0u; prior < symbol_index; prior += 1u) {
      if (text_equal(symbol->name, scope->symbols[prior].name)) return false;
    }
    for (size_t parameter_index = 0u;
         parameter_index < symbol->parameter_count; parameter_index += 1u) {
      const w_seed_frontend_external_parameter *parameter =
          &symbol->parameters[parameter_index];
      if (!text_valid(parameter->name) || !text_valid(parameter->type) ||
          parameter->label_kind > W_SEED_FRONTEND_LABEL_OPTIONAL)
        return false;
    }
    for (size_t requirement_index = 0u;
         requirement_index < symbol->requirement_count; requirement_index += 1u) {
      const w_seed_frontend_host_requirement *requirement =
          &symbol->requirements[requirement_index];
      if (!text_valid(requirement->name) || requirement->name.length == 0u)
        return false;
      for (size_t prior = 0u; prior < requirement_index; prior += 1u) {
        if (text_equal(requirement->name, symbol->requirements[prior].name))
          return false;
      }
    }
  }
  return true;
}

static bool validate_external_input_shape(const w_seed_hlo0_input *input) {
  if (input == NULL || input->frontend_input == NULL) return false;
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  if (frontend_input->external_module_count >
          (size_t)W_SEED_FRONTEND_MAX_EXTERNAL_MODULES ||
      frontend_input->external_module_count > (size_t)UINT32_MAX ||
      (frontend_input->external_module_count != 0u &&
       frontend_input->external_modules == NULL))
    return false;
  for (size_t module_index = 0u;
       module_index < frontend_input->external_module_count;
       module_index += 1u) {
    const w_seed_frontend_external_module *module =
        &frontend_input->external_modules[module_index];
    if (!text_valid(module->module_id) || module->module_id.length == 0u ||
        module->symbol_count >
            (size_t)W_SEED_FRONTEND_MAX_EXTERNAL_SYMBOLS ||
        module->symbol_count > (size_t)UINT32_MAX ||
        (module->symbol_count != 0u && module->symbols == NULL))
      return false;
    for (size_t prior = 0u; prior < module_index; prior += 1u) {
      if (text_equal(module->module_id,
                     frontend_input->external_modules[prior].module_id))
        return false;
    }
    for (size_t symbol_index = 0u; symbol_index < module->symbol_count;
         symbol_index += 1u) {
      const w_seed_frontend_external_symbol *symbol =
          &module->symbols[symbol_index];
      if (!text_valid(symbol->name) || symbol->name.length == 0u ||
          symbol->kind > W_SEED_FRONTEND_EXTERNAL_TYPE ||
          !text_valid(symbol->return_type) || symbol->return_type.length == 0u ||
          symbol->parameter_count >
              (size_t)W_SEED_FRONTEND_MAX_EXTERNAL_PARAMETERS ||
          symbol->parameter_count > (size_t)UINT32_MAX ||
          (symbol->parameter_count != 0u && symbol->parameters == NULL))
        return false;
      for (size_t prior = 0u; prior < symbol_index; prior += 1u) {
        if (text_equal(symbol->name, module->symbols[prior].name))
          return false;
      }
      for (size_t parameter_index = 0u;
           parameter_index < symbol->parameter_count; parameter_index += 1u) {
        const w_seed_frontend_external_parameter *parameter =
            &symbol->parameters[parameter_index];
        if (!text_valid(parameter->name) || parameter->name.length == 0u ||
            !text_valid(parameter->type) || parameter->type.length == 0u ||
            parameter->label_kind > W_SEED_FRONTEND_LABEL_OPTIONAL)
          return false;
      }
    }
  }
  return true;
}

static bool validate_host_scope_subset(const w_seed_hlo0_input *input,
                                       const hlo0_graph *graph) {
  if (input == NULL || graph == NULL || input->host_scope == NULL ||
      !text_is(input->profile_identity, HLO0_PROFILE) ||
      !text_is(input->host_scope->profile, HLO0_PROFILE) ||
      graph->host_symbol_index >= input->host_scope->symbol_count ||
      input->host_scope->symbols == NULL)
    return false;
  const w_seed_frontend_host_prelude_symbol *symbol =
      &input->host_scope->symbols[graph->host_symbol_index];
  if (!text_is(symbol->name, HLO0_CALLEE) ||
      symbol->kind != W_SEED_FRONTEND_EXTERNAL_VALUE ||
      !text_is(symbol->return_type, "()") || symbol->is_const ||
      symbol->parameter_count != 1u || symbol->parameters == NULL ||
      symbol->requirement_count != 1u || symbol->requirements == NULL ||
      !text_is(symbol->parameters[0].type, "String") ||
      symbol->parameters[0].label_kind !=
          W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY ||
      !text_is(symbol->requirements[0].name, HLO0_REQUIREMENT))
    return false;
  return true;
}

static bool validate_frontend_base(const w_seed_hlo0_input *input) {
  if (input == NULL || input->frontend_input == NULL ||
      input->frontend_output == NULL || input->frontend_result == NULL ||
      input->frontend_input->documents == NULL ||
      input->frontend_input->document_count != 1u ||
      !validate_host_scope_shape(input) ||
      !validate_external_input_shape(input) ||
      input->frontend_result->status != W_SEED_FRONTEND_OK ||
      !text_is(input->frontend_result->schema_version,
               W_SEED_FRONTEND_SCHEMA_VERSION) ||
      input->frontend_result->barrier_document != W_SEED_FRONTEND_NONE_SIZE ||
      input->frontend_result->primary_diagnostic !=
          W_SEED_FRONTEND_NONE_SIZE ||
      !counts_equal(&input->frontend_result->required,
                    &input->frontend_result->written) ||
      input->frontend_result->receipt_bytes !=
          input->frontend_result->written.receipt_bytes ||
      !frontend_counts_u32(&input->frontend_result->written) ||
      !output_array_capacity_valid(input->frontend_output,
                                   &input->frontend_result->written) ||
      !validate_document(&input->frontend_input->documents[0]))
    return false;
  const w_seed_frontend_document *document =
      &input->frontend_input->documents[0];
  if (document->source->bytes.length == 0u ||
      document->source->bytes.data == NULL)
    return false;
  return true;
}

static bool validate_module_records(const w_seed_hlo0_input *input,
                                    hlo0_graph *graph) {
  const w_seed_frontend_counts *counts = &input->frontend_result->written;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_document *document =
      &input->frontend_input->documents[0];
  if (graph == NULL || counts->modules != 1u || output->modules == NULL)
    return false;
  graph->module_index = 0u;
  const w_seed_frontend_module *module = &output->modules[graph->module_index];
  if (module->document_index != graph->module_index ||
      !text_valid(module->source_id) ||
      !text_valid(module->module_id) || !text_valid(module->local_module_name) ||
      !span_valid(document, module->span))
    return false;
#define HLO0_RANGE(first_field, count_field, total_field)                       \
  if (!range_valid((size_t)module->first_field,                            \
                   (size_t)module->count_field, counts->total_field))      \
    return false
  HLO0_RANGE(first_import, import_count, imports);
  HLO0_RANGE(first_struct, struct_count, structs);
  HLO0_RANGE(first_type_declaration, type_declaration_count,
             type_declarations);
  HLO0_RANGE(first_alias, alias_count, aliases);
  HLO0_RANGE(first_function, function_count, functions);
  HLO0_RANGE(first_entry, entry_count, entries);
  HLO0_RANGE(first_enum, enum_count, enums);
  HLO0_RANGE(first_const_declaration, const_declaration_count,
             const_declarations);
#undef HLO0_RANGE
  if (module->function_count != 1u || module->entry_count != 1u ||
      module->struct_count != 0u ||
      module->type_declaration_count != 0u || module->alias_count != 0u ||
      module->enum_count != 0u || module->const_declaration_count != 0u)
    return false;
  graph->function_index = (size_t)module->first_function;
  graph->entry_index = (size_t)module->first_entry;
  if (graph->function_index >= counts->functions ||
      graph->entry_index >= counts->entries)
    return false;
  return true;
}

static bool validate_type_record(const w_seed_hlo0_input *input,
                                 const w_seed_frontend_type *type) {
  const w_seed_frontend_document *document =
      &input->frontend_input->documents[0];
  if (type == NULL || !span_valid(document, type->span) ||
      !text_valid(type->spelling) || !text_valid(type->nominal_name) ||
      type->kind > W_SEED_FRONTEND_TYPE_RANGE ||
      (type->element_type != W_SEED_FRONTEND_NONE &&
       (size_t)type->element_type >= input->frontend_result->written.types) ||
      (type->return_type != W_SEED_FRONTEND_NONE &&
       (size_t)type->return_type >= input->frontend_result->written.types) ||
      !optional_range_valid(type->first_parameter, type->parameter_count,
                            input->frontend_result->written.parameters) ||
      (type->enum_base_index != W_SEED_FRONTEND_NONE &&
       (size_t)type->enum_base_index >= input->frontend_result->written.enums) ||
      !optional_range_valid(type->first_subset_member,
                            type->subset_member_count,
                            input->frontend_result->written.enum_subset_members) ||
      (type->generic_application_index != W_SEED_FRONTEND_NONE &&
       (size_t)type->generic_application_index >=
           input->frontend_result->written.generic_applications))
    return false;
  return true;
}

static bool validate_function_records(const w_seed_hlo0_input *input,
                                      hlo0_graph *graph) {
  const w_seed_frontend_counts *counts = &input->frontend_result->written;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_document *document =
      &input->frontend_input->documents[0];
  if (graph == NULL || counts->functions != 1u || output->functions == NULL ||
      graph->function_index >= counts->functions)
    return false;
  const w_seed_frontend_function *function =
      &output->functions[graph->function_index];
  if (function->module_index != graph->module_index ||
      !text_is(function->name, HLO0_ENTRY) ||
      function->exported || function->is_const ||
      function->const_body_supported || function->is_async ||
      function->is_throws || function->is_unsafe ||
      function->has_borrow_clause || !span_valid(document, function->span) ||
      !span_valid(document, function->body_span) ||
      !span_contains(function->span, function->body_span) ||
      function->parameter_count != 0u ||
      function->return_type >= counts->types ||
      function->statement_count != 1u ||
      !range_valid((size_t)function->first_parameter,
                   (size_t)function->parameter_count, counts->parameters) ||
      !range_valid((size_t)function->first_statement,
                   (size_t)function->statement_count, counts->statements))
    return false;
  graph->unit_type_index = (size_t)function->return_type;
  graph->statement_index = (size_t)function->first_statement;
  if (graph->statement_index >= counts->statements) return false;
  return true;
}

static bool validate_entry_records(const w_seed_hlo0_input *input,
                                   const hlo0_graph *graph) {
  const w_seed_frontend_counts *counts = &input->frontend_result->written;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_document *document =
      &input->frontend_input->documents[0];
  if (graph == NULL || counts->entries != 1u || output->entries == NULL ||
      graph->entry_index >= counts->entries)
    return false;
  const w_seed_frontend_entry *entry =
      &output->entries[graph->entry_index];
  return entry->module_index == graph->module_index && entry->valid &&
         text_is(entry->target, HLO0_ENTRY) && span_valid(document, entry->span);
}

static bool validate_statement_records(const w_seed_hlo0_input *input,
                                       hlo0_graph *graph) {
  const w_seed_frontend_counts *counts = &input->frontend_result->written;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_document *document =
      &input->frontend_input->documents[0];
  if (graph == NULL || counts->statements != 1u ||
      output->statements == NULL || graph->statement_index >= counts->statements)
    return false;
  const w_seed_frontend_statement *statement =
      &output->statements[graph->statement_index];
  if (statement->kind != W_SEED_FRONTEND_STMT_EXPRESSION ||
      statement->module_index != graph->module_index ||
      statement->owner_function != graph->function_index ||
      !span_valid(document, statement->span) ||
      statement->expression_index == W_SEED_FRONTEND_NONE ||
      statement->expression_index >= counts->expressions ||
      statement->condition_expression != W_SEED_FRONTEND_NONE ||
      statement->first_child != W_SEED_FRONTEND_NONE ||
      statement->child_count != 0u || statement->binding_name.length != 0u ||
      statement->declared_type != W_SEED_FRONTEND_NONE ||
      statement->next_sibling != W_SEED_FRONTEND_NONE ||
      statement->else_child != W_SEED_FRONTEND_NONE ||
      statement->range_lower_expression != W_SEED_FRONTEND_NONE ||
      statement->range_upper_expression != W_SEED_FRONTEND_NONE ||
      statement->loop_local_ordinal != W_SEED_FRONTEND_NONE)
    return false;
  graph->call_index = (size_t)statement->expression_index;
  return true;
}

static bool identity_fields_valid(const w_seed_hlo0_input *input,
                                  const w_seed_frontend_expression *expression) {
  const w_seed_frontend_counts *counts = &input->frontend_result->written;
  switch (expression->resolved_callee_kind) {
    case W_SEED_FRONTEND_CALLEE_NONE:
      return expression->resolved_host_symbol_index == W_SEED_FRONTEND_NONE &&
             expression->resolved_external_module_index ==
                 W_SEED_FRONTEND_NONE &&
             expression->resolved_external_symbol_index ==
                 W_SEED_FRONTEND_NONE &&
             expression->resolved_function_index == W_SEED_FRONTEND_NONE;
    case W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION:
      return expression->resolved_function_index != W_SEED_FRONTEND_NONE &&
             (size_t)expression->resolved_function_index < counts->functions &&
             expression->resolved_host_symbol_index == W_SEED_FRONTEND_NONE &&
             expression->resolved_external_module_index ==
                 W_SEED_FRONTEND_NONE &&
             expression->resolved_external_symbol_index ==
                 W_SEED_FRONTEND_NONE;
    case W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL:
      return input->host_scope != NULL &&
             expression->resolved_function_index == W_SEED_FRONTEND_NONE &&
             expression->resolved_host_symbol_index != W_SEED_FRONTEND_NONE &&
             (size_t)expression->resolved_host_symbol_index <
                 input->host_scope->symbol_count &&
             expression->resolved_external_module_index ==
                 W_SEED_FRONTEND_NONE &&
             expression->resolved_external_symbol_index ==
                 W_SEED_FRONTEND_NONE;
    case W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL:
      if (input->frontend_input == NULL ||
          expression->resolved_external_module_index ==
              W_SEED_FRONTEND_NONE ||
          (size_t)expression->resolved_external_module_index >=
              input->frontend_input->external_module_count ||
          input->frontend_input->external_modules == NULL)
        return false;
      {
        const w_seed_frontend_external_module *module =
            &input->frontend_input
                 ->external_modules[expression->resolved_external_module_index];
        if (expression->resolved_external_symbol_index ==
                W_SEED_FRONTEND_NONE ||
            (size_t)expression->resolved_external_symbol_index >=
                module->symbol_count ||
            module->symbols == NULL)
          return false;
      }
      return expression->resolved_function_index == W_SEED_FRONTEND_NONE &&
             expression->resolved_host_symbol_index == W_SEED_FRONTEND_NONE &&
             expression->resolved_external_module_index !=
                 W_SEED_FRONTEND_NONE &&
             expression->resolved_external_symbol_index !=
                 W_SEED_FRONTEND_NONE;
  }
  return false;
}

static bool validate_expression_records(const w_seed_hlo0_input *input,
                                       hlo0_graph *graph) {
  const w_seed_frontend_counts *counts = &input->frontend_result->written;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_document *document =
      &input->frontend_input->documents[0];
  if (graph == NULL || counts->expressions != 3u ||
      output->expressions == NULL || graph->call_index >= counts->expressions)
    return false;
  for (size_t index = 0u; index < counts->expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &output->expressions[index];
    if (!span_valid(document, expression->span) ||
        !text_valid(expression->spelling) ||
        !text_valid(expression->operator_text) ||
        expression->module_index != graph->module_index ||
        expression->owner_function != graph->function_index ||
        (expression->left != W_SEED_FRONTEND_NONE &&
         (size_t)expression->left >= counts->expressions) ||
        (expression->right != W_SEED_FRONTEND_NONE &&
         (size_t)expression->right >= counts->expressions) ||
        !optional_range_valid(expression->first_argument,
                              expression->argument_count, counts->arguments) ||
        (expression->inferred_type != W_SEED_FRONTEND_NONE &&
         (size_t)expression->inferred_type >= counts->types) ||
        !identity_fields_valid(input, expression) ||
        (expression->const_byte_offset == W_SEED_FRONTEND_NONE
             ? expression->const_byte_count != 0u
             : !range_valid((size_t)expression->const_byte_offset,
                            (size_t)expression->const_byte_count,
                            counts->const_bytes)))
      return false;
  }
  const w_seed_frontend_expression *call =
      &output->expressions[graph->call_index];
  if (call->kind != W_SEED_FRONTEND_EXPR_CALL || !call->supported ||
      call->left == W_SEED_FRONTEND_NONE ||
      (size_t)call->left >= counts->expressions ||
      call->right != W_SEED_FRONTEND_NONE ||
      call->first_argument == W_SEED_FRONTEND_NONE ||
      call->argument_count != 1u ||
      !range_valid((size_t)call->first_argument, 1u, counts->arguments) ||
      call->inferred_type != graph->unit_type_index ||
      call->resolved_callee_kind !=
          W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL ||
      call->resolved_host_symbol_index == W_SEED_FRONTEND_NONE ||
      call->resolved_function_index != W_SEED_FRONTEND_NONE ||
      call->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
      call->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
      call->const_byte_offset != W_SEED_FRONTEND_NONE ||
      call->const_byte_count != 0u)
    return false;
  graph->identifier_index = (size_t)call->left;
  graph->argument_index = (size_t)call->first_argument;
  graph->host_symbol_index = (size_t)call->resolved_host_symbol_index;
  if (graph->identifier_index == graph->call_index ||
      graph->argument_index >= counts->arguments || output->arguments == NULL)
    return false;
  const w_seed_frontend_expression *identifier =
      &output->expressions[graph->identifier_index];
  if (identifier->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER ||
      !identifier->supported || !text_is(identifier->spelling, HLO0_CALLEE) ||
      identifier->left != W_SEED_FRONTEND_NONE ||
      identifier->right != W_SEED_FRONTEND_NONE ||
      identifier->first_argument != W_SEED_FRONTEND_NONE ||
      identifier->argument_count != 0u ||
      identifier->inferred_type != W_SEED_FRONTEND_NONE ||
      identifier->resolved_callee_kind !=
          W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL ||
      identifier->resolved_host_symbol_index !=
          call->resolved_host_symbol_index ||
      identifier->resolved_function_index != W_SEED_FRONTEND_NONE ||
      identifier->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
      identifier->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
      identifier->const_byte_offset != W_SEED_FRONTEND_NONE ||
      identifier->const_byte_count != 0u)
    return false;
  const w_seed_frontend_argument *argument =
      &output->arguments[graph->argument_index];
  if (argument->expression_index == W_SEED_FRONTEND_NONE ||
      argument->expression_index >= counts->expressions)
    return false;
  graph->literal_index = (size_t)argument->expression_index;
  if (graph->literal_index == graph->call_index ||
      graph->literal_index == graph->identifier_index)
    return false;
  const w_seed_frontend_expression *literal =
      &output->expressions[graph->literal_index];
  if (literal->kind != W_SEED_FRONTEND_EXPR_STRING || !literal->supported ||
      literal->left != W_SEED_FRONTEND_NONE ||
      literal->right != W_SEED_FRONTEND_NONE ||
      literal->first_argument != W_SEED_FRONTEND_NONE ||
      literal->argument_count != 0u ||
      literal->inferred_type != W_SEED_FRONTEND_NONE ||
      literal->resolved_callee_kind != W_SEED_FRONTEND_CALLEE_NONE ||
      literal->resolved_host_symbol_index != W_SEED_FRONTEND_NONE ||
      literal->resolved_function_index != W_SEED_FRONTEND_NONE ||
      literal->resolved_external_module_index != W_SEED_FRONTEND_NONE ||
      literal->resolved_external_symbol_index != W_SEED_FRONTEND_NONE ||
      literal->const_byte_offset == W_SEED_FRONTEND_NONE ||
      literal->const_byte_count != sizeof(HLO0_PAYLOAD) - 1u ||
      literal->const_byte_offset > counts->const_bytes ||
      literal->const_byte_count >
          counts->const_bytes - literal->const_byte_offset ||
      output->const_bytes == NULL ||
      memcmp(output->const_bytes + literal->const_byte_offset, HLO0_PAYLOAD,
             literal->const_byte_count) != 0)
    return false;
  return true;
}

static bool validate_argument_records(const w_seed_hlo0_input *input,
                                      const hlo0_graph *graph) {
  const w_seed_frontend_counts *counts = &input->frontend_result->written;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_document *document =
      &input->frontend_input->documents[0];
  if (graph == NULL || counts->arguments != 1u || output->arguments == NULL ||
      graph->argument_index >= counts->arguments)
    return false;
  const w_seed_frontend_argument *argument =
      &output->arguments[graph->argument_index];
  if (argument->module_index != graph->module_index ||
      argument->owner_expression != graph->identifier_index ||
      argument->expression_index != graph->literal_index ||
      argument->expression_index == W_SEED_FRONTEND_NONE ||
      argument->expression_index >= counts->expressions ||
      argument->label.length != 0u ||
      argument->label.data != NULL ||
      argument->resolved_parameter_ordinal != 0u ||
      !span_valid(document, argument->span))
    return false;
  return true;
}

static bool validate_symbol_records(const w_seed_hlo0_input *input) {
  const w_seed_frontend_counts *counts = &input->frontend_result->written;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_document *document =
      &input->frontend_input->documents[0];
  if (counts->symbols != 3u || output->symbols == NULL) return false;
  bool saw_module = false;
  bool saw_function = false;
  bool saw_entry = false;
  for (size_t index = 0u; index < counts->symbols; index += 1u) {
    const w_seed_frontend_symbol *symbol = &output->symbols[index];
    if (symbol->module_index != 0u || !span_valid(document, symbol->span) ||
        !text_valid(symbol->name))
      return false;
    if (symbol->kind == W_SEED_FRONTEND_SYMBOL_MODULE) {
      if (saw_module || symbol->owner_index != 0u ||
          !text_equal(symbol->name, document->module_id))
        return false;
      saw_module = true;
    } else if (symbol->kind == W_SEED_FRONTEND_SYMBOL_FUNCTION) {
      if (saw_function || symbol->owner_index != 0u ||
          !text_is(symbol->name, HLO0_ENTRY))
        return false;
      saw_function = true;
    } else if (symbol->kind == W_SEED_FRONTEND_SYMBOL_ENTRY) {
      if (saw_entry || symbol->owner_index != 0u ||
          !text_is(symbol->name, HLO0_ENTRY))
        return false;
      saw_entry = true;
    } else {
      return false;
    }
  }
  return saw_module && saw_function && saw_entry;
}

static bool index_or_none(uint32_t index, size_t total) {
  return index == W_SEED_FRONTEND_NONE || (size_t)index < total;
}

/* Validate the record graph before the exact HLO0 subset check. This keeps
 * malformed caller-owned records INVALID while a well-formed larger graph
 * can be reported as UNSUPPORTED. */
static bool validate_frontend_coherence(const w_seed_hlo0_input *input) {
  const w_seed_frontend_counts *counts = &input->frontend_result->written;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_document *document =
      &input->frontend_input->documents[0];
  for (size_t index = 0u; index < counts->modules; index += 1u) {
    const w_seed_frontend_module *module = &output->modules[index];
    if (module->document_index >= input->frontend_input->document_count ||
        !span_valid(document, module->span) || !text_valid(module->source_id) ||
        !text_valid(module->module_id) ||
        !text_valid(module->local_module_name) ||
        !range_valid((size_t)module->first_import,
                     (size_t)module->import_count, counts->imports) ||
        !range_valid((size_t)module->first_struct,
                     (size_t)module->struct_count, counts->structs) ||
        !range_valid((size_t)module->first_type_declaration,
                     (size_t)module->type_declaration_count,
                     counts->type_declarations) ||
        !range_valid((size_t)module->first_alias,
                     (size_t)module->alias_count, counts->aliases) ||
        !range_valid((size_t)module->first_function,
                     (size_t)module->function_count, counts->functions) ||
        !range_valid((size_t)module->first_entry,
                     (size_t)module->entry_count, counts->entries) ||
        !range_valid((size_t)module->first_enum,
                     (size_t)module->enum_count, counts->enums) ||
        !range_valid((size_t)module->first_const_declaration,
                     (size_t)module->const_declaration_count,
                     counts->const_declarations))
      return false;
  }
  for (size_t index = 0u; index < counts->types; index += 1u) {
    if (!validate_type_record(input, &output->types[index])) return false;
  }
  for (size_t index = 0u; index < counts->functions; index += 1u) {
    const w_seed_frontend_function *function = &output->functions[index];
    if (function->module_index >= counts->modules ||
        !span_valid(document, function->span) ||
        !span_valid(document, function->body_span) ||
        !span_contains(function->span, function->body_span) ||
        !text_valid(function->name) ||
        !range_valid((size_t)function->first_parameter,
                     (size_t)function->parameter_count, counts->parameters) ||
        !range_valid((size_t)function->first_statement,
                     (size_t)function->statement_count, counts->statements) ||
        !index_or_none(function->return_type, counts->types))
       return false;
  }
  for (size_t index = 0u; index < counts->parameters; index += 1u) {
    const w_seed_frontend_parameter *parameter = &output->parameters[index];
    if (parameter->module_index >= counts->modules ||
        parameter->owner_function >= counts->functions ||
        !span_valid(document, parameter->span) || !text_valid(parameter->name) ||
        !text_valid(parameter->label) ||
        parameter->label_kind > W_SEED_FRONTEND_LABEL_OPTIONAL ||
        !index_or_none(parameter->type_index, counts->types))
       return false;
  }
  for (size_t index = 0u; index < counts->entries; index += 1u) {
    const w_seed_frontend_entry *entry = &output->entries[index];
    if (entry->module_index >= counts->modules ||
        !span_valid(document, entry->span) || !text_valid(entry->target))
       return false;
  }
  for (size_t index = 0u; index < counts->statements; index += 1u) {
    const w_seed_frontend_statement *statement = &output->statements[index];
    if (statement->module_index >= counts->modules ||
        statement->owner_function >= counts->functions ||
        !span_valid(document, statement->span) ||
        !index_or_none(statement->expression_index, counts->expressions) ||
        !index_or_none(statement->condition_expression, counts->expressions) ||
        !optional_range_valid(statement->first_child, statement->child_count,
                              counts->statements) ||
        !index_or_none(statement->next_sibling, counts->statements) ||
        !index_or_none(statement->else_child, counts->statements) ||
        !index_or_none(statement->range_lower_expression, counts->expressions) ||
        !index_or_none(statement->range_upper_expression, counts->expressions) ||
        !text_valid(statement->binding_name) ||
        !index_or_none(statement->declared_type, counts->types) ||
        !index_or_none(statement->loop_local_ordinal, counts->parameters))
       return false;
  }
  for (size_t index = 0u; index < counts->expressions; index += 1u) {
    const w_seed_frontend_expression *expression = &output->expressions[index];
    const bool basic =
        expression->module_index < counts->modules &&
        (expression->owner_function == W_SEED_FRONTEND_NONE ||
         expression->owner_function < counts->functions) &&
        span_valid(document, expression->span) &&
        text_valid(expression->spelling) && text_valid(expression->operator_text) &&
        index_or_none(expression->left, counts->expressions) &&
        index_or_none(expression->right, counts->expressions) &&
        optional_range_valid(expression->first_argument,
                             expression->argument_count, counts->arguments) &&
        index_or_none(expression->inferred_type, counts->types) &&
        identity_fields_valid(input, expression);
    const bool bytes =
        expression->const_byte_offset == W_SEED_FRONTEND_NONE
            ? expression->const_byte_count == 0u
            : range_valid((size_t)expression->const_byte_offset,
                          (size_t)expression->const_byte_count,
                          counts->const_bytes);
    if (!basic || !bytes) return false;
  }
  for (size_t index = 0u; index < counts->arguments; index += 1u) {
    const w_seed_frontend_argument *argument = &output->arguments[index];
    if (argument->module_index >= counts->modules ||
        argument->owner_expression >= counts->expressions ||
        argument->expression_index >= counts->expressions ||
        !span_valid(document, argument->span) || !text_valid(argument->label))
       return false;
  }
  for (size_t index = 0u; index < counts->symbols; index += 1u) {
    const w_seed_frontend_symbol *symbol = &output->symbols[index];
    if (symbol->module_index >= counts->modules ||
        symbol->kind > W_SEED_FRONTEND_SYMBOL_CONST ||
        !span_valid(document, symbol->span) || !text_valid(symbol->name) ||
        !index_or_none(symbol->type_index, counts->types))
       return false;
  }
  return true;
}

static bool validate_frontend_records(const w_seed_hlo0_input *input,
                                      hlo0_graph *graph) {
  const w_seed_frontend_counts *counts = &input->frontend_result->written;
  const w_seed_frontend_output *output = input->frontend_output;
  const w_seed_frontend_document *document =
      &input->frontend_input->documents[0];
  if (graph == NULL || !validate_module_records(input, graph) ||
      !validate_function_records(input, graph) ||
      !validate_entry_records(input, graph) ||
      !validate_statement_records(input, graph) ||
      !validate_expression_records(input, graph) ||
      !validate_argument_records(input, graph) ||
      !validate_symbol_records(input))
    return false;
  if (counts->types != 1u ||
      output->types == NULL || graph->unit_type_index >= counts->types ||
      !validate_type_record(input, &output->types[graph->unit_type_index]) ||
      output->types[graph->unit_type_index].kind != W_SEED_FRONTEND_TYPE_UNIT ||
      output->types[graph->unit_type_index].element_type !=
          W_SEED_FRONTEND_NONE ||
      output->types[graph->unit_type_index].return_type !=
          W_SEED_FRONTEND_NONE ||
      output->types[graph->unit_type_index].first_parameter !=
          W_SEED_FRONTEND_NONE ||
      output->types[graph->unit_type_index].parameter_count != 0u ||
      !text_is(output->types[graph->unit_type_index].spelling, "()") ||
      counts->const_bytes != sizeof(HLO0_PAYLOAD) - 1u ||
      output->const_bytes == NULL ||
      memcmp(output->const_bytes, HLO0_PAYLOAD, counts->const_bytes) != 0 ||
      counts->facts != 0u || counts->diagnostics != 0u ||
      counts->imports != 0u || counts->import_items != 0u ||
      counts->structs != 0u || counts->fields != 0u ||
      counts->type_declarations != 0u || counts->aliases != 0u ||
      counts->parameters != 0u || counts->enums != 0u ||
      counts->enum_cases != 0u || counts->enum_case_parameters != 0u ||
      counts->switch_arms != 0u || counts->enum_subset_members != 0u ||
      counts->enum_membership_cases != 0u || counts->generic_parameters != 0u ||
      counts->generic_applications != 0u || counts->generic_arguments != 0u ||
      counts->typed_const_expressions != 0u || counts->const_values != 0u ||
      counts->const_elements != 0u || counts->const_declarations != 0u ||
      !text_equal(output->modules[graph->module_index].module_id,
                  document->module_id))
    return false;
  return true;
}

static bool range_end(uintptr_t start, size_t length, uintptr_t *end) {
  if (end == NULL || length > UINTPTR_MAX - start) return false;
  *end = start + (uintptr_t)length;
  return true;
}

/* The plan copies text and payload. Receipt and plan storage must still not
 * overlap any caller-owned frontend/source/host storage. Overflow is treated
 * as an unsafe alias rather than being wrapped. */
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

static bool array_bytes(size_t count, size_t element_size, size_t *bytes) {
  return mul_size(count, element_size, bytes);
}

static bool output_aliases_range(const w_seed_hlo0_output *output,
                                 const void *input_buffer,
                                 size_t input_length) {
  if (output == NULL || input_buffer == NULL || input_length == 0u) return false;
  if (output->plans != NULL &&
      ranges_overlap(output->plans, sizeof(w_seed_hlo0_plan), input_buffer,
                     input_length))
    return true;
  return output->receipt != NULL &&
         ranges_overlap(output->receipt, output->receipt_capacity,
                        input_buffer, input_length);
}

static bool output_aliases_input(const w_seed_hlo0_input *input,
                                 const w_seed_hlo0_output *output) {
  if (input == NULL || output == NULL) return true;
  if (output->plans != NULL &&
      ranges_overlap(output->plans, sizeof(w_seed_hlo0_plan), output->receipt,
                     output->receipt_capacity))
    return true;
#define HLO0_OBJECT(value)                                                      \
  if (output_aliases_range(output, &(value), sizeof(value))) return true
  HLO0_OBJECT(*input);
  HLO0_OBJECT(*input->frontend_input);
  HLO0_OBJECT(*input->frontend_output);
  HLO0_OBJECT(*input->frontend_result);
  if (input->host_scope != NULL) {
    HLO0_OBJECT(*input->host_scope);
  }
#undef HLO0_OBJECT
  const w_seed_frontend_input *frontend_input = input->frontend_input;
  const w_seed_frontend_output *frontend = input->frontend_output;
  const w_seed_frontend_counts *counts = &input->frontend_result->written;
#define HLO0_ARRAY(pointer, count, type)                                        \
  do {                                                                          \
    size_t bytes = 0u;                                                          \
    if (!array_bytes((count), sizeof(type), &bytes) ||                          \
        output_aliases_range(output, (pointer), bytes))                         \
      return true;                                                              \
  } while (0)
  HLO0_ARRAY(frontend_input->documents, frontend_input->document_count,
             w_seed_frontend_document);
  HLO0_ARRAY(frontend_input->external_modules,
             frontend_input->external_module_count,
             w_seed_frontend_external_module);
  HLO0_ARRAY(frontend->modules, counts->modules, w_seed_frontend_module);
  HLO0_ARRAY(frontend->imports, counts->imports, w_seed_frontend_import);
  HLO0_ARRAY(frontend->import_items, counts->import_items,
             w_seed_frontend_import_item);
  HLO0_ARRAY(frontend->structs, counts->structs, w_seed_frontend_struct);
  HLO0_ARRAY(frontend->fields, counts->fields, w_seed_frontend_field);
  HLO0_ARRAY(frontend->type_declarations, counts->type_declarations,
             w_seed_frontend_type_declaration);
  HLO0_ARRAY(frontend->aliases, counts->aliases, w_seed_frontend_alias);
  HLO0_ARRAY(frontend->types, counts->types, w_seed_frontend_type);
  HLO0_ARRAY(frontend->functions, counts->functions, w_seed_frontend_function);
  HLO0_ARRAY(frontend->parameters, counts->parameters,
             w_seed_frontend_parameter);
  HLO0_ARRAY(frontend->entries, counts->entries, w_seed_frontend_entry);
  HLO0_ARRAY(frontend->statements, counts->statements,
             w_seed_frontend_statement);
  HLO0_ARRAY(frontend->expressions, counts->expressions,
             w_seed_frontend_expression);
  HLO0_ARRAY(frontend->arguments, counts->arguments, w_seed_frontend_argument);
  HLO0_ARRAY(frontend->symbols, counts->symbols, w_seed_frontend_symbol);
  HLO0_ARRAY(frontend->facts, counts->facts, w_seed_frontend_fact);
  HLO0_ARRAY(frontend->diagnostics, counts->diagnostics,
             w_seed_frontend_diagnostic);
  HLO0_ARRAY(frontend->diagnostic_facts, counts->diagnostic_facts,
             w_seed_frontend_diagnostic_fact);
  HLO0_ARRAY(frontend->diagnostic_items, counts->diagnostic_items,
             w_seed_frontend_diagnostic_item);
  HLO0_ARRAY(frontend->diagnostic_labels, counts->diagnostic_labels,
             w_seed_frontend_diagnostic_label);
  HLO0_ARRAY(frontend->const_bytes, counts->const_bytes, uint8_t);
  HLO0_ARRAY(frontend->receipt, counts->receipt_bytes, uint8_t);
  HLO0_ARRAY(frontend->enums, counts->enums, w_seed_frontend_enum);
  HLO0_ARRAY(frontend->enum_cases, counts->enum_cases,
             w_seed_frontend_enum_case);
  HLO0_ARRAY(frontend->enum_case_parameters, counts->enum_case_parameters,
             w_seed_frontend_enum_case_parameter);
  HLO0_ARRAY(frontend->const_declarations, counts->const_declarations,
             w_seed_frontend_const_declaration);
  HLO0_ARRAY(frontend->switch_arms, counts->switch_arms,
             w_seed_frontend_switch_arm);
  HLO0_ARRAY(frontend->enum_subset_members, counts->enum_subset_members,
             w_seed_frontend_enum_subset_member);
  HLO0_ARRAY(frontend->enum_membership_cases, counts->enum_membership_cases,
             w_seed_frontend_enum_membership_case);
  HLO0_ARRAY(frontend->generic_parameters, counts->generic_parameters,
             w_seed_frontend_generic_parameter);
  HLO0_ARRAY(frontend->generic_applications, counts->generic_applications,
             w_seed_frontend_generic_application);
  HLO0_ARRAY(frontend->generic_arguments, counts->generic_arguments,
             w_seed_frontend_generic_argument);
  HLO0_ARRAY(frontend->typed_const_expressions, counts->typed_const_expressions,
             w_seed_frontend_typed_const_expression);
  HLO0_ARRAY(frontend->const_values, counts->const_values,
             w_seed_frontend_const_value);
  HLO0_ARRAY(frontend->const_elements, counts->const_elements,
             w_seed_frontend_const_element);
#undef HLO0_ARRAY
  const w_seed_frontend_document *document = &frontend_input->documents[0];
  size_t document_node_bytes = 0u;
  size_t host_symbol_bytes = 0u;
  if (!array_bytes(document->node_count, sizeof(w_seed_cst_node),
                   &document_node_bytes) ||
      (input->host_scope != NULL &&
       !array_bytes(input->host_scope->symbol_count,
                    sizeof(w_seed_frontend_host_prelude_symbol),
                    &host_symbol_bytes)))
    return true;
  if (output_aliases_range(output, document->source->bytes.data,
                           document->source->bytes.length) ||
      output_aliases_range(output, document->nodes, document_node_bytes) ||
      output_aliases_range(output, input->profile_identity.data,
                           input->profile_identity.length))
    return true;
  for (size_t module_index = 0u;
       module_index < frontend_input->external_module_count; module_index += 1u) {
    const w_seed_frontend_external_module *module =
        &frontend_input->external_modules[module_index];
    size_t external_symbol_bytes = 0u;
    if (!array_bytes(module->symbol_count,
                     sizeof(w_seed_frontend_external_symbol),
                     &external_symbol_bytes) ||
        output_aliases_range(output, module->module_id.data,
                             module->module_id.length) ||
        output_aliases_range(output, module->symbols, external_symbol_bytes))
      return true;
    for (size_t symbol_index = 0u; symbol_index < module->symbol_count;
         symbol_index += 1u) {
      const w_seed_frontend_external_symbol *symbol =
          &module->symbols[symbol_index];
      size_t parameter_bytes = 0u;
      if (!array_bytes(symbol->parameter_count,
                       sizeof(w_seed_frontend_external_parameter),
                       &parameter_bytes) ||
          output_aliases_range(output, symbol->name.data, symbol->name.length) ||
          output_aliases_range(output, symbol->return_type.data,
                               symbol->return_type.length) ||
          output_aliases_range(output, symbol->parameters, parameter_bytes))
        return true;
      for (size_t parameter_index = 0u;
           parameter_index < symbol->parameter_count; parameter_index += 1u) {
        const w_seed_frontend_external_parameter *parameter =
            &symbol->parameters[parameter_index];
        if (output_aliases_range(output, parameter->name.data,
                                 parameter->name.length) ||
            output_aliases_range(output, parameter->type.data,
                                 parameter->type.length))
          return true;
      }
    }
  }
  if (input->host_scope != NULL &&
      (output_aliases_range(output, input->host_scope->profile.data,
                            input->host_scope->profile.length) ||
       output_aliases_range(output, input->host_scope->symbols,
                            host_symbol_bytes)))
    return true;
  if (input->host_scope == NULL) return false;
  for (size_t symbol_index = 0u;
       symbol_index < input->host_scope->symbol_count; symbol_index += 1u) {
    const w_seed_frontend_host_prelude_symbol *symbol =
        &input->host_scope->symbols[symbol_index];
    size_t parameter_bytes = 0u;
    size_t requirement_bytes = 0u;
    if (!array_bytes(symbol->parameter_count,
                     sizeof(w_seed_frontend_external_parameter),
                     &parameter_bytes) ||
        !array_bytes(symbol->requirement_count,
                     sizeof(w_seed_frontend_host_requirement),
                     &requirement_bytes))
      return true;
    if (output_aliases_range(output, symbol->name.data, symbol->name.length) ||
        output_aliases_range(output, symbol->return_type.data,
                             symbol->return_type.length) ||
        output_aliases_range(output, symbol->parameters, parameter_bytes) ||
        output_aliases_range(output, symbol->requirements, requirement_bytes))
      return true;
    for (size_t parameter_index = 0u;
         parameter_index < symbol->parameter_count; parameter_index += 1u) {
      const w_seed_frontend_external_parameter *parameter =
          &symbol->parameters[parameter_index];
      if (output_aliases_range(output, parameter->name.data,
                               parameter->name.length) ||
          output_aliases_range(output, parameter->type.data,
                               parameter->type.length))
        return true;
    }
    for (size_t requirement_index = 0u;
         requirement_index < symbol->requirement_count; requirement_index += 1u) {
      const w_seed_frontend_host_requirement *requirement =
          &symbol->requirements[requirement_index];
      if (output_aliases_range(output, requirement->name.data,
                               requirement->name.length))
        return true;
    }
  }
  return false;
}

static bool append_bytes(uint8_t *buffer, size_t capacity, size_t *offset,
                         const void *bytes, size_t length) {
  if (buffer == NULL || offset == NULL ||
      (length != 0u && bytes == NULL) || *offset > capacity ||
      length > capacity - *offset)
    return false;
  if (length != 0u) (void)memcpy(buffer + *offset, bytes, length);
  *offset += length;
  return true;
}

static bool append_literal(uint8_t *buffer, size_t capacity, size_t *offset,
                           const char *literal) {
  if (literal == NULL) return false;
  return append_bytes(buffer, capacity, offset, literal, strlen(literal));
}

static bool append_size(uint8_t *buffer, size_t capacity, size_t *offset,
                        size_t value) {
  char digits[3u * sizeof(size_t) + 1u];
  size_t length = 0u;
  do {
    digits[length] = (char)('0' + (value % 10u));
    value /= 10u;
    length += 1u;
  } while (value != 0u && length < sizeof(digits));
  if (value != 0u) return false;
  for (size_t index = 0u; index < length / 2u; index += 1u) {
    const char swap = digits[index];
    digits[index] = digits[length - index - 1u];
    digits[length - index - 1u] = swap;
  }
  return append_bytes(buffer, capacity, offset, digits, length);
}

static const char HEX[] = "0123456789abcdef";

static bool append_hex(uint8_t *buffer, size_t capacity, size_t *offset,
                       const uint8_t *bytes, size_t length) {
  if (bytes == NULL && length != 0u) return false;
  if (*offset > capacity || length > (capacity - (*offset)) / 2u)
    return false;
  for (size_t index = 0u; index < length; index += 1u) {
    buffer[*offset] = (uint8_t)HEX[bytes[index] >> 4u];
    *offset += 1u;
    buffer[*offset] = (uint8_t)HEX[bytes[index] & 0x0fu];
    *offset += 1u;
  }
  return true;
}

static bool write_receipt(const w_seed_hlo0_plan *plan, uint8_t *buffer,
                          size_t capacity, size_t *written) {
  if (plan == NULL || buffer == NULL || written == NULL) return false;
  size_t offset = 0u;
  if (!append_literal(buffer, capacity, &offset, "schema=") ||
      !append_bytes(buffer, capacity, &offset, plan->schema,
                    strlen(plan->schema)) ||
      !append_literal(buffer, capacity, &offset, "\nprofile=") ||
      !append_bytes(buffer, capacity, &offset, plan->profile,
                    strlen(plan->profile)) ||
      !append_literal(buffer, capacity, &offset, "\nslot=") ||
      !append_bytes(buffer, capacity, &offset, plan->slot, strlen(plan->slot)) ||
      !append_literal(buffer, capacity, &offset, "\nentry=") ||
      !append_bytes(buffer, capacity, &offset, plan->entry_target,
                    strlen(plan->entry_target)) ||
      !append_literal(buffer, capacity, &offset, "\nhandler=") ||
      !append_bytes(buffer, capacity, &offset, plan->handler,
                    strlen(plan->handler)) ||
      !append_literal(buffer, capacity, &offset, "\ncallee=") ||
      !append_bytes(buffer, capacity, &offset, plan->callee,
                    strlen(plan->callee)) ||
      !append_literal(buffer, capacity, &offset, "\nrequirement=") ||
      !append_bytes(buffer, capacity, &offset, plan->requirement,
                    strlen(plan->requirement)) ||
      !append_literal(buffer, capacity, &offset,
                      "\neffects=sync,no-throws,no-unsafe,no-borrows\n") ||
      !append_literal(buffer, capacity, &offset,
                      "signature=zero-params,unit\n") ||
      !append_literal(buffer, capacity, &offset, "payload=") ||
      !append_size(buffer, capacity, &offset, plan->payload_bytes) ||
      !append_literal(buffer, capacity, &offset, ":") ||
      !append_hex(buffer, capacity, &offset, plan->payload, plan->payload_bytes) ||
      !append_literal(buffer, capacity, &offset, "\nnewline=add-lf\nstdout=") ||
      !append_size(buffer, capacity, &offset, plan->stdout_bytes) ||
      !append_literal(buffer, capacity, &offset, "\nsha256=") ||
      !append_hex(buffer, capacity, &offset, plan->stdout_sha256,
                  HLO0_DIGEST_BYTES) ||
      !append_literal(buffer, capacity, &offset, "\nexit=success\n"))
    return false;
  *written = offset;
  return true;
}

static bool copy_literal(char *destination, size_t capacity,
                         const char *literal) {
  if (destination == NULL || literal == NULL) return false;
  const size_t length = strlen(literal);
  if (length >= capacity) return false;
  (void)memcpy(destination, literal, length + 1u);
  return true;
}

typedef enum {
  HLO0_PREPARE_READY = 0,
  HLO0_PREPARE_UNSUPPORTED,
  HLO0_PREPARE_INVALID,
} hlo0_prepare_status;

static hlo0_prepare_status prepare_candidate(const w_seed_hlo0_input *input,
                                              hlo0_candidate *candidate) {
  if (candidate == NULL || !validate_frontend_base(input)) {
    return HLO0_PREPARE_INVALID;
  }
  hlo0_graph graph;
  (void)memset(&graph, 0, sizeof(graph));
  if (!validate_frontend_coherence(input))
    return HLO0_PREPARE_INVALID;
  if (!validate_frontend_records(input, &graph))
    return HLO0_PREPARE_UNSUPPORTED;
  if (!validate_host_scope_subset(input, &graph))
    return HLO0_PREPARE_UNSUPPORTED;
  (void)memset(candidate, 0, sizeof(*candidate));
  w_seed_hlo0_plan *plan = &candidate->plan;
  if (!copy_literal(plan->schema, sizeof(plan->schema),
                    W_SEED_HLO0_SCHEMA_VERSION) ||
      !copy_literal(plan->profile, sizeof(plan->profile), HLO0_PROFILE) ||
      !copy_literal(plan->slot, sizeof(plan->slot), HLO0_SLOT) ||
      !copy_literal(plan->entry_target, sizeof(plan->entry_target), HLO0_ENTRY) ||
      !copy_literal(plan->handler, sizeof(plan->handler), HLO0_ENTRY) ||
      !copy_literal(plan->callee, sizeof(plan->callee), HLO0_CALLEE) ||
      !copy_literal(plan->requirement, sizeof(plan->requirement),
                    HLO0_REQUIREMENT))
    return HLO0_PREPARE_INVALID;
  plan->is_async = false;
  plan->is_throws = false;
  plan->is_unsafe = false;
  plan->has_borrow_clause = false;
  plan->zero_parameters = true;
  plan->unit_return = true;
  const w_seed_frontend_expression *literal =
      &input->frontend_output->expressions[graph.literal_index];
  plan->payload_bytes = literal->const_byte_count;
  if (plan->payload_bytes > W_SEED_HLO0_MAX_PAYLOAD ||
      literal->const_byte_offset == W_SEED_FRONTEND_NONE ||
      literal->const_byte_offset > input->frontend_result->written.const_bytes ||
      plan->payload_bytes >
          input->frontend_result->written.const_bytes -
              literal->const_byte_offset)
    return HLO0_PREPARE_INVALID;
  (void)memcpy(plan->payload,
               input->frontend_output->const_bytes +
                   literal->const_byte_offset,
               plan->payload_bytes);
  plan->newline_policy = W_SEED_HLO0_NEWLINE_ADD_LF;
  if (!add_size(plan->payload_bytes, HLO0_NEWLINE_BYTES, &plan->stdout_bytes))
    return HLO0_PREPARE_INVALID;
  w_seed_sha256_state sha;
  w_seed_sha256_init(&sha);
  w_seed_sha256_update(&sha, plan->payload, plan->payload_bytes);
  static const uint8_t line_feed = 0x0au;
  w_seed_sha256_update(&sha, &line_feed, HLO0_NEWLINE_BYTES);
  w_seed_sha256_final(&sha, plan->stdout_sha256);
  plan->exit_success = true;
  if (!write_receipt(plan, candidate->receipt, sizeof(candidate->receipt),
                     &candidate->receipt_bytes))
    return HLO0_PREPARE_INVALID;
  return HLO0_PREPARE_READY;
}

static void result_reset(w_seed_hlo0_result *result) {
  if (result != NULL) (void)memset(result, 0, sizeof(*result));
}

w_seed_hlo0_status w_seed_hlo0_measure(const w_seed_hlo0_input *input,
                                        w_seed_hlo0_counts *counts,
                                        w_seed_hlo0_result *result) {
  result_reset(result);
  if (counts != NULL) (void)memset(counts, 0, sizeof(*counts));
  if (counts == NULL || result == NULL) {
    if (result != NULL) result->status = W_SEED_HLO0_INVALID;
    return W_SEED_HLO0_INVALID;
  }
  if (input == NULL || input->frontend_result == NULL ||
      input->frontend_result->status != W_SEED_FRONTEND_OK) {
    result->status = W_SEED_HLO0_FRONTEND;
    return W_SEED_HLO0_FRONTEND;
  }
  hlo0_candidate candidate;
  const hlo0_prepare_status prepared = prepare_candidate(input, &candidate);
  if (prepared != HLO0_PREPARE_READY) {
    result->status = prepared == HLO0_PREPARE_UNSUPPORTED
                         ? W_SEED_HLO0_UNSUPPORTED
                         : W_SEED_HLO0_INVALID;
    return result->status;
  }
  counts->plans = 1u;
  counts->payload_bytes = candidate.plan.payload_bytes;
  counts->receipt_bytes = candidate.receipt_bytes;
  result->required = *counts;
  result->status = W_SEED_HLO0_OK;
  return W_SEED_HLO0_OK;
}

w_seed_hlo0_status w_seed_hlo0_run(const w_seed_hlo0_input *input,
                                   w_seed_hlo0_output *output,
                                   w_seed_hlo0_result *result) {
  result_reset(result);
  if (result == NULL) return W_SEED_HLO0_INVALID;
  if (input == NULL || input->frontend_result == NULL ||
      input->frontend_result->status != W_SEED_FRONTEND_OK) {
    result->status = W_SEED_HLO0_FRONTEND;
    return W_SEED_HLO0_FRONTEND;
  }
  hlo0_candidate candidate;
  const hlo0_prepare_status prepared = prepare_candidate(input, &candidate);
  if (prepared != HLO0_PREPARE_READY) {
    result->status = prepared == HLO0_PREPARE_UNSUPPORTED
                         ? W_SEED_HLO0_UNSUPPORTED
                         : W_SEED_HLO0_INVALID;
    return result->status;
  }
  result->required.plans = 1u;
  result->required.payload_bytes = candidate.plan.payload_bytes;
  result->required.receipt_bytes = candidate.receipt_bytes;
  const bool alias = output != NULL && output_aliases_input(input, output);
  const bool capacity =
      output == NULL || output->plan_capacity < 1u || output->plans == NULL ||
      output->receipt == NULL || output->receipt_capacity < candidate.receipt_bytes;
  if (alias || capacity) {
    result->status = alias ? W_SEED_HLO0_INVALID : W_SEED_HLO0_CAPACITY;
    return result->status;
  }
  (void)memcpy(output->plans, &candidate.plan, sizeof(candidate.plan));
  (void)memcpy(output->receipt, candidate.receipt, candidate.receipt_bytes);
  result->written = result->required;
  result->status = W_SEED_HLO0_OK;
  return W_SEED_HLO0_OK;
}
