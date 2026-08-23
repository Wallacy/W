#include "w_seed_frontend.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

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
} frontend_simple_type;

typedef struct {
  size_t index;
  frontend_simple_type type;
  bool supported;
  bool has_name;
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
} frontend_measure;

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
} frontend_context;

static bool normalize_document(frontend_context *context);
static bool detect_duplicate_declarations(frontend_context *context);
static bool resolve_imports(frontend_context *context);
static const char *fact_name(w_seed_frontend_fact_kind kind);
static w_seed_frontend_text document_module_name(
    const w_seed_frontend_document *doc);
static w_seed_frontend_text binding_name_after_keyword(
    const w_seed_frontend_document *doc, w_seed_span span, const char *keyword);
static w_seed_frontend_text import_item_local_name(
    const w_seed_frontend_document *doc, w_seed_span span,
    w_seed_frontend_text *imported_name);
static w_seed_frontend_label_kind parameter_label_kind(
    const w_seed_frontend_document *doc, w_seed_span span);
static w_seed_frontend_text parameter_name_from_span(
    const w_seed_frontend_document *doc, w_seed_span span);

static w_seed_span empty_span(size_t offset) {
  const w_seed_span span = {offset, offset};
  return span;
}

static bool add_size(size_t left, size_t right, size_t *result) {
  if (right > SIZE_MAX - left) return false;
  *result = left + right;
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

static bool receipt_size_module(frontend_context *context,
                               const w_seed_frontend_module *module) {
  return receipt_size_literal(context, "module=") &&
         receipt_size_text(context, module->module_id) &&
         receipt_size_literal(context, "|source=") &&
         receipt_size_text(context, module->source_id) &&
         receipt_size_literal(context, "\n");
}

static bool receipt_size_import(frontend_context *context,
                                const w_seed_frontend_import *import_value) {
  return receipt_size_literal(context, "import=") &&
         receipt_size_size(context, import_value->module_index) &&
         receipt_size_literal(context, "|") &&
         receipt_size_text(context, import_value->path) &&
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

static bool receipt_size_diagnostic(
    frontend_context *context, const w_seed_frontend_diagnostic *diagnostic) {
  return receipt_size_literal(context, "diagnostic=") &&
         receipt_size_text(context, diagnostic->code) &&
         receipt_size_literal(context, "|") &&
         receipt_size_span(context, diagnostic->primary) &&
         receipt_size_literal(context, "\n");
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
  const frontend_simple_type type = {W_SEED_FRONTEND_TYPE_UNKNOWN, false, 0,
                                     {NULL, 0}};
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

static bool is_binary_operator(w_seed_frontend_text text) {
  static const char *const operators[] = {
      "+",  "-",  "*",  "/",  "%",  "==", "!=", "<", "<=", ">",
      ">=", "&&", "||",
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
  if (text_equal(text, "<") || text_equal(text, "<=") ||
      text_equal(text, ">") || text_equal(text, ">=")) {
    return 4;
  }
  if (text_equal(text, "+") || text_equal(text, "-")) return 5;
  if (text_equal(text, "*") || text_equal(text, "/") ||
      text_equal(text, "%")) {
    return 6;
  }
  return -1;
}

static size_t count_root_children(const w_seed_frontend_document *doc,
                                  w_seed_cst_kind kind) {
  return count_direct_kind(doc, doc->parse.root, kind);
}

static bool kind_is_statement(w_seed_cst_kind kind) {
  return kind == W_SEED_CST_LET_STATEMENT ||
         kind == W_SEED_CST_VAR_STATEMENT ||
         kind == W_SEED_CST_RETURN_STATEMENT ||
         kind == W_SEED_CST_IF_STATEMENT ||
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
  measure->type_declarations +=
      count_root_children(doc, W_SEED_CST_TYPE_DECLARATION);
  measure->aliases += count_root_children(doc, W_SEED_CST_ALIAS_DECLARATION);
  measure->functions += count_root_children(doc, W_SEED_CST_FUNCTION);
  measure->entries += count_root_children(doc, W_SEED_CST_ENTRY);

  for (size_t index = 0; index < doc->parse.node_count; index += 1) {
    const w_seed_cst_kind kind = doc->nodes[index].kind;
    if (kind == W_SEED_CST_IMPORT_ITEM) measure->import_items += 1;
    if (kind == W_SEED_CST_FIELD) measure->fields += 1;
    if (kind == W_SEED_CST_PARAMETER) measure->parameters += 1;
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
      !external_input_ready(input)) {
    return false;
  }
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
  }
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
  measure->diagnostics = measure->expressions + measure->facts + 8u;
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
}

w_seed_frontend_status w_seed_frontend_measure(
    const w_seed_frontend_input *input, w_seed_frontend_counts *counts,
    w_seed_frontend_result *result) {
  frontend_measure measure;
  size_t barrier_document = W_SEED_FRONTEND_NONE_SIZE;
  w_seed_span barrier_span = empty_span(0);
  if (counts == NULL || result == NULL) return W_SEED_FRONTEND_INVALID;
  (void)memset(result, 0, sizeof(*result));
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
  if (!receipt_size_source_records(&dry)) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  if (!receipt_size_external_records(&dry)) {
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

static bool context_append_fact(frontend_context *context,
                                w_seed_frontend_fact_kind kind,
                                w_seed_span span, w_seed_frontend_text detail) {
  if (context == NULL) return false;
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

static bool context_append_diagnostic(
    frontend_context *context, w_seed_frontend_diagnostic_kind kind,
    const char *code, w_seed_frontend_text actual,
    w_seed_frontend_text expected, w_seed_frontend_text declaration,
    w_seed_frontend_text label, w_seed_frontend_text accepted_forms,
    w_seed_span primary) {
  if (context == NULL) return false;
  const size_t ordinal = context->count.diagnostics;
  context->count.diagnostics += 1;
  if (context->result != NULL &&
      context->result->primary_diagnostic == W_SEED_FRONTEND_NONE_SIZE) {
    context->result->primary_diagnostic = ordinal;
  }
  if (!context->emit) {
    const w_seed_frontend_diagnostic value = {
        kind,
        {code, strlen(code)},
        actual,
        expected,
        declaration,
        label,
        accepted_forms,
        primary,
        context->module_index,
    };
    return receipt_size_diagnostic(context, &value);
  }
  if (context->output == NULL || context->output->diagnostics == NULL ||
      ordinal >= context->output->diagnostic_capacity) {
    return false;
  }
  w_seed_frontend_diagnostic *diagnostic =
      &context->output->diagnostics[ordinal];
  diagnostic->kind = kind;
  diagnostic->code.data = code;
  diagnostic->code.length = strlen(code);
  diagnostic->actual = actual;
  diagnostic->expected = expected;
  diagnostic->declaration = declaration;
  diagnostic->label = label;
  diagnostic->accepted_forms = accepted_forms;
  diagnostic->primary = primary;
  diagnostic->document_index = context->module_index;
  return true;
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

static bool span_has_keyword(const w_seed_frontend_document *doc,
                             w_seed_span span, const char *keyword) {
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  while (cursor_take(&cursor, &token)) {
    if (token_text(doc, &token, keyword)) return true;
  }
  return false;
}

static w_seed_span import_path_span(const w_seed_frontend_document *doc,
                                    w_seed_span span) {
  frontend_token_cursor cursor = token_cursor_for(doc, span);
  frontend_token token;
  w_seed_span first = empty_span(span.start_byte);
  w_seed_span last = first;
  bool after_from = false;
  bool saw_import = false;
  bool found_path = false;
  while (cursor_take(&cursor, &token)) {
    if (!saw_import) {
      if (token_text(doc, &token, "import")) saw_import = true;
      continue;
    }
    if (token_text(doc, &token, "from")) {
      after_from = true;
      first = empty_span(token.span.end_byte);
      last = first;
      continue;
    }
    if (!after_from && (token_text(doc, &token, "{") ||
                        token_text(doc, &token, "}") ||
                        token_text(doc, &token, ","))) {
      continue;
    }
    if (!found_path) {
      first = token.span;
      found_path = true;
    }
    last = token.span;
  }
  if (!found_path) {
    return empty_span(span.start_byte);
  }
  return trim_span(doc, (w_seed_span){first.start_byte, last.end_byte});
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
  const w_seed_frontend_type value =
      type_record_from_span(doc, doc->nodes[type_node].raw_span);
  if (!context_append_type(context, value, root_index)) return false;
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
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_CONTRACT_ENVELOPE) {
      (void)context_append_fact(context, W_SEED_FRONTEND_FACT_UNSUPPORTED_TYPE,
                                doc->nodes[child].raw_span,
                                text_from_span(doc, doc->nodes[child].raw_span));
    }
    if (doc->nodes[child].kind == W_SEED_CST_TYPE) {
      uint32_t nested = W_SEED_FRONTEND_NONE;
      if (!normalize_type_tree_depth(context, child, &nested, depth + 1u)) {
        return false;
      }
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
        }
        nested_guard += 1;
      }
    }
    guard += 1;
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
  if (doc == NULL || import_index == NULL) return false;
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
  w_seed_frontend_import value;
  (void)memset(&value, 0, sizeof(value));
  value.module_index = (uint32_t)context->module_index;
  value.path = text_from_span(doc, import_path_span(doc, node->raw_span));
  value.alias = alias;
  value.span = node->raw_span;
  value.first_item = (uint32_t)context->count.import_items;
  value.item_count = (uint32_t)count_direct_kind(doc, node_index,
                                                  W_SEED_CST_IMPORT_ITEM);
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
                                             size_t depth);

static bool normalize_function(frontend_context *context, uint32_t node_index,
                               uint32_t *function_index) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || function_index == NULL) return false;
  const w_seed_cst_node *node = &doc->nodes[node_index];
  w_seed_frontend_function value;
  (void)memset(&value, 0, sizeof(value));
  value.module_index = (uint32_t)context->module_index;
  value.name = name_after_keyword(doc, node->raw_span, "fn");
  value.exported = span_has_keyword(doc, node->raw_span, "export");
  value.span = node->raw_span;
  value.body_span = empty_span(node->raw_span.end_byte);
  value.first_parameter = (uint32_t)context->count.parameters;
  value.parameter_count =
      (uint32_t)count_direct_kind(doc, node_index, W_SEED_CST_PARAMETER);
  value.return_type = W_SEED_FRONTEND_NONE;
  value.first_statement = (uint32_t)context->count.statements;
  value.statement_count = 0;
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
        parameter.label = parameter.name;
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
  }
  const uint32_t block_node = first_direct_kind(doc, node_index, W_SEED_CST_BLOCK);
  if (block_node != W_SEED_CST_NONE) {
    value.body_span = doc->nodes[block_node].raw_span;
    if (!normalize_block_statements(context, block_node)) return false;
  }
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
  if (second.length != 0 &&
      (text_equal(first, "external") || text_equal(first, "from"))) {
    return W_SEED_FRONTEND_LABEL_EXTERNAL_REQUIRED;
  }
  if (second.length != 0 && text_equal(first, "_")) {
    return W_SEED_FRONTEND_LABEL_OPTIONAL;
  }
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
  if (second.length != 0 &&
      (text_equal(first, "named") || text_equal(first, "external") ||
       text_equal(first, "from") || text_equal(first, "_"))) {
    return second;
  }
  return first;
}

static bool imported_target_for_name(
    const frontend_context *context, w_seed_frontend_text name,
    w_seed_frontend_text *module_name, w_seed_frontend_text *target_name) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || module_name == NULL || target_name == NULL) return false;
  uint32_t cursor = doc->nodes[doc->parse.root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind != W_SEED_CST_IMPORT) {
      guard += 1;
      continue;
    }
    const w_seed_frontend_text path =
        text_from_span(doc, import_path_span(doc, doc->nodes[child].raw_span));
    uint32_t item_cursor = doc->nodes[child].first_child;
    uint32_t item = W_SEED_CST_NONE;
    size_t item_guard = 0;
    bool matched = false;
    while (next_child(doc, &item_cursor, &item) &&
           item_guard < doc->parse.node_count) {
      if (doc->nodes[item].kind == W_SEED_CST_IMPORT_ITEM) {
        w_seed_frontend_text imported = {NULL, 0};
        const w_seed_frontend_text local = import_item_local_name(
            doc, doc->nodes[item].raw_span, &imported);
        if (text_equal_text(local, name)) {
          *module_name = path;
          *target_name = imported;
          matched = true;
          break;
        }
      }
      item_guard += 1;
    }
    /* A bare `import dep` names a module, not a value.  It does not create a
     * callable/value alias in this bounded slice.  Only explicit import items
     * (`import { value as local } from dep`) may enter value lookup.  Keeping
     * this distinction prevents a module path from accidentally resolving as
     * a local function. */
    if (matched) return true;
    guard += 1;
  }
  return false;
}

static bool external_symbol_for_name(const frontend_context *context,
                                     w_seed_frontend_text name,
                                     const w_seed_frontend_external_symbol **symbol) {
  if (context == NULL || symbol == NULL) return false;
  w_seed_frontend_text module_name = {NULL, 0};
  w_seed_frontend_text target_name = {NULL, 0};
  if (!imported_target_for_name(context, name, &module_name, &target_name)) {
    return false;
  }
  for (size_t module_index = 0;
       module_index < context->input.external_module_count; module_index += 1) {
    const w_seed_frontend_external_module *module =
        &context->input.external_modules[module_index];
    if (!text_equal_text(module->module_id, module_name)) continue;
    for (size_t index = 0; index < module->symbol_count; index += 1) {
      const w_seed_frontend_external_symbol *candidate = &module->symbols[index];
      if (candidate->exported && text_equal_text(candidate->name, target_name)) {
        *symbol = candidate;
        return true;
      }
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

static bool local_argument_expected(
    const w_seed_frontend_document *doc, uint32_t function_node,
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
      const w_seed_frontend_text name =
          parameter_name_from_span(doc, doc->nodes[child].raw_span);
      bool selected = false;
      if (label.length != 0) {
        selected = index == ordinal &&
                   policy != W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
                   text_equal_text(name, label);
      } else {
        selected = index == ordinal &&
                   policy != W_SEED_FRONTEND_LABEL_NAMED_REQUIRED &&
                   policy != W_SEED_FRONTEND_LABEL_EXTERNAL_REQUIRED;
      }
      if (selected) {
        const uint32_t type_node = direct_type_index(doc, child);
        if (type_node == W_SEED_CST_NONE) return false;
        *expected = simple_type_from_text(doc, doc->nodes[type_node].raw_span);
        return true;
      }
      index += 1;
    }
    guard += 1;
  }
  return false;
}

static bool external_argument_expected(
    const w_seed_frontend_external_symbol *symbol, size_t ordinal,
    w_seed_frontend_text label, frontend_simple_type *expected) {
  if (symbol == NULL || expected == NULL) return false;
  if (label.length != 0) {
    if (ordinal >= symbol->parameter_count) return false;
    const w_seed_frontend_external_parameter *parameter =
        &symbol->parameters[ordinal];
    if (parameter->label_kind != W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY &&
        text_equal_text(parameter->name, label)) {
      *expected = simple_type_from_view(parameter->type);
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
  *expected = simple_type_from_view(parameter->type);
  return true;
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
  w_seed_frontend_text module_name = {NULL, 0};
  w_seed_frontend_text target_name = {NULL, 0};
  if (!imported_target_for_name(context, name, &module_name, &target_name)) {
    return false;
  }
  for (size_t document_index = 0; document_index < context->input.document_count;
       document_index += 1) {
    const w_seed_frontend_document *doc =
        &context->input.documents[document_index];
    if (!text_equal_text(document_module_name(doc), module_name)) continue;
    if (function_in_document(doc, target_name, true, function_node)) {
      *owner_doc = doc;
      return true;
    }
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
          return simple_type_from_text(doc, doc->nodes[type_node].raw_span);
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
        return simple_type_from_text(doc, doc->nodes[type_node].raw_span);
      }
    }
  }
  return simple_type_unknown();
}

static frontend_simple_type function_return_type(
    const w_seed_frontend_document *doc, uint32_t function_node) {
  const uint32_t return_node =
      first_direct_kind(doc, function_node, W_SEED_CST_RETURN_TYPE);
  if (return_node == W_SEED_CST_NONE) {
    return simple_type_from_view((w_seed_frontend_text){"()", 2});
  }
  const uint32_t type_node = direct_type_index(doc, return_node);
  return type_node == W_SEED_CST_NONE
             ? simple_type_unknown()
             : simple_type_from_text(doc, doc->nodes[type_node].raw_span);
}

static bool output_type_index_for_simple(const frontend_context *context,
                                         frontend_simple_type type,
                                         uint32_t *index) {
  if (index == NULL) return false;
  *index = W_SEED_FRONTEND_NONE;
  if (context == NULL || !context->emit || context->output == NULL) return true;
  for (size_t item = 0; item < context->count.types; item += 1) {
    const w_seed_frontend_type *candidate = &context->output->types[item];
    if (candidate->kind == type.kind &&
        candidate->bit_width == type.bit_width &&
        candidate->is_signed == type.is_signed &&
        text_equal_text(candidate->spelling, type.spelling)) {
      if (item >= (size_t)UINT32_MAX) return false;
      *index = (uint32_t)item;
      return true;
    }
  }
  return true;
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
  record.supported = supported;
  if (!output_type_index_for_simple(parser->context, type,
                                    &record.inferred_type)) {
    return false;
  }
  uint32_t index = W_SEED_FRONTEND_NONE;
  if (!context_append_expression(parser->context, record, &index)) return false;
  value->index = index;
  value->type = type;
  value->supported = supported;
  value->has_name = kind == W_SEED_FRONTEND_EXPR_IDENTIFIER;
  value->name = value->has_name ? spelling : (w_seed_frontend_text){NULL, 0};
  value->operator_text = operator_text;
  value->span = span;
  return true;
}

static bool expression_parse_bp(frontend_expression_parser *parser,
                                int minimum_precedence,
                                frontend_expr_value *value);

static bool expression_parse_prefix(frontend_expression_parser *parser,
                                     frontend_expr_value *value);

static bool expression_parse_primary(frontend_expression_parser *parser,
                                     frontend_expr_value *value) {
  if (parser == NULL || value == NULL) return false;
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
    frontend_simple_type type = binding_type_for_name(
        parser->context, spelling, token.span);
    bool resolved = type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN;
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
        (void)context_append_fact(
            parser->context, W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL,
            token.span, spelling);
      }
    }
    const bool identifier_supported = resolved;
    return expression_append(parser, W_SEED_FRONTEND_EXPR_IDENTIFIER,
                             token.span, spelling,
                             (w_seed_frontend_text){NULL, 0}, type,
                             identifier_supported,
                             (size_t)W_SEED_FRONTEND_NONE,
                             (size_t)W_SEED_FRONTEND_NONE,
                             W_SEED_FRONTEND_NONE, 0, value);
  }
  if (token.kind == W_SEED_CST_NUMBER ||
      token.kind == W_SEED_CST_LITERAL_EVENT) {
    const frontend_simple_type type = literal_simple_type(
        parser->document, token.span, token.kind);
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
    return expression_append(
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
    return expression_append(parser, W_SEED_FRONTEND_EXPR_PARENTHESIS, span,
                             text_from_span(parser->document, span),
                             (w_seed_frontend_text){NULL, 0}, nested.type,
                             nested.supported, nested.index,
                             (size_t)W_SEED_FRONTEND_NONE,
                             W_SEED_FRONTEND_NONE, 0, value);
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
      const w_seed_frontend_text parameter = parameter_name_from_span(
          owner_doc, owner_doc->nodes[child].raw_span);
      if (label.length == 0) {
        return policy == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY ||
               policy == W_SEED_FRONTEND_LABEL_OPTIONAL;
      }
      if (policy == W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY) {
        guard += 1;
        continue;
      }
      if (text_equal_text(parameter, label)) return true;
    }
    guard += 1;
  }
  return false;
}

static bool expression_parse_postfix(frontend_expression_parser *parser,
                                     frontend_expr_value *value) {
  while (true) {
    frontend_token token;
    if (!cursor_peek(&parser->cursor, &token)) break;
    if (token_text(parser->document, &token, ".") ||
        token_text(parser->document, &token, "?.")) {
      (void)cursor_take(&parser->cursor, &token);
      frontend_token member;
      if (!cursor_take(&parser->cursor, &member)) return false;
      const w_seed_span span = {value->span.start_byte, member.span.end_byte};
      (void)context_append_fact(parser->context,
                                W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                span, text_from_span(parser->document, span));
      if (!expression_append(parser, W_SEED_FRONTEND_EXPR_UNSUPPORTED, span,
                             text_from_span(parser->document, span),
                             text_from_span(parser->document, token.span),
                             simple_type_unknown(), false, value->index,
                             (size_t)W_SEED_FRONTEND_NONE,
                             W_SEED_FRONTEND_NONE, 0, value)) {
        return false;
      }
      continue;
    }
    if (!token_text(parser->document, &token, "(")) break;
    (void)cursor_take(&parser->cursor, &token);
    const uint32_t first_argument = (uint32_t)parser->context->count.arguments;
    size_t argument_count = 0;
    bool labels_valid = true;
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
        if (resolved && !known) {
          labels_valid = false;
          (void)context_append_diagnostic(
              parser->context, W_SEED_FRONTEND_DIAGNOSTIC_LABEL,
              "W-LABEL-0005", value->name,
              (w_seed_frontend_text){"signature", 9}, value->name, label,
              (w_seed_frontend_text){"positional", 10}, value->span);
        }
      }
      frontend_expr_value argument_value;
      if (!expression_parse_bp(parser, 0, &argument_value)) return false;
      if (local_signature || external_signature_found) {
        frontend_simple_type expected = simple_type_unknown();
        const bool expected_found = local_signature
                                        ? local_argument_expected(
                                              signature_doc, signature_node,
                                              argument_count, label, &expected)
                                        : external_argument_expected(
                                              external_signature, argument_count,
                                              label, &expected);
        if (!expected_found) {
          labels_valid = false;
          if (label.length == 0) {
            (void)context_append_diagnostic(
                parser->context, W_SEED_FRONTEND_DIAGNOSTIC_LABEL,
                "W-LABEL-0005", value->name,
                (w_seed_frontend_text){"signature", 9}, value->name, label,
                (w_seed_frontend_text){"named", 5}, value->span);
          }
        } else if (argument_value.type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
                   expected.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
                   !widening_allowed(argument_value.type, expected)) {
          labels_valid = false;
          (void)context_append_fact(
              parser->context, W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
              argument_value.span,
              text_from_span(parser->document, argument_value.span));
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
    if (local_signature) {
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
    }
    if (value->has_name) {
      bool resolved = false;
      (void)call_label_known(parser->context, value->name,
                             (w_seed_frontend_text){NULL, 0}, &resolved);
      if (!resolved) {
        (void)external_label_known(parser->context, value->name,
                                   (w_seed_frontend_text){NULL, 0}, &resolved);
      }
      if (!resolved && value->supported) {
        (void)context_append_fact(parser->context,
                                  W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL,
                                  value->span, value->name);
      }
    }
    frontend_simple_type return_type = simple_type_unknown();
    if (local_signature) {
      return_type = function_return_type(signature_doc, signature_node);
    } else if (external_signature_found) {
      return_type = simple_type_from_view(external_signature->return_type);
    }
    const w_seed_span span = {value->span.start_byte, close.span.end_byte};
    if (!labels_valid) {
      (void)context_append_fact(parser->context,
                                W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                span, text_from_span(parser->document, span));
    }
    const bool supported = value->supported && labels_valid &&
                           return_type.kind != W_SEED_FRONTEND_TYPE_UNKNOWN;
    if (!supported && value->has_name && return_type.kind ==
                                      W_SEED_FRONTEND_TYPE_UNKNOWN) {
      (void)context_append_fact(parser->context,
                                W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                span, text_from_span(parser->document, span));
    }
    if (!expression_append(parser, W_SEED_FRONTEND_EXPR_CALL, span,
                           text_from_span(parser->document, span),
                           (w_seed_frontend_text){NULL, 0}, return_type,
                           supported, value->index,
                           (size_t)W_SEED_FRONTEND_NONE, first_argument,
                           argument_count, value)) {
      return false;
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
    return expression_append(parser,
                             valid ? W_SEED_FRONTEND_EXPR_UNARY
                                   : W_SEED_FRONTEND_EXPR_UNSUPPORTED,
                             span, text_from_span(parser->document, span),
                             text_from_span(parser->document, token.span),
                             token_text(parser->document, &token, "!")
                                 ? simple_type_from_view(
                                       (w_seed_frontend_text){"Bool", 4})
                                 : nested.type,
                             nested.supported && valid, nested.index,
                             (size_t)W_SEED_FRONTEND_NONE,
                             W_SEED_FRONTEND_NONE, 0, value);
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
    frontend_expr_value right;
    const int next_precedence = precedence + 1;
    if (!expression_parse_bp(parser, next_precedence, &right)) return false;
    const w_seed_span span = {value->span.start_byte, right.span.end_byte};
    frontend_simple_type result_type = value->type;
    bool supported = value->supported && right.supported;
    if (text_equal(operator_text, "&&") || text_equal(operator_text, "||")) {
      result_type = simple_type_from_view((w_seed_frontend_text){"Bool", 4});
      if (!type_is_bool(value->type) || !type_is_bool(right.type)) supported = false;
    } else if (text_equal(operator_text, "==") ||
               text_equal(operator_text, "!=") || text_equal(operator_text, "<") ||
               text_equal(operator_text, "<=") || text_equal(operator_text, ">") ||
               text_equal(operator_text, ">=") || text_equal(operator_text, "in")) {
      result_type = simple_type_from_view((w_seed_frontend_text){"Bool", 4});
      if (!type_equal(value->type, right.type) &&
          !(type_is_numeric(value->type) && type_is_numeric(right.type) &&
            (widening_allowed(value->type, right.type) ||
             widening_allowed(right.type, value->type)))) {
        supported = false;
      }
    } else if (type_is_numeric(value->type) && type_is_numeric(right.type)) {
      if (!type_equal(value->type, right.type)) {
        result_type = widening_allowed(value->type, right.type)
                          ? right.type
                          : (widening_allowed(right.type, value->type)
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
                           supported ? W_SEED_FRONTEND_EXPR_BINARY
                                     : W_SEED_FRONTEND_EXPR_UNSUPPORTED,
                           span, text_from_span(parser->document, span),
                           operator_text, result_type, supported, value->index,
                           right.index, W_SEED_FRONTEND_NONE, 0, value)) {
      return false;
    }
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
                                      uint32_t *expression_index) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || expression_index == NULL ||
      expression_node >= doc->parse.node_count) {
    return false;
  }
  frontend_expression_parser parser;
  parser.context = context;
  parser.document = doc;
  parser.cursor = token_cursor_for(doc, doc->nodes[expression_node].raw_span);
  parser.depth = 0;
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
    fallback.supported = false;
    return context_append_expression(context, fallback, expression_index);
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
    fallback.supported = false;
    return context_append_expression(context, fallback, expression_index);
  }
  if (value.index >= (size_t)UINT32_MAX) return false;
  *expression_index = (uint32_t)value.index;
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
      return function_return_type(owner_doc, function_node);
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
  if (expression_node != W_SEED_CST_NONE &&
      !normalize_expression_node(context, expression_node,
                                 &value.expression_index)) {
    return false;
  }
  if ((node->kind == W_SEED_CST_LET_STATEMENT ||
       node->kind == W_SEED_CST_VAR_STATEMENT) &&
      expression_node != W_SEED_CST_NONE && type_node != W_SEED_CST_NONE) {
    const frontend_simple_type actual =
        infer_expression_span(context, doc->nodes[expression_node].raw_span);
    const frontend_simple_type expected =
        simple_type_from_text(doc, doc->nodes[type_node].raw_span);
    if (actual.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        expected.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        !widening_allowed(actual, expected)) {
      const bool narrowing = actual.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                             expected.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                             actual.is_signed == expected.is_signed &&
                             actual.bit_width > expected.bit_width;
      (void)context_append_diagnostic(
          context,
          narrowing ? W_SEED_FRONTEND_DIAGNOSTIC_TYPE
                    : W_SEED_FRONTEND_DIAGNOSTIC_SEMANTIC,
          narrowing ? "W-TYPE-0122" : "W-SEM-0001", actual.spelling,
          expected.spelling, (w_seed_frontend_text){NULL, 0},
          (w_seed_frontend_text){NULL, 0}, (w_seed_frontend_text){NULL, 0},
          doc->nodes[expression_node].raw_span);
      value.kind = W_SEED_FRONTEND_STMT_UNSUPPORTED;
      (void)context_append_fact(context,
                                W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION,
                                node->raw_span, text_from_span(doc, node->raw_span));
    }
  }
  if (node->kind == W_SEED_CST_IF_STATEMENT) {
    value.condition_expression = value.expression_index;
    const frontend_simple_type condition =
        infer_expression_span(context, doc->nodes[expression_node].raw_span);
    if (condition.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        !type_is_bool(condition)) {
      (void)context_append_diagnostic(
          context, W_SEED_FRONTEND_DIAGNOSTIC_SEMANTIC, "W-SEM-0001",
          text_from_span(doc, trim_span(doc, doc->nodes[expression_node].raw_span)),
          (w_seed_frontend_text){"Bool", 4}, (w_seed_frontend_text){NULL, 0},
          (w_seed_frontend_text){NULL, 0}, (w_seed_frontend_text){NULL, 0},
          doc->nodes[expression_node].raw_span);
    }
  }
  if (node->kind == W_SEED_CST_RETURN_STATEMENT && expression_node != W_SEED_CST_NONE) {
    frontend_simple_type actual =
        infer_expression_span(context, doc->nodes[expression_node].raw_span);
    frontend_simple_type expected = simple_type_unknown();
    if (context->function_node != NULL) {
      expected = function_return_type(
          doc, (uint32_t)(context->function_node - doc->nodes));
    }
    if (actual.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        expected.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        !widening_allowed(actual, expected)) {
      const bool narrowing = actual.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                             expected.kind == W_SEED_FRONTEND_TYPE_INTEGER &&
                             actual.is_signed == expected.is_signed &&
                             actual.bit_width > expected.bit_width;
      (void)context_append_diagnostic(
          context,
          narrowing ? W_SEED_FRONTEND_DIAGNOSTIC_TYPE
                    : W_SEED_FRONTEND_DIAGNOSTIC_SEMANTIC,
          narrowing ? "W-TYPE-0122" : "W-SEM-0001",
          actual.spelling, expected.spelling, (w_seed_frontend_text){NULL, 0},
          (w_seed_frontend_text){NULL, 0}, (w_seed_frontend_text){NULL, 0},
          doc->nodes[expression_node].raw_span);
    }
  }
  if (node->kind == W_SEED_CST_RETURN_STATEMENT &&
      expression_node == W_SEED_CST_NONE && context->function_node != NULL) {
    const frontend_simple_type expected = function_return_type(
        doc, (uint32_t)(context->function_node - doc->nodes));
    if (expected.kind != W_SEED_FRONTEND_TYPE_UNKNOWN &&
        expected.kind != W_SEED_FRONTEND_TYPE_UNIT) {
      (void)context_append_diagnostic(
          context, W_SEED_FRONTEND_DIAGNOSTIC_SEMANTIC, "W-SEM-0001",
          (w_seed_frontend_text){"()", 2}, expected.spelling,
          (w_seed_frontend_text){NULL, 0}, (w_seed_frontend_text){NULL, 0},
          (w_seed_frontend_text){NULL, 0}, node->raw_span);
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
  if (node->kind == W_SEED_CST_IF_STATEMENT) {
    uint32_t child_cursor = node->first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t guard = 0;
    while (next_child(doc, &child_cursor, &child) &&
           guard < doc->parse.node_count) {
      if (doc->nodes[child].kind == W_SEED_CST_BLOCK &&
          !normalize_block_statements_depth(context, child, depth + 1u)) {
        return false;
      }
      if (doc->nodes[child].kind == W_SEED_CST_IF_STATEMENT &&
          !normalize_statement_depth(context, child, statement_index,
                                     depth + 1u)) {
        return false;
      }
      guard += 1;
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
                                             size_t depth) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL || block_node >= doc->parse.node_count ||
      depth >= W_SEED_FRONTEND_MAX_NESTING) {
    return false;
  }
  uint32_t child_cursor = doc->nodes[block_node].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0;
  while (next_child(doc, &child_cursor, &child) &&
         guard < doc->parse.node_count) {
    const w_seed_cst_kind kind = doc->nodes[child].kind;
    if (kind_is_statement(kind)) {
      uint32_t statement_index = W_SEED_FRONTEND_NONE;
      if (!normalize_statement_depth(context, child, &statement_index,
                                     depth + 1u)) {
        return false;
      }
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
  return true;
}

static bool normalize_block_statements(frontend_context *context,
                                       uint32_t block_node) {
  return normalize_block_statements_depth(context, block_node, 0u);
}

static bool normalize_document(frontend_context *context) {
  const w_seed_frontend_document *doc = context_document(context);
  if (doc == NULL) return false;
  w_seed_frontend_module module;
  (void)memset(&module, 0, sizeof(module));
  module.source_id = doc->logical_source_id;
  module.module_id = doc->module_id;
  const uint32_t header = first_direct_kind(doc, doc->parse.root,
                                            W_SEED_CST_MODULE_HEADER);
  if (header != W_SEED_CST_NONE) {
    const w_seed_frontend_text header_name =
        name_after_keyword(doc, doc->nodes[header].raw_span, "module");
    if (header_name.length != 0) module.module_id = header_name;
  }
  module.span = doc->nodes[doc->parse.root].raw_span;
  module.first_import = (uint32_t)context->count.imports;
  module.first_struct = (uint32_t)context->count.structs;
  module.first_type_declaration = (uint32_t)context->count.type_declarations;
  module.first_alias = (uint32_t)context->count.aliases;
  module.first_function = (uint32_t)context->count.functions;
  module.first_entry = (uint32_t)context->count.entries;
  module.import_count = 0;
  module.struct_count = 0;
  module.type_declaration_count = 0;
  module.alias_count = 0;
  module.function_count = 0;
  module.entry_count = 0;
  uint32_t module_index = W_SEED_FRONTEND_NONE;
  if (!append_module(context, module, &module_index)) return false;
  context->count.modules += 1;
  uint32_t module_symbol = W_SEED_FRONTEND_NONE;
  if (!normalize_symbol(context, W_SEED_FRONTEND_SYMBOL_MODULE, module_index,
                        module.module_id, true, module.span,
                        W_SEED_FRONTEND_NONE, &module_symbol)) {
    return false;
  }
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

static w_seed_frontend_text document_module_name(
    const w_seed_frontend_document *doc) {
  if (doc == NULL) return (w_seed_frontend_text){NULL, 0};
  const uint32_t header = first_direct_kind(doc, doc->parse.root,
                                            W_SEED_CST_MODULE_HEADER);
  if (header != W_SEED_CST_NONE) {
    const w_seed_frontend_text name =
        name_after_keyword(doc, doc->nodes[header].raw_span, "module");
    if (name.length != 0) return name;
  }
  return doc->module_id;
}

static bool module_id_equal(w_seed_frontend_text left,
                            w_seed_frontend_text right) {
  return left.length != 0 && right.length != 0 && text_equal_text(left, right);
}

static bool local_module_has_symbol(const w_seed_frontend_input *input,
                                    w_seed_frontend_text module_name,
                                    w_seed_frontend_text symbol_name) {
  if (input == NULL) return false;
  for (size_t document_index = 0; document_index < input->document_count;
       document_index += 1) {
    const w_seed_frontend_document *doc = &input->documents[document_index];
    if (!module_id_equal(document_module_name(doc), module_name)) continue;
    uint32_t cursor = doc->nodes[doc->parse.root].first_child;
    uint32_t child = W_SEED_CST_NONE;
    size_t guard = 0;
    while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
      const w_seed_cst_kind kind = doc->nodes[child].kind;
      w_seed_frontend_text name = {NULL, 0};
      bool exported = false;
      if (kind == W_SEED_CST_FUNCTION) name = name_after_keyword(
          doc, doc->nodes[child].raw_span, "fn");
      if (kind == W_SEED_CST_STRUCT) name = name_after_keyword(
          doc, doc->nodes[child].raw_span, "struct");
      if (kind == W_SEED_CST_TYPE_DECLARATION) name = name_after_keyword(
          doc, doc->nodes[child].raw_span, "type");
      if (kind == W_SEED_CST_ALIAS_DECLARATION) name = name_after_keyword(
          doc, doc->nodes[child].raw_span, "alias");
      if (kind == W_SEED_CST_FUNCTION || kind == W_SEED_CST_STRUCT ||
          kind == W_SEED_CST_TYPE_DECLARATION ||
          kind == W_SEED_CST_ALIAS_DECLARATION) {
        exported = span_has_keyword(doc, doc->nodes[child].raw_span, "export");
      }
      if (exported && name.length != 0 && text_equal_text(name, symbol_name)) {
        return true;
      }
      guard += 1;
    }
  }
  return false;
}

static bool external_module_has_symbol(const w_seed_frontend_input *input,
                                       w_seed_frontend_text module_name,
                                       w_seed_frontend_text symbol_name) {
  if (input == NULL) return false;
  for (size_t index = 0; index < input->external_module_count; index += 1) {
    const w_seed_frontend_external_module *module = &input->external_modules[index];
    if (!text_equal_text(module->module_id, module_name)) continue;
    for (size_t symbol = 0; symbol < module->symbol_count; symbol += 1) {
      if (text_equal_text(module->symbols[symbol].name, symbol_name) &&
          module->symbols[symbol].exported) {
        return true;
      }
    }
    return false;
  }
  return false;
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
        kind != W_SEED_CST_ALIAS_DECLARATION && kind != W_SEED_CST_ENTRY) {
      guard += 1;
      continue;
    }
    const char *keyword = kind == W_SEED_CST_FUNCTION
                              ? "fn"
                              : (kind == W_SEED_CST_STRUCT
                                     ? "struct"
                                     : (kind == W_SEED_CST_TYPE_DECLARATION
                                            ? "type"
                                            : (kind == W_SEED_CST_ALIAS_DECLARATION
                                                   ? "alias"
                                                   : "entry")));
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
            earlier_kind == W_SEED_CST_ALIAS_DECLARATION))) {
        const char *earlier_keyword = earlier_kind == W_SEED_CST_FUNCTION
                                          ? "fn"
                                          : (earlier_kind == W_SEED_CST_STRUCT
                                                 ? "struct"
                                                 : (earlier_kind ==
                                                            W_SEED_CST_TYPE_DECLARATION
                                                        ? "type"
                                                        : (earlier_kind ==
                                                                   W_SEED_CST_ALIAS_DECLARATION
                                                               ? "alias"
                                                               : "entry")));
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
  while (next_child(doc, &cursor, &child) && guard < doc->parse.node_count) {
    if (doc->nodes[child].kind == W_SEED_CST_IMPORT) {
      const w_seed_frontend_text path = text_from_span(
          doc, import_path_span(doc, doc->nodes[child].raw_span));
      bool has_module = false;
      for (size_t index = 0; index < context->input.document_count; index += 1) {
        if (module_id_equal(document_module_name(&context->input.documents[index]),
                            path)) {
          has_module = true;
          break;
        }
      }
      for (size_t index = 0; index < context->input.external_module_count; index += 1) {
        if (text_equal_text(context->input.external_modules[index].module_id, path)) {
          has_module = true;
          break;
        }
      }
      if (!has_module) {
        (void)context_append_fact(
            context, W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL,
            doc->nodes[child].raw_span, path);
      }
      uint32_t item_cursor = doc->nodes[child].first_child;
      uint32_t item = W_SEED_CST_NONE;
      size_t item_guard = 0;
      while (next_child(doc, &item_cursor, &item) &&
             item_guard < doc->parse.node_count) {
        if (doc->nodes[item].kind == W_SEED_CST_IMPORT_ITEM) {
          const w_seed_frontend_text name =
              first_word_in_span(doc, doc->nodes[item].raw_span);
          if (!local_module_has_symbol(&context->input, path, name) &&
              !external_module_has_symbol(&context->input, path, name)) {
            (void)context_append_fact(
                context, W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL,
                doc->nodes[item].raw_span, name);
          }
        }
        item_guard += 1;
      }
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
    receipt_write_literal(writer, "|sha256:");
    receipt_write_digest(writer, digest);
    receipt_write_literal(writer, "\n");
  }
  receipt_write_external_records(writer, input);
  if (output != NULL) {
    for (size_t index = 0; index < context->count.modules; index += 1) {
      const w_seed_frontend_module *module = &output->modules[index];
      receipt_write_literal(writer, "module=");
      receipt_write_text(writer, module->module_id);
      receipt_write_literal(writer, "|source=");
      receipt_write_text(writer, module->source_id);
      receipt_write_literal(writer, "\n");
    }
    for (size_t index = 0; index < context->count.imports; index += 1) {
      const w_seed_frontend_import *item = &output->imports[index];
      receipt_write_literal(writer, "import=");
      receipt_write_size(writer, item->module_index);
      receipt_write_literal(writer, "|");
      receipt_write_text(writer, item->path);
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
      receipt_write_literal(writer, "\n");
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
         capacity_ok(required->symbols, output->symbols,
                     output->symbol_capacity) &&
         capacity_ok(required->facts, output->facts, output->fact_capacity) &&
         capacity_ok(required->diagnostics, output->diagnostics,
                     output->diagnostic_capacity) &&
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
  (void)memset(result, 0, sizeof(*result));
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
  if (!receipt_size_source_records(&dry)) {
    result->status = W_SEED_FRONTEND_INVALID;
    return result->status;
  }
  if (!receipt_size_external_records(&dry)) {
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

  frontend_context emit;
  (void)memset(&emit, 0, sizeof(emit));
  emit.input = *input;
  emit.output = output;
  emit.result = result;
  emit.emit = true;
  for (size_t index = 0; index < input->document_count; index += 1) {
    emit.module_index = index;
    if (!normalize_document(&emit) || !detect_duplicate_declarations(&emit) ||
        !resolve_imports(&emit)) {
      result->status = W_SEED_FRONTEND_INVALID;
      return result->status;
    }
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
