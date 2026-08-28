#include "w_seed_frontend.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(size_t) * CHAR_BIT >= W_SEED_FRONTEND_TARGET_USIZE_BITS,
               "w-seed D1 requires a host that can represent target usize");

/* Internal scratch ceilings stay below the CST budget so dry/emit passes do
 * not create multi-megabyte stack frames. Crossing one records an explicit
 * unsupported fact instead of truncating semantic records. */
#define FRONTEND_MAX_PENDING_APPLICATIONS 4096u
#define FRONTEND_MAX_GENERIC_METADATA 4096u
#define FRONTEND_MAX_MEMBERSHIP_ITEMS 4096u
#define FRONTEND_MAX_IMPORTS 4096u
#define FRONTEND_MAX_DIAGNOSTIC_FACTS 8u
#define FRONTEND_MAX_DIAGNOSTIC_ITEMS 4096u
#define FRONTEND_MAX_DIAGNOSTIC_LABELS 8u
#define FRONTEND_DIAGNOSTIC_CATEGORY_TEXT_MAX 128u
#define FRONTEND_DIAGNOSTIC_CATEGORY_SLOTS \
  (W_SEED_FRONTEND_MAX_CST_NODES * 2u)

typedef struct {
  const w_seed_frontend_document *document;
  size_t index;
  w_seed_span bounds;
} frontend_token_cursor;

typedef struct {
  w_seed_cst_kind kind;
  w_seed_span span;
} frontend_token;

typedef struct {
  w_seed_frontend_type_kind kind;
  bool is_signed;
  uint16_t bit_width;
  w_seed_frontend_text spelling;
  uint32_t enum_index;
  w_seed_frontend_text enum_name;
  w_seed_frontend_text enum_alias_name;
  w_seed_span subset_span;
  /* Resolved element identity for StaticList domains. This is filled from
   * CST type children, not from a raw spelling slice. */
  w_seed_frontend_type_kind element_kind;
  bool element_is_signed;
  uint16_t element_bit_width;
  uint32_t element_enum_index;
  w_seed_frontend_text element_spelling;
  w_seed_frontend_text element_enum_name;
  /* Source provenance for diagnostics that point at a type annotation. */
  bool has_origin;
  size_t origin_document_index;
  w_seed_span origin_span;
} frontend_simple_type;

typedef struct {
  size_t index;
  size_t left;
  size_t right;
  w_seed_frontend_expr_kind kind;
  frontend_simple_type type;
  bool supported;
  bool is_integer_literal;
  bool has_name;
  bool is_enum_case;
  uint32_t enum_index;
  uint32_t enum_case_index;
  w_seed_frontend_text name;
  w_seed_frontend_text operator_text;
  w_seed_span span;
} frontend_expr_value;

typedef struct {
  size_t modules;
  size_t imports;
  size_t import_items;
  size_t structs;
  size_t fields;
  size_t type_declarations;
  size_t aliases;
  size_t types;
  size_t functions;
  size_t parameters;
  size_t entries;
  size_t statements;
  size_t expressions;
  size_t arguments;
  size_t symbols;
  size_t facts;
  size_t diagnostics;
  size_t diagnostic_facts;
  size_t diagnostic_items;
  size_t diagnostic_labels;
  size_t enums;
  size_t enum_cases;
  size_t enum_case_parameters;
  size_t switch_arms;
  size_t enum_subset_members;
  size_t enum_membership_cases;
  size_t generic_parameters;
  size_t generic_applications;
  size_t generic_arguments;
  size_t typed_const_expressions;
  size_t const_values;
  size_t const_elements;
  size_t const_bytes;
  size_t const_declarations;
} frontend_measure;

typedef struct {
  size_t module_index;
  uint32_t type_node;
  uint32_t owner_type;
} frontend_pending_application;

typedef struct {
  w_seed_frontend_text key;
  w_seed_frontend_diagnostic_fact_kind kind;
  w_seed_frontend_text text;
  int64_t integer_value;
  const w_seed_frontend_text *items;
  size_t item_count;
} frontend_diagnostic_fact_input;

typedef struct {
  w_seed_frontend_text role;
  w_seed_span span;
  size_t document_index;
} frontend_diagnostic_label_input;

typedef struct {
  bool has_contract;
  bool valid;
  bool full;
  bool duplicate;
  uint32_t enum_index;
  w_seed_frontend_text enum_name;
  w_seed_span list_span;
  size_t item_count;
} frontend_enum_subset_shape;

typedef struct {
  w_seed_frontend_text qualifier;
  w_seed_frontend_text name;
  w_seed_span span;
} frontend_enum_subset_item;

/* The D7 solver keeps only the compact scalar metadata that is needed while
 * the dry and emit passes normalize the same source.  Type indices are
 * process-local and are assigned after inference. */
typedef struct {
  w_seed_frontend_type_kind kind;
  bool is_signed;
  uint16_t bit_width;
  bool known;
} frontend_const_inferred_type;

enum {
  FRONTEND_CONST_INFERENCE_UNSEEN = 0u,
  FRONTEND_CONST_INFERENCE_ACTIVE = 1u,
  FRONTEND_CONST_INFERENCE_DONE = 2u,
};

typedef struct {
  w_seed_frontend_input input;
  w_seed_frontend_output *output;
  w_seed_frontend_result *result;
  bool emit;
  size_t module_index;
  uint32_t function_index;
  const w_seed_cst_node *function_node;
  frontend_measure count;
  size_t receipt_size;
  bool receipt_overflow;
  bool current_function_is_const;
  bool current_const_body_active;
  bool current_const_body_supported;
  bool current_const_root_emitted;
  uint32_t current_module_const;
  uint32_t builtin_usize_type_index;
  bool normalizing_generic_domain;
  frontend_pending_application
      pending_applications[FRONTEND_MAX_PENDING_APPLICATIONS];
  size_t pending_application_count;
  uint32_t generic_domain_type_indices[FRONTEND_MAX_GENERIC_METADATA];
  w_seed_frontend_generic_refinement_kind
      generic_refinement_kinds[FRONTEND_MAX_GENERIC_METADATA];
  frontend_const_inferred_type *const_inferred_types;
  uint32_t *const_declared_type_indices;
  uint32_t *const_inferred_type_indices;
  uint8_t *const_inference_states;
  size_t const_document_bases[W_SEED_FRONTEND_MAX_DOCUMENTS];
  size_t const_total_count;
  bool const_inference_complete;
} frontend_context;

/* These arrays are temporary per-thread inference workspace.  Each measure or
 * run resets them before use; they are not persistent compiler state and are
 * never part of the caller-owned frontend output. */
static _Thread_local frontend_const_inferred_type
    frontend_const_inferred_types_scratch
        [W_SEED_FRONTEND_MAX_CONST_DECLARATIONS];
static _Thread_local uint32_t frontend_const_declared_type_indices_scratch
    [W_SEED_FRONTEND_MAX_CONST_DECLARATIONS];
static _Thread_local uint32_t frontend_const_inferred_type_indices_scratch
    [W_SEED_FRONTEND_MAX_CONST_DECLARATIONS];
static _Thread_local uint8_t frontend_const_inference_states_scratch
    [W_SEED_FRONTEND_MAX_CONST_DECLARATIONS];
static _Thread_local w_seed_module_origin frontend_module_origins_scratch
    [FRONTEND_MAX_IMPORTS];
/* CONTRACT-0002 facts add a stable category prefix to a source spelling. The
 * public text view has one contiguous pointer, so retain these bounded
 * composed values in per-thread storage until the caller starts its next
 * frontend operation. A spelling that does not fit is rejected closed. */
static _Thread_local char frontend_diagnostic_category_scratch
    [FRONTEND_DIAGNOSTIC_CATEGORY_SLOTS]
    [FRONTEND_DIAGNOSTIC_CATEGORY_TEXT_MAX];
static _Thread_local size_t frontend_diagnostic_category_scratch_count;

static bool diagnostic_category_compose(w_seed_frontend_text prefix,
                                        w_seed_frontend_text suffix,
                                        w_seed_frontend_text *out) {
  if (out != NULL) *out = (w_seed_frontend_text){NULL, 0u};
  if (out == NULL || prefix.data == NULL || suffix.data == NULL ||
      prefix.length == 0u || suffix.length == 0u ||
      prefix.length > FRONTEND_DIAGNOSTIC_CATEGORY_TEXT_MAX ||
      suffix.length > FRONTEND_DIAGNOSTIC_CATEGORY_TEXT_MAX - prefix.length ||
      frontend_diagnostic_category_scratch_count >=
          FRONTEND_DIAGNOSTIC_CATEGORY_SLOTS) {
    return false;
  }
  char *destination =
      frontend_diagnostic_category_scratch
          [frontend_diagnostic_category_scratch_count];
  (void)memcpy(destination, prefix.data, prefix.length);
  (void)memcpy(destination + prefix.length, suffix.data, suffix.length);
  *out = (w_seed_frontend_text){
      destination, prefix.length + suffix.length};
  frontend_diagnostic_category_scratch_count += 1u;
  return true;
}

static bool diagnostic_category_arity(size_t arity,
                                      w_seed_frontend_text *out) {
  char decimal[3u * sizeof(size_t) + 1u];
  size_t length = 0u;
  do {
    decimal[length] = (char)('0' + (arity % 10u));
    length += 1u;
    arity /= 10u;
  } while (arity != 0u && length < sizeof(decimal));
  if (arity != 0u) return false;
  for (size_t left = 0u; left < length / 2u; left += 1u) {
    const size_t right = length - left - 1u;
    const char value = decimal[left];
    decimal[left] = decimal[right];
    decimal[right] = value;
  }
  return diagnostic_category_compose(
      (w_seed_frontend_text){"arity:", sizeof("arity:") - 1u},
      (w_seed_frontend_text){decimal, length}, out);
}

static bool normalize_document(frontend_context *context);
static bool normalize_module_const(frontend_context *context,
                                   uint32_t node_index,
                                   uint32_t const_index);
static bool infer_module_const_types(frontend_context *context);
static bool detect_duplicate_declarations(frontend_context *context);
static bool resolve_imports(frontend_context *context);
static bool resolve_frontend_links(frontend_context *context);
static bool resolve_pending_generic_applications(frontend_context *context);
static bool module_const_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    uint32_t *const_index, frontend_simple_type *type,
    const w_seed_frontend_document **owner_doc, uint32_t *owner_node);
static bool imported_target_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    w_seed_frontend_import_target_kind *target_kind, uint32_t *target_index,
    w_seed_frontend_text *target_name);
static bool unresolved_parenthesized_identifier(
    const frontend_context *context, w_seed_span span,
    w_seed_frontend_text *name_out);
static bool module_const_name_is_duplicate(
    const frontend_context *context, w_seed_frontend_text name);
static bool module_const_name_is_untyped(
    const frontend_context *context, w_seed_frontend_text name);
static const char *fact_name(w_seed_frontend_fact_kind kind);
static w_seed_frontend_text document_module_name(
    const w_seed_frontend_document *doc);
static w_seed_frontend_text document_local_module_name(
    const w_seed_frontend_document *doc);
static bool validate_import_resolution(const w_seed_frontend_input *input,
                                       size_t *bad_document,
                                       w_seed_span *bad_span);
static const w_seed_frontend_resolved_import *resolved_import_at(
    const frontend_context *context, size_t import_index);
static bool resolved_import_index_for(const frontend_context *context,
                                      size_t document_index,
                                      uint32_t direct_import_ordinal,
                                      size_t *import_index);
static bool exported_symbol_in_document(
    const w_seed_frontend_document *doc, w_seed_frontend_text name);
static bool exported_symbol_in_external(
    const w_seed_frontend_external_module *module,
    w_seed_frontend_text name);
static bool import_has_from(const w_seed_frontend_document *doc,
                            w_seed_span span);
static bool direct_import_ordinal_for(const w_seed_frontend_document *doc,
                                      uint32_t node_index,
                                      uint32_t *ordinal);
static const char *declaration_keyword(w_seed_cst_kind kind);
static w_seed_frontend_text binding_name_after_keyword(
    const w_seed_frontend_document *doc, w_seed_span span, const char *keyword);
static w_seed_frontend_text import_item_local_name(
    const w_seed_frontend_document *doc, w_seed_span span,
    w_seed_frontend_text *imported_name);
static w_seed_frontend_label_kind parameter_label_kind(
    const w_seed_frontend_document *doc, w_seed_span span);
static w_seed_frontend_text parameter_name_from_span(
    const w_seed_frontend_document *doc, w_seed_span span);
static w_seed_frontend_text parameter_external_label_from_span(
    const w_seed_frontend_document *doc, w_seed_span span);
static w_seed_frontend_text enum_case_parameter_label(
    const w_seed_frontend_document *doc, uint32_t parameter_node);
static const w_seed_frontend_document *context_document(
    const frontend_context *context);
static size_t context_document_index_for(
    const frontend_context *context,
    const w_seed_frontend_document *document);
static w_seed_frontend_text first_word_in_span(
    const w_seed_frontend_document *doc, w_seed_span span);
static w_seed_frontend_text name_after_keyword(
    const w_seed_frontend_document *doc, w_seed_span span,
    const char *keyword);
static bool enum_declaration_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    uint32_t *enum_index, uint32_t *type_index,
    const w_seed_frontend_document **owner_doc, uint32_t *enum_node);
static bool enum_case_for_name(
    const frontend_context *context, uint32_t expected_enum,
    w_seed_frontend_text name, uint32_t *case_index,
    const w_seed_frontend_document **owner_doc, uint32_t *case_node);
static size_t enum_case_parameter_count(const frontend_context *context,
                                        uint32_t case_index);
static bool frontend_type_equal(const frontend_context *context,
                                frontend_simple_type left,
                                frontend_simple_type right);
static bool frontend_widening_allowed(const frontend_context *context,
                                      frontend_simple_type actual,
                                      frontend_simple_type expected);
static bool enum_subset_shape_for_type(
    const frontend_context *context, const w_seed_frontend_document *doc,
    uint32_t type_node, frontend_enum_subset_shape *shape);
static bool enum_subset_shape_for_alias(
    const frontend_context *context, w_seed_frontend_text alias_name,
    frontend_enum_subset_shape *shape);
static bool enum_subset_contains_case(
    const frontend_context *context, frontend_simple_type type,
    uint32_t enum_case_index);
static frontend_simple_type contextual_type_from_span(
    const frontend_context *context, const w_seed_frontend_document *doc,
    w_seed_span span);
static bool normalize_expression_node(frontend_context *context,
                                      uint32_t expression_node,
                                      uint32_t *expression_index,
                                      frontend_simple_type expected,
                                      frontend_simple_type *actual_out,
                                      frontend_expr_value *root_out);
static bool normalize_expression_span(frontend_context *context,
                                      w_seed_span span,
                                      frontend_simple_type expected,
                                      uint32_t *expression_index,
                                      frontend_simple_type *actual_out,
                                      frontend_expr_value *root_out);
static bool normalize_switch_expression(frontend_context *context,
                                         uint32_t switch_node,
                                         uint32_t *expression_index,
                                         frontend_simple_type expected,
                                         frontend_simple_type *actual_out,
                                         frontend_expr_value *root_out);
static frontend_simple_type simple_type_from_view(w_seed_frontend_text spelling);
static frontend_simple_type literal_simple_type(
    const w_seed_frontend_document *doc, w_seed_span span,
    w_seed_cst_kind token_kind);
static bool diagnostic_value_kind_from_simple(
    frontend_simple_type type, bool use_spelling, w_seed_frontend_text *out);
static bool context_append_diagnostic_raw(
    frontend_context *context, const char *code, w_seed_span primary,
    const frontend_diagnostic_fact_input *facts, size_t fact_count,
    const frontend_diagnostic_label_input *labels, size_t label_count);
static bool context_append_diagnostic_raw_at(
    frontend_context *context, const char *code, size_t primary_document_index,
    w_seed_span primary, const frontend_diagnostic_fact_input *facts,
    size_t fact_count, const frontend_diagnostic_label_input *labels,
    size_t label_count);
static bool append_type0121_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text actual_case, frontend_simple_type expected);
static bool append_type0122_diagnostic(
    frontend_context *context, w_seed_span primary,
    frontend_simple_type actual, frontend_simple_type expected,
    w_seed_frontend_text reason);
static bool append_type0120_diagnostic(
    frontend_context *context, w_seed_span primary,
    frontend_simple_type left, w_seed_span left_span, size_t left_document,
    frontend_simple_type right, w_seed_span right_span,
    size_t right_document);
static bool append_match0001_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text subject_type, w_seed_span subject_span,
    const w_seed_frontend_text *missing, size_t missing_count);
static bool append_match0002_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text covered_by, w_seed_frontend_text pattern,
    w_seed_frontend_text subject_type, w_seed_span covered_span,
    size_t covered_document, w_seed_span subject_span);
static bool append_sem0001_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text actual, w_seed_frontend_text expected);
static bool append_label0005_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text declaration, w_seed_frontend_text label,
    const w_seed_frontend_text *accepted_forms, size_t accepted_count);
static bool append_label0006_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text declaration, w_seed_frontend_text label,
    w_seed_frontend_text slot);
static bool append_match0003_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text context_name, w_seed_frontend_text expected_type,
    w_seed_frontend_text member);
static size_t diagnostic_call_accepted_forms(
    const frontend_context *context, bool enum_case_constructor,
    uint32_t enum_case_index, const w_seed_frontend_document *signature_doc,
    uint32_t signature_node,
    const w_seed_frontend_external_symbol *external_signature,
    const w_seed_frontend_host_prelude_symbol *host_signature, size_t ordinal,
    w_seed_frontend_text *forms, size_t form_capacity);
static bool append_const0001_diagnostic(
    frontend_context *context, w_seed_span primary,
    const w_seed_frontend_text *call_chain, size_t call_chain_count,
    w_seed_frontend_text operation, w_seed_frontend_text reason,
    w_seed_frontend_text symbol, size_t owner_document, w_seed_span owner_span);
static bool append_contract0002_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text actual_kind, w_seed_frontend_text expected_kind,
    w_seed_frontend_text head, size_t head_document, w_seed_span head_span,
    w_seed_frontend_text slot, size_t slot_document, w_seed_span slot_span);
static bool append_contract0003_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text expected_type, w_seed_frontend_text head,
    size_t head_document, w_seed_span head_span,
    w_seed_frontend_text predicate_type, size_t slot_document,
    w_seed_span slot_span);
static bool append_contract0004_diagnostic(
    frontend_context *context, w_seed_span primary, w_seed_frontend_text head,
    size_t head_document, w_seed_span head_span, w_seed_frontend_text slot,
    size_t slot_document, w_seed_span slot_span,
    const w_seed_frontend_text *slot_order, size_t slot_order_count,
    w_seed_frontend_text violation);
static bool append_generic0001_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text domain, w_seed_frontend_text parameter,
    w_seed_frontend_text resolution_reason, size_t parameter_document,
    w_seed_span parameter_span);
static bool append_contract0001_diagnostic(
    frontend_context *context, w_seed_span primary,
    const w_seed_frontend_text *available_slots, size_t available_count,
    w_seed_frontend_text head, size_t head_document, w_seed_span head_span,
    w_seed_frontend_text slot);
static bool append_generic0002_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text parameter, size_t parameter_document,
    w_seed_span parameter_span, size_t call_document, w_seed_span call_span);
static bool append_generic0003_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text external_label, w_seed_frontend_text kind,
    w_seed_frontend_text parameter, int64_t position,
    w_seed_frontend_text reason, size_t label_document, w_seed_span label_span);
static bool diagnostic_name_span_after_keyword(
    const w_seed_frontend_document *doc, w_seed_span span,
    const char *keyword, w_seed_frontend_text *name, w_seed_span *name_span);
static size_t diagnostic_generic_slot_order(
    const w_seed_frontend_document *doc, uint32_t generic_node,
    w_seed_frontend_text *slots, size_t slot_capacity);
static bool context_append_fact(frontend_context *context,
                                w_seed_frontend_fact_kind kind,
                                w_seed_span span,
                                w_seed_frontend_text detail);
static bool const_record_failure(frontend_context *context, w_seed_span span,
                                 w_seed_frontend_text detail);
static bool context_append_record(frontend_context *context, size_t ordinal,
                                   const void *value, size_t value_size,
                                   void *array, size_t capacity,
                                   uint32_t *index);
static bool context_append_type(frontend_context *context,
                                 w_seed_frontend_type value,
                                 uint32_t *index);
static bool next_child(const w_seed_frontend_document *doc, uint32_t *cursor,
                       uint32_t *child);
static bool span_has_keyword(const w_seed_frontend_document *doc,
                             w_seed_span span, const char *keyword);
static bool module_id_equal(w_seed_frontend_text left,
                            w_seed_frontend_text right);
static bool normalize_struct_generic_parameters(
    frontend_context *context, uint32_t struct_node, uint32_t struct_index,
    uint32_t first_generic_parameter, uint32_t *generic_parameter_count);
static bool struct_declaration_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    uint32_t *struct_index, const w_seed_frontend_document **owner_doc,
    uint32_t *struct_node);
static bool function_signature_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    const w_seed_frontend_document **owner_doc, uint32_t *function_node);
static bool external_symbol_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    const w_seed_frontend_external_symbol **symbol);
static frontend_simple_type function_return_type(
    const frontend_context *context, const w_seed_frontend_document *doc,
    uint32_t function_node);
static w_seed_span trim_span(const w_seed_frontend_document *doc,
                             w_seed_span span);
static frontend_token_cursor token_cursor_for(
    const w_seed_frontend_document *doc, w_seed_span span);
static bool cursor_take(frontend_token_cursor *cursor, frontend_token *token);
static bool cursor_peek(const frontend_token_cursor *cursor,
                        frontend_token *token);
static bool cursor_take_text(frontend_token_cursor *cursor, const char *text,
                             frontend_token *taken);
static bool cursor_peek_text(const frontend_token_cursor *cursor,
                             const char *text);
static bool token_text(const w_seed_frontend_document *doc,
                       const frontend_token *token, const char *text);
static bool text_equal(w_seed_frontend_text text, const char *literal);
static bool text_equal_text(w_seed_frontend_text left,
                            w_seed_frontend_text right);
static w_seed_frontend_text text_from_span(
    const w_seed_frontend_document *doc, w_seed_span span);
static bool next_child(const w_seed_frontend_document *doc, uint32_t *child,
                       uint32_t *index);
static size_t count_direct_kind(const w_seed_frontend_document *doc,
                                uint32_t parent, w_seed_cst_kind kind);
static uint32_t first_direct_kind(const w_seed_frontend_document *doc,
                                  uint32_t parent, w_seed_cst_kind kind);
static bool generic_parameter_label_omitted(
    const w_seed_frontend_document *doc, uint32_t parameter_node);
static w_seed_frontend_text generic_parameter_word_at(
    const w_seed_frontend_document *doc, uint32_t parameter_node,
    size_t ordinal);
static w_seed_frontend_text generic_parameter_name(
    const w_seed_frontend_document *doc, uint32_t parameter_node);
static uint32_t generic_parameter_type_node(
    const w_seed_frontend_document *doc, uint32_t parameter_node);
static w_seed_frontend_text generic_parameter_external_label(
    const w_seed_frontend_document *doc, uint32_t parameter_node);
static uint32_t generic_refinement_envelope(
    const w_seed_frontend_document *doc, uint32_t type_node);
static frontend_simple_type simple_type_unknown(void);
static bool generic_value_domain_supported(
    const frontend_context *context, const w_seed_frontend_document *doc,
    uint32_t type_node, frontend_simple_type *simple_out);
static bool contract_envelope_has_type(const w_seed_frontend_document *doc,
                                       uint32_t envelope_node);
static bool generic_domain_refers_to_previous_type(
    const w_seed_frontend_document *doc, uint32_t type_node,
    w_seed_frontend_text *name_out, uint32_t *ordinal_out,
    const w_seed_frontend_text *prior_names, const bool *prior_is_type,
    size_t prior_count);
static frontend_simple_type static_list_element_type(
    const frontend_context *context, frontend_simple_type list_type);
static bool static_list_element_from_span(
    const frontend_context *context, const w_seed_frontend_document *doc,
    w_seed_span span, frontend_simple_type *list_type);
static w_seed_span generic_base_type_span(
    const w_seed_frontend_document *doc, uint32_t type_node);
static w_seed_span generic_head_type_span(
    const w_seed_frontend_document *doc, uint32_t type_node);
static bool local_generic_head_for_type(
    const frontend_context *context, const w_seed_frontend_document *doc,
    uint32_t type_node, uint32_t *struct_index, uint32_t *struct_node,
    w_seed_frontend_text *head_name, w_seed_span *base_span);
static bool register_pending_generic_application(frontend_context *context,
                                                 uint32_t type_node,
                                                 uint32_t owner_type);
static bool normalize_type_tree(frontend_context *context, uint32_t type_node,
                               uint32_t *type_index);
static bool context_append_generic_application(
    frontend_context *context, w_seed_frontend_generic_application value,
    uint32_t *index);
static bool context_append_generic_argument(
    frontend_context *context, w_seed_frontend_generic_argument value,
    uint32_t *index);
static bool context_append_typed_const_expression(
    frontend_context *context, w_seed_frontend_typed_const_expression value,
    uint32_t *index);
static bool context_append_const_value(frontend_context *context,
                                       w_seed_frontend_const_value value,
                                       uint32_t *index);
static bool context_append_const_element(frontend_context *context,
                                         w_seed_frontend_const_element value,
                                         uint32_t *index);
static bool context_append_const_bytes(frontend_context *context,
                                       const uint8_t *bytes, size_t count,
                                       uint32_t *first_byte);
static bool integer_literal_parts(w_seed_frontend_text text,
                                  size_t *digits_start, bool *is_signed,
                                  bool *negative, uint16_t *bit_width);
static bool integer_literal_value(w_seed_frontend_text text,
                                   size_t digits_start, uint64_t *value);
static bool unsuffixed_integer_fits(w_seed_frontend_text spelling,
                                    frontend_simple_type expected);
static bool is_binary_operator(w_seed_frontend_text text);
static int operator_precedence(w_seed_frontend_text text);
static bool module_const_index_for_node(const frontend_context *context,
                                        size_t document_index,
                                        uint32_t node_index, uint32_t *index);
static bool initialize_const_document_bases(frontend_context *context);
static uint32_t direct_type_index(const w_seed_frontend_document *doc,
                                  uint32_t node_index);
static uint32_t first_direct_expression(const w_seed_frontend_document *doc,
                                         uint32_t node_index);

static w_seed_span empty_span(size_t offset) {
  const w_seed_span span = {offset, offset};
  return span;
}

static bool add_size(size_t left, size_t right, size_t *result) {
  if (right > SIZE_MAX - left) return false;
  *result = left + right;
  return true;
}

typedef struct {
  frontend_simple_type type;
  w_seed_frontend_text spelling;
  bool valid;
  bool unsupported;
  bool unsuffixed_integer;
} frontend_const_infer_value;

typedef struct {
  frontend_context *context;
  const w_seed_frontend_document *document;
  frontend_token_cursor cursor;
  frontend_simple_type expected_type;
  bool has_expected_type;
  size_t depth;
} frontend_const_type_parser;

static frontend_simple_type const_default_integer_type(void) {
  frontend_simple_type type = simple_type_unknown();
  type.kind = W_SEED_FRONTEND_TYPE_INTEGER;
  type.is_signed = true;
  type.bit_width = 64u;
  type.spelling = (w_seed_frontend_text){"i64", 3};
  return type;
}

static w_seed_frontend_text const_integer_type_spelling(bool is_signed,
                                                        uint16_t bit_width) {
  if (is_signed) {
    switch (bit_width) {
      case 8u:
        return (w_seed_frontend_text){"i8", 2};
      case 16u:
        return (w_seed_frontend_text){"i16", 3};
      case 32u:
        return (w_seed_frontend_text){"i32", 3};
      case 64u:
        return (w_seed_frontend_text){"i64", 3};
      default:
        break;
    }
  } else {
    switch (bit_width) {
      case 8u:
        return (w_seed_frontend_text){"u8", 2};
      case 16u:
        return (w_seed_frontend_text){"u16", 3};
      case 32u:
        return (w_seed_frontend_text){"u32", 3};
      case 64u:
        return (w_seed_frontend_text){"u64", 3};
      default:
        break;
    }
  }
  return (w_seed_frontend_text){NULL, 0};
}

static frontend_simple_type const_inferred_simple_type(
    const frontend_const_inferred_type *inferred) {
  frontend_simple_type type = simple_type_unknown();
  if (inferred == NULL || !inferred->known) return type;
  type.kind = inferred->kind;
  type.is_signed = inferred->is_signed;
  type.bit_width = inferred->bit_width;
  type.spelling = inferred->kind == W_SEED_FRONTEND_TYPE_BOOL
                      ? (w_seed_frontend_text){"Bool", 4}
                      : const_integer_type_spelling(inferred->is_signed,
                                                     inferred->bit_width);
  return type;
}

static bool const_inferred_type_is_scalar(frontend_simple_type type) {
  return type.kind == W_SEED_FRONTEND_TYPE_BOOL ||
         (type.kind == W_SEED_FRONTEND_TYPE_INTEGER && type.bit_width != 0u);
}

static frontend_const_infer_value const_infer_value_invalid(bool unsupported) {
  frontend_const_infer_value value;
  (void)memset(&value, 0, sizeof(value));
  value.type = simple_type_unknown();
  value.valid = false;
  value.unsupported = unsupported;
  return value;
}

static frontend_const_infer_value const_infer_value_type(
    frontend_simple_type type) {
  frontend_const_infer_value value;
  (void)memset(&value, 0, sizeof(value));
  value.type = type;
  value.spelling = type.spelling;
  value.valid = type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN;
  value.unsuffixed_integer = type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                             type.bit_width == 0u;
  return value;
}

static bool module_const_node_for_index(const frontend_context *context,
                                        uint32_t const_index,
                                        uint32_t *node_index) {
  if (node_index != NULL) *node_index = W_SEED_CST_NONE;
  if (context == NULL || context->module_index >= context->input.document_count)
    return false;
  const w_seed_frontend_document *doc =
      &context->input.documents[context->module_index];
  const size_t base = context->const_document_bases[context->module_index];
  if ((size_t)const_index < base ||
      (size_t)const_index >= context->const_total_count)
    return false;
  const size_t wanted_ordinal = (size_t)const_index - base;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t ordinal = 0u;
  size_t guard = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_CONST_DECLARATION) {
      if (ordinal == wanted_ordinal) {
        if (node_index != NULL) *node_index = child;
        return true;
      }
      ordinal += 1u;
    }
    guard += 1u;
  }
  return false;
}

static bool const_declaration_explicit_type(
    const frontend_context *context, uint32_t const_index,
    frontend_simple_type *type, bool *explicit_type) {
  if (type != NULL) *type = simple_type_unknown();
  if (explicit_type != NULL) *explicit_type = false;
  uint32_t node_index = W_SEED_CST_NONE;
  if (!module_const_node_for_index(context, const_index, &node_index)) return false;
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return false;
  const uint32_t type_node = direct_type_index(doc, node_index);
  const bool present = type_node != W_SEED_CST_NONE;
  if (explicit_type != NULL) *explicit_type = present;
  if (present && type != NULL)
    *type = contextual_type_from_span(context, doc, doc->nodes[type_node].raw_span);
  return true;
}

static bool const_infer_declaration(frontend_context *context,
                                    uint32_t const_index,
                                    frontend_simple_type *type);

static frontend_const_infer_value const_infer_bp(
    frontend_const_type_parser *parser, int minimum_precedence);

static frontend_const_infer_value const_infer_primary(
    frontend_const_type_parser *parser) {
  if (parser == NULL || parser->document == NULL)
    return const_infer_value_invalid(true);
  frontend_token token;
  if (!cursor_take(&parser->cursor, &token))
    return const_infer_value_invalid(true);
  const w_seed_frontend_text spelling = text_from_span(parser->document,
                                                       token.span);
  if (token.kind == W_SEED_CST_WORD) {
    if (text_equal(spelling, "true") || text_equal(spelling, "false"))
      return const_infer_value_type(
          simple_type_from_view((w_seed_frontend_text){"Bool", 4}));
    uint32_t const_index = W_SEED_FRONTEND_NONE;
    frontend_simple_type target_type = simple_type_unknown();
    if (!module_const_for_name(parser->context, spelling, &const_index,
                               &target_type, NULL, NULL))
      return const_infer_value_invalid(true);
    if (const_index == W_SEED_FRONTEND_NONE ||
        !const_infer_declaration(parser->context, const_index, &target_type))
      return const_infer_value_invalid(false);
    frontend_const_infer_value value = const_infer_value_type(target_type);
    /* An active unanchored cycle has a provisional unknown type.  Keep the
     * relation valid so its enclosing arithmetic/comparison can materialize
     * the deterministic i64 default after the graph closes. */
    if (target_type.kind == W_SEED_FRONTEND_TYPE_UNKNOWN) value.valid = true;
    return value;
  }
  if (token.kind == W_SEED_CST_NUMBER ||
      token.kind == W_SEED_CST_LITERAL_EVENT) {
    frontend_simple_type type = literal_simple_type(parser->document, token.span,
                                                    token.kind);
    frontend_const_infer_value value = const_infer_value_type(type);
    value.spelling = spelling;
    if (type.kind == W_SEED_FRONTEND_TYPE_INTEGER && type.bit_width == 0u &&
        parser->has_expected_type &&
        parser->expected_type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
        unsuffixed_integer_fits(spelling, parser->expected_type)) {
      value.type = parser->expected_type;
      value.valid = true;
      value.unsuffixed_integer = false;
    }
    if (type.kind == W_SEED_FRONTEND_TYPE_STRING ||
        type.kind == W_SEED_FRONTEND_TYPE_BYTES ||
        type.kind == W_SEED_FRONTEND_TYPE_FLOAT)
      value.unsupported = true;
    return value;
  }
  if (token_text(parser->document, &token, "(")) {
    frontend_const_infer_value nested;
    if (!cursor_peek_text(&parser->cursor, ")")) {
      nested = const_infer_bp(parser, 0);
    } else {
      nested = const_infer_value_invalid(true);
    }
    if (!cursor_take_text(&parser->cursor, ")", NULL))
      return const_infer_value_invalid(true);
    return nested;
  }
  return const_infer_value_invalid(true);
}

static frontend_const_infer_value const_infer_prefix(
    frontend_const_type_parser *parser) {
  if (parser == NULL || parser->document == NULL)
    return const_infer_value_invalid(true);
  frontend_token token;
  if (cursor_peek(&parser->cursor, &token) &&
      (token_text(parser->document, &token, "!") ||
       token_text(parser->document, &token, "-"))) {
    (void)cursor_take(&parser->cursor, &token);
    frontend_const_infer_value nested = const_infer_prefix(parser);
    if (!nested.valid) return nested;
    const bool logical = token_text(parser->document, &token, "!");
    if (logical) {
      if (nested.type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
          nested.type.kind != W_SEED_FRONTEND_TYPE_BOOL)
        return const_infer_value_invalid(false);
      return const_infer_value_type(
          simple_type_from_view((w_seed_frontend_text){"Bool", 4}));
    }
    if (nested.type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        nested.type.kind != W_SEED_FRONTEND_TYPE_INTEGER)
      return const_infer_value_invalid(false);
    if (nested.type.kind == W_SEED_FRONTEND_TYPE_UNKNOWN ||
        nested.type.bit_width == 0u)
      nested.type = const_default_integer_type();
    nested.unsuffixed_integer = false;
    nested.spelling = nested.type.spelling;
    return nested;
  }
  return const_infer_primary(parser);
}

static bool const_infer_integer_join(frontend_context *context,
                                     frontend_const_infer_value *left,
                                     frontend_const_infer_value *right,
                                     frontend_simple_type *joined) {
  if (left == NULL || right == NULL || joined == NULL || !left->valid ||
      !right->valid)
    return false;
  frontend_simple_type left_type = left->type;
  frontend_simple_type right_type = right->type;
  if (left_type.kind == W_SEED_FRONTEND_TYPE_UNKNOWN)
    left_type = const_default_integer_type();
  if (right_type.kind == W_SEED_FRONTEND_TYPE_UNKNOWN)
    right_type = const_default_integer_type();
  if (left_type.kind != W_SEED_FRONTEND_TYPE_INTEGER ||
      right_type.kind != W_SEED_FRONTEND_TYPE_INTEGER)
    return false;
  if (left->unsuffixed_integer && right_type.bit_width != 0u &&
      unsuffixed_integer_fits(left->spelling, right_type)) {
    left->type = left_type = right_type;
    left->unsuffixed_integer = false;
  } else if (right->unsuffixed_integer && left_type.bit_width != 0u &&
             unsuffixed_integer_fits(right->spelling, left_type)) {
    right->type = right_type = left_type;
    right->unsuffixed_integer = false;
  }
  if (frontend_type_equal(context, left_type, right_type)) {
    *joined = left_type;
    return true;
  }
  if (frontend_widening_allowed(context, left_type, right_type)) {
    *joined = right_type;
    return true;
  }
  if (frontend_widening_allowed(context, right_type, left_type)) {
    *joined = left_type;
    return true;
  }
  return false;
}

static frontend_const_infer_value const_infer_bp(
    frontend_const_type_parser *parser, int minimum_precedence) {
  if (parser == NULL || parser->depth >= W_SEED_FRONTEND_MAX_NESTING)
    return const_infer_value_invalid(true);
  parser->depth += 1u;
  frontend_const_infer_value value = const_infer_prefix(parser);
  parser->depth -= 1u;
  if (!value.valid) return value;
  while (true) {
    frontend_token operator_token;
    if (!cursor_peek(&parser->cursor, &operator_token)) break;
    const w_seed_frontend_text operator_text =
        text_from_span(parser->document, operator_token.span);
    const int precedence = operator_precedence(operator_text);
    if (precedence < minimum_precedence || !is_binary_operator(operator_text))
      break;
    (void)cursor_take(&parser->cursor, &operator_token);
    frontend_const_infer_value right =
        const_infer_bp(parser, precedence + 1);
    if (!right.valid) return right;
    const bool logical = text_equal(operator_text, "&&") ||
                         text_equal(operator_text, "||");
    const bool comparison = text_equal(operator_text, "==") ||
                            text_equal(operator_text, "!=") ||
                            text_equal(operator_text, "<") ||
                            text_equal(operator_text, "<=") ||
                            text_equal(operator_text, ">") ||
                            text_equal(operator_text, ">=");
    const bool arithmetic = text_equal(operator_text, "+") ||
                            text_equal(operator_text, "-") ||
                            text_equal(operator_text, "*") ||
                            text_equal(operator_text, "/") ||
                            text_equal(operator_text, "%");
    frontend_simple_type result_type = simple_type_unknown();
    if (logical) {
      if ((value.type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
           value.type.kind != W_SEED_FRONTEND_TYPE_BOOL) ||
          (right.type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
           right.type.kind != W_SEED_FRONTEND_TYPE_BOOL))
        return const_infer_value_invalid(false);
      result_type = simple_type_from_view((w_seed_frontend_text){"Bool", 4});
    } else if (comparison) {
      const bool value_bool_or_unknown =
          value.type.kind == W_SEED_FRONTEND_TYPE_BOOL ||
          value.type.kind == W_SEED_FRONTEND_TYPE_UNKNOWN;
      const bool right_bool_or_unknown =
          right.type.kind == W_SEED_FRONTEND_TYPE_BOOL ||
          right.type.kind == W_SEED_FRONTEND_TYPE_UNKNOWN;
      const bool value_integer_or_unknown =
          value.type.kind == W_SEED_FRONTEND_TYPE_INTEGER ||
          value.type.kind == W_SEED_FRONTEND_TYPE_UNKNOWN;
      const bool right_integer_or_unknown =
          right.type.kind == W_SEED_FRONTEND_TYPE_INTEGER ||
          right.type.kind == W_SEED_FRONTEND_TYPE_UNKNOWN;
      if (value_bool_or_unknown && right_bool_or_unknown) {
        /* An active declaration is an unresolved constraint, not an integer
         * default.  A Bool neighbor must therefore anchor the comparison
         * before the cycle is lowered. */
        result_type = simple_type_from_view((w_seed_frontend_text){"Bool", 4});
      } else if (value_integer_or_unknown && right_integer_or_unknown) {
        frontend_simple_type joined = simple_type_unknown();
        if (!const_infer_integer_join(parser->context, &value, &right,
                                      &joined))
          return const_infer_value_invalid(false);
        result_type = simple_type_from_view((w_seed_frontend_text){"Bool", 4});
      } else {
        return const_infer_value_invalid(false);
      }
    } else if (arithmetic) {
      if (!const_infer_integer_join(parser->context, &value, &right,
                                    &result_type))
        return const_infer_value_invalid(false);
    } else {
      return const_infer_value_invalid(true);
    }
    value = const_infer_value_type(result_type);
  }
  return value;
}

static bool const_infer_declaration(frontend_context *context,
                                    uint32_t const_index,
                                    frontend_simple_type *type) {
  if (type != NULL) *type = simple_type_unknown();
  if (context == NULL ||
      const_index >= W_SEED_FRONTEND_MAX_CONST_DECLARATIONS)
    return false;
  frontend_simple_type explicit_simple = simple_type_unknown();
  bool explicit_type = false;
  if (!const_declaration_explicit_type(context, const_index, &explicit_simple,
                                       &explicit_type))
    return false;
  if (context->const_inference_states[const_index] ==
      FRONTEND_CONST_INFERENCE_DONE) {
    if (context->const_inferred_types[const_index].known)
      explicit_simple = const_inferred_simple_type(
          &context->const_inferred_types[const_index]);
    if (type != NULL) *type = explicit_type && !context->const_inferred_types[const_index].known
                                  ? explicit_simple
                                  : (context->const_inferred_types[const_index].known
                                         ? explicit_simple
                                         : simple_type_unknown());
    return true;
  }
  if (context->const_inference_states[const_index] ==
      FRONTEND_CONST_INFERENCE_ACTIVE) {
    if (type != NULL)
      *type = const_inferred_type_is_scalar(explicit_simple)
                  ? explicit_simple
                  : simple_type_unknown();
    return true;
  }
  context->const_inference_states[const_index] =
      FRONTEND_CONST_INFERENCE_ACTIVE;
  uint32_t node_index = W_SEED_CST_NONE;
  const w_seed_frontend_document *doc = context_document(context);
  if (!module_const_node_for_index(context, const_index, &node_index) ||
      doc == NULL) {
    context->const_inference_states[const_index] =
        FRONTEND_CONST_INFERENCE_DONE;
    return false;
  }
  const uint32_t expression_node = first_direct_expression(doc, node_index);
  frontend_simple_type inferred = simple_type_unknown();
  bool valid = false;
  if (expression_node != W_SEED_CST_NONE) {
    frontend_const_type_parser parser;
    (void)memset(&parser, 0, sizeof(parser));
    parser.context = context;
    parser.document = doc;
    parser.cursor = token_cursor_for(doc, doc->nodes[expression_node].raw_span);
    parser.expected_type = explicit_type ? explicit_simple : simple_type_unknown();
    parser.has_expected_type = explicit_type;
    frontend_const_infer_value value = const_infer_bp(&parser, 0);
    frontend_token trailing;
    valid = value.valid && !value.unsupported &&
            !cursor_peek(&parser.cursor, &trailing);
    inferred = value.type;
    if (valid && inferred.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
        inferred.bit_width == 0u)
      inferred = const_default_integer_type();
    /* A closed identifier component with no explicit or suffix anchor is an
     * integer component with no remaining context.  Materialize the D4
     * default here so a later ConstIR pass can report a reachable cycle with
     * its causal precedence instead of turning the relation into an
     * unresolved frontend symbol. */
    if (valid && inferred.kind == W_SEED_FRONTEND_TYPE_UNKNOWN)
      inferred = const_default_integer_type();
  }
  if (explicit_type) inferred = explicit_simple;
  frontend_const_inferred_type compact;
  (void)memset(&compact, 0, sizeof(compact));
  compact.kind = inferred.kind;
  compact.is_signed = inferred.is_signed;
  compact.bit_width = inferred.bit_width;
  compact.known = valid && const_inferred_type_is_scalar(inferred);
  if (explicit_type && const_inferred_type_is_scalar(explicit_simple))
    compact.known = true;
  context->const_inferred_types[const_index] = compact;
  context->const_inference_states[const_index] =
      FRONTEND_CONST_INFERENCE_DONE;
  if (type != NULL) *type = compact.known ? inferred : simple_type_unknown();
  return true;
}

static w_seed_frontend_type const_synthetic_type(frontend_simple_type simple) {
  w_seed_frontend_type type;
  (void)memset(&type, 0, sizeof(type));
  type.kind = simple.kind;
  type.spelling = simple.kind == W_SEED_FRONTEND_TYPE_BOOL
                      ? (w_seed_frontend_text){"Bool", 4}
                      : (simple.kind == W_SEED_FRONTEND_TYPE_INTEGER
                             ? const_integer_type_spelling(simple.is_signed,
                                                            simple.bit_width)
                             : (w_seed_frontend_text){NULL, 0});
  type.nominal_name = type.spelling;
  type.span = empty_span(0);
  type.is_signed = simple.is_signed;
  type.bit_width = simple.bit_width;
  type.element_type = W_SEED_FRONTEND_NONE;
  type.return_type = W_SEED_FRONTEND_NONE;
  type.first_parameter = W_SEED_FRONTEND_NONE;
  type.parameter_count = 0u;
  type.enum_base_index = W_SEED_FRONTEND_NONE;
  type.first_subset_member = W_SEED_FRONTEND_NONE;
  type.subset_member_count = 0u;
  type.generic_application_index = W_SEED_FRONTEND_NONE;
  return type;
}

/* An omitted function return annotation is the explicit Unit default. Keep
 * this as a structured type record so downstream consumers do not need to
 * inspect the declaration CST or guess from a missing index. */
static w_seed_frontend_type inferred_unit_type(w_seed_span span) {
  w_seed_frontend_type type;
  (void)memset(&type, 0, sizeof(type));
  type.kind = W_SEED_FRONTEND_TYPE_UNIT;
  type.spelling = (w_seed_frontend_text){"()", 2u};
  type.nominal_name = type.spelling;
  type.span = span;
  type.element_type = W_SEED_FRONTEND_NONE;
  type.return_type = W_SEED_FRONTEND_NONE;
  type.first_parameter = W_SEED_FRONTEND_NONE;
  type.parameter_count = 0u;
  type.enum_base_index = W_SEED_FRONTEND_NONE;
  type.first_subset_member = W_SEED_FRONTEND_NONE;
  type.subset_member_count = 0u;
  type.generic_application_index = W_SEED_FRONTEND_NONE;
  return type;
}

static bool infer_module_const_types(frontend_context *context) {
  if (context == NULL || context->module_index >= context->input.document_count)
    return false;
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return false;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_CONST_DECLARATION) {
      uint32_t const_index = W_SEED_FRONTEND_NONE;
      if (!module_const_index_for_node(context, context->module_index, child,
                                       &const_index) ||
          !const_infer_declaration(context, const_index, NULL))
        return false;
    }
    guard += 1u;
  }
  context->const_inference_complete = true;
  cursor = doc->nodes[doc->parse.root].first_child;
  guard = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_CONST_DECLARATION) {
      uint32_t const_index = W_SEED_FRONTEND_NONE;
      if (!module_const_index_for_node(context, context->module_index, child,
                                       &const_index))
        return false;
      const uint32_t type_node = direct_type_index(doc, child);
      if (type_node == W_SEED_CST_NONE) {
        frontend_simple_type simple = const_inferred_simple_type(
            &context->const_inferred_types[const_index]);
        if (const_inferred_type_is_scalar(simple)) {
          const w_seed_frontend_type synthetic = const_synthetic_type(simple);
          uint32_t type_index = W_SEED_FRONTEND_NONE;
          if (!context_append_type(context, synthetic, &type_index)) return false;
          context->const_inferred_type_indices[const_index] = type_index;
          if (context->emit && context->output != NULL &&
              context->output->const_declarations != NULL &&
              (size_t)const_index < context->count.const_declarations) {
            w_seed_frontend_const_declaration *record =
                &context->output->const_declarations[const_index];
            record->effective_type = type_index;
            if (record->symbol_index != W_SEED_FRONTEND_NONE &&
                context->output->symbols != NULL &&
                (size_t)record->symbol_index < context->count.symbols)
              context->output->symbols[record->symbol_index].type_index =
                  type_index;
          }
        }
      }
    }
    guard += 1u;
  }
  return true;
}

typedef struct {
  w_seed_span span;
  w_seed_span value_span;
  w_seed_frontend_text label;
  bool has_label;
} frontend_generic_argument_shape;

typedef bool (*frontend_static_list_child_callback)(
    const w_seed_frontend_document *doc, w_seed_span child_span,
    size_t ordinal, void *user);

static bool visit_static_list_children(
    const w_seed_frontend_document *doc, w_seed_span span,
    frontend_static_list_child_callback callback, void *user,
    size_t *child_count, bool *limit_hit);

typedef struct {
  w_seed_frontend_generic_parameter parameter;
  uint32_t type_node;
  frontend_simple_type domain_simple;
  uint32_t parameter_index;
} frontend_generic_schema_item;

static bool find_type_node_for_span(const w_seed_frontend_document *doc,
                                    w_seed_span span, uint32_t *type_node) {
  if (type_node != NULL) *type_node = W_SEED_CST_NONE;
  if (doc == NULL || type_node == NULL) return false;
  const w_seed_span wanted = trim_span(doc, span);
  uint32_t found = W_SEED_CST_NONE;
  for (size_t index = 0; index < doc->parse.node_count; index += 1u) {
    if (doc->nodes[index].kind != W_SEED_CST_TYPE) continue;
    const w_seed_span candidate = trim_span(doc, doc->nodes[index].raw_span);
    if (candidate.start_byte == wanted.start_byte &&
        candidate.end_byte == wanted.end_byte) {
      if (found == W_SEED_CST_NONE ||
          doc->nodes[index].raw_span.end_byte -
                  doc->nodes[index].raw_span.start_byte <
              doc->nodes[found].raw_span.end_byte -
                  doc->nodes[found].raw_span.start_byte) {
        found = (uint32_t)index;
      }
    }
  }
  *type_node = found;
  return found != W_SEED_CST_NONE;
}

static bool collect_generic_application_shapes(
    const w_seed_frontend_document *doc, w_seed_span envelope_span,
    frontend_generic_argument_shape *shapes, size_t capacity,
    size_t *shape_count, bool *limit_hit) {
  if (shape_count != NULL) *shape_count = 0;
  if (limit_hit != NULL) *limit_hit = false;
  if (doc == NULL || shapes == NULL || shape_count == NULL || capacity == 0u)
    return false;
  frontend_token_cursor cursor = token_cursor_for(doc, envelope_span);
  frontend_token token;
  if (!cursor_take(&cursor, &token) ||
      !text_equal(text_from_span(doc, token.span), "<")) {
    return false;
  }
  size_t angle = 0u;
  size_t paren = 0u;
  size_t bracket = 0u;
  size_t brace = 0u;
  bool argument_open = false;
  bool saw_outer_close = false;
  bool first_word = false;
  bool has_label = false;
  bool have_value_start = false;
  size_t argument_token_count = 0u;
  w_seed_span argument_span = empty_span(token.span.end_byte);
  w_seed_span last_span = argument_span;
  w_seed_span value_span = argument_span;
  w_seed_frontend_text first_text = {NULL, 0};

  while (cursor_take(&cursor, &token)) {
    const w_seed_frontend_text text = text_from_span(doc, token.span);
    const bool split_closers = text.length == 2u && text.data != NULL &&
                               text.data[0] == '>' && text.data[1] == '>';
    const size_t piece_count = split_closers ? 2u : 1u;
    for (size_t piece = 0u; piece < piece_count; piece += 1u) {
      const bool is_closer = split_closers || text_equal(text, ">");
      const w_seed_span piece_span =
          split_closers
              ? (w_seed_span){token.span.start_byte + piece,
                              token.span.start_byte + piece + 1u}
              : token.span;
      const size_t angle_before = angle;
      const char piece_text_storage[2] = {
          split_closers ? '>' : '\0', '\0'};
      const w_seed_frontend_text piece_text =
          split_closers ? (w_seed_frontend_text){piece_text_storage, 1u}
                        : text;
      const bool top_level = angle == 0u && paren == 0u && bracket == 0u &&
                             brace == 0u;

      if (is_closer && top_level) {
        if (!argument_open) {
          saw_outer_close = true;
          break;
        }
        if (last_span.end_byte <= argument_span.start_byte) return false;
        const size_t output_index = *shape_count;
        if (output_index >= capacity) {
          if (limit_hit != NULL) *limit_hit = true;
          return false;
        }
        frontend_generic_argument_shape *shape = &shapes[output_index];
        shape->span = trim_span(
            doc, (w_seed_span){argument_span.start_byte, last_span.end_byte});
        shape->value_span = has_label && have_value_start
                                ? trim_span(doc, (w_seed_span){
                                                value_span.start_byte,
                                                last_span.end_byte})
                                : shape->span;
        shape->label = has_label ? first_text
                                 : (w_seed_frontend_text){NULL, 0};
        shape->has_label = has_label;
        *shape_count += 1u;
        argument_open = false;
        saw_outer_close = true;
        break;
      }

      if (is_closer && angle != 0u) angle -= 1u;
      else if (text_equal(piece_text, "<")) angle += 1u;
      else if (text_equal(piece_text, "(")) paren += 1u;
      else if (text_equal(piece_text, ")")) {
        if (paren == 0u) return false;
        paren -= 1u;
      } else if (text_equal(piece_text, "[")) bracket += 1u;
      else if (text_equal(piece_text, "]")) {
        if (bracket == 0u) return false;
        bracket -= 1u;
      } else if (text_equal(piece_text, "{")) brace += 1u;
      else if (text_equal(piece_text, "}")) {
        if (brace == 0u) return false;
        brace -= 1u;
      }

      if (!argument_open) {
        if (text_equal(piece_text, ",")) return false;
        argument_open = true;
        argument_span = piece_span;
        last_span = piece_span;
        value_span = piece_span;
        argument_token_count = 1u;
        first_word = token.kind == W_SEED_CST_WORD && !split_closers;
        first_text = first_word ? text_from_span(doc, token.span)
                                : (w_seed_frontend_text){NULL, 0};
        has_label = false;
        have_value_start = false;
        continue;
      }

      if (text_equal(piece_text, ",") && top_level) {
        if (last_span.end_byte <= argument_span.start_byte) return false;
        const size_t output_index = *shape_count;
        if (output_index >= capacity) {
          if (limit_hit != NULL) *limit_hit = true;
          return false;
        }
        frontend_generic_argument_shape *shape = &shapes[output_index];
        shape->span = trim_span(
            doc, (w_seed_span){argument_span.start_byte, last_span.end_byte});
        shape->value_span = has_label && have_value_start
                                ? trim_span(doc, (w_seed_span){
                                                value_span.start_byte,
                                                last_span.end_byte})
                                : shape->span;
        shape->label = has_label ? first_text
                                 : (w_seed_frontend_text){NULL, 0};
        shape->has_label = has_label;
        *shape_count += 1u;
        argument_open = false;
        continue;
      }

      if (text_equal(piece_text, ":") && top_level && argument_token_count == 1u &&
          first_word) {
        has_label = true;
        have_value_start = false;
        last_span = piece_span;
        continue;
      }
      if (has_label && !have_value_start && !text_equal(piece_text, ":")) {
        value_span = piece_span;
        have_value_start = true;
      }
      /* A lexer `>>` leaf closes a nested envelope and the outer envelope.
       * The nested TYPE CST owns the complete leaf, so retain its full end
       * for the enclosing argument span while still consuming the two
       * structural closes independently. */
      last_span = split_closers && piece == 0u && angle_before != 0u
                      ? token.span
                      : piece_span;
      argument_token_count += 1u;
    }
    if (saw_outer_close) break;
  }
  if (!saw_outer_close || angle != 0u || paren != 0u || bracket != 0u ||
      brace != 0u || argument_open) {
    /* A final comma leaves no open argument.  The envelope close is still
     * required, and an empty segment is intentionally ignored. */
    if (!saw_outer_close || argument_open) return false;
  }
  return true;
}

/* Walk one StaticList envelope without materializing every token or child
 * span. The callback pass is repeated after the parent ConstValue exists, so
 * element records always point at a contiguous owner range. */
static bool visit_static_list_children(
    const w_seed_frontend_document *doc, w_seed_span span,
    frontend_static_list_child_callback callback, void *user,
    size_t *child_count, bool *limit_hit) {
  if (child_count != NULL) *child_count = 0u;
  if (limit_hit != NULL) *limit_hit = false;
  if (doc == NULL || child_count == NULL) return false;
  frontend_token_cursor cursor = token_cursor_for(doc, trim_span(doc, span));
  frontend_token token;
  if (!cursor_take(&cursor, &token) ||
      !text_equal(text_from_span(doc, token.span), "[")) {
    return false;
  }
  size_t angle = 0u;
  size_t paren = 0u;
  size_t bracket = 0u;
  size_t brace = 0u;
  bool child_open = false;
  bool closed = false;
  w_seed_span child_start = empty_span(token.span.end_byte);
  w_seed_span child_last = child_start;
  while (cursor_take(&cursor, &token)) {
    const w_seed_frontend_text text = text_from_span(doc, token.span);
    const bool top_level = angle == 0u && paren == 0u && bracket == 0u &&
                           brace == 0u;
    if (!child_open && text_equal(text, "]")) {
      closed = true;
      break;
    }
    if (child_open && top_level &&
        (text_equal(text, ",") || text_equal(text, "]"))) {
      if (child_last.end_byte <= child_start.start_byte) return false;
      if (*child_count >= W_SEED_FRONTEND_MAX_STATIC_LIST_ELEMENTS) {
        if (limit_hit != NULL) *limit_hit = true;
        return false;
      }
      const w_seed_span child_span = trim_span(
          doc, (w_seed_span){child_start.start_byte, child_last.end_byte});
      if (callback != NULL &&
          !callback(doc, child_span, *child_count, user)) {
        return false;
      }
      *child_count += 1u;
      child_open = false;
      if (text_equal(text, "]")) {
        closed = true;
        break;
      }
      continue;
    }
    if (!child_open) {
      if (text_equal(text, ",")) return false;
      child_open = true;
      child_start = token.span;
      child_last = token.span;
    } else {
      if (text_equal(text, "<")) angle += 1u;
      else if (text_equal(text, ">")) {
        if (angle == 0u) return false;
        angle -= 1u;
      } else if (text_equal(text, "(")) paren += 1u;
      else if (text_equal(text, ")")) {
        if (paren == 0u) return false;
        paren -= 1u;
      } else if (text_equal(text, "[")) bracket += 1u;
      else if (text_equal(text, "]")) {
        if (bracket == 0u) return false;
        bracket -= 1u;
      } else if (text_equal(text, "{")) brace += 1u;
      else if (text_equal(text, "}")) {
        if (brace == 0u) return false;
        brace -= 1u;
      }
      child_last = token.span;
    }
  }
  if (!closed || child_open || angle != 0u || paren != 0u || bracket != 0u ||
      brace != 0u) {
    return false;
  }
  return true;
}

static size_t generic_parameter_base_for_struct(
    const frontend_context *context, uint32_t wanted_struct) {
  if (context == NULL || wanted_struct == W_SEED_FRONTEND_NONE) return SIZE_MAX;
  size_t struct_ordinal = 0u;
  size_t parameter_ordinal = 0u;
  for (size_t module = 0; module < context->input.document_count; module += 1u) {
    const w_seed_frontend_document *doc = &context->input.documents[module];
    uint32_t cursor = doc->nodes[doc->parse.root].first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t guard = 0u;
    while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
      if (doc->nodes[child].kind == W_SEED_CST_STRUCT) {
        if (struct_ordinal == (size_t)wanted_struct) return parameter_ordinal;
        parameter_ordinal += count_direct_kind(
            doc, first_direct_kind(doc, child, W_SEED_CST_GENERIC_PARAMETERS),
            W_SEED_CST_GENERIC_PARAMETER);
        struct_ordinal += 1u;
      }
      guard += 1u;
    }
  }
  return SIZE_MAX;
}

static bool build_generic_schema(
    frontend_context *context, const w_seed_frontend_document *doc,
    uint32_t struct_node, uint32_t struct_index,
    frontend_generic_schema_item *items, size_t capacity, size_t *item_count,
    bool *limit_hit) {
  if (item_count != NULL) *item_count = 0u;
  if (limit_hit != NULL) *limit_hit = false;
  if (context == NULL || doc == NULL || items == NULL || item_count == NULL)
    return false;
  const uint32_t generic_node =
      first_direct_kind(doc, struct_node, W_SEED_CST_GENERIC_PARAMETERS);
  if (generic_node == W_SEED_CST_NONE) return true;
  const size_t base = generic_parameter_base_for_struct(context, struct_index);
  if (base == SIZE_MAX) return false;
  /* Generic schema slots have an explicit seed ceiling. Keep scratch name
   * tables bounded by that ceiling instead of the full CST node budget. */
  w_seed_frontend_text prior_names[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  w_seed_frontend_text prior_external_labels[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  bool prior_is_type[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  (void)memset(prior_names, 0, sizeof(prior_names));
  (void)memset(prior_external_labels, 0, sizeof(prior_external_labels));
  (void)memset(prior_is_type, 0, sizeof(prior_is_type));
  uint32_t cursor = doc->nodes[generic_node].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t ordinal = 0u;
  size_t guard = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind != W_SEED_CST_GENERIC_PARAMETER) {
      guard += 1u;
      continue;
    }
    if (ordinal >= capacity || ordinal >= W_SEED_FRONTEND_MAX_GENERIC_SLOTS) {
      (void)context_append_fact(
          context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
          doc->nodes[child].raw_span,
          text_from_span(doc, doc->nodes[child].raw_span));
      if (limit_hit != NULL) *limit_hit = true;
      break;
    }
    frontend_generic_schema_item *item = &items[ordinal];
    (void)memset(item, 0, sizeof(*item));
    item->parameter.module_index = (uint32_t)context->module_index;
    item->parameter.owner_kind = W_SEED_FRONTEND_DECL_STRUCT;
    item->parameter.owner_index = struct_index;
    item->parameter.ordinal = (uint32_t)ordinal;
    item->parameter.external_label = generic_parameter_external_label(doc, child);
    item->parameter.internal_name = generic_parameter_name(doc, child);
    item->parameter.label_kind = generic_parameter_label_omitted(doc, child)
                                     ? W_SEED_FRONTEND_LABEL_OPTIONAL
                                     : W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY;
    item->parameter.kind = W_SEED_FRONTEND_GENERIC_KIND_TYPE;
    item->parameter.span = doc->nodes[child].raw_span;
    item->parameter.domain_type = W_SEED_FRONTEND_NONE;
    item->parameter.refinement_kind = W_SEED_FRONTEND_GENERIC_REFINEMENT_NONE;
    item->parameter.predicate_function_index = W_SEED_FRONTEND_NONE;
    item->parameter.predicate_span = empty_span(item->parameter.span.start_byte);
    item->parameter.predicate_function_span =
        empty_span(item->parameter.span.start_byte);
    item->parameter.subject_kind = W_SEED_FRONTEND_GENERIC_SUBJECT_NONE;
    item->parameter.domain_kind = W_SEED_FRONTEND_GENERIC_DOMAIN_NONE;
    item->parameter.dependent_type_parameter_ordinal = W_SEED_FRONTEND_NONE;
    item->type_node = generic_parameter_type_node(doc, child);
    item->domain_simple = simple_type_unknown();
    item->parameter_index = (uint32_t)(base + ordinal);
    if (item->parameter_index < FRONTEND_MAX_GENERIC_METADATA)
      item->parameter.refinement_kind =
          context->generic_refinement_kinds[item->parameter_index];
    else {
      /* Metadata is a caller-owned seed projection. Crossing its bounded
       * ceiling is not a language-schema error: keep the CST-derived TYPE /
       * VALUE classification below and make the application UNSUPPORTED. */
      if (limit_hit != NULL) *limit_hit = true;
      (void)context_append_fact(
          context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
          item->parameter.span,
          text_from_span(doc, item->parameter.span));
    }
    bool duplicate_schema_name = false;
    for (size_t prior = 0u; prior < ordinal; prior += 1u) {
      if ((item->parameter.internal_name.length != 0u &&
           text_equal_text(item->parameter.internal_name,
                           prior_names[prior])) ||
          (item->parameter.external_label.length != 0u &&
           text_equal_text(item->parameter.external_label,
                           prior_external_labels[prior]))) {
        duplicate_schema_name = true;
        break;
      }
    }
    if (duplicate_schema_name) {
      item->parameter.kind = W_SEED_FRONTEND_GENERIC_KIND_INVALID;
      item->parameter.domain_kind = W_SEED_FRONTEND_GENERIC_DOMAIN_INVALID;
    } else if (item->type_node != W_SEED_CST_NONE) {
      item->parameter.label_kind = generic_parameter_label_omitted(doc, child)
                                       ? W_SEED_FRONTEND_LABEL_OPTIONAL
                                       : generic_parameter_word_at(doc, child, 1u)
                                                     .length != 0u
                                             ? W_SEED_FRONTEND_LABEL_EXTERNAL_REQUIRED
                                             : W_SEED_FRONTEND_LABEL_NAMED_REQUIRED;
      uint32_t dependent_ordinal = W_SEED_FRONTEND_NONE;
      const bool dependent = generic_domain_refers_to_previous_type(
          doc, child, NULL, &dependent_ordinal, prior_names, prior_is_type,
          ordinal);
      const bool supported = dependent || generic_value_domain_supported(
          context, doc, item->type_node, &item->domain_simple);
      item->parameter.kind = supported ? W_SEED_FRONTEND_GENERIC_KIND_VALUE
                                       : W_SEED_FRONTEND_GENERIC_KIND_INVALID;
      item->parameter.domain_kind = dependent
                                        ? W_SEED_FRONTEND_GENERIC_DOMAIN_DEPENDENT
                                        : supported
                                              ? W_SEED_FRONTEND_GENERIC_DOMAIN_CONCRETE
                                              : W_SEED_FRONTEND_GENERIC_DOMAIN_INVALID;
      item->parameter.dependent_type_parameter_ordinal =
          dependent ? dependent_ordinal : W_SEED_FRONTEND_NONE;
      if (item->parameter_index < FRONTEND_MAX_GENERIC_METADATA)
        item->parameter.domain_type =
            context->generic_domain_type_indices[item->parameter_index];
    }
    prior_names[ordinal] = item->parameter.internal_name;
    prior_external_labels[ordinal] = item->parameter.external_label;
    prior_is_type[ordinal] =
        item->parameter.kind == W_SEED_FRONTEND_GENERIC_KIND_TYPE;
    *item_count += 1u;
    ordinal += 1u;
    guard += 1u;
  }
  return true;
}

static frontend_simple_type simple_type_from_type_index(
    frontend_context *context, uint32_t type_index,
    frontend_simple_type fallback) {
  if (context == NULL || !context->emit || context->output == NULL ||
      type_index == W_SEED_FRONTEND_NONE ||
      type_index >= context->count.types || context->output->types == NULL)
    return fallback;
  const w_seed_frontend_type *type = &context->output->types[type_index];
  frontend_simple_type result = simple_type_unknown();
  result.kind = type->kind;
  result.spelling = type->spelling;
  result.is_signed = type->is_signed;
  result.bit_width = type->bit_width;
  result.enum_index = type->enum_base_index;
  result.enum_name = type->nominal_name;
  return result.kind == W_SEED_FRONTEND_TYPE_UNKNOWN ? fallback : result;
}

static bool append_invalid_const_value(frontend_context *context,
                                       w_seed_span span, uint32_t type_index,
                                       uint32_t *index) {
  w_seed_frontend_const_value value;
  (void)memset(&value, 0, sizeof(value));
  value.kind = W_SEED_FRONTEND_CONST_INVALID;
  value.type_index = type_index;
  value.span = span;
  value.first_byte = W_SEED_FRONTEND_NONE;
  value.enum_base_index = W_SEED_FRONTEND_NONE;
  value.enum_case_index = W_SEED_FRONTEND_NONE;
  value.first_element = W_SEED_FRONTEND_NONE;
  value.element_count = 0u;
  if (!context_append_const_value(context, value, index)) return false;
  return false;
}

static bool const_tokens_for_span(const w_seed_frontend_document *doc,
                                  w_seed_span span, frontend_token *tokens,
                                  size_t capacity, size_t *count) {
  if (count != NULL) *count = 0u;
  if (doc == NULL || tokens == NULL || count == NULL || capacity == 0u)
    return false;
  frontend_token_cursor cursor = token_cursor_for(doc, trim_span(doc, span));
  frontend_token token;
  while (cursor_take(&cursor, &token)) {
    if (*count >= capacity) return false;
    tokens[(*count)++] = token;
  }
  return *count != 0u;
}

static bool const_integer_value(frontend_context *context,
                               w_seed_frontend_text spelling,
                               frontend_simple_type expected,
                               w_seed_span span, uint32_t type_index,
                               uint32_t *index) {
  frontend_simple_type actual = literal_simple_type(
      context_document(context), span, W_SEED_CST_NUMBER);
  if (actual.kind != W_SEED_FRONTEND_TYPE_INTEGER ||
      expected.kind != W_SEED_FRONTEND_TYPE_INTEGER ||
      !frontend_widening_allowed(context, actual, expected)) {
    return append_invalid_const_value(context, span, type_index, index);
  }
  size_t body_end = spelling.length;
  bool has_suffix = false;
  bool is_signed = true;
  uint16_t width = 0u;
  uint64_t magnitude = 0u;
  if (!integer_literal_parts(spelling, &body_end, &has_suffix, &is_signed,
                             &width) ||
      !integer_literal_value(spelling, body_end, &magnitude)) {
    return append_invalid_const_value(context, span, type_index, index);
  }
  w_seed_frontend_const_value value;
  (void)memset(&value, 0, sizeof(value));
  value.kind = W_SEED_FRONTEND_CONST_INTEGER;
  value.type_index = type_index;
  value.span = span;
  /* The declared value domain owns the identity.  A compatible literal
   * suffix affects admission, but it must not change the canonical payload
   * type recorded for the generic slot. */
  value.integer_signed = expected.is_signed;
  value.integer_bit_width = expected.bit_width;
  value.integer_byte_count = (uint8_t)(value.integer_bit_width / 8u);
  if (value.integer_byte_count == 0u || value.integer_byte_count > 16u)
    return append_invalid_const_value(context, span, type_index, index);
  for (size_t byte = 0; byte < value.integer_byte_count; byte += 1u) {
    value.integer_bytes[byte] = (uint8_t)(magnitude & 0xffu);
    magnitude >>= 8u;
  }
  value.enum_base_index = W_SEED_FRONTEND_NONE;
  value.enum_case_index = W_SEED_FRONTEND_NONE;
  value.first_byte = W_SEED_FRONTEND_NONE;
  value.first_element = W_SEED_FRONTEND_NONE;
  return context_append_const_value(context, value, index);
}

static bool const_value_for_span(frontend_context *context,
                                 const w_seed_frontend_document *doc,
                                 w_seed_span span, frontend_simple_type expected,
                                 uint32_t type_index, uint32_t *index);
static bool const_argument_shape_unsupported(
    const w_seed_frontend_document *doc, w_seed_span span,
    frontend_simple_type expected);
typedef enum {
  FRONTEND_STATIC_ARGUMENT_IMMEDIATE = 0,
  FRONTEND_STATIC_ARGUMENT_REJECTED,
  FRONTEND_STATIC_ARGUMENT_UNSUPPORTED,
} frontend_static_argument_support;
static frontend_static_argument_support static_argument_representable(
    const frontend_context *context, frontend_simple_type type);
static bool generic_type_argument_is_literal(
    const w_seed_frontend_document *doc, w_seed_span span);

typedef struct {
  frontend_context *context;
  const w_seed_frontend_document *doc;
  frontend_simple_type element_expected;
  uint32_t element_type_index;
  uint32_t parent_index;
  bool all_valid;
} frontend_static_list_emit;

static bool emit_static_list_child(const w_seed_frontend_document *doc,
                                   w_seed_span child_span, size_t ordinal,
                                   void *user) {
  frontend_static_list_emit *emit = (frontend_static_list_emit *)user;
  if (emit == NULL || emit->context == NULL || emit->doc != doc) return false;
  uint32_t child_index = W_SEED_FRONTEND_NONE;
  const bool child_valid = const_value_for_span(
      emit->context, doc, child_span, emit->element_expected,
      emit->element_type_index, &child_index);
  if (!child_valid) {
    emit->all_valid = false;
    if (const_argument_shape_unsupported(doc, child_span,
                                         emit->element_expected)) {
      (void)context_append_fact(
          emit->context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE, child_span,
          text_from_span(doc, child_span));
    }
  }
  w_seed_frontend_const_element element;
  element.owner_value = emit->parent_index;
  element.ordinal = (uint32_t)ordinal;
  element.value_index = child_index;
  element.span = child_span;
  uint32_t element_index = W_SEED_FRONTEND_NONE;
  return context_append_const_element(emit->context, element, &element_index);
}

static bool const_value_for_span(frontend_context *context,
                                 const w_seed_frontend_document *doc,
                                 w_seed_span span, frontend_simple_type expected,
                                 uint32_t type_index, uint32_t *index) {
  if (context == NULL || doc == NULL || index == NULL) return false;
  *index = W_SEED_FRONTEND_NONE;
  span = trim_span(doc, span);

  if (expected.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST) {
    frontend_simple_type element_expected = static_list_element_type(context,
                                                                      expected);
    uint32_t element_type_index = W_SEED_FRONTEND_NONE;
    if (type_index != W_SEED_FRONTEND_NONE) {
      element_expected = simple_type_from_type_index(context, type_index,
                                                      element_expected);
      if (context->emit && context->output != NULL &&
          type_index < context->count.types &&
          context->output->types[type_index].element_type !=
              W_SEED_FRONTEND_NONE) {
        element_type_index = context->output->types[type_index].element_type;
        element_expected = simple_type_from_type_index(
            context, element_type_index, element_expected);
      } else if (type_index < W_SEED_FRONTEND_MAX_CST_NODES) {
        /* normalize_type_tree emits the StaticList element immediately after
         * its root in this seed.  This fallback keeps dry-run identities
         * equal to the emitted pass without host pointers. */
        element_type_index = type_index + 1u;
      }
    }
    if (element_expected.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST &&
        element_type_index != W_SEED_FRONTEND_NONE) {
      element_expected = simple_type_from_type_index(context,
                                                      element_type_index,
                                                      element_expected);
    }
    if (element_expected.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST)
      element_expected = static_list_element_type(context, element_expected);
    if (element_expected.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST) {
      /* Nested structural lists are outside this seed.  Reject before the
       * parent is appended so no child edge can escape an invalid owner. */
      return append_invalid_const_value(context, span, type_index, index);
    }
    size_t child_count = 0u;
    bool list_limit_hit = false;
    if (!visit_static_list_children(doc, span, NULL, NULL, &child_count,
                                    &list_limit_hit)) {
      if (list_limit_hit) {
        (void)context_append_fact(
            context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE, span,
            text_from_span(doc, span));
      }
      return append_invalid_const_value(context, span, type_index, index);
    }
    w_seed_frontend_const_value value;
    (void)memset(&value, 0, sizeof(value));
    value.kind = W_SEED_FRONTEND_CONST_STATIC_LIST;
    value.type_index = type_index;
    value.span = span;
    value.first_byte = W_SEED_FRONTEND_NONE;
    value.enum_base_index = W_SEED_FRONTEND_NONE;
    value.enum_case_index = W_SEED_FRONTEND_NONE;
    value.first_element = child_count == 0u
                              ? W_SEED_FRONTEND_NONE
                              : (uint32_t)context->count.const_elements;
    value.element_count = (uint32_t)child_count;
    uint32_t parent_index = W_SEED_FRONTEND_NONE;
    if (!context_append_const_value(context, value, &parent_index)) return false;
    *index = parent_index;
    frontend_static_list_emit emit = {
        context, doc, element_expected, element_type_index, parent_index, true};
    size_t emitted_count = 0u;
    if (!visit_static_list_children(doc, span, emit_static_list_child, &emit,
                                    &emitted_count, &list_limit_hit) ||
        emitted_count != child_count)
      emit.all_valid = false;
    if (!emit.all_valid && context->emit && context->output != NULL &&
        parent_index < context->output->const_value_capacity)
      context->output->const_values[parent_index].kind =
          W_SEED_FRONTEND_CONST_INVALID;
    return emit.all_valid;
  }

  frontend_token tokens[W_SEED_FRONTEND_MAX_NESTING * 2u];
  size_t token_count = 0u;
  if (!const_tokens_for_span(doc, span, tokens,
                             sizeof(tokens) / sizeof(tokens[0]), &token_count))
    return append_invalid_const_value(context, span, type_index, index);

  if (expected.kind == W_SEED_FRONTEND_TYPE_BOOL && token_count == 1u) {
    const w_seed_frontend_text text = text_from_span(doc, tokens[0].span);
    if (text_equal(text, "true") || text_equal(text, "false")) {
      w_seed_frontend_const_value value;
      (void)memset(&value, 0, sizeof(value));
      value.kind = W_SEED_FRONTEND_CONST_BOOL;
      value.type_index = type_index;
      value.span = span;
      value.bool_value = text_equal(text, "true");
      value.first_byte = W_SEED_FRONTEND_NONE;
      value.enum_base_index = W_SEED_FRONTEND_NONE;
      value.enum_case_index = W_SEED_FRONTEND_NONE;
      value.first_element = W_SEED_FRONTEND_NONE;
      return context_append_const_value(context, value, index);
    }
  }

  if (expected.kind == W_SEED_FRONTEND_TYPE_STRING && token_count != 0u) {
    bool literal_only = true;
    for (size_t token_index = 0u; token_index < token_count; token_index += 1u)
      if (tokens[token_index].kind != W_SEED_CST_LITERAL_EVENT)
        literal_only = false;
    const w_seed_frontend_text text = text_from_span(doc, span);
    if (text.length >= 2u &&
        (text.data[0] == '"' || text.data[0] == '\'') &&
        text.data[text.length - 1u] == text.data[0] && literal_only) {
      bool escaped = false;
      for (size_t byte = 1u; byte + 1u < text.length; byte += 1u)
        if (text.data[byte] == '\\') escaped = true;
      if (!escaped) {
        uint32_t offset = W_SEED_FRONTEND_NONE;
        if (!context_append_const_bytes(
                context, (const uint8_t *)text.data + 1u, text.length - 2u,
                &offset)) return false;
        w_seed_frontend_const_value value;
        (void)memset(&value, 0, sizeof(value));
        value.kind = W_SEED_FRONTEND_CONST_STRING;
        value.type_index = type_index;
        value.span = span;
        value.first_byte = offset;
        value.byte_count = (uint32_t)(text.length - 2u);
        value.enum_base_index = W_SEED_FRONTEND_NONE;
        value.enum_case_index = W_SEED_FRONTEND_NONE;
        value.first_element = W_SEED_FRONTEND_NONE;
        return context_append_const_value(context, value, index);
      }
    }
  }

  if (expected.kind == W_SEED_FRONTEND_TYPE_INTEGER && token_count == 1u) {
    const w_seed_frontend_text spelling = text_from_span(doc, tokens[0].span);
    return const_integer_value(context, spelling, expected, span, type_index,
                               index);
  }

  if (expected.kind == W_SEED_FRONTEND_TYPE_ENUM) {
    w_seed_frontend_text case_name = {NULL, 0};
    if (token_count == 2u &&
        text_equal(text_from_span(doc, tokens[0].span), ".") &&
        tokens[1].kind == W_SEED_CST_WORD) {
      case_name = text_from_span(doc, tokens[1].span);
    } else if (token_count == 3u && tokens[0].kind == W_SEED_CST_WORD &&
               text_equal(text_from_span(doc, tokens[1].span), ".") &&
               tokens[2].kind == W_SEED_CST_WORD &&
               text_equal_text(text_from_span(doc, tokens[0].span),
                               expected.enum_name)) {
      /* A qualified case must resolve through the expected enum.  Matching
       * only the final member name would accept OtherStage.accepted when the
       * slot domain is ServiceStage. */
      case_name = text_from_span(doc, tokens[2].span);
    }
    uint32_t case_index = W_SEED_FRONTEND_NONE;
    if (case_name.length != 0u && enum_case_for_name(
                                     context, expected.enum_index, case_name,
                                     &case_index, NULL, NULL)) {
      if (enum_case_parameter_count(context, case_index) != 0u) {
        (void)context_append_fact(
            context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE, span,
            text_from_span(doc, span));
        return append_invalid_const_value(context, span, type_index, index);
      }
      w_seed_frontend_const_value value;
      (void)memset(&value, 0, sizeof(value));
      value.kind = W_SEED_FRONTEND_CONST_ENUM_CASE;
      value.type_index = type_index;
      value.span = span;
      value.enum_base_index = expected.enum_index;
      value.enum_case_index = case_index;
      value.first_byte = W_SEED_FRONTEND_NONE;
      value.first_element = W_SEED_FRONTEND_NONE;
      return context_append_const_value(context, value, index);
    }
  }
  return append_invalid_const_value(context, span, type_index, index);
}

static bool const_argument_shape_unsupported(
    const w_seed_frontend_document *doc, w_seed_span span,
    frontend_simple_type expected) {
  if (doc == NULL) return false;
  frontend_token tokens[W_SEED_FRONTEND_MAX_NESTING * 2u];
  size_t token_count = 0u;
  if (!const_tokens_for_span(doc, span, tokens,
                             sizeof(tokens) / sizeof(tokens[0]), &token_count))
    return false;
  if (expected.kind == W_SEED_FRONTEND_TYPE_STRING && token_count != 0u) {
    const w_seed_frontend_text text = text_from_span(doc, trim_span(doc, span));
    if (text.length >= 2u && (text.data[0] == '"' || text.data[0] == '\'')) {
      for (size_t index = 1u; index + 1u < text.length; index += 1u)
        if (text.data[index] == '\\') return true;
    }
  }
  if (expected.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST && token_count >= 2u &&
      text_equal(text_from_span(doc, tokens[0].span), "[") &&
      text_equal(text_from_span(doc, tokens[token_count - 1u].span), "]"))
    return false;
  if (expected.kind == W_SEED_FRONTEND_TYPE_ENUM) return false;
  return token_count > 1u ||
         (token_count != 0u &&
          text_equal(text_from_span(doc, tokens[0].span), "("));
}

static frontend_static_argument_support static_argument_representable(
    const frontend_context *context, frontend_simple_type type) {
  switch (type.kind) {
    case W_SEED_FRONTEND_TYPE_BOOL:
    case W_SEED_FRONTEND_TYPE_INTEGER:
    case W_SEED_FRONTEND_TYPE_STRING:
    case W_SEED_FRONTEND_TYPE_ENUM:
      return FRONTEND_STATIC_ARGUMENT_IMMEDIATE;
    case W_SEED_FRONTEND_TYPE_BYTES:
    case W_SEED_FRONTEND_TYPE_ENUM_SUBSET:
      return FRONTEND_STATIC_ARGUMENT_UNSUPPORTED;
    case W_SEED_FRONTEND_TYPE_STATIC_LIST: {
      const frontend_simple_type element = static_list_element_type(context, type);
      if (element.kind == W_SEED_FRONTEND_TYPE_UNKNOWN)
        return FRONTEND_STATIC_ARGUMENT_REJECTED;
      return static_argument_representable(context, element);
    }
    case W_SEED_FRONTEND_TYPE_NOMINAL: {
      uint32_t struct_index = W_SEED_FRONTEND_NONE;
      uint32_t struct_node = W_SEED_CST_NONE;
      const w_seed_frontend_document *owner_doc = NULL;
      return struct_declaration_for_name(
                 context, type.spelling, &struct_index, &owner_doc,
                 &struct_node)
                 ? FRONTEND_STATIC_ARGUMENT_UNSUPPORTED
                 : FRONTEND_STATIC_ARGUMENT_REJECTED;
    }
    default:
      return FRONTEND_STATIC_ARGUMENT_REJECTED;
  }
}

static bool generic_type_argument_is_literal(
    const w_seed_frontend_document *doc, w_seed_span span) {
  if (doc == NULL) return false;
  frontend_token tokens[W_SEED_FRONTEND_MAX_NESTING * 2u];
  size_t token_count = 0u;
  if (!const_tokens_for_span(doc, span, tokens,
                             sizeof(tokens) / sizeof(tokens[0]), &token_count) ||
      token_count == 0u)
    return false;
  const w_seed_frontend_text first = text_from_span(doc, tokens[0].span);
  if (text_equal(first, "[") || text_equal(first, "(") ||
      text_equal(first, "{") || text_equal(first, "true") ||
      text_equal(first, "false"))
    return true;
  if (tokens[0].kind == W_SEED_CST_NUMBER ||
      tokens[0].kind == W_SEED_CST_LITERAL_EVENT)
    return true;
  return false;
}

/* Classify an offending application argument without publishing its source
 * spelling as a kind.  Literal values use their semantic scalar category;
 * an argument that resolves as a type uses `type`; expressions that cannot be
 * typed stay in the stable unresolved category. */
static bool diagnostic_generic_actual_kind(
    const w_seed_frontend_document *doc, w_seed_span span,
    w_seed_frontend_text *kind_out) {
  if (kind_out != NULL) *kind_out = (w_seed_frontend_text){NULL, 0u};
  if (doc == NULL || kind_out == NULL) return false;
  frontend_token tokens[W_SEED_FRONTEND_MAX_NESTING * 2u];
  size_t token_count = 0u;
  if (!const_tokens_for_span(doc, span, tokens,
                             sizeof(tokens) / sizeof(tokens[0]),
                             &token_count) ||
      token_count == 0u) {
    *kind_out = (w_seed_frontend_text){"value:unresolved",
                                       sizeof("value:unresolved") - 1u};
    return true;
  }
  for (size_t index = 0u; index < token_count; index += 1u) {
    if (text_equal(text_from_span(doc, tokens[index].span), ".")) {
      *kind_out = (w_seed_frontend_text){"unbound-contextual-member",
                                         sizeof("unbound-contextual-member") -
                                             1u};
      return true;
    }
  }
  const frontend_token *first = &tokens[0];
  const w_seed_frontend_text first_text = text_from_span(doc, first->span);
  if (token_count == 1u &&
      (first->kind == W_SEED_CST_NUMBER ||
       first->kind == W_SEED_CST_LITERAL_EVENT ||
       text_equal(first_text, "true") || text_equal(first_text, "false"))) {
    const frontend_simple_type literal =
        literal_simple_type(doc, first->span, first->kind);
    if (diagnostic_value_kind_from_simple(literal, false, kind_out)) return true;
  }
  uint32_t type_node = W_SEED_CST_NONE;
  if (find_type_node_for_span(doc, span, &type_node)) {
    *kind_out = (w_seed_frontend_text){"type", sizeof("type") - 1u};
    return true;
  }
  *kind_out = (w_seed_frontend_text){"value:unresolved",
                                     sizeof("value:unresolved") - 1u};
  return true;
}

static bool generic_value_is_parenthesized(
    const w_seed_frontend_document *doc, w_seed_span span) {
  if (doc == NULL) return false;
  frontend_token tokens[W_SEED_FRONTEND_MAX_NESTING * 2u];
  size_t token_count = 0u;
  if (!const_tokens_for_span(doc, span, tokens,
                             sizeof(tokens) / sizeof(tokens[0]),
                             &token_count) ||
      token_count < 2u)
    return false;
  return text_equal(text_from_span(doc, tokens[0].span), "(") &&
         text_equal(text_from_span(doc, tokens[token_count - 1u].span), ")");
}

static bool generic_typed_scalar_expected(frontend_simple_type type) {
  return type.kind == W_SEED_FRONTEND_TYPE_BOOL ||
         (type.kind == W_SEED_FRONTEND_TYPE_INTEGER && type.bit_width != 0u &&
          type.bit_width <= 128u);
}

static bool resolve_one_pending_generic_application(
    frontend_context *context, const frontend_pending_application *pending) {
  if (context == NULL || pending == NULL || pending->module_index >=
                                                 context->input.document_count)
    return false;
  const size_t diagnostics_before = context->count.diagnostics;
  const w_seed_frontend_document *doc =
      &context->input.documents[pending->module_index];
  uint32_t struct_index = W_SEED_FRONTEND_NONE;
  uint32_t struct_node = W_SEED_CST_NONE;
  w_seed_frontend_text head_name = {NULL, 0};
  w_seed_span envelope_span = empty_span(0);
  const size_t saved_module = context->module_index;
  context->module_index = pending->module_index;
  if (!local_generic_head_for_type(context, doc, pending->type_node,
                                   &struct_index, &struct_node, &head_name,
                                   &envelope_span)) {
    context->module_index = saved_module;
    return false;
  }
  bool has_later_envelope = false;
  uint32_t envelope_cursor = doc->nodes[pending->type_node].first_child;
  uint32_t envelope_child = W_SEED_CST_NONE;
  bool saw_first_envelope = false;
  size_t envelope_guard = 0u;
  while (next_child(doc, &envelope_cursor, &envelope_child) &&
         envelope_guard < doc->parse.node_count) {
    if (doc->nodes[envelope_child].kind == W_SEED_CST_CONTRACT_ENVELOPE) {
      if (saw_first_envelope) {
        has_later_envelope = true;
        break;
      }
      saw_first_envelope = true;
    }
    envelope_guard += 1u;
  }
  frontend_generic_schema_item schema[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  size_t schema_count = 0u;
  bool schema_limit_hit = false;
  if (!build_generic_schema(context, doc, struct_node, struct_index, schema,
                            sizeof(schema) / sizeof(schema[0]), &schema_count,
                            &schema_limit_hit)) {
    context->module_index = saved_module;
    return false;
  }
  bool schema_invalid = false;
  for (size_t schema_index = 0u; schema_index < schema_count;
       schema_index += 1u) {
    if (schema[schema_index].parameter.kind ==
            W_SEED_FRONTEND_GENERIC_KIND_INVALID ||
        schema[schema_index].parameter.refinement_kind ==
            W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID)
      schema_invalid = true;
  }
  w_seed_frontend_text available_slots[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  size_t available_slot_count = 0u;
  for (size_t schema_index = 0u; schema_index < schema_count;
       schema_index += 1u) {
    const w_seed_frontend_text external_label =
        schema[schema_index].parameter.external_label;
    if (external_label.length == 0u) continue;
    if (available_slot_count >=
        sizeof(available_slots) / sizeof(available_slots[0])) {
      context->module_index = saved_module;
      return false;
    }
    available_slots[available_slot_count] = external_label;
    available_slot_count += 1u;
  }
  const w_seed_span application_head_span =
      generic_head_type_span(doc, pending->type_node);
  frontend_generic_argument_shape shapes[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  size_t shape_count = 0u;
  bool shape_limit_hit = false;
  const bool shapes_ok = collect_generic_application_shapes(
      doc, envelope_span, shapes, sizeof(shapes) / sizeof(shapes[0]),
      &shape_count, &shape_limit_hit);
  typedef struct {
    w_seed_frontend_generic_argument value;
    frontend_simple_type type_simple;
  } generic_argument_build;
  generic_argument_build built[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  (void)memset(built, 0, sizeof(built));
  bool bound[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  (void)memset(bound, 0, sizeof(bound));
  size_t next_slot = 0u;
  bool named_seen = false;
  bool valid = shapes_ok && !schema_invalid;
  bool semantic_error = schema_invalid || (!shapes_ok && !shape_limit_hit);
  bool unsupported_seen = has_later_envelope || schema_limit_hit;
  bool typed_const_expr_seen = false;
  if (has_later_envelope) {
    (void)context_append_fact(
        context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
        doc->nodes[pending->type_node].raw_span,
        text_from_span(doc, doc->nodes[pending->type_node].raw_span));
  }
  if (!shapes_ok) {
    valid = false;
    if (shape_limit_hit) {
      unsupported_seen = true;
      (void)context_append_fact(
          context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE, envelope_span,
          text_from_span(doc, envelope_span));
    } else {
      /* A parser-complete CST cannot take this route. Keep the defensive
       * barrier for a caller-supplied malformed tree without publishing a
       * diagnostic whose source syntax is not parser-reachable. */
      unsupported_seen = true;
      (void)context_append_fact(
          context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE, envelope_span,
          text_from_span(doc, envelope_span));
    }
  }
  for (size_t ordinal = 0u; ordinal < shape_count; ordinal += 1u) {
    const frontend_generic_argument_shape *shape = &shapes[ordinal];
    generic_argument_build *argument = &built[ordinal];
    (void)memset(argument, 0, sizeof(*argument));
    argument->value.module_index = (uint32_t)pending->module_index;
    argument->value.owner_application = W_SEED_FRONTEND_NONE;
    argument->value.source_ordinal = (uint32_t)ordinal;
    argument->value.span = shape->span;
    argument->value.label = shape->label;
    argument->value.parameter_index = W_SEED_FRONTEND_NONE;
    argument->value.parameter_ordinal = W_SEED_FRONTEND_NONE;
    argument->value.kind = shape->has_label
                               ? W_SEED_FRONTEND_GENERIC_ARGUMENT_VALUE
                               : W_SEED_FRONTEND_GENERIC_ARGUMENT_TYPE;
    argument->value.type_index = W_SEED_FRONTEND_NONE;
    argument->value.const_value_index = W_SEED_FRONTEND_NONE;
    argument->value.typed_const_expression_index = W_SEED_FRONTEND_NONE;
    argument->value.binding_status =
        W_SEED_FRONTEND_GENERIC_BINDING_INVALID;
    argument->type_simple = simple_type_unknown();
    size_t slot = SIZE_MAX;
    bool label_matches_type = false;
    size_t type_label_slot = SIZE_MAX;
    if (shape->has_label) {
      /* Named syntax is sticky even when its label is malformed.  A later
       * positional argument must not recover the binding after this point. */
      named_seen = true;
      for (size_t candidate = 0u; candidate < schema_count; candidate += 1u) {
        if (schema[candidate].parameter.kind ==
                W_SEED_FRONTEND_GENERIC_KIND_TYPE &&
            text_equal_text(schema[candidate].parameter.internal_name,
                            shape->label)) {
          label_matches_type = true;
          type_label_slot = candidate;
        }
        if (schema[candidate].parameter.kind !=
                W_SEED_FRONTEND_GENERIC_KIND_TYPE &&
            (text_equal_text(schema[candidate].parameter.external_label,
                             shape->label) ||
             (schema[candidate].parameter.label_kind ==
                  W_SEED_FRONTEND_LABEL_OPTIONAL &&
              text_equal_text(schema[candidate].parameter.internal_name,
                              shape->label)))) {
          slot = candidate;
          break;
        }
      }
      if (slot == SIZE_MAX) {
        valid = false;
        if (label_matches_type) {
          if (type_label_slot == SIZE_MAX) return false;
          (void)append_generic0003_diagnostic(
              context, shape->span, shape->label,
              (w_seed_frontend_text){"type", sizeof("type") - 1u},
              schema[type_label_slot].parameter.internal_name,
              (int64_t)ordinal,
              (w_seed_frontend_text){"type-parameter-must-be-positional",
                                     sizeof("type-parameter-must-be-positional") -
                                         1u},
              pending->module_index, schema[type_label_slot].parameter.span);
        } else {
          (void)append_contract0001_diagnostic(
              context, shape->span, available_slots, available_slot_count,
              head_name, pending->module_index, application_head_span,
              shape->label);
        }
        slot = SIZE_MAX;
      } else if (slot < next_slot || bound[slot]) {
        valid = false;
        w_seed_frontend_text slot_order[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
        const size_t slot_order_count = diagnostic_generic_slot_order(
            doc, first_direct_kind(doc, struct_node,
                                   W_SEED_CST_GENERIC_PARAMETERS),
            slot_order, sizeof(slot_order) / sizeof(slot_order[0]));
        if (slot_order_count == 0u) return false;
        (void)append_contract0004_diagnostic(
            context, shape->span, head_name, pending->module_index,
            application_head_span, schema[slot].parameter.internal_name,
            pending->module_index, schema[slot].parameter.span, slot_order,
            slot_order_count,
            (w_seed_frontend_text){"duplicate", sizeof("duplicate") - 1u});
        slot = SIZE_MAX;
      } else if (slot != next_slot) {
        valid = false;
        const w_seed_frontend_text expected_slot =
            next_slot < schema_count
                ? schema[next_slot].parameter.internal_name
                : (w_seed_frontend_text){"extra", sizeof("extra") - 1u};
        const w_seed_frontend_text expected_kind =
            next_slot < schema_count &&
                    schema[next_slot].parameter.kind ==
                        W_SEED_FRONTEND_GENERIC_KIND_TYPE
                ? (w_seed_frontend_text){"type", sizeof("type") - 1u}
                : (w_seed_frontend_text){"value", sizeof("value") - 1u};
        if (expected_slot.length == 0u) return false;
        const size_t expected_slot_index =
            next_slot < schema_count ? next_slot : SIZE_MAX;
        (void)append_generic0003_diagnostic(
            context, shape->span, shape->label,
            expected_kind,
            expected_slot, (int64_t)ordinal,
            (w_seed_frontend_text){"named-argument-out-of-order",
                                   sizeof("named-argument-out-of-order") - 1u},
            pending->module_index,
            expected_slot_index == SIZE_MAX
                ? shape->span
                : schema[expected_slot_index].parameter.span);
        slot = SIZE_MAX;
      } else {
        /* The slot is still eligible after the order/label checks above. */
      }
    } else {
      if (named_seen && next_slot < schema_count) {
        valid = false;
        const bool has_expected_slot = next_slot < schema_count;
        const w_seed_frontend_text parameter =
            has_expected_slot
                ? schema[next_slot].parameter.internal_name
                : (w_seed_frontend_text){"extra", sizeof("extra") - 1u};
        const w_seed_frontend_text kind =
            has_expected_slot &&
                    schema[next_slot].parameter.kind ==
                        W_SEED_FRONTEND_GENERIC_KIND_TYPE
                ? (w_seed_frontend_text){"type", sizeof("type") - 1u}
                : (w_seed_frontend_text){"value", sizeof("value") - 1u};
        const w_seed_span label_span =
            has_expected_slot ? schema[next_slot].parameter.span : shape->span;
        (void)append_generic0003_diagnostic(
            context, shape->span, (w_seed_frontend_text){"_", 1u}, kind,
            parameter, (int64_t)ordinal,
            (w_seed_frontend_text){"positional-after-named",
                                   sizeof("positional-after-named") - 1u},
            pending->module_index, label_span);
      } else if (next_slot >= schema_count) {
        valid = false;
        (void)append_generic0003_diagnostic(
            context, shape->span, (w_seed_frontend_text){"_", 1u},
            (w_seed_frontend_text){"value", sizeof("value") - 1u},
            (w_seed_frontend_text){"extra", sizeof("extra") - 1u},
            (int64_t)ordinal,
            (w_seed_frontend_text){"extra-argument",
                                   sizeof("extra-argument") - 1u},
            pending->module_index, shape->span);
      } else if (schema[next_slot].parameter.kind ==
                     W_SEED_FRONTEND_GENERIC_KIND_VALUE &&
                 schema[next_slot].parameter.label_kind !=
                     W_SEED_FRONTEND_LABEL_OPTIONAL) {
        valid = false;
        (void)append_generic0003_diagnostic(
            context, shape->span, (w_seed_frontend_text){"_", 1u},
            (w_seed_frontend_text){"value", sizeof("value") - 1u},
            schema[next_slot].parameter.internal_name, (int64_t)ordinal,
            (w_seed_frontend_text){"required-label-omitted",
                                   sizeof("required-label-omitted") - 1u},
            pending->module_index, schema[next_slot].parameter.span);
      } else {
        slot = next_slot;
      }
    }
    if (slot == SIZE_MAX || slot >= schema_count) continue;
    argument->value.parameter_index = schema[slot].parameter_index;
    argument->value.parameter_ordinal = (uint32_t)slot;
    argument->value.kind = schema[slot].parameter.kind ==
                                   W_SEED_FRONTEND_GENERIC_KIND_VALUE
                               ? W_SEED_FRONTEND_GENERIC_ARGUMENT_VALUE
                               : W_SEED_FRONTEND_GENERIC_ARGUMENT_TYPE;
    if (schema[slot].parameter.kind == W_SEED_FRONTEND_GENERIC_KIND_TYPE) {
      if (shape->has_label) {
        valid = false;
        (void)append_generic0003_diagnostic(
            context, shape->span, shape->label,
            (w_seed_frontend_text){"type", sizeof("type") - 1u},
            schema[slot].parameter.internal_name, (int64_t)ordinal,
            (w_seed_frontend_text){"type-parameter-must-be-positional",
                                   sizeof("type-parameter-must-be-positional") -
                                       1u},
            pending->module_index, schema[slot].parameter.span);
      } else if (generic_type_argument_is_literal(doc, shape->value_span)) {
        valid = false;
        w_seed_frontend_text actual_kind;
        if (!diagnostic_generic_actual_kind(doc, shape->value_span,
                                             &actual_kind)) {
          context->module_index = saved_module;
          return false;
        }
        (void)append_contract0002_diagnostic(
            context, shape->value_span, actual_kind,
            (w_seed_frontend_text){"type", sizeof("type") - 1u}, head_name,
            pending->module_index, application_head_span,
            schema[slot].parameter.internal_name, pending->module_index,
            schema[slot].parameter.span);
      } else {
        uint32_t type_node = W_SEED_CST_NONE;
        if (!find_type_node_for_span(doc, shape->value_span, &type_node) ||
            !normalize_type_tree(context, type_node, &argument->value.type_index)) {
          valid = false;
          (void)append_contract0002_diagnostic(
              context, shape->value_span,
              (w_seed_frontend_text){"value:unresolved",
                                     sizeof("value:unresolved") - 1u},
              (w_seed_frontend_text){"type", sizeof("type") - 1u}, head_name,
              pending->module_index, application_head_span,
              schema[slot].parameter.internal_name, pending->module_index,
              schema[slot].parameter.span);
        } else {
          argument->type_simple = contextual_type_from_span(
              context, doc, shape->value_span);
          argument->value.binding_status =
              W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE;
        }
      }
    } else {
      frontend_simple_type expected = schema[slot].domain_simple;
      uint32_t expected_type_index = schema[slot].parameter.domain_type;
      if (schema[slot].parameter.domain_kind ==
          W_SEED_FRONTEND_GENERIC_DOMAIN_DEPENDENT) {
        const uint32_t dependent =
            schema[slot].parameter.dependent_type_parameter_ordinal;
        if (dependent >= ordinal ||
            built[dependent].value.binding_status !=
                W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE ||
            built[dependent].value.kind !=
                W_SEED_FRONTEND_GENERIC_ARGUMENT_TYPE) {
          valid = false;
          w_seed_frontend_text actual_kind;
          if (!diagnostic_generic_actual_kind(doc, shape->value_span,
                                               &actual_kind)) {
            context->module_index = saved_module;
            return false;
          }
          (void)append_contract0002_diagnostic(
              context, shape->value_span, actual_kind,
              (w_seed_frontend_text){"value:dependent",
                                     sizeof("value:dependent") - 1u},
              head_name, pending->module_index, application_head_span,
              schema[slot].parameter.internal_name, pending->module_index,
              schema[slot].parameter.span);
          (void)append_invalid_const_value(context, shape->value_span,
                                           W_SEED_FRONTEND_NONE,
                                           &argument->value.const_value_index);
          bound[slot] = true;
          if (slot == next_slot) next_slot += 1u;
          continue;
        }
        expected_type_index = built[dependent].value.type_index;
        expected = built[dependent].type_simple;
        const frontend_static_argument_support dependent_support =
            static_argument_representable(context, expected);
        if (dependent_support != FRONTEND_STATIC_ARGUMENT_IMMEDIATE) {
          valid = false;
          (void)append_invalid_const_value(context, shape->value_span,
                                           expected_type_index,
                                           &argument->value.const_value_index);
          if (dependent_support == FRONTEND_STATIC_ARGUMENT_UNSUPPORTED) {
            argument->value.binding_status =
                W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED;
            unsupported_seen = true;
            (void)context_append_fact(
                context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
                shape->value_span, text_from_span(doc, shape->value_span));
          } else {
            w_seed_frontend_text actual_kind;
            w_seed_frontend_text expected_kind;
            if (!diagnostic_generic_actual_kind(doc, shape->value_span,
                                                 &actual_kind) ||
                !diagnostic_value_kind_from_simple(expected, true,
                                                   &expected_kind)) {
              context->module_index = saved_module;
              return false;
            }
            (void)append_contract0002_diagnostic(
                context, shape->value_span, actual_kind, expected_kind,
                head_name, pending->module_index, application_head_span,
                schema[slot].parameter.internal_name, pending->module_index,
                schema[slot].parameter.span);
          }
          bound[slot] = true;
          if (slot == next_slot) next_slot += 1u;
          continue;
        }
      }
      if (schema[slot].parameter.domain_kind !=
          W_SEED_FRONTEND_GENERIC_DOMAIN_CONCRETE &&
          schema[slot].parameter.domain_kind !=
          W_SEED_FRONTEND_GENERIC_DOMAIN_DEPENDENT) {
        valid = false;
        w_seed_frontend_text actual_kind;
        w_seed_frontend_text expected_kind;
        if (!diagnostic_generic_actual_kind(doc, shape->value_span,
                                             &actual_kind) ||
            !diagnostic_value_kind_from_simple(expected, true,
                                               &expected_kind)) {
          context->module_index = saved_module;
          return false;
        }
        (void)append_contract0002_diagnostic(
            context, shape->value_span, actual_kind, expected_kind, head_name,
            pending->module_index, application_head_span,
            schema[slot].parameter.internal_name, pending->module_index,
            schema[slot].parameter.span);
      }
      const frontend_simple_type list_element =
          expected.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST
              ? static_list_element_type(context, expected)
              : simple_type_unknown();
      const frontend_static_argument_support expected_support =
          static_argument_representable(context, expected);
      if (generic_value_is_parenthesized(doc, shape->value_span)) {
        /* D3 calculated generic values are deliberately closed scalar
         * expressions.  Keep the frontend responsible only for typing and
         * recording the relation; ConstIR owns evaluation. */
        if (!generic_typed_scalar_expected(expected)) {
          (void)context_append_fact(
              context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
              shape->value_span, text_from_span(doc, shape->value_span));
          argument->value.binding_status =
              W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED;
          unsupported_seen = true;
          valid = false;
          bound[slot] = true;
          if (slot == next_slot) next_slot += 1u;
          continue;
        }
        const uint32_t saved_function_index = context->function_index;
        const w_seed_cst_node *saved_function_node = context->function_node;
        const bool saved_const_function = context->current_function_is_const;
        const bool saved_const_active = context->current_const_body_active;
        context->function_index = W_SEED_FRONTEND_NONE;
        context->function_node = NULL;
        context->current_function_is_const = false;
        context->current_const_body_active = false;
        uint32_t expression_index = W_SEED_FRONTEND_NONE;
        frontend_simple_type actual = simple_type_unknown();
        frontend_expr_value root;
        (void)memset(&root, 0, sizeof(root));
        const bool normalized = normalize_expression_span(
            context, shape->value_span, expected, &expression_index, &actual,
            &root);
        context->function_index = saved_function_index;
        context->function_node = saved_function_node;
        context->current_function_is_const = saved_const_function;
        context->current_const_body_active = saved_const_active;
        w_seed_frontend_import_target_kind imported_kind =
            W_SEED_FRONTEND_IMPORT_UNRESOLVED;
        uint32_t imported_index = W_SEED_FRONTEND_NONE;
        w_seed_frontend_text imported_target = {NULL, 0};
        w_seed_frontend_text unresolved_name = root.name;
        bool unresolved_local =
            context->count.const_declarations != 0u && normalized &&
            root.kind == W_SEED_FRONTEND_EXPR_IDENTIFIER && root.has_name &&
            !root.supported &&
            !module_const_name_is_duplicate(context, root.name) &&
            !module_const_name_is_untyped(context, root.name) &&
            !imported_target_for_name(context, root.name, &imported_kind,
                                      &imported_index, &imported_target);
        if (!unresolved_local && context->count.const_declarations != 0u &&
            normalized && root.kind == W_SEED_FRONTEND_EXPR_PARENTHESIS &&
            !root.supported) {
          unresolved_local = unresolved_parenthesized_identifier(
              context, shape->value_span, &unresolved_name);
          if (unresolved_local &&
              (module_const_name_is_duplicate(context, unresolved_name) ||
               module_const_name_is_untyped(context, unresolved_name)))
            unresolved_local = false;
        }
        if (unresolved_local) {
          semantic_error = true;
          valid = false;
          (void)append_sem0001_diagnostic(context, shape->value_span,
                                          unresolved_name, expected.spelling);
          argument->value.binding_status =
              W_SEED_FRONTEND_GENERIC_BINDING_INVALID;
          bound[slot] = true;
          if (slot == next_slot) next_slot += 1u;
          continue;
        }
        if (!normalized || !root.supported ||
            !frontend_type_equal(context, actual, expected)) {
          (void)context_append_fact(
              context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
              shape->value_span, text_from_span(doc, shape->value_span));
          argument->value.binding_status =
              W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED;
          unsupported_seen = true;
          valid = false;
          bound[slot] = true;
          if (slot == next_slot) next_slot += 1u;
          continue;
        }
        /* A typed record is executable only when this application has no
         * earlier semantic or unsupported relation.  Keep the malformed
         * application in its existing INVALID/UNSUPPORTED state; do not
         * publish an apparently executable pending relation for it. */
        if (!valid || unsupported_seen ||
            context->count.diagnostics > diagnostics_before) {
          bound[slot] = true;
          if (slot == next_slot) next_slot += 1u;
          continue;
        }
        w_seed_frontend_typed_const_expression typed;
        (void)memset(&typed, 0, sizeof(typed));
        typed.module_index = (uint32_t)pending->module_index;
        if (context->count.generic_applications >= (size_t)UINT32_MAX)
          return false;
        typed.owner_application = (uint32_t)context->count.generic_applications;
        typed.argument_ordinal = (uint32_t)ordinal;
        typed.expression_index = expression_index;
        typed.span = shape->value_span;
        typed.expected_type = expected_type_index;
        /* The effective domain is the application slot's resolved type.  The
         * expression tree can carry an equivalent canonical type record (for
         * example, a parameter and a domain can each spell i64), so retain
         * the domain index here and let downstream validation compare the
         * semantic type shape. */
        typed.effective_type = expected_type_index;
        uint32_t typed_index = W_SEED_FRONTEND_NONE;
        if (!context_append_typed_const_expression(context, typed,
                                                   &typed_index)) {
          context->module_index = saved_module;
          return false;
        }
        argument->value.typed_const_expression_index = typed_index;
        argument->value.binding_status =
            W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST;
        typed_const_expr_seen = true;
        bound[slot] = true;
        if (slot == next_slot) next_slot += 1u;
        continue;
      }
      if (expected.kind == W_SEED_FRONTEND_TYPE_BYTES ||
          list_element.kind == W_SEED_FRONTEND_TYPE_BYTES ||
          list_element.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST ||
          expected_support == FRONTEND_STATIC_ARGUMENT_UNSUPPORTED) {
        /* The declaration domain is recognized, but this seed has no
         * caller-owned BYTES payload and does not recurse nested StaticList.
         * Keep the application record for audit with an UNSUPPORTED status;
         * do not emit a contract mismatch diagnostic. */
        (void)append_invalid_const_value(context, shape->value_span,
                                         expected_type_index,
                                         &argument->value.const_value_index);
        (void)context_append_fact(
            context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
            shape->value_span, text_from_span(doc, shape->value_span));
        argument->value.binding_status =
            W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED;
        unsupported_seen = true;
        valid = false;
        bound[slot] = true;
        if (slot == next_slot) next_slot += 1u;
        continue;
      }
      const size_t facts_before_const = context->count.facts;
      const bool const_valid = const_value_for_span(
          context, doc, shape->value_span, expected, expected_type_index,
          &argument->value.const_value_index);
      if (!const_valid) {
        valid = false;
        const bool const_added_unsupported_fact =
            context->count.facts > facts_before_const;
        const bool shape_is_unsupported =
            const_argument_shape_unsupported(doc, shape->value_span, expected);
        if (shape_is_unsupported ||
            (const_added_unsupported_fact &&
             expected.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST)) {
          argument->value.binding_status =
              W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED;
          unsupported_seen = true;
          (void)context_append_fact(
              context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
              shape->value_span, text_from_span(doc, shape->value_span));
          const w_seed_span trimmed_value = trim_span(doc, shape->value_span);
          if (shape_is_unsupported && trimmed_value.start_byte <
                                          trimmed_value.end_byte &&
              doc->source->bytes.data[trimmed_value.start_byte] == '(')
            typed_const_expr_seen = true;
        } else if (expected.kind == W_SEED_FRONTEND_TYPE_ENUM ||
                   expected.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST) {
          /* The seed has no canonical active diagnostic for an unknown
           * contextual enum member.  Preserve the invalid ConstValue and a
           * deterministic unsupported fact without inventing a code. */
          (void)context_append_fact(
              context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
              shape->value_span, text_from_span(doc, shape->value_span));
          semantic_error = true;
        } else {
          frontend_token integer_tokens[2];
          size_t integer_token_count = 0u;
          const bool integer_literal_shape =
              expected.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
              const_tokens_for_span(doc, shape->value_span, integer_tokens,
                                    sizeof(integer_tokens) /
                                        sizeof(integer_tokens[0]),
                                    &integer_token_count) &&
              integer_token_count == 1u &&
              integer_tokens[0].kind == W_SEED_CST_NUMBER;
          if (integer_literal_shape) {
            const frontend_simple_type literal_type = literal_simple_type(
                doc, integer_tokens[0].span, integer_tokens[0].kind);
            (void)append_type0122_diagnostic(
                context, shape->value_span, literal_type, expected,
                (w_seed_frontend_text){"integer is not exactly representable",
                                       sizeof("integer is not exactly representable") - 1u});
          } else {
            w_seed_frontend_text actual_kind;
            w_seed_frontend_text expected_kind;
            if (!diagnostic_generic_actual_kind(doc, shape->value_span,
                                                 &actual_kind) ||
                !diagnostic_value_kind_from_simple(expected, true,
                                                   &expected_kind)) {
              context->module_index = saved_module;
              return false;
            }
            (void)append_contract0002_diagnostic(
                context, shape->value_span, actual_kind, expected_kind,
                head_name, pending->module_index, application_head_span,
                schema[slot].parameter.internal_name, pending->module_index,
                schema[slot].parameter.span);
          }
        }
      } else {
        argument->value.binding_status =
            W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE;
      }
    }
    bound[slot] = true;
    if (slot == next_slot) next_slot += 1u;
  }
  for (size_t slot = 0u; slot < schema_count; slot += 1u) {
    if (bound[slot]) continue;
    valid = false;
    (void)append_generic0002_diagnostic(
        context, schema[slot].parameter.span,
        schema[slot].parameter.internal_name, pending->module_index,
        schema[slot].parameter.span, pending->module_index, envelope_span);
  }
  if (schema_count == 0u && shape_count == 0u) valid = false;
  w_seed_frontend_generic_application application;
  (void)memset(&application, 0, sizeof(application));
  application.module_index = (uint32_t)pending->module_index;
  application.owner_type = pending->owner_type;
  application.head_struct = struct_index;
  application.head_name = head_name;
  application.span = doc->nodes[pending->type_node].raw_span;
  application.envelope_span = envelope_span;
  application.first_argument = (uint32_t)context->count.generic_arguments;
  application.argument_count = (uint32_t)shape_count;
  application.requires_const_evaluation = typed_const_expr_seen;
  for (size_t schema_index = 0u; schema_index < schema_count;
       schema_index += 1u) {
    if (schema[schema_index].parameter.refinement_kind ==
        W_SEED_FRONTEND_GENERIC_REFINEMENT_PREDICATE) {
      application.requires_const_evaluation = true;
      break;
    }
  }
  if (schema_invalid) {
    for (size_t ordinal = 0u; ordinal < shape_count; ordinal += 1u) {
      if (built[ordinal].value.binding_status ==
          W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE) {
        built[ordinal].value.binding_status =
            W_SEED_FRONTEND_GENERIC_BINDING_INVALID;
      }
    }
  }
  /* Diagnostics emitted by slot/kind/domain checks have precedence over an
   * unsupported form in the same application. Schema diagnostics were
   * emitted during normalization and are covered by schema_invalid above. */
  if (context->count.diagnostics > diagnostics_before)
    semantic_error = true;
  application.binding_status =
      semantic_error ? W_SEED_FRONTEND_GENERIC_BINDING_INVALID
                     : unsupported_seen
                           ? W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED
                           : (valid && typed_const_expr_seen)
                                 ? W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST
                           : valid
                                 ? W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE
                                 : W_SEED_FRONTEND_GENERIC_BINDING_INVALID;
  uint32_t application_index = W_SEED_FRONTEND_NONE;
  if (!context_append_generic_application(context, application,
                                           &application_index)) {
    context->module_index = saved_module;
    return false;
  }
  if (context->emit && context->output != NULL &&
      context->output->typed_const_expressions != NULL) {
    for (size_t ordinal = 0u; ordinal < shape_count; ordinal += 1u) {
      const uint32_t typed_index =
          built[ordinal].value.typed_const_expression_index;
      if (typed_index != W_SEED_FRONTEND_NONE &&
          typed_index < context->count.typed_const_expressions) {
        context->output->typed_const_expressions[typed_index]
            .owner_application = application_index;
      }
    }
  }
  for (size_t ordinal = 0u; ordinal < shape_count; ordinal += 1u) {
    built[ordinal].value.owner_application = application_index;
    uint32_t argument_index = W_SEED_FRONTEND_NONE;
    if (!context_append_generic_argument(context, built[ordinal].value,
                                         &argument_index)) {
      context->module_index = saved_module;
      return false;
    }
  }
  if (context->emit && context->output != NULL &&
      pending->owner_type < context->output->type_capacity) {
    context->output->types[pending->owner_type].generic_application_index =
        application_index;
  }
  context->module_index = saved_module;
  return true;
}

static bool resolve_pending_generic_applications(frontend_context *context) {
  if (context == NULL) return false;
  const size_t saved_module = context->module_index;
  size_t index = 0u;
  while (index < context->pending_application_count) {
    if (!resolve_one_pending_generic_application(
            context, &context->pending_applications[index])) {
      context->module_index = saved_module;
      return false;
    }
    index += 1u;
  }
  context->module_index = saved_module;
  return true;
}

static bool add_u32(size_t value, uint32_t *result) {
  if (value >= (size_t)UINT32_MAX) return false;
  *result = (uint32_t)value;
  return true;
}

static size_t receipt_decimal_length(size_t value) {
  size_t length = 1;
  while (value >= 10u) {
    value /= 10u;
    length += 1;
  }
  return length;
}

static bool receipt_size_add(frontend_context *context, size_t amount) {
  if (context == NULL || context->receipt_overflow ||
      amount > SIZE_MAX - context->receipt_size) {
    if (context != NULL) context->receipt_overflow = true;
    return false;
  }
  context->receipt_size += amount;
  return true;
}

static bool receipt_size_literal(frontend_context *context, const char *text) {
  return text != NULL && receipt_size_add(context, strlen(text));
}

static bool receipt_size_size(frontend_context *context, size_t value) {
  return receipt_size_add(context, receipt_decimal_length(value));
}

static bool receipt_size_text(frontend_context *context,
                              w_seed_frontend_text text) {
  if (text.data == NULL && text.length != 0) {
    return receipt_size_literal(context, "0:<invalid>");
  }
  if (!receipt_size_size(context, text.length) ||
      !receipt_size_literal(context, ":")) {
    return false;
  }
  if (text.length > (SIZE_MAX - context->receipt_size) / 2u) {
    context->receipt_overflow = true;
    return false;
  }
  context->receipt_size += text.length * 2u;
  return true;
}

static bool receipt_size_span(frontend_context *context, w_seed_span span) {
  return receipt_size_size(context, span.start_byte) &&
         receipt_size_literal(context, ":") &&
         receipt_size_size(context, span.end_byte);
}

static bool receipt_size_source_records(frontend_context *context) {
  if (context == NULL) return false;
  if (!receipt_size_literal(context, "schema=") ||
      !receipt_size_literal(context, W_SEED_FRONTEND_SCHEMA_VERSION) ||
      !receipt_size_literal(context, "\n")) {
    return false;
  }
  for (size_t index = 0; index < context->input.document_count; index += 1) {
    const w_seed_frontend_document *doc = &context->input.documents[index];
    if (!receipt_size_literal(context, "source=") ||
        !receipt_size_size(context, index) ||
        !receipt_size_literal(context, "|") ||
        !receipt_size_text(context, doc->logical_source_id) ||
        !receipt_size_literal(context, "|") ||
        !receipt_size_text(context, document_module_name(doc)) ||
        !receipt_size_literal(context, "|local=") ||
        !receipt_size_text(context, document_local_module_name(doc)) ||
        !receipt_size_literal(context, "|sha256:") ||
        !receipt_size_add(context, 64u) ||
        !receipt_size_literal(context, "\n")) {
      return false;
    }
  }
  return true;
}

static bool receipt_size_external_records(frontend_context *context) {
  if (context == NULL) return false;
  for (size_t module_index = 0;
       module_index < context->input.external_module_count; module_index += 1) {
    const w_seed_frontend_external_module *module =
        &context->input.external_modules[module_index];
    if (!receipt_size_literal(context, "external-module=") ||
        !receipt_size_text(context, module->module_id) ||
        !receipt_size_literal(context, "\n")) {
      return false;
    }
    for (size_t symbol_index = 0; symbol_index < module->symbol_count;
         symbol_index += 1) {
      const w_seed_frontend_external_symbol *symbol =
          &module->symbols[symbol_index];
      if (!receipt_size_literal(context, "external-symbol=") ||
          !receipt_size_text(context, symbol->name) ||
          !receipt_size_literal(context, "|kind=") ||
          !receipt_size_size(context, (size_t)symbol->kind) ||
          !receipt_size_literal(context, "|exported=") ||
          !receipt_size_size(context, symbol->exported ? 1u : 0u) ||
          !receipt_size_literal(context, "|const=") ||
          !receipt_size_size(context, symbol->is_const ? 1u : 0u) ||
          !receipt_size_literal(context, "|return=") ||
          !receipt_size_text(context, symbol->return_type) ||
          !receipt_size_literal(context, "\n")) {
        return false;
      }
      for (size_t parameter_index = 0;
           parameter_index < symbol->parameter_count; parameter_index += 1) {
        const w_seed_frontend_external_parameter *parameter =
            &symbol->parameters[parameter_index];
        if (!receipt_size_literal(context, "external-parameter=") ||
            !receipt_size_text(context, parameter->name) ||
            !receipt_size_literal(context, "|label=") ||
            !receipt_size_size(context, (size_t)parameter->label_kind) ||
            !receipt_size_literal(context, "|type=") ||
            !receipt_size_text(context, parameter->type) ||
            !receipt_size_literal(context, "\n")) {
          return false;
        }
      }
    }
  }
  return true;
}

static bool receipt_size_host_records(frontend_context *context) {
  if (context == NULL) return false;
  const w_seed_frontend_host_prelude *prelude = context->input.host_scope;
  if (prelude == NULL) return true;
  if (!receipt_size_literal(context, "host-scope=") ||
      !receipt_size_text(context, prelude->profile) ||
      !receipt_size_literal(context, "\n")) {
    return false;
  }
  for (size_t symbol_index = 0u; symbol_index < prelude->symbol_count;
       symbol_index += 1u) {
    const w_seed_frontend_host_prelude_symbol *symbol =
        &prelude->symbols[symbol_index];
    if (!receipt_size_literal(context, "host-symbol=") ||
        !receipt_size_size(context, symbol_index) ||
        !receipt_size_literal(context, "|") ||
        !receipt_size_text(context, symbol->name) ||
        !receipt_size_literal(context, "|kind=") ||
        !receipt_size_size(context, (size_t)symbol->kind) ||
        !receipt_size_literal(context, "|const=") ||
        !receipt_size_size(context, symbol->is_const ? 1u : 0u) ||
        !receipt_size_literal(context, "|return=") ||
        !receipt_size_text(context, symbol->return_type) ||
        !receipt_size_literal(context, "|requirements=" ) ||
        !receipt_size_size(context, symbol->requirement_count) ||
        !receipt_size_literal(context, "\n")) {
      return false;
    }
    for (size_t requirement_index = 0u;
         requirement_index < symbol->requirement_count; requirement_index += 1u) {
      const w_seed_frontend_host_requirement *requirement =
          &symbol->requirements[requirement_index];
      if (!receipt_size_literal(context, "host-requirement=") ||
          !receipt_size_size(context, symbol_index) ||
          !receipt_size_literal(context, "|") ||
          !receipt_size_size(context, requirement_index) ||
          !receipt_size_literal(context, "|") ||
          !receipt_size_text(context, requirement->name) ||
          !receipt_size_literal(context, "\n")) {
        return false;
      }
    }
    for (size_t parameter_index = 0u;
         parameter_index < symbol->parameter_count; parameter_index += 1u) {
      const w_seed_frontend_external_parameter *parameter =
          &symbol->parameters[parameter_index];
      if (!receipt_size_literal(context, "host-parameter=") ||
          !receipt_size_size(context, symbol_index) ||
          !receipt_size_literal(context, "|") ||
          !receipt_size_size(context, parameter_index) ||
          !receipt_size_literal(context, "|") ||
          !receipt_size_text(context, parameter->name) ||
          !receipt_size_literal(context, "|label=") ||
          !receipt_size_size(context, (size_t)parameter->label_kind) ||
          !receipt_size_literal(context, "|type=") ||
          !receipt_size_text(context, parameter->type) ||
          !receipt_size_literal(context, "\n")) {
        return false;
      }
    }
  }
  return true;
}

static bool receipt_size_module(frontend_context *context,
                               const w_seed_frontend_module *module) {
  return receipt_size_literal(context, "module=") &&
         receipt_size_text(context, module->module_id) &&
         receipt_size_literal(context, "|local=") &&
         receipt_size_text(context, module->local_module_name) &&
         receipt_size_literal(context, "|source=") &&
         receipt_size_text(context, module->source_id) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_const_declaration(
    frontend_context *context,
    const w_seed_frontend_const_declaration *value) {
  const bool sized = receipt_size_literal(context, "const-declaration=") &&
         receipt_size_size(context, value->module_index) &&
         receipt_size_literal(context, "|") &&
         receipt_size_text(context, value->name) &&
         receipt_size_literal(context, "|exported=") &&
         receipt_size_size(context, value->exported ? 1u : 0u) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, value->span) &&
         receipt_size_literal(context, "|declared=") &&
         receipt_size_size(context, value->declared_type) &&
         receipt_size_literal(context, "|effective=") &&
         receipt_size_size(context, value->effective_type) &&
         receipt_size_literal(context, "|explicit=") &&
         receipt_size_size(context, value->has_explicit_type ? 1u : 0u) &&
         receipt_size_literal(context, "\n");
  return sized;
}

static bool receipt_size_import(frontend_context *context,
                                const w_seed_frontend_import *import_value) {
  return receipt_size_literal(context, "import=") &&
         receipt_size_size(context, import_value->module_index) &&
         receipt_size_literal(context, "|") &&
         receipt_size_text(context, import_value->path) &&
         receipt_size_literal(context, "|ordinal=") &&
         receipt_size_size(context, import_value->direct_import_ordinal) &&
         receipt_size_literal(context, "|target-kind=") &&
         receipt_size_size(context, (size_t)import_value->target_kind) &&
         receipt_size_literal(context, "|target-index=") &&
         receipt_size_size(context, import_value->target_index) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_symbol(frontend_context *context,
                               const w_seed_frontend_symbol *symbol) {
  return receipt_size_literal(context, "symbol=") &&
         receipt_size_size(context, symbol->module_index) &&
         receipt_size_literal(context, "|") &&
         receipt_size_text(context, symbol->name) &&
         receipt_size_literal(context, "|") &&
         receipt_size_size(context, (size_t)symbol->kind) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_type(frontend_context *context,
                              const w_seed_frontend_type *type) {
  return receipt_size_literal(context, "type=") &&
         receipt_size_size(context, (size_t)type->kind) &&
         receipt_size_literal(context, "|") &&
         receipt_size_text(context, type->spelling) &&
         receipt_size_literal(context, "|enum-base=") &&
         receipt_size_size(context, type->enum_base_index) &&
         receipt_size_literal(context, "|subset-first=") &&
         receipt_size_size(context, type->first_subset_member) &&
         receipt_size_literal(context, "|subset-count=") &&
         receipt_size_size(context, type->subset_member_count) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_generic_application(
    frontend_context *context,
    const w_seed_frontend_generic_application *value) {
  return receipt_size_literal(context, "generic-application=") &&
         receipt_size_size(context, value->module_index) &&
         receipt_size_literal(context, "|owner-type=") &&
         receipt_size_size(context, value->owner_type) &&
         receipt_size_literal(context, "|head=") &&
         receipt_size_size(context, value->head_struct) &&
         receipt_size_literal(context, "|") &&
         receipt_size_text(context, value->head_name) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, value->span) &&
         receipt_size_literal(context, "|envelope=") &&
         receipt_size_span(context, value->envelope_span) &&
         receipt_size_literal(context, "|first-argument=") &&
         receipt_size_size(context, value->first_argument) &&
         receipt_size_literal(context, "|argument-count=") &&
         receipt_size_size(context, value->argument_count) &&
         receipt_size_literal(context, "|binding=") &&
         receipt_size_size(context, (size_t)value->binding_status) &&
         receipt_size_literal(context, "|requires-const=") &&
         receipt_size_size(context, value->requires_const_evaluation ? 1u : 0u) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_generic_argument(
    frontend_context *context, const w_seed_frontend_generic_argument *value) {
  return receipt_size_literal(context, "generic-argument=") &&
         receipt_size_size(context, value->module_index) &&
         receipt_size_literal(context, "|owner=") &&
         receipt_size_size(context, value->owner_application) &&
         receipt_size_literal(context, "|ordinal=") &&
         receipt_size_size(context, value->source_ordinal) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, value->span) &&
         receipt_size_literal(context, "|label=") &&
         receipt_size_text(context, value->label) &&
         receipt_size_literal(context, "|parameter=") &&
         receipt_size_size(context, value->parameter_index) &&
         receipt_size_literal(context, "|parameter-ordinal=") &&
         receipt_size_size(context, value->parameter_ordinal) &&
         receipt_size_literal(context, "|kind=") &&
         receipt_size_size(context, (size_t)value->kind) &&
         receipt_size_literal(context, "|type=") &&
         receipt_size_size(context, value->type_index) &&
         receipt_size_literal(context, "|const=") &&
         receipt_size_size(context, value->const_value_index) &&
         receipt_size_literal(context, "|typed-expr=") &&
         receipt_size_size(context, value->typed_const_expression_index) &&
         receipt_size_literal(context, "|binding=") &&
         receipt_size_size(context, (size_t)value->binding_status) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_typed_const_expression(
    frontend_context *context,
    const w_seed_frontend_typed_const_expression *value) {
  return receipt_size_literal(context, "typed-const-expression=") &&
         receipt_size_size(context, value->module_index) &&
         receipt_size_literal(context, "|owner=") &&
         receipt_size_size(context, value->owner_application) &&
         receipt_size_literal(context, "|argument=") &&
         receipt_size_size(context, value->argument_ordinal) &&
         receipt_size_literal(context, "|expression=") &&
         receipt_size_size(context, value->expression_index) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, value->span) &&
         receipt_size_literal(context, "|expected=") &&
         receipt_size_size(context, value->expected_type) &&
         receipt_size_literal(context, "|effective=") &&
         receipt_size_size(context, value->effective_type) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_const_value(
    frontend_context *context, const w_seed_frontend_const_value *value) {
  if (!receipt_size_literal(context, "const-value=") ||
      !receipt_size_size(context, (size_t)value->kind) ||
      !receipt_size_literal(context, "|type=") ||
      !receipt_size_size(context, value->type_index) ||
      !receipt_size_literal(context, "|span=") ||
      !receipt_size_span(context, value->span) ||
      !receipt_size_literal(context, "|bool=") ||
      !receipt_size_size(context, value->bool_value ? 1u : 0u) ||
      !receipt_size_literal(context, "|signed=") ||
      !receipt_size_size(context, value->integer_signed ? 1u : 0u) ||
      !receipt_size_literal(context, "|width=") ||
      !receipt_size_size(context, value->integer_bit_width) ||
      !receipt_size_literal(context, "|integer=") ||
      !receipt_size_add(context, (size_t)value->integer_byte_count * 2u) ||
      !receipt_size_literal(context, "|first-byte=") ||
      !receipt_size_size(context, value->first_byte) ||
      !receipt_size_literal(context, "|byte-count=") ||
      !receipt_size_size(context, value->byte_count) ||
      !receipt_size_literal(context, "|enum=") ||
      !receipt_size_size(context, value->enum_base_index) ||
      !receipt_size_literal(context, "|case=") ||
      !receipt_size_size(context, value->enum_case_index) ||
      !receipt_size_literal(context, "|first-element=") ||
      !receipt_size_size(context, value->first_element) ||
      !receipt_size_literal(context, "|element-count=") ||
      !receipt_size_size(context, value->element_count) ||
      !receipt_size_literal(context, "\n")) {
    return false;
  }
  return true;
}

static bool receipt_size_const_element(
    frontend_context *context, const w_seed_frontend_const_element *value) {
  return receipt_size_literal(context, "const-element=") &&
         receipt_size_size(context, value->owner_value) &&
         receipt_size_literal(context, "|ordinal=") &&
         receipt_size_size(context, value->ordinal) &&
         receipt_size_literal(context, "|value=") &&
         receipt_size_size(context, value->value_index) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, value->span) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_enum_subset_member(
    frontend_context *context,
    const w_seed_frontend_enum_subset_member *member) {
  return receipt_size_literal(context, "enum-subset-member=") &&
         receipt_size_size(context, member->owner_type) &&
         receipt_size_literal(context, "|enum=") &&
         receipt_size_size(context, member->enum_base_index) &&
         receipt_size_literal(context, "|case=") &&
         receipt_size_size(context, member->enum_case_index) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, member->source_span) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_function(frontend_context *context,
                                 const w_seed_frontend_function *function) {
  return receipt_size_literal(context, "signature=") &&
         receipt_size_text(context, function->name) &&
         receipt_size_literal(context, "|") &&
         receipt_size_size(context, function->parameter_count) &&
         receipt_size_literal(context, "|") &&
         receipt_size_size(context, function->return_type) &&
         receipt_size_literal(context, "|const=") &&
         receipt_size_size(context, function->is_const ? 1u : 0u) &&
         receipt_size_literal(context, "|const-body=") &&
         receipt_size_size(context, function->const_body_supported ? 1u : 0u) &&
         receipt_size_literal(context, "|async=") &&
         receipt_size_size(context, function->is_async ? 1u : 0u) &&
         receipt_size_literal(context, "|throws=") &&
         receipt_size_size(context, function->is_throws ? 1u : 0u) &&
         receipt_size_literal(context, "|unsafe=") &&
         receipt_size_size(context, function->is_unsafe ? 1u : 0u) &&
         receipt_size_literal(context, "|borrows=") &&
         receipt_size_size(context, function->has_borrow_clause ? 1u : 0u) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_call_identity(
    frontend_context *context, size_t call_index, size_t callee_index,
    w_seed_frontend_callee_kind kind,
    uint32_t host_symbol_index, uint32_t external_module_index,
    uint32_t external_symbol_index) {
  return receipt_size_literal(context, "callee=") &&
         receipt_size_size(context, call_index) &&
         receipt_size_literal(context, "|identifier=") &&
         receipt_size_size(context, callee_index) &&
         receipt_size_literal(context, "|kind=") &&
         receipt_size_size(context, (size_t)kind) &&
         receipt_size_literal(context, "|host=") &&
         receipt_size_size(context, host_symbol_index) &&
         receipt_size_literal(context, "|external=") &&
         receipt_size_size(context, external_module_index) &&
         receipt_size_literal(context, ":") &&
         receipt_size_size(context, external_symbol_index) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_enum_membership_case(
    frontend_context *context,
    const w_seed_frontend_enum_membership_case *value) {
  return receipt_size_literal(context, "enum-membership-case=") &&
         receipt_size_size(context, value->module_index) &&
         receipt_size_literal(context, "|owner=") &&
         receipt_size_size(context, value->owner_expression) &&
         receipt_size_literal(context, "|enum=") &&
         receipt_size_size(context, value->enum_base_index) &&
         receipt_size_literal(context, "|case=") &&
         receipt_size_size(context, value->enum_case_index) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, value->source_span) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_generic_parameter(
    frontend_context *context,
    const w_seed_frontend_generic_parameter *value) {
  return receipt_size_literal(context, "generic-parameter=") &&
         receipt_size_size(context, value->module_index) &&
         receipt_size_literal(context, "|owner-kind=") &&
         receipt_size_size(context, (size_t)value->owner_kind) &&
         receipt_size_literal(context, "|owner=") &&
         receipt_size_size(context, value->owner_index) &&
         receipt_size_literal(context, "|ordinal=") &&
         receipt_size_size(context, value->ordinal) &&
         receipt_size_literal(context, "|external-label=") &&
         receipt_size_text(context, value->external_label) &&
         receipt_size_literal(context, "|name=") &&
         receipt_size_text(context, value->internal_name) &&
         receipt_size_literal(context, "|label=") &&
         receipt_size_size(context, (size_t)value->label_kind) &&
         receipt_size_literal(context, "|kind=") &&
         receipt_size_size(context, (size_t)value->kind) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, value->span) &&
         receipt_size_literal(context, "|domain=") &&
         receipt_size_size(context, value->domain_type) &&
         receipt_size_literal(context, "|refinement=") &&
         receipt_size_size(context, (size_t)value->refinement_kind) &&
         receipt_size_literal(context, "|predicate=") &&
         receipt_size_size(context, value->predicate_function_index) &&
         receipt_size_literal(context, "|predicate-span=") &&
         receipt_size_span(context, value->predicate_span) &&
         receipt_size_literal(context, "|predicate-function-span=") &&
         receipt_size_span(context, value->predicate_function_span) &&
         receipt_size_literal(context, "|subject=") &&
         receipt_size_size(context, (size_t)value->subject_kind) &&
         receipt_size_literal(context, "|domain-kind=") &&
         receipt_size_size(context, (size_t)value->domain_kind) &&
         receipt_size_literal(context, "|dependent=") &&
         receipt_size_size(context, value->dependent_type_parameter_ordinal) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_enum(frontend_context *context,
                              const w_seed_frontend_enum *value) {
  return receipt_size_literal(context, "enum=") &&
         receipt_size_size(context, value->module_index) &&
         receipt_size_literal(context, "|") &&
         receipt_size_text(context, value->name) &&
         receipt_size_literal(context, "|exported=") &&
         receipt_size_size(context, value->exported ? 1u : 0u) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, value->span) &&
         receipt_size_literal(context, "|generic=") &&
         receipt_size_span(context, value->generic_span) &&
         receipt_size_literal(context, "|has-generic=") &&
         receipt_size_size(context, value->has_generic_parameters ? 1u : 0u) &&
         receipt_size_literal(context, "|conformance=") &&
         receipt_size_size(context, value->conformance_type) &&
         receipt_size_literal(context, "|conformance-span=") &&
         receipt_size_span(context, value->conformance_span) &&
         receipt_size_literal(context, "|first-case=") &&
         receipt_size_size(context, value->first_case) &&
         receipt_size_literal(context, "|case-count=") &&
         receipt_size_size(context, value->case_count) &&
         receipt_size_literal(context, "|type=") &&
         receipt_size_size(context, value->type_index) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_enum_case(
    frontend_context *context, const w_seed_frontend_enum_case *value) {
  return receipt_size_literal(context, "enum-case=") &&
         receipt_size_size(context, value->module_index) &&
         receipt_size_literal(context, "|") &&
         receipt_size_size(context, value->owner_enum) &&
         receipt_size_literal(context, "|") &&
         receipt_size_text(context, value->name) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, value->span) &&
         receipt_size_literal(context, "|first-payload=") &&
         receipt_size_size(context, value->first_payload) &&
         receipt_size_literal(context, "|payload-count=") &&
         receipt_size_size(context, value->payload_count) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_enum_case_parameter(
    frontend_context *context,
    const w_seed_frontend_enum_case_parameter *value) {
  return receipt_size_literal(context, "enum-case-parameter=") &&
         receipt_size_size(context, value->module_index) &&
         receipt_size_literal(context, "|") &&
         receipt_size_size(context, value->owner_case) &&
         receipt_size_literal(context, "|label=") &&
         receipt_size_text(context, value->label) &&
         receipt_size_literal(context, "|has-label=") &&
         receipt_size_size(context, value->has_label ? 1u : 0u) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, value->span) &&
         receipt_size_literal(context, "|type=") &&
         receipt_size_size(context, value->type_index) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_switch_arm(
    frontend_context *context, const w_seed_frontend_switch_arm *value) {
  return receipt_size_literal(context, "switch-arm=") &&
         receipt_size_size(context, value->module_index) &&
         receipt_size_literal(context, "|owner=") &&
         receipt_size_size(context, value->owner_expression) &&
         receipt_size_literal(context, "|pattern=") &&
         receipt_size_size(context, (size_t)value->pattern_kind) &&
         receipt_size_literal(context, "|enum=") &&
         receipt_size_size(context, value->enum_index) &&
         receipt_size_literal(context, "|case=") &&
         receipt_size_size(context, value->enum_case_index) &&
         receipt_size_literal(context, "|pattern-span=") &&
         receipt_size_span(context, value->pattern_span) &&
         receipt_size_literal(context, "|result=") &&
         receipt_size_size(context, value->result_expression) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, value->span) &&
         receipt_size_literal(context, "|supported=") &&
         receipt_size_size(context, value->supported ? 1u : 0u) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_fact(frontend_context *context,
                              const w_seed_frontend_fact *fact) {
  return receipt_size_literal(context, "fact=") &&
         receipt_size_literal(context, fact_name(fact->kind)) &&
         receipt_size_literal(context, "|") &&
         receipt_size_span(context, fact->span) &&
         receipt_size_literal(context, "|") &&
         receipt_size_text(context, fact->detail) &&
         receipt_size_literal(context, "\n");
}

static size_t receipt_signed_decimal_length(int64_t value) {
  uint64_t magnitude = value < 0
                           ? (uint64_t)(-(value + 1)) + UINT64_C(1)
                           : (uint64_t)value;
  size_t length = value < 0 ? 1u : 0u;
  do {
    magnitude /= UINT64_C(10);
    length += 1u;
  } while (magnitude != 0u);
  return length;
}

static bool receipt_size_signed(frontend_context *context, int64_t value) {
  return receipt_size_add(context, receipt_signed_decimal_length(value));
}

static bool receipt_size_diagnostic_fact_input(
    frontend_context *context, size_t diagnostic_index, size_t first_item,
    const frontend_diagnostic_fact_input *fact) {
  if (context == NULL || fact == NULL) return false;
  if (!receipt_size_literal(context, "diagnostic-fact=") ||
      !receipt_size_size(context, diagnostic_index) ||
      !receipt_size_literal(context, "|key=") ||
      !receipt_size_text(context, fact->key) ||
      !receipt_size_literal(context, "|kind=") ||
      !receipt_size_size(context, (size_t)fact->kind) ||
      !receipt_size_literal(context, "|value=")) {
    return false;
  }
  if (fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER) {
    if (!receipt_size_signed(context, fact->integer_value)) return false;
  } else if (!receipt_size_text(context, fact->text)) {
    return false;
  }
  if (!receipt_size_literal(context, "|items=") ||
      !receipt_size_size(context, first_item) ||
      !receipt_size_literal(context, ":") ||
      !receipt_size_size(context, fact->item_count) ||
      !receipt_size_literal(context, "\n")) {
    return false;
  }
  for (size_t item = 0; item < fact->item_count; item += 1u) {
    if (fact->items == NULL ||
        !receipt_size_literal(context, "diagnostic-item=") ||
        !receipt_size_size(context, diagnostic_index) ||
        !receipt_size_literal(context, "|") ||
        !receipt_size_size(context, item) ||
        !receipt_size_literal(context, "|") ||
        !receipt_size_text(context, fact->items[item]) ||
        !receipt_size_literal(context, "\n")) {
      return false;
    }
  }
  return true;
}

static bool receipt_size_diagnostic_label_input(
    frontend_context *context, size_t diagnostic_index,
    const frontend_diagnostic_label_input *label) {
  return context != NULL && label != NULL &&
         receipt_size_literal(context, "diagnostic-label=") &&
         receipt_size_size(context, diagnostic_index) &&
         receipt_size_literal(context, "|role=") &&
         receipt_size_text(context, label->role) &&
         receipt_size_literal(context, "|document=") &&
         receipt_size_size(context, label->document_index) &&
         receipt_size_literal(context, "|span=") &&
         receipt_size_span(context, label->span) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_diagnostic_input(
    frontend_context *context, size_t diagnostic_index,
    const w_seed_frontend_diagnostic *diagnostic,
    const frontend_diagnostic_fact_input *facts, size_t fact_count,
    const frontend_diagnostic_label_input *labels, size_t label_count) {
  if (context == NULL || diagnostic == NULL ||
      !receipt_size_literal(context, "diagnostic=") ||
      !receipt_size_text(context, diagnostic->code) ||
      !receipt_size_literal(context, "|") ||
      !receipt_size_span(context, diagnostic->primary) ||
      !receipt_size_literal(context, "|facts=") ||
      !receipt_size_size(context, diagnostic->first_fact) ||
      !receipt_size_literal(context, ":") ||
      !receipt_size_size(context, fact_count) ||
      !receipt_size_literal(context, "|labels=") ||
      !receipt_size_size(context, diagnostic->first_label) ||
      !receipt_size_literal(context, ":") ||
      !receipt_size_size(context, label_count) ||
      !receipt_size_literal(context, "\n")) {
    return false;
  }
  size_t item_offset = context->count.diagnostic_items;
  for (size_t index = 0u; index < fact_count; index += 1u) {
    /* Emit stores NONE for scalar facts.  Use the same sentinel while sizing
     * the receipt, or the dry and emit passes produce different bytes. */
    const size_t first_item = facts[index].item_count == 0u
                                  ? (size_t)W_SEED_FRONTEND_NONE
                                  : item_offset;
    if (!receipt_size_diagnostic_fact_input(
            context, diagnostic_index, first_item,
            &facts[index])) {
      return false;
    }
    if (item_offset > SIZE_MAX - facts[index].item_count) {
      return false;
    }
    item_offset += facts[index].item_count;
  }
  for (size_t index = 0u; index < label_count; index += 1u) {
    if (!receipt_size_diagnostic_label_input(context, diagnostic_index,
                                             &labels[index])) {
      return false;
    }
  }
  return true;
}

static w_seed_frontend_text text_from_span(const w_seed_frontend_document *doc,
                                           w_seed_span span) {
  w_seed_frontend_text text = {NULL, 0};
  if (doc == NULL || doc->source == NULL || span.end_byte < span.start_byte ||
      span.end_byte > doc->source->bytes.length) {
    return text;
  }
  text.data = (const char *)(doc->source->bytes.data + span.start_byte);
  text.length = span.end_byte - span.start_byte;
  return text;
}

static bool text_equal(w_seed_frontend_text left, const char *right) {
  const size_t length = strlen(right);
  return left.length == length &&
         (length == 0 || memcmp(left.data, right, length) == 0);
}

static bool text_equal_text(w_seed_frontend_text left,
                            w_seed_frontend_text right) {
  return left.length == right.length &&
         (left.length == 0 || memcmp(left.data, right.data, left.length) == 0);
}

static bool is_ascii_space(uint8_t value) {
  return value == (uint8_t)' ' || value == (uint8_t)'\t' ||
         value == (uint8_t)'\n' || value == (uint8_t)'\r' ||
         value == (uint8_t)'\f' || value == (uint8_t)'\v';
}

static w_seed_span trim_span(const w_seed_frontend_document *doc,
                             w_seed_span span) {
  if (doc == NULL || doc->source == NULL ||
      span.end_byte > doc->source->bytes.length ||
      span.start_byte > span.end_byte) {
    return empty_span(span.start_byte);
  }
  while (span.start_byte < span.end_byte &&
         is_ascii_space(doc->source->bytes.data[span.start_byte])) {
    span.start_byte += 1;
  }
  while (span.end_byte > span.start_byte &&
         is_ascii_space(doc->source->bytes.data[span.end_byte - 1])) {
    span.end_byte -= 1;
  }
  return span;
}

static bool node_is_raw(const w_seed_cst_node *node) {
  return node != NULL && (node->flags & W_SEED_CST_FLAG_RAW_LEAF) != 0;
}

static bool node_is_trivia(const w_seed_cst_node *node) {
  return node != NULL &&
         (node->kind == W_SEED_CST_SOURCE_PREFIX ||
          node->kind == W_SEED_CST_TRIVIA ||
          (node->flags & W_SEED_CST_FLAG_TRIVIA) != 0);
}

static bool node_span_valid(const w_seed_frontend_document *doc,
                            const w_seed_cst_node *node) {
  return doc != NULL && doc->source != NULL && node != NULL &&
         node->raw_span.start_byte <= node->raw_span.end_byte &&
         node->raw_span.end_byte <= doc->source->bytes.length;
}

static bool document_ready(const w_seed_frontend_document *doc,
                           size_t *bad_span_index) {
  if (bad_span_index != NULL) *bad_span_index = 0;
  if (doc == NULL || doc->source == NULL ||
      (doc->source->bytes.length != 0 && doc->source->bytes.data == NULL) ||
      doc->logical_source_id.length == 0 || doc->logical_source_id.data == NULL ||
      doc->module_id.length == 0 || doc->module_id.data == NULL ||
      doc->local_module_name.length == 0 ||
      doc->local_module_name.data == NULL ||
      doc->nodes == NULL || doc->node_count == 0 ||
      doc->parse.status != W_SEED_PARSE_COMPLETE ||
      doc->parse.issue_count != 0 || doc->parse.root >= doc->node_count ||
      doc->parse.node_count > doc->node_count ||
      doc->parse.node_count > W_SEED_FRONTEND_MAX_CST_NODES ||
      doc->nodes[doc->parse.root].kind != W_SEED_CST_DOCUMENT) {
    return false;
  }
  for (size_t index = 0; index < doc->parse.node_count; index += 1) {
    const w_seed_cst_node *node = &doc->nodes[index];
    if (!node_span_valid(doc, node)) {
      if (bad_span_index != NULL) *bad_span_index = index;
      return false;
    }
    if (node->first_child != W_SEED_CST_NONE &&
        node->first_child >= doc->parse.node_count) {
      if (bad_span_index != NULL) *bad_span_index = index;
      return false;
    }
    if (node->next_sibling != W_SEED_CST_NONE &&
        node->next_sibling >= doc->parse.node_count) {
      if (bad_span_index != NULL) *bad_span_index = index;
      return false;
    }
  }
  /* COMPLETE input is a tree, not a bounded walk with silently truncated
   * sibling chains. Reject cycles in every child list before normalization. */
  for (size_t index = 0; index < doc->parse.node_count; index += 1) {
    const w_seed_cst_node *node = &doc->nodes[index];
    uint32_t cursor = doc->nodes[index].first_child;
    size_t guard = 0;
    while (cursor != W_SEED_CST_NONE) {
      if (guard >= doc->parse.node_count ||
          cursor >= doc->parse.node_count || cursor == index ||
          doc->nodes[cursor].raw_span.start_byte < node->raw_span.start_byte ||
          doc->nodes[cursor].raw_span.end_byte > node->raw_span.end_byte) {
        if (bad_span_index != NULL) *bad_span_index = index;
        return false;
      }
      cursor = doc->nodes[cursor].next_sibling;
      guard += 1;
    }
  }
  return true;
}

static bool external_text_valid(w_seed_frontend_text text) {
  return text.length == 0 || text.data != NULL;
}

static bool external_input_ready(const w_seed_frontend_input *input) {
  if (input == NULL) return false;
  if (input->external_module_count != 0 && input->external_modules == NULL) {
    return false;
  }
  if (input->external_module_count >
          (size_t)W_SEED_FRONTEND_MAX_EXTERNAL_MODULES ||
      input->external_module_count > (size_t)UINT32_MAX) {
    return false;
  }
  for (size_t module_index = 0;
       module_index < input->external_module_count; module_index += 1) {
    const w_seed_frontend_external_module *module =
        &input->external_modules[module_index];
    if (!external_text_valid(module->module_id) || module->module_id.length == 0 ||
        (module->symbol_count != 0 && module->symbols == NULL) ||
        module->symbol_count >
            (size_t)W_SEED_FRONTEND_MAX_EXTERNAL_SYMBOLS ||
        module->symbol_count > (size_t)UINT32_MAX) {
      return false;
    }
    for (size_t symbol_index = 0; symbol_index < module->symbol_count;
         symbol_index += 1) {
      const w_seed_frontend_external_symbol *symbol =
          &module->symbols[symbol_index];
      if (!external_text_valid(symbol->name) || symbol->name.length == 0 ||
          (symbol->kind != W_SEED_FRONTEND_EXTERNAL_VALUE &&
           symbol->kind != W_SEED_FRONTEND_EXTERNAL_TYPE) ||
          !external_text_valid(symbol->return_type) ||
          symbol->return_type.length == 0 ||
          (symbol->parameter_count != 0 && symbol->parameters == NULL) ||
          symbol->parameter_count >
              (size_t)W_SEED_FRONTEND_MAX_EXTERNAL_PARAMETERS ||
          symbol->parameter_count > (size_t)UINT32_MAX) {
        return false;
      }
      for (size_t parameter_index = 0;
           parameter_index < symbol->parameter_count; parameter_index += 1) {
        const w_seed_frontend_external_parameter *parameter =
            &symbol->parameters[parameter_index];
        if (!external_text_valid(parameter->name) ||
            parameter->name.length == 0 ||
            parameter->label_kind > W_SEED_FRONTEND_LABEL_OPTIONAL ||
            !external_text_valid(parameter->type) ||
            parameter->type.length == 0) {
          return false;
        }
      }
    }
    for (size_t prior_module = 0; prior_module < module_index;
         prior_module += 1) {
      if (text_equal_text(module->module_id,
                          input->external_modules[prior_module].module_id)) {
        return false;
      }
    }
    for (size_t symbol_index = 0; symbol_index < module->symbol_count;
         symbol_index += 1) {
      for (size_t prior_symbol = 0; prior_symbol < symbol_index;
           prior_symbol += 1) {
        if (text_equal_text(module->symbols[symbol_index].name,
                            module->symbols[prior_symbol].name)) {
          return false;
        }
      }
    }
  }
  return true;
}

static bool host_prelude_input_ready(const w_seed_frontend_input *input) {
  if (input == NULL) return false;
  if (input->host_scope == NULL) return true;
  const w_seed_frontend_host_prelude *prelude = input->host_scope;
  size_t total_symbols = 0u;
  size_t total_parameters = 0u;
  if (!external_text_valid(prelude->profile) || prelude->profile.length == 0u ||
      (prelude->symbol_count != 0u && prelude->symbols == NULL) ||
      prelude->symbol_count > (size_t)W_SEED_FRONTEND_MAX_HOST_SYMBOLS ||
      prelude->symbol_count > (size_t)UINT32_MAX ||
      !add_size(total_symbols, prelude->symbol_count, &total_symbols) ||
      total_symbols > (size_t)W_SEED_FRONTEND_MAX_HOST_SYMBOLS) {
    return false;
  }
  for (size_t symbol_index = 0u; symbol_index < prelude->symbol_count;
       symbol_index += 1u) {
    const w_seed_frontend_host_prelude_symbol *symbol =
        &prelude->symbols[symbol_index];
    if (!external_text_valid(symbol->name) || symbol->name.length == 0u ||
        (symbol->kind != W_SEED_FRONTEND_EXTERNAL_VALUE &&
         symbol->kind != W_SEED_FRONTEND_EXTERNAL_TYPE) ||
        !external_text_valid(symbol->return_type) ||
        symbol->return_type.length == 0u ||
        (symbol->parameter_count != 0u && symbol->parameters == NULL) ||
        symbol->parameter_count >
            (size_t)W_SEED_FRONTEND_MAX_HOST_PARAMETERS ||
        symbol->parameter_count > (size_t)UINT32_MAX ||
        !add_size(total_parameters, symbol->parameter_count,
                  &total_parameters) ||
        total_parameters > (size_t)W_SEED_FRONTEND_MAX_HOST_PARAMETERS ||
        (symbol->requirement_count != 0u && symbol->requirements == NULL) ||
        symbol->requirement_count >
            (size_t)W_SEED_FRONTEND_MAX_HOST_REQUIREMENTS ||
        symbol->requirement_count > (size_t)UINT32_MAX) {
      return false;
    }
    for (size_t prior_symbol = 0u; prior_symbol < symbol_index;
         prior_symbol += 1u) {
      if (text_equal_text(symbol->name,
                          prelude->symbols[prior_symbol].name)) {
        return false;
      }
    }
    for (size_t requirement_index = 0u;
         requirement_index < symbol->requirement_count; requirement_index += 1u) {
      const w_seed_frontend_host_requirement *requirement =
          &symbol->requirements[requirement_index];
      if (!external_text_valid(requirement->name) ||
          requirement->name.length == 0u) {
        return false;
      }
      for (size_t prior_requirement = 0u;
           prior_requirement < requirement_index; prior_requirement += 1u) {
        if (text_equal_text(requirement->name,
                            symbol->requirements[prior_requirement].name)) {
          return false;
        }
      }
    }
    for (size_t parameter_index = 0u;
         parameter_index < symbol->parameter_count; parameter_index += 1u) {
      const w_seed_frontend_external_parameter *parameter =
          &symbol->parameters[parameter_index];
      if (!external_text_valid(parameter->name) ||
          parameter->name.length == 0u ||
          parameter->label_kind > W_SEED_FRONTEND_LABEL_OPTIONAL ||
          !external_text_valid(parameter->type) ||
          parameter->type.length == 0u) {
        return false;
      }
    }
  }
  return true;
}

static bool next_child(const w_seed_frontend_document *doc, uint32_t *cursor,
                       uint32_t *child) {
  if (doc == NULL || cursor == NULL || child == NULL ||
      *cursor == W_SEED_CST_NONE || *cursor >= doc->parse.node_count) {
    return false;
  }
  *child = *cursor;
  *cursor = doc->nodes[*cursor].next_sibling;
  return true;
}

static size_t count_direct_kind(const w_seed_frontend_document *doc,
                                uint32_t parent, w_seed_cst_kind kind) {
  if (doc == NULL || parent >= doc->parse.node_count) return 0;
  size_t count = 0;
  uint32_t cursor = doc->nodes[parent].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == kind) count += 1;
    guard += 1;
  }
  return count;
}

static uint32_t first_direct_kind(const w_seed_frontend_document *doc,
                                  uint32_t parent, w_seed_cst_kind kind) {
  if (doc == NULL || parent >= doc->parse.node_count) return W_SEED_CST_NONE;
  uint32_t cursor = doc->nodes[parent].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == kind) return child;
    guard += 1;
  }
  return W_SEED_CST_NONE;
}

static bool token_cursor_next(frontend_token_cursor *cursor,
                              frontend_token *token) {
  if (cursor == NULL || token == NULL || cursor->document == NULL) return false;
  const w_seed_frontend_document *doc = cursor->document;
  while (cursor->index < doc->parse.node_count) {
    const w_seed_cst_node *node = &doc->nodes[cursor->index];
    cursor->index += 1;
    if (!node_is_raw(node) || node_is_trivia(node)) continue;
    if (node->raw_span.start_byte < cursor->bounds.start_byte ||
        node->raw_span.end_byte > cursor->bounds.end_byte) {
      continue;
    }
    token->kind = node->kind;
    token->span = node->raw_span;
    return true;
  }
  return false;
}

static frontend_token_cursor token_cursor_for(
    const w_seed_frontend_document *doc, w_seed_span bounds) {
  frontend_token_cursor cursor = {doc, 0, bounds};
  return cursor;
}

static bool token_text(const w_seed_frontend_document *doc,
                       const frontend_token *token, const char *text) {
  return token != NULL && text_equal(text_from_span(doc, token->span), text);
}

static bool cursor_take_text(frontend_token_cursor *cursor, const char *text,
                             frontend_token *taken) {
  if (cursor == NULL) return false;
  frontend_token_cursor copy = *cursor;
  frontend_token token;
  if (!token_cursor_next(&copy, &token) ||
      !token_text(cursor->document, &token, text)) {
    return false;
  }
  *cursor = copy;
  if (taken != NULL) *taken = token;
  return true;
}

static bool cursor_peek_text(const frontend_token_cursor *cursor,
                             const char *text) {
  if (cursor == NULL) return false;
  frontend_token_cursor copy = *cursor;
  return cursor_take_text(&copy, text, NULL);
}

static bool cursor_take(frontend_token_cursor *cursor, frontend_token *token) {
  if (cursor == NULL) return false;
  return token_cursor_next(cursor, token);
}

static bool cursor_peek(const frontend_token_cursor *cursor,
                        frontend_token *token) {
  if (cursor == NULL || token == NULL) return false;
  frontend_token_cursor copy = *cursor;
  return token_cursor_next(&copy, token);
}

static w_seed_span owner_span(const w_seed_frontend_document *doc,
                              uint32_t node_index) {
  if (doc == NULL || node_index >= doc->parse.node_count) return empty_span(0);
  return doc->nodes[node_index].raw_span;
}

static bool span_starts_with(const w_seed_frontend_document *doc,
                             w_seed_span span, const char *text) {
  const w_seed_frontend_text view = text_from_span(doc, trim_span(doc, span));
  const size_t length = strlen(text);
  return view.length >= length &&
         (length == 0 || memcmp(view.data, text, length) == 0);
}

static bool ascii_is_digit(uint8_t value) {
  return value >= (uint8_t)'0' && value <= (uint8_t)'9';
}

static bool type_name_integer(w_seed_frontend_text text, bool *is_signed,
                              uint16_t *width) {
  if (text.length < 2 || (text.data[0] != 'u' && text.data[0] != 'i')) {
    return false;
  }
  size_t value = 0;
  for (size_t index = 1; index < text.length; index += 1) {
    const uint8_t byte = (uint8_t)text.data[index];
    if (!ascii_is_digit(byte)) return false;
    value = value * 10u + (size_t)(byte - (uint8_t)'0');
    if (value > UINT16_MAX) return false;
  }
  if (value == 0) return false;
  *is_signed = text.data[0] == 'i';
  *width = (uint16_t)value;
  return true;
}

/* The seed checker has portable storage for the four fixed widths below.
 * i128/u128 stay outside this slice until the target's exact integer model is
 * available; treating them as unknown is safer than accepting a narrowing or
 * an inexact float conversion. */
static bool integer_width_supported(uint16_t width) {
  return width == 8u || width == 16u || width == 32u || width == 64u;
}

static bool looks_like_integer_type(w_seed_frontend_text text) {
  if (text.length < 2 || (text.data[0] != 'i' && text.data[0] != 'u')) {
    return false;
  }
  for (size_t index = 1; index < text.length; index += 1) {
    if (!ascii_is_digit((uint8_t)text.data[index])) return false;
  }
  return true;
}

/* Split an integer literal into its numeric body and an optional _iN/_uN
 * suffix.  A true return means the spelling has a valid integer shape; the
 * caller still validates the width and representability. */
static bool integer_literal_parts(w_seed_frontend_text text,
                                  size_t *body_end, bool *has_suffix,
                                  bool *is_signed, uint16_t *width) {
  if (body_end == NULL || has_suffix == NULL || is_signed == NULL ||
      width == NULL || text.length == 0) {
    return false;
  }
  *body_end = text.length;
  *has_suffix = false;
  *is_signed = true;
  *width = 0;
  size_t underscore = text.length;
  while (underscore != 0) {
    underscore -= 1;
    if (text.data[underscore] != '_') continue;
    if (underscore + 2u >= text.length ||
        (text.data[underscore + 1u] != 'i' &&
         text.data[underscore + 1u] != 'u')) {
      break;
    }
    size_t suffix_width = 0;
    for (size_t index = underscore + 2u; index < text.length; index += 1) {
      if (!ascii_is_digit((uint8_t)text.data[index])) return false;
      suffix_width = suffix_width * 10u +
                     (size_t)(text.data[index] - (uint8_t)'0');
      if (suffix_width > UINT16_MAX) return false;
    }
    if (suffix_width == 0) return false;
    *body_end = underscore;
    *has_suffix = true;
    *is_signed = text.data[underscore + 1u] == 'i';
    *width = (uint16_t)suffix_width;
    break;
  }
  return *body_end != 0;
}

static bool integer_literal_value(w_seed_frontend_text text, size_t body_end,
                                  uint64_t *value) {
  if (value == NULL || body_end == 0 || body_end > text.length) return false;
  uint64_t base = UINT64_C(10);
  size_t index = 0;
  if (body_end >= 2 && text.data[0] == '0' &&
      (text.data[1] == 'x' || text.data[1] == 'X')) {
    base = UINT64_C(16);
    index = 2u;
  } else if (body_end >= 2 && text.data[0] == '0' &&
             (text.data[1] == 'o' || text.data[1] == 'O')) {
    base = UINT64_C(8);
    index = 2u;
  } else if (body_end >= 2 && text.data[0] == '0' &&
             (text.data[1] == 'b' || text.data[1] == 'B')) {
    base = UINT64_C(2);
    index = 2u;
  }
  if (index >= body_end) return false;
  uint64_t result = 0;
  bool saw_digit = false;
  for (; index < body_end; index += 1) {
    const uint8_t byte = (uint8_t)text.data[index];
    if (byte == (uint8_t)'_') continue;
    uint8_t digit = 0;
    if (byte >= (uint8_t)'0' && byte <= (uint8_t)'9') {
      digit = (uint8_t)(byte - (uint8_t)'0');
    } else if (base == UINT64_C(16) && byte >= (uint8_t)'a' &&
               byte <= (uint8_t)'f') {
      digit = (uint8_t)(byte - (uint8_t)'a' + 10u);
    } else if (base == UINT64_C(16) && byte >= (uint8_t)'A' &&
               byte <= (uint8_t)'F') {
      digit = (uint8_t)(byte - (uint8_t)'A' + 10u);
    } else {
      return false;
    }
    if (digit >= base || result > (UINT64_MAX - digit) / base) return false;
    result = result * base + digit;
    saw_digit = true;
  }
  if (!saw_digit) return false;
  *value = result;
  return true;
}

static frontend_simple_type simple_type_unknown(void) {
  frontend_simple_type type;
  (void)memset(&type, 0, sizeof(type));
  type.kind = W_SEED_FRONTEND_TYPE_UNKNOWN;
  type.enum_index = W_SEED_FRONTEND_NONE;
  type.subset_span = empty_span(0);
  type.element_kind = W_SEED_FRONTEND_TYPE_UNKNOWN;
  type.element_enum_index = W_SEED_FRONTEND_NONE;
  return type;
}

static frontend_simple_type simple_type_from_text(
    const w_seed_frontend_document *doc, w_seed_span raw_span) {
  const w_seed_span span = trim_span(doc, raw_span);
  const w_seed_frontend_text spelling = text_from_span(doc, span);
  frontend_simple_type type = simple_type_unknown();
  type.spelling = spelling;
  if (text_equal(spelling, "()")) {
    type.kind = W_SEED_FRONTEND_TYPE_UNIT;
    return type;
  }
  if (text_equal(spelling, "Bool")) {
    type.kind = W_SEED_FRONTEND_TYPE_BOOL;
    return type;
  }
  if (text_equal(spelling, "String")) {
    type.kind = W_SEED_FRONTEND_TYPE_STRING;
    return type;
  }
  if (text_equal(spelling, "bytes") || text_equal(spelling, "Bytes")) {
    type.kind = W_SEED_FRONTEND_TYPE_BYTES;
    return type;
  }
  if (text_equal(spelling, "usize")) {
    type.kind = W_SEED_FRONTEND_TYPE_INTEGER;
    type.is_signed = false;
    type.bit_width = (uint16_t)W_SEED_FRONTEND_TARGET_USIZE_BITS;
    return type;
  }
  if (spelling.length >= 11u &&
      memcmp(spelling.data, "StaticList<", 11u) == 0 &&
      spelling.data[spelling.length - 1u] == '>') {
    type.kind = W_SEED_FRONTEND_TYPE_STATIC_LIST;
    return type;
  }
  bool is_signed = false;
  uint16_t width = 0;
  if (type_name_integer(spelling, &is_signed, &width) &&
      integer_width_supported(width)) {
    type.kind = W_SEED_FRONTEND_TYPE_INTEGER;
    type.is_signed = is_signed;
    type.bit_width = width;
    return type;
  }
  if (text_equal(spelling, "Int") || text_equal(spelling, "UInt")) {
    type.kind = W_SEED_FRONTEND_TYPE_INTEGER;
    type.is_signed = text_equal(spelling, "Int");
    type.bit_width = 64u;
    return type;
  }
  if (looks_like_integer_type(spelling)) {
    /* A spelling such as u7 or i128 is not a nominal type in W.  Keep it
     * unknown so declaration/expression normalization records an explicit
     * unsupported type fact. */
    return type;
  }
  if (text_equal(spelling, "f32") || text_equal(spelling, "f64")) {
    type.kind = W_SEED_FRONTEND_TYPE_FLOAT;
    type.bit_width = (uint16_t)(text_equal(spelling, "f32") ? 32u : 64u);
    return type;
  }
  if (spelling.length != 0 && spelling.data[spelling.length - 1] == '?') {
    type.kind = W_SEED_FRONTEND_TYPE_OPTION;
    type.spelling = spelling;
    return type;
  }
  if (spelling.length >= 6u &&
      memcmp(spelling.data, "Range<", 6u) == 0 &&
      spelling.data[spelling.length - 1u] == '>') {
    type.kind = W_SEED_FRONTEND_TYPE_RANGE;
    return type;
  }
  if (span_starts_with(doc, span, "fn") || span_starts_with(doc, span, "some fn") ||
      span_starts_with(doc, span, "any fn")) {
    type.kind = W_SEED_FRONTEND_TYPE_FUNCTION;
    return type;
  }
  if (spelling.length != 0) type.kind = W_SEED_FRONTEND_TYPE_NOMINAL;
  return type;
}

static bool type_equal(frontend_simple_type left, frontend_simple_type right) {
  if (left.kind != right.kind) return false;
  if (left.kind == W_SEED_FRONTEND_TYPE_INTEGER) {
    return left.is_signed == right.is_signed &&
           left.bit_width == right.bit_width;
  }
  if (left.kind == W_SEED_FRONTEND_TYPE_FLOAT) {
    return left.bit_width == right.bit_width;
  }
  if (left.kind == W_SEED_FRONTEND_TYPE_BOOL ||
      left.kind == W_SEED_FRONTEND_TYPE_STRING ||
      left.kind == W_SEED_FRONTEND_TYPE_BYTES ||
      left.kind == W_SEED_FRONTEND_TYPE_UNIT) {
    return true;
  }
  if (left.kind == W_SEED_FRONTEND_TYPE_OPTION) {
    return text_equal_text(left.spelling, right.spelling);
  }
  if (left.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST ||
      left.kind == W_SEED_FRONTEND_TYPE_RANGE) {
    return text_equal_text(left.spelling, right.spelling);
  }
  if (left.kind == W_SEED_FRONTEND_TYPE_FUNCTION) {
    return text_equal_text(left.spelling, right.spelling);
  }
  return text_equal_text(left.spelling, right.spelling);
}

static bool type_is_bool(frontend_simple_type type) {
  return type.kind == W_SEED_FRONTEND_TYPE_BOOL;
}

static bool type_is_numeric(frontend_simple_type type) {
  return type.kind == W_SEED_FRONTEND_TYPE_INTEGER ||
         type.kind == W_SEED_FRONTEND_TYPE_FLOAT;
}

static bool unsuffixed_integer_fits(w_seed_frontend_text spelling,
                                    frontend_simple_type expected) {
  if (expected.kind != W_SEED_FRONTEND_TYPE_INTEGER ||
      !integer_width_supported(expected.bit_width) || spelling.length == 0) {
    return false;
  }
  size_t body_end = spelling.length;
  bool has_suffix = false;
  bool is_signed = true;
  uint16_t width = 0;
  if (!integer_literal_parts(spelling, &body_end, &has_suffix, &is_signed,
                             &width) ||
      has_suffix) {
    return false;
  }
  uint64_t value = 0;
  if (!integer_literal_value(spelling, body_end, &value)) return false;
  uint64_t maximum = UINT64_MAX;
  if (expected.bit_width < 64u) {
    maximum = expected.is_signed
                  ? (UINT64_C(1) << (expected.bit_width - 1u)) - 1u
                  : (UINT64_C(1) << expected.bit_width) - 1u;
  } else if (expected.is_signed) {
    maximum = INT64_MAX;
  }
  (void)is_signed;
  (void)width;
  return value <= maximum;
}

static bool widening_allowed(frontend_simple_type actual,
                             frontend_simple_type expected) {
  if (type_equal(actual, expected)) return true;
  if (actual.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
      expected.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
      actual.bit_width == 0) {
    return unsuffixed_integer_fits(actual.spelling, expected);
  }
  if (actual.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
      expected.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
      actual.is_signed == expected.is_signed && actual.bit_width != 0 &&
      expected.bit_width != 0 && actual.bit_width < expected.bit_width) {
    return true;
  }
  if (actual.kind == W_SEED_FRONTEND_TYPE_FLOAT &&
      expected.kind == W_SEED_FRONTEND_TYPE_FLOAT &&
      actual.bit_width < expected.bit_width) {
    return true;
  }
  if (actual.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
      expected.kind == W_SEED_FRONTEND_TYPE_FLOAT) {
    if (actual.bit_width == 0) {
      size_t body_end = actual.spelling.length;
      bool has_suffix = false;
      bool is_signed = true;
      uint16_t width = 0;
      uint64_t value = 0;
      if (!integer_literal_parts(actual.spelling, &body_end, &has_suffix,
                                 &is_signed, &width) ||
          has_suffix ||
          !integer_literal_value(actual.spelling, body_end, &value)) {
        return false;
      }
      (void)is_signed;
      (void)width;
      /* Every integer up to 2^24 is exact in f32 and every integer up to
       * 2^53 is exact in f64.  Larger literal values remain unsupported in
       * this small checker instead of silently rounding. */
      const uint64_t exact_limit = expected.bit_width == 32u
                                       ? (UINT64_C(1) << 24u)
                                       : (UINT64_C(1) << 53u);
      return value <= exact_limit;
    }
    if (expected.bit_width == 32u) return actual.bit_width <= 16u;
    if (expected.bit_width == 64u) return actual.bit_width <= 32u;
  }
  return false;
}

static bool frontend_type_is_enum(frontend_simple_type type) {
  return type.kind == W_SEED_FRONTEND_TYPE_ENUM ||
         type.kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
}

static bool enum_subset_set_equal(const frontend_context *context,
                                  frontend_simple_type left,
                                  frontend_simple_type right) {
  if (!frontend_type_is_enum(left) || !frontend_type_is_enum(right) ||
      left.enum_index == W_SEED_FRONTEND_NONE ||
      left.enum_index != right.enum_index) {
    return false;
  }
  if (left.kind == W_SEED_FRONTEND_TYPE_ENUM &&
      right.kind == W_SEED_FRONTEND_TYPE_ENUM) {
    return true;
  }
  if (left.kind == W_SEED_FRONTEND_TYPE_ENUM ||
      right.kind == W_SEED_FRONTEND_TYPE_ENUM) {
    return false;
  }
  const w_seed_frontend_document *doc = context_document(context);
  const w_seed_frontend_document *enum_doc = NULL;
  uint32_t enum_node = W_SEED_CST_NONE;
  if (doc == NULL ||
      !enum_declaration_for_name(context, left.enum_name, NULL, NULL,
                                 &enum_doc, &enum_node)) {
    return false;
  }
  uint32_t case_cursor = enum_doc->nodes[enum_node].first_child;
  uint32_t case_node = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(enum_doc, &case_cursor, &case_node) &&
         guard < enum_doc->parse.node_count) {
    if (enum_doc->nodes[case_node].kind == W_SEED_CST_ENUM_CASE) {
      const w_seed_frontend_text name =
          first_word_in_span(enum_doc, enum_doc->nodes[case_node].raw_span);
      uint32_t case_index = W_SEED_FRONTEND_NONE;
      if (!enum_case_for_name(context, left.enum_index, name, &case_index,
                              NULL, NULL) ||
          enum_subset_contains_case(context, left, case_index) !=
              enum_subset_contains_case(context, right, case_index)) {
        return false;
      }
    }
    guard += 1;
  }
  return true;
}

static bool frontend_type_equal(const frontend_context *context,
                                frontend_simple_type left,
                                frontend_simple_type right) {
  if (frontend_type_is_enum(left) || frontend_type_is_enum(right)) {
    return enum_subset_set_equal(context, left, right);
  }
  return type_equal(left, right);
}

static bool frontend_widening_allowed(const frontend_context *context,
                                      frontend_simple_type actual,
                                      frontend_simple_type expected) {
  if (frontend_type_is_enum(actual) || frontend_type_is_enum(expected)) {
    if (!frontend_type_is_enum(actual) || !frontend_type_is_enum(expected) ||
        actual.enum_index == W_SEED_FRONTEND_NONE ||
        actual.enum_index != expected.enum_index) {
      return false;
    }
    if (expected.kind == W_SEED_FRONTEND_TYPE_ENUM) return true;
    if (actual.kind == W_SEED_FRONTEND_TYPE_ENUM) return false;
    if (enum_subset_set_equal(context, actual, expected)) return true;
    /* A subset is assignable to a superset when every normalized member is
     * present in the expected set.  The membership helper performs the
     * bounded O(n²) scan without a fixed-width bitset. */
    const w_seed_frontend_document *doc = context_document(context);
    if (doc == NULL) return false;
    frontend_token_cursor cursor = token_cursor_for(doc, actual.subset_span);
    frontend_token token;
    while (cursor_take(&cursor, &token)) {
      w_seed_frontend_text name = {NULL, 0};
      if (token_text(doc, &token, ".")) {
        frontend_token member;
        if (!cursor_take(&cursor, &member) ||
            member.kind != W_SEED_CST_WORD) return false;
        name = text_from_span(doc, member.span);
      } else if (token.kind == W_SEED_CST_WORD) {
        frontend_token dot;
        frontend_token member;
        if (!cursor_take(&cursor, &dot) ||
            !token_text(doc, &dot, ".") || !cursor_take(&cursor, &member) ||
            member.kind != W_SEED_CST_WORD) continue;
        name = text_from_span(doc, member.span);
      }
      if (name.length == 0) continue;
      uint32_t case_index = W_SEED_FRONTEND_NONE;
      if (!enum_case_for_name(context, actual.enum_index, name, &case_index,
                              NULL, NULL) ||
          !enum_subset_contains_case(context, expected, case_index)) {
        return false;
      }
    }
    return true;
  }
  return widening_allowed(actual, expected);
}

static bool is_binary_operator(w_seed_frontend_text text) {
  static const char *const operators[] = {
      "+",  "-",  "*",  "/",  "%",  "==", "!=", "<", "<=", ">",
      ">=", "&&", "||", "in", "..<",
  };
  for (size_t index = 0; index < sizeof(operators) / sizeof(operators[0]);
       index += 1) {
    if (text_equal(text, operators[index])) return true;
  }
  return false;
}

static int operator_precedence(w_seed_frontend_text text) {
  if (text_equal(text, "||")) return 1;
  if (text_equal(text, "&&")) return 2;
  if (text_equal(text, "==") || text_equal(text, "!=")) return 3;
  if (text_equal(text, "in")) return 4;
  if (text_equal(text, "<") || text_equal(text, "<=") ||
      text_equal(text, ">") || text_equal(text, ">=")) {
    return 4;
  }
  if (text_equal(text, "..<")) return 5;
  if (text_equal(text, "+") || text_equal(text, "-")) return 6;
  if (text_equal(text, "*") || text_equal(text, "/") ||
      text_equal(text, "%")) {
    return 7;
  }
  return -1;
}

static size_t count_root_children(const w_seed_frontend_document *doc,
                                  w_seed_cst_kind kind) {
  return count_direct_kind(doc, doc->parse.root, kind);
}

static bool initialize_const_document_bases(frontend_context *context) {
  if (context == NULL ||
      context->input.document_count > W_SEED_FRONTEND_MAX_DOCUMENTS)
    return false;
  size_t base = 0u;
  for (size_t document = 0u; document < context->input.document_count;
       document += 1u) {
    context->const_document_bases[document] = base;
    const size_t count = count_root_children(
        &context->input.documents[document], W_SEED_CST_CONST_DECLARATION);
    if (!add_size(base, count, &base) ||
        base > (size_t)W_SEED_FRONTEND_MAX_CONST_DECLARATIONS)
      return false;
  }
  context->const_total_count = base;
  return true;
}

static size_t count_struct_generic_parameters(
    const w_seed_frontend_document *doc) {
  if (doc == NULL || doc->parse.root >= doc->parse.node_count) return 0;
  size_t count = 0;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) &&
         guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_STRUCT) {
      const uint32_t generic_node =
          first_direct_kind(doc, child, W_SEED_CST_GENERIC_PARAMETERS);
      if (generic_node != W_SEED_CST_NONE) {
        count += count_direct_kind(doc, generic_node,
                                   W_SEED_CST_GENERIC_PARAMETER);
      }
    }
    guard += 1;
  }
  return count;
}

static bool kind_is_statement(w_seed_cst_kind kind) {
  return kind == W_SEED_CST_LET_STATEMENT ||
         kind == W_SEED_CST_VAR_STATEMENT ||
         kind == W_SEED_CST_RETURN_STATEMENT ||
         kind == W_SEED_CST_IF_STATEMENT ||
         kind == W_SEED_CST_GUARD_STATEMENT ||
         kind == W_SEED_CST_EXPRESSION_STATEMENT ||
         kind == W_SEED_CST_EXPECT_STATEMENT ||
         kind == W_SEED_CST_REPEAT_STATEMENT ||
         kind == W_SEED_CST_FOR_STATEMENT ||
         kind == W_SEED_CST_BREAK_STATEMENT ||
         kind == W_SEED_CST_CONTINUE_STATEMENT ||
         kind == W_SEED_CST_COMMIT_STATEMENT ||
         kind == W_SEED_CST_ALLOCATOR_BLOCK ||
         kind == W_SEED_CST_SPAWN_STATEMENT ||
         kind == W_SEED_CST_TRANSACTION_EXPRESSION;
}

static bool kind_is_unsupported_owner(w_seed_cst_kind kind) {
  return kind == W_SEED_CST_ARRAY || kind == W_SEED_CST_REPEAT_STATEMENT ||
         kind == W_SEED_CST_FOR_STATEMENT ||
         kind == W_SEED_CST_CONTRACT_ENVELOPE ||
         kind == W_SEED_CST_TRANSACTION_EXPRESSION ||
         kind == W_SEED_CST_COMMIT_STATEMENT ||
         kind == W_SEED_CST_LOCK_EXPRESSION ||
         kind == W_SEED_CST_SPAWN_STATEMENT ||
         kind == W_SEED_CST_ALLOCATOR_BLOCK ||
         kind == W_SEED_CST_CLOSURE_EXPRESSION ||
         kind == W_SEED_CST_CAPTURE_EXPRESSION ||
         kind == W_SEED_CST_FOREIGN_BODY_OWNER;
}

static bool measure_document(const w_seed_frontend_document *doc,
                             frontend_measure *measure) {
  if (doc == NULL || measure == NULL) return false;
  measure->modules += 1;
  measure->imports += count_root_children(doc, W_SEED_CST_IMPORT);
  measure->structs += count_root_children(doc, W_SEED_CST_STRUCT);
  measure->generic_parameters += count_struct_generic_parameters(doc);
  measure->enums += count_root_children(doc, W_SEED_CST_ENUM);
  measure->type_declarations +=
      count_root_children(doc, W_SEED_CST_TYPE_DECLARATION);
  measure->aliases += count_root_children(doc, W_SEED_CST_ALIAS_DECLARATION);
  measure->functions += count_root_children(doc, W_SEED_CST_FUNCTION);
  measure->const_declarations +=
      count_root_children(doc, W_SEED_CST_CONST_DECLARATION);
  /* D7 gives every unannotated module const one source-free effective type
   * record.  Reserve those records in the same caller-owned capacity model as
   * CST type nodes; explicit annotations continue to count only once. */
  uint32_t root_cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t root_child = W_SEED_CST_NONE;
  size_t root_guard = 0u;
  while (next_child(doc, &root_cursor, &root_child) &&
         root_guard < doc->parse.node_count) {
    if (doc->nodes[root_child].kind == W_SEED_CST_CONST_DECLARATION &&
        direct_type_index(doc, root_child) == W_SEED_CST_NONE) {
      measure->types += 1u;
    }
    root_guard += 1u;
  }
  measure->entries += count_root_children(doc, W_SEED_CST_ENTRY);

  for (size_t index = 0; index < doc->parse.node_count; index += 1) {
    const w_seed_cst_kind kind = doc->nodes[index].kind;
    if (kind == W_SEED_CST_IMPORT_ITEM) measure->import_items += 1;
    if (kind == W_SEED_CST_FIELD) measure->fields += 1;
    if (kind == W_SEED_CST_PARAMETER) measure->parameters += 1;
    if (kind == W_SEED_CST_ENUM_CASE) measure->enum_cases += 1;
    if (kind == W_SEED_CST_ENUM_CASE_PARAMETER)
      measure->enum_case_parameters += 1;
    if (kind == W_SEED_CST_SWITCH_ARM) measure->switch_arms += 1;
    if (kind == W_SEED_CST_TYPE) measure->types += 1;
    if (kind == W_SEED_CST_ARGUMENT) measure->arguments += 1;
    if (kind_is_statement(kind)) measure->statements += 1;
    if (kind == W_SEED_CST_EXPRESSION) measure->expressions += 1;
    if (kind == W_SEED_CST_LET_STATEMENT ||
        kind == W_SEED_CST_VAR_STATEMENT || kind == W_SEED_CST_PARAMETER ||
        kind == W_SEED_CST_FIELD || kind == W_SEED_CST_FUNCTION ||
        kind == W_SEED_CST_STRUCT || kind == W_SEED_CST_TYPE_DECLARATION ||
        kind == W_SEED_CST_ALIAS_DECLARATION || kind == W_SEED_CST_ENTRY) {
      measure->symbols += 1;
    }
    if (kind == W_SEED_CST_CONST_DECLARATION) measure->symbols += 1;
    if (kind == W_SEED_CST_ENUM || kind == W_SEED_CST_ENUM_CASE)
      measure->symbols += 1;
    if (kind_is_unsupported_owner(kind)) measure->facts += 1;
  }
  /* Pratt trees contain a primary for each operator side. Reserve one AST
   * record for each CST expression plus its raw leaves. The final pass uses
   * the exact count and reports it in result.required. */
  if (measure->expressions != 0) {
    const size_t extra = measure->expressions;
    if (!add_size(measure->expressions, extra, &measure->expressions)) return false;
  }
  return true;
}

static bool import_edge_cycle_dfs(
    const w_seed_frontend_input *input, size_t document_index,
    uint8_t *states) {
  if (input == NULL || states == NULL ||
      document_index >= input->document_count) {
    return false;
  }
  if (states[document_index] == 1u) return true;
  if (states[document_index] == 2u) return false;
  states[document_index] = 1u;
  for (size_t edge_index = 0u; edge_index < input->resolved_import_count;
       edge_index += 1u) {
    const w_seed_frontend_resolved_import *edge =
        &input->resolved_imports[edge_index];
    if (edge->source_document_index != (uint32_t)document_index ||
        edge->target_kind != W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT) {
      continue;
    }
    if (import_edge_cycle_dfs(input, (size_t)edge->target_index, states)) {
      return true;
    }
  }
  states[document_index] = 2u;
  return false;
}

static bool validate_import_resolution(const w_seed_frontend_input *input,
                                       size_t *bad_document,
                                       w_seed_span *bad_span) {
  if (bad_document != NULL) *bad_document = W_SEED_FRONTEND_NONE_SIZE;
  if (bad_span != NULL) *bad_span = empty_span(0u);
  if (input == NULL || input->documents == NULL || input->document_count == 0u ||
      input->document_count > (size_t)W_SEED_FRONTEND_MAX_DOCUMENTS ||
      input->resolved_import_count > (size_t)UINT32_MAX ||
      input->resolved_import_count > (size_t)FRONTEND_MAX_IMPORTS ||
      (input->resolved_import_count != 0u && input->resolved_imports == NULL)) {
    return false;
  }
  if (!input->import_resolution_complete && input->resolved_import_count != 0u)
    return false;
  size_t expected_edges = 0u;
  for (size_t document_index = 0u; document_index < input->document_count;
       document_index += 1u) {
    const w_seed_frontend_document *doc = &input->documents[document_index];
    w_seed_module_scan_result scan_result;
    const w_seed_module_scan_status scan_status = w_seed_module_scan(
        doc->source, doc->nodes, doc->parse.node_count, &doc->parse,
        frontend_module_origins_scratch, FRONTEND_MAX_IMPORTS, &scan_result);
    if (scan_status != W_SEED_MODULE_SCAN_OK ||
        scan_result.required > (size_t)FRONTEND_MAX_IMPORTS) {
      if (bad_document != NULL) *bad_document = document_index;
      if (bad_span != NULL) *bad_span = owner_span(doc, doc->parse.root);
      return false;
    }
    const w_seed_frontend_text local_name = document_local_module_name(doc);
    if (local_name.length == 0u || local_name.data == NULL) {
      if (bad_document != NULL) *bad_document = document_index;
      if (bad_span != NULL) *bad_span = owner_span(doc, doc->parse.root);
      return false;
    }
    if (scan_result.has_module_header_name &&
        !text_equal_text(text_from_span(doc, scan_result.module_header_name_span),
                         local_name)) {
      if (bad_document != NULL) *bad_document = document_index;
      if (bad_span != NULL)
        *bad_span = scan_result.module_header_name_span;
      return false;
    }
    if (!input->import_resolution_complete) {
      continue;
    }
    for (size_t ordinal = 0u; ordinal < scan_result.required; ordinal += 1u) {
      if (expected_edges >= input->resolved_import_count) {
        if (bad_document != NULL) *bad_document = document_index;
        if (bad_span != NULL)
          *bad_span = frontend_module_origins_scratch[ordinal].declaration_span;
        return false;
      }
      const w_seed_frontend_resolved_import *edge =
          &input->resolved_imports[expected_edges];
      const w_seed_module_origin *origin = &frontend_module_origins_scratch[ordinal];
      if (edge->source_document_index != (uint32_t)document_index ||
          edge->direct_import_ordinal != (uint32_t)ordinal ||
          edge->import_declaration_span.start_byte !=
              origin->declaration_span.start_byte ||
          edge->import_declaration_span.end_byte !=
              origin->declaration_span.end_byte ||
          !w_seed_source_validate_span(doc->source,
                                        edge->import_declaration_span, NULL) ||
          (edge->target_kind !=
               W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT &&
           edge->target_kind !=
               W_SEED_FRONTEND_RESOLVED_IMPORT_EXTERNAL_MODULE)) {
        if (bad_document != NULL) *bad_document = document_index;
        if (bad_span != NULL) *bad_span = origin->declaration_span;
        return false;
      }
      if (edge->target_kind ==
          W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT) {
        if ((size_t)edge->target_index >= input->document_count ||
            edge->target_index == (uint32_t)document_index) {
          if (bad_document != NULL) *bad_document = document_index;
          if (bad_span != NULL) *bad_span = origin->declaration_span;
          return false;
        }
      } else if ((size_t)edge->target_index >= input->external_module_count) {
        if (bad_document != NULL) *bad_document = document_index;
        if (bad_span != NULL) *bad_span = origin->declaration_span;
        return false;
      }
      expected_edges += 1u;
    }
  }
  if (!input->import_resolution_complete) return true;
  if (expected_edges != input->resolved_import_count) {
    if (bad_document != NULL) *bad_document = input->document_count - 1u;
    if (bad_span != NULL) {
      const w_seed_frontend_document *last =
          &input->documents[input->document_count - 1u];
      *bad_span = owner_span(last, last->parse.root);
    }
    return false;
  }
  uint8_t states[W_SEED_FRONTEND_MAX_DOCUMENTS] = {0u};
  for (size_t document_index = 0u; document_index < input->document_count;
       document_index += 1u) {
    if (import_edge_cycle_dfs(input, document_index, states)) {
      if (bad_document != NULL) *bad_document = document_index;
      if (bad_span != NULL) *bad_span = owner_span(
          &input->documents[document_index],
          input->documents[document_index].parse.root);
      return false;
    }
  }
  return true;
}

static bool measure_input(const w_seed_frontend_input *input,
                          frontend_measure *measure, size_t *barrier_document,
                          w_seed_span *barrier_span) {
  if (measure == NULL || barrier_document == NULL || barrier_span == NULL) {
    return false;
  }
  (void)memset(measure, 0, sizeof(*measure));
  *barrier_document = W_SEED_FRONTEND_NONE_SIZE;
  *barrier_span = empty_span(0);
  if (input == NULL ||
      (input->document_count != 0 && input->documents == NULL) ||
      (input->external_module_count != 0 && input->external_modules == NULL) ||
      input->document_count == 0 ||
      input->document_count > (size_t)W_SEED_FRONTEND_MAX_DOCUMENTS ||
      input->document_count > (size_t)UINT32_MAX ||
      !external_input_ready(input) || !host_prelude_input_ready(input)) {
    return false;
  }
  size_t total_const_declarations = 0u;
  for (size_t index = 0; index < input->document_count; index += 1) {
    if (input->documents[index].node_count > (size_t)UINT32_MAX ||
        input->documents[index].parse.node_count >
            (size_t)UINT32_MAX / 2u) {
      return false;
    }
    size_t bad = 0;
    if (!document_ready(&input->documents[index], &bad)) {
      const w_seed_frontend_document *document = &input->documents[index];
      if (document->parse.status != W_SEED_PARSE_COMPLETE ||
          document->parse.issue_count != 0) {
        *barrier_document = index;
        *barrier_span = owner_span(document, document->parse.root);
      }
      return false;
    }
    if (!measure_document(&input->documents[index], measure)) return false;
    const size_t document_consts = count_root_children(
        &input->documents[index], W_SEED_CST_CONST_DECLARATION);
    if (!add_size(total_const_declarations, document_consts,
                 &total_const_declarations) ||
        total_const_declarations >
            (size_t)W_SEED_FRONTEND_MAX_CONST_DECLARATIONS) {
      *barrier_document = index;
      *barrier_span = owner_span(&input->documents[index],
                                 input->documents[index].parse.root);
      return false;
    }
  }
  if (!validate_import_resolution(input, NULL, NULL)) return false;
  /* This seed exposes one logical module record per document. Do not silently
   * merge multiple documents with the same resolved module identity. */
  for (size_t left = 0; left < input->document_count; left += 1) {
    const w_seed_frontend_text left_name =
        document_module_name(&input->documents[left]);
    for (size_t right = left + 1; right < input->document_count; right += 1) {
      if (left_name.length != 0 &&
          text_equal_text(left_name,
                          document_module_name(&input->documents[right]))) {
        return false;
      }
    }
    for (size_t external_index = 0;
         external_index < input->external_module_count; external_index += 1) {
      if (text_equal_text(left_name,
                          input->external_modules[external_index].module_id)) {
        return false;
      }
    }
  }
  /* A diagnostic upper bound is deterministic and caller-independent. The
   * semantic pass lowers it to the actual count before output is published. */
  measure->diagnostics = measure->expressions + measure->facts +
                         measure->generic_parameters + 8u;
  return true;
}

static void counts_from_measure(const frontend_measure *measure,
                                w_seed_frontend_counts *counts) {
  (void)memset(counts, 0, sizeof(*counts));
  counts->modules = measure->modules;
  counts->imports = measure->imports;
  counts->structs = measure->structs;
  counts->fields = measure->fields;
  counts->type_declarations = measure->type_declarations;
  counts->aliases = measure->aliases;
  counts->types = measure->types;
  counts->functions = measure->functions;
  counts->parameters = measure->parameters;
  counts->entries = measure->entries;
  counts->statements = measure->statements;
  counts->expressions = measure->expressions;
  counts->arguments = measure->arguments;
  counts->symbols = measure->symbols;
  counts->facts = measure->facts;
  counts->diagnostics = measure->diagnostics;
  counts->diagnostic_facts = measure->diagnostic_facts;
  counts->diagnostic_items = measure->diagnostic_items;
  counts->diagnostic_labels = measure->diagnostic_labels;
  counts->enums = measure->enums;
  counts->enum_cases = measure->enum_cases;
  counts->enum_case_parameters = measure->enum_case_parameters;
  counts->switch_arms = measure->switch_arms;
  counts->enum_subset_members = measure->enum_subset_members;
  counts->enum_membership_cases = measure->enum_membership_cases;
  counts->generic_parameters = measure->generic_parameters;
  counts->generic_applications = measure->generic_applications;
  counts->generic_arguments = measure->generic_arguments;
  counts->typed_const_expressions = measure->typed_const_expressions;
  counts->const_values = measure->const_values;
  counts->const_elements = measure->const_elements;
  counts->const_bytes = measure->const_bytes;
  counts->const_declarations = measure->const_declarations;
}

w_seed_frontend_status w_seed_frontend_measure(
    const w_seed_frontend_input *input, w_seed_frontend_counts *counts,
    w_seed_frontend_result *result) {
  frontend_measure measure;
  size_t barrier_document = W_SEED_FRONTEND_NONE_SIZE;
  w_seed_span barrier_span = empty_span(0);
  if (counts == NULL || result == NULL) return W_SEED_FRONTEND_INVALID;
  frontend_diagnostic_category_scratch_count = 0u;
  (void)memset(result, 0, sizeof(*result));
  result->schema_version = (w_seed_frontend_text){
      W_SEED_FRONTEND_SCHEMA_VERSION,
      sizeof(W_SEED_FRONTEND_SCHEMA_VERSION) - 1u};
  result->barrier_document = W_SEED_FRONTEND_NONE_SIZE;
  result->primary_diagnostic = W_SEED_FRONTEND_NONE_SIZE;
  if (!measure_input(input, &measure, &barrier_document, &barrier_span)) {
    result->status = barrier_document == W_SEED_FRONTEND_NONE_SIZE
                         ? W_SEED_FRONTEND_INVALID
                         : W_SEED_FRONTEND_BARRIER;
    result->barrier_document = barrier_document;
    result->barrier_span = barrier_span;
    (void)memset(counts, 0, sizeof(*counts));
    return result->status;
  }
  frontend_context dry;
  (void)memset(&dry, 0, sizeof(dry));
  dry.input = *input;
  dry.output = NULL;
  dry.result = result;
  dry.emit = false;
  dry.function_index = W_SEED_FRONTEND_NONE;
  dry.current_module_const = W_SEED_FRONTEND_NONE;
  dry.builtin_usize_type_index = W_SEED_FRONTEND_NONE;
  dry.const_inferred_types = frontend_const_inferred_types_scratch;
  dry.const_declared_type_indices =
      frontend_const_declared_type_indices_scratch;
  dry.const_inferred_type_indices =
      frontend_const_inferred_type_indices_scratch;
  dry.const_inference_states = frontend_const_inference_states_scratch;
  if (!initialize_const_document_bases(&dry)) {
    result->status = W_SEED_FRONTEND_BARRIER;
    result->barrier_document = W_SEED_FRONTEND_NONE_SIZE;
    return result->status;
  }
  (void)memset(dry.const_inferred_types, 0,
               sizeof(frontend_const_inferred_types_scratch));
  (void)memset(dry.const_declared_type_indices, 0xff,
               sizeof(frontend_const_declared_type_indices_scratch));
  (void)memset(dry.const_inferred_type_indices, 0xff,
               sizeof(frontend_const_inferred_type_indices_scratch));
  (void)memset(dry.const_inference_states, 0,
               sizeof(frontend_const_inference_states_scratch));
  (void)memset(dry.generic_domain_type_indices, 0xff,
               sizeof(dry.generic_domain_type_indices));
  if (!receipt_size_source_records(&dry)) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  if (!receipt_size_external_records(&dry)) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  if (!receipt_size_host_records(&dry)) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  for (size_t index = 0; index < input->document_count; index += 1) {
    dry.module_index = index;
    if (!normalize_document(&dry) || !detect_duplicate_declarations(&dry) ||
        !resolve_imports(&dry)) {
      result->status = W_SEED_FRONTEND_INVALID;
      return result->status;
    }
  }
  if (!resolve_pending_generic_applications(&dry)) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  measure = dry.count;
  measure.diagnostics = dry.count.diagnostics;
  counts_from_measure(&measure, counts);
  if (dry.receipt_overflow) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  counts->receipt_bytes = dry.receipt_size;
  result->required = *counts;
  result->required.receipt_bytes = counts->receipt_bytes;
  result->status = W_SEED_FRONTEND_OK;
  return result->status;
}

static const w_seed_frontend_document *context_document(
    const frontend_context *context) {
  if (context == NULL || context->module_index >= context->input.document_count) {
    return NULL;
  }
  return &context->input.documents[context->module_index];
}

static size_t context_document_index_for(
    const frontend_context *context,
    const w_seed_frontend_document *document) {
  if (context == NULL || document == NULL) return W_SEED_FRONTEND_NONE_SIZE;
  for (size_t index = 0u; index < context->input.document_count; index += 1u) {
    if (&context->input.documents[index] == document) return index;
  }
  return W_SEED_FRONTEND_NONE_SIZE;
}

static w_seed_frontend_text first_word_in_span(
    const w_seed_frontend_document *doc, w_seed_span span);
static w_seed_frontend_text name_after_keyword(
    const w_seed_frontend_document *doc, w_seed_span span,
    const char *keyword);
static uint32_t direct_type_index(const w_seed_frontend_document *doc,
                                  uint32_t owner);

/* Resolve enum declarations from the CST, not from the order in which the
 * normalizer happened to visit functions.  This makes forward declarations
 * usable while keeping the append-only enum/case indices authoritative. */
static bool enum_declaration_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    uint32_t *enum_index, uint32_t *type_index,
    const w_seed_frontend_document **owner_doc, uint32_t *enum_node) {
  if (enum_index != NULL) *enum_index = W_SEED_FRONTEND_NONE;
  if (type_index != NULL) *type_index = W_SEED_FRONTEND_NONE;
  if (owner_doc != NULL) *owner_doc = NULL;
  if (enum_node != NULL) *enum_node = W_SEED_CST_NONE;
  if (context == NULL) return false;
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return false;
  size_t ordinal = 0;
  for (size_t document_index = 0; document_index < context->module_index;
       document_index += 1) {
    const w_seed_frontend_document *prior =
        &context->input.documents[document_index];
    for (size_t node_index = 0; node_index < prior->parse.node_count;
         node_index += 1) {
      if (prior->nodes[node_index].kind == W_SEED_CST_ENUM) ordinal += 1;
    }
  }
  {
    uint32_t cursor = doc->nodes[doc->parse.root].first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t guard = 0;
    while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
      if (doc->nodes[child].kind == W_SEED_CST_ENUM) {
        const w_seed_frontend_text candidate =
            name_after_keyword(doc, doc->nodes[child].raw_span, "enum");
        if (text_equal_text(candidate, name)) {
          if (enum_index != NULL) {
            if (!add_u32(ordinal, enum_index)) return false;
          }
          if (owner_doc != NULL) *owner_doc = doc;
          if (enum_node != NULL) *enum_node = child;
          if (type_index != NULL && context->emit && context->output != NULL &&
              ordinal < context->count.enums &&
              context->output->enums != NULL) {
            *type_index = context->output->enums[ordinal].type_index;
          }
          return true;
        }
        ordinal += 1;
      }
      guard += 1;
    }
  }
  return false;
}

static bool enum_case_for_name(
    const frontend_context *context, uint32_t expected_enum,
    w_seed_frontend_text name, uint32_t *case_index,
    const w_seed_frontend_document **owner_doc, uint32_t *case_node) {
  if (case_index != NULL) *case_index = W_SEED_FRONTEND_NONE;
  if (owner_doc != NULL) *owner_doc = NULL;
  if (case_node != NULL) *case_node = W_SEED_CST_NONE;
  if (context == NULL || expected_enum == W_SEED_FRONTEND_NONE) return false;
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return false;
  size_t enum_ordinal = 0;
  size_t case_ordinal = 0;
  for (size_t document_index = 0; document_index < context->module_index;
       document_index += 1) {
    const w_seed_frontend_document *prior =
        &context->input.documents[document_index];
    for (size_t node_index = 0; node_index < prior->parse.node_count;
         node_index += 1) {
      if (prior->nodes[node_index].kind == W_SEED_CST_ENUM) enum_ordinal += 1;
      if (prior->nodes[node_index].kind == W_SEED_CST_ENUM_CASE)
        case_ordinal += 1;
    }
  }
  {
    uint32_t cursor = doc->nodes[doc->parse.root].first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t guard = 0;
    while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
      if (doc->nodes[child].kind == W_SEED_CST_ENUM) {
        uint32_t case_cursor = doc->nodes[child].first_child;
        uint32_t case_child = W_SEED_CST_NONE;
        size_t case_guard = 0;
        while (next_child(doc, &case_cursor, &case_child) &&
               case_guard < doc->parse.node_count) {
          if (doc->nodes[case_child].kind == W_SEED_CST_ENUM_CASE) {
            const w_seed_frontend_text candidate =
                first_word_in_span(doc, doc->nodes[case_child].raw_span);
            if (enum_ordinal == (size_t)expected_enum &&
                text_equal_text(candidate, name)) {
              if (case_index != NULL) {
                if (!add_u32(case_ordinal, case_index)) return false;
              }
              if (owner_doc != NULL) *owner_doc = doc;
              if (case_node != NULL) *case_node = case_child;
              return true;
            }
            case_ordinal += 1;
          }
          case_guard += 1;
        }
        enum_ordinal += 1;
      }
      guard += 1;
    }
  }
  return false;
}

static size_t enum_declaration_name_count(const frontend_context *context,
                                          w_seed_frontend_text name) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return 0;
  size_t count = 0;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_ENUM &&
        text_equal_text(name_after_keyword(doc, doc->nodes[child].raw_span,
                                           "enum"),
                        name)) {
      count += 1;
    }
    guard += 1;
  }
  return count;
}

static size_t alias_declaration_name_count(const frontend_context *context,
                                           w_seed_frontend_text name) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return 0;
  size_t count = 0;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_ALIAS_DECLARATION &&
        text_equal_text(name_after_keyword(doc, doc->nodes[child].raw_span,
                                           "alias"),
                        name)) {
      count += 1;
    }
    guard += 1;
  }
  return count;
}

/* Resolve only local struct heads.  The ordinal is derived from declaration
 * order in the CST, so a use before the struct declaration is stable. */
static bool struct_declaration_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    uint32_t *struct_index, const w_seed_frontend_document **owner_doc,
    uint32_t *struct_node) {
  if (struct_index != NULL) *struct_index = W_SEED_FRONTEND_NONE;
  if (owner_doc != NULL) *owner_doc = NULL;
  if (struct_node != NULL) *struct_node = W_SEED_CST_NONE;
  if (context == NULL || name.length == 0) return false;
  const w_seed_frontend_document *current = context_document(context);
  if (current == NULL) return false;
  size_t ordinal = 0;
  bool found = false;
  for (size_t document_index = 0;
       document_index < context->input.document_count; document_index += 1u) {
    const w_seed_frontend_document *doc =
        &context->input.documents[document_index];
    uint32_t cursor = doc->nodes[doc->parse.root].first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t guard = 0;
    while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
      if (doc->nodes[child].kind == W_SEED_CST_STRUCT) {
        const w_seed_frontend_text candidate =
            name_after_keyword(doc, doc->nodes[child].raw_span, "struct");
        if (module_id_equal(document_module_name(doc),
                            document_module_name(current)) &&
            text_equal_text(candidate, name)) {
          if (found) return false;
          found = true;
          if (!add_u32(ordinal, struct_index)) return false;
          if (owner_doc != NULL) *owner_doc = doc;
          if (struct_node != NULL) *struct_node = child;
        }
        ordinal += 1u;
      }
      guard += 1u;
    }
  }
  return found;
}

static bool enum_case_node_for_index(
    const frontend_context *context, uint32_t wanted_case,
    const w_seed_frontend_document **owner_doc, uint32_t *case_node) {
  if (owner_doc != NULL) *owner_doc = NULL;
  if (case_node != NULL) *case_node = W_SEED_CST_NONE;
  if (context == NULL || wanted_case == W_SEED_FRONTEND_NONE) return false;
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return false;
  size_t ordinal = 0;
  for (size_t document_index = 0; document_index < context->module_index;
       document_index += 1) {
    const w_seed_frontend_document *prior =
        &context->input.documents[document_index];
    for (size_t node_index = 0; node_index < prior->parse.node_count;
         node_index += 1) {
      if (prior->nodes[node_index].kind == W_SEED_CST_ENUM_CASE) ordinal += 1;
    }
  }
  {
    for (size_t node_index = 0; node_index < doc->parse.node_count;
         node_index += 1) {
      if (doc->nodes[node_index].kind != W_SEED_CST_ENUM_CASE) continue;
      if (ordinal == (size_t)wanted_case) {
        if (owner_doc != NULL) *owner_doc = doc;
        if (case_node != NULL) *case_node = (uint32_t)node_index;
        return true;
      }
      ordinal += 1;
    }
  }
  return false;
}

static bool enum_case_argument_expected(
    const frontend_context *context, uint32_t case_index, size_t ordinal,
    w_seed_frontend_text label, frontend_simple_type *expected,
    bool *label_valid, bool *label_previous) {
  const w_seed_frontend_document *doc = NULL;
  uint32_t case_node = W_SEED_CST_NONE;
  if (label_valid != NULL) *label_valid = false;
  if (label_previous != NULL) *label_previous = false;
  if (expected == NULL ||
      !enum_case_node_for_index(context, case_index, &doc, &case_node)) {
    return false;
  }
  uint32_t cursor = doc->nodes[case_node].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t position = 0;
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_ENUM_CASE_PARAMETER) {
      const w_seed_frontend_text parameter_label =
          enum_case_parameter_label(doc, child);
      /* Parameter slots are selected by source ordinal.  A label can only
       * validate that slot; it never reorders the constructor payload. */
      if (position == ordinal) {
        const uint32_t type_node = direct_type_index(doc, child);
        if (type_node != W_SEED_CST_NONE) {
          *expected = contextual_type_from_span(
              context, doc, doc->nodes[type_node].raw_span);
        }
        bool valid = false;
        bool previous = false;
        if (label.length == 0) {
          valid = parameter_label.length == 0;
        } else {
          valid = parameter_label.length != 0 &&
                  text_equal_text(parameter_label, label);
          if (!valid) {
            uint32_t prior_cursor = doc->nodes[case_node].first_child;
            uint32_t prior_child = W_SEED_CST_NONE;
            size_t prior_position = 0;
            size_t prior_guard = 0;
            while (next_child(doc, &prior_cursor, &prior_child) &&
                   prior_guard < doc->parse.node_count &&
                   prior_position < ordinal) {
              if (doc->nodes[prior_child].kind ==
                  W_SEED_CST_ENUM_CASE_PARAMETER) {
                const w_seed_frontend_text prior_label =
                    enum_case_parameter_label(doc, prior_child);
                if (prior_label.length != 0 &&
                    text_equal_text(prior_label, label)) {
                  previous = true;
                  break;
                }
                prior_position += 1;
              }
              prior_guard += 1;
            }
          }
        }
        if (label_valid != NULL) *label_valid = valid;
        if (label_previous != NULL) *label_previous = previous;
        return true;
      }
      position += 1;
    }
    guard += 1;
  }
  return false;
}

static size_t enum_case_parameter_count(const frontend_context *context,
                                        uint32_t case_index) {
  const w_seed_frontend_document *doc = NULL;
  uint32_t case_node = W_SEED_CST_NONE;
  if (!enum_case_node_for_index(context, case_index, &doc, &case_node)) return 0;
  return count_direct_kind(doc, case_node, W_SEED_CST_ENUM_CASE_PARAMETER);
}

static bool enum_subset_item_at(const w_seed_frontend_document *doc,
                                w_seed_span list_span, size_t wanted,
                                frontend_enum_subset_item *item) {
  if (doc == NULL || item == NULL || list_span.end_byte <= list_span.start_byte)
    return false;
  (void)memset(item, 0, sizeof(*item));
  frontend_token_cursor cursor = token_cursor_for(doc, list_span);
  frontend_token token;
  if (!cursor_take_text(&cursor, "[", &token)) return false;
  size_t ordinal = 0;
  while (true) {
    if (cursor_peek_text(&cursor, "]")) return false;
    frontend_token first;
    if (!cursor_take(&cursor, &first)) return false;
    frontend_enum_subset_item current;
    (void)memset(&current, 0, sizeof(current));
    if (token_text(doc, &first, ".")) {
      frontend_token member;
      if (!cursor_take(&cursor, &member) || member.kind != W_SEED_CST_WORD)
        return false;
      current.name = text_from_span(doc, member.span);
      current.span = (w_seed_span){first.span.start_byte, member.span.end_byte};
    } else if (first.kind == W_SEED_CST_WORD) {
      frontend_token dot;
      frontend_token member;
      if (!cursor_take(&cursor, &dot) || !token_text(doc, &dot, ".") ||
          !cursor_take(&cursor, &member) || member.kind != W_SEED_CST_WORD) {
        return false;
      }
      current.qualifier = text_from_span(doc, first.span);
      current.name = text_from_span(doc, member.span);
      current.span = (w_seed_span){first.span.start_byte, member.span.end_byte};
    } else {
      return false;
    }
    if (ordinal == wanted) {
      *item = current;
      return true;
    }
    ordinal += 1;
    if (cursor_peek_text(&cursor, ",")) {
      (void)cursor_take_text(&cursor, ",", NULL);
      if (cursor_peek_text(&cursor, "]")) return false;
      continue;
    }
    if (!cursor_peek_text(&cursor, "]")) return false;
    return false;
  }
}

static bool enum_subset_shape_for_type(
    const frontend_context *context, const w_seed_frontend_document *doc,
    uint32_t type_node, frontend_enum_subset_shape *shape) {
  if (shape == NULL) return false;
  (void)memset(shape, 0, sizeof(*shape));
  shape->enum_index = W_SEED_FRONTEND_NONE;
  shape->list_span = empty_span(0);
  if (context == NULL || doc == NULL || type_node >= doc->parse.node_count)
    return false;
  const w_seed_span raw = trim_span(doc, doc->nodes[type_node].raw_span);
  frontend_token_cursor cursor = token_cursor_for(doc, raw);
  frontend_token base;
  if (!cursor_take(&cursor, &base) || base.kind != W_SEED_CST_WORD) return false;
  shape->enum_name = text_from_span(doc, base.span);
  frontend_token next;
  if (!cursor_peek(&cursor, &next) || !token_text(doc, &next, "<")) {
    return false;
  }
  (void)cursor_take(&cursor, &next);
  /* A subset envelope is the deliberately narrow `Enum<[.case, ...]>`
   * shape.  Other generic/refinement envelopes (for example
   * `Batch<Row>` or `Base<T>`) must stay on the existing unsupported
   * contract path.  Do not turn their syntax failure into a subset
   * diagnostic. */
  if (!cursor_take_text(&cursor, "[", &next)) return false;
  shape->has_contract = true;
  const size_t list_start = next.span.start_byte;
  size_t item_count = 0;
  frontend_token_cursor list_cursor = cursor;
  if (!cursor_peek_text(&list_cursor, "]")) {
    while (true) {
      frontend_token first;
      if (!cursor_take(&list_cursor, &first)) return true;
      if (token_text(doc, &first, ".")) {
        frontend_token member;
        if (!cursor_take(&list_cursor, &member) ||
            member.kind != W_SEED_CST_WORD) return true;
      } else if (first.kind == W_SEED_CST_WORD) {
        frontend_token dot;
        frontend_token member;
        if (!cursor_take(&list_cursor, &dot) ||
            !token_text(doc, &dot, ".") ||
            !cursor_take(&list_cursor, &member) ||
            member.kind != W_SEED_CST_WORD) return true;
      } else {
        return true;
      }
      item_count += 1;
      frontend_token separator;
      if (cursor_peek_text(&list_cursor, ",")) {
        (void)cursor_take_text(&list_cursor, ",", &separator);
        if (cursor_peek_text(&list_cursor, "]")) break;
        continue;
      }
      if (!cursor_peek_text(&list_cursor, "]")) return true;
      break;
    }
  }
  cursor = list_cursor;
  if (!cursor_take_text(&cursor, "]", &next)) return true;
  shape->list_span = (w_seed_span){list_start, next.span.end_byte};
  if (!cursor_take_text(&cursor, ">", &next)) return true;
  if (cursor_peek(&cursor, &next)) return true;
  shape->item_count = item_count;
  if (enum_declaration_name_count(context, shape->enum_name) != 1u ||
      !enum_declaration_for_name(context, shape->enum_name, &shape->enum_index,
                                 NULL, NULL, NULL)) {
    return true;
  }
  if (item_count == 0) return true;
  for (size_t index = 0; index < item_count; index += 1) {
    frontend_enum_subset_item item;
    if (!enum_subset_item_at(doc, shape->list_span, index, &item)) return true;
    if (item.qualifier.length != 0 &&
        !text_equal_text(item.qualifier, shape->enum_name)) {
      return true;
    }
    uint32_t current_case = W_SEED_FRONTEND_NONE;
    if (!enum_case_for_name(context, shape->enum_index, item.name,
                            &current_case, NULL, NULL)) {
      return true;
    }
    for (size_t prior = 0; prior < index; prior += 1) {
      frontend_enum_subset_item previous;
      if (!enum_subset_item_at(doc, shape->list_span, prior, &previous)) {
        return true;
      }
      uint32_t previous_case = W_SEED_FRONTEND_NONE;
      if (enum_case_for_name(context, shape->enum_index, previous.name,
                             &previous_case, NULL, NULL) &&
          previous_case == current_case) {
        shape->duplicate = true;
        return true;
      }
    }
  }
  const w_seed_frontend_document *enum_doc = NULL;
  uint32_t enum_node = W_SEED_CST_NONE;
  if (!enum_declaration_for_name(context, shape->enum_name, NULL, NULL,
                                 &enum_doc, &enum_node)) {
    return true;
  }
  /* Generic enums are not closed base enums in this D0.  Keep their
   * contract envelope on the explicit invalid-subset path instead of
   * accidentally treating a type parameter as a concrete case-set owner. */
  if (first_direct_kind(enum_doc, enum_node,
                        W_SEED_CST_GENERIC_PARAMETERS) != W_SEED_CST_NONE) {
    return true;
  }
  const size_t enum_case_count =
      count_direct_kind(enum_doc, enum_node, W_SEED_CST_ENUM_CASE);
  if (item_count == enum_case_count) {
    bool all_present = true;
    uint32_t case_cursor = enum_doc->nodes[enum_node].first_child;
    uint32_t case_node = W_SEED_CST_NONE;
    size_t guard = 0;
    while (next_child(enum_doc, &case_cursor, &case_node) &&
           guard < enum_doc->parse.node_count) {
      if (enum_doc->nodes[case_node].kind == W_SEED_CST_ENUM_CASE) {
        const w_seed_frontend_text name =
            first_word_in_span(enum_doc, enum_doc->nodes[case_node].raw_span);
        uint32_t index = W_SEED_FRONTEND_NONE;
        if (!enum_case_for_name(context, shape->enum_index, name, &index, NULL,
                                NULL)) {
          all_present = false;
          break;
        }
        bool present = false;
        for (size_t item_index = 0; item_index < item_count; item_index += 1) {
          frontend_enum_subset_item item;
          if (enum_subset_item_at(doc, shape->list_span, item_index, &item) &&
              text_equal_text(item.name, name)) {
            present = true;
            break;
          }
        }
        if (!present) {
          all_present = false;
          break;
        }
      }
      guard += 1;
    }
    shape->full = all_present;
  }
  shape->valid = true;
  return true;
}

static bool enum_subset_shape_for_alias(
    const frontend_context *context, w_seed_frontend_text alias_name,
    frontend_enum_subset_shape *shape) {
  if (shape == NULL) return false;
  (void)memset(shape, 0, sizeof(*shape));
  shape->enum_index = W_SEED_FRONTEND_NONE;
  if (context == NULL || alias_name.length == 0) return false;
  if (alias_declaration_name_count(context, alias_name) != 1u) return false;
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return false;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_ALIAS_DECLARATION &&
        text_equal_text(name_after_keyword(doc, doc->nodes[child].raw_span,
                                           "alias"),
                        alias_name)) {
      const uint32_t type_node = direct_type_index(doc, child);
      if (type_node == W_SEED_CST_NONE) return false;
      return enum_subset_shape_for_type(context, doc, type_node, shape);
    }
    guard += 1;
  }
  return false;
}

static bool enum_subset_contains_case(
    const frontend_context *context, frontend_simple_type type,
    uint32_t enum_case_index) {
  if (type.kind == W_SEED_FRONTEND_TYPE_ENUM) return true;
  if (type.kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET || context == NULL)
    return false;
  const w_seed_frontend_document *doc = context_document(context);
  const w_seed_frontend_document *case_doc = NULL;
  uint32_t case_node = W_SEED_CST_NONE;
  if (doc == NULL ||
      !enum_case_node_for_index(context, enum_case_index, &case_doc,
                                &case_node)) {
    return false;
  }
  const w_seed_frontend_text name =
      first_word_in_span(case_doc, case_doc->nodes[case_node].raw_span);
  for (size_t index = 0; index < W_SEED_FRONTEND_MAX_CST_NODES; index += 1) {
    frontend_enum_subset_item item;
    if (!enum_subset_item_at(doc, type.subset_span, index, &item)) break;
    if (text_equal_text(item.name, name)) return true;
  }
  return false;
}

static frontend_simple_type contextual_type_from_span(
    const frontend_context *context, const w_seed_frontend_document *doc,
    w_seed_span span) {
  const w_seed_span trimmed = trim_span(doc, span);
  frontend_simple_type type = simple_type_from_text(doc, trimmed);
  const size_t document_index = context_document_index_for(context, doc);
  if (doc != NULL && document_index != W_SEED_FRONTEND_NONE_SIZE &&
      trimmed.start_byte <= trimmed.end_byte) {
    type.has_origin = true;
    type.origin_document_index = document_index;
    type.origin_span = trimmed;
  }
  if (type.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST) {
    (void)static_list_element_from_span(context, doc, span, &type);
  }
  if (type.kind == W_SEED_FRONTEND_TYPE_NOMINAL && context != NULL) {
    frontend_enum_subset_shape shape;
    if (enum_subset_shape_for_alias(context, type.spelling, &shape) &&
        shape.has_contract && shape.valid) {
      type.kind = shape.full ? W_SEED_FRONTEND_TYPE_ENUM
                             : W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
      type.enum_index = shape.enum_index;
      type.enum_name = shape.enum_name;
      type.enum_alias_name = type.spelling;
      type.subset_span = shape.list_span;
      return type;
    }
    uint32_t enum_index = W_SEED_FRONTEND_NONE;
    if (enum_declaration_name_count(context, type.spelling) == 1u &&
        enum_declaration_for_name(context, type.spelling, &enum_index, NULL,
                                  NULL, NULL)) {
      type.kind = W_SEED_FRONTEND_TYPE_ENUM;
      type.enum_index = enum_index;
      type.enum_name = type.spelling;
    }
  }
  return type;
}

static bool context_append_fact(frontend_context *context,
                                w_seed_frontend_fact_kind kind,
                                w_seed_span span, w_seed_frontend_text detail) {
  if (context == NULL) return false;
  if (context->current_function_is_const &&
      context->current_const_body_active &&
      (kind == W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE ||
       kind == W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE ||
       kind == W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION) &&
      !const_record_failure(context, span, detail)) {
    return false;
  }
  const size_t ordinal = context->count.facts;
  context->count.facts += 1;
  if (!context->emit) {
    const w_seed_frontend_fact value = {kind, detail, span,
                                        context->module_index};
    return receipt_size_fact(context, &value);
  }
  if (context->output == NULL || ordinal >= context->output->fact_capacity ||
      context->output->facts == NULL) {
    return false;
  }
  w_seed_frontend_fact *fact = &context->output->facts[ordinal];
  fact->kind = kind;
  fact->detail = detail;
  fact->span = span;
  fact->document_index = context->module_index;
  return true;
}

static bool context_append_diagnostic_raw_at(
    frontend_context *context, const char *code, size_t primary_document_index,
    w_seed_span primary, const frontend_diagnostic_fact_input *facts,
    size_t fact_count, const frontend_diagnostic_label_input *labels,
    size_t label_count) {
  if (context == NULL || code == NULL ||
      (fact_count != 0u && facts == NULL) ||
      (label_count != 0u && labels == NULL) ||
      primary_document_index >= context->input.document_count ||
      fact_count > (size_t)UINT32_MAX || label_count > (size_t)UINT32_MAX) {
    return false;
  }
  w_seed_source_error primary_error;
  if (context->input.documents[primary_document_index].source == NULL ||
      !w_seed_source_validate_span(
          context->input.documents[primary_document_index].source, primary,
          &primary_error)) {
    return false;
  }
  const size_t ordinal = context->count.diagnostics;
  const size_t first_fact = context->count.diagnostic_facts;
  const size_t first_label = context->count.diagnostic_labels;
  if (ordinal >= (size_t)UINT32_MAX || first_fact >= (size_t)UINT32_MAX ||
      first_label >= (size_t)UINT32_MAX) {
    return false;
  }
  size_t item_total = 0u;
  for (size_t index = 0u; index < fact_count; index += 1u) {
    const frontend_diagnostic_fact_input *fact = &facts[index];
    if (fact->kind > W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET ||
        fact->key.data == NULL || fact->key.length == 0u ||
        (fact->item_count != 0u && fact->items == NULL) ||
        fact->item_count > (size_t)UINT32_MAX ||
        (fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER &&
         (fact->text.data != NULL || fact->text.length != 0u ||
          fact->items != NULL || fact->item_count != 0u)) ||
        (fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING &&
         (fact->text.data == NULL || fact->text.length == 0u ||
          fact->integer_value != 0 || fact->items != NULL ||
          fact->item_count != 0u)) ||
        ((fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY ||
          fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET) &&
         (fact->text.data != NULL || fact->text.length != 0u ||
          fact->integer_value != 0))) {
      return false;
    }
    if (fact->kind != W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY &&
        fact->kind != W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET &&
        fact->items != NULL) {
      return false;
    }
    if (item_total > SIZE_MAX - fact->item_count) return false;
    item_total += fact->item_count;
    if (fact->kind != W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER &&
        fact->kind != W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING &&
        fact->text.length != 0u && fact->text.data == NULL) {
      return false;
    }
    for (size_t item = 0u; item < fact->item_count; item += 1u) {
      if (fact->items[item].data == NULL && fact->items[item].length != 0u) {
        return false;
      }
    }
    if (index != 0u) {
      const frontend_diagnostic_fact_input *prior = &facts[index - 1u];
      size_t key_index = 0u;
      const size_t common = prior->key.length < fact->key.length
                                ? prior->key.length
                                : fact->key.length;
      while (key_index < common &&
             (uint8_t)prior->key.data[key_index] ==
                 (uint8_t)fact->key.data[key_index]) {
        key_index += 1u;
      }
      if (key_index == common && prior->key.length >= fact->key.length) {
        return false;
      }
      if (key_index != common &&
          (uint8_t)prior->key.data[key_index] >
              (uint8_t)fact->key.data[key_index]) {
        return false;
      }
    }
  }
  for (size_t index = 0u; index < label_count; index += 1u) {
    const frontend_diagnostic_label_input *label = &labels[index];
    if (label->role.data == NULL || label->role.length == 0u) return false;
  }
  if (context->count.diagnostics == SIZE_MAX ||
      context->count.diagnostic_facts > SIZE_MAX - fact_count ||
      context->count.diagnostic_items > SIZE_MAX - item_total ||
      context->count.diagnostic_labels > SIZE_MAX - label_count) {
    return false;
  }
  if (context->count.diagnostic_items > (size_t)UINT32_MAX - item_total) {
    return false;
  }
  const w_seed_frontend_diagnostic value = {
      {code, strlen(code)}, primary, primary_document_index,
      (uint32_t)first_fact, (uint32_t)fact_count,
      (uint32_t)first_label, (uint32_t)label_count};
  if (context->result != NULL &&
      context->result->primary_diagnostic == W_SEED_FRONTEND_NONE_SIZE) {
    context->result->primary_diagnostic = ordinal;
  }
  if (!context->emit) {
    if (!receipt_size_diagnostic_input(context, ordinal, &value, facts,
                                       fact_count, labels, label_count)) {
      return false;
    }
    context->count.diagnostics += 1u;
    context->count.diagnostic_facts += fact_count;
    context->count.diagnostic_items += item_total;
    context->count.diagnostic_labels += label_count;
    return true;
  }
  if (context->output == NULL || context->output->diagnostics == NULL ||
      ordinal >= context->output->diagnostic_capacity ||
      (fact_count != 0u && context->output->diagnostic_facts == NULL) ||
      (item_total != 0u && context->output->diagnostic_items == NULL) ||
      (label_count != 0u && context->output->diagnostic_labels == NULL) ||
      first_fact + fact_count > context->output->diagnostic_fact_capacity ||
      context->count.diagnostic_items + item_total >
          context->output->diagnostic_item_capacity ||
      first_label + label_count > context->output->diagnostic_label_capacity) {
    return false;
  }
  w_seed_frontend_diagnostic *diagnostic =
      &context->output->diagnostics[ordinal];
  *diagnostic = value;
  for (size_t index = 0u; index < fact_count; index += 1u) {
    const frontend_diagnostic_fact_input *input_fact = &facts[index];
    w_seed_frontend_diagnostic_fact *fact =
        &context->output->diagnostic_facts[first_fact + index];
    fact->key = input_fact->key;
    fact->kind = input_fact->kind;
    fact->first_item = W_SEED_FRONTEND_NONE;
    fact->item_count = 0u;
    fact->text = (w_seed_frontend_text){NULL, 0u};
    fact->integer_value = 0;
    if (input_fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING) {
      fact->text = input_fact->text;
    } else if (input_fact->kind ==
               W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER) {
      fact->integer_value = input_fact->integer_value;
    } else {
      fact->item_count = (uint32_t)input_fact->item_count;
    }
    if (input_fact->item_count != 0u) {
      const size_t item_start = context->count.diagnostic_items;
      if (!add_u32(item_start, &fact->first_item)) return false;
      for (size_t item = 0u; item < input_fact->item_count; item += 1u) {
        context->output->diagnostic_items[item_start + item].text =
            input_fact->items[item];
      }
      context->count.diagnostic_items += input_fact->item_count;
    }
  }
  for (size_t index = 0u; index < label_count; index += 1u) {
    w_seed_frontend_diagnostic_label *label =
        &context->output->diagnostic_labels[first_label + index];
    label->role = labels[index].role;
    label->span = labels[index].span;
    label->document_index = labels[index].document_index;
  }
  context->count.diagnostics += 1u;
  context->count.diagnostic_facts += fact_count;
  context->count.diagnostic_labels += label_count;
  return true;
}

static bool context_append_diagnostic_raw(
    frontend_context *context, const char *code, w_seed_span primary,
    const frontend_diagnostic_fact_input *facts, size_t fact_count,
    const frontend_diagnostic_label_input *labels, size_t label_count) {
  return context_append_diagnostic_raw_at(
      context, code, context == NULL ? W_SEED_FRONTEND_NONE_SIZE
                                      : context->module_index,
      primary, facts, fact_count, labels, label_count);
}

static void diagnostic_label(frontend_context *context,
                             frontend_diagnostic_label_input *label,
                             const char *role, w_seed_span span) {
  label->role = (w_seed_frontend_text){role, strlen(role)};
  label->span = span;
  label->document_index = context->module_index;
}

static void diagnostic_label_at(frontend_diagnostic_label_input *label,
                                const char *role, w_seed_span span,
                                size_t document_index) {
  if (label == NULL) return;
  label->role = (w_seed_frontend_text){role, strlen(role)};
  label->span = span;
  label->document_index = document_index;
}

/* Resolve an exact source-backed occurrence when a semantic type carries no
 * origin. If no occurrence can be proven, the typed diagnostic fails closed. */
static bool diagnostic_find_text_span(const frontend_context *context,
                                      w_seed_frontend_text text,
                                      w_seed_span near,
                                      size_t *document_index,
                                      w_seed_span *span) {
  if (context == NULL || text.data == NULL || text.length == 0u ||
      span == NULL || document_index == NULL) {
    return false;
  }
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || doc->source == NULL || text.length > doc->source->bytes.length)
    return false;
  const size_t source_length = doc->source->bytes.length;
  size_t begin = near.start_byte < source_length ? near.start_byte : 0u;
  for (size_t pass = 0u; pass < 2u; pass += 1u) {
    const size_t first = pass == 0u ? begin : 0u;
    const size_t last = pass == 0u
                            ? source_length - text.length
                            : (begin < text.length ? 0u : begin - text.length);
    if (first > source_length - text.length) continue;
    for (size_t offset = first; offset <= last; offset += 1u) {
      if (memcmp(doc->source->bytes.data + offset, text.data, text.length) != 0)
        continue;
      const w_seed_span candidate = {offset, offset + text.length};
      w_seed_source_error source_error;
      if (!w_seed_source_validate_span(doc->source, candidate, &source_error))
        continue;
      *document_index = context->module_index;
      *span = candidate;
      return true;
    }
  }
  return false;
}

static int diagnostic_compare_text(w_seed_frontend_text left,
                                   w_seed_frontend_text right) {
  const size_t common = left.length < right.length ? left.length : right.length;
  if (common != 0u) {
    const int result = memcmp(left.data, right.data, common);
    if (result != 0) return result;
  }
  return left.length < right.length ? -1 : left.length > right.length ? 1 : 0;
}

static bool diagnostic_enum_cases(
    const frontend_context *context, frontend_simple_type expected,
    w_seed_frontend_text *items, size_t item_capacity, size_t *item_count) {
  if (item_count != NULL) *item_count = 0u;
  if (context == NULL || items == NULL || item_count == NULL ||
      item_capacity == 0u || !frontend_type_is_enum(expected)) {
    return false;
  }
  const w_seed_frontend_document *doc = context_document(context);
  if (expected.has_origin &&
      expected.origin_document_index < context->input.document_count) {
    doc = &context->input.documents[expected.origin_document_index];
  }
  if (doc == NULL) return false;
  if (expected.kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET &&
      expected.subset_span.end_byte > expected.subset_span.start_byte) {
    for (size_t ordinal = 0u; ordinal < item_capacity; ordinal += 1u) {
      frontend_enum_subset_item item;
      if (!enum_subset_item_at(doc, expected.subset_span, ordinal, &item))
        break;
      items[*item_count] = item.name;
      *item_count += 1u;
    }
  } else {
    const w_seed_frontend_document *enum_doc = NULL;
    uint32_t enum_node = W_SEED_CST_NONE;
    if (!enum_declaration_for_name(context, expected.enum_name.length != 0u
                                             ? expected.enum_name
                                             : expected.spelling,
                                   NULL, NULL, &enum_doc, &enum_node) ||
        enum_doc == NULL) {
      return false;
    }
    uint32_t cursor = enum_doc->nodes[enum_node].first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t guard = 0u;
    while (*item_count < item_capacity &&
           next_child(enum_doc, &cursor, &child) &&
           guard < enum_doc->parse.node_count) {
      if (enum_doc->nodes[child].kind == W_SEED_CST_ENUM_CASE) {
        const w_seed_frontend_text name =
            first_word_in_span(enum_doc, enum_doc->nodes[child].raw_span);
        if (name.length != 0u) {
          items[*item_count] = name;
          *item_count += 1u;
        }
      }
      guard += 1u;
    }
  }
  /* Sets use byte order, independent of the declaration's semantic order. */
  for (size_t index = 1u; index < *item_count; index += 1u) {
    const w_seed_frontend_text value = items[index];
    size_t insert = index;
    while (insert != 0u &&
           diagnostic_compare_text(items[insert - 1u], value) > 0) {
      items[insert] = items[insert - 1u];
      insert -= 1u;
    }
    items[insert] = value;
  }
  size_t unique = 0u;
  for (size_t index = 0u; index < *item_count; index += 1u) {
    if (unique == 0u ||
        diagnostic_compare_text(items[unique - 1u], items[index]) != 0) {
      items[unique] = items[index];
      unique += 1u;
    }
  }
  *item_count = unique;
  return true;
}

static bool append_type0121_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text actual_case, frontend_simple_type expected) {
  if (context == NULL || actual_case.data == NULL || actual_case.length == 0u ||
      expected.spelling.data == NULL || expected.spelling.length == 0u)
    return false;
  w_seed_frontend_text items[FRONTEND_MAX_DIAGNOSTIC_ITEMS];
  size_t item_count = 0u;
  if (!diagnostic_enum_cases(context, expected, items,
                             sizeof(items) / sizeof(items[0]), &item_count))
    return false;
  const w_seed_frontend_text base_enum =
      expected.enum_name.length != 0u ? expected.enum_name : expected.spelling;
  if (base_enum.data == NULL || base_enum.length == 0u) return false;
  const w_seed_frontend_text expected_type = expected.spelling;
  frontend_diagnostic_fact_input facts[4];
  (void)memset(facts, 0, sizeof(facts));
  facts[0] = (frontend_diagnostic_fact_input){{"actualCase", 10u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               actual_case, 0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"allowedCases", 12u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET,
                                               {NULL, 0u}, 0, items, item_count};
  facts[2] = (frontend_diagnostic_fact_input){{"baseEnum", 8u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               base_enum, 0, NULL, 0u};
  facts[3] = (frontend_diagnostic_fact_input){{"expectedType", 12u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               expected_type, 0, NULL, 0u};
  frontend_diagnostic_label_input label;
  (void)memset(&label, 0, sizeof(label));
  size_t document_index = context->module_index;
  w_seed_span label_span = primary;
  if (expected.has_origin &&
      expected.origin_document_index < context->input.document_count) {
    document_index = expected.origin_document_index;
    label_span = expected.origin_span;
  } else {
    if (!diagnostic_find_text_span(context, expected_type, primary,
                                   &document_index, &label_span))
      return false;
    diagnostic_label_at(&label, "expected-type", label_span, document_index);
    return context_append_diagnostic_raw(context, "W-TYPE-0121", primary,
                                         facts, 4u, &label, 1u);
  }
  diagnostic_label_at(&label, "expected-type", label_span, document_index);
  return context_append_diagnostic_raw(context, "W-TYPE-0121", primary, facts,
                                       4u, &label, 1u);
}

static bool append_type0122_diagnostic(
    frontend_context *context, w_seed_span primary,
    frontend_simple_type actual, frontend_simple_type expected,
    w_seed_frontend_text reason) {
  if (context == NULL || actual.spelling.data == NULL ||
      actual.spelling.length == 0u || expected.spelling.data == NULL ||
      expected.spelling.length == 0u || reason.data == NULL ||
      reason.length == 0u)
    return false;
  const w_seed_frontend_text actual_type = actual.spelling;
  const w_seed_frontend_text expected_type = expected.spelling;
  const w_seed_frontend_text stable_reason = reason;
  frontend_diagnostic_fact_input facts[4];
  (void)memset(facts, 0, sizeof(facts));
  facts[0] = (frontend_diagnostic_fact_input){{"actualType", 10u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               actual_type, 0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"candidateRoutes", 15u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET,
                                               {NULL, 0u}, 0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"expectedType", 12u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               expected_type, 0, NULL, 0u};
  facts[3] = (frontend_diagnostic_fact_input){{"reason", 6u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               stable_reason, 0, NULL, 0u};
  frontend_diagnostic_label_input labels[2];
  (void)memset(labels, 0, sizeof(labels));
  diagnostic_label(context, &labels[0], "call-owner", primary);
  size_t expected_document = context->module_index;
  w_seed_span expected_span = primary;
  if (expected.has_origin &&
      expected.origin_document_index < context->input.document_count) {
    expected_document = expected.origin_document_index;
    expected_span = expected.origin_span;
    diagnostic_label_at(&labels[1], "expected-type", expected_span,
                        expected_document);
  } else {
    size_t expected_document_index = context->module_index;
    w_seed_span expected_type_span = primary;
    if (!diagnostic_find_text_span(context, expected_type, primary,
                                   &expected_document_index,
                                   &expected_type_span))
      return false;
    diagnostic_label_at(&labels[1], "expected-type", expected_type_span,
                        expected_document_index);
  }
  return context_append_diagnostic_raw(context, "W-TYPE-0122", primary, facts,
                                       4u, labels, 2u);
}

static bool append_type0120_diagnostic(
    frontend_context *context, w_seed_span primary,
    frontend_simple_type left, w_seed_span left_span, size_t left_document,
    frontend_simple_type right, w_seed_span right_span,
    size_t right_document) {
  if (context == NULL || left.spelling.data == NULL ||
      left.spelling.length == 0u || right.spelling.data == NULL ||
      right.spelling.length == 0u ||
      left_document >= context->input.document_count ||
      right_document >= context->input.document_count)
    return false;
  const w_seed_frontend_text left_type = left.spelling;
  const w_seed_frontend_text right_type = right.spelling;
  frontend_diagnostic_fact_input facts[2];
  facts[0] = (frontend_diagnostic_fact_input){{"leftType", 8u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               left_type, 0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"rightType", 9u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               right_type, 0, NULL, 0u};
  frontend_diagnostic_label_input labels[2];
  diagnostic_label_at(&labels[0], "branch-result", left_span, left_document);
  diagnostic_label_at(&labels[1], "branch-result", right_span, right_document);
  return context_append_diagnostic_raw(context, "W-TYPE-0120", primary, facts,
                                       2u, labels, 2u);
}

static bool append_match0001_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text subject_type, w_seed_span subject_span,
    const w_seed_frontend_text *missing, size_t missing_count) {
  if (context == NULL || subject_type.data == NULL ||
      subject_type.length == 0u || missing == NULL || missing_count == 0u)
    return false;
  w_seed_frontend_text sorted[FRONTEND_MAX_DIAGNOSTIC_ITEMS];
  if (missing_count > sizeof(sorted) / sizeof(sorted[0])) return false;
  for (size_t index = 0u; index < missing_count; index += 1u) {
    if (missing[index].data == NULL || missing[index].length == 0u)
      return false;
    sorted[index] = missing[index];
  }
  for (size_t index = 1u; index < missing_count; index += 1u) {
    const w_seed_frontend_text value = sorted[index];
    size_t insert = index;
    while (insert != 0u &&
           diagnostic_compare_text(sorted[insert - 1u], value) > 0) {
      sorted[insert] = sorted[insert - 1u];
      insert -= 1u;
    }
    sorted[insert] = value;
  }
  size_t unique = 0u;
  for (size_t index = 0u; index < missing_count; index += 1u) {
    if (unique == 0u ||
        diagnostic_compare_text(sorted[unique - 1u], sorted[index]) != 0) {
      sorted[unique] = sorted[index];
      unique += 1u;
    }
  }
  const w_seed_frontend_text type_text = subject_type;
  frontend_diagnostic_fact_input facts[2];
  facts[0] = (frontend_diagnostic_fact_input){{"missingCases", 12u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET,
                                               {NULL, 0u}, 0, sorted, unique};
  facts[1] = (frontend_diagnostic_fact_input){{"subjectType", 11u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               type_text, 0, NULL, 0u};
  frontend_diagnostic_label_input label;
  diagnostic_label(context, &label, "match-subject", subject_span);
  return context_append_diagnostic_raw(context, "W-MATCH-0001", primary, facts,
                                       2u, &label, 1u);
}

static bool append_match0002_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text covered_by, w_seed_frontend_text pattern,
    w_seed_frontend_text subject_type, w_seed_span covered_span,
    size_t covered_document, w_seed_span subject_span) {
  if (context == NULL || covered_by.data == NULL || covered_by.length == 0u ||
      pattern.data == NULL || pattern.length == 0u ||
      subject_type.data == NULL || subject_type.length == 0u)
    return false;
  const w_seed_frontend_text covered = covered_by;
  const w_seed_frontend_text current = pattern;
  const w_seed_frontend_text subject = subject_type;
  frontend_diagnostic_fact_input facts[3];
  facts[0] = (frontend_diagnostic_fact_input){{"coveredBy", 9u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               covered, 0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"pattern", 7u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               current, 0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"subjectType", 11u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               subject, 0, NULL, 0u};
  frontend_diagnostic_label_input labels[2];
  diagnostic_label_at(&labels[0], "covered-case", covered_span,
                      covered_document);
  diagnostic_label(context, &labels[1], "match-subject", subject_span);
  return context_append_diagnostic_raw(context, "W-MATCH-0002", primary, facts,
                                       3u, labels, 2u);
}

static bool append_sem0001_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text actual, w_seed_frontend_text expected) {
  if (context == NULL || actual.data == NULL || actual.length == 0u ||
      expected.data == NULL || expected.length == 0u)
    return false;
  const w_seed_frontend_text actual_text = actual;
  const w_seed_frontend_text expected_text = expected;
  frontend_diagnostic_fact_input facts[2];
  facts[0] = (frontend_diagnostic_fact_input){{"actual", 6u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               actual_text, 0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"expected", 8u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               expected_text, 0, NULL, 0u};
  return context_append_diagnostic_raw(context, "W-SEM-0001", primary, facts,
                                       2u, NULL, 0u);
}

static bool diagnostic_form_push(w_seed_frontend_text *forms,
                                 size_t form_capacity, size_t *form_count,
                                 w_seed_frontend_text form) {
  if (forms == NULL || form_count == NULL || form.length == 0u) return false;
  if (*form_count >= form_capacity) return false;
  forms[*form_count] = form;
  *form_count += 1u;
  return true;
}

static w_seed_frontend_text diagnostic_enum_case_name(
    const frontend_context *context, uint32_t enum_case_index) {
  const w_seed_frontend_document *document = NULL;
  uint32_t case_node = W_SEED_CST_NONE;
  if (context == NULL ||
      !enum_case_node_for_index(context, enum_case_index, &document,
                                &case_node) ||
      document == NULL)
    return (w_seed_frontend_text){NULL, 0u};
  return first_word_in_span(document, document->nodes[case_node].raw_span);
}

/* Return the accepted form for the selected declaration slot.  Labels are
 * source slices; only an omitted label is represented by the normalized
 * semantic marker `positional`. */
static size_t diagnostic_call_accepted_forms(
    const frontend_context *context, bool enum_case_constructor,
    uint32_t enum_case_index, const w_seed_frontend_document *signature_doc,
    uint32_t signature_node,
    const w_seed_frontend_external_symbol *external_signature,
    const w_seed_frontend_host_prelude_symbol *host_signature, size_t ordinal,
    w_seed_frontend_text *forms, size_t form_capacity) {
  if (forms == NULL || form_capacity == 0u) return 0u;
  size_t form_count = 0u;
  const w_seed_frontend_text positional = {"positional", 10u};
  if (enum_case_constructor) {
    const w_seed_frontend_document *case_doc = NULL;
    uint32_t case_node = W_SEED_CST_NONE;
    if (context == NULL ||
        !enum_case_node_for_index(context, enum_case_index, &case_doc,
                                  &case_node) ||
        case_doc == NULL) {
      return 0u;
    }
    uint32_t cursor = case_doc->nodes[case_node].first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t position = 0u;
    size_t guard = 0u;
    while (next_child(case_doc, &cursor, &child) &&
           guard < case_doc->parse.node_count) {
      if (case_doc->nodes[child].kind == W_SEED_CST_ENUM_CASE_PARAMETER) {
        if (position == ordinal) {
          const w_seed_frontend_text label =
              enum_case_parameter_label(case_doc, child);
          (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                     label.length != 0u ? label : positional);
          return form_count;
        }
        position += 1u;
      }
      guard += 1u;
    }
    return form_count;
  }
  if (signature_doc != NULL && signature_node != W_SEED_CST_NONE) {
    const uint32_t parameters = first_direct_kind(
        signature_doc, signature_node, W_SEED_CST_PARAMETER_LIST);
    if (parameters == W_SEED_CST_NONE) return 0u;
    uint32_t cursor = signature_doc->nodes[parameters].first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t position = 0u;
    size_t guard = 0u;
    while (next_child(signature_doc, &cursor, &child) &&
           guard < signature_doc->parse.node_count) {
      if (signature_doc->nodes[child].kind == W_SEED_CST_PARAMETER) {
        if (position == ordinal) {
          const w_seed_frontend_label_kind policy = parameter_label_kind(
              signature_doc, signature_doc->nodes[child].raw_span);
          const w_seed_frontend_text label = parameter_external_label_from_span(
              signature_doc, signature_doc->nodes[child].raw_span);
          if (policy == W_SEED_FRONTEND_LABEL_OPTIONAL) {
            (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                       positional);
            (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                       label);
          } else if (policy == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY) {
            (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                       positional);
          } else {
            (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                       label.length != 0u ? label : positional);
          }
          return form_count;
        }
        position += 1u;
      }
      guard += 1u;
    }
    return form_count;
  }
  if (external_signature != NULL && ordinal < external_signature->parameter_count) {
    const w_seed_frontend_external_parameter *parameter =
        &external_signature->parameters[ordinal];
    if (parameter->label_kind == W_SEED_FRONTEND_LABEL_OPTIONAL) {
      (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                 positional);
      (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                 parameter->name);
    } else if (parameter->label_kind ==
               W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY) {
      (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                 positional);
    } else {
      (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                 parameter->name.length != 0u
                                     ? parameter->name
                                     : positional);
    }
  }
  if (host_signature != NULL && ordinal < host_signature->parameter_count) {
    const w_seed_frontend_external_parameter *parameter =
        &host_signature->parameters[ordinal];
    if (parameter->label_kind == W_SEED_FRONTEND_LABEL_OPTIONAL) {
      (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                 positional);
      (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                 parameter->name);
    } else if (parameter->label_kind ==
               W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY) {
      (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                 positional);
    } else {
      (void)diagnostic_form_push(forms, form_capacity, &form_count,
                                 parameter->name.length != 0u
                                     ? parameter->name
                                     : positional);
    }
  }
  return form_count;
}

static bool append_label0005_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text declaration, w_seed_frontend_text label,
    const w_seed_frontend_text *accepted_forms, size_t accepted_count) {
  if (context == NULL || declaration.data == NULL || declaration.length == 0u ||
      (label.length != 0u && label.data == NULL))
    return false;
  w_seed_frontend_text fallback = {"positional", 10u};
  if (accepted_count == 0u) {
    accepted_forms = &fallback;
    accepted_count = 1u;
  }
  if (accepted_forms == NULL || accepted_count == 0u ||
      accepted_count > FRONTEND_MAX_DIAGNOSTIC_ITEMS)
    return false;
  for (size_t index = 0u; index < accepted_count; index += 1u) {
    if (accepted_forms[index].data == NULL ||
        accepted_forms[index].length == 0u)
      return false;
  }
  frontend_diagnostic_fact_input facts[3];
  facts[0] = (frontend_diagnostic_fact_input){{"acceptedForms", 13u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY,
                                               {NULL, 0u}, 0, accepted_forms,
                                               accepted_count};
  facts[1] = (frontend_diagnostic_fact_input){{"declaration", 11u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               declaration,
                                               0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"label", 5u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               label.length != 0u ? label : fallback,
                                               0, NULL, 0u};
  return context_append_diagnostic_raw(context, "W-LABEL-0005", primary,
                                       facts, 3u, NULL, 0u);
}

static bool append_label0006_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text declaration, w_seed_frontend_text label,
    w_seed_frontend_text slot) {
  if (context == NULL || declaration.data == NULL || declaration.length == 0u ||
      slot.data == NULL || slot.length == 0u ||
      (label.length != 0u && label.data == NULL))
    return false;
  const w_seed_frontend_text positional = {"positional", 10u};
  const w_seed_frontend_text slot_text = slot;
  frontend_diagnostic_fact_input facts[3];
  facts[0] = (frontend_diagnostic_fact_input){{"declaration", 11u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               declaration,
                                               0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"label", 5u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               label.length != 0u ? label : positional,
                                               0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"slot", 4u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               slot_text, 0, NULL, 0u};
  return context_append_diagnostic_raw(context, "W-LABEL-0006", primary,
                                       facts, 3u, NULL, 0u);
}

static bool append_match0003_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text context_name, w_seed_frontend_text expected_type,
    w_seed_frontend_text member) {
  if (context == NULL || context_name.data == NULL ||
      context_name.length == 0u || expected_type.data == NULL ||
      expected_type.length == 0u || member.data == NULL || member.length == 0u)
    return false;
  const w_seed_frontend_text context_text = context_name;
  const w_seed_frontend_text expected_text = expected_type;
  const w_seed_frontend_text member_text = member;
  frontend_diagnostic_fact_input facts[3];
  facts[0] = (frontend_diagnostic_fact_input){{"context", 7u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               context_text, 0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"expectedType", 12u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               expected_text, 0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"member", 6u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               member_text, 0, NULL, 0u};
  return context_append_diagnostic_raw(context, "W-MATCH-0003", primary,
                                       facts, 3u, NULL, 0u);
}

static bool append_const0001_diagnostic(
    frontend_context *context, w_seed_span primary,
    const w_seed_frontend_text *call_chain, size_t call_chain_count,
    w_seed_frontend_text operation, w_seed_frontend_text reason,
    w_seed_frontend_text symbol, size_t owner_document, w_seed_span owner_span) {
  if (context == NULL || call_chain == NULL || call_chain_count == 0u ||
      call_chain_count > FRONTEND_MAX_DIAGNOSTIC_ITEMS ||
      operation.data == NULL || operation.length == 0u || reason.data == NULL ||
      reason.length == 0u || symbol.data == NULL || symbol.length == 0u ||
      owner_document >= context->input.document_count)
    return false;
  for (size_t index = 0u; index < call_chain_count; index += 1u) {
    if (call_chain[index].data == NULL || call_chain[index].length == 0u)
      return false;
  }
  frontend_diagnostic_fact_input facts[4];
  facts[0] = (frontend_diagnostic_fact_input){{"callChain", 9u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY,
                                               {NULL, 0u}, 0, call_chain,
                                               call_chain_count};
  facts[1] = (frontend_diagnostic_fact_input){{"operation", 9u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               operation, 0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"reason", 6u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               reason, 0, NULL, 0u};
  facts[3] = (frontend_diagnostic_fact_input){{"symbol", 6u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               symbol, 0, NULL, 0u};
  frontend_diagnostic_label_input label;
  diagnostic_label_at(&label, "const-owner", owner_span, owner_document);
  return context_append_diagnostic_raw(context, "W-CONST-0001", primary,
                                       facts, 4u, &label, 1u);
}

static bool append_contract0002_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text actual_kind, w_seed_frontend_text expected_kind,
    w_seed_frontend_text head, size_t head_document, w_seed_span head_span,
    w_seed_frontend_text slot, size_t slot_document, w_seed_span slot_span) {
  if (context == NULL || actual_kind.data == NULL || actual_kind.length == 0u ||
      expected_kind.data == NULL || expected_kind.length == 0u ||
      head.data == NULL || head.length == 0u || slot.data == NULL ||
      slot.length == 0u ||
      head_document >= context->input.document_count ||
      slot_document >= context->input.document_count)
    return false;
  frontend_diagnostic_fact_input facts[4];
  facts[0] = (frontend_diagnostic_fact_input){{"actualKind", 10u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               actual_kind, 0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"expectedKind", 12u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               expected_kind, 0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"head", 4u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               head, 0, NULL, 0u};
  facts[3] = (frontend_diagnostic_fact_input){{"slot", 4u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               slot, 0, NULL, 0u};
  frontend_diagnostic_label_input labels[2];
  diagnostic_label_at(&labels[0], "contract-head", head_span, head_document);
  diagnostic_label_at(&labels[1], "slot-declaration", slot_span,
                      slot_document);
  return context_append_diagnostic_raw(context, "W-CONTRACT-0002", primary,
                                       facts, 4u, labels, 2u);
}

static bool append_contract0003_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text expected_type, w_seed_frontend_text head,
    size_t head_document, w_seed_span head_span,
    w_seed_frontend_text predicate_type, size_t slot_document,
    w_seed_span slot_span) {
  if (context == NULL || expected_type.data == NULL ||
      expected_type.length == 0u || head.data == NULL || head.length == 0u ||
      predicate_type.data == NULL || predicate_type.length == 0u ||
      head_document >= context->input.document_count ||
      slot_document >= context->input.document_count)
    return false;
  frontend_diagnostic_fact_input facts[3];
  facts[0] = (frontend_diagnostic_fact_input){{"expectedType", 12u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               expected_type, 0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"head", 4u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               head, 0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"predicateType", 13u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               predicate_type, 0, NULL, 0u};
  frontend_diagnostic_label_input labels[2];
  diagnostic_label_at(&labels[0], "contract-head", head_span, head_document);
  diagnostic_label_at(&labels[1], "slot-declaration", slot_span,
                      slot_document);
  return context_append_diagnostic_raw(context, "W-CONTRACT-0003", primary,
                                       facts, 3u, labels, 2u);
}

static bool append_contract0004_diagnostic(
    frontend_context *context, w_seed_span primary, w_seed_frontend_text head,
    size_t head_document, w_seed_span head_span, w_seed_frontend_text slot,
    size_t slot_document, w_seed_span slot_span,
    const w_seed_frontend_text *slot_order, size_t slot_order_count,
    w_seed_frontend_text violation) {
  if (context == NULL || head.data == NULL || head.length == 0u ||
      slot.data == NULL || slot.length == 0u ||
      slot_order == NULL || slot_order_count == 0u ||
      slot_order_count > FRONTEND_MAX_DIAGNOSTIC_ITEMS ||
      violation.data == NULL || violation.length == 0u ||
      head_document >= context->input.document_count ||
      slot_document >= context->input.document_count)
    return false;
  for (size_t index = 0u; index < slot_order_count; index += 1u) {
    if (slot_order[index].data == NULL || slot_order[index].length == 0u)
      return false;
  }
  frontend_diagnostic_fact_input facts[4];
  facts[0] = (frontend_diagnostic_fact_input){{"head", 4u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               head, 0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"slot", 4u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               slot, 0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"slotOrder", 9u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_ARRAY,
                                               {NULL, 0u}, 0, slot_order,
                                               slot_order_count};
  facts[3] = (frontend_diagnostic_fact_input){{"violation", 9u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               violation, 0, NULL, 0u};
  frontend_diagnostic_label_input labels[2];
  diagnostic_label_at(&labels[0], "contract-head", head_span, head_document);
  diagnostic_label_at(&labels[1], "slot-declaration", slot_span,
                      slot_document);
  return context_append_diagnostic_raw(context, "W-CONTRACT-0004", primary,
                                       facts, 4u, labels, 2u);
}

static bool append_generic0001_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text domain, w_seed_frontend_text parameter,
    w_seed_frontend_text resolution_reason, size_t parameter_document,
    w_seed_span parameter_span) {
  if (context == NULL || domain.data == NULL || domain.length == 0u ||
      parameter.data == NULL || parameter.length == 0u ||
      resolution_reason.data == NULL || resolution_reason.length == 0u ||
      parameter_document >= context->input.document_count)
    return false;
  frontend_diagnostic_fact_input facts[3];
  facts[0] = (frontend_diagnostic_fact_input){{"domain", 6u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               domain, 0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"parameter", 9u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               parameter, 0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"resolutionReason", 16u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               resolution_reason, 0, NULL, 0u};
  frontend_diagnostic_label_input label;
  diagnostic_label_at(&label, "generic-parameter", parameter_span,
                      parameter_document);
  return context_append_diagnostic_raw(context, "W-GENERIC-0001", primary,
                                       facts, 3u, &label, 1u);
}

static bool append_contract0001_diagnostic(
    frontend_context *context, w_seed_span primary,
    const w_seed_frontend_text *available_slots, size_t available_count,
    w_seed_frontend_text head, size_t head_document, w_seed_span head_span,
    w_seed_frontend_text slot) {
  if (context == NULL || head.length == 0u || head.data == NULL ||
      slot.length == 0u || slot.data == NULL ||
      available_count > W_SEED_FRONTEND_MAX_GENERIC_SLOTS ||
      (available_count != 0u && available_slots == NULL) ||
      head_document >= context->input.document_count) {
    return false;
  }
  w_seed_frontend_text sorted[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  for (size_t index = 0u; index < available_count; index += 1u) {
    if (available_slots[index].data == NULL ||
        available_slots[index].length == 0u)
      return false;
    sorted[index] = available_slots[index];
  }
  for (size_t index = 1u; index < available_count; index += 1u) {
    const w_seed_frontend_text value = sorted[index];
    size_t insert = index;
    while (insert != 0u &&
           diagnostic_compare_text(sorted[insert - 1u], value) > 0) {
      sorted[insert] = sorted[insert - 1u];
      insert -= 1u;
    }
    sorted[insert] = value;
  }
  size_t unique_count = 0u;
  for (size_t index = 0u; index < available_count; index += 1u) {
    if (unique_count == 0u ||
        diagnostic_compare_text(sorted[unique_count - 1u], sorted[index]) !=
            0) {
      sorted[unique_count] = sorted[index];
      unique_count += 1u;
    }
  }
  frontend_diagnostic_fact_input facts[3];
  facts[0] = (frontend_diagnostic_fact_input){{"availableSlots", 14u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET,
                                               {NULL, 0u}, 0, sorted,
                                               unique_count};
  facts[1] = (frontend_diagnostic_fact_input){{"head", 4u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               head, 0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"slot", 4u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               slot, 0, NULL, 0u};
  frontend_diagnostic_label_input label;
  diagnostic_label_at(&label, "contract-head", head_span, head_document);
  return context_append_diagnostic_raw(context, "W-CONTRACT-0001", primary,
                                       facts, 3u, &label, 1u);
}

static bool append_generic0002_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text parameter, size_t parameter_document,
    w_seed_span parameter_span, size_t call_document, w_seed_span call_span) {
  if (context == NULL || parameter.length == 0u || parameter.data == NULL ||
      parameter_document >= context->input.document_count ||
      call_document >= context->input.document_count) {
    return false;
  }
  frontend_diagnostic_fact_input facts[4];
  facts[0] = (frontend_diagnostic_fact_input){{"candidates", 10u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET,
                                               {NULL, 0u}, 0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"equationSources", 15u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING_SET,
                                               {NULL, 0u}, 0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"parameter", 9u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               parameter, 0, NULL, 0u};
  facts[3] = (frontend_diagnostic_fact_input){{"reason", 6u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               {"missing-required-argument", 25u},
                                               0, NULL, 0u};
  frontend_diagnostic_label_input labels[2];
  diagnostic_label_at(&labels[0], "call-owner", call_span, call_document);
  diagnostic_label_at(&labels[1], "generic-parameter", parameter_span,
                      parameter_document);
  return context_append_diagnostic_raw(context, "W-GENERIC-0002", primary,
                                       facts, 4u, labels, 2u);
}

static bool append_generic0003_diagnostic(
    frontend_context *context, w_seed_span primary,
    w_seed_frontend_text external_label, w_seed_frontend_text kind,
    w_seed_frontend_text parameter, int64_t position,
    w_seed_frontend_text reason, size_t label_document, w_seed_span label_span) {
  if (context == NULL || external_label.length == 0u ||
      external_label.data == NULL || kind.length == 0u || kind.data == NULL ||
      parameter.length == 0u || parameter.data == NULL || reason.length == 0u ||
      reason.data == NULL || position < 0 ||
      label_document >= context->input.document_count) {
    return false;
  }
  frontend_diagnostic_fact_input facts[5];
  facts[0] = (frontend_diagnostic_fact_input){{"externalLabel", 13u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               external_label, 0, NULL, 0u};
  facts[1] = (frontend_diagnostic_fact_input){{"kind", 4u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               kind, 0, NULL, 0u};
  facts[2] = (frontend_diagnostic_fact_input){{"parameter", 9u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               parameter, 0, NULL, 0u};
  facts[3] = (frontend_diagnostic_fact_input){{"position", 8u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER,
                                               {NULL, 0u}, position, NULL, 0u};
  facts[4] = (frontend_diagnostic_fact_input){{"reason", 6u},
                                               W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING,
                                               reason, 0, NULL, 0u};
  frontend_diagnostic_label_input label;
  diagnostic_label_at(&label, "generic-parameter", label_span,
                      label_document);
  return context_append_diagnostic_raw(context, "W-GENERIC-0003", primary,
                                       facts, 5u, &label, 1u);
}

static bool const_record_failure(frontend_context *context, w_seed_span span,
                                 w_seed_frontend_text detail) {
  if (context == NULL || !context->current_function_is_const ||
      !context->current_const_body_active) {
    return true;
  }
  context->current_const_body_supported = false;
  if (context->emit && context->output != NULL &&
      context->function_index < context->output->function_capacity &&
      context->output->functions != NULL) {
    context->output->functions[context->function_index].const_body_supported =
        false;
  }
  if (context->current_const_root_emitted) return true;
  context->current_const_root_emitted = true;
  const w_seed_frontend_document *owner_doc = context_document(context);
  if (owner_doc == NULL || context->function_node == NULL) return false;
  const w_seed_span owner_span = context->function_node->raw_span;
  const w_seed_frontend_text owner_name =
      name_after_keyword(owner_doc, owner_span, "fn");
  if (owner_name.length == 0u) return false;
  w_seed_frontend_text call_chain[2];
  size_t call_chain_count = 0u;
  call_chain[call_chain_count] = owner_name;
  call_chain_count += 1u;
  if (detail.length != 0u && !text_equal_text(detail, owner_name)) {
    call_chain[call_chain_count] = detail;
    call_chain_count += 1u;
  }
  const w_seed_frontend_document *signature_doc = NULL;
  uint32_t signature_node = W_SEED_CST_NONE;
  const w_seed_frontend_external_symbol *external_symbol = NULL;
  const bool resolved_call =
      detail.length != 0u &&
      (function_signature_for_name(context, detail, &signature_doc,
                                   &signature_node) ||
       external_symbol_for_name(context, detail, &external_symbol));
  const w_seed_frontend_text operation =
      resolved_call
          ? (w_seed_frontend_text){"call", sizeof("call") - 1u}
          : (w_seed_frontend_text){"expression", sizeof("expression") - 1u};
  const w_seed_frontend_text reason = {
      "not const-safe", sizeof("not const-safe") - 1u};
  const w_seed_frontend_text symbol = detail.length != 0u ? detail : owner_name;
  return append_const0001_diagnostic(
      context, span, call_chain, call_chain_count, operation, reason, symbol,
      context->module_index, owner_span);
}

static bool append_module(frontend_context *context, w_seed_frontend_module value,
                          uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->input.document_count == 0
                             ? 0
                             : context->module_index;
  *index = (uint32_t)ordinal;
  if (!context->emit && !receipt_size_module(context, &value)) return false;
  if (context->emit) {
    if (context->output == NULL || context->output->modules == NULL ||
        ordinal >= context->output->module_capacity) {
      return false;
    }
    context->output->modules[ordinal] = value;
  }
  return true;
}

static bool context_append_const_declaration(
    frontend_context *context, w_seed_frontend_const_declaration value,
    uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->count.const_declarations;
  context->count.const_declarations += 1u;
  if (!add_u32(ordinal, index)) return false;
  if (!context->emit) return true;
  if (context->output == NULL) return false;
  if (context->output->const_declarations == NULL ||
      ordinal >= context->output->const_declaration_capacity)
    return false;
  context->output->const_declarations[ordinal] = value;
  return true;
}

static bool context_append_type(frontend_context *context,
                                w_seed_frontend_type value,
                                uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->count.types;
  context->count.types += 1;
  if (!add_u32(ordinal, index)) return false;
  if (!context->emit && !receipt_size_type(context, &value)) return false;
  if (context->emit) {
    if (context->output == NULL || context->output->types == NULL ||
        ordinal >= context->output->type_capacity) {
      return false;
    }
    context->output->types[ordinal] = value;
  }
  return true;
}

static bool context_append_struct(frontend_context *context,
                                  w_seed_frontend_struct value,
                                  uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->count.structs;
  context->count.structs += 1;
  if (!add_u32(ordinal, index)) return false;
  if (context->emit) {
    if (context->output == NULL || context->output->structs == NULL ||
        ordinal >= context->output->struct_capacity) {
      return false;
    }
    context->output->structs[ordinal] = value;
  }
  return true;
}

static bool context_append_generic_parameter(
    frontend_context *context, w_seed_frontend_generic_parameter value,
    uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->count.generic_parameters;
  context->count.generic_parameters += 1;
  if (!add_u32(ordinal, index)) return false;
  if (!context->emit && !receipt_size_generic_parameter(context, &value))
    return false;
  return context_append_record(
      context, ordinal, &value, sizeof(value),
      context->emit && context->output != NULL
          ? context->output->generic_parameters
          : NULL,
      context->output == NULL ? 0 : context->output->generic_parameter_capacity,
      index);
}

static bool context_append_enum(frontend_context *context,
                                w_seed_frontend_enum value,
                                uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->count.enums;
  context->count.enums += 1;
  if (!add_u32(ordinal, index)) return false;
  if (!context->emit && !receipt_size_enum(context, &value)) return false;
  if (context->emit) {
    if (context->output == NULL || context->output->enums == NULL ||
        ordinal >= context->output->enum_capacity) {
      return false;
    }
    context->output->enums[ordinal] = value;
  }
  return true;
}

static bool context_append_record(frontend_context *context, size_t ordinal,
                                  const void *value, size_t value_size,
                                  void *array, size_t capacity,
                                  uint32_t *index) {
  if (context == NULL || value == NULL || index == NULL ||
      !add_u32(ordinal, index)) {
    return false;
  }
  if (!context->emit) return true;
  if (array == NULL || ordinal >= capacity) return false;
  (void)memcpy((uint8_t *)array + ordinal * value_size, value, value_size);
  return true;
}

static bool context_append_import(frontend_context *context,
                                  w_seed_frontend_import value,
                                  uint32_t *index) {
  const size_t ordinal = context->count.imports;
  context->count.imports += 1;
  if (!context->emit && !receipt_size_import(context, &value)) return false;
  return context_append_record(context, ordinal, &value, sizeof(value),
                               context->emit && context->output != NULL
                                   ? context->output->imports
                                   : NULL,
                               context->output == NULL
                                   ? 0
                                   : context->output->import_capacity,
                               index);
}

static bool context_append_import_item(frontend_context *context,
                                       w_seed_frontend_import_item value,
                                       uint32_t *index) {
  const size_t ordinal = context->count.import_items;
  context->count.import_items += 1;
  return context_append_record(context, ordinal, &value, sizeof(value),
                               context->emit && context->output != NULL
                                   ? context->output->import_items
                                   : NULL,
                               context->output == NULL
                                   ? 0
                                   : context->output->import_item_capacity,
                               index);
}

static bool context_append_field(frontend_context *context,
                                 w_seed_frontend_field value,
                                 uint32_t *index) {
  const size_t ordinal = context->count.fields;
  context->count.fields += 1;
  return context_append_record(context, ordinal, &value, sizeof(value),
                               context->emit && context->output != NULL
                                   ? context->output->fields
                                   : NULL,
                               context->output == NULL
                                   ? 0
                                   : context->output->field_capacity,
                               index);
}

static bool context_append_enum_case(
    frontend_context *context, w_seed_frontend_enum_case value,
    uint32_t *index) {
  const size_t ordinal = context->count.enum_cases;
  context->count.enum_cases += 1;
  if (!context->emit && !receipt_size_enum_case(context, &value)) return false;
  return context_append_record(
      context, ordinal, &value, sizeof(value),
      context->emit && context->output != NULL ? context->output->enum_cases
                                                : NULL,
      context->output == NULL ? 0 : context->output->enum_case_capacity, index);
}

static bool context_append_enum_case_parameter(
    frontend_context *context, w_seed_frontend_enum_case_parameter value,
    uint32_t *index) {
  const size_t ordinal = context->count.enum_case_parameters;
  context->count.enum_case_parameters += 1;
  if (!context->emit && !receipt_size_enum_case_parameter(context, &value))
    return false;
  return context_append_record(
      context, ordinal, &value, sizeof(value),
      context->emit && context->output != NULL
          ? context->output->enum_case_parameters
          : NULL,
                               context->output == NULL ? 0 : context->output->enum_case_parameter_capacity,
                               index);
}

static bool context_append_enum_subset_member(
    frontend_context *context, w_seed_frontend_enum_subset_member value,
    uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->count.enum_subset_members;
  context->count.enum_subset_members += 1;
  if (!context->emit && !receipt_size_enum_subset_member(context, &value)) {
    return false;
  }
  return context_append_record(
      context, ordinal, &value, sizeof(value),
      context->emit && context->output != NULL
          ? context->output->enum_subset_members
          : NULL,
      context->output == NULL ? 0 : context->output->enum_subset_member_capacity,
      index);
}

static bool context_append_enum_membership_case(
    frontend_context *context, w_seed_frontend_enum_membership_case value,
    uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->count.enum_membership_cases;
  context->count.enum_membership_cases += 1;
  if (!context->emit && !receipt_size_enum_membership_case(context, &value)) {
    return false;
  }
  return context_append_record(
      context, ordinal, &value, sizeof(value),
      context->emit && context->output != NULL
          ? context->output->enum_membership_cases
          : NULL,
      context->output == NULL ? 0 : context->output->enum_membership_case_capacity,
      index);
}

static bool report_invalid_enum_subset(frontend_context *context,
                                       w_seed_span span,
                                       frontend_enum_subset_shape shape) {
  (void)shape;
  if (context == NULL) return false;
  /* Invalid subset declarations have no dedicated normative diagnostic code
   * in this D0.  Preserve the unsupported-type fact and closed barrier rather
   * than inventing a contract or semantic diagnostic. */
  return context_append_fact(context, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE,
                             span, text_from_span(context_document(context), span));
}

static bool normalize_enum_subset_type(
    frontend_context *context, const w_seed_frontend_document *doc,
    const w_seed_frontend_type *base_value, frontend_enum_subset_shape shape,
    uint32_t *root_index) {
  if (context == NULL || doc == NULL || base_value == NULL || root_index == NULL)
    return false;
  w_seed_frontend_type value = *base_value;
  value.kind = shape.full ? W_SEED_FRONTEND_TYPE_ENUM
                          : W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  value.nominal_name = shape.enum_name;
  value.enum_base_index = shape.enum_index;
  if (shape.full) {
    /* A full case-set is semantically the base enum.  Keep the source
     * spelling/span on the descriptor for provenance, but expose the same
     * public member sentinels as the canonical enum and emit no duplicate
     * subset records for this occurrence. */
    value.first_subset_member = W_SEED_FRONTEND_NONE;
    value.subset_member_count = 0;
    return context_append_type(context, value, root_index);
  }
  value.first_subset_member = (uint32_t)context->count.enum_subset_members;
  value.subset_member_count = (uint32_t)shape.item_count;
  if (!context_append_type(context, value, root_index)) return false;

  const w_seed_frontend_document *enum_doc = NULL;
  uint32_t enum_node = W_SEED_CST_NONE;
  if (!enum_declaration_for_name(context, shape.enum_name, NULL, NULL,
                                 &enum_doc, &enum_node)) {
    return false;
  }
  uint32_t case_cursor = enum_doc->nodes[enum_node].first_child;
  uint32_t case_node = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(enum_doc, &case_cursor, &case_node) &&
         guard < enum_doc->parse.node_count) {
    if (enum_doc->nodes[case_node].kind == W_SEED_CST_ENUM_CASE) {
      const w_seed_frontend_text name =
          first_word_in_span(enum_doc, enum_doc->nodes[case_node].raw_span);
      for (size_t item_index = 0; item_index < shape.item_count;
           item_index += 1) {
        frontend_enum_subset_item item;
        if (!enum_subset_item_at(doc, shape.list_span, item_index, &item) ||
            !text_equal_text(item.name, name)) {
          continue;
        }
        uint32_t case_index = W_SEED_FRONTEND_NONE;
        if (!enum_case_for_name(context, shape.enum_index, name, &case_index,
                                NULL, NULL)) {
          return false;
        }
        w_seed_frontend_enum_subset_member member;
        member.owner_type = *root_index;
        member.enum_base_index = shape.enum_index;
        member.enum_case_index = case_index;
        member.source_span = item.span;
        uint32_t member_index = W_SEED_FRONTEND_NONE;
        if (!context_append_enum_subset_member(context, member, &member_index)) {
          return false;
        }
        break;
      }
    }
    guard += 1;
  }
  if (context->emit && context->output != NULL &&
      *root_index < context->output->type_capacity) {
    context->output->types[*root_index].first_subset_member = value.first_subset_member;
    context->output->types[*root_index].subset_member_count =
        (uint32_t)(context->count.enum_subset_members -
                   (size_t)value.first_subset_member);
  }
  return true;
}

static bool context_append_type_declaration(
    frontend_context *context, w_seed_frontend_type_declaration value,
    uint32_t *index) {
  const size_t ordinal = context->count.type_declarations;
  context->count.type_declarations += 1;
  return context_append_record(context, ordinal, &value, sizeof(value),
                               context->emit && context->output != NULL
                                   ? context->output->type_declarations
                                   : NULL,
                               context->output == NULL
                                   ? 0
                                   : context->output->type_declaration_capacity,
                               index);
}

static bool context_append_alias(frontend_context *context,
                                 w_seed_frontend_alias value,
                                 uint32_t *index) {
  const size_t ordinal = context->count.aliases;
  context->count.aliases += 1;
  return context_append_record(context, ordinal, &value, sizeof(value),
                               context->emit && context->output != NULL
                                   ? context->output->aliases
                                   : NULL,
                               context->output == NULL
                                   ? 0
                                   : context->output->alias_capacity,
                               index);
}

static bool context_append_parameter(frontend_context *context,
                                     w_seed_frontend_parameter value,
                                     uint32_t *index) {
  const size_t ordinal = context->count.parameters;
  context->count.parameters += 1;
  return context_append_record(context, ordinal, &value, sizeof(value),
                               context->emit && context->output != NULL
                                   ? context->output->parameters
                                   : NULL,
                               context->output == NULL
                                   ? 0
                                   : context->output->parameter_capacity,
                               index);
}

static bool context_append_function(frontend_context *context,
                                    w_seed_frontend_function value,
                                    uint32_t *index) {
  const size_t ordinal = context->count.functions;
  context->count.functions += 1;
  return context_append_record(context, ordinal, &value, sizeof(value),
                               context->emit && context->output != NULL
                                   ? context->output->functions
                                   : NULL,
                               context->output == NULL
                                   ? 0
                                   : context->output->function_capacity,
                               index);
}

static bool context_append_entry(frontend_context *context,
                                 w_seed_frontend_entry value,
                                 uint32_t *index) {
  const size_t ordinal = context->count.entries;
  context->count.entries += 1;
  return context_append_record(context, ordinal, &value, sizeof(value),
                               context->emit && context->output != NULL
                                   ? context->output->entries
                                   : NULL,
                               context->output == NULL
                                   ? 0
                                   : context->output->entry_capacity,
                               index);
}

static bool context_append_statement(frontend_context *context,
                                     w_seed_frontend_statement value,
                                     uint32_t *index) {
  const size_t ordinal = context->count.statements;
  context->count.statements += 1;
  return context_append_record(context, ordinal, &value, sizeof(value),
                               context->emit && context->output != NULL
                                   ? context->output->statements
                                   : NULL,
                               context->output == NULL
                                   ? 0
                                   : context->output->statement_capacity,
                               index);
}

static bool context_append_expression(frontend_context *context,
                                      w_seed_frontend_expression value,
                                      uint32_t *index) {
  const size_t ordinal = context->count.expressions;
  context->count.expressions += 1;
  return context_append_record(context, ordinal, &value, sizeof(value),
                               context->emit && context->output != NULL
                                   ? context->output->expressions
                                   : NULL,
                               context->output == NULL
                                   ? 0
                                   : context->output->expression_capacity,
                               index);
}

static bool context_append_argument(frontend_context *context,
                                    w_seed_frontend_argument value,
                                    uint32_t *index) {
  const size_t ordinal = context->count.arguments;
  context->count.arguments += 1;
  return context_append_record(context, ordinal, &value, sizeof(value),
                               context->emit && context->output != NULL
                                   ? context->output->arguments
                                   : NULL,
                               context->output == NULL
                                   ? 0
                                   : context->output->argument_capacity,
                               index);
}

static bool context_append_generic_application(
    frontend_context *context, w_seed_frontend_generic_application value,
    uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->count.generic_applications;
  context->count.generic_applications += 1;
  if (!add_u32(ordinal, index)) return false;
  if (!context->emit && !receipt_size_generic_application(context, &value))
    return false;
  return context_append_record(
      context, ordinal, &value, sizeof(value),
      context->emit && context->output != NULL
          ? context->output->generic_applications
          : NULL,
      context->output == NULL ? 0 : context->output->generic_application_capacity,
      index);
}

static bool context_append_generic_argument(
    frontend_context *context, w_seed_frontend_generic_argument value,
    uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->count.generic_arguments;
  context->count.generic_arguments += 1;
  if (!add_u32(ordinal, index)) return false;
  if (!context->emit && !receipt_size_generic_argument(context, &value))
    return false;
  return context_append_record(
      context, ordinal, &value, sizeof(value),
      context->emit && context->output != NULL
          ? context->output->generic_arguments
          : NULL,
      context->output == NULL ? 0 : context->output->generic_argument_capacity,
      index);
}

static bool context_append_typed_const_expression(
    frontend_context *context, w_seed_frontend_typed_const_expression value,
    uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->count.typed_const_expressions;
  context->count.typed_const_expressions += 1u;
  if (!add_u32(ordinal, index)) return false;
  if (!context->emit && !receipt_size_typed_const_expression(context, &value))
    return false;
  return context_append_record(
      context, ordinal, &value, sizeof(value),
      context->emit && context->output != NULL
          ? context->output->typed_const_expressions
          : NULL,
      context->output == NULL ? 0
                              : context->output->typed_const_expression_capacity,
      index);
}

static bool context_append_const_value(frontend_context *context,
                                       w_seed_frontend_const_value value,
                                       uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->count.const_values;
  context->count.const_values += 1;
  if (!add_u32(ordinal, index)) return false;
  if (!context->emit && !receipt_size_const_value(context, &value)) return false;
  return context_append_record(
      context, ordinal, &value, sizeof(value),
      context->emit && context->output != NULL ? context->output->const_values
                                                : NULL,
      context->output == NULL ? 0 : context->output->const_value_capacity,
      index);
}

static bool context_append_const_element(
    frontend_context *context, w_seed_frontend_const_element value,
    uint32_t *index) {
  if (context == NULL || index == NULL) return false;
  const size_t ordinal = context->count.const_elements;
  context->count.const_elements += 1;
  if (!add_u32(ordinal, index)) return false;
  if (!context->emit && !receipt_size_const_element(context, &value)) return false;
  return context_append_record(
      context, ordinal, &value, sizeof(value),
      context->emit && context->output != NULL ? context->output->const_elements
                                                : NULL,
      context->output == NULL ? 0 : context->output->const_element_capacity,
      index);
}

static bool context_append_const_bytes(frontend_context *context,
                                       const uint8_t *bytes, size_t length,
                                       uint32_t *offset) {
  if (context == NULL || offset == NULL || length > UINT32_MAX ||
      context->count.const_bytes > (size_t)UINT32_MAX - length) {
    return false;
  }
  const size_t start = context->count.const_bytes;
  context->count.const_bytes += length;
  if (!add_u32(start, offset)) return false;
  if (context->emit) {
    if (context->output == NULL || start > context->output->const_bytes_capacity ||
        length > context->output->const_bytes_capacity - start ||
        (length != 0 && context->output->const_bytes == NULL)) return false;
    if (length != 0) {
      (void)memcpy(context->output->const_bytes + start, bytes, length);
    }
  }
  return true;
}

static bool context_append_switch_arm(
    frontend_context *context, w_seed_frontend_switch_arm value,
    uint32_t *index) {
  const size_t ordinal = context->count.switch_arms;
  context->count.switch_arms += 1;
  if (!context->emit && !receipt_size_switch_arm(context, &value)) return false;
  return context_append_record(
      context, ordinal, &value, sizeof(value),
      context->emit && context->output != NULL ? context->output->switch_arms
                                                : NULL,
      context->output == NULL ? 0 : context->output->switch_arm_capacity, index);
}

static bool context_append_symbol(frontend_context *context,
                                  w_seed_frontend_symbol value,
                                  uint32_t *index) {
  const size_t ordinal = context->count.symbols;
  context->count.symbols += 1;
  if (!context->emit && !receipt_size_symbol(context, &value)) return false;
  return context_append_record(context, ordinal, &value, sizeof(value),
                               context->emit && context->output != NULL
                                   ? context->output->symbols
                                   : NULL,
                               context->output == NULL
                                   ? 0
                                   : context->output->symbol_capacity,
                               index);
}

static w_seed_frontend_text first_word_in_span(
    const w_seed_frontend_document *doc, w_seed_span span) {
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  while (cursor_take(&cursor, &token)) {
    if (token.kind == W_SEED_CST_WORD) return text_from_span(doc, token.span);
  }
  return (w_seed_frontend_text){NULL, 0};
}

static w_seed_frontend_text name_after_keyword(
    const w_seed_frontend_document *doc, w_seed_span span, const char *keyword) {
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  bool saw_keyword = false;
  while (cursor_take(&cursor, &token)) {
    if (!saw_keyword) {
      if (token_text(doc, &token, keyword)) saw_keyword = true;
      continue;
    }
    if (token.kind == W_SEED_CST_WORD) return text_from_span(doc, token.span);
  }
  return (w_seed_frontend_text){NULL, 0};
}

static bool diagnostic_name_span_after_keyword(
    const w_seed_frontend_document *doc, w_seed_span span,
    const char *keyword, w_seed_frontend_text *name, w_seed_span *name_span) {
  if (name != NULL) *name = (w_seed_frontend_text){NULL, 0u};
  if (name_span != NULL) *name_span = empty_span(span.start_byte);
  if (doc == NULL || keyword == NULL || name == NULL || name_span == NULL)
    return false;
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  bool saw_keyword = false;
  while (cursor_take(&cursor, &token)) {
    if (!saw_keyword) {
      if (token_text(doc, &token, keyword)) saw_keyword = true;
      continue;
    }
    if (token.kind == W_SEED_CST_WORD) {
      *name = text_from_span(doc, token.span);
      *name_span = token.span;
      return name->length != 0u;
    }
  }
  return false;
}

static size_t diagnostic_generic_slot_order(
    const w_seed_frontend_document *doc, uint32_t generic_node,
    w_seed_frontend_text *slots, size_t slot_capacity) {
  if (doc == NULL || slots == NULL || slot_capacity == 0u ||
      generic_node == W_SEED_CST_NONE || generic_node >= doc->parse.node_count)
    return 0u;
  uint32_t cursor = doc->nodes[generic_node].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t count = 0u;
  size_t guard = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_GENERIC_PARAMETER) {
      if (count >= slot_capacity) return 0u;
      w_seed_frontend_text slot = generic_parameter_name(doc, child);
      if (slot.length == 0u)
        slot = generic_parameter_external_label(doc, child);
      if (slot.length == 0u) return 0u;
      slots[count] = slot;
      count += 1u;
    }
    guard += 1u;
  }
  return count;
}

static bool span_has_keyword(const w_seed_frontend_document *doc,
                             w_seed_span span, const char *keyword) {
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  while (cursor_take(&cursor, &token)) {
    if (token_text(doc, &token, keyword)) return true;
  }
  return false;
}

/* Function modifiers are declaration-prefix facts.  Do not scan the body for
 * a matching word because a local binding or literal can use the same text. */
static bool function_prefix_has_keyword(const w_seed_frontend_document *doc,
                                        w_seed_span span,
                                        const char *keyword) {
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  while (cursor_take(&cursor, &token)) {
    if (token_text(doc, &token, "fn")) break;
    if (token_text(doc, &token, keyword)) return true;
  }
  return false;
}

static bool direct_import_ordinal_for(const w_seed_frontend_document *doc,
                                      uint32_t node_index,
                                      uint32_t *ordinal) {
  if (ordinal != NULL) *ordinal = W_SEED_FRONTEND_NONE;
  if (doc == NULL || ordinal == NULL || node_index >= doc->parse.node_count ||
      doc->parse.root >= doc->parse.node_count) {
    return false;
  }
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0u;
  uint32_t current = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (child == node_index) {
      if (doc->nodes[child].kind != W_SEED_CST_IMPORT) return false;
      *ordinal = current;
      return true;
    }
    if (doc->nodes[child].kind == W_SEED_CST_IMPORT) current += 1u;
    guard += 1u;
  }
  return false;
}

static w_seed_frontend_type type_record_from_span(
    const w_seed_frontend_document *doc, w_seed_span span) {
  const w_seed_span trimmed = trim_span(doc, span);
  const frontend_simple_type simple = simple_type_from_text(doc, trimmed);
  w_seed_frontend_type value;
  (void)memset(&value, 0, sizeof(value));
  value.kind = simple.kind;
  value.spelling = text_from_span(doc, trimmed);
  value.nominal_name = value.spelling;
  value.span = span;
  value.is_signed = simple.is_signed;
  value.bit_width = simple.bit_width;
  value.element_type = W_SEED_FRONTEND_NONE;
  value.return_type = W_SEED_FRONTEND_NONE;
  value.first_parameter = W_SEED_FRONTEND_NONE;
  value.enum_base_index = W_SEED_FRONTEND_NONE;
  value.first_subset_member = W_SEED_FRONTEND_NONE;
  value.subset_member_count = 0;
  value.generic_application_index = W_SEED_FRONTEND_NONE;
  if (simple.kind == W_SEED_FRONTEND_TYPE_OPTION && value.spelling.length > 0) {
    value.nominal_name.data = value.spelling.data;
    value.nominal_name.length = value.spelling.length - 1;
  }
  return value;
}

static bool normalize_type_tree_depth(frontend_context *context,
                                      uint32_t type_node,
                                      uint32_t *root_index, size_t depth) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || type_node >= doc->parse.node_count || root_index == NULL) {
    return false;
  }
  if (depth >= W_SEED_FRONTEND_MAX_NESTING) return false;
  w_seed_span type_span = doc->nodes[type_node].raw_span;
  uint32_t application_struct = W_SEED_FRONTEND_NONE;
  uint32_t application_struct_node = W_SEED_CST_NONE;
  w_seed_frontend_text application_head = {NULL, 0};
  w_seed_span application_envelope = empty_span(type_span.start_byte);
  const bool application_candidate =
      !context->normalizing_generic_domain &&
      local_generic_head_for_type(context, doc, type_node, &application_struct,
                                  &application_struct_node, &application_head,
                                  &application_envelope);
  (void)application_struct;
  (void)application_struct_node;
  (void)application_head;
  (void)application_envelope;
  if (context->normalizing_generic_domain) {
    /* A generic domain keeps the base spelling separate from a refinement.
     * For StaticList<ServiceStage>, retain the first type envelope and stop
     * before the following predicate envelope. */
    const uint32_t first_envelope =
        first_direct_kind(doc, type_node, W_SEED_CST_CONTRACT_ENVELOPE);
    if (first_envelope != W_SEED_CST_NONE) {
      const bool envelope_has_type =
          first_direct_kind(doc, first_envelope, W_SEED_CST_TYPE) !=
          W_SEED_CST_NONE;
      if (envelope_has_type) {
        type_span.end_byte = doc->nodes[first_envelope].raw_span.end_byte;
      } else {
        type_span.end_byte = doc->nodes[first_envelope].raw_span.start_byte;
      }
    }
  }
  if (application_candidate) type_span = generic_head_type_span(doc, type_node);
  w_seed_frontend_type value = type_record_from_span(doc, type_span);
  if (value.kind == W_SEED_FRONTEND_TYPE_NOMINAL &&
      enum_declaration_name_count(context, value.spelling) == 1u) {
    uint32_t enum_index = W_SEED_FRONTEND_NONE;
    if (enum_declaration_for_name(context, value.spelling, &enum_index, NULL,
                                  NULL, NULL)) {
      value.kind = W_SEED_FRONTEND_TYPE_ENUM;
      value.nominal_name = value.spelling;
      value.enum_base_index = enum_index;
    }
  }
  frontend_enum_subset_shape subset_shape;
  bool has_subset_shape =
      !application_candidate &&
      enum_subset_shape_for_type(context, doc, type_node, &subset_shape);
  if (!has_subset_shape && value.kind == W_SEED_FRONTEND_TYPE_NOMINAL) {
    has_subset_shape = enum_subset_shape_for_alias(context, value.spelling,
                                                    &subset_shape);
  }
  if (has_subset_shape && subset_shape.has_contract) {
    if (!subset_shape.valid) {
      (void)report_invalid_enum_subset(context, value.span, subset_shape);
      if (!context_append_type(context, value, root_index)) return false;
    } else {
      return normalize_enum_subset_type(context, doc, &value, subset_shape,
                                        root_index);
    }
  } else {
    if (!context_append_type(context, value, root_index)) return false;
  }
  if (application_candidate &&
      !register_pending_generic_application(context, type_node, *root_index)) {
    return false;
  }
  if (value.kind == W_SEED_FRONTEND_TYPE_UNKNOWN ||
      value.kind == W_SEED_FRONTEND_TYPE_FUNCTION) {
    (void)context_append_fact(context, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE,
                              value.span, value.spelling);
  }
  if (value.kind == W_SEED_FRONTEND_TYPE_OPTION &&
      value.spelling.length > 1u) {
    const w_seed_span inner_span = {
        value.span.start_byte, value.span.end_byte - 1u};
    const w_seed_frontend_type inner = type_record_from_span(doc, inner_span);
    uint32_t inner_index = W_SEED_FRONTEND_NONE;
    if (!context_append_type(context, inner, &inner_index)) return false;
    if (context->emit && context->output != NULL &&
        *root_index < context->output->type_capacity) {
      context->output->types[*root_index].element_type = inner_index;
    }
  }
  uint32_t cursor = doc->nodes[type_node].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  uint32_t first_nested_type = W_SEED_FRONTEND_NONE;
  bool saw_nested_type = false;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_CONTRACT_ENVELOPE &&
        !context->normalizing_generic_domain &&
        !application_candidate &&
        !has_subset_shape && value.kind != W_SEED_FRONTEND_TYPE_STATIC_LIST &&
        value.kind != W_SEED_FRONTEND_TYPE_RANGE) {
      (void)context_append_fact(context, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE,
                                doc->nodes[child].raw_span,
                                text_from_span(doc, doc->nodes[child].raw_span));
    }
    if (application_candidate) {
      /* Generic application arguments are owned by the pending application.
       * Do not normalize their TYPE nodes through the envelope root. */
      guard += 1u;
      continue;
    }
    if (doc->nodes[child].kind == W_SEED_CST_TYPE) {
      uint32_t nested = W_SEED_FRONTEND_NONE;
      if (!normalize_type_tree_depth(context, child, &nested, depth + 1u)) {
        return false;
      }
      saw_nested_type = true;
      if (first_nested_type == W_SEED_FRONTEND_NONE) first_nested_type = nested;
    } else if (!node_is_raw(&doc->nodes[child])) {
      uint32_t nested_cursor = doc->nodes[child].first_child;
      uint32_t nested_child = W_SEED_CST_NONE;
      size_t nested_guard = 0;
      while (next_child(doc, &nested_cursor, &nested_child) &&
             nested_guard < doc->parse.node_count) {
        if (doc->nodes[nested_child].kind == W_SEED_CST_TYPE) {
          uint32_t nested = W_SEED_FRONTEND_NONE;
          if (!normalize_type_tree_depth(context, nested_child, &nested,
                                         depth + 1u)) {
            return false;
          }
          saw_nested_type = true;
          if (first_nested_type == W_SEED_FRONTEND_NONE)
            first_nested_type = nested;
        }
        nested_guard += 1;
      }
    }
    guard += 1;
  }
  if (value.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST && !saw_nested_type &&
      value.spelling.length > 12u &&
      value.span.end_byte > value.span.start_byte + 12u) {
    /* Resolve the element from the CST TYPE child. Do not slice the raw
     * spelling: comments and nested trivia are not semantic type text. */
    uint32_t envelope_cursor = doc->nodes[type_node].first_child;
    uint32_t envelope_child = W_SEED_CST_NONE;
    size_t envelope_guard = 0u;
    while (next_child(doc, &envelope_cursor, &envelope_child) &&
           envelope_guard < doc->parse.node_count) {
      if (doc->nodes[envelope_child].kind == W_SEED_CST_CONTRACT_ENVELOPE &&
          contract_envelope_has_type(doc, envelope_child)) {
        const uint32_t element_node =
            first_direct_kind(doc, envelope_child, W_SEED_CST_TYPE);
        if (element_node != W_SEED_CST_NONE &&
            !normalize_type_tree_depth(context, element_node,
                                        &first_nested_type, depth + 1u)) {
          return false;
        }
        break;
      }
      envelope_guard += 1u;
    }
  }
  if (value.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST &&
      context->emit && context->output != NULL &&
      *root_index < context->output->type_capacity) {
    context->output->types[*root_index].element_type = first_nested_type;
  }
  return true;
}

static bool normalize_type_tree(frontend_context *context, uint32_t type_node,
                                uint32_t *root_index) {
  return normalize_type_tree_depth(context, type_node, root_index, 0u);
}

static uint32_t direct_type_index(const w_seed_frontend_document *doc,
                                  uint32_t owner) {
  return first_direct_kind(doc, owner, W_SEED_CST_TYPE);
}

static bool normalize_symbol(frontend_context *context,
                             w_seed_frontend_symbol_kind kind,
                             uint32_t owner_index, w_seed_frontend_text name,
                             bool exported, w_seed_span span,
                             uint32_t type_index, uint32_t *symbol_index) {
  w_seed_frontend_symbol value;
  (void)memset(&value, 0, sizeof(value));
  value.kind = kind;
  value.module_index = (uint32_t)context->module_index;
  value.owner_index = owner_index;
  value.name = name;
  value.exported = exported;
  value.span = span;
  value.type_index = type_index;
  return context_append_symbol(context, value, symbol_index);
}

static bool normalize_import(frontend_context *context, uint32_t node_index,
                             uint32_t *import_index) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || import_index == NULL ||
      node_index >= doc->parse.node_count) return false;
  const w_seed_cst_node *node = &doc->nodes[node_index];
  frontend_token_cursor cursor = token_cursor_for(doc, node->raw_span);
  frontend_token token;
  bool saw_import = false;
  bool saw_from = false;
  bool saw_brace = false;
  w_seed_frontend_text alias = {NULL, 0};
  while (cursor_take(&cursor, &token)) {
    if (!saw_import) {
      if (token_text(doc, &token, "import")) saw_import = true;
      continue;
    }
    if (token_text(doc, &token, "from")) {
      saw_from = true;
      continue;
    }
    if (!saw_from && token_text(doc, &token, "{")) {
      saw_brace = true;
      continue;
    }
    if (!saw_from && !saw_brace && token.kind == W_SEED_CST_WORD &&
        alias.length == 0) {
      alias = text_from_span(doc, token.span);
    }
  }
  uint32_t direct_ordinal = W_SEED_FRONTEND_NONE;
  if (!direct_import_ordinal_for(doc, node_index, &direct_ordinal)) return false;
  w_seed_span path_span = empty_span(node->raw_span.start_byte);
  if (!w_seed_module_scan_import_path_span(
          doc->source, doc->nodes, doc->parse.node_count, node->raw_span,
          &path_span)) {
    return false;
  }
  w_seed_frontend_import value;
  (void)memset(&value, 0, sizeof(value));
  value.module_index = (uint32_t)context->module_index;
  value.path = text_from_span(doc, path_span);
  value.alias = alias;
  value.span = node->raw_span;
  value.first_item = (uint32_t)context->count.import_items;
  value.item_count = (uint32_t)count_direct_kind(doc, node_index,
                                                  W_SEED_CST_IMPORT_ITEM);
  value.direct_import_ordinal = direct_ordinal;
  value.target_kind = W_SEED_FRONTEND_IMPORT_UNRESOLVED;
  value.target_index = W_SEED_FRONTEND_NONE;
  if (context->input.import_resolution_complete) {
    size_t edge_index = SIZE_MAX;
    const w_seed_frontend_resolved_import *edge = NULL;
    if (!resolved_import_index_for(context, context->module_index,
                                   direct_ordinal, &edge_index) ||
        (edge = resolved_import_at(context, edge_index)) == NULL) {
      return false;
    }
    value.target_kind = edge->target_kind ==
                                W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT
                            ? W_SEED_FRONTEND_IMPORT_LOCAL_DOCUMENT
                            : W_SEED_FRONTEND_IMPORT_EXTERNAL_MODULE;
    value.target_index = edge->target_index;
  }
  if (!context_append_import(context, value, import_index)) return false;
  uint32_t child_cursor = node->first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &child_cursor, &child) &&
         guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_IMPORT_ITEM) {
      w_seed_frontend_import_item item;
      item.module_index = (uint32_t)context->module_index;
      w_seed_frontend_text imported_name = {NULL, 0};
      item.local_name = import_item_local_name(
          doc, doc->nodes[child].raw_span, &imported_name);
      item.name = imported_name;
      item.span = doc->nodes[child].raw_span;
      uint32_t item_index = W_SEED_FRONTEND_NONE;
      if (!context_append_import_item(context, item, &item_index)) return false;
    }
    guard += 1;
  }
  return true;
}

static bool generic_parameter_label_omitted(
    const w_seed_frontend_document *doc, uint32_t parameter_node) {
  if (doc == NULL || parameter_node >= doc->parse.node_count) return false;
  const uint32_t first_word = first_direct_kind(doc, parameter_node,
                                                 W_SEED_CST_WORD);
  if (first_word == W_SEED_CST_NONE) return false;
  return text_equal(text_from_span(doc, doc->nodes[first_word].raw_span), "_");
}

static w_seed_frontend_text generic_parameter_word_at(
    const w_seed_frontend_document *doc, uint32_t parameter_node,
    size_t ordinal) {
  if (doc == NULL || parameter_node >= doc->parse.node_count)
    return (w_seed_frontend_text){NULL, 0};
  uint32_t cursor = doc->nodes[parameter_node].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  size_t word_ordinal = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_WORD) {
      if (word_ordinal == ordinal) {
        return text_from_span(doc, doc->nodes[child].raw_span);
      }
      word_ordinal += 1u;
    }
    guard += 1;
  }
  return (w_seed_frontend_text){NULL, 0};
}

static w_seed_frontend_text generic_parameter_name(
    const w_seed_frontend_document *doc, uint32_t parameter_node) {
  const w_seed_frontend_text first =
      generic_parameter_word_at(doc, parameter_node, 0u);
  const bool omitted = text_equal(first, "_");
  const w_seed_frontend_text candidate =
      generic_parameter_word_at(doc, parameter_node, omitted ? 2u : 1u);
  return candidate.length != 0u
             ? candidate
             : (omitted ? generic_parameter_word_at(doc, parameter_node, 1u)
                        : first);
}

static uint32_t generic_parameter_type_node(
    const w_seed_frontend_document *doc, uint32_t parameter_node) {
  return first_direct_kind(doc, parameter_node, W_SEED_CST_TYPE);
}

static w_seed_frontend_text generic_parameter_external_label(
    const w_seed_frontend_document *doc, uint32_t parameter_node) {
  if (generic_parameter_label_omitted(doc, parameter_node) ||
      generic_parameter_type_node(doc, parameter_node) == W_SEED_CST_NONE) {
    return (w_seed_frontend_text){NULL, 0};
  }
  return generic_parameter_word_at(doc, parameter_node, 0u);
}

static bool contract_envelope_has_type(const w_seed_frontend_document *doc,
                                       uint32_t envelope_node) {
  return doc != NULL && envelope_node != W_SEED_CST_NONE &&
         first_direct_kind(doc, envelope_node, W_SEED_CST_TYPE) !=
             W_SEED_CST_NONE;
}

static w_seed_span generic_base_type_span(
    const w_seed_frontend_document *doc, uint32_t type_node) {
  if (doc == NULL || type_node == W_SEED_CST_NONE ||
      type_node >= doc->parse.node_count) {
    return empty_span(0);
  }
  w_seed_span span = doc->nodes[type_node].raw_span;
  const uint32_t first_envelope =
      first_direct_kind(doc, type_node, W_SEED_CST_CONTRACT_ENVELOPE);
  if (first_envelope != W_SEED_CST_NONE) {
    span.end_byte = contract_envelope_has_type(doc, first_envelope)
                        ? doc->nodes[first_envelope].raw_span.end_byte
                        : doc->nodes[first_envelope].raw_span.start_byte;
  }
  return trim_span(doc, span);
}

static w_seed_span generic_head_type_span(
    const w_seed_frontend_document *doc, uint32_t type_node) {
  if (doc == NULL || type_node == W_SEED_CST_NONE ||
      type_node >= doc->parse.node_count) {
    return empty_span(0);
  }
  w_seed_span span = doc->nodes[type_node].raw_span;
  const uint32_t first_envelope =
      first_direct_kind(doc, type_node, W_SEED_CST_CONTRACT_ENVELOPE);
  if (first_envelope != W_SEED_CST_NONE)
    span.end_byte = doc->nodes[first_envelope].raw_span.start_byte;
  return trim_span(doc, span);
}

static bool local_generic_head_for_type(
    const frontend_context *context, const w_seed_frontend_document *doc,
    uint32_t type_node, uint32_t *struct_index, uint32_t *struct_node,
    w_seed_frontend_text *head_name, w_seed_span *envelope_span) {
  if (struct_index != NULL) *struct_index = W_SEED_FRONTEND_NONE;
  if (struct_node != NULL) *struct_node = W_SEED_CST_NONE;
  if (head_name != NULL) *head_name = (w_seed_frontend_text){NULL, 0};
  if (envelope_span != NULL) *envelope_span = empty_span(0);
  if (context == NULL || doc == NULL || type_node == W_SEED_CST_NONE ||
      type_node >= doc->parse.node_count) return false;
  const uint32_t envelope =
      first_direct_kind(doc, type_node, W_SEED_CST_CONTRACT_ENVELOPE);
  if (envelope == W_SEED_CST_NONE) return false;
  /* Application head resolution stops at the first envelope opener.  The
   * domain helper retains a type-bearing envelope, but that is not the
   * nominal head span for Matrix<T,...>. */
  const w_seed_span base_span = generic_head_type_span(doc, type_node);
  frontend_token_cursor cursor = token_cursor_for(doc, base_span);
  frontend_token token;
  w_seed_frontend_text candidate = {NULL, 0};
  size_t words = 0;
  while (cursor_take(&cursor, &token)) {
    if (token.kind == W_SEED_CST_WORD) {
      candidate = text_from_span(doc, token.span);
      words += 1u;
    }
  }
  if (words != 1u || candidate.length == 0) return false;
  const w_seed_frontend_document *owner_doc = NULL;
  uint32_t owner_node = W_SEED_CST_NONE;
  uint32_t owner_index = W_SEED_FRONTEND_NONE;
  if (!struct_declaration_for_name(context, candidate, &owner_index,
                                   &owner_doc, &owner_node) ||
      owner_doc == NULL || owner_node == W_SEED_CST_NONE) {
    return false;
  }
  /* A struct head is usable only when the local type-head namespace has no
   * enum, alias, or type declaration with the same name.  Duplicate facts
   * alone are not enough: do not publish a valid application through an
   * ambiguous spelling. */
  for (size_t module_index = 0u; module_index < context->input.document_count;
       module_index += 1u) {
    const w_seed_frontend_document *candidate_doc =
        &context->input.documents[module_index];
    if (!module_id_equal(document_module_name(candidate_doc),
                         document_module_name(doc)))
      continue;
    uint32_t root_cursor = candidate_doc->nodes[candidate_doc->parse.root].first_child;
    uint32_t root_child = W_SEED_CST_NONE;
    size_t root_guard = 0u;
    while (next_child(candidate_doc, &root_cursor, &root_child) &&
           root_guard < candidate_doc->parse.node_count) {
      const w_seed_cst_kind kind = candidate_doc->nodes[root_child].kind;
      const char *keyword = kind == W_SEED_CST_ENUM
                                ? "enum"
                                : kind == W_SEED_CST_ALIAS_DECLARATION
                                      ? "alias"
                                      : kind == W_SEED_CST_TYPE_DECLARATION
                                            ? "type"
                                            : NULL;
      if (keyword != NULL &&
          text_equal_text(name_after_keyword(
                              candidate_doc,
                              candidate_doc->nodes[root_child].raw_span,
                              keyword),
                          candidate))
        return false;
      root_guard += 1u;
    }
  }
  if (struct_index != NULL) *struct_index = owner_index;
  if (struct_node != NULL) *struct_node = owner_node;
  if (head_name != NULL) *head_name = candidate;
  if (envelope_span != NULL) *envelope_span = doc->nodes[envelope].raw_span;
  return true;
}

static bool register_pending_generic_application(frontend_context *context,
                                                 uint32_t type_node,
                                                 uint32_t owner_type) {
  if (context == NULL) return false;
  if (context->pending_application_count >=
      FRONTEND_MAX_PENDING_APPLICATIONS) {
    const w_seed_frontend_document *doc = context_document(context);
    if (doc != NULL && type_node < doc->parse.node_count) {
      (void)context_append_fact(
          context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
          doc->nodes[type_node].raw_span,
          text_from_span(doc, doc->nodes[type_node].raw_span));
    }
    /* The type remains normalized, but this seed does not publish an
     * application record beyond its bounded pending-work projection. */
    return true;
  }
  frontend_pending_application *pending =
      &context->pending_applications[context->pending_application_count];
  pending->module_index = context->module_index;
  pending->type_node = type_node;
  pending->owner_type = owner_type;
  context->pending_application_count += 1u;
  return true;
}

static uint32_t generic_refinement_envelope(
    const w_seed_frontend_document *doc, uint32_t type_node) {
  if (doc == NULL || type_node == W_SEED_CST_NONE ||
      type_node >= doc->parse.node_count)
    return W_SEED_CST_NONE;
  uint32_t cursor = doc->nodes[type_node].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_CONTRACT_ENVELOPE &&
        !contract_envelope_has_type(doc, child)) {
      return child;
    }
    guard += 1;
  }
  return W_SEED_CST_NONE;
}

static w_seed_span generic_predicate_span(
    const w_seed_frontend_document *doc, uint32_t envelope_node) {
  if (doc == NULL || envelope_node == W_SEED_CST_NONE) return empty_span(0);
  const uint32_t expression =
      first_direct_kind(doc, envelope_node, W_SEED_CST_EXPRESSION);
  if (expression != W_SEED_CST_NONE) return doc->nodes[expression].raw_span;
  const uint32_t parentheses =
      first_direct_kind(doc, envelope_node, W_SEED_CST_PARENTHESES);
  if (parentheses != W_SEED_CST_NONE) return doc->nodes[parentheses].raw_span;
  return doc->nodes[envelope_node].raw_span;
}

static w_seed_frontend_text generic_predicate_call_name(
    const w_seed_frontend_document *doc, w_seed_span span) {
  if (doc == NULL) return (w_seed_frontend_text){NULL, 0};
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token tokens[16];
  size_t token_count = 0;
  frontend_token token;
  while (cursor_take(&cursor, &token)) {
    if (token_count >= sizeof(tokens) / sizeof(tokens[0])) {
      return (w_seed_frontend_text){NULL, 0};
    }
    tokens[token_count] = token;
    token_count += 1u;
  }
  while (token_count >= 2u && token_text(doc, &tokens[0], "(") &&
         token_text(doc, &tokens[token_count - 1u], ")")) {
    (void)memmove(tokens, tokens + 1u,
                  (token_count - 2u) * sizeof(tokens[0]));
    token_count -= 2u;
  }
  if (token_count != 5u || tokens[0].kind != W_SEED_CST_WORD ||
      !token_text(doc, &tokens[1], "(") ||
      !token_text(doc, &tokens[2], ".") || tokens[3].kind != W_SEED_CST_WORD ||
      !token_text(doc, &tokens[3], "member") ||
      !token_text(doc, &tokens[4], ")")) {
    return (w_seed_frontend_text){NULL, 0};
  }
  return text_from_span(doc, tokens[0].span);
}

static bool generic_predicate_mentions_member(
    const w_seed_frontend_document *doc, w_seed_span span) {
  if (doc == NULL) return false;
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  while (cursor_take(&cursor, &token)) {
    if (!token_text(doc, &token, ".")) continue;
    frontend_token member;
    if (cursor_take(&cursor, &member) &&
        token_text(doc, &member, "member")) {
      return true;
    }
  }
  return false;
}

static bool function_declaration_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    uint32_t *function_index, const w_seed_frontend_document **owner_doc,
    uint32_t *function_node) {
  if (function_index != NULL) *function_index = W_SEED_FRONTEND_NONE;
  if (owner_doc != NULL) *owner_doc = NULL;
  if (function_node != NULL) *function_node = W_SEED_CST_NONE;
  if (context == NULL || name.length == 0 || function_index == NULL) return false;
  const w_seed_frontend_document *resolved_doc = NULL;
  uint32_t resolved_node = W_SEED_CST_NONE;
  if (!function_signature_for_name(context, name, &resolved_doc,
                                   &resolved_node) ||
      resolved_doc == NULL || resolved_node == W_SEED_CST_NONE) {
    return false;
  }
  size_t ordinal = 0;
  for (size_t document_index = 0;
       document_index < context->input.document_count; document_index += 1) {
    const w_seed_frontend_document *doc =
        &context->input.documents[document_index];
    uint32_t cursor = doc->nodes[doc->parse.root].first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t guard = 0;
    while (next_child(doc, &cursor, &child) &&
           guard < doc->parse.node_count) {
      if (doc->nodes[child].kind == W_SEED_CST_FUNCTION) {
        if (doc == resolved_doc && child == resolved_node) {
          if (!add_u32(ordinal, function_index)) return false;
          if (owner_doc != NULL) *owner_doc = doc;
          if (function_node != NULL) *function_node = child;
          return true;
        }
        ordinal += 1;
      }
      guard += 1;
    }
  }
  return false;
}

static bool generic_value_domain_supported(
    const frontend_context *context, const w_seed_frontend_document *doc,
    uint32_t type_node, frontend_simple_type *simple_out) {
  if (simple_out != NULL) *simple_out = simple_type_unknown();
  if (context == NULL || doc == NULL || type_node == W_SEED_CST_NONE)
    return false;
  const frontend_simple_type simple =
      contextual_type_from_span(context, doc, generic_base_type_span(doc,
                                                                       type_node));
  if (simple_out != NULL) *simple_out = simple;
  if (simple.kind == W_SEED_FRONTEND_TYPE_ENUM) return true;
  if (simple.kind == W_SEED_FRONTEND_TYPE_NOMINAL) {
    uint32_t struct_index = W_SEED_FRONTEND_NONE;
    uint32_t struct_node = W_SEED_CST_NONE;
    const w_seed_frontend_document *owner_doc = NULL;
    return struct_declaration_for_name(context, simple.spelling,
                                       &struct_index, &owner_doc,
                                       &struct_node);
  }
  if (simple.kind == W_SEED_FRONTEND_TYPE_INTEGER ||
      simple.kind == W_SEED_FRONTEND_TYPE_BOOL ||
      simple.kind == W_SEED_FRONTEND_TYPE_STRING ||
      simple.kind == W_SEED_FRONTEND_TYPE_BYTES) {
    return true;
  }
  if (simple.kind != W_SEED_FRONTEND_TYPE_STATIC_LIST) return false;
  if (simple_out != NULL &&
      !static_list_element_from_span(context, doc,
                                     doc->nodes[type_node].raw_span,
                                     simple_out)) {
    return false;
  }
  const uint32_t envelope =
      first_direct_kind(doc, type_node, W_SEED_CST_CONTRACT_ENVELOPE);
  if (envelope == W_SEED_CST_NONE ||
      !contract_envelope_has_type(doc, envelope))
    return false;
  const frontend_simple_type element =
      simple_out == NULL ? static_list_element_type(context, simple)
                         : static_list_element_type(context, *simple_out);
  return element.kind == W_SEED_FRONTEND_TYPE_ENUM ||
         element.kind == W_SEED_FRONTEND_TYPE_INTEGER ||
         element.kind == W_SEED_FRONTEND_TYPE_BOOL ||
         element.kind == W_SEED_FRONTEND_TYPE_STRING ||
         element.kind == W_SEED_FRONTEND_TYPE_BYTES ||
         element.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST;
}

static bool generic_predicate_signature_valid(
    frontend_context *context, const w_seed_frontend_document *domain_doc,
    uint32_t domain_type_node, const w_seed_frontend_document *predicate_doc,
    uint32_t predicate_node, bool *returns_bool) {
  if (returns_bool != NULL) *returns_bool = false;
  if (context == NULL || domain_doc == NULL || predicate_doc == NULL ||
      predicate_node == W_SEED_CST_NONE)
    return false;
  const uint32_t parameter_list =
      first_direct_kind(predicate_doc, predicate_node,
                        W_SEED_CST_PARAMETER_LIST);
  const size_t parameter_count =
      parameter_list == W_SEED_CST_NONE
          ? 0u
          : count_direct_kind(predicate_doc, parameter_list,
                              W_SEED_CST_PARAMETER);
  const uint32_t return_node =
      first_direct_kind(predicate_doc, predicate_node, W_SEED_CST_RETURN_TYPE);
  if (return_node == W_SEED_CST_NONE) return false;
  const uint32_t return_type = direct_type_index(predicate_doc, return_node);
  if (return_type == W_SEED_CST_NONE) return false;
  const frontend_simple_type returned = contextual_type_from_span(
      context, predicate_doc, predicate_doc->nodes[return_type].raw_span);
  if (returns_bool != NULL) *returns_bool = type_is_bool(returned);
  if (parameter_count != 1u) return false;
  const uint32_t parameter =
      parameter_list == W_SEED_CST_NONE
          ? W_SEED_CST_NONE
          : first_direct_kind(predicate_doc, parameter_list,
                              W_SEED_CST_PARAMETER);
  const uint32_t parameter_type =
      parameter == W_SEED_CST_NONE
          ? W_SEED_CST_NONE
          : direct_type_index(predicate_doc, parameter);
  if (parameter_type == W_SEED_CST_NONE)
    return false;
  const frontend_simple_type expected = contextual_type_from_span(
      context, domain_doc, generic_base_type_span(domain_doc, domain_type_node));
  const frontend_simple_type actual = contextual_type_from_span(
      context, predicate_doc, predicate_doc->nodes[parameter_type].raw_span);
  return frontend_type_equal(context, expected, actual);
}

static bool generic_domain_refers_to_previous_type(
    const w_seed_frontend_document *doc, uint32_t generic_node,
    w_seed_frontend_text *previous_name, uint32_t *previous_ordinal,
    const w_seed_frontend_text *names, const bool *is_type,
    size_t prior_count) {
  if (previous_name != NULL) *previous_name = (w_seed_frontend_text){NULL, 0};
  if (previous_ordinal != NULL) *previous_ordinal = W_SEED_FRONTEND_NONE;
  if (doc == NULL || generic_node == W_SEED_CST_NONE || names == NULL ||
      is_type == NULL || prior_count == 0u) return false;
  const uint32_t type_node = generic_parameter_type_node(doc, generic_node);
  if (type_node == W_SEED_CST_NONE) return false;
  const w_seed_frontend_text domain =
      text_from_span(doc, generic_base_type_span(doc, type_node));
  for (size_t ordinal = 0; ordinal < prior_count; ordinal += 1u) {
    if (is_type[ordinal] && text_equal_text(domain, names[ordinal])) {
      if (previous_name != NULL) *previous_name = names[ordinal];
      if (previous_ordinal != NULL) {
        if (!add_u32(ordinal, previous_ordinal)) return false;
      }
      return true;
    }
  }
  return false;
}

static bool normalize_struct_generic_parameters(
    frontend_context *context, uint32_t struct_node, uint32_t struct_index,
    uint32_t first_generic_parameter, uint32_t *generic_parameter_count) {
  const w_seed_frontend_document *doc = context_document(context);
  if (generic_parameter_count != NULL) *generic_parameter_count = 0;
  if (context == NULL || doc == NULL || struct_node >= doc->parse.node_count)
    return false;
  const uint32_t generic_node =
      first_direct_kind(doc, struct_node, W_SEED_CST_GENERIC_PARAMETERS);
  if (generic_node == W_SEED_CST_NONE) return true;
  uint32_t cursor = doc->nodes[generic_node].first_child;
  uint32_t child = W_SEED_CST_NONE;
  uint32_t ordinal = 0;
  w_seed_frontend_text prior_names[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  w_seed_frontend_text prior_external_labels[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  bool prior_is_type[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
  (void)memset(prior_names, 0, sizeof(prior_names));
  (void)memset(prior_external_labels, 0, sizeof(prior_external_labels));
  (void)memset(prior_is_type, 0, sizeof(prior_is_type));
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind != W_SEED_CST_GENERIC_PARAMETER) {
      guard += 1;
      continue;
    }
    if (ordinal >= W_SEED_FRONTEND_MAX_GENERIC_SLOTS) {
      (void)context_append_fact(
          context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
          doc->nodes[child].raw_span,
          text_from_span(doc, doc->nodes[child].raw_span));
      break;
    }
    w_seed_frontend_generic_parameter value;
    (void)memset(&value, 0, sizeof(value));
    value.module_index = (uint32_t)context->module_index;
    value.owner_kind = W_SEED_FRONTEND_DECL_STRUCT;
    value.owner_index = struct_index;
    value.ordinal = ordinal;
    value.external_label = generic_parameter_external_label(doc, child);
    value.internal_name = generic_parameter_name(doc, child);
    value.label_kind = generic_parameter_label_omitted(doc, child)
                           ? W_SEED_FRONTEND_LABEL_OPTIONAL
                           : W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY;
    value.kind = W_SEED_FRONTEND_GENERIC_KIND_TYPE;
    value.span = doc->nodes[child].raw_span;
    value.domain_type = W_SEED_FRONTEND_NONE;
    value.refinement_kind = W_SEED_FRONTEND_GENERIC_REFINEMENT_NONE;
    value.predicate_function_index = W_SEED_FRONTEND_NONE;
    value.predicate_span = empty_span(value.span.start_byte);
    value.predicate_function_span = empty_span(value.span.start_byte);
    value.subject_kind = W_SEED_FRONTEND_GENERIC_SUBJECT_NONE;
    value.domain_kind = W_SEED_FRONTEND_GENERIC_DOMAIN_NONE;
    value.dependent_type_parameter_ordinal = W_SEED_FRONTEND_NONE;

    bool duplicate_schema_name = false;
    w_seed_frontend_text duplicate_slot = {NULL, 0u};
    for (size_t prior = 0u; prior < ordinal; prior += 1u) {
      if ((value.internal_name.length != 0u &&
           text_equal_text(value.internal_name, prior_names[prior])) ||
          (value.external_label.length != 0u &&
           text_equal_text(value.external_label,
                           prior_external_labels[prior]))) {
        duplicate_schema_name = true;
        duplicate_slot = value.internal_name.length != 0u
                             ? value.internal_name
                             : value.external_label;
        break;
      }
    }
    if (duplicate_schema_name) {
      value.kind = W_SEED_FRONTEND_GENERIC_KIND_INVALID;
      value.domain_kind = W_SEED_FRONTEND_GENERIC_DOMAIN_INVALID;
      w_seed_frontend_text head = {NULL, 0u};
      w_seed_span head_span = empty_span(value.span.start_byte);
      if (!diagnostic_name_span_after_keyword(
              doc, doc->nodes[struct_node].raw_span, "struct", &head,
              &head_span))
        return false;
      w_seed_frontend_text slot_order[W_SEED_FRONTEND_MAX_GENERIC_SLOTS];
      const size_t slot_order_count = diagnostic_generic_slot_order(
          doc, generic_node, slot_order,
          sizeof(slot_order) / sizeof(slot_order[0]));
      if (duplicate_slot.length == 0u || slot_order_count == 0u) return false;
      (void)append_contract0004_diagnostic(
          context, value.span, head, context->module_index, head_span,
          duplicate_slot, context->module_index, value.span, slot_order,
          slot_order_count,
          (w_seed_frontend_text){"duplicate", sizeof("duplicate") - 1u});
    }

    const uint32_t type_node = generic_parameter_type_node(doc, child);
    if (type_node != W_SEED_CST_NONE && !duplicate_schema_name) {
      value.label_kind = generic_parameter_label_omitted(doc, child)
                             ? W_SEED_FRONTEND_LABEL_OPTIONAL
                             : generic_parameter_word_at(doc, child, 1u)
                                           .length != 0u
                                   ? W_SEED_FRONTEND_LABEL_EXTERNAL_REQUIRED
                                   : W_SEED_FRONTEND_LABEL_NAMED_REQUIRED;
      w_seed_frontend_text dependent_name = {NULL, 0};
      uint32_t dependent_ordinal = W_SEED_FRONTEND_NONE;
      const bool dependent = generic_domain_refers_to_previous_type(
          doc, child, &dependent_name, &dependent_ordinal, prior_names,
          prior_is_type, (size_t)ordinal);
      frontend_simple_type domain_simple = simple_type_unknown();
      const bool domain_supported = dependent ||
          generic_value_domain_supported(context, doc, type_node, &domain_simple);
      value.kind = domain_supported
                       ? W_SEED_FRONTEND_GENERIC_KIND_VALUE
                       : W_SEED_FRONTEND_GENERIC_KIND_INVALID;
      value.domain_kind = dependent
                              ? W_SEED_FRONTEND_GENERIC_DOMAIN_DEPENDENT
                              : domain_supported
                                    ? W_SEED_FRONTEND_GENERIC_DOMAIN_CONCRETE
                                    : W_SEED_FRONTEND_GENERIC_DOMAIN_INVALID;
      value.dependent_type_parameter_ordinal =
          dependent ? dependent_ordinal : W_SEED_FRONTEND_NONE;
      if (!dependent) {
        const bool previous_generic_domain = context->normalizing_generic_domain;
        context->normalizing_generic_domain = true;
        const bool normalized_domain =
            normalize_type_tree(context, type_node, &value.domain_type);
        context->normalizing_generic_domain = previous_generic_domain;
        if (!normalized_domain) return false;
      }
      const uint32_t refinement = generic_refinement_envelope(doc, type_node);
      if (refinement != W_SEED_CST_NONE) {
        value.predicate_span = generic_predicate_span(doc, refinement);
        const w_seed_frontend_text predicate_name =
            generic_predicate_call_name(doc, value.predicate_span);
        const w_seed_frontend_document *predicate_doc = NULL;
        uint32_t predicate_node = W_SEED_CST_NONE;
        if (predicate_name.length == 0u) {
          /* This schema publishes only a direct callable predicate.  Inline
           * refinements and compound/nested calls remain unsupported. */
          value.refinement_kind =
              W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID;
          value.subject_kind = generic_predicate_mentions_member(
                                   doc, value.predicate_span)
                                   ? W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID
                                   : W_SEED_FRONTEND_GENERIC_SUBJECT_NONE;
          (void)context_append_fact(
              context, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
              value.predicate_span,
              text_from_span(doc, value.predicate_span));
        } else if (function_declaration_for_name(context, predicate_name,
                                           &value.predicate_function_index,
                                           &predicate_doc, &predicate_node)) {
          value.predicate_function_span =
              predicate_doc->nodes[predicate_node].raw_span;
          w_seed_frontend_text contract_head = {NULL, 0u};
          w_seed_span contract_head_span =
              empty_span(doc->nodes[struct_node].raw_span.start_byte);
          if (!diagnostic_name_span_after_keyword(
                  doc, doc->nodes[struct_node].raw_span, "struct",
                  &contract_head, &contract_head_span))
            return false;
          const w_seed_frontend_text contract_slot =
              value.internal_name.length != 0u ? value.internal_name
                                               : value.external_label;
          if (contract_slot.length == 0u) return false;
          const size_t contract_head_document = context->module_index;
          const size_t contract_slot_document = context->module_index;
          const bool is_const = function_prefix_has_keyword(
              predicate_doc, value.predicate_function_span, "const");
          bool returns_bool = false;
          const bool signature_compatible = generic_predicate_signature_valid(
              context, doc, type_node, predicate_doc, predicate_node,
              &returns_bool);
          if (!returns_bool) {
            value.refinement_kind =
                W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID;
            value.subject_kind = W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID;
            frontend_simple_type predicate_return = function_return_type(
                context, predicate_doc, predicate_node);
            w_seed_frontend_text predicate_type = predicate_return.spelling;
            if (predicate_type.length == 0u) {
              const uint32_t return_node = first_direct_kind(
                  predicate_doc, predicate_node, W_SEED_CST_RETURN_TYPE);
              const uint32_t return_type =
                  return_node == W_SEED_CST_NONE
                      ? W_SEED_CST_NONE
                      : direct_type_index(predicate_doc, return_node);
              if (return_type != W_SEED_CST_NONE)
                predicate_type = text_from_span(
                    predicate_doc, predicate_doc->nodes[return_type].raw_span);
            }
            if (predicate_type.length == 0u) return false;
            (void)append_contract0003_diagnostic(
                context, value.predicate_span,
                (w_seed_frontend_text){"Bool", sizeof("Bool") - 1u},
                contract_head, contract_head_document, contract_head_span,
                predicate_type, contract_slot_document, value.span);
          } else if (!is_const) {
            value.refinement_kind =
                W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID;
            value.subject_kind = W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID;
            const w_seed_frontend_text call_chain[1] = {predicate_name};
            (void)append_const0001_diagnostic(
                context, value.predicate_span, call_chain, 1u,
                (w_seed_frontend_text){"call", sizeof("call") - 1u},
                (w_seed_frontend_text){"not const-safe",
                                       sizeof("not const-safe") - 1u},
                predicate_name,
                context_document_index_for(context, predicate_doc),
                value.predicate_function_span);
          } else if (!signature_compatible) {
            value.refinement_kind =
                W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID;
            value.subject_kind = W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID;
            const uint32_t parameter_list = first_direct_kind(
                predicate_doc, predicate_node, W_SEED_CST_PARAMETER_LIST);
            const size_t parameter_count =
                parameter_list == W_SEED_CST_NONE
                    ? 0u
                    : count_direct_kind(predicate_doc, parameter_list,
                                         W_SEED_CST_PARAMETER);
            w_seed_frontend_text actual_kind = {NULL, 0u};
            w_seed_frontend_text expected_kind = {NULL, 0u};
            if (parameter_count != 1u) {
              if (!diagnostic_category_arity(parameter_count, &actual_kind))
                return false;
              expected_kind = (w_seed_frontend_text){
                  "arity:1", sizeof("arity:1") - 1u};
            } else {
              const uint32_t parameter = first_direct_kind(
                  predicate_doc, parameter_list, W_SEED_CST_PARAMETER);
              const uint32_t parameter_type =
                  parameter == W_SEED_CST_NONE
                      ? W_SEED_CST_NONE
                      : direct_type_index(predicate_doc, parameter);
              if (parameter_type == W_SEED_CST_NONE) return false;
              const w_seed_frontend_text actual_type = text_from_span(
                  predicate_doc, predicate_doc->nodes[parameter_type].raw_span);
              const w_seed_frontend_text expected_type = text_from_span(
                  doc, generic_base_type_span(doc, type_node));
              if (actual_type.length == 0u || expected_type.length == 0u ||
                  !diagnostic_category_compose(
                      (w_seed_frontend_text){"value:", sizeof("value:") - 1u},
                      actual_type, &actual_kind) ||
                  !diagnostic_category_compose(
                      (w_seed_frontend_text){"value:", sizeof("value:") - 1u},
                      expected_type, &expected_kind))
                return false;
            }
            (void)append_contract0002_diagnostic(
                context, value.predicate_span, actual_kind, expected_kind,
                contract_head, contract_head_document, contract_head_span,
                contract_slot, contract_slot_document, value.span);
          } else {
            value.refinement_kind =
                W_SEED_FRONTEND_GENERIC_REFINEMENT_PREDICATE;
            value.subject_kind = W_SEED_FRONTEND_GENERIC_SUBJECT_MEMBER;
          }
        } else {
          value.predicate_function_index = W_SEED_FRONTEND_NONE;
          value.refinement_kind = W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID;
          value.subject_kind = W_SEED_FRONTEND_GENERIC_SUBJECT_INVALID;
          (void)context_append_fact(
              context, W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL,
              value.predicate_span, predicate_name);
        }
      }
      if (!domain_supported) {
        const w_seed_frontend_text domain =
            text_from_span(doc, generic_base_type_span(doc, type_node));
        const w_seed_frontend_text parameter =
            value.internal_name.length != 0u ? value.internal_name
                                             : value.external_label;
        if (domain.length == 0u || parameter.length == 0u) return false;
        const w_seed_frontend_text resolution_reason =
            domain_simple.kind == W_SEED_FRONTEND_TYPE_NOMINAL
                ? (w_seed_frontend_text){"unresolved-domain",
                                         sizeof("unresolved-domain") - 1u}
                : (w_seed_frontend_text){
                      "not-static-argument-representable",
                      sizeof("not-static-argument-representable") - 1u};
        (void)append_generic0001_diagnostic(
            context, doc->nodes[type_node].raw_span, domain, parameter,
            resolution_reason, context->module_index, value.span);
      }
    } else if (value.internal_name.length == 0) {
      value.kind = W_SEED_FRONTEND_GENERIC_KIND_INVALID;
      value.refinement_kind = W_SEED_FRONTEND_GENERIC_REFINEMENT_INVALID;
      (void)context_append_fact(
          context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
          doc->nodes[child].raw_span, text_from_span(doc, doc->nodes[child].raw_span));
    }
    const size_t prior_count = generic_parameter_count == NULL
                                   ? 0u
                                   : (size_t)*generic_parameter_count;
    size_t expected_size = 0;
    uint32_t expected_index = W_SEED_FRONTEND_NONE;
    if (!add_size((size_t)first_generic_parameter, prior_count,
                  &expected_size) ||
        !add_u32(expected_size, &expected_index) ||
        context->count.generic_parameters != expected_size) {
      return false;
    }
    uint32_t generic_index = W_SEED_FRONTEND_NONE;
    if (!context_append_generic_parameter(context, value, &generic_index) ||
        generic_index != expected_index)
      return false;
    if (generic_index < FRONTEND_MAX_GENERIC_METADATA) {
      context->generic_domain_type_indices[generic_index] = value.domain_type;
      context->generic_refinement_kinds[generic_index] =
          value.refinement_kind;
    } else
      (void)context_append_fact(
          context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE, value.span,
          text_from_span(doc, value.span));
    if (ordinal < FRONTEND_MAX_GENERIC_METADATA) {
      prior_names[ordinal] = value.internal_name;
      prior_external_labels[ordinal] = value.external_label;
      prior_is_type[ordinal] = value.kind == W_SEED_FRONTEND_GENERIC_KIND_TYPE;
    }
    if (generic_parameter_count != NULL) *generic_parameter_count += 1u;
    ordinal += 1u;
    guard += 1;
  }
  return true;
}

static bool normalize_struct(frontend_context *context, uint32_t node_index,
                             uint32_t *struct_index) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || struct_index == NULL) return false;
  const w_seed_cst_node *node = &doc->nodes[node_index];
  w_seed_frontend_struct value;
  (void)memset(&value, 0, sizeof(value));
  value.module_index = (uint32_t)context->module_index;
  value.name = name_after_keyword(doc, node->raw_span, "struct");
  value.exported = span_has_keyword(doc, node->raw_span, "export");
  value.span = node->raw_span;
  value.first_generic_parameter = (uint32_t)context->count.generic_parameters;
  value.generic_parameter_count = 0;
  value.first_field = (uint32_t)context->count.fields;
  value.field_count = (uint32_t)count_direct_kind(doc, node_index,
                                                   W_SEED_CST_FIELD);
  if (!context_append_struct(context, value, struct_index)) return false;
  uint32_t symbol_index = W_SEED_FRONTEND_NONE;
  if (!normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_STRUCT, *struct_index,
                        value.name, value.exported, value.span,
                        W_SEED_FRONTEND_NONE, &symbol_index)) {
    return false;
  }
  if (!normalize_struct_generic_parameters(
          context, node_index, *struct_index,
          value.first_generic_parameter, &value.generic_parameter_count)) {
    return false;
  }
  if (context->emit && context->output != NULL &&
      *struct_index < context->output->struct_capacity) {
    context->output->structs[*struct_index].generic_parameter_count =
        value.generic_parameter_count;
  }
  uint32_t child_cursor = node->first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &child_cursor, &child) &&
         guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_FIELD) {
      w_seed_frontend_field field;
      (void)memset(&field, 0, sizeof(field));
      field.module_index = (uint32_t)context->module_index;
      field.owner_struct = *struct_index;
      field.name = first_word_in_span(doc, doc->nodes[child].raw_span);
      field.span = doc->nodes[child].raw_span;
      field.type_index = W_SEED_FRONTEND_NONE;
      const uint32_t type_node = direct_type_index(doc, child);
      if (type_node != W_SEED_CST_NONE &&
          !normalize_type_tree(context, type_node, &field.type_index)) {
        return false;
      }
      uint32_t field_index = W_SEED_FRONTEND_NONE;
      if (!context_append_field(context, field, &field_index) ||
          !normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_FIELD, field_index,
                            field.name, value.exported, field.span,
                            field.type_index, &symbol_index)) {
        return false;
      }
    }
    guard += 1;
  }
  return true;
}

static bool normalize_enum(frontend_context *context, uint32_t node_index,
                           uint32_t *enum_index);

static bool normalize_enum(frontend_context *context, uint32_t node_index,
                           uint32_t *enum_index) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || enum_index == NULL || node_index >= doc->parse.node_count)
    return false;
  const w_seed_cst_node *node = &doc->nodes[node_index];
  w_seed_frontend_enum value;
  (void)memset(&value, 0, sizeof(value));
  value.module_index = (uint32_t)context->module_index;
  value.name = name_after_keyword(doc, node->raw_span, "enum");
  value.exported = span_has_keyword(doc, node->raw_span, "export");
  value.span = node->raw_span;
  value.generic_span = empty_span(node->raw_span.start_byte);
  value.has_generic_parameters = false;
  value.conformance_type = W_SEED_FRONTEND_NONE;
  value.conformance_span = empty_span(node->raw_span.start_byte);
  value.first_case = (uint32_t)context->count.enum_cases;
  value.case_count = (uint32_t)count_direct_kind(doc, node_index,
                                                  W_SEED_CST_ENUM_CASE);
  value.type_index = W_SEED_FRONTEND_NONE;

  const uint32_t generic_node =
      first_direct_kind(doc, node_index, W_SEED_CST_GENERIC_PARAMETERS);
  if (generic_node != W_SEED_CST_NONE) {
    value.generic_span = doc->nodes[generic_node].raw_span;
    value.has_generic_parameters = true;
  }

  /* Emit the enum's canonical nominal type before its conformance surface.
   * This makes the type index stable and independent of payload traversal. */
  w_seed_frontend_type enum_type;
  (void)memset(&enum_type, 0, sizeof(enum_type));
  enum_type.kind = W_SEED_FRONTEND_TYPE_ENUM;
  enum_type.spelling = value.name;
  enum_type.nominal_name = value.name;
  enum_type.span = node->raw_span;
  enum_type.element_type = W_SEED_FRONTEND_NONE;
  enum_type.return_type = W_SEED_FRONTEND_NONE;
  enum_type.first_parameter = W_SEED_FRONTEND_NONE;
  enum_type.enum_base_index = (uint32_t)context->count.enums;
  enum_type.first_subset_member = W_SEED_FRONTEND_NONE;
  enum_type.subset_member_count = 0;
  enum_type.generic_application_index = W_SEED_FRONTEND_NONE;
  if (!context_append_type(context, enum_type, &value.type_index)) return false;

  const uint32_t conformance_node = direct_type_index(doc, node_index);
  if (conformance_node != W_SEED_CST_NONE)
    value.conformance_span = doc->nodes[conformance_node].raw_span;
  if (conformance_node != W_SEED_CST_NONE &&
      !normalize_type_tree(context, conformance_node,
                           &value.conformance_type)) {
    return false;
  }
  if (value.has_generic_parameters) {
    (void)context_append_fact(
        context, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE, value.generic_span,
        text_from_span(doc, value.generic_span));
  }
  if (!context_append_enum(context, value, enum_index)) return false;
  uint32_t symbol_index = W_SEED_FRONTEND_NONE;
  if (!normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_ENUM, *enum_index,
                        value.name, value.exported, value.span, value.type_index,
                        &symbol_index)) {
    return false;
  }

  uint32_t case_cursor = node->first_child;
  uint32_t case_node = W_SEED_CST_NONE;
  size_t case_guard = 0;
  while (next_child(doc, &case_cursor, &case_node) &&
         case_guard < doc->parse.node_count) {
    if (doc->nodes[case_node].kind != W_SEED_CST_ENUM_CASE) {
      case_guard += 1;
      continue;
    }
    const w_seed_cst_node *case_cst = &doc->nodes[case_node];
    w_seed_frontend_enum_case case_value;
    (void)memset(&case_value, 0, sizeof(case_value));
    case_value.module_index = (uint32_t)context->module_index;
    case_value.owner_enum = *enum_index;
    case_value.name = first_word_in_span(doc, case_cst->raw_span);
    case_value.span = case_cst->raw_span;
    case_value.first_payload = (uint32_t)context->count.enum_case_parameters;
    case_value.payload_count = (uint32_t)count_direct_kind(
        doc, case_node, W_SEED_CST_ENUM_CASE_PARAMETER);
    uint32_t case_index = W_SEED_FRONTEND_NONE;
    if (!context_append_enum_case(context, case_value, &case_index)) return false;
    if (!normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_ENUM_CASE,
                          case_index, case_value.name, value.exported,
                          case_value.span, value.type_index, &symbol_index)) {
      return false;
    }

    uint32_t parameter_cursor = case_cst->first_child;
    uint32_t parameter_node = W_SEED_CST_NONE;
    size_t parameter_guard = 0;
    while (next_child(doc, &parameter_cursor, &parameter_node) &&
           parameter_guard < doc->parse.node_count) {
      if (doc->nodes[parameter_node].kind !=
          W_SEED_CST_ENUM_CASE_PARAMETER) {
        parameter_guard += 1;
        continue;
      }
      const w_seed_cst_node *parameter_cst = &doc->nodes[parameter_node];
      w_seed_frontend_enum_case_parameter parameter;
      (void)memset(&parameter, 0, sizeof(parameter));
      parameter.module_index = (uint32_t)context->module_index;
      parameter.owner_case = case_index;
      parameter.label = enum_case_parameter_label(doc, parameter_node);
      parameter.has_label = parameter.label.length != 0;
      parameter.span = parameter_cst->raw_span;
      parameter.type_index = W_SEED_FRONTEND_NONE;
      const uint32_t type_node = direct_type_index(doc, parameter_node);
      if (type_node != W_SEED_CST_NONE &&
          !normalize_type_tree(context, type_node, &parameter.type_index)) {
        return false;
      }
      uint32_t parameter_index = W_SEED_FRONTEND_NONE;
      if (!context_append_enum_case_parameter(context, parameter,
                                              &parameter_index)) {
        return false;
      }
      parameter_guard += 1;
    }
    if (context->emit && context->output != NULL &&
        case_index < context->output->enum_case_capacity) {
      context->output->enum_cases[case_index].payload_count =
          (uint32_t)(context->count.enum_case_parameters -
                     (size_t)case_value.first_payload);
    }
    case_guard += 1;
  }
  if (context->emit && context->output != NULL &&
      *enum_index < context->output->enum_capacity) {
    context->output->enums[*enum_index].case_count =
        (uint32_t)(context->count.enum_cases - (size_t)value.first_case);
  }
  return true;
}

static bool normalize_type_declaration(frontend_context *context,
                                       uint32_t node_index, bool alias,
                                       uint32_t *declaration_index) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || declaration_index == NULL) return false;
  const w_seed_cst_node *node = &doc->nodes[node_index];
  const char *keyword = alias ? "alias" : "type";
  const w_seed_frontend_text name = name_after_keyword(doc, node->raw_span,
                                                        keyword);
  const uint32_t type_node = direct_type_index(doc, node_index);
  uint32_t type_index = W_SEED_FRONTEND_NONE;
  if (type_node != W_SEED_CST_NONE &&
      !normalize_type_tree(context, type_node, &type_index)) {
    return false;
  }
  if (alias) {
    w_seed_frontend_alias value;
    (void)memset(&value, 0, sizeof(value));
    value.module_index = (uint32_t)context->module_index;
    value.name = name;
    value.exported = span_has_keyword(doc, node->raw_span, "export");
    value.span = node->raw_span;
    value.type_index = type_index;
    if (!context_append_alias(context, value, declaration_index)) return false;
    uint32_t symbol_index = W_SEED_FRONTEND_NONE;
    return normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_ALIAS,
                            *declaration_index, name, value.exported, value.span,
                            type_index, &symbol_index);
  }
  w_seed_frontend_type_declaration value;
  (void)memset(&value, 0, sizeof(value));
  value.module_index = (uint32_t)context->module_index;
  value.name = name;
  value.exported = span_has_keyword(doc, node->raw_span, "export");
  value.span = node->raw_span;
  value.type_index = type_index;
  if (!context_append_type_declaration(context, value, declaration_index)) {
    return false;
  }
  uint32_t symbol_index = W_SEED_FRONTEND_NONE;
  return normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_TYPE,
                          *declaration_index, name, value.exported, value.span,
                          type_index, &symbol_index);
}

static bool normalize_block_statements(frontend_context *context,
                                       uint32_t block_node);
static bool normalize_statement_depth(frontend_context *context,
                                      uint32_t node_index,
                                      uint32_t *statement_index,
                                      size_t depth);
static bool normalize_block_statements_depth(frontend_context *context,
                                             uint32_t block_node,
                                             size_t depth,
                                             uint32_t *first_statement,
                                             uint32_t *statement_count);

static bool normalize_function(frontend_context *context, uint32_t node_index,
                               uint32_t *function_index) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || function_index == NULL) return false;
  const w_seed_cst_node *node = &doc->nodes[node_index];
  w_seed_frontend_function value;
  (void)memset(&value, 0, sizeof(value));
  value.module_index = (uint32_t)context->module_index;
  value.name = name_after_keyword(doc, node->raw_span, "fn");
  value.exported = function_prefix_has_keyword(doc, node->raw_span, "export");
  value.is_const = function_prefix_has_keyword(doc, node->raw_span, "const");
  value.const_body_supported = value.is_const;
  value.is_async = (node->flags & W_SEED_CST_FUNCTION_FLAG_ASYNC) != 0u;
  value.is_throws = (node->flags & W_SEED_CST_FUNCTION_FLAG_THROWS) != 0u;
  value.is_unsafe = (node->flags & W_SEED_CST_FUNCTION_FLAG_UNSAFE) != 0u;
  value.has_borrow_clause =
      (node->flags & W_SEED_CST_FUNCTION_FLAG_BORROWS) != 0u;
  value.span = node->raw_span;
  value.body_span = empty_span(node->raw_span.end_byte);
  value.first_parameter = (uint32_t)context->count.parameters;
  value.parameter_count =
      (uint32_t)count_direct_kind(doc, node_index, W_SEED_CST_PARAMETER);
  value.return_type = W_SEED_FRONTEND_NONE;
  value.first_statement = (uint32_t)context->count.statements;
  value.statement_count = 0;
  context->current_function_is_const = value.is_const;
  context->current_const_body_active = false;
  context->current_const_body_supported = value.const_body_supported;
  context->current_const_root_emitted = false;
  if (!context_append_function(context, value, function_index)) return false;
  context->function_index = *function_index;
  context->function_node = node;

  const uint32_t parameters_node =
      first_direct_kind(doc, node_index, W_SEED_CST_PARAMETER_LIST);
  if (parameters_node != W_SEED_CST_NONE) {
    uint32_t child_cursor = doc->nodes[parameters_node].first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t guard = 0;
    while (next_child(doc, &child_cursor, &child) &&
           guard < doc->parse.node_count) {
      if (doc->nodes[child].kind == W_SEED_CST_PARAMETER) {
        w_seed_frontend_parameter parameter;
        (void)memset(&parameter, 0, sizeof(parameter));
        parameter.module_index = (uint32_t)context->module_index;
        parameter.owner_function = *function_index;
        parameter.name = parameter_name_from_span(doc, doc->nodes[child].raw_span);
        parameter.label =
            parameter_external_label_from_span(doc, doc->nodes[child].raw_span);
        parameter.label_kind = parameter_label_kind(doc, doc->nodes[child].raw_span);
        parameter.span = doc->nodes[child].raw_span;
        parameter.type_index = W_SEED_FRONTEND_NONE;
        const uint32_t type_node = direct_type_index(doc, child);
        if (type_node != W_SEED_CST_NONE &&
            !normalize_type_tree(context, type_node, &parameter.type_index)) {
          return false;
        }
        uint32_t parameter_index = W_SEED_FRONTEND_NONE;
        if (!context_append_parameter(context, parameter, &parameter_index)) {
          return false;
        }
        uint32_t symbol_index = W_SEED_FRONTEND_NONE;
        if (!normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_PARAMETER,
                              parameter_index, parameter.name, false,
                              parameter.span, parameter.type_index,
                              &symbol_index)) {
          return false;
        }
      }
      guard += 1;
    }
  }
  const uint32_t return_node =
      first_direct_kind(doc, node_index, W_SEED_CST_RETURN_TYPE);
  if (return_node != W_SEED_CST_NONE) {
    const uint32_t type_node = direct_type_index(doc, return_node);
    if (type_node != W_SEED_CST_NONE &&
        !normalize_type_tree(context, type_node, &value.return_type)) {
      return false;
    }
  } else {
    const w_seed_frontend_type unit =
        inferred_unit_type(empty_span(node->raw_span.end_byte));
    if (!context_append_type(context, unit, &value.return_type)) return false;
  }
  const uint32_t block_node = first_direct_kind(doc, node_index, W_SEED_CST_BLOCK);
  if (block_node != W_SEED_CST_NONE) {
    value.body_span = doc->nodes[block_node].raw_span;
    context->current_const_body_active = value.is_const;
    if (!normalize_block_statements(context, block_node)) return false;
    context->current_const_body_active = false;
  } else if (value.is_const) {
    context->current_const_body_active = true;
    (void)const_record_failure(context, node->raw_span,
                               text_from_span(doc, node->raw_span));
    context->current_const_body_active = false;
  }
  value.const_body_supported = context->current_const_body_supported;
  value.parameter_count = (uint32_t)(context->count.parameters -
                                     (size_t)value.first_parameter);
  value.statement_count = (uint32_t)(context->count.statements -
                                     (size_t)value.first_statement);
  if (context->emit && context->output != NULL &&
      *function_index < context->output->function_capacity) {
    context->output->functions[*function_index] = value;
  }
  if (!context->emit && !receipt_size_function(context, &value)) return false;
  uint32_t symbol_index = W_SEED_FRONTEND_NONE;
  return normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_FUNCTION,
                          *function_index, value.name, value.exported,
                          value.span, value.return_type, &symbol_index);
}

static bool normalize_entry(frontend_context *context, uint32_t node_index,
                            uint32_t *entry_index) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || entry_index == NULL) return false;
  const w_seed_cst_node *node = &doc->nodes[node_index];
  w_seed_frontend_entry value;
  value.module_index = (uint32_t)context->module_index;
  value.target = name_after_keyword(doc, node->raw_span, "entry");
  value.span = node->raw_span;
  value.valid = false;
  const uint32_t root = doc->parse.root;
  uint32_t child_cursor = doc->nodes[root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &child_cursor, &child) &&
         guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_FUNCTION &&
        text_equal_text(name_after_keyword(doc, doc->nodes[child].raw_span, "fn"),
                        value.target)) {
      value.valid = true;
      break;
    }
    guard += 1;
  }
  if (!value.valid) {
    (void)context_append_fact(context, W_SEED_FRONTEND_FACT_INVALID_ENTRY,
                              value.span, value.target);
  }
  if (!context_append_entry(context, value, entry_index)) return false;
  uint32_t symbol_index = W_SEED_FRONTEND_NONE;
  if (!normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_ENTRY, *entry_index,
                        value.target, false, value.span,
                        W_SEED_FRONTEND_NONE, &symbol_index)) {
    return false;
  }
  return true;
}

typedef struct {
  frontend_context *context;
  const w_seed_frontend_document *document;
  frontend_token_cursor cursor;
  size_t depth;
  frontend_simple_type expected_type;
  bool has_expected_type;
  bool suppress_short_diagnostic;
} frontend_expression_parser;

static frontend_simple_type simple_type_from_view(w_seed_frontend_text spelling) {
  frontend_simple_type type = simple_type_unknown();
  type.spelling = spelling;
  if (text_equal(spelling, "()")) {
    type.kind = W_SEED_FRONTEND_TYPE_UNIT;
  } else if (text_equal(spelling, "Bool")) {
    type.kind = W_SEED_FRONTEND_TYPE_BOOL;
  } else if (text_equal(spelling, "String")) {
    type.kind = W_SEED_FRONTEND_TYPE_STRING;
  } else if (text_equal(spelling, "bytes") || text_equal(spelling, "Bytes")) {
    type.kind = W_SEED_FRONTEND_TYPE_BYTES;
  } else if (text_equal(spelling, "usize")) {
    type.kind = W_SEED_FRONTEND_TYPE_INTEGER;
    type.is_signed = false;
    type.bit_width = (uint16_t)W_SEED_FRONTEND_TARGET_USIZE_BITS;
  } else if (spelling.length >= 11u &&
             memcmp(spelling.data, "StaticList<", 11u) == 0 &&
             spelling.data[spelling.length - 1u] == '>') {
    type.kind = W_SEED_FRONTEND_TYPE_STATIC_LIST;
  } else if (spelling.length >= 6u &&
             memcmp(spelling.data, "Range<", 6u) == 0 &&
             spelling.data[spelling.length - 1u] == '>') {
    type.kind = W_SEED_FRONTEND_TYPE_RANGE;
  } else if (text_equal(spelling, "f32") || text_equal(spelling, "f64")) {
    type.kind = W_SEED_FRONTEND_TYPE_FLOAT;
    type.bit_width = (uint16_t)(text_equal(spelling, "f32") ? 32u : 64u);
  } else {
    bool is_signed = false;
    uint16_t width = 0;
    if (type_name_integer(spelling, &is_signed, &width) &&
        integer_width_supported(width)) {
      type.kind = W_SEED_FRONTEND_TYPE_INTEGER;
      type.is_signed = is_signed;
      type.bit_width = width;
    } else if (text_equal(spelling, "Int") || text_equal(spelling, "UInt")) {
      type.kind = W_SEED_FRONTEND_TYPE_INTEGER;
      type.is_signed = text_equal(spelling, "Int");
      type.bit_width = 64u;
    } else if (looks_like_integer_type(spelling)) {
      return type;
    } else if (spelling.length != 0 && spelling.data[spelling.length - 1] == '?') {
      type.kind = W_SEED_FRONTEND_TYPE_OPTION;
    } else if (spelling.length != 0) {
      type.kind = W_SEED_FRONTEND_TYPE_NOMINAL;
    }
  }
  return type;
}

static void static_list_set_element(frontend_simple_type *list_type,
                                     frontend_simple_type element) {
  if (list_type == NULL) return;
  list_type->element_kind = element.kind;
  list_type->element_is_signed = element.is_signed;
  list_type->element_bit_width = element.bit_width;
  list_type->element_enum_index = element.enum_index;
  list_type->element_spelling = element.spelling;
  list_type->element_enum_name = element.enum_name;
}

static bool static_list_element_from_span(
    const frontend_context *context, const w_seed_frontend_document *doc,
    w_seed_span span, frontend_simple_type *list_type) {
  if (context == NULL || doc == NULL || list_type == NULL ||
      list_type->kind != W_SEED_FRONTEND_TYPE_STATIC_LIST)
    return false;
  uint32_t type_node = W_SEED_CST_NONE;
  if (!find_type_node_for_span(doc, span, &type_node)) return false;
  uint32_t cursor = doc->nodes[type_node].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_CONTRACT_ENVELOPE &&
        contract_envelope_has_type(doc, child)) {
      const uint32_t element_node =
          first_direct_kind(doc, child, W_SEED_CST_TYPE);
      if (element_node == W_SEED_CST_NONE) return false;
      frontend_simple_type element = contextual_type_from_span(
          context, doc, doc->nodes[element_node].raw_span);
      if (element.kind == W_SEED_FRONTEND_TYPE_UNKNOWN) return false;
      if (element.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST) {
        (void)static_list_element_from_span(
            context, doc, doc->nodes[element_node].raw_span, &element);
      }
      static_list_set_element(list_type, element);
      return true;
    }
    guard += 1u;
  }
  return false;
}

static frontend_simple_type static_list_element_type(
    const frontend_context *context, frontend_simple_type list_type) {
  if (list_type.kind != W_SEED_FRONTEND_TYPE_STATIC_LIST ||
      list_type.element_kind != W_SEED_FRONTEND_TYPE_UNKNOWN) {
    if (list_type.kind != W_SEED_FRONTEND_TYPE_STATIC_LIST)
      return simple_type_unknown();
    frontend_simple_type element = simple_type_unknown();
    element.kind = list_type.element_kind;
    element.is_signed = list_type.element_is_signed;
    element.bit_width = list_type.element_bit_width;
    element.enum_index = list_type.element_enum_index;
    element.spelling = list_type.element_spelling;
    element.enum_name = list_type.element_enum_name;
    return element;
  }
  /* A StaticList without CST-derived element metadata is not a resolvable
   * domain. Never recover it by slicing/reparsing the raw spelling: comments,
   * nested envelopes and close-token ownership are semantic CST boundaries. */
  (void)context;
  return simple_type_unknown();
}

static frontend_simple_type literal_simple_type(
    const w_seed_frontend_document *doc, w_seed_span span,
    w_seed_cst_kind token_kind) {
  const w_seed_span trimmed = trim_span(doc, span);
  const w_seed_frontend_text text = text_from_span(doc, trimmed);
  frontend_simple_type type = simple_type_unknown();
  type.spelling = text;
  if (token_kind == W_SEED_CST_LITERAL_EVENT ||
      (text.length != 0 && (text.data[0] == '"' || text.data[0] == '\'' ||
                            (text.length > 1 && text.data[0] == 'b')))) {
    type.kind = W_SEED_FRONTEND_TYPE_STRING;
    if (text.length > 1 && text.data[0] == 'b')
      type.kind = W_SEED_FRONTEND_TYPE_BYTES;
    return type;
  }
  if (text_equal(text, "true") || text_equal(text, "false")) {
    type.kind = W_SEED_FRONTEND_TYPE_BOOL;
    return type;
  }
  bool floating = false;
  const bool hexadecimal =
      text.length >= 2 && text.data[0] == '0' &&
      (text.data[1] == 'x' || text.data[1] == 'X');
  for (size_t index = 0; index < text.length; index += 1) {
    if (text.data[index] == '.' ||
        (!hexadecimal && (text.data[index] == 'e' ||
                          text.data[index] == 'E'))) {
      floating = true;
      break;
    }
  }
  if (floating) {
    type.kind = W_SEED_FRONTEND_TYPE_FLOAT;
    type.bit_width = 64;
    if (text.length >= 4 &&
        (text.data[text.length - 3] == 'f' ||
         text.data[text.length - 3] == 'F') &&
        text.data[text.length - 2] == '3' && text.data[text.length - 1] == '2') {
      type.bit_width = 32;
    }
  } else {
    size_t body_end = text.length;
    bool has_suffix = false;
    bool is_signed = true;
    uint16_t width = 0;
    uint64_t value = 0;
    if (!integer_literal_parts(text, &body_end, &has_suffix, &is_signed,
                               &width) ||
        !integer_literal_value(text, body_end, &value)) {
      /* Keep the source spelling in an UNKNOWN record.  The normalizer emits
       * an unsupported-expression fact for this literal, so malformed or
       * overflowing suffixes are never silently accepted. */
      return type;
    }
    if (!has_suffix) {
      type.kind = W_SEED_FRONTEND_TYPE_INTEGER;
      type.is_signed = true;
      type.bit_width = 0;
    } else {
      if (!integer_width_supported(width)) return type;
      const uint64_t maximum =
          width < 64u
              ? (is_signed ? (UINT64_C(1) << (width - 1u)) - 1u
                           : (UINT64_C(1) << width) - 1u)
              : (is_signed ? (uint64_t)INT64_MAX : UINT64_MAX);
      if (value > maximum) return type;
      type.kind = W_SEED_FRONTEND_TYPE_INTEGER;
      type.is_signed = is_signed;
      type.bit_width = width;
    }
  }
  return type;
}

static bool diagnostic_value_kind_from_simple(
    frontend_simple_type type, bool use_spelling, w_seed_frontend_text *out) {
  if (out != NULL) *out = (w_seed_frontend_text){NULL, 0u};
  if (out == NULL) return false;
  if (use_spelling && type.spelling.data != NULL &&
      type.spelling.length != 0u) {
    return diagnostic_category_compose(
        (w_seed_frontend_text){"value:", sizeof("value:") - 1u},
        type.spelling, out);
  }
  switch (type.kind) {
    case W_SEED_FRONTEND_TYPE_BOOL:
      *out = (w_seed_frontend_text){"value:Bool", sizeof("value:Bool") - 1u};
      return true;
    case W_SEED_FRONTEND_TYPE_INTEGER:
      *out = (w_seed_frontend_text){"value:integer",
                                    sizeof("value:integer") - 1u};
      return true;
    case W_SEED_FRONTEND_TYPE_FLOAT:
      *out = (w_seed_frontend_text){"value:float", sizeof("value:float") - 1u};
      return true;
    case W_SEED_FRONTEND_TYPE_STRING:
      *out = (w_seed_frontend_text){"value:String",
                                    sizeof("value:String") - 1u};
      return true;
    case W_SEED_FRONTEND_TYPE_BYTES:
      *out = (w_seed_frontend_text){"value:bytes",
                                    sizeof("value:bytes") - 1u};
      return true;
    case W_SEED_FRONTEND_TYPE_UNIT:
      *out = (w_seed_frontend_text){"value:()", sizeof("value:()") - 1u};
      return true;
    case W_SEED_FRONTEND_TYPE_STATIC_LIST:
      *out = (w_seed_frontend_text){"value:static-list",
                                    sizeof("value:static-list") - 1u};
      return true;
    default:
      *out = (w_seed_frontend_text){"value:unresolved",
                                    sizeof("value:unresolved") - 1u};
      return true;
  }
}

static frontend_simple_type simple_type_from_frontend_type(
    const w_seed_frontend_type *source) {
  frontend_simple_type type = simple_type_unknown();
  if (source == NULL) return type;
  type.kind = source->kind;
  type.is_signed = source->is_signed;
  type.bit_width = source->bit_width;
  type.spelling = source->spelling;
  type.enum_index = source->enum_base_index;
  type.enum_name = source->nominal_name;
  return type;
}

static bool module_const_index_for_node(const frontend_context *context,
                                        size_t document_index,
                                        uint32_t node_index,
                                        uint32_t *index) {
  if (context == NULL || index == NULL ||
      document_index >= context->input.document_count) return false;
  size_t ordinal = context->const_document_bases[document_index];
  const w_seed_frontend_document *doc = &context->input.documents[document_index];
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_CONST_DECLARATION) {
      if (child == node_index) {
        if (ordinal >=
            (size_t)W_SEED_FRONTEND_MAX_CONST_DECLARATIONS)
          return false;
        *index = (uint32_t)ordinal;
        return true;
      }
      ordinal += 1u;
    }
    guard += 1u;
  }
  return false;
}

/* Resolve only module-level const declarations from the current module.
 * This helper reads CST for forward references.  It never reads an import or
 * an ambient symbol table. */
static bool module_const_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    uint32_t *const_index, frontend_simple_type *type,
    const w_seed_frontend_document **owner_doc, uint32_t *owner_node) {
  if (const_index != NULL) *const_index = W_SEED_FRONTEND_NONE;
  if (type != NULL) *type = simple_type_unknown();
  if (owner_doc != NULL) *owner_doc = NULL;
  if (owner_node != NULL) *owner_node = W_SEED_CST_NONE;
  if (context == NULL || name.length == 0u ||
      context->module_index >= context->input.document_count)
    return false;
  const w_seed_frontend_document *doc =
      &context->input.documents[context->module_index];
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0u;
  bool found = false;
  uint32_t found_index = W_SEED_FRONTEND_NONE;
  frontend_simple_type found_type = simple_type_unknown();
  const w_seed_frontend_document *found_doc = NULL;
  uint32_t found_node = W_SEED_CST_NONE;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind != W_SEED_CST_CONST_DECLARATION) {
      guard += 1u;
      continue;
    }
    const w_seed_frontend_text candidate =
        name_after_keyword(doc, doc->nodes[child].raw_span, "const");
    if (!text_equal_text(candidate, name)) {
      guard += 1u;
      continue;
    }
    uint32_t index = W_SEED_FRONTEND_NONE;
    if (!module_const_index_for_node(context, context->module_index, child,
                                     &index))
      return false;
    const uint32_t type_node = direct_type_index(doc, child);
    if (found) {
      /* Keep the existing duplicate-symbol fact as the public barrier.  A
       * resolver must not silently choose one of two same-module consts. */
      return false;
    }
    found = true;
    found_index = index;
    found_doc = doc;
    found_node = child;
    found_type = type_node == W_SEED_CST_NONE
                     ? simple_type_unknown()
                     : contextual_type_from_span(
                           context, doc, doc->nodes[type_node].raw_span);
    if (context->emit && context->output != NULL &&
        context->output->const_declarations != NULL &&
        (size_t)index < context->count.const_declarations) {
      const w_seed_frontend_const_declaration *record =
          &context->output->const_declarations[index];
      if (record->effective_type != W_SEED_FRONTEND_NONE &&
          (size_t)record->effective_type < context->count.types) {
        found_type = simple_type_from_frontend_type(
            &context->output->types[record->effective_type]);
      }
    }
    if (found_type.kind == W_SEED_FRONTEND_TYPE_UNKNOWN &&
        index < W_SEED_FRONTEND_MAX_CONST_DECLARATIONS &&
        context->const_inferred_types[index].known) {
      found_type = const_inferred_simple_type(
          &context->const_inferred_types[index]);
    }
  }
  if (!found) return false;
  if (const_index != NULL) *const_index = found_index;
  if (type != NULL) *type = found_type;
  if (owner_doc != NULL) *owner_doc = found_doc;
  if (owner_node != NULL) *owner_node = found_node;
  return true;
}

static bool function_in_document(const w_seed_frontend_document *doc,
                                 w_seed_frontend_text name,
                                 bool require_export,
                                 uint32_t *function_node) {
  if (doc == NULL || function_node == NULL) return false;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_FUNCTION &&
        text_equal_text(name_after_keyword(doc, doc->nodes[child].raw_span, "fn"),
                        name) &&
        (!require_export ||
         span_has_keyword(doc, doc->nodes[child].raw_span, "export"))) {
      *function_node = child;
      return true;
    }
    guard += 1;
  }
  return false;
}

static bool function_node_is_const(const w_seed_frontend_document *doc,
                                   uint32_t function_node) {
  if (doc == NULL || function_node >= doc->parse.node_count ||
      doc->nodes[function_node].kind != W_SEED_CST_FUNCTION) {
    return false;
  }
  return function_prefix_has_keyword(doc, doc->nodes[function_node].raw_span,
                                     "const");
}

static w_seed_frontend_text import_item_local_name(
    const w_seed_frontend_document *doc, w_seed_span span,
    w_seed_frontend_text *imported_name) {
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  w_seed_frontend_text first = {NULL, 0};
  bool after_as = false;
  while (cursor_take(&cursor, &token)) {
    if (token.kind != W_SEED_CST_WORD) continue;
    const w_seed_frontend_text current = text_from_span(doc, token.span);
    if (first.length == 0) {
      first = current;
      continue;
    }
    if (after_as) {
      if (imported_name != NULL) *imported_name = first;
      return current;
    }
    after_as = text_equal(current, "as");
  }
  if (imported_name != NULL) *imported_name = first;
  return first;
}

static w_seed_frontend_label_kind parameter_label_kind(
    const w_seed_frontend_document *doc, w_seed_span span) {
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  w_seed_frontend_text first = {NULL, 0};
  w_seed_frontend_text second = {NULL, 0};
  while (cursor_take(&cursor, &token)) {
    if (token_text(doc, &token, ":")) {
      break;
    }
    if (token.kind != W_SEED_CST_WORD) continue;
    if (first.length == 0) {
      first = text_from_span(doc, token.span);
    } else if (second.length == 0) {
      second = text_from_span(doc, token.span);
    }
  }
  if (second.length != 0 && text_equal(first, "named")) {
    return W_SEED_FRONTEND_LABEL_NAMED_REQUIRED;
  }
  if (second.length != 0 && text_equal(first, "_")) {
    return W_SEED_FRONTEND_LABEL_OPTIONAL;
  }
  /* Any two-name form is external/internal.  `from` and `to` are common
   * spellings, but they are not keywords. */
  if (second.length != 0) return W_SEED_FRONTEND_LABEL_EXTERNAL_REQUIRED;
  return W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY;
}

static w_seed_frontend_text parameter_name_from_span(
    const w_seed_frontend_document *doc, w_seed_span span) {
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  w_seed_frontend_text first = {NULL, 0};
  w_seed_frontend_text second = {NULL, 0};
  while (cursor_take(&cursor, &token)) {
    if (token_text(doc, &token, ":")) break;
    if (token.kind != W_SEED_CST_WORD) continue;
    const w_seed_frontend_text value = text_from_span(doc, token.span);
    if (first.length == 0) {
      first = value;
    } else if (second.length == 0) {
      second = value;
    }
  }
  if (second.length != 0) return second;
  return first;
}

static w_seed_frontend_text parameter_external_label_from_span(
    const w_seed_frontend_document *doc, w_seed_span span) {
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  w_seed_frontend_text first = {NULL, 0};
  w_seed_frontend_text second = {NULL, 0};
  while (cursor_take(&cursor, &token)) {
    if (token_text(doc, &token, ":")) break;
    if (token.kind != W_SEED_CST_WORD) continue;
    const w_seed_frontend_text value = text_from_span(doc, token.span);
    if (first.length == 0) {
      first = value;
    } else if (second.length == 0) {
      second = value;
    }
  }
  if (second.length == 0) return (w_seed_frontend_text){NULL, 0};
  if (text_equal(first, "named") || text_equal(first, "_")) return second;
  return first;
}

static w_seed_frontend_text enum_case_parameter_label(
    const w_seed_frontend_document *doc, uint32_t parameter_node) {
  if (doc == NULL || parameter_node >= doc->parse.node_count) {
    return (w_seed_frontend_text){NULL, 0};
  }
  const w_seed_span span = doc->nodes[parameter_node].raw_span;
  const uint32_t type_node = direct_type_index(doc, parameter_node);
  if (type_node == W_SEED_CST_NONE) return (w_seed_frontend_text){NULL, 0};
  const w_seed_span type_span = doc->nodes[type_node].raw_span;
  /* A positional payload owns a TYPE at the parameter start.  Only inspect
   * the prefix before that TYPE for a label; colons inside function types or
   * nested contracts are part of the type surface. */
  if (type_span.start_byte <= span.start_byte) {
    return (w_seed_frontend_text){NULL, 0};
  }
  const w_seed_span prefix = {span.start_byte, type_span.start_byte};
  frontend_token_cursor cursor = token_cursor_for(doc, prefix);
  frontend_token token;
  w_seed_frontend_text first = {NULL, 0};
  while (cursor_take(&cursor, &token)) {
    if (token_text(doc, &token, ":")) return first;
    if (token.kind == W_SEED_CST_WORD && first.length == 0)
      first = text_from_span(doc, token.span);
  }
  return (w_seed_frontend_text){NULL, 0};
}

static bool resolved_import_index_for(const frontend_context *context,
                                      size_t document_index,
                                      uint32_t direct_import_ordinal,
                                      size_t *import_index) {
  if (import_index != NULL) *import_index = SIZE_MAX;
  if (context == NULL || import_index == NULL ||
      document_index >= context->input.document_count ||
      !context->input.import_resolution_complete) {
    return false;
  }
  size_t base = 0u;
  for (size_t index = 0u; index < document_index; index += 1u) {
    const size_t imports = count_root_children(
        &context->input.documents[index], W_SEED_CST_IMPORT);
    if (!add_size(base, imports, &base)) return false;
  }
  const size_t current = count_root_children(
      &context->input.documents[document_index], W_SEED_CST_IMPORT);
  if ((size_t)direct_import_ordinal >= current ||
      !add_size(base, (size_t)direct_import_ordinal, import_index)) {
    return false;
  }
  return *import_index < context->input.resolved_import_count;
}

static const w_seed_frontend_resolved_import *resolved_import_at(
    const frontend_context *context, size_t import_index) {
  if (context == NULL || !context->input.import_resolution_complete ||
      context->input.resolved_imports == NULL ||
      import_index >= context->input.resolved_import_count) {
    return NULL;
  }
  return &context->input.resolved_imports[import_index];
}

static bool import_has_from(const w_seed_frontend_document *doc,
                            w_seed_span span) {
  if (doc == NULL) return false;
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  bool saw_import = false;
  while (cursor_take(&cursor, &token)) {
    if (!saw_import) {
      if (token_text(doc, &token, "import")) saw_import = true;
      continue;
    }
    if (token_text(doc, &token, "from")) return true;
  }
  return false;
}

static bool imported_target_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    w_seed_frontend_import_target_kind *target_kind, uint32_t *target_index,
    w_seed_frontend_text *target_name) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || target_kind == NULL || target_index == NULL ||
      target_name == NULL || !context->input.import_resolution_complete) {
    return false;
  }
  *target_kind = W_SEED_FRONTEND_IMPORT_UNRESOLVED;
  *target_index = W_SEED_FRONTEND_NONE;
  *target_name = (w_seed_frontend_text){NULL, 0};
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0u;
  uint32_t direct_ordinal = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind != W_SEED_CST_IMPORT) {
      guard += 1u;
      continue;
    }
    size_t edge_index = SIZE_MAX;
    if (!import_has_from(doc, doc->nodes[child].raw_span) ||
        !resolved_import_index_for(context, context->module_index,
                                   direct_ordinal, &edge_index)) {
      direct_ordinal += 1u;
      guard += 1u;
      continue;
    }
    const w_seed_frontend_resolved_import *edge =
        resolved_import_at(context, edge_index);
    if (edge == NULL) return false;
    uint32_t item_cursor = doc->nodes[child].first_child;
    uint32_t item = W_SEED_CST_NONE;
    size_t item_guard = 0u;
    while (next_child(doc, &item_cursor, &item) &&
           item_guard < doc->parse.node_count) {
      if (doc->nodes[item].kind == W_SEED_CST_IMPORT_ITEM) {
        w_seed_frontend_text imported = {NULL, 0};
        const w_seed_frontend_text local = import_item_local_name(
            doc, doc->nodes[item].raw_span, &imported);
        if (imported.length != 0u && text_equal_text(local, name)) {
          *target_kind = edge->target_kind ==
                                 W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT
                             ? W_SEED_FRONTEND_IMPORT_LOCAL_DOCUMENT
                             : W_SEED_FRONTEND_IMPORT_EXTERNAL_MODULE;
          *target_index = edge->target_index;
          *target_name = imported;
          return true;
        }
      }
      item_guard += 1u;
    }
    direct_ordinal += 1u;
    guard += 1u;
  }
  return false;
}

static bool external_symbol_for_name(const frontend_context *context,
                                     w_seed_frontend_text name,
                                     const w_seed_frontend_external_symbol **symbol) {
  if (context == NULL || symbol == NULL) return false;
  w_seed_frontend_import_target_kind target_kind =
      W_SEED_FRONTEND_IMPORT_UNRESOLVED;
  uint32_t target_index = W_SEED_FRONTEND_NONE;
  w_seed_frontend_text target_name = {NULL, 0};
  if (!imported_target_for_name(context, name, &target_kind, &target_index,
                                &target_name) ||
      target_kind != W_SEED_FRONTEND_IMPORT_EXTERNAL_MODULE ||
      (size_t)target_index >= context->input.external_module_count) {
    return false;
  }
  const w_seed_frontend_external_module *module =
      &context->input.external_modules[target_index];
  for (size_t index = 0u; index < module->symbol_count; index += 1u) {
    const w_seed_frontend_external_symbol *candidate = &module->symbols[index];
    if (candidate->exported && text_equal_text(candidate->name, target_name)) {
      *symbol = candidate;
      return true;
    }
  }
  return false;
}

static bool external_symbol_identity_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    uint32_t *module_index, uint32_t *symbol_index) {
  if (module_index != NULL) *module_index = W_SEED_FRONTEND_NONE;
  if (symbol_index != NULL) *symbol_index = W_SEED_FRONTEND_NONE;
  if (context == NULL) return false;
  w_seed_frontend_import_target_kind target_kind =
      W_SEED_FRONTEND_IMPORT_UNRESOLVED;
  uint32_t target_index = W_SEED_FRONTEND_NONE;
  w_seed_frontend_text target_name = {NULL, 0u};
  if (!imported_target_for_name(context, name, &target_kind, &target_index,
                                &target_name) ||
      target_kind != W_SEED_FRONTEND_IMPORT_EXTERNAL_MODULE ||
      (size_t)target_index >= context->input.external_module_count) {
    return false;
  }
  const w_seed_frontend_external_module *module =
      &context->input.external_modules[target_index];
  for (size_t index = 0u; index < module->symbol_count; index += 1u) {
    const w_seed_frontend_external_symbol *candidate = &module->symbols[index];
    if (candidate->exported && text_equal_text(candidate->name, target_name)) {
      if (module_index != NULL) *module_index = target_index;
      if (symbol_index != NULL) {
        if (index >= (size_t)UINT32_MAX) return false;
        *symbol_index = (uint32_t)index;
      }
      return true;
    }
  }
  return false;
}

static bool host_symbol_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    const w_seed_frontend_host_prelude_symbol **symbol,
    uint32_t *symbol_index) {
  if (symbol != NULL) *symbol = NULL;
  if (symbol_index != NULL) *symbol_index = W_SEED_FRONTEND_NONE;
  if (context == NULL || context->input.host_scope == NULL ||
      name.length == 0u) {
    return false;
  }
  const w_seed_frontend_host_prelude *prelude = context->input.host_scope;
  for (size_t index = 0u; index < prelude->symbol_count; index += 1u) {
    const w_seed_frontend_host_prelude_symbol *candidate =
        &prelude->symbols[index];
    if (text_equal_text(candidate->name, name)) {
      if (symbol != NULL) *symbol = candidate;
      if (symbol_index != NULL) {
        if (index >= (size_t)UINT32_MAX) return false;
        *symbol_index = (uint32_t)index;
      }
      return true;
    }
  }
  return false;
}

static bool external_label_known(const frontend_context *context,
                                 w_seed_frontend_text callee,
                                 w_seed_frontend_text label, bool *resolved) {
  const w_seed_frontend_external_symbol *symbol = NULL;
  if (resolved != NULL) *resolved = false;
  if (!external_symbol_for_name(context, callee, &symbol)) return false;
  if (resolved != NULL) *resolved = true;
  if (label.length == 0) return true;
  for (size_t index = 0; index < symbol->parameter_count; index += 1) {
    const w_seed_frontend_external_parameter *parameter =
        &symbol->parameters[index];
    if (parameter->label_kind == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY) {
      continue;
    }
    if (text_equal_text(parameter->name, label)) return true;
  }
  return false;
}

static bool host_label_known(const frontend_context *context,
                             w_seed_frontend_text callee,
                             w_seed_frontend_text label, bool *resolved) {
  const w_seed_frontend_host_prelude_symbol *symbol = NULL;
  if (resolved != NULL) *resolved = false;
  if (!host_symbol_for_name(context, callee, &symbol, NULL)) return false;
  if (resolved != NULL) *resolved = true;
  if (label.length == 0u) return true;
  for (size_t index = 0u; index < symbol->parameter_count; index += 1u) {
    const w_seed_frontend_external_parameter *parameter =
        &symbol->parameters[index];
    if (parameter->label_kind == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY)
      continue;
    if (text_equal_text(parameter->name, label)) return true;
  }
  return false;
}

static bool local_argument_expected(
    const frontend_context *context, const w_seed_frontend_document *doc,
    uint32_t function_node,
    size_t ordinal, w_seed_frontend_text label,
    frontend_simple_type *expected) {
  const uint32_t parameters =
      first_direct_kind(doc, function_node, W_SEED_CST_PARAMETER_LIST);
  if (parameters == W_SEED_CST_NONE || expected == NULL) return false;
  uint32_t cursor = doc->nodes[parameters].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t index = 0;
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_PARAMETER) {
      const w_seed_frontend_label_kind policy =
          parameter_label_kind(doc, doc->nodes[child].raw_span);
      const w_seed_frontend_text label_from_span =
          parameter_external_label_from_span(doc, doc->nodes[child].raw_span);
      bool selected = false;
      if (label.length != 0) {
        selected = index == ordinal &&
                   policy != W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
                   text_equal_text(label_from_span, label);
      } else {
        selected = index == ordinal &&
                   policy != W_SEED_FRONTEND_LABEL_NAMED_REQUIRED &&
                   policy != W_SEED_FRONTEND_LABEL_EXTERNAL_REQUIRED;
      }
      if (selected) {
        const uint32_t type_node = direct_type_index(doc, child);
        if (type_node == W_SEED_CST_NONE) return false;
        *expected = contextual_type_from_span(
            context, doc, doc->nodes[type_node].raw_span);
        return true;
      }
      index += 1;
    }
    guard += 1;
  }
  return false;
}

static frontend_simple_type external_contextual_type(
    const frontend_context *context, w_seed_frontend_text spelling) {
  frontend_simple_type type = simple_type_from_view(spelling);
  if (type.kind == W_SEED_FRONTEND_TYPE_NOMINAL && context != NULL) {
    frontend_enum_subset_shape shape;
    if (enum_subset_shape_for_alias(context, spelling, &shape) &&
        shape.has_contract && shape.valid) {
      type.kind = shape.full ? W_SEED_FRONTEND_TYPE_ENUM
                             : W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
      type.enum_index = shape.enum_index;
      type.enum_name = shape.enum_name;
      type.enum_alias_name = type.spelling;
      type.subset_span = shape.list_span;
      return type;
    }
    uint32_t enum_index = W_SEED_FRONTEND_NONE;
    if (enum_declaration_name_count(context, type.spelling) == 1u &&
        enum_declaration_for_name(context, type.spelling, &enum_index, NULL,
                                  NULL, NULL)) {
      type.kind = W_SEED_FRONTEND_TYPE_ENUM;
      type.enum_index = enum_index;
      type.enum_name = type.spelling;
    }
  }
  return type;
}

static bool external_argument_expected(
    const frontend_context *context,
    const w_seed_frontend_external_symbol *symbol, size_t ordinal,
    w_seed_frontend_text label, frontend_simple_type *expected) {
  if (symbol == NULL || expected == NULL) return false;
  if (label.length != 0) {
    if (ordinal >= symbol->parameter_count) return false;
    const w_seed_frontend_external_parameter *parameter =
        &symbol->parameters[ordinal];
    if (parameter->label_kind != W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
        text_equal_text(parameter->name, label)) {
      *expected = external_contextual_type(context, parameter->type);
      return true;
    }
    return false;
  }
  if (ordinal >= symbol->parameter_count) return false;
  const w_seed_frontend_external_parameter *parameter =
      &symbol->parameters[ordinal];
  if (parameter->label_kind == W_SEED_FRONTEND_LABEL_NAMED_REQUIRED ||
      parameter->label_kind == W_SEED_FRONTEND_LABEL_EXTERNAL_REQUIRED) {
    return false;
  }
  *expected = external_contextual_type(context, parameter->type);
  return true;
}

static bool host_argument_expected(
    const frontend_context *context,
    const w_seed_frontend_host_prelude_symbol *symbol, size_t ordinal,
    w_seed_frontend_text label, frontend_simple_type *expected) {
  if (symbol == NULL || expected == NULL) return false;
  if (label.length != 0u) {
    if (ordinal >= symbol->parameter_count) return false;
    const w_seed_frontend_external_parameter *parameter =
        &symbol->parameters[ordinal];
    if (parameter->label_kind != W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
        text_equal_text(parameter->name, label)) {
      *expected = external_contextual_type(context, parameter->type);
      return true;
    }
    return false;
  }
  if (ordinal >= symbol->parameter_count) return false;
  const w_seed_frontend_external_parameter *parameter =
      &symbol->parameters[ordinal];
  if (parameter->label_kind == W_SEED_FRONTEND_LABEL_NAMED_REQUIRED ||
      parameter->label_kind == W_SEED_FRONTEND_LABEL_EXTERNAL_REQUIRED) {
    return false;
  }
  *expected = external_contextual_type(context, parameter->type);
  return true;
}

static uint32_t external_argument_ordinal(
    const w_seed_frontend_external_parameter *parameters,
    size_t parameter_count, size_t offset, w_seed_frontend_text label) {
  if (parameters == NULL && parameter_count != 0u)
    return W_SEED_FRONTEND_NONE;
  if (label.length == 0u) {
    if (offset >= parameter_count) return W_SEED_FRONTEND_NONE;
    const w_seed_frontend_external_parameter *parameter =
        &parameters[offset];
    return parameter->label_kind == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY ||
                   parameter->label_kind == W_SEED_FRONTEND_LABEL_OPTIONAL
               ? (offset >= (size_t)UINT32_MAX ? W_SEED_FRONTEND_NONE
                                               : (uint32_t)offset)
               : W_SEED_FRONTEND_NONE;
  }
  uint32_t selected = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u; index < parameter_count; index += 1u) {
    const w_seed_frontend_external_parameter *parameter = &parameters[index];
    if (parameter->label_kind != W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
        text_equal_text(parameter->name, label)) {
      if (selected != W_SEED_FRONTEND_NONE) return W_SEED_FRONTEND_NONE;
      if (index >= (size_t)UINT32_MAX) return W_SEED_FRONTEND_NONE;
      selected = (uint32_t)index;
    }
  }
  return selected;
}

static bool function_signature_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    const w_seed_frontend_document **owner_doc, uint32_t *function_node) {
  if (context == NULL || owner_doc == NULL || function_node == NULL) return false;
  const w_seed_frontend_document *local = context_document(context);
  if (function_in_document(local, name, false, function_node)) {
    *owner_doc = local;
    return true;
  }
  w_seed_frontend_import_target_kind target_kind =
      W_SEED_FRONTEND_IMPORT_UNRESOLVED;
  uint32_t target_index = W_SEED_FRONTEND_NONE;
  w_seed_frontend_text target_name = {NULL, 0};
  if (!imported_target_for_name(context, name, &target_kind, &target_index,
                                &target_name) ||
      target_kind != W_SEED_FRONTEND_IMPORT_LOCAL_DOCUMENT ||
      (size_t)target_index >= context->input.document_count) {
    return false;
  }
  const w_seed_frontend_document *doc =
      &context->input.documents[target_index];
  if (function_in_document(doc, target_name, true, function_node)) {
    *owner_doc = doc;
    return true;
  }
  return false;
}

static uint32_t innermost_block_for_span(
    const w_seed_frontend_document *doc, uint32_t function_node,
    w_seed_span span) {
  if (doc == NULL || function_node >= doc->parse.node_count) {
    return W_SEED_CST_NONE;
  }
  uint32_t best = W_SEED_CST_NONE;
  size_t best_size = SIZE_MAX;
  const w_seed_span function_span = doc->nodes[function_node].raw_span;
  for (size_t index = 0; index < doc->parse.node_count; index += 1) {
    const w_seed_cst_node *candidate = &doc->nodes[index];
    if (candidate->kind != W_SEED_CST_BLOCK ||
        candidate->raw_span.start_byte < function_span.start_byte ||
        candidate->raw_span.end_byte > function_span.end_byte ||
        candidate->raw_span.start_byte > span.start_byte ||
        candidate->raw_span.end_byte < span.end_byte) {
      continue;
    }
    const size_t candidate_size = candidate->raw_span.end_byte -
                                  candidate->raw_span.start_byte;
    if (candidate_size < best_size) {
      best = (uint32_t)index;
      best_size = candidate_size;
    }
  }
  return best;
}

static bool block_scope_contains(const w_seed_frontend_document *doc,
                                 uint32_t outer, uint32_t inner) {
  if (doc == NULL || outer == W_SEED_CST_NONE ||
      inner == W_SEED_CST_NONE || outer >= doc->parse.node_count ||
      inner >= doc->parse.node_count) {
    return false;
  }
  return doc->nodes[outer].raw_span.start_byte <=
             doc->nodes[inner].raw_span.start_byte &&
         doc->nodes[outer].raw_span.end_byte >=
             doc->nodes[inner].raw_span.end_byte;
}

static frontend_simple_type binding_type_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    w_seed_span use_span) {
  if (context == NULL) return simple_type_unknown();
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || context->function_node == NULL) return simple_type_unknown();
  const uint32_t parameters = first_direct_kind(
      doc, (uint32_t)(context->function_node - doc->nodes),
      W_SEED_CST_PARAMETER_LIST);
  if (parameters != W_SEED_CST_NONE) {
    uint32_t cursor = doc->nodes[parameters].first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t guard = 0;
    while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
      if (doc->nodes[child].kind == W_SEED_CST_PARAMETER &&
          text_equal_text(parameter_name_from_span(doc,
                                                   doc->nodes[child].raw_span),
                          name)) {
        const uint32_t type_node = direct_type_index(doc, child);
        if (type_node != W_SEED_CST_NONE)
          return contextual_type_from_span(context, doc,
                                           doc->nodes[type_node].raw_span);
      }
      guard += 1;
    }
  }
  const w_seed_span function_span = context->function_node->raw_span;
  const uint32_t use_block = innermost_block_for_span(
      doc, (uint32_t)(context->function_node - doc->nodes), use_span);
  for (size_t index = 0; index < doc->parse.node_count; index += 1) {
    const w_seed_cst_node *candidate = &doc->nodes[index];
    if ((candidate->kind == W_SEED_CST_LET_STATEMENT ||
         candidate->kind == W_SEED_CST_VAR_STATEMENT) &&
        candidate->raw_span.start_byte >= function_span.start_byte &&
        candidate->raw_span.end_byte <= function_span.end_byte &&
        candidate->raw_span.end_byte <= use_span.start_byte &&
        text_equal_text(binding_name_after_keyword(
                            doc, candidate->raw_span,
                            candidate->kind == W_SEED_CST_LET_STATEMENT ? "let"
                                                                         : "var"),
                        name) &&
        block_scope_contains(
            doc,
            innermost_block_for_span(
                doc, (uint32_t)(context->function_node - doc->nodes),
                candidate->raw_span),
            use_block)) {
      const uint32_t type_node = direct_type_index(doc, (uint32_t)index);
      if (type_node != W_SEED_CST_NONE) {
        return contextual_type_from_span(context, doc,
                                         doc->nodes[type_node].raw_span);
      }
    }
  }
  /* A range loop binder is a lexical usize local. The CST owner remains the
   * authority for scope because the frontend does not expose source text to
   * downstream const lowering. */
  for (size_t index = 0; index < doc->parse.node_count; index += 1u) {
    const w_seed_cst_node *candidate = &doc->nodes[index];
    if (candidate->kind != W_SEED_CST_FOR_STATEMENT ||
        candidate->raw_span.start_byte < function_span.start_byte ||
        candidate->raw_span.end_byte > function_span.end_byte ||
        use_span.start_byte < candidate->raw_span.start_byte ||
        use_span.end_byte > candidate->raw_span.end_byte) {
      continue;
    }
    const w_seed_frontend_text binder =
        binding_name_after_keyword(doc, candidate->raw_span, "for");
    if (text_equal_text(binder, name))
      return simple_type_from_view((w_seed_frontend_text){"usize", 5});
  }
  return simple_type_unknown();
}

static frontend_simple_type function_return_type(
    const frontend_context *context, const w_seed_frontend_document *doc,
    uint32_t function_node) {
  const uint32_t return_node =
      first_direct_kind(doc, function_node, W_SEED_CST_RETURN_TYPE);
  if (return_node == W_SEED_CST_NONE) {
    return simple_type_from_view((w_seed_frontend_text){"()", 2});
  }
  const uint32_t type_node = direct_type_index(doc, return_node);
  return type_node == W_SEED_CST_NONE
             ? simple_type_unknown()
             : contextual_type_from_span(context, doc,
                                         doc->nodes[type_node].raw_span);
}

static bool output_subset_members_match_simple(
    const frontend_context *context, uint32_t candidate_index,
    frontend_simple_type expected) {
  if (context == NULL || context->output == NULL ||
      candidate_index >= context->count.types ||
      expected.kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET ||
      expected.enum_index == W_SEED_FRONTEND_NONE) {
    return false;
  }
  if (context->output->types == NULL) return false;
  const w_seed_frontend_type *candidate =
      &context->output->types[candidate_index];
  if (candidate->kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET ||
      candidate->enum_base_index != expected.enum_index ||
      candidate->first_subset_member == W_SEED_FRONTEND_NONE ||
      (size_t)candidate->first_subset_member + candidate->subset_member_count >
          context->count.enum_subset_members ||
      context->output->enum_subset_members == NULL) {
    return false;
  }
  const w_seed_frontend_document *enum_doc = NULL;
  uint32_t enum_node = W_SEED_CST_NONE;
  if (!enum_declaration_for_name(context, expected.enum_name, NULL, NULL,
                                 &enum_doc, &enum_node)) {
    return false;
  }
  uint32_t case_cursor = enum_doc->nodes[enum_node].first_child;
  uint32_t case_node = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(enum_doc, &case_cursor, &case_node) &&
         guard < enum_doc->parse.node_count) {
    if (enum_doc->nodes[case_node].kind == W_SEED_CST_ENUM_CASE) {
      const w_seed_frontend_text name =
          first_word_in_span(enum_doc, enum_doc->nodes[case_node].raw_span);
      uint32_t case_index = W_SEED_FRONTEND_NONE;
      if (!enum_case_for_name(context, expected.enum_index, name, &case_index,
                              NULL, NULL)) {
        return false;
      }
      bool candidate_has_case = false;
      for (uint32_t offset = 0; offset < candidate->subset_member_count;
           offset += 1) {
        const w_seed_frontend_enum_subset_member *member =
            &context->output->enum_subset_members[
                candidate->first_subset_member + offset];
        if (member->enum_case_index == case_index) {
          candidate_has_case = true;
          break;
        }
      }
      if (candidate_has_case !=
          enum_subset_contains_case(context, expected, case_index)) {
        return false;
      }
    }
    guard += 1;
  }
  return true;
}

static bool output_alias_type_index_for_simple(
    const frontend_context *context, frontend_simple_type type,
    uint32_t *index) {
  if (context == NULL || context->output == NULL || index == NULL ||
      type.kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET ||
      type.enum_alias_name.length == 0 ||
      alias_declaration_name_count(context, type.enum_alias_name) != 1u ||
      context->output->aliases == NULL) {
    return false;
  }
  uint32_t alias_type_index = W_SEED_FRONTEND_NONE;
  size_t matches = 0;
  for (size_t alias_index = 0; alias_index < context->count.aliases;
       alias_index += 1) {
    const w_seed_frontend_alias *alias =
        &context->output->aliases[alias_index];
    if (!text_equal_text(alias->name, type.enum_alias_name)) continue;
    alias_type_index = alias->type_index;
    matches += 1;
  }
  if (matches != 1u ||
      !output_subset_members_match_simple(context, alias_type_index, type)) {
    return false;
  }
  *index = alias_type_index;
  return true;
}

static bool output_type_index_for_simple(frontend_context *context,
                                         frontend_simple_type type,
                                         uint32_t *index) {
  if (index == NULL) return false;
  *index = W_SEED_FRONTEND_NONE;
  if (context == NULL) return true;
  if (type.kind == W_SEED_FRONTEND_TYPE_INTEGER && !type.is_signed &&
      type.bit_width == (uint16_t)W_SEED_FRONTEND_TARGET_USIZE_BITS &&
      text_equal(type.spelling, "usize")) {
    if (context->builtin_usize_type_index == W_SEED_FRONTEND_NONE) {
      w_seed_frontend_type builtin;
      (void)memset(&builtin, 0, sizeof(builtin));
      builtin.kind = W_SEED_FRONTEND_TYPE_INTEGER;
      builtin.spelling = (w_seed_frontend_text){"usize", 5};
      builtin.nominal_name = builtin.spelling;
      builtin.span = empty_span(0);
      builtin.is_signed = false;
      builtin.bit_width = (uint16_t)W_SEED_FRONTEND_TARGET_USIZE_BITS;
      builtin.element_type = W_SEED_FRONTEND_NONE;
      builtin.return_type = W_SEED_FRONTEND_NONE;
      builtin.first_parameter = W_SEED_FRONTEND_NONE;
      builtin.enum_base_index = W_SEED_FRONTEND_NONE;
      builtin.first_subset_member = W_SEED_FRONTEND_NONE;
      builtin.subset_member_count = 0u;
      builtin.generic_application_index = W_SEED_FRONTEND_NONE;
      uint32_t builtin_index = W_SEED_FRONTEND_NONE;
      if (!context_append_type(context, builtin, &builtin_index)) return false;
      context->builtin_usize_type_index = builtin_index;
    }
    *index = context->builtin_usize_type_index;
    return true;
  }
  if (!context->emit || context->output == NULL) return true;
  if (type.kind == W_SEED_FRONTEND_TYPE_ENUM &&
      type.enum_index != W_SEED_FRONTEND_NONE &&
      context->output->enums != NULL &&
      type.enum_index < context->count.enums) {
    const uint32_t canonical =
        context->output->enums[type.enum_index].type_index;
    if (canonical < context->count.types &&
        context->output->types[canonical].kind ==
            W_SEED_FRONTEND_TYPE_ENUM &&
        context->output->types[canonical].enum_base_index == type.enum_index) {
      *index = canonical;
      return true;
    }
  }
  if (output_alias_type_index_for_simple(context, type, index)) return true;
  for (size_t item = 0; item < context->count.types; item += 1) {
    const w_seed_frontend_type *candidate = &context->output->types[item];
    if (candidate->kind == type.kind &&
        candidate->bit_width == type.bit_width &&
        candidate->is_signed == type.is_signed &&
        text_equal_text(candidate->spelling, type.spelling) &&
        candidate->enum_base_index == type.enum_index) {
      if (item >= (size_t)UINT32_MAX) return false;
      *index = (uint32_t)item;
      return true;
    }
  }
  /* A typed integer literal keeps its source spelling (for example
   * ``1_u8``) in the expression record.  Its resolved type spelling is the
   * suffix (``u8``), so exact spelling cannot identify the canonical type.
   * The intrinsic kind/width/signedness tuple is the existing frontend type
   * identity for integers; use its first deterministic candidate. */
  if (type.kind == W_SEED_FRONTEND_TYPE_INTEGER && type.bit_width != 0u) {
    for (size_t item = 0; item < context->count.types; item += 1u) {
      const w_seed_frontend_type *candidate = &context->output->types[item];
      if (candidate->kind == type.kind &&
          candidate->bit_width == type.bit_width &&
          candidate->is_signed == type.is_signed &&
          candidate->enum_base_index == W_SEED_FRONTEND_NONE) {
        if (item >= (size_t)UINT32_MAX) return false;
        *index = (uint32_t)item;
        return true;
      }
    }
  }
  /* A simple String literal carries literal spelling in its expression
   * record, while the canonical frontend type is the declaration spelling
   * ``String``.  Resolve by the normalized kind/metadata tuple; downstream
   * passes receive the arena slice and never reparse this spelling. */
  if (type.kind == W_SEED_FRONTEND_TYPE_STRING) {
    for (size_t item = 0; item < context->count.types; item += 1u) {
      const w_seed_frontend_type *candidate = &context->output->types[item];
      if (candidate->kind == W_SEED_FRONTEND_TYPE_STRING &&
          !candidate->is_signed && candidate->bit_width == 0u) {
        if (item >= (size_t)UINT32_MAX) return false;
        *index = (uint32_t)item;
        return true;
      }
    }
  }
  return true;
}

static uint32_t loop_ordinal_for_cst_node(const frontend_context *context,
                                          uint32_t node_index) {
  if (context == NULL || context->function_node == NULL) {
    return W_SEED_FRONTEND_NONE;
  }
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || node_index >= doc->parse.node_count ||
      doc->nodes[node_index].kind != W_SEED_CST_FOR_STATEMENT) {
    return W_SEED_FRONTEND_NONE;
  }
  const w_seed_span function_span = context->function_node->raw_span;
  const w_seed_span target_span = doc->nodes[node_index].raw_span;
  uint32_t ordinal = 0u;
  for (size_t index = 0u; index < doc->parse.node_count; index += 1u) {
    const w_seed_cst_node *candidate = &doc->nodes[index];
    if (candidate->kind != W_SEED_CST_FOR_STATEMENT ||
        candidate->raw_span.start_byte < function_span.start_byte ||
        candidate->raw_span.end_byte > function_span.end_byte) {
      continue;
    }
    if (candidate->raw_span.start_byte < target_span.start_byte ||
        (candidate->raw_span.start_byte == target_span.start_byte &&
         index < (size_t)node_index)) {
      if (ordinal == UINT32_MAX - 1u) return W_SEED_FRONTEND_NONE;
      ordinal += 1u;
      continue;
    }
    if (index == (size_t)node_index) return ordinal;
  }
  return W_SEED_FRONTEND_NONE;
}

static uint32_t loop_local_ordinal_for_span(
    const frontend_context *context, w_seed_frontend_text name,
    w_seed_span span) {
  if (context == NULL || context->function_node == NULL || name.length == 0u)
    return W_SEED_FRONTEND_NONE;
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return W_SEED_FRONTEND_NONE;
  const w_seed_span function_span = context->function_node->raw_span;
  uint32_t best_node = W_SEED_FRONTEND_NONE;
  size_t best_width = SIZE_MAX;
  for (size_t index = 0; index < doc->parse.node_count; index += 1u) {
    const w_seed_cst_node *candidate = &doc->nodes[index];
    if (candidate->kind != W_SEED_CST_FOR_STATEMENT ||
        candidate->raw_span.start_byte < function_span.start_byte ||
        candidate->raw_span.end_byte > function_span.end_byte ||
        span.start_byte < candidate->raw_span.start_byte ||
        span.end_byte > candidate->raw_span.end_byte) {
      continue;
    }
    const w_seed_frontend_text binder =
        binding_name_after_keyword(doc, candidate->raw_span, "for");
    if (text_equal_text(binder, name)) {
      const size_t width = candidate->raw_span.end_byte -
                           candidate->raw_span.start_byte;
      if (best_node == W_SEED_FRONTEND_NONE || width < best_width) {
        best_node = (uint32_t)index;
        best_width = width;
      }
    }
  }
  return best_node == W_SEED_FRONTEND_NONE
             ? W_SEED_FRONTEND_NONE
             : loop_ordinal_for_cst_node(context, best_node);
}

static bool expression_append(frontend_expression_parser *parser,
                              w_seed_frontend_expr_kind kind,
                              w_seed_span span, w_seed_frontend_text spelling,
                              w_seed_frontend_text operator_text,
                              frontend_simple_type type, bool supported,
                              size_t left, size_t right, uint32_t first_argument,
                              size_t argument_count,
                              frontend_expr_value *value) {
  if (parser == NULL || parser->context == NULL || value == NULL) return false;
  w_seed_frontend_expression record;
  (void)memset(&record, 0, sizeof(record));
  record.kind = kind;
  record.module_index = (uint32_t)parser->context->module_index;
  record.owner_function = parser->context->function_index;
  record.spelling = spelling;
  record.operator_text = operator_text;
  record.span = span;
  record.left = left >= (size_t)UINT32_MAX ? W_SEED_FRONTEND_NONE
                                           : (uint32_t)left;
  record.right = right >= (size_t)UINT32_MAX ? W_SEED_FRONTEND_NONE
                                            : (uint32_t)right;
  record.first_argument = first_argument;
  record.argument_count = (uint32_t)argument_count;
  record.inferred_type = W_SEED_FRONTEND_NONE;
  record.enum_index = value->is_enum_case
                          ? value->enum_index
                          : (frontend_type_is_enum(type) ? type.enum_index
                                                         : W_SEED_FRONTEND_NONE);
  record.enum_case_index = value->is_enum_case
                               ? value->enum_case_index
                               : W_SEED_FRONTEND_NONE;
  record.first_switch_arm = W_SEED_FRONTEND_NONE;
  record.switch_arm_count = 0;
  record.first_membership_case = W_SEED_FRONTEND_NONE;
  record.membership_case_count = 0;
  record.has_bool_value = false;
  record.bool_value = false;
  record.has_integer_value = false;
  (void)memset(record.integer_value, 0, sizeof(record.integer_value));
  record.const_byte_offset = W_SEED_FRONTEND_NONE;
  record.const_byte_count = 0u;
  record.resolved_parameter_ordinal = W_SEED_FRONTEND_NONE;
  record.resolved_function_index = W_SEED_FRONTEND_NONE;
  record.resolved_callee_kind = W_SEED_FRONTEND_CALLEE_NONE;
  record.resolved_host_symbol_index = W_SEED_FRONTEND_NONE;
  record.resolved_external_module_index = W_SEED_FRONTEND_NONE;
  record.resolved_external_symbol_index = W_SEED_FRONTEND_NONE;
  record.resolved_local_ordinal = W_SEED_FRONTEND_NONE;
  record.member_name = (w_seed_frontend_text){NULL, 0};
  record.resolved_const_declaration = W_SEED_FRONTEND_NONE;
  if (kind == W_SEED_FRONTEND_EXPR_IDENTIFIER) {
    record.resolved_local_ordinal = loop_local_ordinal_for_span(
        parser->context, spelling, span);
  }
  if (kind == W_SEED_FRONTEND_EXPR_BOOL &&
      (text_equal(spelling, "true") || text_equal(spelling, "false"))) {
    record.has_bool_value = true;
    record.bool_value = text_equal(spelling, "true");
  } else if (kind == W_SEED_FRONTEND_EXPR_INTEGER &&
             type.kind == W_SEED_FRONTEND_TYPE_INTEGER) {
    size_t body_end = spelling.length;
    bool has_suffix = false;
    bool is_signed = true;
    uint16_t width = 0;
    uint64_t integer_value = 0;
    if (integer_literal_parts(spelling, &body_end, &has_suffix, &is_signed,
                              &width) &&
        integer_literal_value(spelling, body_end, &integer_value)) {
      record.has_integer_value = true;
      for (size_t byte = 0; byte < sizeof(integer_value); byte += 1u) {
        record.integer_value[byte] =
            (uint8_t)(integer_value >> (byte * 8u));
      }
    }
    (void)has_suffix;
    (void)is_signed;
    (void)width;
  }
  if (kind == W_SEED_FRONTEND_EXPR_STRING && supported) {
    /* Keep the same source-backed subset as ConstValue String.  The
     * downstream ConstIR pass receives bytes and a range, not literal
     * spelling. */
    frontend_token tokens[W_SEED_FRONTEND_MAX_NESTING * 2u];
    size_t token_count = 0u;
    bool simple = const_tokens_for_span(parser->document, span, tokens,
                                        sizeof(tokens) / sizeof(tokens[0]),
                                        &token_count);
    for (size_t token_index = 0u; simple && token_index < token_count;
         token_index += 1u) {
      if (tokens[token_index].kind != W_SEED_CST_LITERAL_EVENT) simple = false;
    }
    const w_seed_frontend_text text = text_from_span(parser->document, span);
    if (simple &&
        (text.length < 2u ||
         (text.data[0] != '"' && text.data[0] != '\'') ||
         text.data[text.length - 1u] != text.data[0])) {
      simple = false;
    }
    if (simple) {
      for (size_t byte = 1u; byte + 1u < text.length; byte += 1u) {
        if (text.data[byte] == '\\') {
          simple = false;
          break;
        }
      }
    }
    if (simple) {
      if (text.length - 2u > (size_t)UINT32_MAX) return false;
      uint32_t offset = W_SEED_FRONTEND_NONE;
      if (!context_append_const_bytes(
              parser->context, (const uint8_t *)text.data + 1u,
              text.length - 2u, &offset))
        return false;
      record.const_byte_offset = offset;
      record.const_byte_count = (uint32_t)(text.length - 2u);
    } else {
      supported = false;
      (void)context_append_fact(
          parser->context, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION, span,
          text_from_span(parser->document, span));
    }
  }
  if (kind == W_SEED_FRONTEND_EXPR_ENUM_MEMBERSHIP &&
      value->enum_index != W_SEED_FRONTEND_NONE) {
    record.enum_index = value->enum_index;
  }
  record.supported = supported;
  if (!output_type_index_for_simple(parser->context, type,
                                    &record.inferred_type)) {
    return false;
  }
  uint32_t index = W_SEED_FRONTEND_NONE;
  if (!context_append_expression(parser->context, record, &index)) return false;
  value->index = index;
  value->left = left;
  value->right = right;
  value->kind = kind;
  value->type = type;
  value->supported = supported;
  value->has_name = kind == W_SEED_FRONTEND_EXPR_IDENTIFIER;
  value->is_enum_case = kind == W_SEED_FRONTEND_EXPR_ENUM_CASE;
  if (value->is_enum_case) {
    value->enum_index = value->enum_index == W_SEED_FRONTEND_NONE
                            ? type.enum_index
                            : value->enum_index;
  } else {
    value->enum_index = frontend_type_is_enum(type)
                            ? type.enum_index
                            : W_SEED_FRONTEND_NONE;
    value->enum_case_index = W_SEED_FRONTEND_NONE;
  }
  value->name = value->has_name ? spelling : (w_seed_frontend_text){NULL, 0};
  value->operator_text = operator_text;
  value->span = span;
  if (kind == W_SEED_FRONTEND_EXPR_ENUM_MEMBERSHIP) {
    value->enum_index = record.enum_index;
    value->enum_case_index = W_SEED_FRONTEND_NONE;
  }
  return true;
}

/* Apply contextual integer typing without reparsing source.  Unsuffixed
 * integer literals remain signed/width-zero until an enclosing range supplies
 * the usize context.  The frontend record must receive the same canonical
 * type in emit mode so downstream const lowering sees the contextual type,
 * while dry mode only carries the value metadata. */
static bool expression_value_set_type(frontend_expression_parser *parser,
                                      frontend_expr_value *value,
                                      frontend_simple_type type) {
  if (parser == NULL || parser->context == NULL || value == NULL) return false;
  value->type = type;
  if (parser->context->emit && parser->context->output != NULL &&
      value->index != W_SEED_FRONTEND_NONE &&
      value->index < parser->context->output->expression_capacity) {
    uint32_t type_index = W_SEED_FRONTEND_NONE;
    if (!output_type_index_for_simple(parser->context, type, &type_index)) {
      return false;
    }
    parser->context->output->expressions[value->index].inferred_type =
        type_index;
  }
  return true;
}

static bool expression_value_is_unsuffixed_integer(
    const frontend_expr_value *value) {
  return value != NULL && value->is_integer_literal &&
         value->type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
         value->type.bit_width == 0u;
}

static bool expression_parse_bp(frontend_expression_parser *parser,
                                int minimum_precedence,
                                frontend_expr_value *value);

static bool expression_parse_prefix(frontend_expression_parser *parser,
                                     frontend_expr_value *value);

static bool expression_parse_primary(frontend_expression_parser *parser,
                                     frontend_expr_value *value) {
  if (parser == NULL || value == NULL) return false;
  (void)memset(value, 0, sizeof(*value));
  value->index = W_SEED_FRONTEND_NONE;
  value->enum_index = W_SEED_FRONTEND_NONE;
  value->enum_case_index = W_SEED_FRONTEND_NONE;
  frontend_token token;
  if (!cursor_take(&parser->cursor, &token)) return false;
  const w_seed_frontend_text spelling = text_from_span(parser->document,
                                                        token.span);
  if (token.kind == W_SEED_CST_WORD) {
    if (text_equal(spelling, "true") || text_equal(spelling, "false")) {
      return expression_append(parser, W_SEED_FRONTEND_EXPR_BOOL, token.span,
                               spelling, (w_seed_frontend_text){NULL, 0},
                               simple_type_from_view((w_seed_frontend_text){
                                   "Bool", 4}), true,
                               (size_t)W_SEED_FRONTEND_NONE,
                               (size_t)W_SEED_FRONTEND_NONE,
                               W_SEED_FRONTEND_NONE, 0, value);
    }
    if (text_equal(spelling, "try") || text_equal(spelling, "await") ||
        text_equal(spelling, "copy") || text_equal(spelling, "take") ||
        text_equal(spelling, "ref") || text_equal(spelling, "pin")) {
      frontend_expr_value nested;
      if (!expression_parse_primary(parser, &nested)) return false;
      const w_seed_span span = {token.span.start_byte, nested.span.end_byte};
      (void)context_append_fact(parser->context,
                                W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                span, text_from_span(parser->document, span));
      return expression_append(parser, W_SEED_FRONTEND_EXPR_UNSUPPORTED, span,
                               text_from_span(parser->document, span), spelling,
                               nested.type, false, nested.index,
                               (size_t)W_SEED_FRONTEND_NONE,
                             W_SEED_FRONTEND_NONE, 0, value);
    }
    /* A qualified enum value is nominally resolved before the generic postfix
     * path.  Unknown members remain unsupported facts through the existing
     * member-postfix barrier. */
    frontend_token dot;
    frontend_token member;
    frontend_token_cursor qualified = parser->cursor;
    if (cursor_peek(&qualified, &dot) && token_text(parser->document, &dot, ".")) {
      (void)cursor_take(&qualified, &dot);
      if (cursor_peek(&qualified, &member) && member.kind == W_SEED_CST_WORD) {
        const w_seed_frontend_text member_name =
            text_from_span(parser->document, member.span);
        uint32_t enum_index = W_SEED_FRONTEND_NONE;
        uint32_t type_index = W_SEED_FRONTEND_NONE;
        if (enum_declaration_name_count(parser->context, spelling) == 1u &&
            enum_declaration_for_name(parser->context, spelling, &enum_index,
                                      &type_index, NULL, NULL)) {
          uint32_t case_index = W_SEED_FRONTEND_NONE;
          if (enum_case_for_name(parser->context, enum_index, member_name,
                                 &case_index, NULL, NULL)) {
            (void)cursor_take(&parser->cursor, &dot);
            (void)cursor_take(&parser->cursor, &member);
            const w_seed_span span = {token.span.start_byte, member.span.end_byte};
            frontend_simple_type enum_type = simple_type_from_view(spelling);
            enum_type.kind = W_SEED_FRONTEND_TYPE_ENUM;
            enum_type.enum_index = enum_index;
            enum_type.enum_name = spelling;
            if (parser->has_expected_type &&
                parser->expected_type.kind ==
                    W_SEED_FRONTEND_TYPE_ENUM_SUBSET &&
                parser->expected_type.enum_index != enum_index) {
              (void)append_type0121_diagnostic(
                  parser->context, span, member_name, parser->expected_type);
              return expression_append(
                  parser, W_SEED_FRONTEND_EXPR_UNSUPPORTED, span,
                  text_from_span(parser->document, span),
                  (w_seed_frontend_text){NULL, 0}, simple_type_unknown(),
                  false, (size_t)W_SEED_FRONTEND_NONE,
                  (size_t)W_SEED_FRONTEND_NONE, W_SEED_FRONTEND_NONE, 0,
                  value);
            }
            if (parser->has_expected_type &&
                frontend_type_is_enum(parser->expected_type) &&
                parser->expected_type.enum_index == enum_index) {
              if (!enum_subset_contains_case(parser->context,
                                              parser->expected_type, case_index)) {
                (void)append_type0121_diagnostic(
                    parser->context, span, member_name,
                    parser->expected_type);
                return expression_append(
                    parser, W_SEED_FRONTEND_EXPR_UNSUPPORTED, span,
                    text_from_span(parser->document, span),
                    (w_seed_frontend_text){NULL, 0}, simple_type_unknown(),
                    false, (size_t)W_SEED_FRONTEND_NONE,
                    (size_t)W_SEED_FRONTEND_NONE, W_SEED_FRONTEND_NONE, 0,
                    value);
              }
              enum_type = parser->expected_type;
            }
            value->is_enum_case = true;
            value->enum_index = enum_index;
            value->enum_case_index = case_index;
            return expression_append(
                parser, W_SEED_FRONTEND_EXPR_ENUM_CASE, span,
                text_from_span(parser->document, span),
                (w_seed_frontend_text){NULL, 0}, enum_type, true,
                (size_t)W_SEED_FRONTEND_NONE,
                (size_t)W_SEED_FRONTEND_NONE, W_SEED_FRONTEND_NONE, 0, value);
          }
        }
      }
    }
    frontend_simple_type type = binding_type_for_name(
        parser->context, spelling, token.span);
    bool resolved = type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN;
    uint32_t resolved_module_const = W_SEED_FRONTEND_NONE;
    if (!resolved) {
      const w_seed_frontend_document *owner_doc = NULL;
      uint32_t function_node = W_SEED_CST_NONE;
      resolved = function_signature_for_name(parser->context, spelling,
                                              &owner_doc, &function_node);
      if (!resolved) {
        const w_seed_frontend_external_symbol *external = NULL;
        resolved = external_symbol_for_name(parser->context, spelling,
                                            &external);
      }
      if (!resolved) {
        const w_seed_frontend_host_prelude_symbol *host = NULL;
        resolved = host_symbol_for_name(parser->context, spelling, &host, NULL);
      }
      if (!resolved) {
        frontend_simple_type module_type = simple_type_unknown();
        if (module_const_for_name(parser->context, spelling,
                                  &resolved_module_const, &module_type, NULL,
                                  NULL)) {
          type = module_type;
          /* Module const references use the enclosing explicit type when the
           * source-backed target has the same scalar type.  The canonical
           * type index can differ because each declaration owns its type
           * record.  Keep a real mismatch visible to the lowerability check. */
          if (parser->has_expected_type &&
              ((type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                parser->expected_type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                type.is_signed == parser->expected_type.is_signed &&
                type.bit_width == parser->expected_type.bit_width) ||
               (type.kind == W_SEED_FRONTEND_TYPE_BOOL &&
                parser->expected_type.kind == W_SEED_FRONTEND_TYPE_BOOL)))
            type = parser->expected_type;
          resolved = type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN;
        }
      }
      if (!resolved) {
        (void)context_append_fact(
            parser->context, W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL,
            token.span, spelling);
      }
    }
    const bool identifier_supported = resolved;
    const bool appended = expression_append(
        parser, W_SEED_FRONTEND_EXPR_IDENTIFIER, token.span, spelling,
        (w_seed_frontend_text){NULL, 0}, type, identifier_supported,
        (size_t)W_SEED_FRONTEND_NONE, (size_t)W_SEED_FRONTEND_NONE,
        W_SEED_FRONTEND_NONE, 0, value);
    if (appended && resolved_module_const != W_SEED_FRONTEND_NONE &&
        parser->context->emit && parser->context->output != NULL &&
        value->index < parser->context->output->expression_capacity) {
      parser->context->output->expressions[value->index]
          .resolved_const_declaration = resolved_module_const;
    }
    return appended;
  }
  if (token.kind == W_SEED_CST_NUMBER ||
      token.kind == W_SEED_CST_LITERAL_EVENT) {
    frontend_simple_type type = literal_simple_type(
        parser->document, token.span, token.kind);
    /* A calculated generic scalar has an explicit domain.  Apply that
     * context to unsuffixed integer leaves before appending the record so a
     * closed `(6 * 7)` tree carries i64 (or the declared width) throughout. */
    if (parser->has_expected_type &&
        parser->expected_type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
        type.kind == W_SEED_FRONTEND_TYPE_INTEGER && type.bit_width == 0u &&
        unsuffixed_integer_fits(text_from_span(parser->document, token.span),
                                parser->expected_type)) {
      type = parser->expected_type;
    }
    const bool literal_supported = type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN;
    if (!literal_supported) {
      (void)context_append_fact(parser->context,
                                W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                token.span, spelling);
    }
    w_seed_span literal_span = token.span;
    if (token.kind == W_SEED_CST_LITERAL_EVENT) {
      frontend_token_cursor copy = parser->cursor;
      frontend_token next;
      while (cursor_peek(&copy, &next) &&
             next.kind == W_SEED_CST_LITERAL_EVENT) {
        (void)cursor_take(&copy, &next);
        literal_span.end_byte = next.span.end_byte;
        parser->cursor = copy;
      }
    }
    const w_seed_frontend_text text = text_from_span(parser->document,
                                                      literal_span);
    const bool appended = expression_append(
        parser, !literal_supported
                   ? W_SEED_FRONTEND_EXPR_UNSUPPORTED
                   : (type.kind == W_SEED_FRONTEND_TYPE_FLOAT
                          ? W_SEED_FRONTEND_EXPR_FLOAT
                          : (type.kind == W_SEED_FRONTEND_TYPE_STRING ||
                                     type.kind == W_SEED_FRONTEND_TYPE_BYTES
                                 ? (type.kind == W_SEED_FRONTEND_TYPE_BYTES
                                        ? W_SEED_FRONTEND_EXPR_BYTES
                                        : W_SEED_FRONTEND_EXPR_STRING)
                                 : W_SEED_FRONTEND_EXPR_INTEGER)),
        literal_span, text, (w_seed_frontend_text){NULL, 0}, type,
        literal_supported,
        (size_t)W_SEED_FRONTEND_NONE, (size_t)W_SEED_FRONTEND_NONE,
        W_SEED_FRONTEND_NONE, 0, value);
    if (appended) {
      value->is_integer_literal =
          literal_supported && type.kind == W_SEED_FRONTEND_TYPE_INTEGER;
    }
    return appended;
  }
  if (token_text(parser->document, &token, "(")) {
    frontend_expr_value nested;
    if (!cursor_peek_text(&parser->cursor, ")") &&
        !expression_parse_bp(parser, 0, &nested)) {
      return false;
    }
    frontend_token close;
    if (!cursor_take_text(&parser->cursor, ")", &close)) return false;
    const w_seed_span span = {token.span.start_byte, close.span.end_byte};
    const bool appended = expression_append(
        parser, W_SEED_FRONTEND_EXPR_PARENTHESIS, span,
        text_from_span(parser->document, span),
        (w_seed_frontend_text){NULL, 0}, nested.type, nested.supported,
        nested.index, (size_t)W_SEED_FRONTEND_NONE, W_SEED_FRONTEND_NONE, 0,
        value);
    if (appended) value->is_integer_literal = false;
    return appended;
  }
  if (token_text(parser->document, &token, "[")) {
    size_t depth = 1;
    frontend_token next;
    w_seed_span span = token.span;
    while (cursor_take(&parser->cursor, &next)) {
      span.end_byte = next.span.end_byte;
      if (token_text(parser->document, &next, "[")) depth += 1;
      if (token_text(parser->document, &next, "]")) {
        depth -= 1;
        if (depth == 0) break;
      }
    }
    (void)context_append_fact(parser->context,
                              W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                              span, text_from_span(parser->document, span));
    return expression_append(parser, W_SEED_FRONTEND_EXPR_UNSUPPORTED, span,
                             text_from_span(parser->document, span),
                             (w_seed_frontend_text){NULL, 0},
                             simple_type_unknown(), false,
                             (size_t)W_SEED_FRONTEND_NONE,
                             (size_t)W_SEED_FRONTEND_NONE,
                             W_SEED_FRONTEND_NONE, 0, value);
  }
  if (token_text(parser->document, &token, ".")) {
    frontend_token member;
    if (!cursor_take(&parser->cursor, &member)) return false;
    const w_seed_span span = {token.span.start_byte, member.span.end_byte};
    const w_seed_frontend_text case_name =
        text_from_span(parser->document, member.span);
    if (parser->has_expected_type &&
        frontend_type_is_enum(parser->expected_type)) {
      uint32_t enum_index = parser->expected_type.enum_index;
      if (enum_index == W_SEED_FRONTEND_NONE &&
          enum_declaration_name_count(parser->context,
                                      parser->expected_type.spelling) == 1u) {
        (void)enum_declaration_for_name(parser->context,
                                        parser->expected_type.spelling,
                                        &enum_index, NULL, NULL, NULL);
      }
      if (enum_index != W_SEED_FRONTEND_NONE) {
        uint32_t case_index = W_SEED_FRONTEND_NONE;
        if (enum_case_for_name(parser->context, enum_index, case_name,
                               &case_index, NULL, NULL)) {
          if (!enum_subset_contains_case(parser->context,
                                         parser->expected_type, case_index)) {
            (void)append_type0121_diagnostic(
                parser->context, span, case_name, parser->expected_type);
            return expression_append(
                parser, W_SEED_FRONTEND_EXPR_UNSUPPORTED, span,
                text_from_span(parser->document, span),
                (w_seed_frontend_text){NULL, 0}, simple_type_unknown(), false,
                (size_t)W_SEED_FRONTEND_NONE,
                (size_t)W_SEED_FRONTEND_NONE, W_SEED_FRONTEND_NONE, 0, value);
          }
          frontend_simple_type enum_type = parser->expected_type;
          enum_type.kind = W_SEED_FRONTEND_TYPE_ENUM;
          if (parser->expected_type.kind == W_SEED_FRONTEND_TYPE_ENUM_SUBSET) {
            enum_type.kind = W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
          }
          enum_type.enum_index = enum_index;
          if (enum_type.enum_name.length == 0) {
            enum_type.enum_name = parser->expected_type.spelling;
          }
          value->is_enum_case = true;
          value->enum_index = enum_index;
          value->enum_case_index = case_index;
          return expression_append(
              parser, W_SEED_FRONTEND_EXPR_ENUM_CASE, span,
              text_from_span(parser->document, span),
              (w_seed_frontend_text){NULL, 0}, enum_type, true,
              (size_t)W_SEED_FRONTEND_NONE,
              (size_t)W_SEED_FRONTEND_NONE, W_SEED_FRONTEND_NONE, 0, value);
        }
      }
      if (enum_declaration_name_count(parser->context,
                                      parser->expected_type.spelling) != 1u) {
        (void)append_match0003_diagnostic(
            parser->context, span, (w_seed_frontend_text){"enum-member", 11u},
            parser->has_expected_type ? parser->expected_type.spelling
                                      : (w_seed_frontend_text){"none", 4u},
            case_name);
      } else {
        (void)context_append_fact(
            parser->context, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
            span, text_from_span(parser->document, span));
      }
      return expression_append(parser, W_SEED_FRONTEND_EXPR_UNSUPPORTED, span,
                               text_from_span(parser->document, span),
                               (w_seed_frontend_text){NULL, 0},
                               simple_type_unknown(), false,
                               (size_t)W_SEED_FRONTEND_NONE,
                               (size_t)W_SEED_FRONTEND_NONE,
                               W_SEED_FRONTEND_NONE, 0, value);
    }
    const bool ambiguous_expected_enum =
        parser->has_expected_type &&
        parser->expected_type.kind == W_SEED_FRONTEND_TYPE_NOMINAL &&
        enum_declaration_name_count(parser->context,
                                    parser->expected_type.spelling) > 1u;
    if ((!parser->has_expected_type || ambiguous_expected_enum) &&
        !parser->suppress_short_diagnostic) {
      (void)append_match0003_diagnostic(
          parser->context, span, (w_seed_frontend_text){"short-enum", 10u},
          (w_seed_frontend_text){"none", 4u}, case_name);
    } else {
      (void)context_append_fact(
          parser->context, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION, span,
          text_from_span(parser->document, span));
    }
    return expression_append(parser, W_SEED_FRONTEND_EXPR_UNSUPPORTED, span,
                             text_from_span(parser->document, span),
                             (w_seed_frontend_text){NULL, 0},
                             simple_type_unknown(), false,
                             (size_t)W_SEED_FRONTEND_NONE,
                             (size_t)W_SEED_FRONTEND_NONE,
                             W_SEED_FRONTEND_NONE, 0, value);
  }
  const w_seed_span span = token.span;
  (void)context_append_fact(parser->context,
                            W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION, span,
                            spelling);
  return expression_append(parser, W_SEED_FRONTEND_EXPR_UNSUPPORTED, span,
                           spelling, (w_seed_frontend_text){NULL, 0},
                           simple_type_unknown(), false,
                           (size_t)W_SEED_FRONTEND_NONE,
                           (size_t)W_SEED_FRONTEND_NONE,
                           W_SEED_FRONTEND_NONE, 0, value);
}

static bool call_label_known(const frontend_context *context,
                             w_seed_frontend_text callee,
                             w_seed_frontend_text label, bool *resolved) {
  if (resolved != NULL) *resolved = false;
  const w_seed_frontend_document *owner_doc = NULL;
  uint32_t function_node = W_SEED_CST_NONE;
  if (!function_signature_for_name(context, callee, &owner_doc, &function_node)) {
    return false;
  }
  if (resolved != NULL) *resolved = true;
  const uint32_t parameters =
      first_direct_kind(owner_doc, function_node, W_SEED_CST_PARAMETER_LIST);
  if (parameters == W_SEED_CST_NONE) return false;
  uint32_t cursor = owner_doc->nodes[parameters].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(owner_doc, &cursor, &child) &&
         guard < owner_doc->parse.node_count) {
    if (owner_doc->nodes[child].kind == W_SEED_CST_PARAMETER) {
      const w_seed_frontend_label_kind policy = parameter_label_kind(
          owner_doc, owner_doc->nodes[child].raw_span);
      const w_seed_frontend_text parameter_label =
          parameter_external_label_from_span(owner_doc,
                                             owner_doc->nodes[child].raw_span);
      if (label.length == 0) {
        return policy == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY ||
               policy == W_SEED_FRONTEND_LABEL_OPTIONAL;
      }
      if (policy == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY) {
        guard += 1;
        continue;
      }
      if (text_equal_text(parameter_label, label)) return true;
    }
    guard += 1;
  }
  return false;
}

static bool expression_parse_postfix(frontend_expression_parser *parser,
                                     frontend_expr_value *value) {
  bool enum_case_constructor_called = false;
  while (true) {
    frontend_token token;
    if (!cursor_peek(&parser->cursor, &token)) break;
    if (token_text(parser->document, &token, ".") ||
        token_text(parser->document, &token, "?.")) {
      (void)cursor_take(&parser->cursor, &token);
      frontend_token member;
      if (!cursor_take(&parser->cursor, &member) ||
          member.kind != W_SEED_CST_WORD) return false;
      const w_seed_span span = {value->span.start_byte, member.span.end_byte};
      const w_seed_frontend_text member_name =
          text_from_span(parser->document, member.span);
      bool supported = false;
      frontend_simple_type result_type = simple_type_unknown();
      if (value->type.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST &&
          text_equal(member_name, "count")) {
        result_type = simple_type_from_view((w_seed_frontend_text){"usize", 5});
        supported = true;
      }
      if (!supported) {
        (void)context_append_fact(
            parser->context, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
            span, text_from_span(parser->document, span));
      }
      if (!expression_append(parser,
                             supported ? W_SEED_FRONTEND_EXPR_MEMBER
                                       : W_SEED_FRONTEND_EXPR_UNSUPPORTED,
                             span, text_from_span(parser->document, span),
                             text_from_span(parser->document, token.span),
                             result_type, supported, value->index,
                             (size_t)W_SEED_FRONTEND_NONE,
                             W_SEED_FRONTEND_NONE, 0, value)) {
        return false;
      }
      if (parser->context->emit && parser->context->output != NULL &&
          value->index < parser->context->count.expressions) {
        parser->context->output->expressions[value->index].member_name =
            member_name;
      }
      value->is_integer_literal = false;
      continue;
    }
    if (token_text(parser->document, &token, "[")) {
      (void)cursor_take(&parser->cursor, &token);
      frontend_expr_value index_value;
      if (!expression_parse_bp(parser, 0, &index_value) ||
          !cursor_take_text(&parser->cursor, "]", NULL)) return false;
      const w_seed_span span = {value->span.start_byte,
                                index_value.span.end_byte + 1u};
      frontend_simple_type result_type = simple_type_unknown();
      bool supported = value->type.kind == W_SEED_FRONTEND_TYPE_STATIC_LIST &&
                       index_value.type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                       !index_value.type.is_signed;
      if (supported) result_type = static_list_element_type(parser->context, value->type);
      if (result_type.kind == W_SEED_FRONTEND_TYPE_UNKNOWN) supported = false;
      if (!supported) {
        (void)context_append_fact(
            parser->context, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
            span, text_from_span(parser->document, span));
      }
      if (!expression_append(parser,
                             supported ? W_SEED_FRONTEND_EXPR_INDEX
                                       : W_SEED_FRONTEND_EXPR_UNSUPPORTED,
                             span, text_from_span(parser->document, span),
                             (w_seed_frontend_text){"[]", 2}, result_type,
                             supported, value->index, index_value.index,
                             W_SEED_FRONTEND_NONE, 0, value)) {
        return false;
      }
      value->is_integer_literal = false;
      continue;
    }
    if (!token_text(parser->document, &token, "(")) break;
    (void)cursor_take(&parser->cursor, &token);
    const uint32_t first_argument = (uint32_t)parser->context->count.arguments;
    size_t argument_count = 0;
    bool labels_valid = true;
    const bool enum_case_constructor = value->is_enum_case;
    const w_seed_frontend_text diagnostic_declaration =
        enum_case_constructor
            ? diagnostic_enum_case_name(parser->context, value->enum_case_index)
            : value->name;
    bool enum_constructor_diagnostic_emitted = false;
    const size_t enum_constructor_parameter_count =
        enum_case_constructor
            ? enum_case_parameter_count(parser->context,
                                        value->enum_case_index)
            : 0;
    if (enum_case_constructor) enum_case_constructor_called = true;
    const w_seed_frontend_document *signature_doc = NULL;
    uint32_t signature_node = W_SEED_CST_NONE;
    const bool local_signature =
        value->has_name && function_signature_for_name(
                                parser->context, value->name, &signature_doc,
                                &signature_node);
    const w_seed_frontend_external_symbol *external_signature = NULL;
    const bool external_signature_found =
        !local_signature && value->has_name &&
        external_symbol_for_name(parser->context, value->name,
                                 &external_signature);
    const w_seed_frontend_host_prelude_symbol *host_signature = NULL;
    const bool host_signature_found =
        !local_signature && !external_signature_found && value->has_name &&
        host_symbol_for_name(parser->context, value->name, &host_signature,
                             NULL);
    while (!cursor_peek_text(&parser->cursor, ")")) {
      frontend_token possible_label;
      w_seed_frontend_text label = {NULL, 0};
      if (cursor_peek(&parser->cursor, &possible_label) &&
          possible_label.kind == W_SEED_CST_WORD) {
        frontend_token_cursor look = parser->cursor;
        (void)cursor_take(&look, &possible_label);
        if (cursor_peek_text(&look, ":")) {
          (void)cursor_take(&parser->cursor, &possible_label);
          (void)cursor_take_text(&parser->cursor, ":", NULL);
          label = text_from_span(parser->document, possible_label.span);
        }
      }
      if (label.length != 0 && value->has_name) {
        bool resolved = false;
        bool known = call_label_known(parser->context, value->name, label,
                                      &resolved);
        if (!resolved) {
          known = external_label_known(parser->context, value->name, label,
                                       &resolved);
        }
        if (!resolved) {
          known = host_label_known(parser->context, value->name, label,
                                   &resolved);
        }
        if (resolved && !known) {
          labels_valid = false;
          w_seed_frontend_text accepted_forms[2];
          const size_t accepted_count = diagnostic_call_accepted_forms(
              parser->context, enum_case_constructor, value->enum_case_index,
              signature_doc, signature_node, external_signature, host_signature,
              argument_count, accepted_forms,
              sizeof(accepted_forms) / sizeof(accepted_forms[0]));
          (void)append_label0005_diagnostic(
              parser->context, value->span, diagnostic_declaration, label,
              accepted_forms, accepted_count);
        }
      }
      frontend_simple_type expected = simple_type_unknown();
      bool expected_found = false;
      bool enum_label_valid = false;
      bool enum_label_previous = false;
      if (enum_case_constructor) {
        expected_found = enum_case_argument_expected(
            parser->context, value->enum_case_index, argument_count, label,
            &expected, &enum_label_valid, &enum_label_previous);
      } else if (local_signature || external_signature_found ||
                 host_signature_found) {
        expected_found = local_signature
                             ? local_argument_expected(
                                   parser->context, signature_doc,
                                   signature_node, argument_count, label,
                                   &expected)
                             : external_signature_found
                                   ? external_argument_expected(
                                         parser->context, external_signature,
                                         argument_count, label, &expected)
                                   : host_argument_expected(
                                         parser->context, host_signature,
                                         argument_count, label, &expected);
      }
      const frontend_simple_type saved_expected = parser->expected_type;
      const bool saved_has_expected = parser->has_expected_type;
      const bool saved_suppress_short = parser->suppress_short_diagnostic;
      parser->expected_type = expected;
      parser->has_expected_type = expected_found;
      parser->suppress_short_diagnostic =
          !expected_found && !local_signature && !external_signature_found &&
          !host_signature_found;
      frontend_expr_value argument_value;
      if (!expression_parse_bp(parser, 0, &argument_value)) return false;
      parser->expected_type = saved_expected;
      parser->has_expected_type = saved_has_expected;
      parser->suppress_short_diagnostic = saved_suppress_short;
      if (enum_case_constructor || local_signature ||
          external_signature_found || host_signature_found) {
        if (!expected_found) {
          labels_valid = false;
          if (enum_case_constructor) {
            if (!enum_constructor_diagnostic_emitted) {
              w_seed_frontend_text accepted_forms[2];
              const size_t accepted_count = diagnostic_call_accepted_forms(
                  parser->context, true, value->enum_case_index, NULL,
                  W_SEED_CST_NONE, NULL, NULL, argument_count, accepted_forms,
                  sizeof(accepted_forms) / sizeof(accepted_forms[0]));
              (void)append_label0005_diagnostic(
                  parser->context, value->span, diagnostic_declaration, label,
                  accepted_forms, accepted_count);
              enum_constructor_diagnostic_emitted = true;
            }
          } else if (label.length == 0) {
            w_seed_frontend_text accepted_forms[2];
            const size_t accepted_count = diagnostic_call_accepted_forms(
                parser->context, false, value->enum_case_index, signature_doc,
                signature_node, external_signature, host_signature,
                argument_count,
                accepted_forms,
                sizeof(accepted_forms) / sizeof(accepted_forms[0]));
            (void)append_label0005_diagnostic(
                parser->context, value->span, diagnostic_declaration, label,
                accepted_forms, accepted_count);
          }
        } else if (enum_case_constructor && !enum_label_valid) {
          labels_valid = false;
          if (!enum_constructor_diagnostic_emitted) {
            if (enum_label_previous) {
              (void)append_label0006_diagnostic(
                  parser->context, value->span, diagnostic_declaration, label,
                  label);
            } else {
              w_seed_frontend_text accepted_forms[2];
              const size_t accepted_count = diagnostic_call_accepted_forms(
                  parser->context, true, value->enum_case_index, NULL,
                  W_SEED_CST_NONE, NULL, NULL, argument_count, accepted_forms,
                  sizeof(accepted_forms) / sizeof(accepted_forms[0]));
              (void)append_label0005_diagnostic(
                  parser->context, value->span, diagnostic_declaration, label,
                  accepted_forms, accepted_count);
            }
            enum_constructor_diagnostic_emitted = true;
          }
        } else if (argument_value.type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
                   expected.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
                   !frontend_widening_allowed(parser->context,
                                               argument_value.type,
                                               expected)) {
          labels_valid = false;
          if (enum_case_constructor) {
            if (!enum_constructor_diagnostic_emitted) {
              const bool narrowing =
                  argument_value.type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                  expected.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                  argument_value.type.is_signed == expected.is_signed &&
                  argument_value.type.bit_width != 0 &&
                  expected.bit_width != 0 &&
                  argument_value.type.bit_width > expected.bit_width;
              if (narrowing) {
                (void)append_type0122_diagnostic(
                    parser->context, argument_value.span, argument_value.type,
                    expected, (w_seed_frontend_text){
                                  "integer narrowing is not implicit",
                                  sizeof("integer narrowing is not implicit") - 1u});
              } else {
                (void)append_sem0001_diagnostic(
                    parser->context, argument_value.span,
                    argument_value.type.spelling, expected.spelling);
              }
              enum_constructor_diagnostic_emitted = true;
            }
          } else {
            (void)context_append_fact(
                parser->context, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                argument_value.span,
                text_from_span(parser->document, argument_value.span));
          }
        }
      }
      w_seed_frontend_argument argument;
      argument.module_index = (uint32_t)parser->context->module_index;
      argument.owner_expression = value->index >= (size_t)UINT32_MAX
                                     ? W_SEED_FRONTEND_NONE
                                     : (uint32_t)value->index;
      argument.label = label;
      argument.span = argument_value.span;
      argument.expression_index = argument_value.index >= (size_t)UINT32_MAX
                                      ? W_SEED_FRONTEND_NONE
                                      : (uint32_t)argument_value.index;
      argument.resolved_parameter_ordinal = W_SEED_FRONTEND_NONE;
      uint32_t argument_index = W_SEED_FRONTEND_NONE;
      if (!context_append_argument(parser->context, argument, &argument_index)) {
        return false;
      }
      argument_count += 1;
      if (!cursor_peek_text(&parser->cursor, ",")) break;
      (void)cursor_take_text(&parser->cursor, ",", NULL);
    }
    frontend_token close;
    if (!cursor_take_text(&parser->cursor, ")", &close)) return false;
    if (enum_case_constructor) {
      if (enum_constructor_parameter_count != argument_count) {
        labels_valid = false;
        if (!enum_constructor_diagnostic_emitted) {
          w_seed_frontend_text accepted_forms[2];
          const size_t accepted_count = diagnostic_call_accepted_forms(
              parser->context, true, value->enum_case_index, NULL,
              W_SEED_CST_NONE, NULL, NULL,
              argument_count < enum_constructor_parameter_count
                  ? argument_count
                  : 0u,
              accepted_forms,
              sizeof(accepted_forms) / sizeof(accepted_forms[0]));
          (void)append_label0005_diagnostic(
              parser->context, value->span, diagnostic_declaration,
              (w_seed_frontend_text){NULL, 0u}, accepted_forms,
              accepted_count);
          enum_constructor_diagnostic_emitted = true;
        }
      }
    } else if (local_signature) {
      const uint32_t parameters = first_direct_kind(
          signature_doc, signature_node, W_SEED_CST_PARAMETER_LIST);
      if (parameters == W_SEED_CST_NONE ||
          count_direct_kind(signature_doc, parameters, W_SEED_CST_PARAMETER) !=
              argument_count) {
        labels_valid = false;
      }
    } else if (external_signature_found &&
               external_signature->parameter_count != argument_count) {
      labels_valid = false;
    } else if (host_signature_found &&
               host_signature->parameter_count != argument_count) {
      labels_valid = false;
    }
    if (value->has_name) {
      bool resolved = false;
      (void)call_label_known(parser->context, value->name,
                             (w_seed_frontend_text){NULL, 0}, &resolved);
      if (!resolved) {
        (void)external_label_known(parser->context, value->name,
                                   (w_seed_frontend_text){NULL, 0}, &resolved);
      }
      if (!resolved) {
        (void)host_label_known(parser->context, value->name,
                               (w_seed_frontend_text){NULL, 0}, &resolved);
      }
      if (!resolved && value->supported) {
        (void)context_append_fact(parser->context,
                                  W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL,
                                  value->span, value->name);
      }
    }
    frontend_simple_type return_type = simple_type_unknown();
    if (enum_case_constructor) {
      return_type = value->type;
    } else if (local_signature) {
      return_type = function_return_type(parser->context, signature_doc,
                                          signature_node);
    } else if (external_signature_found) {
      return_type = simple_type_from_view(external_signature->return_type);
    } else if (host_signature_found) {
      return_type = simple_type_from_view(host_signature->return_type);
    }
    const w_seed_span span = {value->span.start_byte, close.span.end_byte};
    if (enum_case_constructor && enum_constructor_parameter_count == 0) {
      labels_valid = false;
      if (!enum_constructor_diagnostic_emitted) {
        const w_seed_frontend_text positional = {"positional", 10u};
        (void)append_label0005_diagnostic(
            parser->context, span, diagnostic_declaration,
            (w_seed_frontend_text){NULL, 0u}, &positional, 1u);
        enum_constructor_diagnostic_emitted = true;
      }
    }
    if (!labels_valid) {
      if (!enum_case_constructor) {
        (void)context_append_fact(parser->context,
                                  W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                  span, text_from_span(parser->document, span));
      }
    }
    bool const_call_safe = true;
    if (parser->context->current_function_is_const && value->has_name &&
        !enum_case_constructor) {
      const bool local_const =
          local_signature && function_node_is_const(signature_doc, signature_node);
      const bool external_const =
          external_signature_found && external_signature->is_const;
      const bool host_const = host_signature_found && host_signature->is_const;
      if ((local_signature && !local_const) ||
          (external_signature_found && !external_const) ||
          (host_signature_found && !host_const)) {
        const_call_safe = false;
        (void)const_record_failure(parser->context, span, value->name);
      }
    }
    const bool supported = value->supported && labels_valid &&
                           return_type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
                           const_call_safe;
    if (!supported && value->has_name && return_type.kind ==
                                      W_SEED_FRONTEND_TYPE_UNKNOWN) {
      (void)context_append_fact(parser->context,
                                W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                span, text_from_span(parser->document, span));
    }
    const size_t callee_index = value->index;
    const w_seed_frontend_text callee_name =
        value->has_name || value->is_enum_case
            ? text_from_span(parser->document, value->span)
            : (w_seed_frontend_text){NULL, 0u};
    if (!expression_append(parser, W_SEED_FRONTEND_EXPR_CALL, span,
                           text_from_span(parser->document, span),
                           (w_seed_frontend_text){NULL, 0}, return_type,
                           supported, value->index,
                           (size_t)W_SEED_FRONTEND_NONE, first_argument,
                           argument_count, value)) {
      return false;
    }
    if (!parser->context->emit) {
      w_seed_frontend_callee_kind identity_kind =
          W_SEED_FRONTEND_CALLEE_NONE;
      uint32_t host_symbol_identity = W_SEED_FRONTEND_NONE;
      uint32_t external_module_identity = W_SEED_FRONTEND_NONE;
      uint32_t external_symbol_identity = W_SEED_FRONTEND_NONE;
      if (local_signature) {
        identity_kind = W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION;
      } else if (external_signature_found &&
                 external_symbol_identity_for_name(
                     parser->context, callee_name, &external_module_identity,
                     &external_symbol_identity)) {
        identity_kind = W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL;
      } else if (host_signature_found &&
                 host_symbol_for_name(parser->context, callee_name, NULL,
                                      &host_symbol_identity)) {
        identity_kind = W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL;
      }
      if (!receipt_size_call_identity(
              parser->context, value->index, callee_index,
              identity_kind, host_symbol_identity, external_module_identity,
              external_symbol_identity)) {
        return false;
      }
    }
    value->is_integer_literal = false;
  }
  if (value->is_enum_case && !enum_case_constructor_called &&
      enum_case_parameter_count(parser->context, value->enum_case_index) != 0) {
    (void)context_append_fact(parser->context,
                              W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                              value->span,
                              text_from_span(parser->document, value->span));
    value->supported = false;
    if (parser->context->emit && parser->context->output != NULL &&
        value->index < parser->context->count.expressions) {
      parser->context->output->expressions[value->index].supported = false;
    }
  }
  return true;
}

static bool expression_parse_prefix_inner(frontend_expression_parser *parser,
                                          frontend_expr_value *value) {
  if (parser == NULL || value == NULL) return false;
  frontend_token token;
  if (!cursor_peek(&parser->cursor, &token)) return false;
  if (token_text(parser->document, &token, "!") ||
      token_text(parser->document, &token, "-")) {
    (void)cursor_take(&parser->cursor, &token);
    frontend_expr_value nested;
    if (!expression_parse_prefix(parser, &nested)) return false;
    const w_seed_span span = {token.span.start_byte, nested.span.end_byte};
    const bool valid = token_text(parser->document, &token, "!")
                           ? type_is_bool(nested.type)
                           : type_is_numeric(nested.type);
    if (!valid) {
      (void)context_append_fact(parser->context,
                                W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                span, text_from_span(parser->document, span));
    }
    const bool appended = expression_append(
        parser,
        valid ? W_SEED_FRONTEND_EXPR_UNARY
              : W_SEED_FRONTEND_EXPR_UNSUPPORTED,
        span, text_from_span(parser->document, span),
        text_from_span(parser->document, token.span),
        token_text(parser->document, &token, "!")
            ? simple_type_from_view((w_seed_frontend_text){"Bool", 4})
            : nested.type,
        nested.supported && valid, nested.index,
        (size_t)W_SEED_FRONTEND_NONE, W_SEED_FRONTEND_NONE, 0, value);
    if (appended) value->is_integer_literal = false;
    return appended;
  }
  if (!expression_parse_primary(parser, value)) return false;
  return expression_parse_postfix(parser, value);
}

static bool expression_parse_prefix(frontend_expression_parser *parser,
                                     frontend_expr_value *value) {
  if (parser == NULL || value == NULL ||
      parser->depth >= W_SEED_FRONTEND_MAX_NESTING) {
    return false;
  }
  parser->depth += 1u;
  const bool result = expression_parse_prefix_inner(parser, value);
  parser->depth -= 1u;
  return result;
}

typedef struct {
  uint32_t case_index;
  w_seed_span span;
} frontend_membership_item;

static void membership_consume_to_delimiter(frontend_expression_parser *parser,
                                            w_seed_span *span) {
  if (parser == NULL) return;
  size_t nested = 0;
  frontend_token token;
  while (cursor_take(&parser->cursor, &token)) {
    if (span != NULL) span->end_byte = token.span.end_byte;
    if (token_text(parser->document, &token, "(")) {
      nested += 1u;
      continue;
    }
    if (token_text(parser->document, &token, ")")) {
      if (nested != 0) {
        nested -= 1u;
        continue;
      }
      return;
    }
    if (nested == 0 && token_text(parser->document, &token, ",")) return;
  }
}

static bool expression_parse_membership(
    frontend_expression_parser *parser, const frontend_expr_value *subject,
    frontend_token operator_token, frontend_expr_value *value) {
  if (parser == NULL || subject == NULL || value == NULL) return false;
  frontend_token open;
  if (!cursor_take_text(&parser->cursor, "(", &open)) return false;
  frontend_membership_item items[FRONTEND_MAX_MEMBERSHIP_ITEMS];
  size_t item_count = 0;
  bool valid = subject->supported && subject->has_name &&
               frontend_type_is_enum(subject->type) &&
               subject->type.enum_index != W_SEED_FRONTEND_NONE &&
               subject->type.enum_name.length != 0;
  bool saw_item = false;
  bool closed = false;
  w_seed_span span = {subject->span.start_byte, open.span.end_byte};
  while (true) {
    frontend_token next;
    if (!cursor_peek(&parser->cursor, &next)) break;
    if (token_text(parser->document, &next, ")")) {
      (void)cursor_take(&parser->cursor, &next);
      span.end_byte = next.span.end_byte;
      closed = true;
      break;
    }
    frontend_token first;
    if (!cursor_take(&parser->cursor, &first)) break;
    span.end_byte = first.span.end_byte;
    w_seed_frontend_text qualifier = {NULL, 0};
    w_seed_frontend_text case_name = {NULL, 0};
    w_seed_span case_span = first.span;
    bool item_shape = false;
    if (token_text(parser->document, &first, ".")) {
      frontend_token member;
      if (cursor_take(&parser->cursor, &member) &&
          member.kind == W_SEED_CST_WORD) {
        case_name = text_from_span(parser->document, member.span);
        case_span.end_byte = member.span.end_byte;
        item_shape = true;
      }
    } else if (first.kind == W_SEED_CST_WORD) {
      frontend_token dot;
      frontend_token member;
      if (cursor_take(&parser->cursor, &dot) &&
          token_text(parser->document, &dot, ".") &&
          cursor_take(&parser->cursor, &member) &&
          member.kind == W_SEED_CST_WORD) {
        qualifier = text_from_span(parser->document, first.span);
        case_name = text_from_span(parser->document, member.span);
        case_span.end_byte = member.span.end_byte;
        item_shape = true;
      }
    }
    saw_item = true;
    if (!item_shape) {
      valid = false;
      membership_consume_to_delimiter(parser, &span);
    } else {
      bool case_valid = valid;
      if (qualifier.length != 0 &&
          !text_equal_text(qualifier, subject->type.enum_name)) {
        case_valid = false;
      }
      uint32_t case_index = W_SEED_FRONTEND_NONE;
      if (case_valid &&
          !enum_case_for_name(parser->context, subject->type.enum_index,
                              case_name, &case_index, NULL, NULL)) {
        case_valid = false;
      }
      if (case_valid && enum_case_parameter_count(parser->context, case_index) != 0)
        case_valid = false;
      frontend_token payload;
      if (cursor_peek(&parser->cursor, &payload) &&
          token_text(parser->document, &payload, "(")) {
        case_valid = false;
        membership_consume_to_delimiter(parser, &span);
      }
      if (case_valid) {
        for (size_t prior = 0; prior < item_count; prior += 1) {
          if (items[prior].case_index == case_index) {
            case_valid = false;
            break;
          }
        }
      }
      if (!case_valid) {
        valid = false;
      } else if (item_count < FRONTEND_MAX_MEMBERSHIP_ITEMS) {
        items[item_count].case_index = case_index;
        items[item_count].span = case_span;
        item_count += 1u;
      } else {
        valid = false;
      }
    }
    frontend_token separator;
    if (cursor_peek(&parser->cursor, &separator) &&
        token_text(parser->document, &separator, ",")) {
      (void)cursor_take(&parser->cursor, &separator);
      span.end_byte = separator.span.end_byte;
      continue;
    }
    if (cursor_peek(&parser->cursor, &separator) &&
        token_text(parser->document, &separator, ")")) {
      (void)cursor_take(&parser->cursor, &separator);
      span.end_byte = separator.span.end_byte;
      closed = true;
      break;
    }
    valid = false;
    membership_consume_to_delimiter(parser, &span);
    if (cursor_peek(&parser->cursor, &separator) &&
        token_text(parser->document, &separator, ")")) {
      (void)cursor_take(&parser->cursor, &separator);
      span.end_byte = separator.span.end_byte;
      closed = true;
    }
    break;
  }
  if (!closed || !saw_item || item_count == 0) valid = false;
  /* Canonical identity follows enum declaration order.  Source spans remain
   * attached to each case record for diagnostics and source maps. */
  for (size_t left = 0; left < item_count; left += 1) {
    for (size_t right = left + 1; right < item_count; right += 1) {
      if (items[right].case_index < items[left].case_index) {
        const frontend_membership_item swap = items[left];
        items[left] = items[right];
        items[right] = swap;
      }
    }
  }
  if (!valid) {
    (void)context_append_fact(parser->context,
                              W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                              span, text_from_span(parser->document, span));
  }
  frontend_simple_type bool_type =
      simple_type_from_view((w_seed_frontend_text){"Bool", 4});
  (void)memset(value, 0, sizeof(*value));
  value->index = W_SEED_FRONTEND_NONE;
  value->enum_index = subject->type.enum_index;
  value->enum_case_index = W_SEED_FRONTEND_NONE;
  if (!expression_append(parser, W_SEED_FRONTEND_EXPR_ENUM_MEMBERSHIP, span,
                         text_from_span(parser->document, span),
                         text_from_span(parser->document, operator_token.span),
                         bool_type, valid, subject->index,
                         (size_t)W_SEED_FRONTEND_NONE, W_SEED_FRONTEND_NONE, 0,
                         value)) {
    return false;
  }
  const uint32_t owner = (uint32_t)value->index;
  const uint32_t first_case = (uint32_t)parser->context->count.enum_membership_cases;
  if (valid) {
    for (size_t index = 0; index < item_count; index += 1) {
      w_seed_frontend_enum_membership_case record;
      record.module_index = (uint32_t)parser->context->module_index;
      record.owner_expression = owner;
      record.enum_base_index = subject->type.enum_index;
      record.enum_case_index = items[index].case_index;
      record.source_span = items[index].span;
      uint32_t ignored = W_SEED_FRONTEND_NONE;
      if (!context_append_enum_membership_case(parser->context, record,
                                               &ignored)) {
        return false;
      }
    }
  }
  if (parser->context->emit && parser->context->output != NULL &&
      owner < parser->context->output->expression_capacity) {
    w_seed_frontend_expression *record =
        &parser->context->output->expressions[owner];
    record->first_membership_case = valid ? first_case : W_SEED_FRONTEND_NONE;
    record->membership_case_count = valid
                                        ? (uint32_t)(parser->context->count.enum_membership_cases -
                                                     (size_t)first_case)
                                        : 0;
    record->supported = valid;
  }
  value->type = bool_type;
  value->supported = valid;
  value->has_name = false;
  value->is_enum_case = false;
  value->enum_index = subject->type.enum_index;
  value->enum_case_index = W_SEED_FRONTEND_NONE;
  value->name = (w_seed_frontend_text){NULL, 0};
  value->operator_text = text_from_span(parser->document, operator_token.span);
  value->span = span;
  return true;
}

static bool expression_parse_bp_inner(frontend_expression_parser *parser,
                                      int minimum_precedence,
                                      frontend_expr_value *value) {
  if (parser == NULL || value == NULL ||
      !expression_parse_prefix(parser, value)) {
    return false;
  }
  while (true) {
    frontend_token operator_token;
    if (!cursor_peek(&parser->cursor, &operator_token)) break;
    const w_seed_frontend_text operator_text =
        text_from_span(parser->document, operator_token.span);
    const int precedence = operator_precedence(operator_text);
    if (precedence < minimum_precedence || !is_binary_operator(operator_text)) {
      break;
    }
    (void)cursor_take(&parser->cursor, &operator_token);
    if (text_equal(operator_text, "in")) {
      const frontend_expr_value subject = *value;
      if (!expression_parse_membership(parser, &subject, operator_token,
                                       value)) {
        return false;
      }
      continue;
    }
    frontend_expr_value right;
    const int next_precedence = precedence + 1;
    if (!expression_parse_bp(parser, next_precedence, &right)) return false;
    const w_seed_span span = {value->span.start_byte, right.span.end_byte};
    frontend_simple_type result_type = value->type;
    bool supported = value->supported && right.supported;
    const bool arithmetic_or_comparison =
        text_equal(operator_text, "+") || text_equal(operator_text, "-") ||
        text_equal(operator_text, "*") || text_equal(operator_text, "/") ||
        text_equal(operator_text, "%") || text_equal(operator_text, "==") ||
        text_equal(operator_text, "!=") || text_equal(operator_text, "<") ||
        text_equal(operator_text, "<=") || text_equal(operator_text, ">") ||
        text_equal(operator_text, ">=");
    if (arithmetic_or_comparison &&
        value->type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
        right.type.kind == W_SEED_FRONTEND_TYPE_INTEGER) {
      /* Widening validation accepts a width-zero literal, but the emitted
       * child record must carry the concrete operand type as well.  Coerce
       * exactly one unsuffixed literal when the other operand is concrete and
       * the literal is representable. */
      if (expression_value_is_unsuffixed_integer(value) &&
          right.type.bit_width != 0u &&
          unsuffixed_integer_fits(value->type.spelling, right.type)) {
        if (!expression_value_set_type(parser, value, right.type)) return false;
      } else if (expression_value_is_unsuffixed_integer(&right) &&
                 value->type.bit_width != 0u &&
                 unsuffixed_integer_fits(right.type.spelling, value->type)) {
        if (!expression_value_set_type(parser, &right, value->type)) return false;
      }
    }
    if (text_equal(operator_text, "&&") || text_equal(operator_text, "||")) {
      result_type = simple_type_from_view((w_seed_frontend_text){"Bool", 4});
      if (!type_is_bool(value->type) || !type_is_bool(right.type)) supported = false;
    } else if (text_equal(operator_text, "..<")) {
      result_type = simple_type_from_view((w_seed_frontend_text){"Range<usize>", 12});

      /* A range is a usize-boundary expression.  Give an unsuffixed integer
       * literal the contextual unsigned type before comparing the operands.
       * This keeps `1..<stages.count` source-backed and makes both dry and
       * emit passes observe the same supported root metadata. */
      frontend_simple_type expected_element = simple_type_unknown();
      if (parser->has_expected_type &&
          parser->expected_type.kind == W_SEED_FRONTEND_TYPE_RANGE &&
          parser->expected_type.spelling.length > 7u &&
          parser->expected_type.spelling.data != NULL) {
        const size_t element_length =
            parser->expected_type.spelling.length - 7u;
        expected_element = simple_type_from_view((w_seed_frontend_text){
            parser->expected_type.spelling.data + 6u, element_length});
      }
      const bool left_unsuffixed =
          value->is_integer_literal &&
          value->type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
          value->type.is_signed && value->type.bit_width == 0u;
      const bool right_unsuffixed =
          right.is_integer_literal &&
          right.type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
          right.type.is_signed && right.type.bit_width == 0u;
      if (left_unsuffixed) {
        frontend_simple_type target = expected_element;
        if (target.kind != W_SEED_FRONTEND_TYPE_INTEGER &&
            right.type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            !right.type.is_signed) {
          target = right.type;
        }
        if (target.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            !target.is_signed &&
            !expression_value_set_type(parser, value, target)) {
          return false;
        }
      }
      if (right_unsuffixed) {
        frontend_simple_type target = expected_element;
        if (target.kind != W_SEED_FRONTEND_TYPE_INTEGER &&
            value->type.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            !value->type.is_signed) {
          target = value->type;
        }
        if (target.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            !target.is_signed &&
            !expression_value_set_type(parser, &right, target)) {
          return false;
        }
      }
      if (value->type.kind != W_SEED_FRONTEND_TYPE_INTEGER ||
          right.type.kind != W_SEED_FRONTEND_TYPE_INTEGER ||
          value->type.is_signed != right.type.is_signed ||
          value->type.bit_width != right.type.bit_width) {
        supported = false;
      }
    } else if (text_equal(operator_text, "==") ||
               text_equal(operator_text, "!=") || text_equal(operator_text, "<") ||
               text_equal(operator_text, "<=") || text_equal(operator_text, ">") ||
               text_equal(operator_text, ">=") || text_equal(operator_text, "in")) {
      result_type = simple_type_from_view((w_seed_frontend_text){"Bool", 4});
      if (!frontend_type_equal(parser->context, value->type, right.type) &&
          !(type_is_numeric(value->type) && type_is_numeric(right.type) &&
            (frontend_widening_allowed(parser->context, value->type,
                                       right.type) ||
             frontend_widening_allowed(parser->context, right.type,
                                       value->type)))) {
        supported = false;
      }
    } else if (type_is_numeric(value->type) && type_is_numeric(right.type)) {
      if (!frontend_type_equal(parser->context, value->type, right.type)) {
        result_type = frontend_widening_allowed(parser->context, value->type,
                                                right.type)
                          ? right.type
                          : (frontend_widening_allowed(parser->context,
                                                       right.type,
                                                       value->type)
                                 ? value->type
                                 : simple_type_unknown());
        if (result_type.kind == W_SEED_FRONTEND_TYPE_UNKNOWN) supported = false;
      }
    } else {
      supported = false;
    }
    if (!supported) {
      (void)context_append_fact(parser->context,
                                W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                span, text_from_span(parser->document, span));
    }
    if (!expression_append(parser,
                           supported
                               ? (text_equal(operator_text, "..<")
                                      ? W_SEED_FRONTEND_EXPR_RANGE
                                      : W_SEED_FRONTEND_EXPR_BINARY)
                               : W_SEED_FRONTEND_EXPR_UNSUPPORTED,
                           span, text_from_span(parser->document, span),
                           operator_text, result_type, supported, value->index,
                           right.index, W_SEED_FRONTEND_NONE, 0, value)) {
      return false;
    }
    value->is_integer_literal = false;
  }
  return true;
}

static bool expression_parse_bp(frontend_expression_parser *parser,
                                int minimum_precedence,
                                frontend_expr_value *value) {
  if (parser == NULL || value == NULL ||
      parser->depth >= W_SEED_FRONTEND_MAX_NESTING) {
    return false;
  }
  parser->depth += 1u;
  const bool result =
      expression_parse_bp_inner(parser, minimum_precedence, value);
  parser->depth -= 1u;
  return result;
}

static bool normalize_expression_node(frontend_context *context,
                                      uint32_t expression_node,
                                      uint32_t *expression_index,
                                      frontend_simple_type expected,
                                      frontend_simple_type *actual_out,
                                      frontend_expr_value *root_out) {
  const w_seed_frontend_document *doc = context_document(context);
  if (actual_out != NULL) *actual_out = simple_type_unknown();
  if (root_out != NULL) (void)memset(root_out, 0, sizeof(*root_out));
  if (doc == NULL || expression_index == NULL ||
      expression_node >= doc->parse.node_count) {
    return false;
  }
  const uint32_t switch_node =
      first_direct_kind(doc, expression_node, W_SEED_CST_SWITCH_EXPRESSION);
  const bool switch_owner_exact =
      switch_node != W_SEED_CST_NONE &&
      count_direct_kind(doc, expression_node, W_SEED_CST_SWITCH_EXPRESSION) ==
          1u &&
      trim_span(doc, doc->nodes[expression_node].raw_span).start_byte ==
          trim_span(doc, doc->nodes[switch_node].raw_span).start_byte &&
      trim_span(doc, doc->nodes[expression_node].raw_span).end_byte ==
          trim_span(doc, doc->nodes[switch_node].raw_span).end_byte;
  if (switch_owner_exact) {
    return normalize_switch_expression(context, switch_node, expression_index,
                                       expected, actual_out, root_out);
  }
  frontend_expression_parser parser;
  parser.context = context;
  parser.document = doc;
  parser.cursor = token_cursor_for(doc, doc->nodes[expression_node].raw_span);
  parser.depth = 0;
  parser.expected_type = expected;
  parser.has_expected_type = expected.kind != W_SEED_FRONTEND_TYPE_UNKNOWN;
  parser.suppress_short_diagnostic = false;
  frontend_expr_value value;
  if (!expression_parse_bp(&parser, 0, &value)) {
    const w_seed_span span = doc->nodes[expression_node].raw_span;
    (void)context_append_fact(context, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                              span, text_from_span(doc, span));
    w_seed_frontend_expression fallback;
    (void)memset(&fallback, 0, sizeof(fallback));
    fallback.kind = W_SEED_FRONTEND_EXPR_UNSUPPORTED;
    fallback.module_index = (uint32_t)context->module_index;
    fallback.owner_function = context->function_index;
    fallback.spelling = text_from_span(doc, span);
    fallback.span = span;
    fallback.left = W_SEED_FRONTEND_NONE;
    fallback.right = W_SEED_FRONTEND_NONE;
    fallback.first_argument = W_SEED_FRONTEND_NONE;
    fallback.enum_index = W_SEED_FRONTEND_NONE;
    fallback.enum_case_index = W_SEED_FRONTEND_NONE;
    fallback.first_switch_arm = W_SEED_FRONTEND_NONE;
    fallback.switch_arm_count = 0;
    fallback.first_membership_case = W_SEED_FRONTEND_NONE;
    fallback.membership_case_count = 0;
    fallback.resolved_parameter_ordinal = W_SEED_FRONTEND_NONE;
    fallback.resolved_function_index = W_SEED_FRONTEND_NONE;
    fallback.resolved_local_ordinal = W_SEED_FRONTEND_NONE;
    fallback.resolved_const_declaration = W_SEED_FRONTEND_NONE;
    fallback.member_name = (w_seed_frontend_text){NULL, 0};
    fallback.supported = false;
    if (actual_out != NULL) *actual_out = simple_type_unknown();
    const bool appended =
        context_append_expression(context, fallback, expression_index);
    if (appended && root_out != NULL) {
      root_out->index = *expression_index;
      root_out->left = W_SEED_FRONTEND_NONE;
      root_out->right = W_SEED_FRONTEND_NONE;
      root_out->kind = W_SEED_FRONTEND_EXPR_UNSUPPORTED;
      root_out->type = simple_type_unknown();
      root_out->supported = false;
      root_out->span = span;
    }
    return appended;
  }
  frontend_token trailing;
  if (cursor_peek(&parser.cursor, &trailing)) {
    const w_seed_span span = doc->nodes[expression_node].raw_span;
    (void)context_append_fact(context,
                              W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                              span, text_from_span(doc, span));
    w_seed_frontend_expression fallback;
    (void)memset(&fallback, 0, sizeof(fallback));
    fallback.kind = W_SEED_FRONTEND_EXPR_UNSUPPORTED;
    fallback.module_index = (uint32_t)context->module_index;
    fallback.owner_function = context->function_index;
    fallback.spelling = text_from_span(doc, span);
    fallback.span = span;
    fallback.left = W_SEED_FRONTEND_NONE;
    fallback.right = W_SEED_FRONTEND_NONE;
    fallback.first_argument = W_SEED_FRONTEND_NONE;
    fallback.enum_index = W_SEED_FRONTEND_NONE;
    fallback.enum_case_index = W_SEED_FRONTEND_NONE;
    fallback.first_switch_arm = W_SEED_FRONTEND_NONE;
    fallback.switch_arm_count = 0;
    fallback.first_membership_case = W_SEED_FRONTEND_NONE;
    fallback.membership_case_count = 0;
    fallback.resolved_parameter_ordinal = W_SEED_FRONTEND_NONE;
    fallback.resolved_function_index = W_SEED_FRONTEND_NONE;
    fallback.resolved_local_ordinal = W_SEED_FRONTEND_NONE;
    fallback.resolved_const_declaration = W_SEED_FRONTEND_NONE;
    fallback.member_name = (w_seed_frontend_text){NULL, 0};
    fallback.supported = false;
    if (actual_out != NULL) *actual_out = simple_type_unknown();
    const bool appended =
        context_append_expression(context, fallback, expression_index);
    if (appended && root_out != NULL) {
      root_out->index = *expression_index;
      root_out->left = W_SEED_FRONTEND_NONE;
      root_out->right = W_SEED_FRONTEND_NONE;
      root_out->kind = W_SEED_FRONTEND_EXPR_UNSUPPORTED;
      root_out->type = simple_type_unknown();
      root_out->supported = false;
      root_out->span = span;
    }
    return appended;
  }
  if (value.index >= (size_t)UINT32_MAX) return false;
  *expression_index = (uint32_t)value.index;
  if (actual_out != NULL) *actual_out = value.type;
  if (root_out != NULL) *root_out = value;
  return true;
}

/* Generic value arguments do not own a CST expression node.  Parse the
 * already bounded value span through the same typed Pratt frontend used by
 * statements, without reparsing source downstream. */
static bool normalize_expression_span(frontend_context *context,
                                      w_seed_span span,
                                      frontend_simple_type expected,
                                      uint32_t *expression_index,
                                      frontend_simple_type *actual_out,
                                      frontend_expr_value *root_out) {
  const w_seed_frontend_document *doc = context_document(context);
  if (actual_out != NULL) *actual_out = simple_type_unknown();
  if (root_out != NULL) (void)memset(root_out, 0, sizeof(*root_out));
  if (doc == NULL || expression_index == NULL) return false;
  frontend_expression_parser parser;
  parser.context = context;
  parser.document = doc;
  parser.cursor = token_cursor_for(doc, trim_span(doc, span));
  parser.depth = 0u;
  parser.expected_type = expected;
  parser.has_expected_type = expected.kind != W_SEED_FRONTEND_TYPE_UNKNOWN;
  parser.suppress_short_diagnostic = true;
  frontend_expr_value value;
  if (!expression_parse_bp(&parser, 0, &value)) return false;
  frontend_token trailing;
  if (cursor_peek(&parser.cursor, &trailing)) return false;
  if (value.index >= (size_t)UINT32_MAX) return false;
  *expression_index = (uint32_t)value.index;
  if (actual_out != NULL) *actual_out = value.type;
  if (root_out != NULL) *root_out = value;
  return true;
}

static w_seed_frontend_text binding_name_after_keyword(
    const w_seed_frontend_document *doc, w_seed_span span, const char *keyword) {
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  bool saw_keyword = false;
  while (cursor_take(&cursor, &token)) {
    if (!saw_keyword) {
      if (token_text(doc, &token, keyword)) saw_keyword = true;
      continue;
    }
    if (token.kind == W_SEED_CST_WORD) return text_from_span(doc, token.span);
  }
  return (w_seed_frontend_text){NULL, 0};
}

static frontend_simple_type infer_expression_span(frontend_context *context,
                                                   w_seed_span span) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return simple_type_unknown();
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token first;
  if (!cursor_peek(&cursor, &first)) return simple_type_unknown();
  const w_seed_frontend_text first_text = text_from_span(doc, first.span);
  if (text_equal(first_text, "true") || text_equal(first_text, "false")) {
    return simple_type_from_view((w_seed_frontend_text){"Bool", 4});
  }
  if (first.kind == W_SEED_CST_NUMBER ||
      first.kind == W_SEED_CST_LITERAL_EVENT) {
    return literal_simple_type(doc, first.span, first.kind);
  }
  if (text_equal(first_text, "!")) {
    return simple_type_from_view((w_seed_frontend_text){"Bool", 4});
  }
  frontend_token token;
  while (cursor_take(&cursor, &token)) {
    const w_seed_frontend_text text = text_from_span(doc, token.span);
    if (text_equal(text, ".") || text_equal(text, "?.")) {
      return simple_type_unknown();
    }
    if (text_equal(text, "in") || text_equal(text, "is") ||
        text_equal(text, "<<") || text_equal(text, ">>") ||
        text_equal(text, "??") || text_equal(text, "=") ||
        text_equal(text, "+=") || text_equal(text, "-=") ||
        text_equal(text, "*=") || text_equal(text, "/=") ||
        text_equal(text, "%=") || text_equal(text, "&") ||
        text_equal(text, "|") || text_equal(text, "^") ||
        text_equal(text, "**") || text_equal(text, "..") ||
        text_equal(text, "...") || text_equal(text, "@")) {
      return simple_type_unknown();
    }
    if (text_equal(text, "==") || text_equal(text, "!=") ||
        text_equal(text, "<") || text_equal(text, "<=") ||
        text_equal(text, ">") || text_equal(text, ">=") ||
        text_equal(text, "&&") || text_equal(text, "||")) {
      return simple_type_from_view((w_seed_frontend_text){"Bool", 4});
    }
  }
  if (first.kind == W_SEED_CST_WORD) {
    const frontend_simple_type binding =
        binding_type_for_name(context, first_text, span);
    if (binding.kind != W_SEED_FRONTEND_TYPE_UNKNOWN) return binding;
    const w_seed_frontend_document *owner_doc = NULL;
    uint32_t function_node = W_SEED_CST_NONE;
    if (function_signature_for_name(context, first_text, &owner_doc,
                                    &function_node)) {
      return function_return_type(context, owner_doc, function_node);
    }
    const w_seed_frontend_external_symbol *external = NULL;
    if (external_symbol_for_name(context, first_text, &external)) {
      return simple_type_from_view(external->return_type);
    }
  }
  return simple_type_unknown();
}

static uint32_t first_direct_expression(const w_seed_frontend_document *doc,
                                        uint32_t owner) {
  return first_direct_kind(doc, owner, W_SEED_CST_EXPRESSION);
}

static bool switch_pattern_names(
    const w_seed_frontend_document *doc, w_seed_span span,
    w_seed_frontend_text *qualifier, w_seed_frontend_text *case_name) {
  if (qualifier != NULL) *qualifier = (w_seed_frontend_text){NULL, 0};
  if (case_name != NULL) *case_name = (w_seed_frontend_text){NULL, 0};
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token first;
  if (!cursor_take(&cursor, &first)) return false;
  if (token_text(doc, &first, ".")) {
    frontend_token member;
    if (!cursor_take(&cursor, &member) || member.kind != W_SEED_CST_WORD)
      return false;
    if (case_name != NULL) *case_name = text_from_span(doc, member.span);
    return true;
  }
  if (first.kind != W_SEED_CST_WORD) return false;
  frontend_token dot;
  frontend_token member;
  if (!cursor_take(&cursor, &dot) || !token_text(doc, &dot, ".") ||
      !cursor_take(&cursor, &member) || member.kind != W_SEED_CST_WORD) {
    return false;
  }
  if (qualifier != NULL) *qualifier = text_from_span(doc, first.span);
  if (case_name != NULL) *case_name = text_from_span(doc, member.span);
  return true;
}

static bool normalize_switch_expression(
    frontend_context *context, uint32_t switch_node,
    uint32_t *expression_index, frontend_simple_type expected,
    frontend_simple_type *actual_out, frontend_expr_value *root_out) {
  const w_seed_frontend_document *doc = context_document(context);
  if (actual_out != NULL) *actual_out = simple_type_unknown();
  if (doc == NULL || expression_index == NULL ||
      switch_node >= doc->parse.node_count) {
    return false;
  }
  const w_seed_cst_node *switch_cst = &doc->nodes[switch_node];
  const uint32_t subject_node = first_direct_expression(doc, switch_node);
  if (subject_node == W_SEED_CST_NONE) return false;
  frontend_simple_type subject_type = simple_type_unknown();
  uint32_t subject_expression = W_SEED_FRONTEND_NONE;
  if (!normalize_expression_node(context, subject_node, &subject_expression,
                                 simple_type_unknown(), &subject_type, NULL)) {
    return false;
  }

  uint32_t subject_enum = W_SEED_FRONTEND_NONE;
  uint32_t subject_enum_type = W_SEED_FRONTEND_NONE;
  const w_seed_frontend_document *subject_enum_doc = NULL;
  uint32_t subject_enum_node = W_SEED_CST_NONE;
  const bool subject_is_enum =
      frontend_type_is_enum(subject_type) &&
      subject_type.enum_index != W_SEED_FRONTEND_NONE &&
      subject_type.enum_name.length != 0 &&
      enum_declaration_for_name(context, subject_type.enum_name, &subject_enum,
                                &subject_enum_type, &subject_enum_doc,
                                &subject_enum_node);

  w_seed_frontend_expression switch_record;
  (void)memset(&switch_record, 0, sizeof(switch_record));
  switch_record.kind = W_SEED_FRONTEND_EXPR_SWITCH;
  switch_record.module_index = (uint32_t)context->module_index;
  switch_record.owner_function = context->function_index;
  switch_record.spelling = text_from_span(doc, switch_cst->raw_span);
  switch_record.span = switch_cst->raw_span;
  switch_record.left = subject_expression;
  switch_record.right = W_SEED_FRONTEND_NONE;
  switch_record.first_argument = W_SEED_FRONTEND_NONE;
  switch_record.argument_count = 0;
  switch_record.inferred_type = W_SEED_FRONTEND_NONE;
  switch_record.enum_index = subject_is_enum ? subject_enum
                                             : W_SEED_FRONTEND_NONE;
  switch_record.enum_case_index = W_SEED_FRONTEND_NONE;
  switch_record.first_switch_arm = (uint32_t)context->count.switch_arms;
  switch_record.switch_arm_count = 0;
  switch_record.first_membership_case = W_SEED_FRONTEND_NONE;
  switch_record.membership_case_count = 0;
  switch_record.resolved_parameter_ordinal = W_SEED_FRONTEND_NONE;
  switch_record.resolved_function_index = W_SEED_FRONTEND_NONE;
  switch_record.supported = subject_is_enum;
  uint32_t switch_index = W_SEED_FRONTEND_NONE;
  if (!context_append_expression(context, switch_record, &switch_index)) {
    return false;
  }

  bool switch_supported = subject_is_enum;
  bool patterns_supported = subject_is_enum;
  bool wildcard_seen = false;
  w_seed_span wildcard_span = empty_span(switch_cst->raw_span.start_byte);
  size_t arm_count = 0;
  /* Derive the join from arm results.  An outer expected type only supplies
   * nominal context for short enum values; it must not hide narrowing at the
   * statement boundary. */
  frontend_simple_type join_type = simple_type_unknown();
  bool have_join = false;
  w_seed_span join_span = empty_span(switch_cst->raw_span.start_byte);
  size_t join_document = context->module_index;
  uint32_t arm_cursor = switch_cst->first_child;
  uint32_t arm_node = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &arm_cursor, &arm_node) &&
         guard < doc->parse.node_count) {
    if (doc->nodes[arm_node].kind != W_SEED_CST_SWITCH_ARM) {
      guard += 1;
      continue;
    }
    const uint32_t pattern_node =
        first_direct_kind(doc, arm_node, W_SEED_CST_ENUM_PATTERN);
    const uint32_t wildcard_node =
        first_direct_kind(doc, arm_node, W_SEED_CST_WILDCARD_PATTERN);
    const uint32_t literal_node =
        first_direct_kind(doc, arm_node, W_SEED_CST_LITERAL_PATTERN);
    const uint32_t result_node = first_direct_expression(doc, arm_node);
    const w_seed_cst_node *pattern_cst = NULL;
    w_seed_frontend_switch_pattern_kind pattern_kind =
        W_SEED_FRONTEND_SWITCH_PATTERN_LITERAL;
    if (pattern_node != W_SEED_CST_NONE) {
      pattern_cst = &doc->nodes[pattern_node];
      pattern_kind = W_SEED_FRONTEND_SWITCH_PATTERN_ENUM_CASE;
    } else if (wildcard_node != W_SEED_CST_NONE) {
      pattern_cst = &doc->nodes[wildcard_node];
      pattern_kind = W_SEED_FRONTEND_SWITCH_PATTERN_WILDCARD;
    } else if (literal_node != W_SEED_CST_NONE) {
      pattern_cst = &doc->nodes[literal_node];
    }
    const w_seed_span pattern_span =
        pattern_cst == NULL ? owner_span(doc, arm_node) : pattern_cst->raw_span;
    uint32_t arm_enum = W_SEED_FRONTEND_NONE;
    uint32_t arm_case = W_SEED_FRONTEND_NONE;
    bool arm_supported = subject_is_enum && pattern_cst != NULL &&
                         result_node != W_SEED_CST_NONE;
    if (pattern_kind == W_SEED_FRONTEND_SWITCH_PATTERN_ENUM_CASE &&
        pattern_cst != NULL) {
      w_seed_frontend_text qualifier = {NULL, 0};
      w_seed_frontend_text case_name = {NULL, 0};
      if (!switch_pattern_names(doc, pattern_span, &qualifier, &case_name) ||
          !subject_is_enum ||
           (qualifier.length != 0 &&
           !text_equal_text(qualifier, subject_type.enum_name)) ||
          !enum_case_for_name(context, subject_enum, case_name, &arm_case,
                              NULL, NULL)) {
        arm_supported = false;
        switch_supported = false;
        patterns_supported = false;
        (void)context_append_fact(context,
                                  W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                  pattern_span, text_from_span(doc, pattern_span));
      } else {
        arm_enum = subject_enum;
        if (!enum_subset_contains_case(context, subject_type, arm_case)) {
          arm_supported = false;
          switch_supported = false;
          (void)append_match0002_diagnostic(
              context, pattern_span, subject_type.spelling, case_name,
              subject_type.spelling,
              subject_type.has_origin ? subject_type.origin_span : pattern_span,
              subject_type.has_origin ? subject_type.origin_document_index
                                      : context->module_index,
              doc->nodes[subject_node].raw_span);
        }
      }
    } else if (pattern_kind == W_SEED_FRONTEND_SWITCH_PATTERN_LITERAL) {
      arm_supported = false;
      switch_supported = false;
      patterns_supported = false;
      (void)context_append_fact(context,
                                W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                pattern_span, text_from_span(doc, pattern_span));
    }
    if (subject_is_enum) arm_enum = subject_enum;
    const bool arm_after_wildcard = wildcard_seen;
    if (arm_after_wildcard) {
      arm_supported = false;
      switch_supported = false;
      (void)append_match0002_diagnostic(
          context, pattern_span, text_from_span(doc, wildcard_span),
          text_from_span(doc, pattern_span), subject_type.spelling,
          wildcard_span, context->module_index,
          doc->nodes[subject_node].raw_span);
    }
    if (pattern_kind == W_SEED_FRONTEND_SWITCH_PATTERN_WILDCARD) {
      if (wildcard_seen && !arm_after_wildcard) {
        arm_supported = false;
        switch_supported = false;
        (void)append_match0002_diagnostic(
            context, pattern_span, text_from_span(doc, wildcard_span),
            text_from_span(doc, pattern_span), subject_type.spelling,
            wildcard_span, context->module_index,
            doc->nodes[subject_node].raw_span);
      }
      wildcard_seen = true;
      wildcard_span = pattern_span;
    }
    if (arm_enum != W_SEED_FRONTEND_NONE && arm_case != W_SEED_FRONTEND_NONE &&
        !arm_after_wildcard) {
      uint32_t prior_cursor = switch_cst->first_child;
      uint32_t prior_node = W_SEED_CST_NONE;
      size_t prior_guard = 0;
      while (next_child(doc, &prior_cursor, &prior_node) &&
             prior_guard < doc->parse.node_count && prior_node != arm_node) {
        if (doc->nodes[prior_node].kind == W_SEED_CST_SWITCH_ARM) {
          const uint32_t prior_pattern =
              first_direct_kind(doc, prior_node, W_SEED_CST_ENUM_PATTERN);
          if (prior_pattern != W_SEED_CST_NONE) {
            w_seed_frontend_text ignored_qualifier = {NULL, 0};
            w_seed_frontend_text prior_name = {NULL, 0};
            if (switch_pattern_names(doc, doc->nodes[prior_pattern].raw_span,
                                     &ignored_qualifier, &prior_name)) {
              uint32_t prior_case = W_SEED_FRONTEND_NONE;
              if (enum_case_for_name(context, subject_enum, prior_name,
                                     &prior_case, NULL, NULL) &&
                  prior_case == arm_case) {
                arm_supported = false;
                switch_supported = false;
                (void)append_match0002_diagnostic(
                    context, pattern_span, prior_name,
                    text_from_span(doc, pattern_span), subject_type.spelling,
                    doc->nodes[prior_pattern].raw_span,
                    context->module_index, doc->nodes[subject_node].raw_span);
                break;
              }
            }
          }
        }
        prior_guard += 1;
      }
    }

    frontend_simple_type arm_expected = frontend_type_is_enum(expected)
                                            ? expected
                                            : simple_type_unknown();
    frontend_simple_type arm_type = simple_type_unknown();
    uint32_t result_expression = W_SEED_FRONTEND_NONE;
    if (result_node != W_SEED_CST_NONE &&
        !normalize_expression_node(context, result_node, &result_expression,
                                   arm_expected, &arm_type, NULL)) {
      return false;
    }
    if (context->emit && context->output != NULL &&
        result_expression != W_SEED_FRONTEND_NONE &&
        result_expression < context->count.expressions &&
        !context->output->expressions[result_expression].supported) {
      arm_supported = false;
      switch_supported = false;
    }
    if (result_node == W_SEED_CST_NONE ||
        arm_type.kind == W_SEED_FRONTEND_TYPE_UNKNOWN) {
      arm_supported = false;
      switch_supported = false;
    } else if (!have_join) {
      join_type = arm_type;
      have_join = true;
      join_span = doc->nodes[result_node].raw_span;
      join_document = context->module_index;
    } else if (!frontend_widening_allowed(context, arm_type, join_type) &&
               !frontend_widening_allowed(context, join_type, arm_type)) {
      arm_supported = false;
      switch_supported = false;
      (void)append_type0120_diagnostic(
          context, doc->nodes[result_node].raw_span, arm_type,
          doc->nodes[result_node].raw_span, context->module_index, join_type,
          join_span, join_document);
    } else if (frontend_widening_allowed(context, join_type, arm_type)) {
      join_type = arm_type;
      join_span = doc->nodes[result_node].raw_span;
      join_document = context->module_index;
    } else if (frontend_widening_allowed(context, arm_type, join_type)) {
      /* Keep the existing representative and its source span. */
    }
    w_seed_frontend_switch_arm arm;
    (void)memset(&arm, 0, sizeof(arm));
    arm.module_index = (uint32_t)context->module_index;
    arm.owner_expression = switch_index;
    arm.pattern_kind = pattern_kind;
    arm.enum_index = arm_enum;
    arm.enum_case_index = arm_case;
    arm.pattern_span = pattern_span;
    arm.result_expression = result_expression;
    arm.span = doc->nodes[arm_node].raw_span;
    arm.supported = arm_supported;
    uint32_t arm_index = W_SEED_FRONTEND_NONE;
    if (!context_append_switch_arm(context, arm, &arm_index)) return false;
    arm_count += 1;
    guard += 1;
  }

  w_seed_frontend_text missing_cases[FRONTEND_MAX_DIAGNOSTIC_ITEMS];
  size_t missing_case_count = 0u;
  if (subject_is_enum && patterns_supported && !wildcard_seen) {
    uint32_t expected_cursor = subject_enum_doc->nodes[subject_enum_node].first_child;
    uint32_t expected_case_node = W_SEED_CST_NONE;
    size_t expected_guard = 0;
    while (next_child(subject_enum_doc, &expected_cursor, &expected_case_node) &&
           expected_guard < subject_enum_doc->parse.node_count) {
      if (subject_enum_doc->nodes[expected_case_node].kind ==
          W_SEED_CST_ENUM_CASE) {
        const w_seed_frontend_text expected_name = first_word_in_span(
            subject_enum_doc, subject_enum_doc->nodes[expected_case_node].raw_span);
        uint32_t expected_case_index = W_SEED_FRONTEND_NONE;
        if (!enum_case_for_name(context, subject_enum, expected_name,
                                &expected_case_index, NULL, NULL) ||
            !enum_subset_contains_case(context, subject_type,
                                       expected_case_index)) {
          expected_guard += 1;
          continue;
        }
        bool covered = false;
        uint32_t scan_cursor = switch_cst->first_child;
        uint32_t scan_arm = W_SEED_CST_NONE;
        size_t scan_guard = 0;
        while (next_child(doc, &scan_cursor, &scan_arm) &&
               scan_guard < doc->parse.node_count) {
          if (doc->nodes[scan_arm].kind == W_SEED_CST_SWITCH_ARM) {
            const uint32_t scan_pattern =
                first_direct_kind(doc, scan_arm, W_SEED_CST_ENUM_PATTERN);
            if (scan_pattern != W_SEED_CST_NONE) {
              w_seed_frontend_text ignored_qualifier = {NULL, 0};
              w_seed_frontend_text scan_name = {NULL, 0};
              if (switch_pattern_names(doc, doc->nodes[scan_pattern].raw_span,
                                       &ignored_qualifier, &scan_name) &&
                  text_equal_text(scan_name, expected_name)) {
                covered = true;
                break;
              }
            }
          }
          scan_guard += 1;
        }
        if (!covered) {
          switch_supported = false;
          if (missing_case_count <
              sizeof(missing_cases) / sizeof(missing_cases[0])) {
            missing_cases[missing_case_count] = expected_name;
            missing_case_count += 1u;
          }
        }
      }
      expected_guard += 1;
    }
  }
  if (missing_case_count != 0u) {
    (void)append_match0001_diagnostic(
        context, switch_cst->raw_span, subject_type.spelling,
        doc->nodes[subject_node].raw_span, missing_cases, missing_case_count);
  }
  if (!subject_is_enum) {
    (void)context_append_fact(context, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE,
                              switch_cst->raw_span,
                              text_from_span(doc, switch_cst->raw_span));
  }
  if (context->emit && context->output != NULL &&
      switch_index < context->output->expression_capacity) {
    w_seed_frontend_expression *record =
        &context->output->expressions[switch_index];
    record->switch_arm_count = (uint32_t)arm_count;
    record->inferred_type = W_SEED_FRONTEND_NONE;
    if (have_join) {
      frontend_simple_type inferred = join_type;
      if (expected.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
          frontend_widening_allowed(context, join_type, expected)) {
        inferred = expected;
      }
      (void)output_type_index_for_simple(context, inferred,
                                         &record->inferred_type);
    }
    record->supported = switch_supported && have_join;
  }
  if (actual_out != NULL) *actual_out = have_join ? join_type : simple_type_unknown();
  if (root_out != NULL) {
    (void)memset(root_out, 0, sizeof(*root_out));
    root_out->index = switch_index;
    root_out->left = subject_expression;
    root_out->right = W_SEED_FRONTEND_NONE;
    root_out->kind = W_SEED_FRONTEND_EXPR_SWITCH;
    root_out->type = have_join ? join_type : simple_type_unknown();
    root_out->supported = switch_supported && have_join;
    root_out->span = switch_cst->raw_span;
  }
  *expression_index = switch_index;
  return true;
}

static bool normalize_statement_depth(frontend_context *context,
                                      uint32_t node_index,
                                      uint32_t *statement_index,
                                      size_t depth) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || statement_index == NULL ||
      depth >= W_SEED_FRONTEND_MAX_NESTING) {
    return false;
  }
  const w_seed_cst_node *node = &doc->nodes[node_index];
  w_seed_frontend_statement value;
  (void)memset(&value, 0, sizeof(value));
  value.module_index = (uint32_t)context->module_index;
  value.owner_function = context->function_index;
  value.span = node->raw_span;
  value.expression_index = W_SEED_FRONTEND_NONE;
  value.condition_expression = W_SEED_FRONTEND_NONE;
  value.first_child = W_SEED_FRONTEND_NONE;
  value.child_count = 0;
  value.binding_name = (w_seed_frontend_text){NULL, 0};
  value.declared_type = W_SEED_FRONTEND_NONE;
  value.next_sibling = W_SEED_FRONTEND_NONE;
  value.else_child = W_SEED_FRONTEND_NONE;
  value.range_lower_expression = W_SEED_FRONTEND_NONE;
  value.range_upper_expression = W_SEED_FRONTEND_NONE;
  value.loop_local_ordinal = W_SEED_FRONTEND_NONE;
  switch (node->kind) {
    case W_SEED_CST_LET_STATEMENT:
      value.kind = W_SEED_FRONTEND_STMT_LET;
      value.binding_name = binding_name_after_keyword(doc, node->raw_span, "let");
      break;
    case W_SEED_CST_VAR_STATEMENT:
      value.kind = W_SEED_FRONTEND_STMT_VAR;
      value.binding_name = binding_name_after_keyword(doc, node->raw_span, "var");
      break;
    case W_SEED_CST_RETURN_STATEMENT:
      value.kind = W_SEED_FRONTEND_STMT_RETURN;
      break;
    case W_SEED_CST_IF_STATEMENT:
      value.kind = W_SEED_FRONTEND_STMT_IF;
      break;
    case W_SEED_CST_GUARD_STATEMENT:
      value.kind = W_SEED_FRONTEND_STMT_GUARD;
      break;
    case W_SEED_CST_FOR_STATEMENT:
      value.kind = W_SEED_FRONTEND_STMT_FOR;
      value.binding_name = binding_name_after_keyword(doc, node->raw_span, "for");
      value.loop_local_ordinal = loop_ordinal_for_cst_node(context, node_index);
      if (value.loop_local_ordinal == W_SEED_FRONTEND_NONE) {
        value.kind = W_SEED_FRONTEND_STMT_UNSUPPORTED;
      }
      break;
    case W_SEED_CST_EXPECT_STATEMENT:
      value.kind = W_SEED_FRONTEND_STMT_EXPECT;
      break;
    case W_SEED_CST_EXPRESSION_STATEMENT:
      value.kind = W_SEED_FRONTEND_STMT_EXPRESSION;
      break;
    default:
      value.kind = W_SEED_FRONTEND_STMT_UNSUPPORTED;
      break;
  }
  const uint32_t type_node = direct_type_index(doc, node_index);
  if (type_node != W_SEED_CST_NONE &&
      !normalize_type_tree(context, type_node, &value.declared_type)) {
    return false;
  }
  const uint32_t expression_node = first_direct_expression(doc, node_index);
  frontend_simple_type expected_outer = simple_type_unknown();
  if ((node->kind == W_SEED_CST_LET_STATEMENT ||
       node->kind == W_SEED_CST_VAR_STATEMENT) &&
      type_node != W_SEED_CST_NONE) {
    expected_outer = contextual_type_from_span(context, doc,
                                               doc->nodes[type_node].raw_span);
  } else if (node->kind == W_SEED_CST_RETURN_STATEMENT &&
             context->function_node != NULL) {
    expected_outer = function_return_type(
        context, doc, (uint32_t)(context->function_node - doc->nodes));
  } else if (node->kind == W_SEED_CST_FOR_STATEMENT) {
    expected_outer = simple_type_from_view(
        (w_seed_frontend_text){"Range<usize>", 12});
  }
  frontend_simple_type normalized_actual = simple_type_unknown();
  frontend_expr_value expression_value;
  (void)memset(&expression_value, 0, sizeof(expression_value));
  if (expression_node != W_SEED_CST_NONE &&
      !normalize_expression_node(context, expression_node,
                                 &value.expression_index, expected_outer,
                                 &normalized_actual, &expression_value)) {
    return false;
  }
  if (expression_node != W_SEED_CST_NONE &&
      normalized_actual.kind == W_SEED_FRONTEND_TYPE_UNKNOWN) {
    normalized_actual = infer_expression_span(
        context, doc->nodes[expression_node].raw_span);
  }
  if ((node->kind == W_SEED_CST_LET_STATEMENT ||
       node->kind == W_SEED_CST_VAR_STATEMENT) &&
      expression_node != W_SEED_CST_NONE && type_node != W_SEED_CST_NONE) {
    const frontend_simple_type actual = normalized_actual;
    const frontend_simple_type expected = expected_outer;
    if (actual.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        expected.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        !frontend_widening_allowed(context, actual, expected)) {
      const bool narrowing = actual.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                             expected.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                             actual.is_signed == expected.is_signed &&
                             actual.bit_width > expected.bit_width;
      if (narrowing) {
        (void)append_type0122_diagnostic(
            context, doc->nodes[expression_node].raw_span, actual, expected,
            (w_seed_frontend_text){"integer narrowing is not implicit",
                                   sizeof("integer narrowing is not implicit") - 1u});
      } else {
        (void)append_sem0001_diagnostic(
            context, doc->nodes[expression_node].raw_span, actual.spelling,
            expected.spelling);
      }
      value.kind = W_SEED_FRONTEND_STMT_UNSUPPORTED;
      (void)context_append_fact(context,
                                W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                node->raw_span, text_from_span(doc, node->raw_span));
    }
  }
  if (node->kind == W_SEED_CST_IF_STATEMENT) {
    value.condition_expression = value.expression_index;
    const frontend_simple_type condition = normalized_actual;
    if (condition.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        !type_is_bool(condition)) {
      (void)append_sem0001_diagnostic(
          context, doc->nodes[expression_node].raw_span,
          text_from_span(doc, trim_span(doc, doc->nodes[expression_node].raw_span)),
          (w_seed_frontend_text){"Bool", 4});
    }
  }
  if (node->kind == W_SEED_CST_GUARD_STATEMENT) {
    value.condition_expression = value.expression_index;
    const frontend_simple_type condition = normalized_actual;
    if (condition.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        !type_is_bool(condition)) {
      (void)append_sem0001_diagnostic(
          context, doc->nodes[expression_node].raw_span,
          text_from_span(doc, trim_span(doc, doc->nodes[expression_node].raw_span)),
          (w_seed_frontend_text){"Bool", 4});
    }
  }
  if (node->kind == W_SEED_CST_FOR_STATEMENT &&
      value.expression_index != W_SEED_FRONTEND_NONE) {
    const bool range_supported =
        expression_value.kind == W_SEED_FRONTEND_EXPR_RANGE &&
        expression_value.supported &&
        expression_value.left < (size_t)UINT32_MAX &&
        expression_value.right < (size_t)UINT32_MAX;
    if (!range_supported) {
      value.kind = W_SEED_FRONTEND_STMT_UNSUPPORTED;
    } else {
      value.range_lower_expression = (uint32_t)expression_value.left;
      value.range_upper_expression = (uint32_t)expression_value.right;
    }
  }
  if (node->kind == W_SEED_CST_RETURN_STATEMENT && expression_node != W_SEED_CST_NONE) {
    frontend_simple_type actual = normalized_actual;
    frontend_simple_type expected = expected_outer;
    if (actual.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        expected.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        !frontend_widening_allowed(context, actual, expected)) {
      const bool narrowing = actual.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                             expected.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                             actual.is_signed == expected.is_signed &&
                             actual.bit_width > expected.bit_width;
      if (narrowing) {
        (void)append_type0122_diagnostic(
            context, doc->nodes[expression_node].raw_span, actual, expected,
            (w_seed_frontend_text){"integer narrowing is not implicit",
                                   sizeof("integer narrowing is not implicit") - 1u});
      } else {
        (void)append_sem0001_diagnostic(
            context, doc->nodes[expression_node].raw_span, actual.spelling,
            expected.spelling);
      }
    }
  }
  if (node->kind == W_SEED_CST_RETURN_STATEMENT &&
      expression_node == W_SEED_CST_NONE && context->function_node != NULL) {
    const frontend_simple_type expected = function_return_type(
        context, doc, (uint32_t)(context->function_node - doc->nodes));
    if (expected.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        expected.kind != W_SEED_FRONTEND_TYPE_UNIT) {
      (void)append_sem0001_diagnostic(
          context, node->raw_span, (w_seed_frontend_text){"()", 2},
          expected.spelling);
      value.kind = W_SEED_FRONTEND_STMT_UNSUPPORTED;
      (void)context_append_fact(context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
                                node->raw_span, text_from_span(doc, node->raw_span));
    }
  }
  if (!context_append_statement(context, value, statement_index)) return false;
  if ((node->kind == W_SEED_CST_LET_STATEMENT ||
       node->kind == W_SEED_CST_VAR_STATEMENT) && value.binding_name.length != 0) {
    uint32_t symbol_index = W_SEED_FRONTEND_NONE;
    if (!normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_BINDING,
                          *statement_index, value.binding_name, false,
                          value.span, value.declared_type, &symbol_index)) {
      return false;
    }
  }
  if (node->kind == W_SEED_CST_IF_STATEMENT ||
      node->kind == W_SEED_CST_GUARD_STATEMENT ||
      node->kind == W_SEED_CST_FOR_STATEMENT) {
    uint32_t child_cursor = node->first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t guard = 0;
    uint32_t then_first = W_SEED_FRONTEND_NONE;
    uint32_t then_count = 0u;
    bool saw_then_block = false;
    while (next_child(doc, &child_cursor, &child) &&
           guard < doc->parse.node_count) {
      if (doc->nodes[child].kind == W_SEED_CST_BLOCK) {
        uint32_t nested_first = W_SEED_FRONTEND_NONE;
        uint32_t nested_count = 0u;
        if (!normalize_block_statements_depth(
                context, child, depth + 1u, &nested_first, &nested_count)) {
          return false;
        }
        if (node->kind == W_SEED_CST_GUARD_STATEMENT && !saw_then_block) {
          /* guard has only an else block. */
          if (context->emit && context->output != NULL &&
              *statement_index < context->output->statement_capacity) {
            context->output->statements[*statement_index].else_child =
                nested_first;
          }
        } else if (!saw_then_block) {
          then_first = nested_first;
          then_count = nested_count;
          saw_then_block = true;
        }
      } else if (doc->nodes[child].kind == W_SEED_CST_IF_STATEMENT) {
        uint32_t nested_statement = W_SEED_FRONTEND_NONE;
        if (!normalize_statement_depth(context, child, &nested_statement,
                                       depth + 1u)) {
          return false;
        }
        if (context->emit && context->output != NULL &&
            *statement_index < context->output->statement_capacity) {
          context->output->statements[*statement_index].else_child =
              nested_statement;
        }
      } else if (kind_is_statement(doc->nodes[child].kind)) {
        uint32_t nested_statement = W_SEED_FRONTEND_NONE;
        if (!normalize_statement_depth(context, child, &nested_statement,
                                       depth + 1u)) {
          return false;
        }
        if (node->kind == W_SEED_CST_GUARD_STATEMENT &&
            context->emit && context->output != NULL &&
            *statement_index < context->output->statement_capacity) {
          context->output->statements[*statement_index].else_child =
              nested_statement;
        }
      }
      guard += 1u;
    }
    if (context->emit && context->output != NULL &&
        *statement_index < context->output->statement_capacity) {
      w_seed_frontend_statement *record =
          &context->output->statements[*statement_index];
      if (node->kind != W_SEED_CST_GUARD_STATEMENT) {
        record->first_child = then_first;
        record->child_count = then_count;
      }
    }
  }
  if (value.kind == W_SEED_FRONTEND_STMT_UNSUPPORTED) {
    (void)context_append_fact(context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
                              value.span, text_from_span(doc, value.span));
  }
  return true;
}

static bool normalize_block_statements_depth(frontend_context *context,
                                             uint32_t block_node,
                                             size_t depth,
                                             uint32_t *first_statement,
                                             uint32_t *statement_count) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || block_node >= doc->parse.node_count ||
      depth >= W_SEED_FRONTEND_MAX_NESTING) {
    return false;
  }
  if (first_statement != NULL) *first_statement = W_SEED_FRONTEND_NONE;
  if (statement_count != NULL) *statement_count = 0u;
  uint32_t child_cursor = doc->nodes[block_node].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  uint32_t previous = W_SEED_FRONTEND_NONE;
  uint32_t direct_count = 0u;
  while (next_child(doc, &child_cursor, &child) &&
         guard < doc->parse.node_count) {
    const w_seed_cst_kind kind = doc->nodes[child].kind;
    if (kind_is_statement(kind)) {
      uint32_t statement_index = W_SEED_FRONTEND_NONE;
      if (!normalize_statement_depth(context, child, &statement_index,
                                     depth + 1u)) {
        return false;
      }
      if (first_statement != NULL && *first_statement == W_SEED_FRONTEND_NONE)
        *first_statement = statement_index;
      if (context->emit && context->output != NULL &&
          previous != W_SEED_FRONTEND_NONE &&
          previous < context->output->statement_capacity) {
        context->output->statements[previous].next_sibling = statement_index;
      }
      previous = statement_index;
      direct_count += 1u;
    } else if (!node_is_raw(&doc->nodes[child]) &&
               kind != W_SEED_CST_BLOCK && kind != W_SEED_CST_EXPRESSION) {
      if (kind_is_unsupported_owner(kind)) {
        (void)context_append_fact(context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
                                  doc->nodes[child].raw_span,
                                  text_from_span(doc, doc->nodes[child].raw_span));
      }
    }
    guard += 1;
  }
  if (statement_count != NULL) {
    *statement_count = (uint32_t)direct_count;
  }
  return true;
}

static bool normalize_block_statements(frontend_context *context,
                                       uint32_t block_node) {
  return normalize_block_statements_depth(context, block_node, 0u, NULL, NULL);
}

static bool module_const_expression_kind_allowed(
    w_seed_frontend_expr_kind kind) {
  return kind == W_SEED_FRONTEND_EXPR_BOOL ||
         kind == W_SEED_FRONTEND_EXPR_INTEGER ||
         kind == W_SEED_FRONTEND_EXPR_IDENTIFIER ||
         kind == W_SEED_FRONTEND_EXPR_PARENTHESIS ||
         kind == W_SEED_FRONTEND_EXPR_UNARY ||
         kind == W_SEED_FRONTEND_EXPR_BINARY;
}

static bool unresolved_parenthesized_identifier(
    const frontend_context *context, w_seed_span span,
    w_seed_frontend_text *name_out) {
  const w_seed_frontend_document *doc = context_document(context);
  if (name_out != NULL) *name_out = (w_seed_frontend_text){NULL, 0};
  if (context == NULL || doc == NULL) return false;
  frontend_token tokens[W_SEED_FRONTEND_MAX_NESTING * 2u];
  size_t token_count = 0u;
  if (!const_tokens_for_span(doc, trim_span(doc, span), tokens,
                             sizeof(tokens) / sizeof(tokens[0]), &token_count) ||
      token_count < 3u) {
    return false;
  }
  size_t open_count = 0u;
  while (open_count < token_count &&
         token_text(doc, &tokens[open_count], "("))
    open_count += 1u;
  if (open_count == 0u || open_count >= token_count ||
      tokens[open_count].kind != W_SEED_CST_WORD)
    return false;
  size_t close_count = open_count + 1u;
  while (close_count < token_count &&
         token_text(doc, &tokens[close_count], ")"))
    close_count += 1u;
  if (close_count != token_count ||
      token_count != open_count * 2u + 1u)
    return false;
  const w_seed_frontend_text name =
      text_from_span(doc, tokens[open_count].span);
  w_seed_frontend_import_target_kind imported_kind =
      W_SEED_FRONTEND_IMPORT_UNRESOLVED;
  uint32_t imported_index = W_SEED_FRONTEND_NONE;
  w_seed_frontend_text imported_target = {NULL, 0};
  if (imported_target_for_name(context, name, &imported_kind, &imported_index,
                               &imported_target))
    return false;
  if (name_out != NULL) *name_out = name;
  return true;
}

static bool module_const_name_is_duplicate(
    const frontend_context *context, w_seed_frontend_text name) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || name.length == 0u) return false;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t matches = 0u;
  size_t guard = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_CONST_DECLARATION &&
        text_equal_text(name_after_keyword(doc, doc->nodes[child].raw_span,
                                           "const"),
                        name)) {
      matches += 1u;
      if (matches > 1u) return true;
    }
    guard += 1u;
  }
  return false;
}

static bool module_const_name_is_untyped(
    const frontend_context *context, w_seed_frontend_text name) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || name.length == 0u) return false;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_CONST_DECLARATION &&
        text_equal_text(name_after_keyword(doc, doc->nodes[child].raw_span,
                                           "const"),
                        name)) {
      return direct_type_index(doc, child) == W_SEED_CST_NONE;
    }
    guard += 1u;
  }
  return false;
}

static bool module_const_record_write(frontend_context *context,
                                      uint32_t index,
                                      const w_seed_frontend_const_declaration *value) {
  if (context == NULL || value == NULL) return false;
  if (!context->emit) return true;
  if (context->output == NULL) return false;
  if (context->output->const_declarations == NULL ||
      (size_t)index >= context->output->const_declaration_capacity)
    return false;
  context->output->const_declarations[index] = *value;
  return true;
}

static bool normalize_module_const(frontend_context *context,
                                   uint32_t node_index,
                                   uint32_t const_index) {
  const w_seed_frontend_document *doc = context_document(context);
  if (context == NULL || doc == NULL || node_index >= doc->parse.node_count)
    return false;
  const w_seed_cst_node *node = &doc->nodes[node_index];
  const uint32_t type_node = direct_type_index(doc, node_index);
  const bool explicit_type = type_node != W_SEED_CST_NONE;
  uint32_t declared_type = W_SEED_FRONTEND_NONE;
  const uint32_t expression_node = first_direct_expression(doc, node_index);
  w_seed_frontend_const_declaration value;
  (void)memset(&value, 0, sizeof(value));
  value.module_index = (uint32_t)context->module_index;
  value.name = name_after_keyword(doc, node->raw_span, "const");
  value.exported = span_has_keyword(doc, node->raw_span, "export");
  value.span = node->raw_span;
  value.body_span = expression_node == W_SEED_CST_NONE
                        ? empty_span(node->raw_span.end_byte)
                        : doc->nodes[expression_node].raw_span;
  value.declared_type = declared_type;
  value.initializer_expression = W_SEED_FRONTEND_NONE;
  value.symbol_index = W_SEED_FRONTEND_NONE;
  value.has_explicit_type = explicit_type;
  value.lowerable = false;
  value.effective_type = explicit_type ? declared_type : W_SEED_FRONTEND_NONE;
  if (const_index < W_SEED_FRONTEND_MAX_CONST_DECLARATIONS) {
    value.declared_type = context->const_declared_type_indices[const_index];
    declared_type = value.declared_type;
    if (context->const_inferred_type_indices[const_index] !=
        W_SEED_FRONTEND_NONE)
      value.effective_type =
          context->const_inferred_type_indices[const_index];
    if (explicit_type) value.effective_type = value.declared_type;
  }
  if (context->emit && context->output != NULL) {
    w_seed_frontend_const_declaration *records =
        context->output->const_declarations;
    if (records == NULL || (size_t)const_index >= context->count.const_declarations)
      return false;
    value = records[const_index];
    declared_type = value.declared_type;
  }

  frontend_simple_type expected = explicit_type
                                      ? contextual_type_from_span(
                                            context, doc,
                                            doc->nodes[type_node].raw_span)
                                      : simple_type_unknown();
  if (!explicit_type && context->const_inference_complete &&
      const_index < W_SEED_FRONTEND_MAX_CONST_DECLARATIONS)
    expected = const_inferred_simple_type(
        &context->const_inferred_types[const_index]);
  if (!explicit_type && context->emit && context->output != NULL &&
      context->output->const_declarations != NULL &&
      (size_t)const_index < context->count.const_declarations) {
    const uint32_t effective =
        context->output->const_declarations[const_index].effective_type;
    if (effective != W_SEED_FRONTEND_NONE &&
        (size_t)effective < context->count.types)
      expected = simple_type_from_frontend_type(&context->output->types[effective]);
  }
  frontend_simple_type actual = simple_type_unknown();
  frontend_expr_value expression_value;
  (void)memset(&expression_value, 0, sizeof(expression_value));
  expression_value.index = W_SEED_FRONTEND_NONE;
  const uint32_t saved_function = context->function_index;
  const w_seed_cst_node *saved_function_node = context->function_node;
  const bool saved_const = context->current_function_is_const;
  const bool saved_active = context->current_const_body_active;
  const uint32_t saved_const_index = context->current_module_const;
  context->function_index = W_SEED_FRONTEND_NONE;
  context->function_node = NULL;
  context->current_function_is_const = false;
  context->current_const_body_active = false;
  context->current_module_const = const_index;
  bool normalized = expression_node != W_SEED_CST_NONE &&
                    normalize_expression_node(
                        context, expression_node, &value.initializer_expression,
                        expected, &actual, &expression_value);
  context->function_index = saved_function;
  context->function_node = saved_function_node;
  context->current_function_is_const = saved_const;
  context->current_const_body_active = saved_active;
  context->current_module_const = saved_const_index;
  /* Preserve the declaration-owned scalar type index on the initializer root.
   * The type arena interns equivalent scalar records, so expression parsing
   * can otherwise retain an earlier const's index and break the source
   * declaration relation. */
  if (context->emit && context->output != NULL &&
      value.initializer_expression != W_SEED_FRONTEND_NONE &&
      (size_t)value.initializer_expression <
          context->output->expression_capacity &&
      value.effective_type != W_SEED_FRONTEND_NONE &&
      (size_t)value.effective_type < context->count.types) {
    context->output->expressions[value.initializer_expression].inferred_type =
        value.effective_type;
  }
  if (!normalized || expression_value.index == W_SEED_FRONTEND_NONE) {
    (void)context_append_fact(
        context, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION, value.body_span,
        text_from_span(doc, value.body_span));
  }
  const bool scalar_type =
      expected.kind == W_SEED_FRONTEND_TYPE_BOOL ||
      (expected.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
       expected.bit_width != 0u);
  w_seed_frontend_import_target_kind imported_kind =
      W_SEED_FRONTEND_IMPORT_UNRESOLVED;
  uint32_t imported_index = W_SEED_FRONTEND_NONE;
  w_seed_frontend_text imported_target = {NULL, 0};
  const bool unresolved_local =
      normalized &&
      expression_value.kind == W_SEED_FRONTEND_EXPR_IDENTIFIER &&
      expression_value.has_name && !expression_value.supported &&
      !module_const_name_is_duplicate(context, expression_value.name) &&
       !imported_target_for_name(context, expression_value.name, &imported_kind,
                                 &imported_index, &imported_target);
  if (unresolved_local) {
    (void)append_sem0001_diagnostic(context, value.body_span,
                                    expression_value.name, expected.spelling);
  }
  if (scalar_type &&
      normalized && expression_value.supported &&
      module_const_expression_kind_allowed(expression_value.kind) &&
      actual.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
      expected.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
      !unresolved_local && !frontend_widening_allowed(context, actual, expected)) {
    const bool narrowing =
        actual.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
        expected.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
        actual.is_signed == expected.is_signed &&
        actual.bit_width > expected.bit_width;
    if (narrowing) {
      (void)append_type0122_diagnostic(
          context, value.body_span, actual, expected,
          (w_seed_frontend_text){"integer narrowing is not implicit",
                                 sizeof("integer narrowing is not implicit") - 1u});
    } else {
      (void)append_sem0001_diagnostic(context, value.body_span,
                                      actual.spelling, expected.spelling);
    }
  }
  value.lowerable = scalar_type && normalized &&
                    expression_value.supported &&
                    module_const_expression_kind_allowed(expression_value.kind) &&
                    actual.kind == expected.kind &&
                    (expected.kind != W_SEED_FRONTEND_TYPE_INTEGER ||
                     (actual.is_signed == expected.is_signed &&
                      actual.bit_width == expected.bit_width));
  if (explicit_type && !scalar_type) {
    (void)context_append_fact(context, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE,
                              doc->nodes[type_node].raw_span,
                              text_from_span(doc, doc->nodes[type_node].raw_span));
  } else if (normalized && !expression_value.supported) {
    (void)context_append_fact(
        context, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION, value.body_span,
        text_from_span(doc, value.body_span));
  } else if (normalized &&
             !module_const_expression_kind_allowed(expression_value.kind)) {
    (void)context_append_fact(
        context, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION, value.body_span,
        text_from_span(doc, value.body_span));
  }
  if (!module_const_record_write(context, const_index, &value)) return false;
  /* Const records are emitted after normalization has assigned the exact
   * declared/effective indices.  Size the dry receipt from that same value so
   * NONE placeholders cannot change the decimal width of the preflight. */
  if (!context->emit && !receipt_size_const_declaration(context, &value))
    return false;
  return true;
}

static bool normalize_document(frontend_context *context) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return false;
  w_seed_frontend_module module;
  (void)memset(&module, 0, sizeof(module));
  module.source_id = doc->logical_source_id;
  module.module_id = doc->module_id;
  module.local_module_name = document_local_module_name(doc);
  module.span = doc->nodes[doc->parse.root].raw_span;
  module.first_import = (uint32_t)context->count.imports;
  module.first_struct = (uint32_t)context->count.structs;
  module.first_type_declaration = (uint32_t)context->count.type_declarations;
  module.first_alias = (uint32_t)context->count.aliases;
  module.first_function = (uint32_t)context->count.functions;
  module.first_entry = (uint32_t)context->count.entries;
  module.first_enum = (uint32_t)context->count.enums;
  module.first_const_declaration = (uint32_t)context->count.const_declarations;
  module.import_count = 0;
  module.struct_count = 0;
  module.type_declaration_count = 0;
  module.alias_count = 0;
  module.function_count = 0;
  module.entry_count = 0;
  module.enum_count = 0;
  module.const_declaration_count = 0;
  uint32_t module_index = W_SEED_FRONTEND_NONE;
  if (!append_module(context, module, &module_index)) return false;
  context->count.modules += 1;
  uint32_t module_symbol = W_SEED_FRONTEND_NONE;
  if (!normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_MODULE, module_index,
                        module.module_id, true, module.span,
                        W_SEED_FRONTEND_NONE, &module_symbol)) {
    return false;
  }

  /* Publish all declaration records before lowering any initializer.  This
   * gives every same-module initializer deterministic forward-reference
   * targets while keeping the physical record order source-backed. */
  uint32_t const_cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t const_node = W_SEED_CST_NONE;
  size_t const_guard = 0u;
  while (next_child(doc, &const_cursor, &const_node) &&
         const_guard < doc->parse.node_count) {
    if (doc->nodes[const_node].kind == W_SEED_CST_CONST_DECLARATION) {
      const uint32_t type_node = direct_type_index(doc, const_node);
      uint32_t declared_type = W_SEED_FRONTEND_NONE;
      if (type_node != W_SEED_CST_NONE &&
          !normalize_type_tree(context, type_node, &declared_type))
        return false;
      w_seed_frontend_const_declaration value;
      (void)memset(&value, 0, sizeof(value));
      value.module_index = module_index;
      value.name = name_after_keyword(doc, doc->nodes[const_node].raw_span,
                                      "const");
      value.exported =
          span_has_keyword(doc, doc->nodes[const_node].raw_span, "export");
      value.span = doc->nodes[const_node].raw_span;
      const uint32_t body_node = first_direct_expression(doc, const_node);
      value.body_span = body_node == W_SEED_CST_NONE
                            ? empty_span(value.span.end_byte)
                            : doc->nodes[body_node].raw_span;
      value.declared_type = declared_type;
      value.initializer_expression = W_SEED_FRONTEND_NONE;
      value.symbol_index = W_SEED_FRONTEND_NONE;
      value.has_explicit_type = type_node != W_SEED_CST_NONE;
      value.lowerable = false;
      value.effective_type = value.has_explicit_type
                                 ? declared_type
                                 : W_SEED_FRONTEND_NONE;
      uint32_t const_index = W_SEED_FRONTEND_NONE;
      if (!context_append_const_declaration(context, value, &const_index))
        return false;
      if (const_index < W_SEED_FRONTEND_MAX_CONST_DECLARATIONS) {
        context->const_declared_type_indices[const_index] = declared_type;
        context->const_inferred_type_indices[const_index] = declared_type;
      }
      uint32_t symbol_index = W_SEED_FRONTEND_NONE;
      if (!normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_CONST,
                            const_index, value.name, value.exported, value.span,
                            value.declared_type, &symbol_index))
        return false;
      value.symbol_index = symbol_index;
      if (context->emit && context->output != NULL) {
        w_seed_frontend_const_declaration *records =
            context->output->const_declarations;
        if (records == NULL || (size_t)const_index >= context->count.const_declarations)
          return false;
        records[const_index].symbol_index = symbol_index;
      }
      module.const_declaration_count += 1u;
    }
    const_guard += 1u;
  }
  /* D7 solves the local const graph only after every declaration and symbol is
   * published.  Initializers are normalized below with the resulting scalar
   * context; no ConstIR or value evaluation occurs in this phase. */
  if (!infer_module_const_types(context)) return false;
  uint32_t child_cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &child_cursor, &child) &&
         guard < doc->parse.node_count) {
    uint32_t item_index = W_SEED_FRONTEND_NONE;
    bool recognized = true;
    switch (doc->nodes[child].kind) {
      case W_SEED_CST_IMPORT:
        if (!normalize_import(context, child, &item_index)) return false;
        module.import_count += 1;
        break;
      case W_SEED_CST_STRUCT:
        if (!normalize_struct(context, child, &item_index)) return false;
        module.struct_count += 1;
        break;
      case W_SEED_CST_ENUM:
        if (!normalize_enum(context, child, &item_index)) return false;
        module.enum_count += 1;
        break;
      case W_SEED_CST_TYPE_DECLARATION:
        if (!normalize_type_declaration(context, child, false, &item_index)) {
          return false;
        }
        module.type_declaration_count += 1;
        break;
      case W_SEED_CST_ALIAS_DECLARATION:
        if (!normalize_type_declaration(context, child, true, &item_index)) {
          return false;
        }
        module.alias_count += 1;
        break;
      case W_SEED_CST_FUNCTION:
        if (!normalize_function(context, child, &item_index)) return false;
        module.function_count += 1;
        break;
      case W_SEED_CST_CONST_DECLARATION: {
        uint32_t const_index = W_SEED_FRONTEND_NONE;
        if (!module_const_index_for_node(context, context->module_index, child,
                                         &const_index) ||
            !normalize_module_const(context, child, const_index))
          return false;
        break;
      }
      case W_SEED_CST_ENTRY:
        if (!normalize_entry(context, child, &item_index)) return false;
        module.entry_count += 1;
        break;
      case W_SEED_CST_TEST:
        recognized = false;
        (void)context_append_fact(context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
                                  doc->nodes[child].raw_span,
                                  text_from_span(doc, doc->nodes[child].raw_span));
        break;
      case W_SEED_CST_MODULE_HEADER:
      case W_SEED_CST_SOURCE_PREFIX:
      case W_SEED_CST_TRIVIA:
        break;
      default:
        if (!node_is_raw(&doc->nodes[child])) {
          recognized = false;
          (void)context_append_fact(
              context, W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE,
              doc->nodes[child].raw_span,
              text_from_span(doc, doc->nodes[child].raw_span));
        }
        break;
    }
    (void)recognized;
    guard += 1;
  }
  if (context->emit && context->output != NULL &&
      context->module_index < context->output->module_capacity) {
    context->output->modules[context->module_index] = module;
  }
  return true;
}

/* Resolve append-only frontend facts after every declaration is present in the
 * output. Downstream consumers must not repeat this name-based resolution. */
static bool resolve_frontend_links(frontend_context *context) {
  if (context == NULL || context->output == NULL ||
      (context->count.expressions != 0u &&
       context->output->expressions == NULL) ||
      (context->count.functions != 0u && context->output->functions == NULL) ||
      (context->count.parameters != 0u &&
       context->output->parameters == NULL) ||
      (context->count.arguments != 0u && context->output->arguments == NULL))
    return false;
  for (size_t expression_index = 0;
       expression_index < context->count.expressions; expression_index += 1u) {
    w_seed_frontend_expression *expression =
        &context->output->expressions[expression_index];
    if ((size_t)expression->module_index >= context->input.document_count)
      return false;
    context->module_index = expression->module_index;
    if (expression->owner_function == W_SEED_FRONTEND_NONE) continue;
    if ((size_t)expression->owner_function >= context->count.functions)
      return false;
    const w_seed_frontend_function *owner =
        &context->output->functions[expression->owner_function];
    if (expression->kind == W_SEED_FRONTEND_EXPR_IDENTIFIER) {
      for (uint32_t ordinal = 0; ordinal < owner->parameter_count;
           ordinal += 1u) {
        const size_t parameter_index =
            (size_t)owner->first_parameter + ordinal;
        if (parameter_index >= context->count.parameters) return false;
        const w_seed_frontend_parameter *parameter =
            &context->output->parameters[parameter_index];
        if (text_equal_text(parameter->name, expression->spelling)) {
          expression->resolved_parameter_ordinal = ordinal;
          break;
        }
      }
    }
    if (expression->kind != W_SEED_FRONTEND_EXPR_CALL ||
        expression->left == W_SEED_FRONTEND_NONE ||
        (size_t)expression->left >= context->count.expressions)
      continue;
    w_seed_frontend_expression *callee =
        &context->output->expressions[expression->left];
    if (callee->kind != W_SEED_FRONTEND_EXPR_IDENTIFIER) continue;
    uint32_t target = W_SEED_FRONTEND_NONE;
    bool duplicate = false;
    w_seed_frontend_callee_kind callee_kind = W_SEED_FRONTEND_CALLEE_NONE;
    uint32_t external_module_index = W_SEED_FRONTEND_NONE;
    uint32_t external_symbol_index = W_SEED_FRONTEND_NONE;
    uint32_t host_symbol_index = W_SEED_FRONTEND_NONE;
    for (size_t function_index = 0; function_index < context->count.functions;
         function_index += 1u) {
      const w_seed_frontend_function *candidate =
          &context->output->functions[function_index];
      if (candidate->module_index == owner->module_index &&
          text_equal_text(candidate->name, callee->spelling)) {
        if (target != W_SEED_FRONTEND_NONE) duplicate = true;
        target = (uint32_t)function_index;
      }
    }
    if (duplicate) target = W_SEED_FRONTEND_NONE;
    if (target != W_SEED_FRONTEND_NONE) {
      callee_kind = W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION;
    }
    if (target == W_SEED_FRONTEND_NONE) {
      w_seed_frontend_import_target_kind imported_kind =
          W_SEED_FRONTEND_IMPORT_UNRESOLVED;
      uint32_t imported_module_index = W_SEED_FRONTEND_NONE;
      w_seed_frontend_text imported_name = {NULL, 0};
      if (imported_target_for_name(context, callee->spelling, &imported_kind,
                                   &imported_module_index, &imported_name) &&
          imported_kind == W_SEED_FRONTEND_IMPORT_LOCAL_DOCUMENT &&
          (size_t)imported_module_index < context->input.document_count) {
        for (size_t function_index = 0;
             function_index < context->count.functions; function_index += 1u) {
          const w_seed_frontend_function *candidate =
              &context->output->functions[function_index];
          if (candidate->module_index == imported_module_index &&
              candidate->exported &&
              text_equal_text(candidate->name, imported_name)) {
            if (target != W_SEED_FRONTEND_NONE) duplicate = true;
            target = (uint32_t)function_index;
          }
        }
      }
    }
    if (duplicate) target = W_SEED_FRONTEND_NONE;
    if (target != W_SEED_FRONTEND_NONE &&
        callee_kind == W_SEED_FRONTEND_CALLEE_NONE) {
      callee_kind = W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION;
    }
    if (target == W_SEED_FRONTEND_NONE && !duplicate &&
        external_symbol_identity_for_name(context, callee->spelling,
                                           &external_module_index,
                                           &external_symbol_index)) {
      callee_kind = W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL;
    }
    if (target == W_SEED_FRONTEND_NONE && callee_kind ==
                                               W_SEED_FRONTEND_CALLEE_NONE &&
        !duplicate &&
        host_symbol_for_name(context, callee->spelling, NULL,
                             &host_symbol_index)) {
      callee_kind = W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL;
    }
    if (duplicate) {
      target = W_SEED_FRONTEND_NONE;
      callee_kind = W_SEED_FRONTEND_CALLEE_NONE;
    }
    callee->resolved_function_index = target;
    expression->resolved_function_index = target;
    callee->resolved_callee_kind = callee_kind;
    expression->resolved_callee_kind = callee_kind;
    callee->resolved_host_symbol_index = host_symbol_index;
    expression->resolved_host_symbol_index = host_symbol_index;
    callee->resolved_external_module_index = external_module_index;
    expression->resolved_external_module_index = external_module_index;
    callee->resolved_external_symbol_index = external_symbol_index;
    expression->resolved_external_symbol_index = external_symbol_index;
    if (target == W_SEED_FRONTEND_NONE &&
        callee_kind != W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL &&
        callee_kind != W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL)
      continue;
    if (callee_kind == W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL) {
      const w_seed_frontend_external_module *module =
          &context->input.external_modules[external_module_index];
      const w_seed_frontend_external_symbol *symbol =
          &module->symbols[external_symbol_index];
      for (uint32_t offset = 0; offset < expression->argument_count;
           offset += 1u) {
        const size_t argument_index =
            (size_t)expression->first_argument + offset;
        if (argument_index >= context->count.arguments) return false;
        w_seed_frontend_argument *argument =
            &context->output->arguments[argument_index];
        argument->resolved_parameter_ordinal = external_argument_ordinal(
            symbol->parameters, symbol->parameter_count, offset,
            argument->label);
      }
      continue;
    }
    if (callee_kind == W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL) {
      const w_seed_frontend_host_prelude *prelude = context->input.host_scope;
      if (prelude == NULL || host_symbol_index >= prelude->symbol_count)
        return false;
      const w_seed_frontend_host_prelude_symbol *symbol =
          &prelude->symbols[host_symbol_index];
      for (uint32_t offset = 0; offset < expression->argument_count;
           offset += 1u) {
        const size_t argument_index =
            (size_t)expression->first_argument + offset;
        if (argument_index >= context->count.arguments) return false;
        w_seed_frontend_argument *argument =
            &context->output->arguments[argument_index];
        argument->resolved_parameter_ordinal = external_argument_ordinal(
            symbol->parameters, symbol->parameter_count, offset,
            argument->label);
      }
      continue;
    }
    const w_seed_frontend_function *target_function =
        &context->output->functions[target];
    for (uint32_t offset = 0; offset < expression->argument_count; offset += 1u) {
      const size_t argument_index =
          (size_t)expression->first_argument + offset;
      if (argument_index >= context->count.arguments) return false;
      w_seed_frontend_argument *argument =
          &context->output->arguments[argument_index];
      uint32_t ordinal = W_SEED_FRONTEND_NONE;
      if (argument->label.length == 0u) {
        if (offset < target_function->parameter_count) {
          const size_t parameter_index =
              (size_t)target_function->first_parameter + offset;
          if (parameter_index >= context->count.parameters) return false;
          const w_seed_frontend_parameter *parameter =
              &context->output->parameters[parameter_index];
          if (parameter->label_kind ==
                  W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY ||
              parameter->label_kind == W_SEED_FRONTEND_LABEL_OPTIONAL) {
            ordinal = offset;
          }
        }
      } else {
        for (uint32_t parameter_offset = 0;
             parameter_offset < target_function->parameter_count;
             parameter_offset += 1u) {
          const size_t parameter_index =
              (size_t)target_function->first_parameter + parameter_offset;
          if (parameter_index >= context->count.parameters) return false;
          const w_seed_frontend_parameter *parameter =
              &context->output->parameters[parameter_index];
          if (text_equal_text(parameter->label, argument->label)) {
            if (ordinal != W_SEED_FRONTEND_NONE) {
              ordinal = W_SEED_FRONTEND_NONE;
              break;
            }
            ordinal = parameter_offset;
          }
        }
      }
      argument->resolved_parameter_ordinal = ordinal;
    }
  }
  return true;
}

static w_seed_frontend_text document_module_name(
    const w_seed_frontend_document *doc) {
  if (doc == NULL) return (w_seed_frontend_text){NULL, 0};
  return doc->module_id;
}

static w_seed_frontend_text document_local_module_name(
    const w_seed_frontend_document *doc) {
  if (doc == NULL) return (w_seed_frontend_text){NULL, 0};
  return doc->local_module_name;
}

static bool module_id_equal(w_seed_frontend_text left,
                            w_seed_frontend_text right) {
  return left.length != 0 && right.length != 0 && text_equal_text(left, right);
}

static bool exported_symbol_in_document(const w_seed_frontend_document *doc,
                                        w_seed_frontend_text symbol_name) {
  if (doc == NULL || symbol_name.length == 0u) return false;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    const w_seed_cst_kind kind = doc->nodes[child].kind;
    const char *keyword = declaration_keyword(kind);
    if (keyword[0] != '\0' &&
        span_has_keyword(doc, doc->nodes[child].raw_span, "export") &&
        text_equal_text(name_after_keyword(doc, doc->nodes[child].raw_span,
                                           keyword),
                        symbol_name)) {
      return true;
    }
    guard += 1u;
  }
  return false;
}

static bool exported_symbol_in_external(
    const w_seed_frontend_external_module *module,
    w_seed_frontend_text symbol_name) {
  if (module == NULL || symbol_name.length == 0u) return false;
  for (size_t index = 0u; index < module->symbol_count; index += 1u) {
    const w_seed_frontend_external_symbol *symbol = &module->symbols[index];
    if (symbol->exported && text_equal_text(symbol->name, symbol_name))
      return true;
  }
  return false;
}

static const char *declaration_keyword(w_seed_cst_kind kind) {
  switch (kind) {
    case W_SEED_CST_FUNCTION:
      return "fn";
    case W_SEED_CST_STRUCT:
      return "struct";
    case W_SEED_CST_TYPE_DECLARATION:
      return "type";
    case W_SEED_CST_ALIAS_DECLARATION:
      return "alias";
    case W_SEED_CST_ENUM:
      return "enum";
    case W_SEED_CST_CONST_DECLARATION:
      return "const";
    case W_SEED_CST_ENTRY:
      return "entry";
    default:
      return "";
  }
}

static bool detect_duplicate_declarations(frontend_context *context) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return false;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    const w_seed_cst_kind kind = doc->nodes[child].kind;
    if (kind != W_SEED_CST_FUNCTION && kind != W_SEED_CST_STRUCT &&
        kind != W_SEED_CST_TYPE_DECLARATION &&
        kind != W_SEED_CST_ALIAS_DECLARATION && kind != W_SEED_CST_ENUM &&
        kind != W_SEED_CST_CONST_DECLARATION &&
        kind != W_SEED_CST_ENTRY) {
      guard += 1;
      continue;
    }
    const char *keyword = declaration_keyword(kind);
    const w_seed_frontend_text name =
        name_after_keyword(doc, doc->nodes[child].raw_span, keyword);
    uint32_t earlier_cursor = doc->nodes[doc->parse.root].first_child;
    uint32_t earlier = W_SEED_CST_NONE;
    size_t earlier_guard = 0;
    while (next_child(doc, &earlier_cursor, &earlier) &&
           earlier_guard < doc->parse.node_count && earlier != child) {
      const w_seed_cst_kind earlier_kind = doc->nodes[earlier].kind;
      if (earlier_kind == kind ||
          (kind != W_SEED_CST_ENTRY &&
           (earlier_kind == W_SEED_CST_FUNCTION ||
            earlier_kind == W_SEED_CST_STRUCT ||
            earlier_kind == W_SEED_CST_TYPE_DECLARATION ||
            earlier_kind == W_SEED_CST_ALIAS_DECLARATION ||
            earlier_kind == W_SEED_CST_ENUM ||
            earlier_kind == W_SEED_CST_CONST_DECLARATION))) {
        const char *earlier_keyword = declaration_keyword(earlier_kind);
        if (text_equal_text(name_after_keyword(
                                doc, doc->nodes[earlier].raw_span,
                                earlier_keyword),
                            name)) {
          (void)context_append_fact(
              context, W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL,
              doc->nodes[child].raw_span, name);
          break;
        }
      }
      earlier_guard += 1;
    }
    if (kind == W_SEED_CST_ENUM) {
      uint32_t case_cursor = doc->nodes[child].first_child;
      uint32_t case_node = W_SEED_CST_NONE;
      size_t case_guard = 0;
      while (next_child(doc, &case_cursor, &case_node) &&
             case_guard < doc->parse.node_count) {
        if (doc->nodes[case_node].kind == W_SEED_CST_ENUM_CASE) {
          const w_seed_frontend_text case_name =
              first_word_in_span(doc, doc->nodes[case_node].raw_span);
          uint32_t earlier_case_cursor = doc->nodes[child].first_child;
          uint32_t earlier_case = W_SEED_CST_NONE;
          size_t earlier_case_guard = 0;
          while (next_child(doc, &earlier_case_cursor, &earlier_case) &&
                 earlier_case_guard < doc->parse.node_count &&
                 earlier_case != case_node) {
            if (doc->nodes[earlier_case].kind == W_SEED_CST_ENUM_CASE &&
                text_equal_text(
                    first_word_in_span(doc, doc->nodes[earlier_case].raw_span),
                    case_name)) {
              (void)context_append_fact(
                  context, W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL,
                  doc->nodes[case_node].raw_span, case_name);
              break;
            }
            earlier_case_guard += 1;
          }
        }
        case_guard += 1;
      }
    }
    guard += 1;
  }
  return true;
}

static bool resolve_imports(frontend_context *context) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return false;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  uint32_t direct_ordinal = 0u;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_IMPORT) {
      const w_seed_span declaration_span = doc->nodes[child].raw_span;
      w_seed_span path_span = empty_span(declaration_span.start_byte);
      if (!w_seed_module_scan_import_path_span(
              doc->source, doc->nodes, doc->parse.node_count,
              declaration_span, &path_span)) {
        return false;
      }
      const w_seed_frontend_text path = text_from_span(doc, path_span);
      w_seed_frontend_import_target_kind target_kind =
          W_SEED_FRONTEND_IMPORT_UNRESOLVED;
      uint32_t target_index = W_SEED_FRONTEND_NONE;
      if (context->input.import_resolution_complete) {
        size_t edge_index = SIZE_MAX;
        const w_seed_frontend_resolved_import *edge = NULL;
        if (!resolved_import_index_for(context, context->module_index,
                                       direct_ordinal, &edge_index) ||
            (edge = resolved_import_at(context, edge_index)) == NULL) {
          return false;
        }
        target_kind = edge->target_kind ==
                              W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT
                          ? W_SEED_FRONTEND_IMPORT_LOCAL_DOCUMENT
                          : W_SEED_FRONTEND_IMPORT_EXTERNAL_MODULE;
        target_index = edge->target_index;
      }
      if (!context->input.import_resolution_complete) {
        (void)context_append_fact(
            context, W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL,
            declaration_span, path);
      }
      uint32_t item_cursor = doc->nodes[child].first_child;
      uint32_t item = W_SEED_CST_NONE;
      size_t item_guard = 0;
      while (next_child(doc, &item_cursor, &item) &&
             item_guard < doc->parse.node_count) {
        if (doc->nodes[item].kind == W_SEED_CST_IMPORT_ITEM) {
          w_seed_frontend_text imported_name = {NULL, 0};
          const w_seed_frontend_text local_name = import_item_local_name(
              doc, doc->nodes[item].raw_span, &imported_name);
          bool exported = true;
          if (!context->input.import_resolution_complete) {
            exported = false;
          } else if (imported_name.length != 0u) {
            if (target_kind == W_SEED_FRONTEND_IMPORT_LOCAL_DOCUMENT) {
              exported = (size_t)target_index < context->input.document_count &&
                         exported_symbol_in_document(
                             &context->input.documents[target_index],
                             imported_name);
            } else if (target_kind ==
                       W_SEED_FRONTEND_IMPORT_EXTERNAL_MODULE) {
              exported = (size_t)target_index <
                             context->input.external_module_count &&
                         exported_symbol_in_external(
                             &context->input.external_modules[target_index],
                             imported_name);
            } else {
              exported = false;
            }
          }
          if (!exported) {
            (void)context_append_fact(
                context, W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL,
                doc->nodes[item].raw_span,
                imported_name.length != 0u ? imported_name : local_name);
          }
        }
        item_guard += 1;
      }
      direct_ordinal += 1u;
    }
    guard += 1;
  }
  return true;
}

typedef struct {
  uint8_t *bytes;
  size_t capacity;
  size_t length;
  bool overflow;
} frontend_receipt_writer;

static void receipt_write_bytes(frontend_receipt_writer *writer,
                               const uint8_t *bytes, size_t length) {
  if (writer == NULL) return;
  if (length > SIZE_MAX - writer->length) {
    writer->overflow = true;
    return;
  }
  if (writer->bytes != NULL) {
    if (writer->length > writer->capacity ||
        length > writer->capacity - writer->length) {
      writer->overflow = true;
      return;
    }
    (void)memcpy(writer->bytes + writer->length, bytes, length);
  }
  writer->length += length;
}

static void receipt_write_literal(frontend_receipt_writer *writer,
                                  const char *text) {
  receipt_write_bytes(writer, (const uint8_t *)text, strlen(text));
}

static void receipt_write_size(frontend_receipt_writer *writer, size_t value) {
  char digits[3u * sizeof(size_t) + 3u];
  size_t length = 0;
  if (value == 0) {
    digits[length] = '0';
    length += 1;
  } else {
    while (value != 0 && length < sizeof(digits)) {
      digits[length] = (char)('0' + (value % 10u));
      value /= 10u;
      length += 1;
    }
  }
  for (size_t index = 0; index < length / 2u; index += 1) {
    const char swap = digits[index];
    digits[index] = digits[length - index - 1u];
    digits[length - index - 1u] = swap;
  }
  receipt_write_bytes(writer, (const uint8_t *)digits, length);
}

static void receipt_write_signed(frontend_receipt_writer *writer,
                                 int64_t value) {
  uint64_t magnitude = value < 0
                           ? (uint64_t)(-(value + 1)) + UINT64_C(1)
                           : (uint64_t)value;
  if (value < 0) receipt_write_literal(writer, "-");
  char digits[3u * sizeof(uint64_t) + 3u];
  size_t length = 0u;
  do {
    digits[length] = (char)('0' + (magnitude % UINT64_C(10)));
    magnitude /= UINT64_C(10);
    length += 1u;
  } while (magnitude != 0u);
  while (length != 0u) {
    length -= 1u;
    receipt_write_bytes(writer, (const uint8_t *)&digits[length], 1u);
  }
}

static void receipt_write_text(frontend_receipt_writer *writer,
                               w_seed_frontend_text text) {
  static const char digits[] = "0123456789abcdef";
  if (text.data == NULL && text.length != 0) {
    receipt_write_literal(writer, "0:<invalid>");
    return;
  }
  /* Text is length-prefixed and hex encoded. This keeps the line-oriented
   * receipt unambiguous for source names and fact details containing '|',
   * newlines, or other delimiters. */
  receipt_write_size(writer, text.length);
  receipt_write_literal(writer, ":");
  for (size_t index = 0; index < text.length; index += 1) {
    const uint8_t byte = (uint8_t)text.data[index];
    const uint8_t pair[2] = {(uint8_t)digits[byte >> 4],
                             (uint8_t)digits[byte & 0x0fu]};
    receipt_write_bytes(writer, pair, sizeof(pair));
  }
}

static void receipt_write_span(frontend_receipt_writer *writer,
                               w_seed_span span) {
  receipt_write_size(writer, span.start_byte);
  receipt_write_literal(writer, ":");
  receipt_write_size(writer, span.end_byte);
}

static void receipt_write_digest(frontend_receipt_writer *writer,
                                 const uint8_t digest[32]) {
  static const char digits[] = "0123456789abcdef";
  for (size_t index = 0; index < 32; index += 1) {
    const uint8_t byte = digest[index];
    const uint8_t pair[2] = {(uint8_t)digits[byte >> 4],
                             (uint8_t)digits[byte & 0x0fu]};
    receipt_write_bytes(writer, pair, sizeof(pair));
  }
}

static void receipt_write_hex_bytes(frontend_receipt_writer *writer,
                                    const uint8_t *bytes, size_t length) {
  static const char digits[] = "0123456789abcdef";
  for (size_t index = 0; index < length; index += 1u) {
    const uint8_t pair[2] = {(uint8_t)digits[bytes[index] >> 4],
                             (uint8_t)digits[bytes[index] & 0x0fu]};
    receipt_write_bytes(writer, pair, sizeof(pair));
  }
}

static void receipt_write_external_records(
    frontend_receipt_writer *writer, const w_seed_frontend_input *input) {
  if (writer == NULL || input == NULL) return;
  for (size_t module_index = 0;
       module_index < input->external_module_count; module_index += 1) {
    const w_seed_frontend_external_module *module =
        &input->external_modules[module_index];
    receipt_write_literal(writer, "external-module=");
    receipt_write_text(writer, module->module_id);
    receipt_write_literal(writer, "\n");
    for (size_t symbol_index = 0; symbol_index < module->symbol_count;
         symbol_index += 1) {
      const w_seed_frontend_external_symbol *symbol =
          &module->symbols[symbol_index];
      receipt_write_literal(writer, "external-symbol=");
      receipt_write_text(writer, symbol->name);
      receipt_write_literal(writer, "|kind=");
      receipt_write_size(writer, (size_t)symbol->kind);
      receipt_write_literal(writer, "|exported=");
      receipt_write_size(writer, symbol->exported ? 1u : 0u);
      receipt_write_literal(writer, "|const=");
      receipt_write_size(writer, symbol->is_const ? 1u : 0u);
      receipt_write_literal(writer, "|return=");
      receipt_write_text(writer, symbol->return_type);
      receipt_write_literal(writer, "\n");
      for (size_t parameter_index = 0;
           parameter_index < symbol->parameter_count; parameter_index += 1) {
        const w_seed_frontend_external_parameter *parameter =
            &symbol->parameters[parameter_index];
        receipt_write_literal(writer, "external-parameter=");
        receipt_write_text(writer, parameter->name);
        receipt_write_literal(writer, "|label=");
        receipt_write_size(writer, (size_t)parameter->label_kind);
        receipt_write_literal(writer, "|type=");
        receipt_write_text(writer, parameter->type);
        receipt_write_literal(writer, "\n");
      }
    }
  }
}

static void receipt_write_host_records(frontend_receipt_writer *writer,
                                       const w_seed_frontend_input *input) {
  if (writer == NULL || input == NULL || input->host_scope == NULL) return;
  const w_seed_frontend_host_prelude *prelude = input->host_scope;
  receipt_write_literal(writer, "host-scope=");
  receipt_write_text(writer, prelude->profile);
  receipt_write_literal(writer, "\n");
  for (size_t symbol_index = 0u; symbol_index < prelude->symbol_count;
       symbol_index += 1u) {
    const w_seed_frontend_host_prelude_symbol *symbol =
        &prelude->symbols[symbol_index];
    receipt_write_literal(writer, "host-symbol=");
    receipt_write_size(writer, symbol_index);
    receipt_write_literal(writer, "|");
    receipt_write_text(writer, symbol->name);
    receipt_write_literal(writer, "|kind=");
    receipt_write_size(writer, (size_t)symbol->kind);
    receipt_write_literal(writer, "|const=");
    receipt_write_size(writer, symbol->is_const ? 1u : 0u);
    receipt_write_literal(writer, "|return=");
    receipt_write_text(writer, symbol->return_type);
    receipt_write_literal(writer, "|requirements=");
    receipt_write_size(writer, symbol->requirement_count);
    receipt_write_literal(writer, "\n");
    for (size_t requirement_index = 0u;
         requirement_index < symbol->requirement_count; requirement_index += 1u) {
      const w_seed_frontend_host_requirement *requirement =
          &symbol->requirements[requirement_index];
      receipt_write_literal(writer, "host-requirement=");
      receipt_write_size(writer, symbol_index);
      receipt_write_literal(writer, "|");
      receipt_write_size(writer, requirement_index);
      receipt_write_literal(writer, "|");
      receipt_write_text(writer, requirement->name);
      receipt_write_literal(writer, "\n");
    }
    for (size_t parameter_index = 0u;
         parameter_index < symbol->parameter_count; parameter_index += 1u) {
      const w_seed_frontend_external_parameter *parameter =
          &symbol->parameters[parameter_index];
      receipt_write_literal(writer, "host-parameter=");
      receipt_write_size(writer, symbol_index);
      receipt_write_literal(writer, "|");
      receipt_write_size(writer, parameter_index);
      receipt_write_literal(writer, "|");
      receipt_write_text(writer, parameter->name);
      receipt_write_literal(writer, "|label=");
      receipt_write_size(writer, (size_t)parameter->label_kind);
      receipt_write_literal(writer, "|type=");
      receipt_write_text(writer, parameter->type);
      receipt_write_literal(writer, "\n");
    }
  }
}

static const char *fact_name(w_seed_frontend_fact_kind kind) {
  switch (kind) {
    case W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE:
      return "unsupported-node";
    case W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE:
      return "unsupported-type";
    case W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION:
      return "unsupported-expression";
    case W_SEED_FRONTEND_FACT_DUPLICATE_LOCAL_SYMBOL:
      return "duplicate-local-symbol";
    case W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL:
      return "unresolved-imported-symbol";
    case W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL:
      return "unresolved-local-symbol";
    case W_SEED_FRONTEND_FACT_INVALID_ENTRY:
      return "invalid-entry";
  }
  return "unknown";
}

static void receipt_write_records(frontend_receipt_writer *writer,
                                  const w_seed_frontend_input *input,
                                  const w_seed_frontend_output *output,
                                  const frontend_context *context) {
  receipt_write_literal(writer, "schema=");
  receipt_write_literal(writer, W_SEED_FRONTEND_SCHEMA_VERSION);
  receipt_write_literal(writer, "\n");
  for (size_t index = 0; index < input->document_count; index += 1) {
    const w_seed_frontend_document *doc = &input->documents[index];
    uint8_t digest[32];
    w_seed_sha256_state sha;
    w_seed_sha256_init(&sha);
    w_seed_sha256_update(&sha, doc->source->bytes.data, doc->source->bytes.length);
    w_seed_sha256_final(&sha, digest);
    receipt_write_literal(writer, "source=");
    receipt_write_size(writer, index);
    receipt_write_literal(writer, "|");
    receipt_write_text(writer, doc->logical_source_id);
    receipt_write_literal(writer, "|");
    receipt_write_text(writer, document_module_name(doc));
    receipt_write_literal(writer, "|local=");
    receipt_write_text(writer, document_local_module_name(doc));
    receipt_write_literal(writer, "|sha256:");
    receipt_write_digest(writer, digest);
    receipt_write_literal(writer, "\n");
  }
  receipt_write_external_records(writer, input);
  receipt_write_host_records(writer, input);
  if (output != NULL) {
    for (size_t index = 0; index < context->count.modules; index += 1) {
      const w_seed_frontend_module *module = &output->modules[index];
      receipt_write_literal(writer, "module=");
      receipt_write_text(writer, module->module_id);
      receipt_write_literal(writer, "|local=");
      receipt_write_text(writer, module->local_module_name);
      receipt_write_literal(writer, "|source=");
      receipt_write_text(writer, module->source_id);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.const_declarations;
         index += 1u) {
      const w_seed_frontend_const_declaration *value =
          &output->const_declarations[index];
      receipt_write_literal(writer, "const-declaration=");
      receipt_write_size(writer, value->module_index);
      receipt_write_literal(writer, "|");
      receipt_write_text(writer, value->name);
      receipt_write_literal(writer, "|exported=");
      receipt_write_size(writer, value->exported ? 1u : 0u);
       receipt_write_literal(writer, "|span=");
       receipt_write_span(writer, value->span);
       receipt_write_literal(writer, "|declared=");
       receipt_write_size(writer, value->declared_type);
       receipt_write_literal(writer, "|effective=");
       receipt_write_size(writer, value->effective_type);
       receipt_write_literal(writer, "|explicit=");
       receipt_write_size(writer, value->has_explicit_type ? 1u : 0u);
       receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.imports; index += 1) {
      const w_seed_frontend_import *item = &output->imports[index];
      receipt_write_literal(writer, "import=");
      receipt_write_size(writer, item->module_index);
      receipt_write_literal(writer, "|");
      receipt_write_text(writer, item->path);
      receipt_write_literal(writer, "|ordinal=");
      receipt_write_size(writer, item->direct_import_ordinal);
      receipt_write_literal(writer, "|target-kind=");
      receipt_write_size(writer, (size_t)item->target_kind);
      receipt_write_literal(writer, "|target-index=");
      receipt_write_size(writer, item->target_index);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.enums; index += 1) {
      const w_seed_frontend_enum *value = &output->enums[index];
      receipt_write_literal(writer, "enum=");
      receipt_write_size(writer, value->module_index);
      receipt_write_literal(writer, "|");
      receipt_write_text(writer, value->name);
      receipt_write_literal(writer, "|exported=");
      receipt_write_size(writer, value->exported ? 1u : 0u);
      receipt_write_literal(writer, "|span=");
      receipt_write_span(writer, value->span);
      receipt_write_literal(writer, "|generic=");
      receipt_write_span(writer, value->generic_span);
      receipt_write_literal(writer, "|has-generic=");
      receipt_write_size(writer, value->has_generic_parameters ? 1u : 0u);
      receipt_write_literal(writer, "|conformance=");
      receipt_write_size(writer, value->conformance_type);
      receipt_write_literal(writer, "|conformance-span=");
      receipt_write_span(writer, value->conformance_span);
      receipt_write_literal(writer, "|first-case=");
      receipt_write_size(writer, value->first_case);
      receipt_write_literal(writer, "|case-count=");
      receipt_write_size(writer, value->case_count);
      receipt_write_literal(writer, "|type=");
      receipt_write_size(writer, value->type_index);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.enum_cases; index += 1) {
      const w_seed_frontend_enum_case *value = &output->enum_cases[index];
      receipt_write_literal(writer, "enum-case=");
      receipt_write_size(writer, value->module_index);
      receipt_write_literal(writer, "|");
      receipt_write_size(writer, value->owner_enum);
      receipt_write_literal(writer, "|");
      receipt_write_text(writer, value->name);
      receipt_write_literal(writer, "|span=");
      receipt_write_span(writer, value->span);
      receipt_write_literal(writer, "|first-payload=");
      receipt_write_size(writer, value->first_payload);
      receipt_write_literal(writer, "|payload-count=");
      receipt_write_size(writer, value->payload_count);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.enum_case_parameters;
         index += 1) {
      const w_seed_frontend_enum_case_parameter *value =
          &output->enum_case_parameters[index];
      receipt_write_literal(writer, "enum-case-parameter=");
      receipt_write_size(writer, value->module_index);
      receipt_write_literal(writer, "|");
      receipt_write_size(writer, value->owner_case);
      receipt_write_literal(writer, "|label=");
      receipt_write_text(writer, value->label);
      receipt_write_literal(writer, "|has-label=");
      receipt_write_size(writer, value->has_label ? 1u : 0u);
      receipt_write_literal(writer, "|span=");
      receipt_write_span(writer, value->span);
      receipt_write_literal(writer, "|type=");
      receipt_write_size(writer, value->type_index);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.switch_arms; index += 1) {
      const w_seed_frontend_switch_arm *value = &output->switch_arms[index];
      receipt_write_literal(writer, "switch-arm=");
      receipt_write_size(writer, value->module_index);
      receipt_write_literal(writer, "|owner=");
      receipt_write_size(writer, value->owner_expression);
      receipt_write_literal(writer, "|pattern=");
      receipt_write_size(writer, (size_t)value->pattern_kind);
      receipt_write_literal(writer, "|enum=");
      receipt_write_size(writer, value->enum_index);
      receipt_write_literal(writer, "|case=");
      receipt_write_size(writer, value->enum_case_index);
      receipt_write_literal(writer, "|pattern-span=");
      receipt_write_span(writer, value->pattern_span);
      receipt_write_literal(writer, "|result=");
      receipt_write_size(writer, value->result_expression);
      receipt_write_literal(writer, "|span=");
      receipt_write_span(writer, value->span);
      receipt_write_literal(writer, "|supported=");
      receipt_write_size(writer, value->supported ? 1u : 0u);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.enum_membership_cases;
         index += 1) {
      const w_seed_frontend_enum_membership_case *value =
          &output->enum_membership_cases[index];
      receipt_write_literal(writer, "enum-membership-case=");
      receipt_write_size(writer, value->module_index);
      receipt_write_literal(writer, "|owner=");
      receipt_write_size(writer, value->owner_expression);
      receipt_write_literal(writer, "|enum=");
      receipt_write_size(writer, value->enum_base_index);
      receipt_write_literal(writer, "|case=");
      receipt_write_size(writer, value->enum_case_index);
      receipt_write_literal(writer, "|span=");
      receipt_write_span(writer, value->source_span);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.generic_parameters;
         index += 1) {
      const w_seed_frontend_generic_parameter *value =
          &output->generic_parameters[index];
      receipt_write_literal(writer, "generic-parameter=");
      receipt_write_size(writer, value->module_index);
      receipt_write_literal(writer, "|owner-kind=");
      receipt_write_size(writer, (size_t)value->owner_kind);
      receipt_write_literal(writer, "|owner=");
      receipt_write_size(writer, value->owner_index);
      receipt_write_literal(writer, "|ordinal=");
      receipt_write_size(writer, value->ordinal);
      receipt_write_literal(writer, "|external-label=");
      receipt_write_text(writer, value->external_label);
      receipt_write_literal(writer, "|name=");
      receipt_write_text(writer, value->internal_name);
      receipt_write_literal(writer, "|label=");
      receipt_write_size(writer, (size_t)value->label_kind);
      receipt_write_literal(writer, "|kind=");
      receipt_write_size(writer, (size_t)value->kind);
      receipt_write_literal(writer, "|span=");
      receipt_write_span(writer, value->span);
      receipt_write_literal(writer, "|domain=");
      receipt_write_size(writer, value->domain_type);
      receipt_write_literal(writer, "|refinement=");
      receipt_write_size(writer, (size_t)value->refinement_kind);
      receipt_write_literal(writer, "|predicate=");
      receipt_write_size(writer, value->predicate_function_index);
      receipt_write_literal(writer, "|predicate-span=");
      receipt_write_span(writer, value->predicate_span);
      receipt_write_literal(writer, "|predicate-function-span=");
      receipt_write_span(writer, value->predicate_function_span);
      receipt_write_literal(writer, "|subject=");
      receipt_write_size(writer, (size_t)value->subject_kind);
      receipt_write_literal(writer, "|domain-kind=");
      receipt_write_size(writer, (size_t)value->domain_kind);
      receipt_write_literal(writer, "|dependent=");
      receipt_write_size(writer, value->dependent_type_parameter_ordinal);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.generic_applications;
         index += 1u) {
      const w_seed_frontend_generic_application *value =
          &output->generic_applications[index];
      receipt_write_literal(writer, "generic-application=");
      receipt_write_size(writer, value->module_index);
      receipt_write_literal(writer, "|owner-type=");
      receipt_write_size(writer, value->owner_type);
      receipt_write_literal(writer, "|head=");
      receipt_write_size(writer, value->head_struct);
      receipt_write_literal(writer, "|");
      receipt_write_text(writer, value->head_name);
      receipt_write_literal(writer, "|span=");
      receipt_write_span(writer, value->span);
      receipt_write_literal(writer, "|envelope=");
      receipt_write_span(writer, value->envelope_span);
      receipt_write_literal(writer, "|first-argument=");
      receipt_write_size(writer, value->first_argument);
      receipt_write_literal(writer, "|argument-count=");
      receipt_write_size(writer, value->argument_count);
      receipt_write_literal(writer, "|binding=");
      receipt_write_size(writer, (size_t)value->binding_status);
      receipt_write_literal(writer, "|requires-const=");
      receipt_write_size(writer, value->requires_const_evaluation ? 1u : 0u);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.generic_arguments;
         index += 1u) {
      const w_seed_frontend_generic_argument *value =
          &output->generic_arguments[index];
      receipt_write_literal(writer, "generic-argument=");
      receipt_write_size(writer, value->module_index);
      receipt_write_literal(writer, "|owner=");
      receipt_write_size(writer, value->owner_application);
      receipt_write_literal(writer, "|ordinal=");
      receipt_write_size(writer, value->source_ordinal);
      receipt_write_literal(writer, "|span=");
      receipt_write_span(writer, value->span);
      receipt_write_literal(writer, "|label=");
      receipt_write_text(writer, value->label);
      receipt_write_literal(writer, "|parameter=");
      receipt_write_size(writer, value->parameter_index);
      receipt_write_literal(writer, "|parameter-ordinal=");
      receipt_write_size(writer, value->parameter_ordinal);
      receipt_write_literal(writer, "|kind=");
      receipt_write_size(writer, (size_t)value->kind);
      receipt_write_literal(writer, "|type=");
      receipt_write_size(writer, value->type_index);
      receipt_write_literal(writer, "|const=");
      receipt_write_size(writer, value->const_value_index);
      receipt_write_literal(writer, "|typed-expr=");
      receipt_write_size(writer, value->typed_const_expression_index);
      receipt_write_literal(writer, "|binding=");
      receipt_write_size(writer, (size_t)value->binding_status);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.typed_const_expressions;
         index += 1u) {
      const w_seed_frontend_typed_const_expression *value =
          &output->typed_const_expressions[index];
      receipt_write_literal(writer, "typed-const-expression=");
      receipt_write_size(writer, value->module_index);
      receipt_write_literal(writer, "|owner=");
      receipt_write_size(writer, value->owner_application);
      receipt_write_literal(writer, "|argument=");
      receipt_write_size(writer, value->argument_ordinal);
      receipt_write_literal(writer, "|expression=");
      receipt_write_size(writer, value->expression_index);
      receipt_write_literal(writer, "|span=");
      receipt_write_span(writer, value->span);
      receipt_write_literal(writer, "|expected=");
      receipt_write_size(writer, value->expected_type);
      receipt_write_literal(writer, "|effective=");
      receipt_write_size(writer, value->effective_type);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.const_values; index += 1u) {
      const w_seed_frontend_const_value *value = &output->const_values[index];
      receipt_write_literal(writer, "const-value=");
      receipt_write_size(writer, (size_t)value->kind);
      receipt_write_literal(writer, "|type=");
      receipt_write_size(writer, value->type_index);
      receipt_write_literal(writer, "|span=");
      receipt_write_span(writer, value->span);
      receipt_write_literal(writer, "|bool=");
      receipt_write_size(writer, value->bool_value ? 1u : 0u);
      receipt_write_literal(writer, "|signed=");
      receipt_write_size(writer, value->integer_signed ? 1u : 0u);
      receipt_write_literal(writer, "|width=");
      receipt_write_size(writer, value->integer_bit_width);
      receipt_write_literal(writer, "|integer=");
      receipt_write_hex_bytes(writer, value->integer_bytes,
                              value->integer_byte_count);
      receipt_write_literal(writer, "|first-byte=");
      receipt_write_size(writer, value->first_byte);
      receipt_write_literal(writer, "|byte-count=");
      receipt_write_size(writer, value->byte_count);
      receipt_write_literal(writer, "|enum=");
      receipt_write_size(writer, value->enum_base_index);
      receipt_write_literal(writer, "|case=");
      receipt_write_size(writer, value->enum_case_index);
      receipt_write_literal(writer, "|first-element=");
      receipt_write_size(writer, value->first_element);
      receipt_write_literal(writer, "|element-count=");
      receipt_write_size(writer, value->element_count);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.const_elements; index += 1u) {
      const w_seed_frontend_const_element *value = &output->const_elements[index];
      receipt_write_literal(writer, "const-element=");
      receipt_write_size(writer, value->owner_value);
      receipt_write_literal(writer, "|ordinal=");
      receipt_write_size(writer, value->ordinal);
      receipt_write_literal(writer, "|value=");
      receipt_write_size(writer, value->value_index);
      receipt_write_literal(writer, "|span=");
      receipt_write_span(writer, value->span);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.symbols; index += 1) {
      const w_seed_frontend_symbol *symbol = &output->symbols[index];
      receipt_write_literal(writer, "symbol=");
      receipt_write_size(writer, symbol->module_index);
      receipt_write_literal(writer, "|");
      receipt_write_text(writer, symbol->name);
      receipt_write_literal(writer, "|");
      receipt_write_size(writer, (size_t)symbol->kind);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.types; index += 1) {
      const w_seed_frontend_type *type = &output->types[index];
      receipt_write_literal(writer, "type=");
      receipt_write_size(writer, (size_t)type->kind);
      receipt_write_literal(writer, "|");
      receipt_write_text(writer, type->spelling);
      receipt_write_literal(writer, "|enum-base=");
      receipt_write_size(writer, type->enum_base_index);
      receipt_write_literal(writer, "|subset-first=");
      receipt_write_size(writer, type->first_subset_member);
      receipt_write_literal(writer, "|subset-count=");
      receipt_write_size(writer, type->subset_member_count);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.enum_subset_members;
         index += 1) {
      const w_seed_frontend_enum_subset_member *member =
          &output->enum_subset_members[index];
      receipt_write_literal(writer, "enum-subset-member=");
      receipt_write_size(writer, member->owner_type);
      receipt_write_literal(writer, "|enum=");
      receipt_write_size(writer, member->enum_base_index);
      receipt_write_literal(writer, "|case=");
      receipt_write_size(writer, member->enum_case_index);
      receipt_write_literal(writer, "|span=");
      receipt_write_span(writer, member->source_span);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.functions; index += 1) {
      const w_seed_frontend_function *function = &output->functions[index];
      receipt_write_literal(writer, "signature=");
      receipt_write_text(writer, function->name);
      receipt_write_literal(writer, "|");
      receipt_write_size(writer, function->parameter_count);
      receipt_write_literal(writer, "|");
      receipt_write_size(writer, function->return_type);
      receipt_write_literal(writer, "|const=");
      receipt_write_size(writer, function->is_const ? 1u : 0u);
      receipt_write_literal(writer, "|const-body=");
      receipt_write_size(writer, function->const_body_supported ? 1u : 0u);
      receipt_write_literal(writer, "|async=");
      receipt_write_size(writer, function->is_async ? 1u : 0u);
      receipt_write_literal(writer, "|throws=");
      receipt_write_size(writer, function->is_throws ? 1u : 0u);
      receipt_write_literal(writer, "|unsafe=");
      receipt_write_size(writer, function->is_unsafe ? 1u : 0u);
      receipt_write_literal(writer, "|borrows=");
      receipt_write_size(writer, function->has_borrow_clause ? 1u : 0u);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0u; index < context->count.expressions; index += 1u) {
      const w_seed_frontend_expression *expression =
          &output->expressions[index];
      if (expression->kind != W_SEED_FRONTEND_EXPR_CALL) continue;
      uint32_t callee_index = W_SEED_FRONTEND_NONE;
      if (expression->left != W_SEED_FRONTEND_NONE &&
          (size_t)expression->left < context->count.expressions) {
        callee_index = expression->left;
      }
      receipt_write_literal(writer, "callee=");
      receipt_write_size(writer, index);
      receipt_write_literal(writer, "|identifier=");
      receipt_write_size(writer, callee_index);
      receipt_write_literal(writer, "|kind=");
      receipt_write_size(writer, (size_t)expression->resolved_callee_kind);
      receipt_write_literal(writer, "|host=");
      receipt_write_size(writer, expression->resolved_host_symbol_index);
      receipt_write_literal(writer, "|external=");
      receipt_write_size(writer, expression->resolved_external_module_index);
      receipt_write_literal(writer, ":");
      receipt_write_size(writer, expression->resolved_external_symbol_index);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.facts; index += 1) {
      const w_seed_frontend_fact *fact = &output->facts[index];
      receipt_write_literal(writer, "fact=");
      receipt_write_literal(writer, fact_name(fact->kind));
      receipt_write_literal(writer, "|");
      receipt_write_span(writer, fact->span);
      receipt_write_literal(writer, "|");
      receipt_write_text(writer, fact->detail);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.diagnostics; index += 1) {
      const w_seed_frontend_diagnostic *diagnostic = &output->diagnostics[index];
      receipt_write_literal(writer, "diagnostic=");
      receipt_write_text(writer, diagnostic->code);
      receipt_write_literal(writer, "|");
      receipt_write_span(writer, diagnostic->primary);
      receipt_write_literal(writer, "|facts=");
      receipt_write_size(writer, diagnostic->first_fact);
      receipt_write_literal(writer, ":");
      receipt_write_size(writer, diagnostic->fact_count);
      receipt_write_literal(writer, "|labels=");
      receipt_write_size(writer, diagnostic->first_label);
      receipt_write_literal(writer, ":");
      receipt_write_size(writer, diagnostic->label_count);
      receipt_write_literal(writer, "\n");
      for (size_t fact_index = 0u; fact_index < diagnostic->fact_count;
           fact_index += 1u) {
        const w_seed_frontend_diagnostic_fact *fact =
            &output->diagnostic_facts[(size_t)diagnostic->first_fact +
                                      fact_index];
        receipt_write_literal(writer, "diagnostic-fact=");
        receipt_write_size(writer, index);
        receipt_write_literal(writer, "|key=");
        receipt_write_text(writer, fact->key);
        receipt_write_literal(writer, "|kind=");
        receipt_write_size(writer, (size_t)fact->kind);
        receipt_write_literal(writer, "|value=");
        if (fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_INTEGER) {
          receipt_write_signed(writer, fact->integer_value);
        } else if (fact->kind == W_SEED_FRONTEND_DIAGNOSTIC_FACT_STRING) {
          receipt_write_text(writer, fact->text);
        } else {
          receipt_write_text(writer, (w_seed_frontend_text){NULL, 0u});
        }
        receipt_write_literal(writer, "|items=");
        receipt_write_size(writer, fact->first_item);
        receipt_write_literal(writer, ":");
        receipt_write_size(writer, fact->item_count);
        receipt_write_literal(writer, "\n");
        for (size_t item_index = 0u; item_index < fact->item_count;
             item_index += 1u) {
          const w_seed_frontend_diagnostic_item *item =
              &output->diagnostic_items[(size_t)fact->first_item + item_index];
          receipt_write_literal(writer, "diagnostic-item=");
          receipt_write_size(writer, index);
          receipt_write_literal(writer, "|");
          receipt_write_size(writer, item_index);
          receipt_write_literal(writer, "|");
          receipt_write_text(writer, item->text);
          receipt_write_literal(writer, "\n");
        }
      }
      for (size_t label_index = 0u; label_index < diagnostic->label_count;
           label_index += 1u) {
        const w_seed_frontend_diagnostic_label *label =
            &output->diagnostic_labels[(size_t)diagnostic->first_label +
                                       label_index];
        receipt_write_literal(writer, "diagnostic-label=");
        receipt_write_size(writer, index);
        receipt_write_literal(writer, "|role=");
        receipt_write_text(writer, label->role);
        receipt_write_literal(writer, "|document=");
        receipt_write_size(writer, label->document_index);
        receipt_write_literal(writer, "|span=");
        receipt_write_span(writer, label->span);
        receipt_write_literal(writer, "\n");
      }
    }
  }
}

static bool capacity_ok(size_t required, const void *array, size_t capacity) {
  return required == 0 || (array != NULL && capacity >= required);
}

static bool output_capacity_ok(const w_seed_frontend_output *output,
                               const frontend_measure *required,
                               size_t receipt_required) {
  if (output == NULL || required == NULL) return false;
  return capacity_ok(required->modules, output->modules,
                    output->module_capacity) &&
         capacity_ok(required->imports, output->imports,
                     output->import_capacity) &&
         capacity_ok(required->import_items, output->import_items,
                     output->import_item_capacity) &&
         capacity_ok(required->structs, output->structs,
                     output->struct_capacity) &&
         capacity_ok(required->enums, output->enums, output->enum_capacity) &&
         capacity_ok(required->enum_cases, output->enum_cases,
                     output->enum_case_capacity) &&
         capacity_ok(required->enum_case_parameters,
                     output->enum_case_parameters,
                     output->enum_case_parameter_capacity) &&
         capacity_ok(required->const_declarations, output->const_declarations,
                     output->const_declaration_capacity) &&
         capacity_ok(required->fields, output->fields, output->field_capacity) &&
         capacity_ok(required->type_declarations, output->type_declarations,
                     output->type_declaration_capacity) &&
         capacity_ok(required->aliases, output->aliases, output->alias_capacity) &&
         capacity_ok(required->types, output->types, output->type_capacity) &&
         capacity_ok(required->functions, output->functions,
                     output->function_capacity) &&
         capacity_ok(required->parameters, output->parameters,
                     output->parameter_capacity) &&
         capacity_ok(required->entries, output->entries, output->entry_capacity) &&
         capacity_ok(required->statements, output->statements,
                     output->statement_capacity) &&
         capacity_ok(required->expressions, output->expressions,
                     output->expression_capacity) &&
         capacity_ok(required->arguments, output->arguments,
                     output->argument_capacity) &&
         capacity_ok(required->switch_arms, output->switch_arms,
                     output->switch_arm_capacity) &&
         capacity_ok(required->enum_subset_members,
                     output->enum_subset_members,
                     output->enum_subset_member_capacity) &&
         capacity_ok(required->enum_membership_cases,
                     output->enum_membership_cases,
                     output->enum_membership_case_capacity) &&
         capacity_ok(required->generic_parameters,
                     output->generic_parameters,
                     output->generic_parameter_capacity) &&
         capacity_ok(required->generic_applications,
                     output->generic_applications,
                     output->generic_application_capacity) &&
         capacity_ok(required->generic_arguments,
                     output->generic_arguments,
                     output->generic_argument_capacity) &&
         capacity_ok(required->typed_const_expressions,
                     output->typed_const_expressions,
                     output->typed_const_expression_capacity) &&
         capacity_ok(required->const_values, output->const_values,
                     output->const_value_capacity) &&
         capacity_ok(required->const_elements, output->const_elements,
                     output->const_element_capacity) &&
         capacity_ok(required->const_bytes, output->const_bytes,
                     output->const_bytes_capacity) &&
         capacity_ok(required->symbols, output->symbols,
                     output->symbol_capacity) &&
         capacity_ok(required->facts, output->facts, output->fact_capacity) &&
         capacity_ok(required->diagnostics, output->diagnostics,
                     output->diagnostic_capacity) &&
         capacity_ok(required->diagnostic_facts, output->diagnostic_facts,
                     output->diagnostic_fact_capacity) &&
         capacity_ok(required->diagnostic_items, output->diagnostic_items,
                     output->diagnostic_item_capacity) &&
         capacity_ok(required->diagnostic_labels, output->diagnostic_labels,
                     output->diagnostic_label_capacity) &&
         capacity_ok(receipt_required, output->receipt, output->receipt_capacity);
}

static void result_counts_from_measure(const frontend_measure *measure,
                                       w_seed_frontend_counts *counts) {
  counts_from_measure(measure, counts);
}

w_seed_frontend_status w_seed_frontend_run(
    const w_seed_frontend_input *input, w_seed_frontend_output *output,
    w_seed_frontend_result *result) {
  frontend_measure ignored_measure;
  size_t barrier_document = W_SEED_FRONTEND_NONE_SIZE;
  w_seed_span barrier_span = empty_span(0);
  if (result == NULL || output == NULL) return W_SEED_FRONTEND_INVALID;
  frontend_diagnostic_category_scratch_count = 0u;
  (void)memset(result, 0, sizeof(*result));
  result->schema_version = (w_seed_frontend_text){
      W_SEED_FRONTEND_SCHEMA_VERSION,
      sizeof(W_SEED_FRONTEND_SCHEMA_VERSION) - 1u};
  result->barrier_document = W_SEED_FRONTEND_NONE_SIZE;
  result->primary_diagnostic = W_SEED_FRONTEND_NONE_SIZE;
  if (!measure_input(input, &ignored_measure, &barrier_document, &barrier_span)) {
    result->status = barrier_document == W_SEED_FRONTEND_NONE_SIZE
                         ? W_SEED_FRONTEND_INVALID
                         : W_SEED_FRONTEND_BARRIER;
    result->barrier_document = barrier_document;
    result->barrier_span = barrier_span;
    return result->status;
  }

  frontend_context dry;
  (void)memset(&dry, 0, sizeof(dry));
  dry.input = *input;
  dry.output = NULL;
  dry.result = result;
  dry.emit = false;
  dry.builtin_usize_type_index = W_SEED_FRONTEND_NONE;
  dry.const_inferred_types = frontend_const_inferred_types_scratch;
  dry.const_declared_type_indices =
      frontend_const_declared_type_indices_scratch;
  dry.const_inferred_type_indices =
      frontend_const_inferred_type_indices_scratch;
  dry.const_inference_states = frontend_const_inference_states_scratch;
  if (!initialize_const_document_bases(&dry)) {
    result->status = W_SEED_FRONTEND_BARRIER;
    result->barrier_document = W_SEED_FRONTEND_NONE_SIZE;
    return result->status;
  }
  (void)memset(dry.const_inferred_types, 0,
               sizeof(frontend_const_inferred_types_scratch));
  (void)memset(dry.const_declared_type_indices, 0xff,
               sizeof(frontend_const_declared_type_indices_scratch));
  (void)memset(dry.const_inferred_type_indices, 0xff,
               sizeof(frontend_const_inferred_type_indices_scratch));
  (void)memset(dry.const_inference_states, 0,
               sizeof(frontend_const_inference_states_scratch));
  (void)memset(dry.generic_domain_type_indices, 0xff,
               sizeof(dry.generic_domain_type_indices));
  if (!receipt_size_source_records(&dry)) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  if (!receipt_size_external_records(&dry)) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  if (!receipt_size_host_records(&dry)) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  for (size_t index = 0; index < input->document_count; index += 1) {
    dry.module_index = index;
    if (!normalize_document(&dry) || !detect_duplicate_declarations(&dry) ||
        !resolve_imports(&dry)) {
      result->status = W_SEED_FRONTEND_INVALID;
      return result->status;
    }
  }
  if (!resolve_pending_generic_applications(&dry)) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  if (dry.receipt_overflow) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  const size_t required_receipt = dry.receipt_size;
  result_counts_from_measure(&dry.count, &result->required);
  result->required.receipt_bytes = required_receipt;
  result->receipt_bytes = required_receipt;
  if (!output_capacity_ok(output, &dry.count, required_receipt)) {
    result->status = W_SEED_FRONTEND_CAPACITY;
    return result->status;
  }

  frontend_diagnostic_category_scratch_count = 0u;
  frontend_context emit;
  (void)memset(&emit, 0, sizeof(emit));
  emit.input = *input;
  emit.output = output;
  emit.result = result;
  emit.emit = true;
  emit.function_index = W_SEED_FRONTEND_NONE;
  emit.current_module_const = W_SEED_FRONTEND_NONE;
  emit.builtin_usize_type_index = W_SEED_FRONTEND_NONE;
  emit.const_inferred_types = frontend_const_inferred_types_scratch;
  emit.const_declared_type_indices =
      frontend_const_declared_type_indices_scratch;
  emit.const_inferred_type_indices =
      frontend_const_inferred_type_indices_scratch;
  emit.const_inference_states = frontend_const_inference_states_scratch;
  if (!initialize_const_document_bases(&emit)) {
    result->status = W_SEED_FRONTEND_BARRIER;
    result->barrier_document = W_SEED_FRONTEND_NONE_SIZE;
    return result->status;
  }
  (void)memset(emit.const_inferred_types, 0,
               sizeof(frontend_const_inferred_types_scratch));
  (void)memset(emit.const_declared_type_indices, 0xff,
               sizeof(frontend_const_declared_type_indices_scratch));
  (void)memset(emit.const_inferred_type_indices, 0xff,
               sizeof(frontend_const_inferred_type_indices_scratch));
  (void)memset(emit.const_inference_states, 0,
               sizeof(frontend_const_inference_states_scratch));
  (void)memset(emit.generic_domain_type_indices, 0xff,
               sizeof(emit.generic_domain_type_indices));
  for (size_t index = 0; index < input->document_count; index += 1) {
    emit.module_index = index;
    if (!normalize_document(&emit) || !detect_duplicate_declarations(&emit) ||
        !resolve_imports(&emit)) {
      result->status = W_SEED_FRONTEND_INVALID;
      return result->status;
    }
  }
  if (!resolve_pending_generic_applications(&emit)) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  if (!resolve_frontend_links(&emit)) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  frontend_receipt_writer writer = {output->receipt, output->receipt_capacity, 0,
                                    false};
  receipt_write_records(&writer, input, output, &emit);
  if (writer.overflow) {
    result->status = W_SEED_FRONTEND_CAPACITY;
    return result->status;
  }
  if (writer.length != required_receipt) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  result_counts_from_measure(&emit.count, &result->written);
  result->written.receipt_bytes = writer.length;
  result->receipt_bytes = writer.length;
  if (emit.count.diagnostics != 0) {
    result->status = W_SEED_FRONTEND_DIAGNOSTICS;
  } else if (emit.count.facts != 0) {
    result->status = W_SEED_FRONTEND_UNSUPPORTED;
  } else {
    result->status = W_SEED_FRONTEND_OK;
  }
  return result->status;
}
